#include "vectorcore/VectorCoreTA.h"

#include <unordered_map>
#include <algorithm>

#include "vectorcore/VectorCore.h"
#include "vectorcore/VectorCoreTAStats.h"
#include "core/Core.h"

namespace JCore {

const static std::unordered_map<TAWriteType, const char*> taWriteInfo = {
    {TAWriteType::EVICT, "EVICT"},
    {TAWriteType::LDQ_REPICK, "LDQ REPICK"},
    {TAWriteType::LOAD, "LOAD"},
    {TAWriteType::READ_FILL, "READ FILL"},
    {TAWriteType::STORE, "STORE"},
};

static std::unordered_map<TAEntryState, const char*> taEntryStateInfo = {
    {TAEntryState::INVALID, "INVALID"},
    {TAEntryState::SHARED, "SHARED"},
    {TAEntryState::MODIFIED, "MODIFIED"}
};

// 判断是否misalign
inline static bool isMisaligned(uint64_t addr, uint32_t size)
{
    uint64_t end_addr = addr + size - 1;
    return (addr / 256) != (end_addr / 256);
}

template<typename T>
inline void PrintDataFormatted(const T& container)
{
    std::cout << "Data (hex):" << std::endl;

    for (size_t i = 0; i < container.data.size(); ++i) {
        if (i % 16 == 0) {
            if (i > 0) std::cout << std::endl;
            std::cout << std::hex << std::setfill('0')<< std::setw(4)  << i << ": ";
        }

        std::cout << std::hex << std::setfill('0')<< std::setw(2)
                  << static_cast<int>(container.data[i]) << " ";

        if (i % 8 == 7) std::cout << " "; // 每8字节加个空格
    }
    std::cout << std::dec << std::endl;
}

inline void PrintDataFormatted2(std::vector<uint8_t> data, uint64_t from)
{
    std::cout << "from: 0x" << std::hex << from << " Data (hex):" << std::endl << std::dec;

    for (size_t i = 0; i < data.size(); ++i) {
        if (i % 16 == 0) {
            if (i > 0) std::cout << std::endl;
            std::cout << std::hex << std::setfill(' ') << std::setw(4) << i + from << ": ";
        }

        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(data[i]) << " ";

        if (i % 8 == 7) std::cout << " "; // 每8字节加个空格
    }
    std::cout << std::dec << std::endl;
}

inline bool IsPow2(uint64_t val)
{
    return ((val & (val - 1)) == 0);
}

inline uint32_t count_bits_log(uint32_t num)
{
    if (num == 0) {
        return 1;
    }

    if (num < 0) {
        // 负数，计算其绝对值的位数+1（符号位）
        ASSERT(0);
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

MissRequestQueue::MissRequestEntry::MissRequestEntry (uint64_t addr, uint32_t eid, uint64_t reqId)
    : address(addr), entryId(eid), requestId(reqId) {}

MissRequestQueue::MissRequestQueue(size_t depth, uint32_t scalarCnt)
    : maxDepth_(depth)
{
    addressMap_.resize(scalarCnt);
    nextEntryId_.resize(scalarCnt, 0);
}

bool MissRequestQueue::addMissRequest(uint64_t address, uint32_t stid, uint64_t requestId, uint32_t entryId)
{
    if (addressMap_[stid].size() >= maxDepth_) {
        return false;
    }
    auto it = addressMap_[stid].find(address);
    if (it != addressMap_[stid].end()) {
        ASSERT(0 && "should be merged");
    }

    nextEntryId_[stid]++;
    auto entry = std::make_shared<MissRequestEntry>(address, entryId, requestId);
    addressMap_[stid][address] = entry;
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[Tile load Miss Queue] record ld miss req, entryId=" << entryId <<", reqId="
        << requestId << ", addr=0x" << std::hex << address << dec;
    return true;
}

void MissRequestQueue::addMergeEntryId(uint64_t address, uint32_t stid, uint32_t entryId)
{
    auto addrIt = addressMap_[stid].find(address);
    if (addrIt == addressMap_[stid].end()) {
        ASSERT(0);
    }
    addressMap_[stid][address]->mergeEntryId.push_back(entryId);
}

void MissRequestQueue::removeOnFill(uint64_t requestId, uint32_t stid)
{
    for (auto it = addressMap_[stid].begin() ; it != addressMap_[stid].end();) {
        if (it->second->requestId == requestId) {
            LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "delete ld miss entry, entryId=" << dec << it->second->entryId
                <<", reqId=" << it->second->requestId;
            it = addressMap_[stid].erase(it);
            break;
        } else {
            ++it;
        }
    }
}

bool MissRequestQueue::FindReqByAddress(uint64_t address, uint32_t stid, uint32_t camEid,
                                        uint32_t &eid, uint64_t &reqId)
{
    auto it = addressMap_[stid].find(address);
    if (it != addressMap_[stid].end()) {
        eid = it->second->entryId;
        reqId = it->second->requestId;
        addMergeEntryId(address, stid, camEid);
        LOG_INFO_M(Unit::VECTOR, Stage::NA) << "Tile Load Miss, merge other req, entryId="<< eid << ", addr=0x" << hex
            << address << dec << ", hit reqId=" << reqId;
        return true;
    }

    return false;
}

bool MissRequestQueue::FindReqByAddress(uint64_t address, uint32_t stid)
{
    auto it = addressMap_[stid].find(address);
    if (it != addressMap_[stid].end()) {
        return true;
    }
    return false;
}

void MissRequestQueue::GetMissReqEntryId(uint64_t reqId, uint32_t stid, uint32_t &eid, uint32_t &addr)
{
    for (const auto& pair : addressMap_[stid]) {
        if (pair.second->requestId == reqId) {
            eid = pair.second->entryId;
            addr = pair.second->address;
            LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "fill data get info, reqId=" << reqId << ", eid=" <<eid<<", addr=0x"
                << hex << addr << dec;
            return;
        }
    }
}

void MissRequestQueue::GetMergeReqEntryId(uint64_t reqId, uint32_t stid, std::vector<uint32_t> &eids)
{
    for (const auto& pair : addressMap_[stid]) {
        if (pair.second->requestId == reqId) {
            eids = pair.second->mergeEntryId;
            return;
        }
    }
}

size_t MissRequestQueue::getCurrentSize(uint32_t stid) const
{
    return addressMap_[stid].size();
}

size_t MissRequestQueue::getMaxDepth() const
{
    return maxDepth_;
}

bool MissRequestQueue::isEmpty(uint32_t stid) const
{
    return addressMap_[stid].empty();
}

bool MissRequestQueue::isFull(uint32_t stid) const
{
    return addressMap_[stid].size() >= maxDepth_;
}

void MissRequestQueue::clear(uint32_t stid)
{
    addressMap_[stid].clear();
}

void MissRequestQueue::printQueueStatus() const
{
    std::cout << "Miss Request Queue Status:" << std::endl;
    for (uint32_t stid = 0; stid < addressMap_.size(); ++stid) {
        std::cout << "  Current Size: " << addressMap_[stid].size()
              << "/" << maxDepth_ << std::endl;
        for (const auto& entry : addressMap_[stid]) {
            std::cout << "  entryId=" << entry.second->entryId
                    << ", Addr: 0x" << std::hex << entry.first
                    << std::dec << ", reqId=" << entry.second->requestId << std::endl;
            for (const auto& merge : entry.second->mergeEntryId) {
                std::cout << ", Merge entryId=" << merge << std::endl;
            }
        }
    }
}

VectorCoreTA::VectorCoreTA()
{
}

VectorCoreTA::~VectorCoreTA()
{
}

void VectorCoreTA::Build(VectorCore *top, uint64_t set, uint64_t way, uint64_t entrySize,
    uint64_t loadQSize, uint64_t storeQSize, bool use_tile_cache)
{
    ASSERT(way > 0);
    this->vtop = top;
    this->nSets = set;
    this->nWays = way;
    this->nTaSets = top->GetConfig().ta_cache_set;
    this->nTaWays = top->GetConfig().ta_cache_way;
    this->cacheOffsetBits = 8; // 2^8 = 256B
    uint32_t temp = set;
    while (temp > 1) {
        this->cacheIndexBits++;
        temp >>= 1;
    }
    this->entrySizes = entrySize;
    this->addrMask = static_cast<uint64_t>(~(255ULL)); // 256B 地址对齐
    this->pTileUtils = top->pTileUtils;
    this->ldBand = top->GetConfig().ta_tile_ld_band;
    this->stBand = top->GetConfig().ta_tile_st_band;
    this->use_tile_cache = use_tile_cache;
    this->threadNum = top->GetConfig().vector_core_thread_num;
    this->ldqPerThreSize = loadQSize;
    this->stqPerThreSize = storeQSize;
    // 初始化tBuffer
    tBuffer.resize(nSets);
    for (uint64_t i = 0; i < nSets; i++) {
        tBuffer[i].resize(nWays, TAEntry(entrySizes));
    }
    // stash mode
    inputCache.resize(nTaSets);
    for (uint64_t i = 0; i < nTaSets; i++) {
        inputCache[i].resize(nTaWays, TAEntry(top->GetConfig().ta_cache_size));
    }

    loadQueue.resize(loadQSize);
    storeQueue.resize(storeQSize * threadNum);
    for (uint64_t i = 0; i < loadQSize; i++) {
        loadQueue[i].Init(i);
    }
    for (uint64_t i = 0; i < storeQSize * threadNum; i++) {
        storeQueue[i].Init(i);
    }

    ldqEntryOccCnt = 0;
    stqEntryOccCnt.resize(threadNum, 0);

    // Write Back Q by order
    for (uint64_t i = 0; i < static_cast<uint64_t>(TAWriteType::TA_WRITE_END); ++i) {
        writeBackQ.emplace_back(SimQueue<VecTileRegStReq>());
    }

    loadMissQueue = std::make_shared<MissRequestQueue>(loadQSize*64,
        top->GetSim()->core->configs.scalar_smt_thread);
    prefetchMissQueue = std::make_shared<MissRequestQueue>(
        top->GetConfig().pfQueueMaxSize + top->GetConfig().hpfQueueMaxSize,
        top->GetSim()->core->configs.scalar_smt_thread);

    scb = std::make_shared<VecSCB>();
    scb->SetSim(top->GetSim());
    scb->Build(this, top, top->GetConfig().ta_scb_entry_size);
    for (size_t tid = 0; tid < threadNum; tid++) {
        SimQueue<VecTARequest> req;
        req.Build();
        retireScbMap_.insert(std::make_pair(tid, req));
    }

    stQ_credit.resize(threadNum);
    ldQ_credit = loadQSize;
    for (uint64_t i = 0; i < threadNum; i++) {
        stQ_credit[i] = storeQSize;
    }

    uint32_t coreIdOffset = 9;
    tileReqId = (1 << coreIdOffset);
    resolveCnt = 0;

    drainBlockId.resize(GetSim()->core->configs.scalar_smt_thread);
    prefetchReqQ.resize(GetSim()->core->configs.scalar_smt_thread);
    hprefetchReqQ.resize(GetSim()->core->configs.scalar_smt_thread);
}

void VectorCoreTA::Xfer()
{
    resolveCnt = 0;
    // 处理load queue/store queue状态机, E2 -> E3
    for (auto it = loadQueue.begin(); it != loadQueue.end(); it++) {
        switch ((*it).stage) {
            case VecTAStage::STAGE_E1:
                (*it).stage = VecTAStage::STAGE_E2;
                break;
            case VecTAStage::STAGE_E2:
                (*it).stage = VecTAStage::STAGE_E3;
                break;
            case VecTAStage::STAGE_E3:
                (*it).stage = VecTAStage::INVALID;
                break;
            default:
                break;
        }
    }

    for (auto it = storeQueue.begin(); it != storeQueue.end(); it++) {
        switch ((*it).stage) {
            case VecTAStage::STAGE_E1:
                (*it).stage = VecTAStage::STAGE_E2;
                break;
            case VecTAStage::STAGE_E2:
                (*it).stage = VecTAStage::STAGE_E3;
                break;
            case VecTAStage::STAGE_E3:
                (*it).stage = VecTAStage::INVALID;
                break;
            default:
                break;
        }
    }

    scb->Xfer();
    ldOutputQ->Work();
    stOutputQ->Work();
    for (size_t tid = 0; tid < threadNum; tid++) {
        retireScbMap_[tid].Work();
    }
}

void VectorCoreTA::Reset()
{
}

void VectorCoreTA::Work()
{
    if (vtop->GetConfig().enable_scb) {
        scb->Work();
    }
    for (uint32_t stid = 0; stid < drainBlockId.size(); ++stid) {
        DrainEmpty(stid);
    }
    ReceiveNonFlush();
    ProcessVscatterTileSending();  // VSCATTER: 处理 tile 发送状态转换
    ProcessDbidResponse();

    // 处理TileRegister 数据回填
    ReceiveTRData();
    ReceiveTRRsp();
    ReceiveBridgeResp();

    ldInPipeCnt = ldBand;
    E1();
    E2();
    E3();

    if (vtop->GetConfig().enable_scb) {
        RetireStore();
    }
    for (uint32_t stid = 0; stid < drainBlockId.size(); ++stid) {
        if (!drainScbDataQ[stid]->Empty() && vtop->GetConfig().enable_scb && !scb->isInDrainMode[stid]) {
            ScbDrainBus drainInfo = drainScbDataQ[stid]->Read();
            LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "[VectorCore TA Stage]: Receive resolve bid " <<
                drainInfo.bid << " stid " << drainInfo.stid;
            drainBlockId[drainInfo.stid] = drainInfo.bid;
            scb->DrainEntry(drainInfo.bid, drainInfo.stid);
            DrainTBuffer(drainInfo.bid, drainInfo.stid);
        }
    }
    SendStashResp();
    StatsCount();
    for (uint32_t stid = 0; stid < GetSim()->core->configs.scalar_smt_thread; ++stid) {
        Prefetch(stid);
    }
    // DumpStoreQueue
}

void VectorCoreTA::Dump()
{
    DumpLoadQueue();
    DumpStoreQueue();
    scb->DumpScb();
}

bool VectorCoreTA::IsBusy()
{
    bool stqFree = true;
    for (size_t i = 0; i < threadNum; i++) {
        stqFree &= (stqEntryOccCnt[i] == 0);
    }
    stqEntryOccCnt.resize(threadNum);
    if (vtop->GetConfig().enable_scb) {
        return !scb->IsEmpty() && !stqFree;
    } else {
        return !stqFree;
    }
}
// -------------------------------------------inputCache 功能------------------------------------------------------
void VectorCoreTA::ReceiveStashReq(uint32_t addr, TileRegVecLdRes req)
{
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "vec resive data addr = " << hex << addr << dec;
    FillData(pTileUtils->Addr2Bytes(addr), req);
}

uint64_t VectorCoreTA::GetBlockWay(uint32_t address, bool &hitTO)
{
    //(1) tag匹配
    /*
    | Tag (6 bits) | Set Index (7 bits) | Offset (8 bits) | => 2M tile reg
    */
    /*
    cube[acccvt->ring->tilereg] =>resolve to BCC => BCC stash to tmu => vector[tilereg->ring->tilecache]
    -----------------------------------------------
    流程: tilereg回填 -> ring to BCC -> wakeup -> 加载命中
    -----------------------------------------------
    tilereg回填   : 数据回填
    ring to BCC   : 数据接收成功，返回response
    BCC wakeup    : BCC wakeup
    load hit      : load always hit
    -----------------------------------------------
    */
    uint32_t tag;
    uint32_t index;
    uint32_t offset;
    decodeAddress(address, tag, index, offset);
    auto& set = inputCache[index];

    for (uint64_t way = 0; way < set.size(); ++way) {
        auto& cacheLine = set[way];
        if (cacheLine.valid && cacheLine.tag == tag) {
            LOG_DEBUG_M(Unit::VECTOR, Stage::E2) << "get ta way: Address=0x" << std::hex << address
                << " (Set:" << std::dec << index << ", Way:" << way << ")";
            hitTO = false;
            return way;
        }
    }
    hitTO = true;
    uint64_t hitTOWay = vtop->GetConfig().stash_to_enable ? scb->GetTOWay(address) : -1U;
    LOG_INFO_M(Unit::VECTOR, Stage::E2) << "check Address=0x" << std::hex << address << dec << ", wayId:" << hitTOWay;
    // load always hit ta/to cache(if stash_to_enable)
    if (hitTOWay == -1U) {
        for (uint32_t stid = 0; stid < GetSim()->core->configs.scalar_smt_thread; ++stid) {
            GetSim()->core->PrintCoreStatus(stid);
        }
    }
    ASSERT(hitTOWay != -1U);
    return hitTOWay;
}

void VectorCoreTA::SendStashResp()
{
    if (!stashRespQueue.empty()) {
        auto pkt = stashRespQueue.front();
        if (!vtop->HasValidSpbReadEntry()) {
            LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "[Tile Cache resp] No Valid spb" << *pkt;
            return;
        }
        LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId << "]Stash origin req " << *pkt;
        pkt->SetChannelType(ChannelType::REQ0);
        ASSERT(pkt->IsStash());
        pkt->SetStashResp(true);
        std::shared_ptr<CrossStation> station = vtop->GetFreeReadStation();
        pkt->SetFlitSrcNodeId(station->GetId());
        if (!pkt->broadcast) {
            pkt->SetCHICommand(CHICommand::ReadUnique);
            pkt->SetFlitTgtNodeId(pkt->GetArcSrcNode());
        } else {
            if (pkt->broadcastList.empty()) {
                pkt->SetFlitTgtNodeId(pkt->broadcastTgt);
            } else {
                BroadcastInfo info = pkt->broadcastList[0];
                pkt->broadcastList.erase(pkt->broadcastList.begin());
                pkt->SetFlitTgtNodeId(info.station);
            }
        }
        station->WriteSpb(pkt);
        stashRespQueue.pop_front();
        LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId << "]Vector stash resp " << *pkt;
        if (pkt->cmd) {
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << ", count:" << pkt->cmd->stashCnt;
        }
        if (pkt->broadcast) {
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId << "]Vector stash resp[broadcast]."
                << " target station: " << dec << pkt->GetArcTgtNode();
        }
    }
}

// -------------------------------------------dynamic stash 功能------------------------------------------------
bool MissRequestQueue::PrefetchCam(uint64_t address, uint32_t stid)
{
    auto it = addressMap_[stid].find(address);
    if (it != addressMap_[stid].end()) {
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "Tile Load merge other req, entryId="<< it->second->entryId
            << ", addr=0x" << hex << address << dec << ", hit reqId=" << it->second->requestId;
        return true;
    }
    return false;
}

bool VectorCoreTA::PrefetchCamCache(uint32_t addr, uint32_t stid)
{
    uint32_t tag;
    uint32_t index;
    uint32_t offset;
    decodeAddress(addr, tag, index, offset);

    if (index >= nSets) {
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "Index out of range: " << index << " >= " << nSets;
        return false;
    }
    auto& set = tBuffer[index];
    for (uint32_t i = 0; i < nWays; ++i) {
        auto& cacheLine = set[i];
        if (cacheLine.valid && cacheLine.tag == tag && cacheLine.stid == stid) {
            return true;
        }
    }
    return false;
}

void VectorCoreTA::TrainPf(uint64_t addr, uint64_t stride, ROBID bid, uint32_t stid)
{
    if (!vtop->GetConfig().dynamic_stash_mode) {
        return;
    }
    for (uint32_t i = 0; i < stride; ++i) {
        uint64_t prefetchAddr = addr + i * MAX_TILE_DATA_BYTE;
        if (prefetchAddr >= vtop->pTileUtils->GetUBConfig()->tile_reg_size * 1024) { // 当前load在tilereg边界不需要预取
            return;
        }

        if (vtop->GetConfig().prefetch_pending_en && (prefetchReqQ[stid].size() >= vtop->GetConfig().pfQueueMaxSize)) {
            return;
        }

        if (IsInHPfQ(prefetchAddr, stid)) {
            hprefetchReqQ[stid].clear();
            continue;
        }

        if (prefetchMissQueue->FindReqByAddress(prefetchAddr, stid)) {
            continue;
        }

        if (!IsNormalCacheHit(prefetchAddr)) {
            if (!vtop->IsInAddrRange(bid, stid, prefetchAddr)) {
                continue;
            }
            if (loadMissQueue->PrefetchCam(prefetchAddr, stid)) {
                vtop->m_stats.tileLoadPrefetchDropCnt++;
                continue;
            }
            if (PrefetchCamCache(prefetchAddr, stid)) {
                vtop->m_stats.tileLoadPrefetchDrop1Cnt++;
                continue;
            }
            WriteStashReq(prefetchAddr, stid);
        }
    }
    vtop->m_stats.tileLoadTrainPfCnt++;
}

void VectorCoreTA::TrainHPf(uint64_t addr, uint64_t stride, ROBID bid, uint32_t stid)
{
    if (!vtop->GetConfig().dynamic_stash_mode) {
        return;
    }
    for (uint32_t i = 0; i < stride; ++i) {
        uint64_t prefetchAddr = addr + i * MAX_TILE_DATA_BYTE;
        if (prefetchAddr >= vtop->pTileUtils->GetUBConfig()->tile_reg_size * 1024) { // 当前load在tilereg边界不需要预取
            return;
        }

        if (vtop->GetConfig().prefetch_pending_en &&
            (hprefetchReqQ[stid].size() >= vtop->GetConfig().hpfQueueMaxSize)) {
            return;
        }

        if (prefetchMissQueue->FindReqByAddress(prefetchAddr, stid)) {
            continue;
        }

        if (!IsNormalCacheHit(prefetchAddr)) {
            if (!vtop->IsInAddrRange(bid, stid, prefetchAddr)) {
                continue;
            }
            if (loadMissQueue->PrefetchCam(prefetchAddr, stid)) {
                vtop->m_stats.tileLoadPrefetchDropCnt++;
                continue;
            }
            if (PrefetchCamCache(prefetchAddr, stid)) {
                vtop->m_stats.tileLoadPrefetchDrop1Cnt++;
                continue;
            }
            WriteHeadStashReq(prefetchAddr, stid);
        }
    }
    vtop->m_stats.tileHeadTrainPfCnt++;
}

void VectorCoreTA::WriteStashReq(uint64_t addr, uint32_t stid)
{
    if (IsInPfQ(addr, stid)) {
        return;
    }
    prefetchReqQ[stid].push_back(addr);
    vtop->m_stats.tileLoadTriggerPfCnt++;
}

void VectorCoreTA::WriteHeadStashReq(uint64_t addr, uint32_t stid)
{
    hprefetchReqQ[stid].push_back(addr);
    vtop->m_stats.tileHeadTriggerPfCnt++;
}

void VectorCoreTA::Prefetch(uint32_t stid)
{
    if (vtop->pTileUtils->configs.strict_closest_injection) {
        if (!prefetchReqQ[stid].empty()) {
            ProcessPrefetchQ(prefetchReqQ[stid], stid);
        } else if (!hprefetchReqQ[stid].empty()) {
            ProcessPrefetchQ(hprefetchReqQ[stid], stid);
        }
        return;
    }

    while (vtop->HasValidSpbReadEntry() && (hprefetchReqQ[stid].size() != 0 || prefetchReqQ[stid].size() != 0) &&
            prefetchMissQueue->getCurrentSize(stid) < vtop->GetConfig().pfPendingMaxSize) {
        uint64_t addr = 0;
        if (prefetchReqQ[stid].size() != 0) {
            addr = prefetchReqQ[stid][0];
            prefetchReqQ[stid].erase(prefetchReqQ[stid].begin());
            hprefetchReqQ[stid].clear();
        } else if (hprefetchReqQ[stid].size() != 0) {
            addr = hprefetchReqQ[stid][0];
            hprefetchReqQ[stid].erase(hprefetchReqQ[stid].begin());
        }

        if (prefetchMissQueue->FindReqByAddress(addr, stid)) {
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << "STID: " << std::dec << stid << ", addr: 0x" << std::hex << addr
                                                << " has been recored, skip.";
            continue;
        }

        assert(addr != UINT64_MAX);

        // if ((addr ) == static_cast<uint64_t>(-1ULL)) {
        //     addr = 0;
        // }

        auto req = VecTileRegLdReq();
        req.SetStid(stid);
        req.SetReqId(tileReqId);
        req.SetSize(MAX_TILE_DATA_BYTE);
        req.SetSrc(addr);
        LOG_INFO_M(Unit::VECTOR, Stage::NA) << "Create req, stid: " << dec << stid << ", req: " << req;
        if (!prefetchMissQueue->addMissRequest(addr, stid, tileReqId, 0)) {
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << "Drop req: " << req << ", stid: " << std::dec << stid;
            continue;
        }
        if (!vtop->pTileUtils->IsRingOrXbarMode(false)) {
            req.SetStashFlag();
            this->tileRegLdReqQ->Write(req);
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
                << "] Load prefetch req to, tileReqId:" << tileReqId << hex << ", addr:0x"<< addr << dec;
        } else {
            std::shared_ptr<CrossStation> station = nullptr;
            if (vtop->pTileUtils->configs.closest_injection) {
                station = vtop->GetFreeReadStation(req.GetSrc(), false);
            } else {
                station = vtop->GetFreeReadStation();
            }
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << "Create RING req, stid: " << dec << stid << ", req: " << req;
            std::shared_ptr<Request> pkt = station->Rxreq(req.GetSrc(), req.GetStid(), vtop->m_coreId);
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << "Ring pkt: " << *pkt;
            pkt->SetSize(req.GetSize());
            pkt->SetBufId(req.GetReqId());
            pkt->SetPEType(MachineType::VECTOR);
            pkt->SetPrefetchReq(true);
            tileReqId++;
            station->WriteSpb(pkt);
            if (station->GetId() == vtop->ldRingport0Id) {
                vtop->m_stats.tile_port0_load_request++;
            } else {
                vtop->m_stats.tile_port1_load_request++;
            }
            vtop->m_stats.tileTotalRdRequest++;
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId << "] Load prefetch req to Ring "
                << hex << *pkt << dec;
        }
    }
}

void VectorCoreTA::ProcessPrefetchQ(std::vector<uint64_t> &reqQ, uint32_t stid)
{
    auto reqBegin = reqQ.begin();
    while (reqBegin != reqQ.end()) {
        uint64_t addr = *reqBegin;
        if (prefetchMissQueue->FindReqByAddress(addr, stid)) {
            reqQ.erase(reqBegin);
            continue;
        }
        if (!vtop->HasValidSpbReadEntry(addr, true)) {
            reqBegin++;
            continue;
        }
        reqQ.erase(reqBegin);

        assert(addr != UINT64_MAX);
        if ((addr | TILE_MASK) == static_cast<uint64_t>(-1ULL)) {
            addr = 0;
        }

        auto req = VecTileRegLdReq();
        req.SetReqId(tileReqId);
        req.SetSize(MAX_TILE_DATA_BYTE);
        req.SetSrc(addr);
        req.SetStid(stid);
        if (!prefetchMissQueue->addMissRequest(addr, stid, tileReqId, 0)) {
            continue;
        }
        if (!vtop->pTileUtils->IsRingOrXbarMode(false)) {
            req.SetStashFlag();
            this->tileRegLdReqQ->Write(req);
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
                << "] Load prefetch req to, tileReqId:" << tileReqId << hex << ", addr:0x"<< addr << dec;
        } else {
            std::shared_ptr<CrossStation> station = vtop->GetFreeReadStation(req.GetSrc(), true);
            std::shared_ptr<Request> pkt = station->Rxreq(req.GetSrc(), req.GetStid(), vtop->m_coreId);
            pkt->SetSize(req.GetSize());
            pkt->SetBufId(req.GetReqId());
            pkt->SetPEType(MachineType::VECTOR);
            pkt->SetPrefetchReq(true);
            tileReqId++;
            station->WriteSpb(pkt);
            if (station->GetId() == vtop->ldRingport0Id) {
                vtop->m_stats.tile_port0_load_request++;
            } else {
                vtop->m_stats.tile_port1_load_request++;
            }
            vtop->m_stats.tileTotalRdRequest++;
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId << "] Load prefetch req to Ring "
                << hex << *pkt << dec;
        }
    }
}

void VectorCoreTA::WakeupPrefetchQueue(TileRegVecLdRes req)
{
    std::vector<uint32_t> missReqEntryIds;
    prefetchMissQueue->GetMergeReqEntryId(req.GetRespId(), req.GetStid(), missReqEntryIds);

    for (auto it : missReqEntryIds) {
        FillWakeMergeLoad(it, req);
    }
    // 从miss queue 删除
    prefetchMissQueue->removeOnFill(req.GetRespId(), req.GetStid());
}
// -------------------------------------------TBuffer 功能------------------------------------------------------
// tbuffer 功能: (1) load miss -> tileregister -> fill -> tubbufer
//              (2) load hit -> fetch data
// 地址分解
void VectorCoreTA::decodeAddress(uint32_t address, uint32_t& tag, uint32_t& index, uint32_t& offset)
{
    offset = address & ((1 << cacheOffsetBits) - 1); // cacheline offset
    if (!vtop->GetConfig().stash_mode) {
        index = (address >> cacheOffsetBits) & ((1 << cacheIndexBits) - 1);
        tag = (address >> (cacheOffsetBits + cacheIndexBits));
    } else {
        index = (address >> cacheOffsetBits) & (nTaSets - 1);
        tag = (address >> (cacheOffsetBits + count_bits_log(nTaSets)));  // 1M 地址， 高8位：tag
        ASSERT(index >= 0 && index < nTaSets);
    }
}

bool VectorCoreTA::IsTBufferHit(const LaneAddr& lane, std::vector<uint8_t>& result, bool& hitStash)
{
    if (lane.bytes == 0 || lane.bytes > 4) {
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "Invalid lane size: " << lane.bytes;
        return false;
    }

    uint32_t tag;
    uint32_t index;
    uint32_t offset;
    uint64_t addr = pTileUtils->Addr2Bytes(lane.addr);
    decodeAddress(addr, tag, index, offset);

    if (index >= (vtop->GetConfig().stash_mode ? nTaSets : nSets)) {
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "Index out of range: " << index << " >= " << nSets;
        return false;
    }
    if (offset + lane.bytes > entrySizes) {
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "Address cross cacheline boundary: offset=" << offset
            << ", bytes=" << lane.bytes;
        // TODO：先识别， 需要细粒度实现
        // ASSERT 0
        return false;
    }

    if (vtop->GetConfig().stash_mode) {
        bool hitTO = false;
        auto way = GetBlockWay(addr, hitTO);
        LOG_DEBUG_M(Unit::VECTOR, Stage::E2) << "Hit TO Cache:"<< hitTO << ", Load Cache: Address=0x" << std::hex <<
            addr << " (Set:" << std::dec << index << ", Way:" << way << ")";
        result.resize(lane.bytes);
        if (hitTO) {
            ASSERT(vtop->GetConfig().stash_to_enable);
            scb->MergeTOData(addr, way, lane.bytes, result);
            vtop->m_stats.tileLoadHitTo++;
            return true;
        }
        auto set = inputCache[index];
        auto& cache_line = set[way];
        ASSERT(cache_line.valid);
        std::copy_n(cache_line.data.data() + offset, lane.bytes, result.data());
        return true;
    } else {
        return (IsNormalCacheHit(lane, result, hitStash));
    }

    return false;
}

bool VectorCoreTA::IsNormalCacheHit(const LaneAddr& lane, std::vector<uint8_t>& result, bool& hitStash)
{
    uint32_t tag;
    uint32_t index;
    uint32_t offset;
    uint64_t addr = pTileUtils->Addr2Bytes(lane.addr);
    decodeAddress(addr, tag, index, offset);
    auto& set = tBuffer[index];
    for (uint32_t i = 0; i < nWays; ++i) {
        auto& cache_line = set[i];
        if (cache_line.valid && cache_line.tag == tag) {
            if (cache_line.isStash) {
                hitStash = true;
            }
            if (vtop->GetConfig().TileCacheReplacementPolicies == 0) {
                UpdateLRU(index, i);
            }
            LOG_DEBUG_M(Unit::VECTOR, Stage::E2) << "Hit Load Cache: Address=0x" << std::hex << lane.addr
                << " (Set:" << std::dec << index << ", Way:" << i << ")";
            result.resize(lane.bytes);
            std::copy_n(cache_line.data.data() + offset, lane.bytes, result.data());

            ++cache_line.hitCnt;
            return true;
        }
    }
    return false;
}

bool VectorCoreTA::IsNormalCacheHit(uint64_t addr)
{
    uint32_t tag;
    uint32_t index;
    uint32_t offset;
    decodeAddress(addr, tag, index, offset);
    auto& set = tBuffer[index];
    for (uint32_t i = 0; i < nWays; ++i) {
        auto& cacheLine = set[i];
        if (cacheLine.valid && cacheLine.tag == tag) {
            return true;
        }
    }
    return false;
}

uint64_t VectorCoreTA::GetHitWay(uint32_t address)
{
    uint32_t tag;
    uint32_t index;
    uint32_t offset;
    decodeAddress(address, tag, index, offset);
    if (index >= nSets || index < 0) {
        LOG_ERROR("index out of range");
        std::cout << "index:" << index << ", address:0x" << hex << address << dec << endl;
        ASSERT(0);
        return -1;
    }
    uint64_t way = findLRUWay(index);
    if (tBuffer[index][way].valid) {
        vtop->m_stats.tileCacheEvictCnt++;
        if (tBuffer[index][way].isStash && tBuffer[index][way].hitCnt == 0) {
            vtop->m_stats.tileLoadBadPrefetchCnt++;
        }
        ResetEntry(index, way);
    }
    LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "get load way, set=" << index << ", way=" << way << ", addr=0x" << hex <<
        address << dec;
    return way;
}

void VectorCoreTA::GetHitInfo(uint32_t address, uint64_t &set, uint64_t &way)
{
    uint32_t tag;
    uint32_t index;
    uint32_t offset;
    decodeAddress(address, tag, index, offset);
    if (index >= nSets) return;
    for (uint64_t i = 0; i < nWays; i++) {
        if (tBuffer[index][i].valid && tBuffer[index][i].tag == tag) {
            set = index;
            way = i;
            return;
        }
    }
}

// 更新LRU信息
void VectorCoreTA::UpdateLRU(uint64_t set, uint64_t way)
{
    if (set < nSets && way < nWays) {
        tBuffer[set][way].aaccelss_time = ++global_time;
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "update lru, set=" << set << ", way=" << way << ", aaccelss_cnt=" <<
            global_time;
    } else {
        ASSERT(0 && " set/way value is over MAX!");
    }
}

uint64_t VectorCoreTA::findLRUWay(uint64_t set)
{
    ASSERT(set < nSets);
    uint64_t lru_way = 0;
    uint32_t min_time = tBuffer[set][0].aaccelss_time;

    for (uint64_t i = 1; i < nWays; i++) {
        if (!tBuffer[set][i].valid) {
            return i;
        }
    }

    for (uint64_t i = 1; i < nWays; i++) {
        if (tBuffer[set][i].aaccelss_time < min_time) {
            min_time = tBuffer[set][i].aaccelss_time;
            lru_way = i;
        }
    }
    return lru_way;
}

void VectorCoreTA::DrainTBuffer(ROBID &bid, uint32_t stid)
{
    for (uint64_t i = 0; i < nSets; i++) {
        for (uint64_t j = 0; j < nWays; j++) {
            if (tBuffer[i][j].bid == bid && tBuffer[i][j].stid == stid) {
                vtop->m_stats.tileCacheEvictCnt++;
                tBuffer[i][j].Reset();
            }
        }
    }
}

void VectorCoreTA::printData(const uint8_t* data, size_t length)
{
    std::cout << std::hex << std::setfill('0');
    for (size_t i = 0; i < length; ++i) {
        std::cout << std::setw(2) << static_cast<int>(data[i]) << " ";
    }
    std::cout << std::dec << std::endl;
}

void VectorCoreTA::FillData(uint32_t address, TileRegVecLdRes req)
{
    uint32_t tag = 0;
    uint32_t index = 0;
    uint32_t offset = 0;
    decodeAddress(address, tag, index, offset);
    uint64_t way = 0;
    if (vtop->GetConfig().stash_mode) {
        // (1) 获取way id
        // (2) 获取原始address, 更新cacheline tag
        way = req.GetWay();
        ASSERT(index < nTaSets);
        if (!(way >= 0 && way < nTaWays)) {
            cout << "Wrong Way, Write Addr=0x" << hex << address << " Set=" << dec << index << ", Way=" << way << endl;
            ASSERT(0);
        }
        inputCache[index][way].valid = true;
        inputCache[index][way].tag = tag;
        inputCache[index][way].stid = req.GetStid();
        auto freeMemSize = inputCache[index][way].data.capacity() - offset;
        ASSERT(freeMemSize >= static_cast<decltype(freeMemSize)>(offset) && "Insufficient free memory!");
        std::copy_n(req.GetData() + offset, freeMemSize, inputCache[index][way].data.data() + offset);
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "Write Addr=0x" << hex << address << " (Set=" << dec << index <<
            ", Way="<< way << ")";
    } else {
        way = GetHitWay(address);
        ASSERT(index < nSets);
        if (!(way >= 0 && way < nWays)) {
            cout << "Wrong Way, Write Addr=0x" << hex << address << " Set=" << dec << index << ", Way=" << way << endl;
            ASSERT(false);
        }
        tBuffer[index][way].valid = true;
        tBuffer[index][way].tag = tag;
        tBuffer[index][way].isStash = req.IsPrefetch();
        tBuffer[index][way].addr = address;
        tBuffer[index][way].stid = req.GetStid();
        UpdateLRU(index, way);
        auto freeMemSize = tBuffer[index][way].data.capacity() - offset;
        ASSERT(freeMemSize >= static_cast<decltype(freeMemSize)>(offset) && "Insufficient free memory!");
        std::copy_n(req.GetData() + offset, freeMemSize, tBuffer[index][way].data.data() + offset);
        string str = req.IsPrefetch() ? "prefetch" : "demand";
        LOG_INFO_M(Unit::VECTOR, Stage::NA) << str << " Write Addr=0x" << hex << address << " (Set=" << dec << index <<
            ", Way="<< way << ")";
    }

    if (dumpCacheData) {
        printData(req.GetData(), offset);
    }
}

void VectorCoreTA::ResetEntry(uint64_t set, uint64_t way)
{
    if (tBuffer[set][way].valid) {
        tBuffer[set][way].valid = false;
        tBuffer[set][way].Reset();
        tBuffer[set][way].data.clear();
    }
}


void VectorCoreTA::DrainAllEntry()
{
    // 目前 tile cache只缓存load data， 不需要drain空
    return;
    auto processModifiedEntry = [&](uint64_t set, uint64_t way, TAEntry& entry) {
        uint64_t id = tileReqId++;
        VecTileRegStReq req;

        req.SetReqId(id);
        req.SetSize(MAX_TILE_DATA_BYTE);
        req.SetDest(entry.addr);
        req.SetData(entry.data.data(), 256);
        writeBackQ[static_cast<uint64_t>(TAWriteType::EVICT)].Write(req);
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "Drain cache line: set=" << set << ", way=" << way
            << ", Writing back to tileRegister addr=0x" << std::hex << entry.addr << std::dec;
    };

    for (uint64_t set = 0; set < nSets; set++) {
        for (uint64_t way = 0; way < nWays; way++) {
            TAEntry& entry = tBuffer[set][way];
            if (entry.valid && entry.state == TAEntryState::MODIFIED) {
                processModifiedEntry(set, way, entry);
            }
        }
    }
}

void VectorCoreTA::DumpTBufferState()
{
    cout << "=== TBuffer State ===" << endl;
    for (uint64_t s = 0; s < nSets; s++) {
        cout << "Set " << dec << s << ":" << endl;
        for (uint64_t w = 0; w < nWays; w++) {
            TAEntry& entry = tBuffer[s][w];
            if (entry.valid) {
                cout << "  Way " << w << ": Tag=0x" << hex << setw(2) << setfill('0')
                        << (int)entry.tag << ", Valid=" << entry.valid
                        << ", AaccelssTime=" << dec << entry.aaccelss_time << endl;
            } else {
                cout << "  Way " << w << ": Invalid" << endl;
            }
        }
    }
}

// ---------------------------------------load queue------------------------------------------------------------

void VectorCoreTA::AllocLoadQueue(MemRequest &req)
{
    uint32_t index = GetLdqFreeId();
    loadQueue[index].SetMemReq(req);
    loadQueue[index].SetAllocCycle(GetSim()->getCycles());
    loadQueue[index].stage = VecTAStage::STAGE_E1;
    loadQueue[index].status = VecTAReqStatus::PENDING;
    ldqEntryOccCnt++;
    vtop->m_stats.tile_total_load_inst++;
    auto &entry = loadQueue[index];
    if (entry.req.tileLdqCyc == 0) {
        entry.req.tileLdqCyc = GetSim()->getCycles();
    }
    LOG_INFO_M(Unit::VECTOR, Stage::E1) << " [VECTOR " << vtop->m_coreId << "]Recv load req: " << req << dec
        << ", load queue Id=" << index;
    std::stringstream split_str;
    split_str << "Tile Load Split req, width:" << req.width << " " <<endl;
    for (uint32_t lane = 0; lane < req.lanes; ++lane) {
        uint64_t addr = (req.addrs.Get(lane, static_cast<uint32_t>(OperandWidth::OPDW_D)));
        if (addr > 0x27FFFF) {
            addr = addr & 0x27FFFF;
        }
        bool isMisalign = isMisaligned(addr, req.width);
        uint64_t align_addr = addr & addrMask;
        if (lane > 0 && lane % 8 == 0) {
            split_str << ", Lane_id:" << lane << ", addr: 0x" << hex << addr <<", align:0x"<< align_addr <<dec<<endl;
        } else {
            split_str << "Lane_id:" << lane << ", addr: 0x" << hex << addr << ", align:0x"<< align_addr <<", "<<dec;
        }
        entry.laneAddrVec.push_back(LaneAddr(lane, addr, align_addr, req.width, req.stid));
        entry.ldLaneStateVec.push_back(LaneAddr(lane, addr, align_addr, req.width, req.stid));
        entry.laneAddrResl.push_back(LaneAddr(lane, addr, align_addr, req.width, req.stid));
        // reqMap 以256B 地址对齐
        // 1. 地址是misaglign
        // 2. 不是misaglign
        bool found = false;
        for (const auto& addr_item: entry.alignAddrs) {
            if (addr_item.addr == align_addr) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (vtop->GetConfig().dynamic_stash_mode) {
                uint32_t trainAddr = (align_addr + vtop->GetConfig().prefetch_stride * 256);
                TrainPf(trainAddr, vtop->GetConfig().prefetch_size, entry.req.bid, req.stid);
            }
            entry.alignAddrs.push_back(VecAlignReqInfo(align_addr, req.stid));
            entry.alignAddrs.back().e1Cycle = GetSim()->getCycles();
        }
        // 3. 是
        if (isMisalign) {
            uint64_t next_align_addr = align_addr + entrySizes;
            found = false;
            for (const auto& addr_item: entry.alignAddrs) {
                if (addr_item.addr == next_align_addr) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                entry.alignAddrs.push_back(VecAlignReqInfo(next_align_addr, req.stid));
                entry.alignAddrs.back().e1Cycle = GetSim()->getCycles();
            }
        }
    }
    // for count
    entry.addrPmuCnt.addrCnt = entry.alignAddrs.size();
    entry.oriAddrsCnt = entry.alignAddrs.size();
    vtop->m_stats.tile_total_load_request += entry.alignAddrs.size();
    if (entry.alignAddrs.size() == 1) {
        vtop->m_stats.tile_total_continious_load_inst++;
    } else {
        vtop->m_stats.tile_total_gather_load_inst++;
    }
    LOG_DEBUG_M(Unit::VECTOR, Stage::E1) << split_str.str();
    LOG_INFO_M(Unit::VECTOR, Stage::E1) << " [VECTOR " << vtop->m_coreId << "]Generate load req num: "
        << entry.alignAddrs.size() << entry.req << dec << " " << entry.GetRemainAddrs();
}

void VectorCoreTA::ReceiveBridgeResp()
{
    while (!m_bridgeVecLdRetQ->Empty()) {
        ldOutputQ->Write(m_bridgeVecLdRetQ->Read());
    }

    while (!m_bridgeVecStRslvQ->Empty()) {
        stOutputQ->Write(m_bridgeVecStRslvQ->Read());
    }
}

void VectorCoreTA::ResolveLdqEntry(uint32_t id, uint32_t stid)
{
    ROBID oldestBID = GetSim()->core->bctrl->blockROB.getOldestBlockID(stid);
    BlockCommandPtr cmd = GetSim()->core->bctrl->blockROB.GetBlockCMDPtr(oldestBID, stid);
    if (IsBlockTypeParallel(cmd->blockType)) {
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "[TA Load Pipe E3 Stage] Because load requests resolve. Reset the idle"
            << " cycle.";
        GetSim()->ResetWaitCycle();
    }
    loadQueue[id].SetEraseCycle(GetSim()->getCycles());
    uint32_t load_to_use = loadQueue[id].GetEraseCycle() - loadQueue[id].GetAllocCycle();
    vtop->m_stats.tile_total_load_lat += load_to_use;
    if (load_to_use <= 10) {
        vtop->m_stats.tile_load_to_use[0]++;
    } else if (load_to_use <= 20) {
        vtop->m_stats.tile_load_to_use[1]++;
    } else if (load_to_use <= 30) {
        vtop->m_stats.tile_load_to_use[2]++;
    } else if (load_to_use <= 50) {
        vtop->m_stats.tile_load_to_use[3]++;
    } else {
        vtop->m_stats.tile_load_to_use[4]++;
    }
    LOG_INFO_M(Unit::VECTOR, Stage::E3) << " [VECTOR " << vtop->m_coreId << "]Tile Load Resolved, "
        << loadQueue[id].req;
    loadQueue[id].req.e3Cyc = GetSim()->getCycles();
    ldOutputQ->Write(loadQueue[id].req);
}

void VectorCoreTA::DeallocateLdqEntry(uint32_t id)
{
    vtop->m_stats.tile_lsu_ldq_deallocate++;
    vtop->m_stats.tile_lsu_avg_lhq_lifetime += (GetSim()->getCycles() - loadQueue[id].GetAllocCycle());
    loadQueue[id].valid = false;
    ldQ_credit++;
    loadQueue[id].Reset();
    ldqEntryOccCnt--;
}

void VectorCoreTA::FillWakeMissLoad(uint32_t id, TileRegVecLdRes req)
{
    auto &it = loadQueue[id];
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "TA Load Refill from ring, Wake up loadQueue entryId=" << it.entryId
        << ", reqId=" << req.GetRespId() << ", stage=" << EnumToString(it.stage) << " " << it.req << dec;

    auto fill = it.loadIds.find(req.GetRespId());
    if (fill == it.loadIds.end()) {
        return; // entry is flushed by inner branch
    }

    auto processLaneData = [&] (VecTARequest& entry, uint64_t align_addr) {
        for (auto lane = entry.laneAddrResl.begin(); lane != entry.laneAddrResl.end();) {
            if (lane->alignAddr == align_addr) {
                std::vector<uint8_t> result(lane->bytes);
                auto freeMemSize = result.capacity();
                ASSERT(freeMemSize >= static_cast<decltype(freeMemSize)>(lane->bytes) && "Insufficient free memory!");
                std::copy(req.GetData() + (lane->addr - lane->alignAddr),
                    req.GetData() + (lane->addr - lane->alignAddr) + lane->bytes, result.begin());
                entry.req.data.SetVal(result, lane->laneId, lane->bytes);
                entry.ldLaneStateVec[lane->laneId].dataFromTile = true;
                lane = entry.laneAddrResl.erase(lane);
            } else {
                lane++;
            }
        }
    };

    processLaneData(it, fill->second);
    it.req.tileLdRefillCyc = GetSim()->getCycles();
    it.loadIds.erase(fill);
    if (it.loadIds.empty() && it.alignAddrs.empty()) {
        ResolveLdqEntry(it.entryId, it.req.stid);
        ++resolveCnt;
        if (vtop->GetConfig().load_to_use_enable) {
            DoLoadWakeUp(it.req);
        }
        if (!vtop->GetConfig().ldq_deallocate_by_nonflush) {
            LOG_INFO_M(Unit::VECTOR, Stage::E3) << " [VECTOR " << vtop->m_coreId
                << "]Tile Load Queue Deallocated, Req: " << it.req << dec;
            DeallocateLdqEntry(it.entryId);
        } else {
            it.status = VecTAReqStatus::WAIT_OLDEST;
        }
    }
}

void VectorCoreTA::FillWakeMergeLoad(uint32_t id, TileRegVecLdRes req)
{
    auto &it = loadQueue[id];
    uint64_t reqId = req.GetRespId();
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "TA Load Refill from ring, Wake up merge entryId=" << it.entryId <<
        ", reqId=" << reqId << ", " << it.req << ", status=" << EnumToString(it.status);
    uint64_t alignAddr = 0;
    auto itDel = std::find_if(it.alignAddrs.begin(), it.alignAddrs.end(),
        [reqId, &req](const VecAlignReqInfo& entry) {
            return entry.valid && entry.missReqId == reqId && entry.stid == req.GetStid();
        });
    if (itDel != it.alignAddrs.end()) {
        alignAddr = itDel->addr;
        it.alignAddrs.erase(itDel);
    } else {
        return; // entry is flushed by inner branch
    }

    auto processLaneData = [&](VecTARequest& entry, uint64_t align_addr) {
        for (auto lane = entry.laneAddrResl.begin(); lane != entry.laneAddrResl.end();) {
            if (lane->alignAddr == align_addr) {
                std::vector<uint8_t> result(lane->bytes);
                auto freeMemSize = result.capacity();
                ASSERT(freeMemSize >= static_cast<decltype(freeMemSize)>(lane->bytes) && "Insufficient free memory!");
                std::copy(req.GetData() + (lane->addr - lane->alignAddr),
                    req.GetData() + (lane->addr - lane->alignAddr) + lane->bytes, result.begin());
                entry.req.data.SetVal(result, lane->laneId, lane->bytes);
                entry.ldLaneStateVec[lane->laneId].dataFromTile = true;
                lane = entry.laneAddrResl.erase(lane);
            } else {
                lane++;
            }
        }
    };

    processLaneData(it, alignAddr);
    it.req.tileLdRefillCyc = GetSim()->getCycles();
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "Tile req wake up merge entryId=" << it.entryId << ", alignAddrs size="
            << it.alignAddrs.size() << ", loadIds size=" << it.loadIds.size();
    if (it.alignAddrs.empty() && it.loadIds.empty()) {
        if (vtop->GetConfig().load_to_use_enable) {
            DoLoadWakeUp(it.req);
        }
        ++resolveCnt;
        ResolveLdqEntry(it.entryId, it.req.stid);
        if (!vtop->GetConfig().ldq_deallocate_by_nonflush) {
            LOG_INFO_M(Unit::VECTOR, Stage::E3) << " [VECTOR " << vtop->m_coreId
                << "]Tile Load Queue Deallocated, Req: " << it.req << dec;
            DeallocateLdqEntry(it.entryId);
        } else {
            it.status = VecTAReqStatus::WAIT_OLDEST;
        }
    }
}

void VectorCoreTA::DumpLoadQueue()
{
    std::cout << "dump load queue" << std::endl;
    for (auto& load : loadQueue) {
        if (!load.valid) {
            continue;
        }
        std::cout << load.req << dec << ", entryId=" << load.entryId << ", total_req=" << load.oriAddrsCnt
                  << ", handle_cnt=" << load.checkAddrCnt << ", alloc_ldq=" << load.GetAllocCycle()
                  << ", status=" << EnumToString(load.status) << std::endl;
    }
}

void VectorCoreTA::StatsCount()
{
    // for stats, stats size is 4
    ASSERT(threadNum <= 4);
    unsigned ldq_occ_total = 0;
    unsigned stq_occ_total = 0;
    for (unsigned tid = 0; tid < threadNum; tid++) {
        ldq_occ_total += ldqEntryOccCnt;
        stq_occ_total += stqEntryOccCnt[tid];
        if (stqEntryOccCnt[tid] == stqPerThreSize) {
            vtop->m_stats.tile_lsu_stq_th_full[tid]++;
        }
    }
    vtop->m_stats.tile_lsu_avg_lhq_occ += ldq_occ_total;
    vtop->m_stats.tile_lsu_avg_stq_occ += stq_occ_total;
    vtop->m_stats.tile_lsu_avg_scb_occ += scb->GetOcc();

    if (ldq_occ_total == ldqPerThreSize) {
        vtop->m_stats.tile_lsu_ldq_full++;
    }
    if (stq_occ_total == (stqPerThreSize * threadNum)) {
        vtop->m_stats.tile_lsu_stq_full++;
    }
    for (uint32_t stid = 0; stid < scb->isInDrainMode.size(); ++stid) {
        if (scb->isInDrainMode[stid]) {
            vtop->m_stats.toCacheDrainCycle++;
        }
    }
}

void VectorCoreTA::WakeupLoadQueue(TileRegVecLdRes req)
{
    // 遍历miss queue, 匹配reqId, 存放有entryId
    uint32_t missReqEntryId = UINT32_MAX;
    uint32_t missAddr = 0;
    // 处理miss req
    loadMissQueue->GetMissReqEntryId(req.GetRespId(), req.GetStid(), missReqEntryId, missAddr);
    // 被flush掉的pending请求需要判断
    if (missReqEntryId != UINT32_MAX) {
        if (loadQueue[missReqEntryId].valid == true) {
            if (use_tile_cache) {
                FillData(missAddr, req);
            }
            FillWakeMissLoad(missReqEntryId, req);
        }
    } else {
        std::cout << "assertion, reqId=" << req.GetRespId() << std::endl;
        ASSERT(0);
    }
    // 处理merge miss req
    std::vector<uint32_t> missReqEntryIds;
    loadMissQueue->GetMergeReqEntryId(req.GetRespId(), req.GetStid(), missReqEntryIds);

    for (auto it : missReqEntryIds) {
        LOG_INFO_M(Unit::VECTOR, Stage::NA) << "Recv req: " << req << ", and wake up corresponding reqs: " << it;
        FillWakeMergeLoad(it, req);
    }
    // 从miss queue 删除
    loadMissQueue->removeOnFill(req.GetRespId(), req.GetStid());
}

// ---------------------------------------store queue------------------------------------------------------------
uint32_t VectorCoreTA::GetLdqFreeId()
{
    for (uint32_t i = 0; i < vtop->GetConfig().ta_ldq_size; i++) {
        if (!loadQueue[i].valid) {
            ASSERT(loadQueue[i].status == VecTAReqStatus::INVALID);
            return i;
        }
    }
    ASSERT(0 && "no aval entry id!");
    return -1;
}

// ---------------------------------------store queue------------------------------------------------------------

uint32_t VectorCoreTA::GetStqFreeId(uint32_t _tid)
{
    for (uint32_t i = vtop->GetConfig().ta_stq_size*_tid; i <= vtop->GetConfig().ta_stq_size*(_tid+1)-1; i++) {
            if (!storeQueue[i].valid) {
                ASSERT(storeQueue[i].status == VecTAReqStatus::INVALID);
                return storeQueue[i].entryId;
            }
    }
    ASSERT(0 && "no aval entry id!");
    return UINT32_MAX;
}

void VectorCoreTA::AllocStoreQueue(MemRequest &req, uint32_t _tid)
{
    uint32_t index = req.uinst->vcSid.val % vtop->GetConfig().ta_stq_size + _tid * vtop->GetConfig().ta_stq_size;
    if (storeQueue[index].valid) {
        LOG_DEBUG_M(Unit::VECTOR, Stage::E1) << "[VECTOR " << vtop->m_coreId
            << "] ERROR: Attempting to allocate valid STQ entry! index=" << index
            << ", existing entry: " << storeQueue[index].req
            << ", isVscatter=" << storeQueue[index].isVscatter
            << ", status=" << EnumToString(storeQueue[index].status);
    }
    ASSERT(!storeQueue[index].valid);

    storeQueue[index].SetMemReq(req);

    if (req.uinst->isVgather) {
        storeQueue[index].SetVgather();
        storeQueue[index].SetGroupSlotId(req.uinst->groupSlotId);
    }

    if (req.uinst->accMemInfo && !req.uinst->accMemInfo->local &&
        !req.uinst->isVgather) {
            req.uinst->isVscatter = true;
        }

    if (req.uinst->isVscatter) {
        storeQueue[index].SetVscatter();
        storeQueue[index].SetGroupSlotId(req.uinst->groupSlotId);
        LOG_INFO_M(Unit::VECTOR, Stage::E1) << "[VECTOR " << vtop->m_coreId
            << "] memreq received is vscatter, GroupSlotId: " << req.uinst->groupSlotId;
    }

    if (!vtop->GetConfig().enable_scb) {
        storeQueue[index].stage = VecTAStage::STAGE_E1;
    }
    storeQueue[index].SetAllocCycle(GetSim()->getCycles());
    storeQueue[index].status = VecTAReqStatus::WAIT_OLDEST;
    vtop->m_stats.tile_total_store_inst++;
    stqEntryOccCnt[_tid]++;
    auto &entry = storeQueue[index];
    LOG_INFO_M(Unit::VECTOR, Stage::E1) << "[VECTOR " << vtop->m_coreId << "] Tile lsu Recv store req:" << req
        << ", stq entryId=" << index;
    auto addAlignAddr = [&](uint32_t lane, uint64_t addr, uint64_t alignAddr, uint64_t data, uint32_t width,
        uint32_t stid) {
        // Merge data
        bool found = false;
        for (auto& laneAddrMem : entry.laneAddrVec) {
            if (laneAddrMem.alignAddr == alignAddr) {
                uint64_t start = addr - alignAddr;
                uint64_t end = start + width;
                for (uint32_t i = start; i < end; ++i) {
                    laneAddrMem.data[i] = static_cast<uint8_t>((data >> ((i - start) * BITS_IN_BYTE)) & 0xFF);
                }
                laneAddrMem.bytes += width;
                laneAddrMem.addr = addr < laneAddrMem.addr ? addr : laneAddrMem.addr;
                laneAddrMem.mask.Set(start, end);
                found = true;
                break;
            }
        }
        // Add unique addr
        if (!found) {
            LaneAddr laneAddr(lane, addr, alignAddr, width, stid);
            entry.laneAddrVec.push_back(laneAddr);
            entry.laneAddrVec.back().data = std::vector<uint8_t>(MAX_TILE_DATA_BYTE, 0);
            uint64_t start = addr - alignAddr;
            uint64_t end = start + width;
            for (uint32_t i = start; i < end; ++i) {
                entry.laneAddrVec.back().data[i] = static_cast<uint8_t>((data >> ((i - start) * BITS_IN_BYTE)) & 0xFF);
            }
            entry.laneAddrVec.back().mask.InitWithElem(start, end, vtop->pTileUtils->configs.mask_element_size);
        }
    };

    auto processLaneAddress = [&](uint32_t lane) {
        uint64_t addr = (req.addrs.Get(lane, static_cast<uint32_t>(OperandWidth::OPDW_D)));
        if (addr > 0x27FFFF) {
            addr = addr & 0x27FFFF;
        }
        bool isMisalign = isMisaligned(addr, req.width);
        uint64_t align_addr = addr & addrMask;
        uint64_t bytes_4B = req.data.Get(lane, req.width * 8);
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "[Tile Store] Split req, LaneId: " << lane << ", addr: 0x"
            << hex << addr << ", alignAddr:0x" << align_addr << ", req.width:" << req.width << dec;
        uint32_t width = isMisalign ? align_addr + entrySizes - addr : req.width;
        addAlignAddr(lane, addr, align_addr, bytes_4B, width, req.stid);
        if (isMisalign) {
            uint64_t nextAlignAddr = align_addr + entrySizes;
            addAlignAddr(lane, nextAlignAddr, nextAlignAddr, bytes_4B >> (width * BITS_IN_BYTE), req.width - width,
                req.stid);
        }
    };

    for (uint32_t lane = 0; lane < req.lanes; ++lane) {
        vtop->m_stats.tile_total_store_byte += req.width;
        processLaneAddress(lane);
    }

    if (entry.laneAddrVec.size() == 1) {
        vtop->m_stats.tile_total_continious_store_inst++;
    } else {
        vtop->m_stats.tile_total_scatter_store_inst++;
    }
    vtop->m_stats.tile_total_store_request += entry.laneAddrVec.size();

    // VGATHER: STORE指令把GM地址写入TMU
    // VGATHER指令格式: STORE [TMU位置], GM地址
    // - SRC0 = GM地址（要写入TMU的数据）→ 地址位宽
    // - SRC1 = TMU基址寄存器
    // - SRC2/SRC3 = 偏移
    // 因此从SRC0读取地址DataType
    if (req.uinst->isVgather) {
        for (uint32_t i = 0; i < entry.laneAddrVec.size(); i++) {
            vtop->m_vectorGS->VgatherCounterPlus(req.uinst->bid, req.uinst->gid);
            vtop->m_vectorGS->vGatherQ.push_back(
                entry.req.uinst->GenVgatherReq(entry.laneAddrVec[i].alignAddr));
        }
        // ASSERT entry.laneAddrVec.size() == 1
    }

    // VSCATTER: 从指令操作数直接读取位宽
    // VSCATTER使用STORE操作数顺序
    // STORE: addr = srcs[SRC1_IDX] + srcs[SRC2_IDX], 地址基址是SRC1
    if (req.uinst->isVscatter) {
        // VSCATTER: GenVscatterReq内部直接从psrcs获取width并转换
        vtop->m_vectorGS->vGatherQ.push_back(
            entry.req.uinst->GenVscatterReq(index)
        );
        LOG_INFO_M(Unit::VECTOR, Stage::E1) << "[VECTOR " << vtop->m_coreId
            << "] VSCATTER req queued with stqIndex=" << index;
    }

    ResolveStqEntry(index, entry.req.stid);
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId << "] Tile Store generate store req num:"
        << dec << entry.laneAddrVec.size();
}

std::vector<uint8_t> VectorCoreTA::LoadCamStq(uint32_t bid, uint32_t gid, uint32_t rid, uint64_t addr,
    unsigned size, bool& hit, uint32_t stid)
{
    std::vector<uint8_t> result(size, 0);
    LaneAddr laneCam(-1, addr, addr, size, stid);

    auto isRelevantStore = [&](const VecTARequest& entry) {
        if (!entry.valid) return false;
        if (entry.req.bid.val != bid || entry.req.gid.val != gid) return false;
        if (entry.req.rid.val > rid) return false;
        return true;
    };

    auto processLane = [&](const LaneAddr& storeLane) {
        LaneDataTransfer mergeStq(laneCam, storeLane);

        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << mergeStq.PrintCoverageInfo();

        if (mergeStq.copyFromStoreToLoad()) {
            printLaneAddr(laneCam, "Load after copy");
            hit = true;
            if (laneCam.isValid()) {
                return true;
            }
        }

        return false;
    };

    // 遍历store queue
    for (auto it = storeQueue.begin(); it != storeQueue.end(); it++) {
        if (!isRelevantStore(*it)) continue;

        // 遍历每个lane
        for (size_t lane = 0; lane < it->laneAddrVec.size(); lane++) {
            if (processLane(it->laneAddrVec[lane])) {
                return laneCam.data;
            }
        }
    }

    return laneCam.data;
}

bool VectorCoreTA::IsLdqFull()
{
    return ldqEntryOccCnt == loadQueue.size();
}

bool VectorCoreTA::IsStqFull(uint32_t tid)
{
    return stqEntryOccCnt[tid] == storeQueue.size()/GetSim()->core->configs.threadCount;
}

void VectorCoreTA::WakeupStoreQueue(uint64_t reqId)
{
    for (auto it = storeQueue.begin(); it != storeQueue.end(); it++) {
        auto evict = (*it).storeIds.find(reqId);
        if (evict != (*it).storeIds.end()) {
            LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "Wake up store entryId=" << it->entryId << ", Req:" << it->req;
            if ((*it).storeIds.size() == 1) {
                (*it).status = VecTAReqStatus::WAIT_RSV;
                (*it).stage = VecTAStage::STAGE_E3;
            }
            if ((*it).storeIds.size() == 1) {
                LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "Store entryId=" << it->entryId << ", waiting to resolve. Req:"
                    << it->req;
            }
            (*it).storeIds.erase(evict);
        }
    }
}

void VectorCoreTA::StoreCamLoadQueue(const VecTARequest& entry)
{
    // for nuke,
    // 1. 比较store和load是否有地址重叠
    // 2. 如果有重叠，但load 已经是WAIT_OLDEST/WAIT_RSV 状态，表面load拿到了不正确数据，需要nuke
    auto checkLoadStoreOverlap = [&](VecTARequest& loadEntry, const VecTARequest& storeEntry) {
        for (size_t lane = 0; lane < loadEntry.ldLaneStateVec.size(); lane++) {
            LaneDataTransfer mergeStq(loadEntry.ldLaneStateVec[lane], storeEntry.laneAddrVec[lane]);

            if (mergeStq.hasOverlap() && loadEntry.ldLaneStateVec[lane].dataFromTile) {
                LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "Tile load data from tile register, nuke load, store entryId="
                            << storeEntry.entryId << ", load lane=" << lane << ", load addr=0x" << std::hex
                            << loadEntry.ldLaneStateVec[lane].addr << ", size="
                            << loadEntry.ldLaneStateVec[lane].bytes;
                loadEntry.status = VecTAReqStatus::WAIT_NUKE;
                ASSERT(0);
                return true; // Found overlap that requires nuke
            }
        }
        return false; // No nuke required
    };

    for (auto it = loadQueue.begin(); it != loadQueue.end(); it++) {
        if (!it->valid) continue;

        if (checkLoadStoreOverlap(*it, entry)) {
            return; // Early return if nuke is required
        }
    }
}

std::vector<VecTARequest> VectorCoreTA::SortStqByRID(uint32_t gid)
{
    std::vector<VecTARequest> result;

    // 筛选出相同groupId的entry
    for (const auto& entry : storeQueue) {
        if (entry.valid && entry.req.gid.val == gid) {
            result.push_back(entry);
        }
    }

    std::sort(result.begin(), result.end(), std::greater<VecTARequest>());

    return result;
}

void VectorCoreTA::ResolveStqEntry(uint32_t id, uint32_t stid)
{
    ASSERT(stid != -1U);
    LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "Tile Store Resolved. Req: " << storeQueue[id].req << dec;
    ROBID oldestBID = GetSim()->core->bctrl->blockROB.getOldestBlockID(stid);
    BlockCommandPtr cmd = GetSim()->core->bctrl->blockROB.GetBlockCMDPtr(oldestBID, stid);
    if (IsBlockTypeParallel(cmd->blockType)) {
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "[TA Load Pipe E3 Stage] store requests resolve. Reset the idle cycle.";
        GetSim()->ResetWaitCycle();
    }
    storeQueue[id].SetEraseCycle(GetSim()->getCycles());
    vtop->m_stats.tile_total_store_lat += storeQueue[id].GetEraseCycle() - storeQueue[id].GetAllocCycle();
    storeQueue[id].req.e3Cyc = GetSim()->getCycles();
    stOutputQ->Write(storeQueue[id].req);
}

void VectorCoreTA::AppendToRetireEntry(const VecTARequest& entry)
{
    // RETIRE_SCB 候选，排队写SCB
    unsigned threadId = entry.req.tid;
    ASSERT(threadId < threadNum);
    auto& it = retireScbMap_[threadId];
    ASSERT(!it.Full());
    it.Write(entry);
}

void VectorCoreTA::RetireStore()
{
    /*
    * 线程轮询，上一个线程的store req 完全retire 到scb后，切换线程
    * 1. 遍历Store Queue, 挑选RETIRE_SCB的entry
    * 2. 按retire带宽retire到SCB
    * 3. deallocate entry
    */
    unsigned checkThCnt = 0;
    for (size_t tid = 0; tid < threadNum; tid++) {
        if (retireScbMap_[scbThreadPtr].Empty()) {
            scbThreadPtr = (scbThreadPtr + 1) % threadNum;
        } else {
            break;
        }
        checkThCnt++;
    }
    if (checkThCnt == threadNum) {
        return;
    }
    LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "store retire to scb, pick thread=" << scbThreadPtr;
    uint64_t retireCnt = 0;
    auto it = retireScbMap_[scbThreadPtr].Front();
    bool noCredit = false;
    auto& entry = storeQueue[it.entryId];
    if (!entry.valid) {
        return;
    }
    if (entry.isVscatter) {
        if (entry.status == VecTAReqStatus::SEND_DATA_TILE_DONE) {
            retireScbMap_[scbThreadPtr].Read();
            DeallocateStqEntry(entry.entryId);
            scbThreadPtr = (scbThreadPtr + 1) % threadNum;
            return;
        } else {
            scbThreadPtr = (scbThreadPtr + 1) % threadNum;
            return;
        }
    }
    if (vtop->GetConfig().perfect_store_enable) {
        retireScbMap_[scbThreadPtr].Read();
        scbThreadPtr = (scbThreadPtr + 1) % threadNum;
        DeallocateStqEntry(entry.entryId);
        return;
    }
    for (auto lane = entry.laneAddrVec.begin(); lane != entry.laneAddrVec.end();) {
        if (retireCnt == vtop->GetConfig().ta_store_retire_band) {
            break;
        }
        uint64_t addr = lane->alignAddr;
        /* 之前的写法，scb不允许merge，现在通过reissue标记check
        if (scb->IsFull() && scb->CannotMerge(addr)) {
        */
        if (scb->IsFull()) {
            noCredit = true;
            break;
        }
        VecTileRegStReq req = VecTileRegStReq();
        req.SetReqId(99);
        req.SetDest(addr);
        req.SetData((*lane).data.data(), 256);
        req.SetSize(MAX_TILE_DATA_BYTE);
        req.SetDataMask((*lane).mask);
        req.SetBid(entry.req.bid);
        req.SetGid(entry.req.gid);
        req.SetRid(entry.req.rid);
        req.SetTid(entry.req.tid);
        req.SetStid(entry.req.stid);
        if (entry.GetVgather()) {
            req.SetVgather();
            req.SetGroupSlotId(entry.GetGroupSlotId());
        }

        if (entry.req.isCTInst) {
            req.SetCTInst();
        }
        scb->vecStReqQ.emplace_back(req);
        scb->HandleStore();
        retireCnt++;
        LOG_INFO_M(Unit::VECTOR, Stage::NA) << "store retire to scb, has credit=" << !noCredit << "store inst "
            << entry.req.uinst->Dump() << hex << ", addr=0x" << (*lane).alignAddr << ", " << req << dec;
        lane = entry.laneAddrVec.erase(lane);
    }
    if (entry.laneAddrVec.empty()) {
        retireScbMap_[scbThreadPtr].Read();
        scbThreadPtr = (scbThreadPtr + 1) % threadNum; // 下cycle，线程轮询
        DeallocateStqEntry(entry.entryId);
    }
}

void VectorCoreTA::DeallocateStqEntry(uint32_t id)
{
    // LOG_INFO_M(Unit::VECTOR, Stage::NA) << "Tile Store release entryId=" << id << " "<< storeQueue[id].req << dec;
    vtop->m_stats.tile_lsu_stq_deallocate++;
    vtop->m_stats.tile_lsu_avg_stq_lifetime += (GetSim()->getCycles() - storeQueue[id].GetAllocCycle());

    storeQueue[id].valid = false;
    stQ_credit[id/vtop->GetConfig().ta_stq_size]++;
    std::shared_ptr<VecPE> pe = GetSim()->core->vectorTop->GetPE(storeQueue[id].req.uinst->coreID);
    auto &prob = pe->prob[id/vtop->GetConfig().ta_stq_size];
    prob->UpdateTileLsuWindowPos(storeQueue[id].req.uinst->vcSid);
    storeQueue[id].Reset();
    stqEntryOccCnt[id/vtop->GetConfig().ta_stq_size]--;
}

void VectorCoreTA::DumpStoreQueue()
{
    std::cout << "==dump store queue at cycle " << GetSim()->getCycles() << " ==" << std::endl;
    int validCount = 0;
    for (auto entry: storeQueue) {
        if (entry.valid) {
            validCount++;
            std::cout << dec << "  entryId=" << entry.entryId << ", req=" << entry.req << ", status="
                << EnumToString(entry.status) << ", isVscatter=" << entry.isVscatter
                << ", alloc_cycle=" << entry.GetAllocCycle()
                << ", stuck=" << (GetSim()->getCycles() - entry.GetAllocCycle()) << " cycles" << std::endl;
        }
    }
    if (validCount == 0) {
        std::cout << "  (no valid entries)" << std::endl;
    }

    // 打印 retireScbMap_ 信息
    std::cout << "==dump retireScbMap at cycle " << GetSim()->getCycles() << " ==" << std::endl;
    for (auto& pair : retireScbMap_) {
        unsigned threadId = pair.first;
        auto& queue = pair.second;
        std::cout << "  Thread " << threadId << ": ";
        if (queue.Empty()) {
            std::cout << "(empty)" << std::endl;
        } else {
            std::cout << queue.Size() << " entries" << std::endl;
            // 打印队列中的每个entry
            auto& q = queue.GetRawReadData();  // 获取内部readQ队列
            for (size_t i = 0; i < q.size(); i++) {
                auto& entry = q[i];
                std::cout << "    [" << i << "] entryId=" << entry.entryId
                    << ", req=" << entry.req
                    << ", status=" << EnumToString(entry.status)
                    << ", isVscatter=" << entry.isVscatter << std::endl;
            }
        }
    }
}

bool VectorCoreTA::CheckEmptyByBid(ROBID &bid, uint32_t stid)
{
    for (auto entry: storeQueue) {
        if (entry.valid && entry.req.bid == bid && entry.req.stid == stid) {
            return false;
        }
    }

    return true;
}
// ---------------------------------------pipeline------------------------------------------------------------
void VectorCoreTA::I2()
{
    // do nothing
}

void VectorCoreTA::E1()
{
    // 从lda_pipe/sta_pipe 接收请求
    loadQ.Work();
    // load => allocate load queue
    int handled = 0;
    while (!loadQ.Empty() && handled < ldBand) {
        auto req = loadQ.Read();
        if (!IsLdqFull()) {
            AllocLoadQueue(req);
            ldInPipeCnt--;
            assert(ldInPipeCnt >= 0);
            ++handled;
        } else {
            ASSERT(0);
        }
    }

    storeQ.Work();
    handled = 0;
    // store => allocate store queue
    while (!storeQ.Empty() && handled < stBand) {
        auto req = storeQ.Read();
        if (!IsStqFull(req.tid)) {
            AllocStoreQueue(req, req.tid);
            ++handled;
        } else {
            ASSERT(0);
        }
    }
}

uint32_t VectorCoreTA::LoadArbitration()
{
    uint32_t min_time = UINT32_MAX;
    uint32_t min_entry_id = UINT32_MAX;
    for (uint32_t i = 0 ; i< loadQueue.size(); i++) {
        const auto& entry = loadQueue[i];
        if (!entry.valid) {
            continue;
        }
        bool needPick = entry.checkAddrCnt < entry.oriAddrsCnt;
        if (needPick && entry.alignAddrs.size() >= 1 && entry.req.tileLdqCyc <= min_time) {
            min_time = entry.req.tileLdqCyc;
            min_entry_id = i;
        }
    }
    return min_entry_id;
}

uint32_t VectorCoreTA::StoreArbitration()
{
    for (uint32_t _tid = arbTid+1; _tid < GetSim()->core->configs.threadCount + arbTid+1; _tid ++) {
        for (int j = vtop->GetConfig().ta_ldq_size - 1; j >= 0; j--) {
            int tmp = _tid % GetSim()->core->configs.threadCount;
            if (storeQueue[tmp*vtop->GetConfig().ta_stq_size+j].status == VecTAReqStatus::PENDING &&
                storeQueue[tmp*vtop->GetConfig().ta_stq_size+j].stage != VecTAStage::STAGE_E1 &&
                storeQueue[tmp*vtop->GetConfig().ta_stq_size+j].valid) {
                    arbTid = tmp;
                    return tmp*vtop->GetConfig().ta_stq_size+j;
            }
        }
    }
    return UINT32_MAX;
}

void VectorCoreTA::E2()
{
    for (uint32_t i = resolveCnt; i < vtop->GetConfig().ld_pipe_cnt; i++) {
        uint32_t LdIndex = LoadArbitration();
        if (LdIndex != UINT32_MAX) {
            LOG_INFO_M(Unit::VECTOR, Stage::E2) << " [VECTOR " << vtop->m_coreId << "]Tile load arb, pick entryId="
                << LdIndex << ", " << loadQueue[LdIndex].req;
            LoadHandleE2(LdIndex);
        }
    }

    if (!vtop->GetConfig().enable_scb) {
        uint32_t StIndex = StoreArbitration();
        if (StIndex != UINT32_MAX) {
            StoreHandleE2(StIndex);
        }
    }
}

void VectorCoreTA::E3()
{
    for (auto it = loadQueue.begin(); it != loadQueue.end(); it++) {
        if (((*it).stage == VecTAStage::STAGE_E3) && (it->status == VecTAReqStatus::WAIT_RSV)) {
            ResolveLdqEntry(it->entryId, it->req.stid);
            if (!vtop->GetConfig().ldq_deallocate_by_nonflush) {
                LOG_INFO_M(Unit::VECTOR, Stage::E3) << " [VECTOR " << vtop->m_coreId
                    << "]Tile Load Queue Deallocated, Req: " << it->req << dec;
                DeallocateLdqEntry(it->entryId);
            } else {
                it->status = VecTAReqStatus::WAIT_OLDEST;
            }
        }
    }
    if (!vtop->GetConfig().enable_scb) {
        for (auto it = storeQueue.begin(); it != storeQueue.end(); it++) {
            if ((*it).stage == VecTAStage::STAGE_E3 && ((*it).status == VecTAReqStatus::WAIT_RSV)) { // enable scb不走这
                DeallocateStqEntry((*it).entryId);
            }
        }
    }
}

void VectorCoreTA::E4()
{
    // do nothing
}

void VectorCoreTA::DoLoadWakeUp(MemRequest& req) const
{
    if (vtop->GetConfig().load_to_use_enable) {
        vtop->LoadWakeUp(req);
    }
}

void VectorCoreTA::LoadHandleE2(int _index)
{
    auto it = &loadQueue[_index];
    it->req.e2Cyc = GetSim()->getCycles();
    assert(it->valid);
    uint32_t align_256B_addr = UINT32_MAX;
    bool hitCache = false;
    if (vtop->GetConfig().perfect_load_enable) {
        for (auto iter = it->alignAddrs.begin(); iter != it->alignAddrs.end();) {
            iter = it->alignAddrs.erase(iter);
        }
        LOG_INFO_M(Unit::VECTOR, Stage::E2) << " [VECTOR " << dec << vtop->m_coreId << "]LoadHandleE2, Req: "
            << it->req;
        DoLoadWakeUp(it->req);
        it->status = VecTAReqStatus::WAIT_RSV;
        it->stage = VecTAStage::STAGE_E2;
        return;
    }
    for (auto lane = it->laneAddrVec.begin(); lane != it->laneAddrVec.end();) {
        bool hitStqEntry = false;
        std::vector<uint8_t> data = LoadCamStq(it->req.bid.GetVal(), it->req.gid.GetVal(),
            it->req.rid.GetVal(), lane->addr, lane->bytes, hitStqEntry, it->req.stid);

        if (hitStqEntry) {
            vtop->m_stats.tile_load_hit_stq++;
            it->req.data.SetVal(data, lane->laneId, lane->bytes);
            if (lane->isValid()) {
                lane = it->laneAddrVec.erase(lane);
                continue;
            }
        }
        std::vector<uint8_t> laneData;
        bool hitStash = false;
        if (use_tile_cache && IsTBufferHit(*lane, laneData, hitStash)) {
            align_256B_addr = lane->alignAddr;
            it->req.data.SetVal(laneData, lane->laneId, lane->bytes);
            LOG_DEBUG_M(Unit::VECTOR, Stage::E2) << "lane:" << lane->laneId << ", addr=0x" << hex << lane->addr
                << ", hit load cache " << it->req;
            lane = it->laneAddrVec.erase(lane);
            hitCache = true;
        } else {
            lane++;
        }
        auto itDel = std::find_if(it->alignAddrs.begin(), it->alignAddrs.end(),
            [align_256B_addr, &it] (const VecAlignReqInfo& entry) {
                return entry.valid && entry.addr == align_256B_addr && entry.stid == it->req.stid;
            });
        if (itDel != it->alignAddrs.end()) {
            it->checkAddrCnt++;
            if (hitStash) {
                vtop->m_stats.loadHitCacheStash++;
            } else {
                vtop->m_stats.loadHitCacheDemand++;
            }
            vtop->m_stats.tileLoadHitCache++;
            it->alignAddrs.erase(itDel);
            it->stage = VecTAStage::STAGE_E2;
            LOG_DEBUG_M(Unit::VECTOR, Stage::E2) << "remove align addr=0x" << hex << align_256B_addr << dec
                << ", stage=" <<  EnumToString(it->stage) << " " << it->req;
        }
    }
    for (auto align_256B = it->alignAddrs.begin(); align_256B != it->alignAddrs.end();) {
        if (!align_256B->valid || (align_256B->stateFSM == AddrState::HIT_CACHE) ||
            align_256B->WaitMissFill()) {
                align_256B++;
                continue;
            }

        uint32_t hitEntryId = 0;
        uint64_t hitReqId = 0;
        // 检查Miss Queue, 是否hit 到已经发送的miss req
        uint64_t addr = pTileUtils->Addr2Bytes(align_256B->addr);
        if (loadMissQueue->FindReqByAddress(addr, it->req.stid, it->entryId, hitEntryId, hitReqId)) {
            align_256B->stateFSM = AddrState::MISS_HIT_OTHER_REQUEST;
            it->status = VecTAReqStatus::WAIT_WAKEUP;
            align_256B->missReqEntryId = hitEntryId;
            align_256B->missReqId = hitReqId;
            it->addrPmuCnt.missMergeCnt += 1;
            if (it->req.tileLdMissCyc == 0) {
                it->req.tileLdMissCyc = GetSim()->getCycles();
            }
            vtop->m_stats.tile_load_hit_missq_cnt++;
            vtop->m_stats.tile_load_miss_cnt++;
            LOG_INFO_M(Unit::VECTOR, Stage::E2) << " [VECTOR " << vtop->m_coreId << "]Tile load merge entryId="
                << it->entryId << ", reqId=" << hitReqId <<", addr=0x" << hex << addr << " " << (*it).req;
            it->checkAddrCnt++;
            align_256B++;
            hitCache = false;
            continue;
        }
        if (prefetchMissQueue->FindReqByAddress(addr, it->req.stid, it->entryId, hitEntryId, hitReqId)) {
            align_256B->stateFSM = AddrState::MISS_HIT_OTHER_REQUEST;
            it->status = VecTAReqStatus::WAIT_WAKEUP;
            it->addrPmuCnt.missMergeCnt += 1;
            vtop->m_stats.tile_load_miss_cnt++;
            vtop->m_stats.tileLoadHitPrefetchRoadCnt++;
            align_256B->missReqEntryId = hitEntryId;
            align_256B->missReqId = hitReqId;
            it->checkAddrCnt++;
            align_256B++;
            hitCache = false;
            continue;
        }
        uint32_t reqId = CreateTileRegLdReq(align_256B->addr, it->entryId, it->req.stid);
        if (reqId != UINT32_MAX) {
            it->status = VecTAReqStatus::WAIT_WAKEUP;
            EraseAddrPfQ(align_256B->addr, it->req.stid);
            EraseAddrHPfQ(align_256B->addr, it->req.stid);
            it->loadIds.emplace(reqId, align_256B->addr);
            it->addrPmuCnt.missReqCnt += 1;
            if (it->req.tileLdMissCyc == 0) {
                it->req.tileLdMissCyc = GetSim()->getCycles();
            }
            vtop->m_stats.tile_load_miss_cnt++;
            it->checkAddrCnt++;
            hitCache = false;
            align_256B = it->alignAddrs.erase(align_256B);
        } else {
            align_256B++;
        }

        if (reqId != UINT32_MAX) {
            LOG_INFO_M(Unit::VECTOR, Stage::E2) << " [VECTOR " << vtop->m_coreId << "]Tile load send miss req, entryId="
                << it->entryId << ", reqId=" <<reqId<<", addr=0x" << hex << align_256B->addr << " " << (*it).req;
        }
        // 每个load pipe只发1个miss 请求
        break;
    }
    it->status = it->alignAddrs.empty() ? VecTAReqStatus::WAIT_RSP : it->status; // 等ring fill, 才能resolve
    if (hitCache && it->alignAddrs.empty() && it->loadIds.empty()) {
        DoLoadWakeUp(it->req);
        it->status = VecTAReqStatus::WAIT_RSV; // E3 resolve
    }
    if (it->alignAddrs.empty()) {
        ASSERT(it->oriAddrsCnt == it->checkAddrCnt);
    }
    LOG_DEBUG_M(Unit::VECTOR, Stage::E2) << "check status, status=" << EnumToString(it->status)
                << ", stage=" <<  EnumToString(it->stage) << " " << it->req;
}

void VectorCoreTA::StoreHandleE2(int _index)
{
    auto it = &storeQueue[_index];
    it->req.e2Cyc = GetSim()->getCycles();
    ASSERT(it->valid);
    StoreCamLoadQueue(*it);

    bool allSend = true;
    for (auto &lane : it->laneAddrVec) {
        if (lane.sended) {
            continue;
        }
        uint32_t reqId = CreateTileWriteReq(it->entryId, lane.alignAddr, lane.data, 256, lane.mask, lane.stid);
        if (reqId != UINT32_MAX) {
            lane.sended = true;
            it->storeIds.emplace(reqId, lane.alignAddr);
            LOG_INFO_M(Unit::VECTOR, Stage::E2) << " [VECTOR " << vtop->m_coreId
                << "]Tile store send req to tile register, reqId=" << reqId
                << ", bid=" << it->req.bid.GetVal() << ", gid=" << it->req.gid.GetVal()
                << ", rid=" << it->req.rid.GetVal() << hex << ", addr=0x" << lane.alignAddr;
        } else {
            allSend = false;
            break;
        }
    }
    it->status = allSend ? VecTAReqStatus::WAIT_RSP : VecTAReqStatus::PENDING;
}

void VectorCoreTA::DrainEmpty(uint32_t stid)
{
    if (!scb->isInDrainMode[stid]) {
        return;
    }
    if (vtop->GetConfig().enable_scb) {
        if (scb->IsEmpty(drainBlockId[stid], stid) && CheckEmptyByBid(drainBlockId[stid], stid) &&
            !IsDisableROBID(drainBlockId[stid])) {
            ScbDrainBus drainInfo{0, drainBlockId[stid], stid};
            drainScbRespQ[stid]->Write(drainInfo);
            scb->isInDrainMode[stid] = false;
            scb->ClearWay(drainBlockId[stid], stid);
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId << "] send resp-drain empty, bid="
                << drainBlockId[stid] << " stid " << stid;
        }
    } else {
        if (CheckEmptyByBid(drainBlockId[stid], stid)) {
            ScbDrainBus drainInfo = {0, drainBlockId[stid], stid};
            drainScbRespQ[stid]->Write(drainInfo);
            scb->isInDrainMode[stid] = false;
        }
    }
}

// ---------------------------------------iex interface----------------------------------------------

// 处理oldest
void VectorCoreTA::ReceiveNonFlush()
{
    processLoadNonFlush();
    processStoreNonFlush();
}

void VectorCoreTA::processLoadNonFlush()
{
    if (!vtop->GetConfig().ldq_deallocate_by_nonflush) {
        while (!vtop->loadNonFlushQ->Empty()) {
            vtop->loadNonFlushQ->Read();
        }
        return;
    }
    if (vtop->loadNonFlushQ->Empty()) return;
    auto isMatchingLoadEntry = [&](const VecTARequest& entry, LdNonFlushBus &req) {
        return LessEqual(entry.req.bid, entry.req.gid, entry.req.rid, req.bid, req.gid, req.rid) &&
            req.tid == entry.req.tid &&
            entry.status == VecTAReqStatus::WAIT_OLDEST;
    };
    while (!vtop->loadNonFlushQ->Empty()) {
        LdNonFlushBus req = vtop->loadNonFlushQ->Read();

        for (auto& entry : loadQueue) {
            if (isMatchingLoadEntry(entry, req)) {
                LOG_INFO_M(Unit::VECTOR, Stage::E3) << " [VECTOR " << vtop->m_coreId
                    << "]Tile Load Queue Deallocated, Req: " << entry.req << dec;
                DeallocateLdqEntry(entry.entryId);
                break;
            }
        }
    }
}

void VectorCoreTA::processStoreNonFlush()
{
    if (vtop->storeNonFlushQ->Empty()) {
        return;
    }

    auto processMatchingStoreEntries = [this](const StNonFlushBus &req) {
        std::vector<int> localStoreQueueIdx;
        for (size_t i = 0; i < storeQueue.size(); i++) {
            const auto& entry = storeQueue[i];
            if (!(entry.valid && LessEqual(entry.req.uinst->vcSid, req.lsID) && (!entry.GetVscatter()) &&
                    req.tid == entry.req.tid && entry.status == VecTAReqStatus::WAIT_OLDEST)) {
                continue;
            }
            localStoreQueueIdx.push_back(i);
        }

        std::vector<int> globalStoreQueueIdx;
        for (size_t i = 0; i < storeQueue.size(); i++) {
            const auto& entry = storeQueue[i];
            if (!(entry.valid && LessEqual(entry.req.uinst->vcSid, req.lsID) && (entry.GetVscatter()) &&
                    req.tid == entry.req.tid && entry.status == VecTAReqStatus::WAIT_OLDEST)) {
                continue;
            }
            globalStoreQueueIdx.push_back(i);
        }

        // VSCATTER 处理逻辑
        // 按 ROB ID 排序，确保按程序序处理
        std::sort(globalStoreQueueIdx.begin(), globalStoreQueueIdx.end(),
            [this](const size_t a, const size_t b) {
                auto& aentry = storeQueue[a];
                auto& bentry = storeQueue[b];
                return LessEqual(aentry.req.rid, bentry.req.rid);
            });

        // 处理 VSCATTER entry
        for (auto& idx : globalStoreQueueIdx) {
            auto& entry = storeQueue[idx];

            auto updateVscatterEntryStatus = [this, &entry, &globalStoreQueueIdx, idx]() {
                // VSCATTER 需要检查是否是最老的 entry

                // 检查是否是队列中最老的（排完序后第一个就是最老的）
                bool isOldest = (idx == globalStoreQueueIdx[0]);

                if (isOldest) {
                    AppendToRetireEntry(entry);
                    entry.status = VecTAReqStatus::SEND_REQ_TO_TMA;
                    LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << dec << vtop->m_coreId
                        << "] VSCATTER is oldest, ready to send req to TMA, sid=" << entry.req.uinst->vcSid.val
                        << ", rid=" << entry.req.rid.GetVal()
                        << ", entryId=" << entry.entryId
                        << ", Req: " << entry.req;
                    vtop->m_vectorGS->TriggerVscatter(entry.entryId); // 使用 STQ index (entryId) 查找并发送 VSCATTER req
                } else {
                    LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << dec << vtop->m_coreId
                        << "] VSCATTER not oldest, continue waiting, sid=" << entry.req.uinst->vcSid.val
                        << ", rid=" << entry.req.rid.GetVal() << ", Req: " << entry.req;
                }
            };

            updateVscatterEntryStatus();
        }

        std::sort(localStoreQueueIdx.begin(), localStoreQueueIdx.end(),
            [this](const size_t a, const size_t b) {
                auto& aentry = storeQueue[a];
                auto& bentry = storeQueue[b];
                return LessEqual(aentry.req.rid, bentry.req.rid);
            });

        for (auto& idx : localStoreQueueIdx) {
            auto& entry = storeQueue[idx];
            auto updateEntryStatus = [this, &entry]() {
                if (vtop->GetConfig().enable_scb) {
                    entry.status = VecTAReqStatus::RETIRE_SCB;
                    AppendToRetireEntry(entry);
                    LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << dec << vtop->m_coreId
                        << "]Tile Store retire scb, sid=" << entry.req.uinst->vcSid.val << ", Req: " << entry.req;
                } else {
                    entry.status = VecTAReqStatus::PENDING;
                    entry.stage = VecTAStage::STAGE_E2;
                    LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "Tile Store Switch to Pipe, Req: " << entry.req <<
                        ", is oldest, pick to E2 stage"<< dec;
                }
            };

            updateEntryStatus();
        }
    };

    auto logNonFlushRequest = [this](const StNonFlushBus &req) {
            LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "store recv non-flush, B" << req.bid.GetVal()
                      << "-G" << req.gid.GetVal() << "-T" << req.tid
                      << "-R" << req.rid.GetVal()
                      << ", lsid=" << req.lsID.GetVal();
    };
    while (!vtop->storeNonFlushQ->Empty()) {
        StNonFlushBus req = vtop->storeNonFlushQ->Read();
        logNonFlushRequest(req);
        processMatchingStoreEntries(req);
    }
}

// ---------------------------------------tileregister interface----------------------------------------------

void VectorCoreTA::ProcessDbidResponse()
{
    for (uint32_t i = 0; i < vtop->stationReadRange; i++) {
        auto &station = vtop->stations[i];
        if (station->HasNoResp(vtop->m_coreId)) {
            continue;
        }
        // Step 1: Peek数据（不弹出）
        shared_ptr<Request> pkt = station->GetDataComReqFront(vtop->m_coreId);
        if (!pkt) {
            continue;
        }
        // Step 2: 检查CHIFlit类型是否正确
        CHIFlit recievePkt = pkt->GetFlit();
        if (pkt->GetChannel() != ChannelType::RSP || recievePkt.cmd != CHICommand::CompDBIDResp) {
            // 不是VSCATTER的tile数据，跳过
            continue;
        }
        // Step 3: 提取 txnId (STQ index)，直接访问 entry
        uint32_t stqIndex = recievePkt.txnId;  // txnId 就是 STQ index
        uint32_t compGroupSlotId = recievePkt.addr;  // BPQ entry index (暂未使用)

        // Step 4: 边界检查
        if (stqIndex >= storeQueue.size()) {
            LOG_DEBUG_M(Unit::VECTOR, Stage::NA)
                << "Received DBID Response with invalid stqIndex=" << stqIndex
                << " (storeQueue size=" << storeQueue.size() << "). Skipping.";
            continue;
        }

        // Step 5: O(1) 直接访问 entry
        VecTARequest* entry = &storeQueue[stqIndex];

        // Step 6: 验证 entry 是否有效
        if (!entry->valid || !entry->isVscatter) {
            LOG_DEBUG_M(Unit::VECTOR, Stage::NA)
                << "Received DBID Response but entry invalid or not VSCATTER: "
                << "stqIndex=" << stqIndex << ", valid=" << entry->valid
                << ", isVscatter=" << entry->isVscatter << ". Skipping.";
            continue;
        }

        // Step 7: 检查 entry 的 groupSlotId 是否有效并打印
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
            << "] DBID Response: stqIndex=" << stqIndex
            << ", entry.groupSlotId=" << entry->groupSlotId
            << ", entry.status=" << EnumToString(entry->status);

        // Step 8: 检查 entry 状态是否正确
        if (entry->status != VecTAReqStatus::SEND_REQ_TO_TMA) {
            LOG_DEBUG_M(Unit::VECTOR, Stage::NA)
                << "VSCATTER entry found but status mismatch: "
                << "expected SEND_REQ_TO_TMA, got " << EnumToString(entry->status)
                << ". Skipping.";
            continue;
        }

        // Step 8: 所有检查通过，弹出 DBID 响应包
        station->PopDataComReq(vtop->m_coreId);

        // Step 9: 保存 bpqID (从 DBID Response 的 addr 字段获取)
        // 根据数据宽度判断是否需要拆分双bpqID
        uint32_t dataWidth = entry->req.data.width;

        if (dataWidth == 64) {
            // 64位数据：从addr字段拆分出两个bpqID
            entry->bpqID = compGroupSlotId;
            entry->bpqID1 = static_cast<uint16_t>(compGroupSlotId & 0xFFFF);        // 低16位
            entry->bpqID2 = static_cast<uint16_t>((compGroupSlotId >> 16) & 0xFFFF); // 高16位
            entry->hasDualBPQ = true;

            LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
                << "] VSCATTER DBID Response (64-bit): stqIndex=" << stqIndex
                << ", packedBPQID=" << entry->bpqID
                << ", bpqID1=" << entry->bpqID1
                << ", bpqID2=" << entry->bpqID2
                << ", state -> RECEIVE_DBID_DONE";
        } else {
            // 非64位数据：仅使用单个bpqID
            entry->bpqID = compGroupSlotId;
            entry->bpqID1 = static_cast<uint16_t>(compGroupSlotId);
            entry->bpqID2 = 0;
            entry->hasDualBPQ = false;

            LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
                << "] VSCATTER DBID Response: stqIndex=" << stqIndex
                << ", bpqID=" << entry->bpqID
                << ", state -> RECEIVE_DBID_DONE";
        }

        // Step 10: 更新状态为 RECEIVE_DBID_DONE，等待发送地址 tiles
        entry->status = VecTAReqStatus::RECEIVE_DBID_DONE;

        break;  // 一次处理一个
    }
}

// VSCATTER: 处理 tile 发送状态转换（全局单tile约束 + 倒序检查状态）
void VectorCoreTA::ProcessVscatterTileSending()
{
    // ========== 全局单tile发送控制 ==========
    bool tileSentThisCycle = false;  // 本周期是否已发送tile（全局约束）

    // 遍历 storeQueue，检查 VSCATTER entry 的状态
    for (size_t i = 0; i < storeQueue.size(); i++) {
        VecTARequest& entry = storeQueue[i];

        if (!entry.valid || !entry.isVscatter) {
            continue;
        }

        // ========== 全局约束检查 ==========
        if (tileSentThisCycle) {
            // 本周期已发送tile，停止处理所有entry
            break;
        }

        // ========== 倒序检查状态（从高到低）==========

        // 状态4：正在发送数据tiles
        if (entry.status == VecTAReqStatus::SENDING_DATA_TILES) {
            // 检查SPB容量（只需1个tile）
            if (!vtop->HasValidSpbWriteEntry(BufferType::LOAD_DATA_RES, 1)) {
                LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
                    << "] ProcessVscatterTileSending: SPB full for data tile, wait next cycle, stqIndex=" << i;
                continue;  // SPB拥塞，跳过此entry，检查下一个entry
            }

            // 发送下一个数据tile
            SendVscatterDataTiles(entry.bpqID, i);
            tileSentThisCycle = true;  // 标记本周期已发送

            // 检查是否所有数据tiles发送完成
            if (entry.IsDataTilesSent()) {
                // 所有数据tiles发送完成
                entry.status = VecTAReqStatus::SEND_DATA_TILE_DONE;
                // DeallocateStqEntry entry.entryId // 释放entry
                LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
                    << "] VSCATTER completed suaccelssfully, stqIndex=" << i;
            }
            // 否则保持SENDING_DATA_TILES状态，下周期继续发送

            break;  // 本周期已发送tile，停止处理
        } else if (entry.status == VecTAReqStatus::SEND_ADDR_TILE_DONE) {
            // 状态3：地址tiles完成，准备发送数据tiles
            // 计算数据tile总数
            uint32_t dataWidth = static_cast<uint32_t>(entry.req.uinst->psrcs_[SRC0_IDX]->width);
            entry.totalDataTiles = (dataWidth == 64) ? 2 : 1;
            entry.dataTilesSent = 0;

            // 检查SPB容量（只需1个tile）
            if (!vtop->HasValidSpbWriteEntry(BufferType::LOAD_DATA_RES, 1)) {
                LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
                    << "] ProcessVscatterTileSending: SPB full for first data tile, wait next cycle, stqIndex=" << i;
                continue;
            }

            // 状态转换：SEND_ADDR_TILE_DONE -> SENDING_DATA_TILES
            entry.status = VecTAReqStatus::SENDING_DATA_TILES;

            // 发送第一个数据tile
            SendVscatterDataTiles(entry.bpqID, i);
            tileSentThisCycle = true;  // 标记本周期已发送

            // 检查是否所有数据tiles发送完成（32位情况）
            if (entry.IsDataTilesSent()) {
                entry.status = VecTAReqStatus::SEND_DATA_TILE_DONE;
                // DeallocateStqEntry entry.entryId
                LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
                    << "] VSCATTER completed suaccelssfully, stqIndex=" << i;
            }
            // 注意：此时status=SENDING_DATA_TILES或SEND_DATA_TILE_DONE
            // 但这些分支已经在前面检查过，不会再次执行

            break;  // 本周期已发送tile，停止处理
        } else if (entry.status == VecTAReqStatus::SENDING_ADDR_TILES) {
            // 状态2：正在发送地址tiles
            // 检查SPB容量（只需1个tile）
            if (!vtop->HasValidSpbWriteEntry(BufferType::LOAD_DATA_RES, 1)) {
                LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
                    << "] ProcessVscatterTileSending: SPB full for addr tile, wait next cycle, stqIndex=" << i;
                continue;
            }

            // 发送下一个地址tile
            SendVscatterAddrTiles(entry.bpqID1, i);
            tileSentThisCycle = true;  // 标记本周期已发送

            // 检查是否所有地址tiles发送完成
            if (entry.IsAddrTilesSent()) {
                // 所有地址tiles发送完成
                entry.status = VecTAReqStatus::SEND_ADDR_TILE_DONE;
                LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
                    << "] VSCATTER all addr tiles sent, state -> SEND_ADDR_TILE_DONE, stqIndex=" << i;
            }
            // 注意：即使转换到SEND_ADDR_TILE_DONE，该分支已检查过

            break;  // 本周期已发送tile，停止处理
        } else if (entry.status == VecTAReqStatus::RECEIVE_DBID_DONE) {
            // 状态1：收到DBID响应，准备发送地址tiles
            // 计算地址tile总数
            uint32_t addrWidth = static_cast<uint32_t>(entry.req.uinst->psrcs_[SRC1_IDX]->width);
            entry.totalAddrTiles = (addrWidth == 64) ? 2 : 1;
            entry.addrTilesSent = 0;

            // 检查SPB容量（只需1个tile）
            if (!vtop->HasValidSpbWriteEntry(BufferType::LOAD_DATA_RES, 1)) {
                LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
                    << "] ProcessVscatterTileSending: SPB full for first addr tile, wait next cycle, stqIndex=" << i;
                continue;
            }

            // 状态转换：RECEIVE_DBID_DONE -> SENDING_ADDR_TILES
            entry.status = VecTAReqStatus::SENDING_ADDR_TILES;

            // 发送第一个地址tile
            SendVscatterAddrTiles(entry.bpqID1, i);
            tileSentThisCycle = true;  // 标记本周期已发送

            // 检查是否所有地址tiles发送完成（32位情况）
            if (entry.IsAddrTilesSent()) {
                entry.status = VecTAReqStatus::SEND_ADDR_TILE_DONE;
                LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
                    << "] VSCATTER all addr tiles sent, state -> SEND_ADDR_TILE_DONE, stqIndex=" << i;
            }
            // 注意：此时status可能是SENDING_ADDR_TILES或SEND_ADDR_TILE_DONE
            // 但这些分支都已经在前面检查过，不会再次执行

            break;  // 本周期已发送tile，停止处理
        }
    }  // end for

    // 调试信息：打印本周期发送状态
    if (tileSentThisCycle) {
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
            << "] ProcessVscatterTileSending: VSCATTER tile sent this cycle";
    }
}

// VSCATTER: 发送地址 Tile(s)
void VectorCoreTA::SendVscatterAddrTiles(uint16_t bpqID1, uint32_t txnId)
{
    // Step 1: 边界检查
    if (txnId >= storeQueue.size()) {
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
            << "] SendVscatterAddrTiles: Invalid stqIndex=" << txnId
            << " (storeQueue size=" << storeQueue.size() << ")";
        return;
    }

    // Step 2: O(1) 直接访问 entry
    VecTARequest* entry = &storeQueue[txnId];

    // Step 3: 验证状态
    if (entry->status != VecTAReqStatus::SENDING_ADDR_TILES) {
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
            << "] SendVscatterAddrTiles: Entry status not SENDING_ADDR_TILES, stqIndex=" << txnId
            << ", status=" << EnumToString(entry->status);
        return;
    }

    LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
        << "] SendVscatterAddrTiles: Sending one addr tile, stqIndex=" << txnId
        << ", bpqID=" << bpqID1
        << ", progress=" << entry->addrTilesSent << "/" << entry->totalAddrTiles;

    // Step 4: 获取写 station (VSCATTER地址Tile使用LOAD_DATA_RES类型)
    std::shared_ptr<CrossStation> station = vtop->GetFreeWriteStation(BufferType::LOAD_DATA_RES);

    // 打印 station 获取结果
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
        << "] SendVscatterAddrTiles: GetFreeWriteStation result: "
        << (station ? "valid station" : "nullptr (false)");

    // 检查 station 是否有效
    if (!station) {
        LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
            << "] SendVscatterAddrTiles: Failed to get free write station, returning early";
        return;
    }

    // Step 5: 获取 TMA node ID 和 VEC node ID
    int tmaNodeId = vtop->pTileUtils->GetUBConfig()->tma_read_port[0];
    int vecNodeId = station->GetId();

    // Step 6: 提取 lane 数量（仅用于日志）
    /* size_t laneCount = entry->req.lanes
    if laneCount > 64
        laneCount = 64
    */

    // Step 7: 获取地址位宽和 mask
    uint32_t addrWidth = static_cast<uint32_t>(entry->req.uinst->psrcs_[SRC1_IDX]->width);
    uint64_t mask = entry->req.mask;  // 64-bit mask

    // Step 8: 确定当前tile索引
    int tileIdx = entry->addrTilesSent;  // 从已发送计数获取当前索引

    LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
        << "] VSCATTER Addr Tile - tileIdx=" << tileIdx
        << ", addrWidth=" << addrWidth << " bits, mask=0x" << std::hex << mask << std::dec;

    // Step 9: 分配 256B tile 缓冲区并初始化为 0
    alignas(8) uint8_t addrTileData[MAX_TILE_DATA_BYTE];
    memset(addrTileData, 0, MAX_TILE_DATA_BYTE);

    if (addrWidth == 64) {
        // 64-bit地址：打包32个地址到当前tile
        size_t laneStart = tileIdx * 32;
        size_t laneEnd = laneStart + 32;
        for (size_t lane = laneStart; lane < laneEnd; lane++) {
            if ((mask & (1ULL << lane)) != 0) {
                uint64_t addrValue = entry->req.addrs.Get(lane);
                size_t offset = (lane - laneStart) * sizeof(uint64_t);
                std::memcpy(addrTileData + offset, &addrValue, sizeof(uint64_t));
            }
        }

        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
            << "] VSCATTER Addr Tile[" << tileIdx << "] Content (64-bit):";
        for (int i = 0; i < 32; i++) {
            uint64_t value;
            std::memcpy(&value, &addrTileData[i * sizeof(uint64_t)], sizeof(uint64_t));
            LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "    pos[" << i << "]: 0x"
                << std::hex << value << std::dec;
        }

        std::shared_ptr<Request> addrTile = station->RxvscatterData(
            bpqID1, txnId, tileIdx, vecNodeId, tmaNodeId, vtop->m_coreId);
        addrTile->SetData(addrTileData);
        addrTile->SetSize(MAX_TILE_DATA_BYTE);  // 固定为 256B
        addrTile->SetPEType(MachineType::VECTOR);
        station->WriteSpb(addrTile);

        LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
            << "] VSCATTER Addr Tile[" << tileIdx << "] sent: bpqID=" << bpqID1
            << ", txnId=0x" << std::hex << txnId << std::dec
            << ", lanes[" << laneStart << "-" << (laneEnd - 1) << "]"
            << ", size=256B";
    } else {
        // 32-bit地址：打包64个地址到单个tile
        for (size_t lane = 0; lane < 64; lane++) {
            if ((mask & (1ULL << lane)) != 0) {
                uint32_t addrValue = static_cast<uint32_t>(entry->req.addrs.Get(lane));
                size_t offset = lane * sizeof(uint32_t);
                std::memcpy(addrTileData + offset, &addrValue, sizeof(uint32_t));
            }
        }

        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
            << "] VSCATTER Addr Tile (32-bit) Content:";
        for (int i = 0; i < 64; i++) {
            uint32_t value;
            std::memcpy(&value, &addrTileData[i * sizeof(uint32_t)], sizeof(uint32_t));
            LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "    pos[" << i << "]: 0x"
                << std::hex << value << std::dec;
        }

        std::shared_ptr<Request> addrTile = station->RxvscatterData(
            bpqID1, txnId, 0, vecNodeId, tmaNodeId, vtop->m_coreId);
        addrTile->SetData(addrTileData);
        addrTile->SetSize(MAX_TILE_DATA_BYTE);  // 固定为 256B
        addrTile->SetPEType(MachineType::VECTOR);
        station->WriteSpb(addrTile);

        LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
            << "] VSCATTER Addr Tile sent: bpqID=" << bpqID1
            << ", txnId=0x" << std::hex << txnId << std::dec
            << ", size=256B";
    }

    // Step 10: 更新计数（不更新状态，状态由ProcessVscatterTileSending管理）
    entry->addrTilesSent++;
    vtop->m_stats.tileTotalWrRequest++;

    LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
        << "] VSCATTER Addr tile sent, progress: " << entry->addrTilesSent << "/" << entry->totalAddrTiles
        << ", stqIndex=" << txnId;
}

// VSCATTER: 发送数据 Tile(s)
void VectorCoreTA::SendVscatterDataTiles(uint32_t bpqID, uint32_t txnId)
{
    // Step 1: 边界检查
    if (txnId >= storeQueue.size()) {
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
            << "] SendVscatterDataTiles: Invalid stqIndex=" << txnId
            << " (storeQueue size=" << storeQueue.size() << ")";
        return;
    }

    // Step 2: O(1) 直接访问 entry
    VecTARequest* entry = &storeQueue[txnId];

    // Step 3: 验证状态
    if (entry->status != VecTAReqStatus::SENDING_DATA_TILES) {
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
            << "] SendVscatterDataTiles: Entry status not SENDING_DATA_TILES, stqIndex=" << txnId
            << ", status=" << EnumToString(entry->status);
        return;
    }

    // Step 4: 检查是否已发送完成
    if (entry->dataTilesSent >= entry->totalDataTiles) {
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
            << "] SendVscatterDataTiles: All data tiles already sent, stqIndex=" << txnId
            << ", sent=" << entry->dataTilesSent << ", total=" << entry->totalDataTiles;
        return;
    }

    LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
        << "] SendVscatterDataTiles: Sending one data tile, stqIndex=" << txnId
        << ", bpqID=" << bpqID
        << ", progress=" << entry->dataTilesSent << "/" << entry->totalDataTiles;

    // Step 4: 获取写 station (VSCATTER数据Tile使用LOAD_DATA_RES类型)
    std::shared_ptr<CrossStation> station = vtop->GetFreeWriteStation(BufferType::LOAD_DATA_RES);

    // 打印 station 获取结果
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
        << "] SendVscatterDataTiles: GetFreeWriteStation result: "
        << (station ? "valid station" : "nullptr (false)");

    // 检查 station 是否有效
    if (!station) {
        LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
            << "] SendVscatterDataTiles: Failed to get free write station, returning early";
        return;
    }

    // Step 5: 获取 TMA node ID 和 VEC node ID
    int tmaNodeId = vtop->pTileUtils->GetUBConfig()->tma_read_port[0];
    int vecNodeId = station->GetId();

    // Step 6: 提取 lane 数量（仅用于日志）
    /* size_t laneCount = entry->req.lanes
    if laneCount > 64
        laneCount = 64
    */

    // Step 7: 获取数据位宽和 mask
    uint32_t dataWidth = static_cast<uint32_t>(entry->req.uinst->psrcs_[SRC0_IDX]->width);
    uint64_t mask = entry->req.mask;  // 64-bit mask

    // Step 8: 确定当前tile索引
    int tileIdx = entry->dataTilesSent;  // 从已发送计数获取当前索引

    LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
        << "] VSCATTER Data Tile - tileIdx=" << tileIdx
        << ", dataWidth=" << dataWidth << " bits, mask=0x" << std::hex << mask << std::dec;

    // Step 9: 分配 256B tile 缓冲区并初始化为 0
    alignas(8) uint8_t dataTileData[MAX_TILE_DATA_BYTE];
    memset(dataTileData, 0, MAX_TILE_DATA_BYTE);

    if (dataWidth == 64) {
        // 64-bit数据：打包32个数据到当前tile
        size_t laneStart = tileIdx * 32;
        size_t laneEnd = laneStart + 32;
        for (size_t lane = laneStart; lane < laneEnd; lane++) {
            if ((mask & (1ULL << lane)) != 0) {
                uint64_t dataValue = entry->req.data.Get(lane);
                size_t offset = (lane - laneStart) * sizeof(uint64_t);
                std::memcpy(dataTileData + offset, &dataValue, sizeof(uint64_t));
            }
        }

        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
            << "] VSCATTER Data Tile[" << tileIdx << "] Content (64-bit):";
        for (int i = 0; i < 32; i++) {
            uint64_t value;
            std::memcpy(&value, &dataTileData[i * sizeof(uint64_t)], sizeof(uint64_t));
            LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "    pos[" << i << "]: 0x"
                << std::hex << value << std::dec;
        }

        // 根据tileIdx选择正确的bpqID
        uint16_t tileBPQID = (tileIdx == 0) ? entry->bpqID1 : entry->bpqID2;

        std::shared_ptr<Request> dataTile = station->RxvscatterData(
            tileBPQID, txnId, 2 + tileIdx, vecNodeId, tmaNodeId, vtop->m_coreId);
        dataTile->SetData(dataTileData);
        dataTile->SetSize(MAX_TILE_DATA_BYTE);  // 固定 256B
        dataTile->SetPEType(MachineType::VECTOR);
        station->WriteSpb(dataTile);

        LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
            << "] VSCATTER Data Tile[" << tileIdx << "] sent: bpqID=" << tileBPQID
            << ", txnId=0x" << std::hex << txnId << std::dec
            << ", lanes[" << laneStart << "-" << (laneEnd - 1) << "]"
            << ", size=256B";
    } else {
        // 32-bit数据：打包64个数据到单个tile
        for (size_t lane = 0; lane < 64; lane++) {
            if ((mask & (1ULL << lane)) != 0) {
                uint32_t dataValue = static_cast<uint32_t>(entry->req.data.Get(lane));
                size_t offset = lane * sizeof(uint32_t);
                std::memcpy(dataTileData + offset, &dataValue, sizeof(uint32_t));
            }
        }

        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
            << "] VSCATTER Data Tile (32-bit) Content:";
        for (int i = 0; i < 64; i++) {
            uint32_t value;
            std::memcpy(&value, &dataTileData[i * sizeof(uint32_t)], sizeof(uint32_t));
            LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "    pos[" << i << "]: 0x"
                << std::hex << value << std::dec;
        }

        std::shared_ptr<Request> dataTile = station->RxvscatterData(
            entry->bpqID1, txnId, 2, vecNodeId, tmaNodeId, vtop->m_coreId);
        dataTile->SetData(dataTileData);
        dataTile->SetSize(MAX_TILE_DATA_BYTE);  // 固定 256B
        dataTile->SetPEType(MachineType::VECTOR);
        station->WriteSpb(dataTile);

        LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
            << "] VSCATTER Data Tile sent: bpqID=" << entry->bpqID1
            << ", txnId=0x" << std::hex << txnId << std::dec
            << ", size=256B";
    }

    // Step 10: 更新计数（不更新状态，状态由ProcessVscatterTileSending管理）
    entry->dataTilesSent++;
    vtop->m_stats.tileTotalWrRequest++;

    LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId
        << "] VSCATTER Data tile sent, progress: " << entry->dataTilesSent << "/" << entry->totalDataTiles
        << ", stqIndex=" << txnId;
}

void VectorCoreTA::SendTRReq()
{
}

void VectorCoreTA::ReceiveTRData()
{
    // 收到tileRegister load data
    // (2) 通过reqId/Bid/Gid/rid wakeup load queue, 状态机转 E2
    if (vtop->pTileUtils->IsRingOrXbarMode(false)) {
        LoadFromRing();
        return;
    }
    int i = 0;
    while (!tileRegLdResQ->Empty() && i < 2) {
        auto res = tileRegLdResQ->Read();
        if (res.IsPrefetch()) {
            WakeupPrefetchQueue(res);
            res.SetWay(UINT32_MAX); // wayid为UINT32_MAX - 1代表LOAD stash
            ReceiveStashReq(res.GetAddr(), res);
        } else {
            WakeupLoadQueue(res);
        }
        ++i;
    }
}

void VectorCoreTA::ReceiveTRRsp()
{
    if (vtop->pTileUtils->IsRingOrXbarMode(false)) {
        StoreFromRing();
        return;
    }
    if (!vtop->GetConfig().enable_scb) {
        int i = 0;
        while (!tileRegWrRspQ->Empty() && i < stBand) {
            auto res = tileRegWrRspQ->Read();
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[Vector TA] Recv response from tile reg, reqId=" << res.GetRespId();
            WakeupStoreQueue(res.GetRespId());
            ++i;
        }
    } else {
        int i = 0;
        while (!tileRegWrRspQ->Empty() && i < stBand) {
            auto res = tileRegWrRspQ->Read();
            scb->HandleTileRegRsp(res.GetRespId());
            WakeupStoreQueue(res.GetRespId());
            ++i;
        }
    }
}

uint32_t VectorCoreTA::CreateTileRegLdReq(uint64_t addr, uint32_t eId, uint32_t stid, uint64_t size)
{
    auto req = VecTileRegLdReq();
    req.SetStid(stid);
    uint64_t originAddr = pTileUtils->Addr2Bytes(addr);
    LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "Tile Register ld req, reqId=" << tileReqId << " , addr=0x" << std::hex <<
        addr << dec;
    req.SetReqId(tileReqId);
    req.SetSize(size);
    req.SetSrc(addr);
    if (vtop->pTileUtils->IsRingOrXbarMode(false)) {
        bool succ = LoadToRing(req);
        if (!succ) {
            loadQueue[eId].valid = true;
            loadQueue[eId].stage = VecTAStage::STAGE_E1;
            loadQueue[eId].status = VecTAReqStatus::PENDING;
            return UINT32_MAX;
        } else {
            loadMissQueue->addMissRequest(originAddr, stid, tileReqId, eId);
        }
    } else {
        if (loadMissQueue->addMissRequest(originAddr, stid, tileReqId, eId)) {
            this->tileRegLdReqQ->Write(req);
        } else {
            return UINT32_MAX;
        }
    }

    return tileReqId++;
}

uint32_t VectorCoreTA::CreateTileWriteReq(unsigned entryId, uint64_t addr, const std::vector<uint8_t>& data,
                                          uint64_t size, DataMask mask, uint32_t stid)
{
    VecTileRegStReq req = VecTileRegStReq();
    uint32_t reqId = tileReqId++;
    // 调整地址, 按照 Tile 的地址粒度来调整
    req.SetReqId(reqId);
    req.SetDest(addr);
    req.SetData(data.data(), size);
    req.SetSize(size);
    req.SetDataMask(mask);
    req.SetStid(stid);
    if (vtop->pTileUtils->IsRingOrXbarMode(false)) {
        bool succ = StoreToRing(req, mask);
        if (!succ) {
            return UINT32_MAX;
        }
        LOG_INFO_M(Unit::VECTOR, Stage::E2) << " [VECTOR " << vtop->m_coreId << "]Creating Ring st req, reqId=" << dec
            << reqId << " , addr=0x" << std::hex << addr << ", size:" << size;
    } else {
        this->tileRegStReqQ->Write(req);
        LOG_INFO_M(Unit::VECTOR, Stage::E2) << " [VECTOR " << vtop->m_coreId << "]Creating Queue st req, reqId=" << dec
            << reqId << " , addr=0x" << std::hex << addr << ", size:" << size;
    }
    if (!vtop->GetConfig().enable_scb) {
        storeQueue[entryId].writeCyc = GetSim()->cycles;
    }

    return reqId;
}

bool VectorCoreTA::LoadToRing(VecTileRegLdReq req) const
{
    if (!vtop->HasValidSpbReadEntry()) {
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "[Vector TA] No Valid spb";
        return false;
    }
    std::shared_ptr<CrossStation> station = nullptr;
    if (vtop->pTileUtils->configs.strict_closest_injection) {
        if (!vtop->HasValidSpbReadEntry(req.GetSrc(), true)) {
            LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "[Vector TA] No Valid spb";
            return false;
        }
        station = vtop->GetFreeReadStation();
    } else if (vtop->pTileUtils->configs.closest_injection) {
        station = vtop->GetFreeReadStation(req.GetSrc(), false);
    } else {
        station = vtop->GetFreeReadStation();
    }
    std::shared_ptr<Request> pkt = station->Rxreq(req.GetSrc(), req.GetStid(), vtop->m_coreId);
    pkt->SetPEType(MachineType::VECTOR);
    pkt->SetSize(req.GetSize());
    pkt->SetBufId(req.GetReqId());
    station->WriteSpb(pkt);
    if (station->GetId() == vtop->ldRingport0Id) {
        vtop->m_stats.tile_port0_load_request ++;
    } else {
        vtop->m_stats.tile_port1_load_request ++;
    }
    vtop->m_stats.tileTotalRdRequest++;
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << vtop->m_coreId << "] Load Miss req to Ring " << hex
        << *pkt << dec;
    return true;
}

bool VectorCoreTA::StoreToRing(VecTileRegStReq req, DataMask mask)
{
    if (!vtop->HasValidSpbWriteEntry()) {
        return false;
    }

    std::shared_ptr<CrossStation> station = vtop->GetFreeWriteStation();
    std::shared_ptr<Request> pkt = station->Rxdat(req.GetDest(), req.GetStid(), vtop->m_coreId);
    pkt->SetSize(req.GetSize());
    pkt->SetBufId(req.GetReqId());
    pkt->SetPEType(MachineType::VECTOR);
    uint8_t data[MAX_TILE_DATA_BYTE];
    uint8_t *stData = req.GetData();
    for (size_t i = 0; i < MAX_TILE_DATA_BYTE; ++i) {
        data[i] = stData[i];
    }
    pkt->SetData(data);
    pkt->dmask = mask;
    station->WriteSpb(pkt);
    vtop->m_stats.tileTotalWrRequest++;
    LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "Store req will be sent to ring " << *pkt;
    return true;
}

void VectorCoreTA::LoadFromRing()
{
    for (uint32_t i = 0; i < vtop->stationReadRange; i++) {
        auto &station = vtop->stations[i];
        if (station->HasNoResp(vtop->m_coreId)) {
            continue;
        }
        // Use peek to check request type first, don't pop DBID Response
        std::shared_ptr<Request> pkt = station->GetDataComReqFront(vtop->m_coreId);
        if (!pkt) {
            continue;
        }
        // Skip DBID Response (CompDBIDResp) - it should be handled by ProcessDbidResponse()
        if (pkt->GetChannel() == ChannelType::RSP && pkt->GetFlit().cmd == CHICommand::CompDBIDResp) {
            continue;
        }
        // Now pop the request for processing
        pkt = station->GetDataComReq(vtop->m_coreId);
        if (pkt->cmd) {
            ASSERT(pkt->stid == pkt->cmd->stid);
        }

        uint8_t *data = pkt->GetData();
        uint32_t reqId = pkt->GetBufId();
        TileRegVecLdRes res;
        res.SetStid(pkt->stid);
        res.SetRespId(reqId);
        res.SetData(data);
        res.SetPrefetch(pkt->IsPrefetchResp());
        if (vtop->GetConfig().stash_mode) {
            res.SetWay(pkt->GetWayId());
            ReceiveStashReq(pkt->GetAddr(), res);
            stashRespQueue.emplace_back(pkt);
        } else if (pkt->IsPrefetchResp()) {
            WakeupPrefetchQueue(res);
            if (pkt->IsPrefetchResp()) {
                res.SetWay(UINT32_MAX); // wayid为UINT32_MAX - 1代表LOAD stash
                vtop->m_stats.stashNum++;
            }
            ReceiveStashReq(pkt->GetAddr(), res);
        } else {
            WakeupLoadQueue(res);
        }
    }
}

bool VectorCoreTA::IsInPfQ(uint64_t addr, uint32_t stid)
{
    for (uint32_t i = 0; i < prefetchReqQ[stid].size(); i++) {
        if (prefetchReqQ[stid][i] == addr) {
            return true;
        }
    }
    return false;
}

bool VectorCoreTA::IsInHPfQ(uint64_t addr, uint32_t stid)
{
    for (uint32_t i = 0; i < hprefetchReqQ[stid].size(); i++) {
        if (hprefetchReqQ[stid][i] == addr) {
            return true;
        }
    }
    return false;
}

void VectorCoreTA::EraseAddrPfQ(uint64_t addr, uint32_t stid)
{
    for (uint32_t i = 0; i < prefetchReqQ[stid].size(); i++) {
        if (prefetchReqQ[stid][i] == addr) {
            prefetchReqQ[stid].erase(prefetchReqQ[stid].begin()+i);
            break;
        }
    }
}

void VectorCoreTA::EraseAddrHPfQ(uint64_t addr, uint32_t stid)
{
    for (uint32_t i = 0; i < hprefetchReqQ[stid].size(); i++) {
        if (hprefetchReqQ[stid][i] == addr) {
            hprefetchReqQ[stid].erase(hprefetchReqQ[stid].begin()+i);
            return;
        }
    }
}


void VectorCoreTA::StoreFromRing()
{
    for (uint32_t i = vtop->stationReadRange; i < vtop->stations.size(); i++) {
        auto &station = vtop->stations[i];
        if (station->HasNoResp(vtop->m_coreId)) {
            continue;
        }
        // Use peek to check request type first, don't pop DBID Response
        std::shared_ptr<Request> pkt = station->GetDataComReqFront(vtop->m_coreId);
        if (!pkt) {
            continue;
        }
        // Skip DBID Response (CompDBIDResp) - it should be handled by ProcessDbidResponse()
        if (pkt->GetChannel() == ChannelType::RSP && pkt->GetFlit().cmd == CHICommand::CompDBIDResp) {
            continue;
        }
        // Now pop the request for processing
        pkt = station->GetDataComReq(vtop->m_coreId);

        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "Recv write response from ring. " << *pkt;
        if (!vtop->GetConfig().enable_scb) {
            WakeupStoreQueue(pkt->GetBufId());
        } else {
            scb->HandleTileRegRsp(pkt->GetBufId());
        }
    }
}

SimSys* VectorCoreTA::GetSim()
{
    if (vtop == nullptr) {
        return nullptr;
    }
    return vtop->GetSim();
}

void VectorCoreTA::Resolve()
{
    if (!(*this->resolveSignal)) {
        return;
    }
    LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "[Vector TA] Recv resolve signal";
}

static bool CheckFlush(FlushBus &bus, uint32_t stid, ROBID &bid, ROBID &gid, ROBID &subId)
{
    if (bus.req.stid != stid) {
        return false;
    }
    if (bus.baseOnBid) {
        return LessEqual(bus.req.bid, bid);
    }

    return LessEqual(bus.req.bid, bus.req.gid, bus.req.rid, bid, gid, subId);
}

void VectorCoreTA::Flush(FlushBus &bus)
{
    // TODO: Use lsID instead of rid.
    // Load/store input
    auto matchReq = [&bus](MemRequest &req) {
        if (req.stid != bus.req.stid) {
            return false;
        }
        if (bus.baseOnThread && req.tid != bus.req.tid) {
            return false;
        }
        if (bus.baseOnBid) {
            return LessEqual(bus.req.bid, req.bid);
        }
        return LessEqual(bus.req.bid, bus.req.gid, bus.req.rid, req.bid, req.gid, req.rid);
    };

    LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "[VECTOR TA] Recv flush bus " << bus;
    if (LessEqual(bus.req.bid, drainBlockId[bus.req.stid])) {
        DisableROBID(drainBlockId[bus.req.stid]);
    }
    loadQ.FlushIf(matchReq);
    storeQ.FlushIf(matchReq);
    ldOutputQ->FlushIf(matchReq);
    stOutputQ->FlushIf(matchReq);
    scb->Flush(bus);
    // TODO: Need to flush the tile queue, but there are errors. Error case is here.
    for (auto it = loadQueue.begin(); it != loadQueue.end(); it++) {
        if (!(*it).valid) {
            continue;
        }
        if (CheckFlush(bus, (*it).req.stid, (*it).req.bid, (*it).req.gid, (*it).req.rid)) {
            (*it).Reset();
            ldqEntryOccCnt--;
            LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << dec << "[VECTOR TA] flush, load bid=" << (*it).req.bid
                << ", gid=" << (*it).req.gid << ", tid=" << (*it).req.tid
                << ", rid=" << (*it).req.rid;
            (*it).Reset();
        }
    }
    for (auto it = storeQueue.begin(); it != storeQueue.end(); it++) {
        if (!(*it).valid) {
            continue;
        }
        if (CheckFlush(bus, (*it).req.stid, (*it).req.bid, (*it).req.gid, (*it).req.rid)) {
            (*it).Reset();
            stqEntryOccCnt[(*it).entryId/vtop->GetConfig().ta_stq_size]--;
            LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "[VECTOR TA] flush, store bid=" << dec << (*it).req.bid
                << ", gid=" << (*it).req.gid << ", tid=" << (*it).req.tid
                << ", rid=" << (*it).req.rid;
            (*it).Reset();
        }
    }

    auto vecMatchReq = [&bus](VecTARequest &vecReq) {
        auto& req = vecReq.req;
        if (req.stid != bus.req.stid) {
            return false;
        }
        if (bus.baseOnThread && req.tid != bus.req.tid) {
            return false;
        }
        if (bus.baseOnBid) {
            return LessEqual(bus.req.bid, req.bid);
        }
        return LessEqual(bus.req.bid, bus.req.gid, bus.req.rid, req.bid, req.gid, req.rid);
    };

    for (auto& it : retireScbMap_) {
        it.second.FlushIf(vecMatchReq);
    }
}

std::ostream& operator<<(std::ostream& out, const struct TAEntry& entry)
{
    out << "tag: 0x" << entry.tag << std::endl;
    ASSERT(entry.state <= TAEntryState::MODIFIED);
    out << "state: " << taEntryStateInfo[entry.state];
    return out;
}

std::string LaneDataTransfer::PrintCoverageInfo() const
{
    if (!areValid()) {
        return "";
    }
    std::stringstream oss;
    oss << "Load: addr=0x" << std::hex << load_.addr << ", bytes=" << std::dec << load_.bytes << std::endl;
    oss << "Store: addr=0x" << std::hex << store_.addr << ", bytes=" << std::dec << store_.bytes << std::endl;
    if (isStoreFullyCoverLoad()) {
        oss << "Store fully covers load" << std::endl;
    } else if (hasOverlap()) {
        auto info = getOverlapInfo();
        oss << "Partial overlap: " << info.overlap_size << " bytes" << std::endl;
        oss << "Overlap region: 0x" << std::hex << info.overlap_start
                    << " - 0x" << info.overlap_end << std::dec << std::endl;
    } else {
        oss << "No overlap" << std::endl;
    }
    return oss.str();
}

} // namespace JCore
