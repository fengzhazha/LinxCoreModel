#include "TileSCB.h"

#include <cstdint>
#include "core/Core.h"
#include "mtccore/memcore/MemoryCoreTA.h"
#include "core/Packet.h"

namespace JCore {

TileSCB::TileSCB()
{
}

TileSCB::~TileSCB()
{
}

SimSys* TileSCB::GetSim()
{
    return m_sim;
}
void TileSCB::SetSim(SimSys *_sim)
{
    m_sim = _sim;
}

void TileSCB::Build(MemoryCoreTA *top, uint32_t entryNum)
{
    this->vecLsu = top;
    this->entryNum = entryNum;
    Entries.clear();
    scbCommitQ.clear();
    receiveLastRsp.clear();
    Entries.reserve(entryNum);
    for (size_t i = 0; i < entryNum; i++) {
        Entries.emplace_back(i);
    }
}

void TileSCB::Work()
{
    Evict();
    DrainAllEntry();
}

void TileSCB::Xfer()
{
}
void TileSCB::Reset()
{
}
int TileSCB::FindFreeEntry()
{
    for (size_t i = 0; i < Entries.size(); i++) {
        if (!Entries[i].valid) {
            return static_cast<int>(i);
        }
    }
    return -1;
}
void TileSCB::HandleTileRegRsp(uint32_t reqId)
{
    if (VERBOSE_ON) {
        std::cout << "[Tile SCB] Recv response from tile reg, reqId=" << reqId << std::endl;
    }
    for (auto& entry : Entries) {
        if (entry.valid && entry.waitRsp && entry.writeReqId == reqId) {
            LOG_INFO_M(Unit::MTC, Stage::NA) << "[Tile SCB] receive Recv ring response bid:" << dec << entry.bid <<
                " is from last inst:" << IsLastInstGetRsp(entry.bid.val);
            if (GetSim()->core->configs.mtc_wait_ring_rsp && IsDrainAllScbEntry(entry.bid.val, entry.id) &&
                (entry.IsFromLastInst() || IsLastInstGetRsp(entry.bid.val))) {
                ASSERT(scbCommitQ.count(entry.bid.val) == 0);
                scbCommitQ.insert(entry.bid.val);
                RemoveFromRspSet(entry.bid.val);
                LOG_INFO_M(Unit::MTC, Stage::NA) << "[Tile SCB] bid:" << dec << entry.bid <<
                    " Recv last ring response";
            } else if (entry.IsFromLastInst()) {
                LastInstGetRsp(entry.bid.val);
            }
            entry.Reset();
        }
    }
}

bool TileSCB::IsLastInstGetRsp(uint32_t bid) const
{
    auto it = receiveLastRsp.find(bid);
    if (it != receiveLastRsp.end()) {
        return true;
    }
    return false;
}

void TileSCB::LastInstGetRsp(uint32_t bid)
{
    receiveLastRsp.insert(bid);
}

void TileSCB::RemoveFromRspSet(uint32_t bid)
{
    auto it = receiveLastRsp.find(bid);
    if (it != receiveLastRsp.end()) {
        receiveLastRsp.erase(it);
    }
}

bool TileSCB::IsDrainAllScbEntry(uint32_t bid, int id) const
{
    auto &writeQ = scbStReqQ->GetRawWriteData();
    for (auto req : writeQ) {
        if (req.GetBid().val == bid) {
            return false;
        }
    }
    for (auto& entry : Entries) {
        if ((entry.id != id) && (entry.bid.val == bid) && entry.valid) {
            return false;
        }
    }

    return true;
}

bool TileSCB::IsFull() const
{
    for (const auto& entry : Entries) {
        if (!entry.valid) {
            return false;
        }
    }
    return true;
}

bool TileSCB::IsEmpty() const
{
    for (const auto& entry : Entries) {
        if (entry.valid && !entry.waitRsp) {
            return false;
        }
    }
    return true;
}

unsigned TileSCB::GetOcc() const
{
    unsigned occ = 0;
    for (const auto& entry : Entries) {
        occ += (entry.valid) ? 1 : 0;
    }
    return occ;
}

void TileSCB::Evict()
{
    // 判断full
    if (IsEmpty()) {
        return;
    }
    // 如果full, 选择1个cycle最大的entry evict
    int evictId = SelectEvictEntry();
    if (evictId == -1) {
        return;
    }
    // 发送write请求
    auto& entry = Entries[evictId];
    uint32_t reqId = vecLsu->CreateTileWriteReq(evictId, entry.addr, entry.data, 256, entry.mask);
    if (reqId != UINT32_MAX) {
        entry.writeReqId = reqId;
        entry.waitRsp  = true;
        entry.evictCycle = GetSim()->getCycles();
        if (VERBOSE_ON) {
            cout << "[Tile SCB] send write req, entryId=" << evictId << ", reqId=" << reqId << ", addr=0x"
                << hex << entry.addr << dec << ", bid=" << entry.bid.val << " is from last inst=" <<
                entry.IsFromLastInst() << std::endl;
        }
    }
}

void TileSCB::DrainEntry(ROBID &bid)
{
    isInDrainMode = true;
    drainBid = bid;
    if (VERBOSE_ON) {
        cout << "[Tile SCB] start to drain scb, bid=" << bid << std::endl;
    }
}

void TileSCB::DrainAllEntry()
{
    if (!isInDrainMode) {
        return;
    }
    for (auto& entry : Entries) {
        if (entry.valid && !entry.waitRsp && entry.bid == drainBid) {
            uint32_t reqId = vecLsu->CreateTileWriteReq(entry.id, entry.addr, entry.data, 256, entry.mask);
            if (reqId != UINT32_MAX) {
                entry.writeReqId = reqId;
                entry.waitRsp  = true;
                entry.evictCycle = GetSim()->getCycles();
                if (VERBOSE_ON) {
                    cout << "[Tile SCB] drain scb entry, entryId=" << entry.id << ", reqId=" << reqId << ", addr=0x"
                        << hex << entry.addr << dec << std::endl;
                }
            }
        }
    }
}

bool TileSCB::IsHalfFull(void) const
{
    uint32_t validCnt = 0;
    for (const auto& entry : Entries) {
        if (entry.valid) { validCnt++; }
    }
    if (validCnt >= (Entries.size())) {
        return true;
    } else {
        return false;
    }
}

int TileSCB::SelectEvictEntry() const
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
    return evictId;
}

void TileSCB::ProcessTStqReq()
{
    auto &writeQ = scbStReqQ->GetRawWriteData();
    // 然后处理 scbStReqQ
    uint32_t cnt = 0;
    while (!writeQ.empty() && (cnt < GetSim()->core->configs.write_scb_entry_size)) {
        VecTileRegStReq req = writeQ.front();
        uint64_t alignedAddr =  req.GetDest();
        assert((alignedAddr & (MAX_TILE_DATA_BYTE - 1)) == 0);

        int sameAddrEntry = FindEntryByAddress(alignedAddr);
        if (sameAddrEntry != -1) {
            MergeEntry(sameAddrEntry, alignedAddr, req.GetData(), req.GetDataMask(), req.IsLast());
            ASSERT(Entries[sameAddrEntry].bid == req.GetBid());
        } else {
            if (FindFreeEntry() == -1) {
                LOG_INFO_M(Unit::MTC, Stage::NA) << "[Tile SCB] Allocate SCB fail! B" <<
                    req.GetBid()<< ":G" << req.GetGid() << ":R" << req.GetRid() << ", addr=0x" <<
                    hex << alignedAddr << dec << "is from last inst " << req.IsLast();
                break;
            }
            AllocateEntry(alignedAddr, req.GetBid(), req.GetData(), req.GetDataMask(), req.IsLast());
        }
        if (VERBOSE_ON) {
            std::cout << "[Tile SCB] Allocate SCB, B" << req.GetBid()<< ":G" << req.GetGid() << ":R" << req.GetRid()
                  << ", addr=0x" << hex << alignedAddr << dec << "is from last inst" << req.IsLast() <<endl;
        }
        writeQ.pop_front();
        cnt++;
    }
    // 当buffer中还有没有写入TileSCB的，则反压
    if (writeQ.size() > 0) {
        scbStReqQ->setStall();
    } else {
        scbStReqQ->unsetStall();
    }
}

void TileSCB::HandleStore()
{
    if (vecSCBStReqQ.empty()) {
        return;
    }

    VecTileRegStReq req = vecSCBStReqQ.front();
    uint32_t alignedAddr =  req.GetDest();
    assert((alignedAddr & (MAX_TILE_DATA_BYTE - 1)) == 0);

    int sameAddrEntry = FindEntryByAddress(alignedAddr);
    if (sameAddrEntry != -1) {
        MergeEntry(sameAddrEntry, alignedAddr, req.GetData(), req.GetDataMask());
    } else {
        cout << "HandleStore" << endl;
        if (FindFreeEntry() == -1) {
            return;
        }
        DumpScb();
        AllocateEntry(alignedAddr, req.GetBid(), req.GetData(), req.GetDataMask());
        if (VERBOSE_ON) {
            std::cout << "[Tile SCB] Allocate SCB, B" << req.GetBid()<< ":G" << req.GetGid() << ":R" << req.GetRid()
                << ", addr=0x" << hex << alignedAddr << dec <<endl;
        }
    }

    if (VERBOSE_ON) {
        std::cout << "[Tile SCB] allocate SCB, " << req <<endl;
    }
    vecSCBStReqQ.erase(vecSCBStReqQ.begin());
}

int TileSCB::FindEntryByAddress(uint64_t alignedAddr)
{
    for (size_t i = 0; i < Entries.size(); i++) {
        if (Entries[i].valid && Entries[i].addr == alignedAddr) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void TileSCB::MergeEntry(int entryIdx, uint64_t addr, uint8_t* data, DataMask reqMask, bool isLast)
{
    TileSCBEntry& entry = Entries[entryIdx];
    if (VERBOSE_ON) {
        std::cout << "[Tile SCB] Merging store to entryId=" << entryIdx << ", addr=0x" << std::hex << addr
            << std::dec<< std::endl;
    }
    entry.MergeData(data, reqMask);
    entry.allocCycle = GetSim()->getCycles();
    if (isLast) {
        entry.SetFromLastInst();
    }
}

void TileSCB::AllocateEntry(uint64_t addr, const ROBID &bid, uint8_t* data, DataMask reqMask, bool isLast)
{
    int freeEntry = FindFreeEntry();
    if (freeEntry == -1) {
        assert(0 && "No free store queue Entries available!");
    }

    TileSCBEntry& entry = Entries[freeEntry];
    entry.valid = true;
    entry.addr = addr;
    entry.bid = bid;
    entry.allocCycle = GetSim()->getCycles();
    entry.mask = reqMask;
    entry.data.assign(data, data + MAX_TILE_DATA_BYTE);
    if (isLast) {
        entry.SetFromLastInst();
    }
}

bool TileSCB::CannotMerge(uint64_t addr)
{
    int sameAddrEntry = FindEntryByAddress(addr);
    if (sameAddrEntry != -1) {
        TileSCBEntry& entry = Entries[sameAddrEntry];
        if (entry.waitRsp) {
            return true; // if writing data to tile register, can not merge
        }
    }
    return false;
}
void TileSCB::DumpScb()
{
    std::cout << "=dump scb=" << std::endl;
    for (const auto& entry : Entries) {
        if (!entry.valid) {
            continue;
        }
    }
}

} // namespace JCore