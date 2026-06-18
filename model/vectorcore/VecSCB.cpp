#include "VecSCB.h"

#include <cstdint>
#include "core/Core.h"
#include "vectorcore/VectorCore.h"
#include "vectorcore/VectorCoreTA.h"
#include "core/Packet.h"

namespace JCore {

inline uint32_t CountBits(uint32_t num)
{
    if (num == 0) {
        return 1;
    }
    if (num < 0) {
        // 负数，计算其绝对值的位数+1（符号位）
        ASSERT(0 && "Num wrong");
        return 1;
    }
    uint32_t bits = 0;
    uint32_t n = num;
    while (n > 0) {
        n >>= 1;
        ++bits;
    }
    return bits;
}

VecSCB::VecSCB()
{
}

VecSCB::~VecSCB()
{
}

SimSys* VecSCB::GetSim()
{
    return m_sim;
}
void VecSCB::SetSim(SimSys *_sim)
{
    m_sim = _sim;
}

void VecSCB::Build(VectorCoreTA *top, VectorCore *vec, uint32_t entryNum)
{
    this->vecLsu = top;
    this->vtop = vec;
    this->entryNum = entryNum;
    this->evictMode = static_cast<EvictMode>(vec->GetConfig().scb_evict_mode);
    this->nToSets = vec->GetConfig().to_cache_set;
    this->nToWays = vec->GetConfig().to_cache_way;
    Entries.clear();
    Entries.reserve(entryNum);
    for (size_t i = 0; i < entryNum; i++) {
        Entries.emplace_back(i);
    }
    outputCache.resize(nToSets);
    for (uint64_t i = 0; i < nToSets; i++) {
        outputCache[i].resize(nToWays, TOEntry(i));
    }

    isInDrainMode.resize(GetSim()->core->configs.scalar_smt_thread);
    drainBid.resize(GetSim()->core->configs.scalar_smt_thread);
    drainWays.resize(GetSim()->core->configs.scalar_smt_thread);
}

bool VecSCB::IsEvictPerCycle() const
{
    return evictMode == EvictMode::PER_CYCLE;
}

void VecSCB::Work()
{
    RecvStashClear();
    Evict();
    for (uint32_t stid = 0; stid < drainBid.size(); ++stid) {
        DrainAllEntry(stid);
    }
}

void VecSCB::Xfer()
{
}
void VecSCB::Reset()
{
}
int VecSCB::FindFreeEntry()
{
    for (size_t i = 0; i < Entries.size(); i++) {
        if (!Entries[i].valid) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void VecSCB::RecvStashClear()
{
    while (!vtop->toClearQ->Empty()) {
        TileOperandPtr operand = vtop->toClearQ->Read();
        uint64_t addr = operand->baseAddr;
        for (auto &set : outputCache) {
            set[operand->wayId].Reset();
        }
        toWays.emplace_back(TOWayInfo(operand->bid.val, operand->stid, operand->wayId, operand->size, addr));
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "[Tile SCB] Init out cache"
                                             << dec << ", bid:" << operand->bid << ", way:" << operand->wayId
                                             << ", size:" << operand->size << ", addr:0x" << hex<< addr << dec;
    }
}

void VecSCB::HandleTileRegRsp(uint32_t reqId)
{
    if (vtop->GetConfig().stash_mode && vtop->GetConfig().stash_to_enable) {
        HandleTileRspTO(reqId);
        return;
    }
    LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "[Tile SCB] Recv response from tile reg, reqId=" << reqId;
    for (auto& entry : Entries) {
        if (entry.valid && entry.waitRsp && entry.writeReqId == reqId && entry.reissue) {
            entry.waitRsp = false;
        } else if (entry.valid && entry.waitRsp && entry.writeReqId == reqId && !entry.reissue) {
            if (entry.isVgather) {
                vtop->m_vectorGS->TriggerVgather(entry.groupSlotId);
            }
            vtop->m_stats.tile_lsu_scb_deallocate++;
            vtop->m_stats.tile_lsu_avg_scb_lifetime += (GetSim()->getCycles() - entry.allocCycle);
            entry.Reset();
        }
    }
}

void VecSCB::HandleTileRspTO(uint32_t reqId)
{
    LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "[TileOutCache] Recv response from tile reg, reqId=" << reqId;
    for (auto it = activeOutWay.begin(); it != activeOutWay.end(); it++) {
        bool sendReq = false;
        for (size_t set = 0; set < nToSets; set++) {
            auto& entry = outputCache[set][it->first];
            if (!entry.valid || !entry.waitRsp || entry.writeReqId != reqId) {
                continue;
            }
            entry.waitRsp = false;
            if (!entry.reissue) {
                sendReq = true;
                entry.SetStatus(false);
                break;
            }
        }
        if (sendReq) {
            break;
        }
    }
}

bool VecSCB::IsFull(bool updatePMU) const
{
    if (vtop->GetConfig().stash_mode && vtop->GetConfig().stash_to_enable) {
        return false;
    }
    for (const auto& entry : Entries) {
        if (!entry.valid) {
            return false;
        }
    }
    if (updatePMU) {
        vtop->m_stats.tile_lsu_scb_full++;
    }
    return true;
}

bool VecSCB::IsEmpty() const
{
    ASSERT(!(vtop->GetConfig().stash_mode && vtop->GetConfig().stash_to_enable) && "should check stash_mode");
    for (const auto& entry : Entries) {
        if (entry.valid && !entry.waitRsp) {
            return false;
        }
    }
    return true;
}

bool VecSCB::IsWayEmpty(uint32_t way) const
{
    for (size_t set = 0; set < nToSets; set++) {
        auto& entry = outputCache[set][way];
        if (entry.valid && entry.IsDirty()) {
            return false;
        }
    }
    return true;
}

bool VecSCB::IsEmpty(ROBID bid, uint32_t stid) const
{
    if (!(vtop->GetConfig().stash_mode && vtop->GetConfig().stash_to_enable)) {
        for (const auto& entry : Entries) {
            if (entry.valid && entry.bid == bid && entry.IsDirty() && entry.stid == stid) {
                return false;
            }
        }
    } else {
        std::vector<uint32_t> ways = GetAvailWay(bid, stid);
        /* 需要确认
        if (way == UINT64_MAX) {
            return true;
        }
        */
        for (uint32_t way : ways) {
            if (IsWayEmpty(way)) {
                continue;
            }
            return false;
        }
    }
    return true;
}

unsigned VecSCB::GetOcc() const
{
    unsigned occ = 0;
    for (const auto& entry : Entries) {
        occ += (entry.valid) ? 1 : 0;
    }
    return occ;
}

void VecSCB::DecodeAddress(uint32_t address, uint32_t& tag, uint32_t& index) const
{
    index = (address >> 8) & (nToSets - 1);
    tag = (address >> (8 + CountBits(nToSets)));
    ASSERT(index >= 0 && index < nToSets);
}
// -------------------------------------------outputCache 功能--------------------------------------------------
/**
(1) 每个block 占1个way => 寻找可用的way, 保存{blockid, wayId} map
(2) allocate entry(256Bytes)
(3) merge entry
(4) block resolve, evict到tile reg, reset way => invalid
**/
/**
         ┌────────────────┐     ┌────────────┐    ┌──────────────────┐    ┌─────────────────────┐
         │  store address │ ->  │ choose way │ -> │ allocate or merge│ -> │ block resolve, evict│
         └────────────────┘     └────────────┘    └──────────────────┘    └─────────────────────┘
**/
std::vector<uint32_t> VecSCB::GetAvailWay(ROBID bid, uint32_t stid) const
{
    std::vector<uint32_t> ways;
    for (auto it = toWays.begin(); it != toWays.end(); it++) {
        if (it->GetBid() == bid.val && it->GetStid() == stid) {
            ways.emplace_back(it->GetWay());
        }
    }
    return ways;
}

void VecSCB::Evict()
{
    if (vtop->GetConfig().stash_mode && vtop->GetConfig().stash_to_enable) {
        EvictOutCache();
        return;
    }
    // 判断full
    if (IsEmpty()) {
        return;
    }
    if (IsEvictPerCycle() || IsFull(false)) {
        // 如果full, 选择1个cycle最大的entry evict
        int evictId = SelectEvictEntry();
        if (evictId == -1) {
            assert(0 && "no avail evict entry");
        }
        // 发送write请求
        auto& entry = Entries[evictId];
        uint32_t reqId = vecLsu->CreateTileWriteReq(evictId, entry.addr, entry.data, 256, entry.mask, entry.stid);
        if (reqId != UINT32_MAX) {
            entry.writeReqId = reqId;
            entry.waitRsp  = true;
            entry.reissue  = false;
            entry.evictCycle = GetSim()->getCycles();
            LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "[Tile SCB] send write req, entryId=" << evictId
                                                 << ", reqId=" << reqId << ", addr=0x" << hex << entry.addr << dec;
        }
    }
}

void VecSCB::EvictOutCache()
{
    // 每cycle evict 1个entry
    for (auto it = activeOutWay.begin(); it != activeOutWay.end(); it++) {
        bool sendReq = false;
        for (size_t set = 0; set < nToSets; set++) {
            auto& entry = outputCache[set][it->first];
            if (!entry.valid || !entry.IsNeedEvict()) {
                continue;
            }
            // 发送write请求
            uint32_t reqId = vecLsu->CreateTileWriteReq(entry.id, entry.addr, entry.data,
                                                        MAX_TILE_DATA_BYTE, entry.mask, entry.stid);
            if (reqId != UINT32_MAX) {
                entry.writeReqId = reqId;
                entry.waitRsp  = true;
                entry.reissue  = false;
                entry.evictCycle = GetSim()->getCycles();
                LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "[TileOutCache] send write req, entryId=" << entry.id <<
                    ", reqId=" << reqId << ", addr=0x" << hex << entry.addr<< dec;
                sendReq = true;
                break;
            }
        }
        if (sendReq) {
            break;
        }
    }
}

void VecSCB::DrainEntry(ROBID &bid, uint32_t stid)
{
    isInDrainMode[stid] = true;
    drainBid[stid] = bid;
    drainWays[stid][bid] =  GetAvailWay(bid, stid);
    LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "[Tile SCB] start to drain scb, bid=" << bid.val;
}

void VecSCB::DrainAllEntry(uint32_t stid)
{
    if (!isInDrainMode[stid]) {
        return;
    }
    if (vtop->GetConfig().stash_mode && vtop->GetConfig().stash_to_enable) {
        DrainOutCacheEntry(drainBid[stid], stid);
        return;
    }
    for (auto& entry : Entries) {
        if (entry.valid && !entry.waitRsp && entry.bid == drainBid[stid]) {
            uint32_t reqId = vecLsu->CreateTileWriteReq(entry.id, entry.addr, entry.data, 256, entry.mask, entry.stid);
            if (reqId != UINT32_MAX) {
                entry.writeReqId = reqId;
                entry.waitRsp  = true;
                entry.reissue  = false;
                entry.evictCycle = GetSim()->getCycles();
                LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "[Tile SCB] drain scb entry, entryId=" << entry.id << ", reqId="
                    << reqId << ", addr=0x" << hex << entry.addr << dec;
            }
        }
    }
}

void VecSCB::DrainOutCacheEntry(ROBID bid, uint32_t stid)
{
    if (drainWays[stid].count(bid) == 0 || drainWays[stid][bid].empty()) {
        return;
    }
    uint64_t way = drainWays[stid][bid].front();
    if (IsWayEmpty(way)) {
        drainWays[stid][bid].erase(drainWays[stid][bid].begin());
        return;
    }
    ASSERT(way >= 0);
    for (size_t set = 0; set < nToSets; set++) {
        auto& entry = outputCache[set][way];
        if (entry.IsNeedEvict() && entry.bid == drainBid[stid]) {
            // 发送write请求
            uint32_t reqId = vecLsu->CreateTileWriteReq(entry.id, entry.addr, entry.data, 256, entry.mask, entry.stid);
            if (reqId != UINT32_MAX) {
                entry.writeReqId = reqId;
                entry.waitRsp  = true;
                entry.reissue  = false;
                entry.evictCycle = GetSim()->getCycles();
                LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "[TileOutCache] drain scb entry, entryId=" << entry.id <<
                    ", reqId=" << reqId << ", addr=0x" << hex << entry.addr << dec;
                break;
            }
        }
    }
}

int VecSCB::SelectEvictEntry() const
{
    if (IsEmpty()) {
        return -1;
    }

    int evictId = -1;
    uint32_t minCycle = UINT32_MAX;

    for (const auto& entry : Entries) {
        if (entry.valid && entry.allocCycle < minCycle && !entry.waitRsp) {
            minCycle = entry.allocCycle;
            evictId = entry.id;
        }
    }

    if (evictId == -1 && !IsEmpty()) {
        evictId = Entries[0].id;
    }
    return evictId;
}

void VecSCB::HandleStore()
{
    if (vecStReqQ.empty()) {
        return;
    }

    VecTileRegStReq req = vecStReqQ.front();
    if (vtop->GetConfig().stash_mode && vtop->GetConfig().stash_to_enable) {
        WriteOutCache(req);
    } else {
        WriteSCB(req);
    }
    vecStReqQ.erase(vecStReqQ.begin());
}

void VecSCB::WriteSCB(VecTileRegStReq& req)
{
    uint32_t alignedAddr = req.GetDest();
    assert((alignedAddr & (MAX_TILE_DATA_BYTE - 1)) == 0);

    int sameAddrEntry = FindEntryByAddress(alignedAddr);
    if (sameAddrEntry != -1) {
        MergeEntry(sameAddrEntry, alignedAddr, req, req.GetData(), req.GetDataMask());
    } else {
        AllocateEntry(alignedAddr, req, req.GetData(), req.GetDataMask(), req.GetStid());
        LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[Tile SCB] Allocate SCB, B" << req.GetBid()<< "-G" << req.GetGid()
            << "-T" << req.GetTid() << "-R" << req.GetRid() << ", addr=0x" << hex << alignedAddr << dec;
    }
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[Tile SCB] allocate SCB, " << req;
}

void VecSCB::WriteOutCache(VecTileRegStReq& req)
{
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[Tile OutCache] receive req from storeQueue, " << req;
    uint32_t alignedAddr = req.GetDest();
    assert((alignedAddr & (MAX_TILE_DATA_BYTE - 1)) == 0);
    uint32_t index = 0;
    uint32_t tag = 0;
    DecodeAddress(alignedAddr, tag, index);
    uint64_t way = GetTOWayByAddr(alignedAddr, req.GetBid(), req.GetStid());
    auto& entry = outputCache[index][way];
    ASSERT(index < nToSets);
    ASSERT(way < nToWays);
    if (entry.valid) {
        MergeOutCache(entry, alignedAddr, req, req.GetData(), req.GetDataMask());
    } else {
        if (activeOutWay.count(way) == 0) {
            activeOutWay.emplace(way, true);
        }
        AllocOutCache(entry, alignedAddr, req, req.GetBid(), req.GetData(), req.GetDataMask(), req.GetStid());
        LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[Tile OutCache] Allocate OutCache, way:" << way << ", B" << req.GetBid()
            << "-G" << req.GetGid() << "-T" << req.GetTid() << "-R" << req.GetRid() << ", addr=0x" <<
            hex << alignedAddr << dec;
    }
}

void VecSCB::ClearTOWay(uint32_t bid, uint32_t stid)
{
    for (auto it = toWays.begin(); it != toWays.end();) {
        if (it->GetBid() == bid && it->GetStid() == stid) {
            LOG_DEBUG_M(Unit::VECTOR, Stage::E2) << "[Tile TO] release bid:" << bid << ", way:" << it->GetWay();
            it = toWays.erase(it);
        } else {
            it++;
        }
    }
}

uint64_t VecSCB::GetTOWayByAddr(uint64_t addr, ROBID bid, uint32_t stid)
{
    uint32_t way = UINT32_MAX;
    for (const auto& info : toWays) {
        if (info.FindWayByAddress(addr, way)) {
            if (info.GetBid() != bid.val || info.GetStid() != stid) {
                cout << "bid not match, to fill bid:" << dec << info.GetBid() << ", store bid:" << bid << ", way:" <<
                    way << ", addr:0x" << hex << addr << dec << endl;
                ASSERT(0 && "not match");
            }
            return way;
        }
    }
    for (const auto& info : toWays) {
        info.DumpInfo();
    }
    ASSERT(0 && "Can't find TO way to write!");
    return -1ULL;
}

void VecSCB::MergeOutCache(TOEntry& entry, uint32_t addr, VecTileRegStReq& req, uint8_t* data, DataMask reqMask)
{
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[Tile OutCache] Merging store, addr=0x" << hex << addr << dec;
    entry.MergeData(data, reqMask);
    assert(!(entry.isVgather && req.GetVgather()));
    entry.SetStatus(true);
    entry.allocCycle = GetSim()->getCycles();
    if (entry.waitRsp) {
        entry.reissue = true;
    }
}

void VecSCB::AllocOutCache(TOEntry& entry, uint32_t addr, VecTileRegStReq& req, const ROBID &bid, uint8_t* data,
                           DataMask reqMask, uint32_t stid)
{
    uint32_t index = 0;
    uint32_t tag = 0;
    DecodeAddress(addr, tag, index);
    entry.valid = true;
    entry.addr = addr;
    entry.tag = tag;
    entry.bid = bid;
    entry.gid = req.GetGid();
    entry.isVgather = req.GetVgather();
    entry.groupSlotId = req.GetGroupSlotId();
    entry.SetStatus(true);
    entry.allocCycle = GetSim()->getCycles();
    entry.mask = reqMask;
    entry.data.assign(data, data + MAX_TILE_DATA_BYTE);
    entry.stid = stid;
}

uint64_t VecSCB::GetTOWay(uint32_t address)
{
    uint32_t tag = 0;
    uint32_t index = 0;
    DecodeAddress(address, tag, index);
    auto& set = outputCache[index];

    for (uint64_t way = 0; way < set.size(); ++way) {
        auto& cacheLine = set[way];
        if (cacheLine.valid && cacheLine.tag == tag) {
            LOG_DEBUG_M(Unit::VECTOR, Stage::E2) << "get to way: Address=0x" << std::hex << address
                << " (Set:" << std::dec << index << ", Way:" << way << ")";
            return way;
        }
    }
    return -1U;
}

void VecSCB::MergeTOData(uint32_t address, unsigned way, unsigned bytes, std::vector<uint8_t>& result)
{
    uint32_t tag = 0;
    uint32_t index = 0;
    uint32_t offset = address & ((1 << 8) - 1); // cacheline offset
    DecodeAddress(address, tag, index);
    auto& cacheLine = outputCache[index][way];
    std::copy_n(cacheLine.data.data() + offset, bytes, result.data());
}

void VecSCB::SetTOWay(unsigned blockId, unsigned way)
{
    if (wayMapByBcc_.count(blockId) == 0) {
        wayMapByBcc_[blockId] = way;
    } else {
        wayMapByBcc_[blockId] = way;
    }
}

int VecSCB::FindEntryByAddress(uint32_t alignedAddr)
{
    for (size_t i = 0; i < Entries.size(); i++) {
        if (Entries[i].valid && Entries[i].addr == alignedAddr) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void VecSCB::MergeEntry(int entryIdx, uint32_t addr, VecTileRegStReq& req, uint8_t* data, DataMask reqMask)
{
    TOEntry& entry = Entries[entryIdx];
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[Tile SCB] Merging store to entryId=" << entryIdx << ", addr=0x" << hex
        << addr << dec;

    entry.MergeData(data, reqMask);
    assert(!(entry.isVgather && req.GetVgather()));
    entry.allocCycle = GetSim()->getCycles();
    vtop->m_stats.tile_lsu_scb_merge_cnt++;
    if (entry.waitRsp) {
        entry.reissue = true;
    }
    entry.SetStatus(true);
}

void VecSCB::AllocateEntry(uint32_t addr, VecTileRegStReq& req, uint8_t* data, DataMask reqMask, uint32_t stid)
{
    int freeEntry = FindFreeEntry();
    if (freeEntry == -1) {
        assert(0 && "No free store queue Entries available!");
    }

    TOEntry& entry = Entries[freeEntry];
    entry.valid = true;
    entry.addr = addr;
    entry.bid = req.GetBid();
    entry.gid = req.GetGid();
    entry.isVgather = req.GetVgather();
    entry.groupSlotId = req.GetGroupSlotId();
    entry.fromCTInst = req.GetIsCTInst();
    entry.allocCycle = GetSim()->getCycles();
    entry.mask = reqMask;
    entry.data.assign(data, data + MAX_TILE_DATA_BYTE);
    vtop->m_stats.tile_lsu_scb_allocate_cnt++;
    entry.SetStatus(true);
    entry.stid = stid;
}

bool VecSCB::CannotMerge(uint32_t addr)
{
    int sameAddrEntry = FindEntryByAddress(addr);
    if (sameAddrEntry == -1) {
        return true; // if writing data to tile register, can not merge
    }
    return false;
}

void VecSCB::Flush(FlushBus &bus)
{
    auto checkFlush = [](FlushBus &b, ROBID &bId) -> bool {
        if (b.baseOnBid) {
            return LessEqual(b.req.bid, bId);
        }
        return LessEqual(b.req.bid, bId);
    };
    for (auto it = Entries.begin(); it != Entries.end(); it++) {
        if (!(*it).valid) {
            continue;
        }
        if (checkFlush(bus, (*it).bid) && (*it).stid == bus.req.stid) {
            LOG_ERROR << "flush scb, " << dec  << (*it).bid;
            LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "[Tile SCB] flush, store bid=" << dec << (*it).bid;
            (*it).Reset();
        }
    }
}

void VecSCB::DumpScb()
{
    std::cout << "=dump scb=" << std::endl;
    for (const auto& entry : Entries) {
        if (!entry.valid) {
            continue;
        }
        std::cout << "entryId=" << entry.id << ", Bid=" << entry.bid << ", addr=0x" << hex << entry.addr << dec <<endl;
    }
}

} // namespace JCore
