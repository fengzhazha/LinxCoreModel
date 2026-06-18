#include "mtccore/memcore/MemoryCoreTA.h"

#include <unordered_map>
#include <algorithm>

#include "core/Core.h"
#include "mtccore/memcore/MemoryCore.h"
#include "mtccore/memcore/MemoryCoreTAStats.h"

namespace JCore {

const static std::unordered_map<MtcTAWriteType, const char*> taWriteInfo = {
    {MtcTAWriteType::EVICT, "EVICT"},
    {MtcTAWriteType::MTC_LDQ_REPICK, "LDQ REPICK"},
    {MtcTAWriteType::LOAD, "LOAD"},
    {MtcTAWriteType::READ_FILL, "READ FILL"},
    {MtcTAWriteType::STORE, "STORE"},
};

static std::unordered_map<MtcTAEntryState, const char*> MtcTAEntryStateInfo = {
    {MtcTAEntryState::INVALID, "INVALID"},
    {MtcTAEntryState::SHARED, "SHARED"},
    {MtcTAEntryState::MODIFIED, "MODIFIED"}
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

MtcMissRequestQueue::MtcMissRequestEntry::MtcMissRequestEntry (uint64_t addr, uint32_t eid, uint64_t reqId)
    : address(addr), entryId(eid), requestId(reqId) {}

MtcMissRequestQueue::MtcMissRequestQueue (size_t depth, bool verbose)
    : maxDepth_(depth), nextEntryId_(0), verbose_(verbose) {}

bool MtcMissRequestQueue::addMissRequest (uint64_t address, uint64_t requestId, uint32_t entryId)
{
    if (addressMap_.size() >= maxDepth_) {
        return false;
    }
    auto it = addressMap_.find(address);
    if (it != addressMap_.end()) {
        ASSERT(0 && "should be merged");
    }

    nextEntryId_++;
    auto entry = std::make_shared<MtcMissRequestEntry>(address, entryId, requestId);
    addressMap_[address] = entry;
    if (verbose_) {
        std::cout << "[Tile load Miss Queue] record ld miss req, entryId=" << entryId <<", reqId=" << requestId
            << ", addr=0x" << std::hex << address << dec << std::endl;
    }
    return true;
}

void MtcMissRequestQueue::shift(uint32_t _entryId)
{
    for (auto &t : addressMap_) {
        if (t.second->entryId == (_entryId-1)) {
            t.second->entryId = _entryId;
        }
        for (size_t i = 0; i < t.second->mergeEntryId.size(); ++i) {
            if (t.second->mergeEntryId[i] == (_entryId-1)) {
                t.second->mergeEntryId[i] = _entryId;
                break;
            }
        }
    }
}

void MtcMissRequestQueue::addMergeEntryId(uint64_t address, uint32_t entryId)
{
    auto addrIt = addressMap_.find(address);
    if (addrIt == addressMap_.end()) {
        ASSERT(0);
    }
    addressMap_[address]->mergeEntryId.push_back(entryId);
}

void MtcMissRequestQueue::removeOnFill(uint64_t requestId)
{
    for (auto it = addressMap_.begin() ; it != addressMap_.end();) {
        if (it->second->requestId == requestId) {
            if (verbose_) {
                std::cout << "delete ld miss entry, entryId=" << dec << it->second->entryId <<", reqId=" <<
                    it->second->requestId << std::endl;
            }
            it = addressMap_.erase(it);
            break;
        } else {
            ++it;
        }
    }
}

bool MtcMissRequestQueue::findReqByAddress(uint64_t address, uint32_t camEid, uint32_t &eid, uint64_t &reqId)
{
    auto it = addressMap_.find(address);
    if (it != addressMap_.end()) {
        eid = it->second->entryId;
        reqId = it->second->requestId;
        addMergeEntryId(address, camEid);
        if (verbose_) {
            std::cout << "[TA Load Pipe] Miss hit other req, entryId="<< eid << ", addr=0x" << hex
                << address << dec << ", hit reqId=" << reqId << std::endl;
        }
        return true;
    }

    return false;
}
void MtcMissRequestQueue::GetMissReqEntryId(uint64_t reqId, uint32_t &eid, uint32_t &addr)
{
    for (const auto& pair : addressMap_) {
        if (pair.second->requestId == reqId) {
            eid = pair.second->entryId;
            addr = pair.second->address;
            if (verbose_) {
                cout << "fill data get info, reqId=" << reqId << ", eid=" << eid << ", addr=0x"
                    << hex << addr << dec << endl;
            }
            return;
        }
    }
}

void MtcMissRequestQueue::GetMergeReqEntryId(uint64_t reqId, std::vector<uint32_t> &eids)
{
    for (const auto& pair : addressMap_) {
        if (pair.second->requestId == reqId) {
            eids = pair.second->mergeEntryId;
            return;
        }
    }
}

size_t MtcMissRequestQueue::getCurrentSize() const
{
    return addressMap_.size();
}

size_t MtcMissRequestQueue::getMaxDepth() const
{
    return maxDepth_;
}

bool MtcMissRequestQueue::isEmpty() const
{
    return addressMap_.empty();
}

bool MtcMissRequestQueue::isFull() const
{
    return addressMap_.size() >= maxDepth_;
}

void MtcMissRequestQueue::clear()
{
    addressMap_.clear();
}

void MtcMissRequestQueue::printQueueStatus() const
{
    std::cout << "Miss Request Queue Status:" << std::endl;
    std::cout << "  Current Size: " << addressMap_.size()
              << "/" << maxDepth_ << std::endl;
    for (const auto& entry : addressMap_) {
        std::cout << "  entryId=" << entry.second->entryId
                  << ", Addr: 0x" << std::hex << entry.first
                  << std::dec << ", reqId=" << entry.second->requestId << std::endl;
        for (const auto& merge : entry.second->mergeEntryId) {
            std::cout << ", Merge entryId=" << merge << std::endl;
        }
    }
}

MemoryCoreTA::MemoryCoreTA()
{
}

MemoryCoreTA::~MemoryCoreTA()
{
}

void MemoryCoreTA::Build(MemoryCore *top, uint64_t set, uint64_t way, uint64_t nEntry, uint64_t entrySize,
    uint64_t loadQSize, uint64_t storeQSize, bool use_tile_cache)
{
    ASSERT(way > 0);
    ASSERT(nEntry > 0);
    this->vtop = top;
    this->nSets = set;
    this->nWays = way;
    this->nEntries = nEntry;
    this->cacheOffsetBits = 8; // 2^8 = 256B
    uint32_t temp = set;
    while (temp > 1) {
        this->cacheIndexBits++;
        temp >>= 1;
    }
    this->cacheTagBits = 32 - cacheIndexBits - cacheOffsetBits; // 假设 32bit 地址
    this->addrBitWidth = static_cast<uint64_t>(std::log2(nEntry));
    this->entrySizes = entrySize;
    this->tagMask = static_cast<uint64_t>(~(nEntry - 1));
    this->idxMask = static_cast<uint64_t>(nEntry - 1);
    this->addrMask = static_cast<uint64_t>(~(255ULL)); // 256B 地址对齐
    this->pTileUtils = top->pTileUtils;
    this->ldBand = top->GetConfig().ta_tile_ld_band;
    this->stBand = top->GetConfig().ta_tile_st_band;
    this->use_tile_cache = use_tile_cache;

    // 初始化tBuffer
    tBuffer.resize(nSets);
    for (uint64_t i = 0; i < nSets; i++) {
        tBuffer[i].resize(nWays, MtcTAEntry(entrySizes));
    }
    // tBuffer = std::vector<std::vector<TAEntry>>(way, std::vector<TAEntry>(nEntry, TAEntry(entrySize)));
    if (!IsPow2(nEntry)) {
        LOG_WARN_M(Unit::MTC, Stage::NA) << " Vector tile tBuffer entry is not power of 2, may cause index cal error";
    }

    loadQueue.resize(loadQSize * GetSim()->core->configs.threadCount);
    storeQueue.resize(storeQSize * GetSim()->core->configs.threadCount);
    for (uint64_t i = 0; i < loadQSize*GetSim()->core->configs.threadCount; i++) {
        loadQueue[i].Init(i);
    }
    for (uint64_t i = 0; i < storeQSize * GetSim()->core->configs.threadCount; i++) {
        storeQueue[i].Init(i);
    }

    ldqEntryOccCnt.resize(GetSim()->core->configs.threadCount);
    stqEntryOccCnt.resize(GetSim()->core->configs.threadCount);

    // Write Back Q by order
    for (uint64_t i = 0; i < static_cast<uint64_t>(MtcTAWriteType::TA_WRITE_END); ++i) {
        writeBackQ.emplace_back(SimQueue<VecTileRegStReq>());
    }

    loadMissQueue = std::make_shared<MtcMissRequestQueue>(loadQSize*64, VERBOSE_ON);

    scb = std::make_shared<TileSCB>();
    scb->SetSim(top->GetSim());
    scb->Build(this, top->GetConfig().ta_scb_entry_size);
}

void MemoryCoreTA::Xfer()
{
    // 处理load queue/store queue状态机, E2 -> E3
    for (auto it = loadQueue.begin(); it != loadQueue.end(); it++) {
        switch ((*it).stage) {
            case MtcTAStage::STAGE_E1:
                (*it).stage = MtcTAStage::STAGE_E2;
                break;
            case MtcTAStage::STAGE_E2:
                (*it).stage = MtcTAStage::STAGE_E3;
                break;
            case MtcTAStage::STAGE_E3:
                (*it).stage = MtcTAStage::INVALID;
                break;
            default:
                break;
        }
    }
    for (auto it = storeQueue.begin(); it != storeQueue.end(); it++) {
        switch ((*it).stage) {
            case MtcTAStage::STAGE_E1:
                (*it).stage = MtcTAStage::STAGE_E2;
                break;
            case MtcTAStage::STAGE_E2:
                (*it).stage = MtcTAStage::STAGE_E3;
                break;
            case MtcTAStage::STAGE_E3:
                (*it).stage = MtcTAStage::INVALID;
                break;
            default:
                break;
        }
    }

    LdQShift();
    StQShift();

    scb->Xfer();
    outputQ->Work();
}

void MemoryCoreTA::Reset()
{
}

void MemoryCoreTA::Work()
{
    scb->Work();
    DrainEmpty();
    ReceiveNonFlush();
    // 处理TileRegister 数据回填
    ReceiveTlsData();
    ReceiveTRData();
    ReceiveTRRsp();
    ldInPipeCnt = ldBand;
    E1();
    E2();
    E3();
    RetireStore();

    if (!drainScbDataQ->Empty() && vtop->GetConfig().enable_scb && !scb->isInDrainMode) {
        ScbDrainBus drainInfo = drainScbDataQ->Read();
        if (VERBOSE_ON) {
            std::cout << "[VectorCore TA Stage]: Receive resolve bid " << drainInfo.bid << std::endl;
        }
        drainBlockId = drainInfo.bid;
        scb->DrainEntry(drainInfo.bid);
    }
}

void MemoryCoreTA::Dump()
{
    DumpLoadQueue();
    DumpStoreQueue();
    scb->DumpScb();
}
bool MemoryCoreTA::IsBusy()
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
// -------------------------------------------TBuffer 功能------------------------------------------------------
// tbuffer 功能: (1) load miss -> tileregister -> fill -> tubbufer
//              (2) load hit -> fetch data
// 地址分解
void MemoryCoreTA::decodeAddress(uint32_t address, uint32_t& tag, uint32_t& index, uint32_t& offset)
{
    offset = address & ((1 << cacheOffsetBits) - 1); // cacheline offset
    index = (address >> cacheOffsetBits) & ((1 << cacheIndexBits) - 1);
    tag = (address >> (cacheOffsetBits + cacheIndexBits));  // 1M 地址， 高8位：tag
}

bool MemoryCoreTA::IsTBufferHit(const MtcLaneAddr& lane, std::vector<uint8_t>& result)
{
    if (lane.bytes == 0|| lane.bytes > 4) {
        if (VERBOSE_ON) {
            std::cout << "Mtc Invalid lane size: " << lane.bytes << std::endl;
        }
        return false;
    }

    uint32_t tag;
    uint32_t index;
    uint32_t offset;
    decodeAddress(lane.addr, tag, index, offset);

    if (index >= nSets) {
        if (VERBOSE_ON) {
            std::cout << "Index out of range: " << index << " >= " << nSets << std::endl;
        }
        return false;
    }
    if (offset + lane.bytes > entrySizes) {
        if (VERBOSE_ON) {
            std::cout << "Address cross cacheline boundary: offset=" << offset
                      << ", bytes=" << lane.bytes << std::endl;
        }
        // TODO：先识别， 需要细粒度实现
        ASSERT(0);
        return false;
    }

    auto& set = tBuffer[index];

    for (uint32_t i = 0; i < nWays; ++i) {
        auto& cache_line = set[i];
        if (cache_line.valid && cache_line.tag == tag) {
            UpdateLRU(index, i);
            if (VERBOSE_ON) {
                std::cout << "TBuffer HIT: Address=0x" << std::hex << lane.addr
                          << " (Set:" << std::dec << index << ", Way:" << i << ")" << std::endl;
            }
            result.resize(lane.bytes);
            std::copy_n(cache_line.data.data() + offset, lane.bytes, result.data());

            ++cache_line.hitCnt;
            return true;
        }
    }

    return false;
}

uint64_t MemoryCoreTA::GetHitWay(uint32_t address)
{
    uint32_t tag;
    uint32_t index;
    uint32_t offset;
    decodeAddress(address, tag, index, offset);
    if (index >= nSets || index < 0) {
        LOG_ERROR("index out of range");
        ASSERT(0);
        return -1;
    }
    uint64_t way = findLRUWay(index);
    if (tBuffer[index][way].valid) {
        ResetEntry(index, way);
    }
    return way;
}

void MemoryCoreTA::GetHitInfo(uint32_t address, uint64_t &set, uint64_t &way)
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
void MemoryCoreTA::UpdateLRU(uint64_t set, uint64_t way)
{
    if (set < nSets && way < nWays) {
        tBuffer[set][way].aaccelss_time = ++global_time;
    } else {
        ASSERT(0 && " set/way value is over MAX!");
    }
}

// 找到需要替换的way（LRU算法）
uint64_t MemoryCoreTA::findLRUWay(uint64_t set)
{
    ASSERT(set < nSets);
    uint64_t lru_way = 0;
    uint32_t min_time = tBuffer[set][0].aaccelss_time;

    for (uint64_t i = 1; i < nWays; i++) {
        if (tBuffer[set][i].aaccelss_time < min_time) {
            min_time = tBuffer[set][i].aaccelss_time;
            lru_way = i;
        }
    }
    return lru_way;
}

void MemoryCoreTA::printData(const uint8_t* data, size_t length)
{
    std::cout << std::hex << std::setfill('0');
    for (size_t i = 0; i < length; ++i) {
        std::cout << std::setw(2) << static_cast<int>(data[i]) << " ";
    }
    std::cout << std::dec << std::endl;
}

void MemoryCoreTA::FillData(uint32_t address, TileRegVecLdRes req)
{
    uint32_t tag;
    uint32_t index;
    uint32_t offset;
    decodeAddress(address, tag, index, offset);
    uint64_t way = GetHitWay(address);
    if (!(way >= 0 && way < nWays)) {
        cout << "Wrong Way, Write Addr=0x" << hex << address << " Set=" << dec << index << ", Way=" << way << endl;
        ASSERT(0);
    }

    auto freeMemSize = tBuffer[index][way].data.capacity() - offset;
    ASSERT(offset <= static_cast<decltype(offset)>(freeMemSize) && "Insufficient free memory!");
    std::copy(req.GetData(), req.GetData() + offset, tBuffer[index][way].data.begin() + offset);
    if (VERBOSE_ON) {
        cout << "Write Addr=0x" << hex << address << " (Set=" << dec << index << ", Way=" << way << ")" << endl;
    }
    if (VERBOSE_ON) {
        printData(req.GetData(), offset);
    }
}

void MemoryCoreTA::ResetEntry(uint64_t set, uint64_t way)
{
    if (tBuffer[set][way].valid) {
        tBuffer[set][way].valid = false;
        tBuffer[set][way].data.clear();
    }
}


void MemoryCoreTA::DrainAllEntry()
{
    // 目前 tile cache只缓存load data， 不需要drain空
    return;
    auto processModifiedEntry = [&](uint64_t set, uint64_t way, MtcTAEntry& entry) {
        uint64_t id = tileReqId++;
        VecTileRegStReq req;

        req.SetReqId(id);
        req.SetSize(MAX_TILE_DATA_BYTE);
        req.SetDest(entry.addr);
        req.SetData(entry.data.data(), 256);
        writeBackQ[static_cast<uint64_t>(MtcTAWriteType::EVICT)].Write(req);
        if (VERBOSE_ON) {
            std::cout << "Drain cache line: set=" << set << ", way=" << way
                        << ", Writing back to tileRegister addr=0x" << std::hex
                        << entry.addr << std::dec << std::endl;
        }
    };

    for (uint64_t set = 0; set < nSets; set++) {
        for (uint64_t way = 0; way < nWays; way++) {
            MtcTAEntry& entry = tBuffer[set][way];
            if (entry.valid && entry.state == MtcTAEntryState::MODIFIED) {
                processModifiedEntry(set, way, entry);
            }
        }
    }
}

void MemoryCoreTA::DumpTBufferState()
{
    cout << "\n=== TBuffer State ===" << endl;
    for (uint64_t s = 0; s < nSets; s++) {
        cout << "Set " << dec << s << ":" << endl;
        for (uint64_t w = 0; w < nWays; w++) {
            MtcTAEntry& entry = tBuffer[s][w];
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
void MemoryCoreTA::AllocLoadQueue(MemRequest &req, uint32_t _tid)
{
    uint32_t index = GetLdqFreeId(_tid);
    loadQueue[index].SetMemReq(req);
    loadQueue[index].SetAllocCycle(GetSim()->getCycles());
    loadQueue[index].stage = MtcTAStage::STAGE_E1;
    loadQueue[index].status = MtcTAReqStatus::PENDING;
    ldqEntryOccCnt[_tid]++;
    auto &entry = loadQueue[index];

    if (VERBOSE_ON) {
        cout << "[MTC TA Load Pipe E1 Stage] Recv load req: " << req << ", load queue Id=" << index << endl;
    }
    for (uint32_t lane = 0; lane < req.lanes; ++lane) {
        vtop->m_stats.tile_total_load_byte += req.width;
        // ASSERT((req.addrs.Get(lane, static_cast<uint32_t>(OperandWidth::OPDW_D)) & TILE_MASK) == TILE_MASK);
        // TILE_MASK, hack 处理, 区分load 请求是tile register/global memory
        // uint64_t addr = (req.addrs.Get(lane, static_cast<uint32_t>(OperandWidth::OPDW_D)) - TILE_MASK);
        uint64_t addr;
        if ((req.addrs.Get(lane, static_cast<uint32_t>(OperandWidth::OPDW_D)) & TILE_MASK) != TILE_MASK) {
            // TODO: addr need to check.
            addr = 0;
        } else {
            addr = (req.addrs.Get(lane, static_cast<uint32_t>(OperandWidth::OPDW_D)) - TILE_MASK);
        }
        bool isMisalign = isMisaligned(addr, req.width);
        uint64_t align_addr = addr & addrMask;
        if (VERBOSE_ON) {
            std::cout << "[MTC TA Load Pipe E1 Stage] Split req. Lane id: " << lane << " addr: 0x"
                << hex << addr << ", align addr:0x" << align_addr << dec << ", is cross 256B:"
                    << isMisalign << " req.width:" << req.width << endl;
        }
        entry.laneAddrVec.push_back(MtcLaneAddr(lane, addr, align_addr, req.width));
        entry.ldLaneStateVec.push_back(MtcLaneAddr(lane, addr, align_addr, req.width));
        entry.laneAddrResl.push_back(MtcLaneAddr(lane, addr, align_addr, req.width));
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
            entry.alignAddrs.push_back(MtcAlignReqInfo(align_addr));
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
                entry.alignAddrs.push_back(MtcAlignReqInfo(next_align_addr));
                entry.alignAddrs.back().e1Cycle = GetSim()->getCycles();
            }
        }
    }
    // for count
    entry.addrPmuCnt.addrCnt = entry.alignAddrs.size();

    if (VERBOSE_ON) {
        std::cout << "[MTC TA Load Pipe E1 Stage] Generate load req num:"
            << std::dec << " " << entry.alignAddrs.size() << entry.req << " " << entry.GetRemainAddrs() << std::endl;
    }
}

void MemoryCoreTA::ResolveLdqEntry(uint32_t id)
{
    if (VERBOSE_ON) {
        cout << "[MTC TA Load Pipe E3 Stage] Load Resolved: " << loadQueue[id].req << std::endl;
    }
    ROBID oldestBID = GetSim()->core->bctrl->blockROB.getOldestBlockID(0);
    BlockCommandPtr cmd = GetSim()->core->bctrl->blockROB.GetBlockCMDPtr(oldestBID, 0);
    if (IsBlockTypeNeedVReg(cmd->blockType)) {
        if ((VERBOSE_ON && STAGE_ENABLED(StageID::LSU_ALL))) {
            cout << "[MTC TA Load Pipe E3 Stage] Because load requests resolve. Reset the idle cycle." << endl;
        }
        GetSim()->ResetWaitCycle();
    }
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
    outputQ->Write(loadQueue[id].req);
}

void MemoryCoreTA::DeallocateLdqEntry(uint32_t id)
{
    loadQueue[id].valid = false;
    loadQueue[id].Reset();
    ldqEntryOccCnt[id/vtop->GetConfig().ta_ldq_size]--;
}

void MemoryCoreTA::FillWakeMissLoad(uint32_t id, TileRegVecLdRes req)
{
    auto &it = loadQueue[id];
    if (VERBOSE_ON) {
        std::cout << "[TA Load Pipe Fill] Wake up loadQueue entryId=" << it.entryId << ", reqId=" << req.GetRespId()
            << ", stage=" << EnumToString(it.stage) << " " << it.req << dec << std::endl;
    }

    auto fill = it.loadIds.find(req.GetRespId());
    if (fill == it.loadIds.end()) {
        return; // entry is flushed by inner branch
    }

    auto processLaneData = [&] (MtcTARequest& entry, uint64_t align_addr) {
        for (auto lane = entry.laneAddrResl.begin(); lane != entry.laneAddrResl.end();) {
            if (lane->alignAddr == align_addr) {
                std::vector<uint8_t> result(lane->bytes);
                auto copySize = lane->bytes;
                auto freeMemSize = result.capacity();
                ASSERT(freeMemSize >= static_cast<decltype(freeMemSize)>(copySize) && "Insufficient free memory!");
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
    it.loadIds.erase(fill);
    if (it.loadIds.empty() && it.alignAddrs.empty()) {
        it.status = MtcTAReqStatus::WAIT_OLDEST;
        ResolveLdqEntry(it.entryId);
        DeallocateLdqEntry(it.entryId);
    }
}

void MemoryCoreTA::FillWakeMergeLoad(uint32_t id, TileRegVecLdRes req)
{
    auto &it = loadQueue[id];
    uint64_t reqId = req.GetRespId();
    if (VERBOSE_ON) {
        std::cout << "[TA Load Pipe Fill] Wake up merge entryId=" << dec << it.entryId << ", reqId=" << reqId << " "
            << it.req << ", status=" << EnumToString(it.status) << std::endl;
    }
    uint64_t alignAddr = 0;
    auto itDel = std::find_if(it.alignAddrs.begin(), it.alignAddrs.end(),
        [reqId](const MtcAlignReqInfo& entry) {
            return entry.valid && entry.missReqId == reqId;
        });
    if (itDel!= it.alignAddrs.end()) {
        alignAddr = itDel->addr;
        it.alignAddrs.erase(itDel);
    } else {
        return; // entry is flushed by inner branch
    }

    auto processLaneData = [&](MtcTARequest& entry, uint64_t align_addr) {
        for (auto lane = entry.laneAddrResl.begin(); lane != entry.laneAddrResl.end();) {
            if (lane->alignAddr == align_addr) {
                std::vector<uint8_t> result(lane->bytes);
                auto copySize = lane->bytes;
                auto freeMemSize = result.capacity();
                ASSERT(freeMemSize >= static_cast<decltype(freeMemSize)>(copySize) && "Insufficient free memory!");
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
    if (VERBOSE_ON) {
        std::cout << "[MTC TA Load Pipe Fill] Wake up merge entryId=" << dec << it.entryId << ", alignAddrs size="
            << it.alignAddrs.size() << ", loadIds size=" << it.loadIds.size() << std::endl;
    }
    if (it.alignAddrs.empty() && it.loadIds.empty()) {
        it.status = MtcTAReqStatus::WAIT_OLDEST;
        ResolveLdqEntry(it.entryId);
        DeallocateLdqEntry(it.entryId);
    }
}

void MemoryCoreTA::DumpLoadQueue()
{
    std::cout << "dump load queue" << std::endl;
    for (auto& load : loadQueue) {
        if (!load.valid) {
            continue;
        }
        std::cout << load.req << dec << ", entryId=" << load.entryId << std::endl;
    }
}

void MemoryCoreTA::WakeupLoadQueue(TileRegVecLdRes req)
{
    // 遍历miss queue, 匹配reqId, 存放有entryId
    uint32_t missReqEntryId = UINT32_MAX;
    uint32_t missAddr = 0;
    // 处理miss req
    loadMissQueue->GetMissReqEntryId(req.GetRespId(), missReqEntryId, missAddr);
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
    loadMissQueue->GetMergeReqEntryId(req.GetRespId(), missReqEntryIds);

    for (auto it : missReqEntryIds) {
        FillWakeMergeLoad(it, req);
    }
    // 从miss queue 删除
    loadMissQueue->removeOnFill(req.GetRespId());
}

// ---------------------------------------store queue------------------------------------------------------------
uint32_t MemoryCoreTA::GetLdqFreeId(uint32_t _tid)
{
    for (uint32_t i = vtop->GetConfig().ta_ldq_size*_tid; i <= vtop->GetConfig().ta_ldq_size*(_tid+1)-1; i++) {
        if (!loadQueue[i].valid) {
            ASSERT(loadQueue[i].status == MtcTAReqStatus::INVALID);
            return loadQueue[i].entryId;
        }
    }
    ASSERT(0 && "no aval entry id!");
    return -1;
}

// ---------------------------------------store queue------------------------------------------------------------

uint32_t MemoryCoreTA::GetStqFreeId(uint32_t _tid)
{
    for (uint32_t i = vtop->GetConfig().ta_stq_size*_tid; i <= vtop->GetConfig().ta_stq_size*(_tid+1)-1; i++) {
            if (!storeQueue[i].valid) {
                ASSERT(storeQueue[i].status == MtcTAReqStatus::INVALID);
                return storeQueue[i].entryId;
            }
    }
    ASSERT(0 && "no aval entry id!");
    return UINT32_MAX;
}

void MemoryCoreTA::AllocStoreQueue(MemRequest &req, uint32_t _tid)
{
    uint32_t index = GetStqFreeId(_tid);
    ASSERT(index < UINT32_MAX);

    storeQueue[index].SetMemReq(req);
    if (!vtop->GetConfig().enable_scb) {
        storeQueue[index].stage = MtcTAStage::STAGE_E1;
    }
    storeQueue[index].SetAllocCycle(GetSim()->getCycles());
    storeQueue[index].status = MtcTAReqStatus::WAIT_OLDEST;

    stqEntryOccCnt[_tid]++;
    auto &entry = storeQueue[index];

    if (VERBOSE_ON) {
        cout << "[MTC TA Store Pipe E1 Stage] Recv store req: " << req << ", store queue entryId=" << index << endl;
    }

    auto addAlignAddr = [&](uint32_t lane, uint64_t addr, uint64_t alignAddr, uint64_t data, uint32_t width) {
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
            MtcLaneAddr laneAddr(lane, addr, alignAddr, width);
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
        uint64_t addr;
        if ((req.addrs.Get(lane, static_cast<uint32_t>(OperandWidth::OPDW_D)) & TILE_MASK) != TILE_MASK) {
            // TODO: addr need to check.
            addr = 0;
        } else {
            addr = (req.addrs.Get(lane, static_cast<uint32_t>(OperandWidth::OPDW_D)) - TILE_MASK);
        }

        bool isMisalign = isMisaligned(addr, req.width);
        uint64_t align_addr = addr & addrMask;
        uint64_t bytes_4B = req.data.Get(lane, req.width * 8);

        if (VERBOSE_ON) {
            std::cout << "[MTC TA Store Pipe E2 Stage] Split req. Lane id: " << lane << " addr: 0x"
                << hex << addr << ", align addr:0x" << align_addr << dec << ", is cross 256B:" << isMisalign << endl;
        }
        uint32_t width = isMisalign ? align_addr + entrySizes - addr : req.width;
        addAlignAddr(lane, addr, align_addr, bytes_4B, width);
        if (isMisalign) {
            uint64_t nextAlignAddr = align_addr + entrySizes;
            addAlignAddr(lane, nextAlignAddr, nextAlignAddr, bytes_4B >> (width * BITS_IN_BYTE), req.width - width);
        }
    };

    for (uint32_t lane = 0; lane < req.lanes; ++lane) {
        vtop->m_stats.tile_total_store_byte += req.width;
        processLaneAddress(lane);
    }
    ResolveStqEntry(index);
    if (VERBOSE_ON) {
        std::cout << "[TA Store Pipe] generate store req num:"
            << std::dec << entry.laneAddrVec.size() << std::endl;
    }
}

std::vector<uint8_t> MemoryCoreTA::LoadCamStq(uint32_t bid, uint32_t gid, uint32_t rid, uint64_t addr,
    unsigned size, bool& hit)
{
    std::vector<uint8_t> result(size, 0);
    MtcLaneAddr laneCam(-1, addr, addr, size);

    auto isRelevantStore = [&](const MtcTARequest& entry) {
        if (!entry.valid) return false;
        if (entry.req.bid.val != bid || entry.req.gid.val != gid) return false;
        if (entry.req.rid.val > rid) return false;
        return true;
    };

    auto processLane = [&](const MtcLaneAddr& storeLane) {
        MtcLaneDataTransfer mergeStq(laneCam, storeLane);

        if (VERBOSE_ON) {
            mergeStq.printCoverageInfo();
        }

        if (mergeStq.copyFromStoreToLoad()) {
            printLaneAddr(laneCam, "Load after copy");
            hit = true;
            if (laneCam.isValid()) {
                return true; // 找到匹配，提前返回
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
                return laneCam.data; // 找到匹配，提前返回
            }
        }
    }

    return laneCam.data;
}

bool MemoryCoreTA::IsLdqFull(uint32_t tid)
{
    return ldqEntryOccCnt[tid] == loadQueue.size()/GetSim()->core->configs.threadCount;
}

bool MemoryCoreTA::IsStqFull(uint32_t tid)
{
    return stqEntryOccCnt[tid] == storeQueue.size()/GetSim()->core->configs.threadCount;
}

void MemoryCoreTA::WakeupStoreQueue(uint64_t reqId)
{
    for (auto it = storeQueue.begin(); it != storeQueue.end(); it++) {
        auto evict = (*it).storeIds.find(reqId);
        if (evict != (*it).storeIds.end()) {
            if (VERBOSE_ON) {
                cout << "[MTC Store Fill Pipe] Wake up store entryId=" << it->entryId << ", Req: " << it->req << endl;
            }
            if ((*it).storeIds.size() == 1) {
                (*it).status = MtcTAReqStatus::WAIT_RSV;
                (*it).stage = MtcTAStage::STAGE_E3;
            }
            if (VERBOSE_ON && (*it).storeIds.size() == 1) {
                std::cout << "[MTC Store Fill Pipe] Store entryId=" << it->entryId << ", is waiting to resolve. Req: "
                    << it->req << std::endl;
            }
            (*it).storeIds.erase(evict);
        }
    }
}

void MemoryCoreTA::StoreCamLoadQueue(const MtcTARequest& entry)
{
    // for nuke,
    // 1. 比较store和load是否有地址重叠
    // 2. 如果有重叠，但load 已经是WAIT_OLDEST/WAIT_RSV 状态，表面load拿到了不正确数据，需要nuke
    auto checkLoadStoreOverlap = [&](MtcTARequest& loadEntry, const MtcTARequest& storeEntry) {
        for (size_t lane = 0; lane < loadEntry.ldLaneStateVec.size(); lane++) {
            MtcLaneDataTransfer mergeStq(loadEntry.ldLaneStateVec[lane], storeEntry.laneAddrVec[lane]);

            if (mergeStq.hasOverlap() && loadEntry.ldLaneStateVec[lane].dataFromTile) {
                if (VERBOSE_ON) {
                    std::cout << "[MTC TA Store Pipe E2 Stage] load data from tile register, nuke load, store entryId="
                              << storeEntry.entryId << ", load lane=" << lane << ", load addr=0x" << std::hex
                              << loadEntry.ldLaneStateVec[lane].addr << ", size="
                              << loadEntry.ldLaneStateVec[lane].bytes << std::endl;
                }
                loadEntry.status = MtcTAReqStatus::WAIT_NUKE;
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

std::vector<MtcTARequest> MemoryCoreTA::SortStqByRID(uint32_t gid)
{
    std::vector<MtcTARequest> result;

    // 筛选出相同groupId的entry
    for (const auto& entry : storeQueue) {
        if (entry.valid && entry.req.gid.val == gid) {
            result.push_back(entry);
        }
    }

    std::sort(result.begin(), result.end(), std::greater<MtcTARequest>());

    return result;
}

void MemoryCoreTA::ResolveStqEntry(uint32_t id)
{
    if (VERBOSE_ON) {
        cout << "[MTC TA Store Stage] Store Resolved. Req: " << storeQueue[id].req << std::endl;
    }
    ROBID oldestBID = GetSim()->core->bctrl->blockROB.getOldestBlockID(0);
    BlockCommandPtr cmd = GetSim()->core->bctrl->blockROB.GetBlockCMDPtr(oldestBID, 0);
    if (IsBlockTypeNeedVReg(cmd->blockType)) {
        if ((VERBOSE_ON && STAGE_ENABLED(StageID::LSU_ALL))) {
            cout << "[MTC TA Load Pipe E3 Stage] Because store requests resolve. Reset the idle cycle." << endl;
        }
        GetSim()->ResetWaitCycle();
    }
    storeQueue[id].SetEraseCycle(GetSim()->getCycles());
    vtop->m_stats.tile_total_store_lat += storeQueue[id].GetEraseCycle() - storeQueue[id].GetAllocCycle();

    outputQ->Write(storeQueue[id].req);
}

void MemoryCoreTA::RetireStore()
{
    /*
    * 1. 遍历Store Queue, 挑选RETIRE_SCB的entry
    * 2. 按retire带宽retire到SCB
    * 3. deallocate entry
    */
    std::vector<MtcTARequest> validStoreEntry;
    for (const auto& store : storeQueue) {
        // TODO:需要rob 修改non-flush ptr
        if (store.valid) {
            validStoreEntry.push_back(store);
        }
    }

    if (validStoreEntry.empty()) {
        return;
    }

    std::sort(validStoreEntry.begin(), validStoreEntry.end(),
        [](const MtcTARequest& a, const MtcTARequest& b) {
            return a > b;
        });
    uint64_t retireCnt = 0;
    for (auto it = validStoreEntry.begin(); it != validStoreEntry.end();) {
        bool noCredit = false;
        auto& entry = storeQueue[it->entryId];
        ASSERT(entry.valid);
        for (auto lane = entry.laneAddrVec.begin(); lane != entry.laneAddrVec.end();) {
            if (scb->IsFull() || (retireCnt == vtop->GetConfig().ta_store_retire_band)) {
                noCredit = true;
                break;
            }
            if (scb->CannotMerge((*lane).alignAddr)) {
                break;
            }
            VecTileRegStReq req = VecTileRegStReq();

            req.SetReqId(99);
            req.SetDest((*lane).alignAddr);
            req.SetData((*lane).data.data(), 256);
            req.SetSize(MAX_TILE_DATA_BYTE);
            req.SetDataMask((*lane).mask);
            req.SetBid(entry.req.bid);
            req.SetGid(entry.req.gid);
            req.SetRid(entry.req.rid);
            scb->vecSCBStReqQ.emplace_back(req);
            scb->HandleStore();
            retireCnt++;
            if (VERBOSE_ON) {
                std::cout << "[Tile Store Retire] store retire to scb, has credit=" << !noCredit << " B" <<
                    entry.req.bid.GetVal() << ":G" << entry.req.gid.GetVal() << ":R" << entry.req.rid.GetVal()
                        << hex << ", addr=0x" << (*lane).alignAddr << dec << endl;
            }
            lane = entry.laneAddrVec.erase(lane);
        }
        if (noCredit) {
            break;
        }
        if (entry.laneAddrVec.empty()) {
            DeallocateStqEntry(it->entryId);
            it = validStoreEntry.erase(it);
        } else {
            it++;
        }
    }
}

void MemoryCoreTA::DeallocateStqEntry(uint32_t id)
{
    if (VERBOSE_ON) {
        std::cout << "[Tile Store] release entryId=" << id << " "<< storeQueue[id].req << dec << endl;
    }
    storeQueue[id].valid = false;
    storeQueue[id].Reset();
    stqEntryOccCnt[id/vtop->GetConfig().ta_stq_size]--;
}

void MemoryCoreTA::DumpStoreQueue()
{
    std::cout << "==dump store queue==" << std::endl;
    for (auto entry: storeQueue) {
        if (entry.valid) {
            std::cout << dec << "entryId=" << entry.entryId << ", req=" << entry.req << ", status="
                << EnumToString(entry.status) << std::endl;
        }
    }
}

bool MemoryCoreTA::CheckEmptyByBid(ROBID &bid)
{
    for (auto entry: storeQueue) {
        if (entry.valid && entry.req.bid == bid) {
            return false;
        }
    }

    return true;
}
// ---------------------------------------pipeline------------------------------------------------------------
void MemoryCoreTA::I2()
{
    // do nothing
}

void MemoryCoreTA::E1()
{
    // 从lda_pipe/sta_pipe 接收请求
    loadQ.Work();
    // load => allocate load queue
    int handled = 0;
    while (!loadQ.Empty() && handled < ldBand) {
        auto req = loadQ.Read();
        if (!IsLdqFull(req.tid)) {
            AllocLoadQueue(req, req.tid);
            LdQShift();
            ldInPipeCnt--;
            assert(ldInPipeCnt >= 0);
            ++handled;
        } else {
            ASSERT(0);
        }
    }
    // store => allocate store queue
    storeQ.Work();
    handled = 0;
    // store => allocate store queue
    while (!storeQ.Empty() && handled < stBand) {
        auto req = storeQ.Read();
        if (!IsStqFull(req.tid)) {
            AllocStoreQueue(req, req.tid);
            StQShift();
            ++handled;
        } else {
            ASSERT(0);
        }
    }
}

uint32_t MemoryCoreTA::LoadArbitration()
{
    for (uint32_t _tid = arbTid+1; _tid < GetSim()->core->configs.threadCount + arbTid+1; _tid ++) {
        for (int j = vtop->GetConfig().ta_ldq_size - 1; j >= 0; j--) {
            int tmp = _tid % GetSim()->core->configs.threadCount;
            if (loadQueue[tmp*vtop->GetConfig().ta_ldq_size+j].status == MtcTAReqStatus::PENDING &&
                loadQueue[tmp*vtop->GetConfig().ta_ldq_size+j].stage != MtcTAStage::STAGE_E1 &&
                loadQueue[tmp*vtop->GetConfig().ta_ldq_size+j].valid) {
                    arbTid = tmp;
                    return tmp*vtop->GetConfig().ta_ldq_size+j;
            }
        }
    }
    return UINT32_MAX;
}

uint32_t MemoryCoreTA::StoreArbitration()
{
    for (uint32_t _tid = arbTid+1; _tid < GetSim()->core->configs.threadCount + arbTid+1; _tid ++) {
        for (int j = vtop->GetConfig().ta_ldq_size - 1; j >= 0; j--) {
            int tmp = _tid % GetSim()->core->configs.threadCount;
            if (storeQueue[tmp*vtop->GetConfig().ta_stq_size+j].status == MtcTAReqStatus::PENDING &&
                storeQueue[tmp*vtop->GetConfig().ta_stq_size+j].stage != MtcTAStage::STAGE_E1 &&
                storeQueue[tmp*vtop->GetConfig().ta_stq_size+j].valid) {
                    arbTid = tmp;
                    return tmp*vtop->GetConfig().ta_stq_size+j;
            }
        }
    }
    return UINT32_MAX;
}

void MemoryCoreTA::LdQShift()
{
    for (uint64_t i = 0 ; i < GetSim()->core->configs.threadCount; i++) {
        for (int j = vtop->GetConfig().ta_ldq_size-1; j >= 1; j--) {
            if (!loadQueue[i*vtop->GetConfig().ta_ldq_size+j].valid &&
                loadQueue[i*vtop->GetConfig().ta_ldq_size+j-1].valid) {
                    std::swap(loadQueue[i*vtop->GetConfig().ta_ldq_size+j],
                        loadQueue[i*vtop->GetConfig().ta_ldq_size+j-1]);
                    loadQueue[i*vtop->GetConfig().ta_ldq_size+j].entryId = i*vtop->GetConfig().ta_ldq_size+j;
                    loadQueue[i*vtop->GetConfig().ta_ldq_size+j-1].entryId = i*vtop->GetConfig().ta_ldq_size+j-1;
                    loadQueue[i*vtop->GetConfig().ta_ldq_size+j-1].Reset();
                    loadMissQueue->shift(i*vtop->GetConfig().ta_ldq_size+j);
            }
        }
    }
}

void MemoryCoreTA::StQShift()
{
    for (uint64_t i = 0 ; i < GetSim()->core->configs.threadCount; i++) {
        for (int j = vtop->GetConfig().ta_stq_size-1; j >= 1; j--) {
            if (!storeQueue[i*vtop->GetConfig().ta_stq_size+j].valid &&
                storeQueue[i*vtop->GetConfig().ta_stq_size+j-1].valid) {
                    std::swap(storeQueue[i*vtop->GetConfig().ta_stq_size+j],
                        storeQueue[i*vtop->GetConfig().ta_stq_size+j-1]);
                    storeQueue[i*vtop->GetConfig().ta_stq_size+j].entryId = i*vtop->GetConfig().ta_stq_size+j;
                    storeQueue[i*vtop->GetConfig().ta_stq_size+j-1].entryId = i*vtop->GetConfig().ta_stq_size+j-1;
                    storeQueue[i*vtop->GetConfig().ta_stq_size+j-1].Reset();
            }
        }
    }
}

void MemoryCoreTA::E2()
{
    uint32_t LdIndex = LoadArbitration();
    if (LdIndex != UINT32_MAX) {
        LoadHandleE2(LdIndex);
    }

    uint32_t StIndex = StoreArbitration();
    if (StIndex != UINT32_MAX) {
        StoreHandleE2(StIndex);
    }
}

void MemoryCoreTA::E3()
{
    for (auto it = storeQueue.begin(); it != storeQueue.end(); it++) {
        if ((*it).stage == MtcTAStage::STAGE_E3) { // enable scb不走这
            if ((*it).status == MtcTAReqStatus::WAIT_RSV) {
                DeallocateStqEntry((*it).entryId);
            }
        }
    }
}

void MemoryCoreTA::E4()
{
    // do nothing
}

void MemoryCoreTA::LoadHandleE2(int _index)
{
    auto it = &loadQueue[_index];
    assert(it->valid && it->status == MtcTAReqStatus::PENDING);
    uint32_t align_256B_addr = UINT32_MAX;

    for (auto lane = it->laneAddrVec.begin(); lane != it->laneAddrVec.end();) {
        bool hitStqEntry = false;
        std::vector<uint8_t> data = LoadCamStq(it->req.bid.GetVal(), it->req.gid.GetVal(),
            it->req.rid.GetVal(), lane->addr, lane->bytes, hitStqEntry);

        if (hitStqEntry) {
            std::cout << "hit store queue "<< std::endl;
            it->req.data.SetVal(data, lane->laneId, lane->bytes);
            if (lane->isValid()) {
                lane = it->laneAddrVec.erase(lane);
                vtop->m_stats.tile_load_to_use[1]++;
                continue;
            }
        }
        std::vector<uint8_t> laneData;
        if (use_tile_cache && IsTBufferHit(*lane, laneData)) {
            align_256B_addr = lane->alignAddr;
            it->req.data.SetVal(laneData, lane->laneId, lane->bytes);
            lane = it->laneAddrVec.erase(lane);
        } else {
            lane++;
        }
        auto itDel = std::find_if(it->alignAddrs.begin(), it->alignAddrs.end(),
            [align_256B_addr] (const MtcAlignReqInfo& entry) {
                return entry.valid && entry.addr == align_256B_addr;
            });
        if (itDel != it->alignAddrs.end()) {
            it->alignAddrs.erase(itDel);
        }
    }
    for (auto align_256B = it->alignAddrs.begin(); align_256B != it->alignAddrs.end();) {
        if (!align_256B->valid || (align_256B->stateFSM == MtcAddrState::HIT_CACHE) ||
            align_256B->WaitMissFill()) {
                align_256B++;
                continue;
            }

        uint32_t hitEntryId = 0;
        uint64_t hitReqId = 0;
        // 检查Miss Queue, 是否hit 到已经发送的miss req
        if (loadMissQueue->findReqByAddress(align_256B->addr, it->entryId, hitEntryId, hitReqId)) {
            align_256B->stateFSM = MtcAddrState::MISS_HIT_OTHER_REQUEST;
            align_256B->missReqEntryId = hitEntryId;
            align_256B->missReqId = hitReqId;
            it->addrPmuCnt.missMergeCnt += 1;
            align_256B++;
            continue;
        }
        uint32_t reqId = CreateTileRegLdReq(align_256B->addr, it->entryId);
        if (reqId!= UINT32_MAX) {
            it->loadIds.emplace(reqId, align_256B->addr);
            it->addrPmuCnt.missReqCnt += 1;
            align_256B = it->alignAddrs.erase(align_256B);
        } else {
            align_256B++;
        }

        if (VERBOSE_ON && reqId != UINT32_MAX) {
            cout << "[TA Load Pipe] send miss req, entryId=" << it->entryId << ", reqId=" <<reqId<<", addr=0x"
                << hex << align_256B->addr << " " << (*it).req << dec << std::endl;
        }
        // 每个load pipe只发1个miss 请求
        break;
    }
    it->status = it->alignAddrs.empty() ? MtcTAReqStatus::WAIT_RSP : it->status;
}

void MemoryCoreTA::StoreHandleE2(int _index)
{
    auto it = &storeQueue[_index];
    ASSERT((!it->valid)&&(it->status != MtcTAReqStatus::PENDING));
    StoreCamLoadQueue(*it);

    bool allSend = true;
    for (auto &lane : it->laneAddrVec) {
        if (lane.sended) {
            continue;
        }
        uint32_t reqId = CreateTileWriteReq(it->entryId, lane.alignAddr, lane.data, 256, lane.mask);
        if (reqId != UINT32_MAX) {
            lane.sended = true;
            it->storeIds.emplace(reqId, lane.alignAddr);
            if (VERBOSE_ON) {
                std::cout << "[Tile Store E2 Stage], send store data to tile register, reqId=" << reqId
                            << ", bid=" << it->req.bid.GetVal() << ", gid=" << it->req.gid.GetVal()
                            << ", rid=" << it->req.rid.GetVal() << hex << ", addr=0x"
                            << lane.alignAddr << dec << endl;
            }
        } else {
            allSend = false;
            break;
        }
    }
    it->status = allSend ? MtcTAReqStatus::WAIT_RSP : MtcTAReqStatus::PENDING;
}

void MemoryCoreTA::DrainEmpty()
{
    if (!scb->isInDrainMode) {
        return;
    }
    if (vtop->GetConfig().enable_scb) {
        startDrainScb = false;
        if (scb->IsEmpty() && CheckEmptyByBid(drainBlockId)) {
            ScbDrainBus drainInfo{0, drainBlockId};
            drainScbRespQ->Write(drainInfo);
            scb->isInDrainMode = false;
        }
    } else {
        if (CheckEmptyByBid(drainBlockId)) {
            ScbDrainBus drainInfo = {0, drainBlockId};
            drainScbRespQ->Write(drainInfo);
            scb->isInDrainMode = false;
        }
    }
}

// 处理oldest
void MemoryCoreTA::ReceiveNonFlush()
{
    processLoadNonFlush();
    processStoreNonFlush();
}

void MemoryCoreTA::processLoadNonFlush()
{
    if (GetSim()->core->configs.mtc_tls_enable) {
        return;
    }
    if (vtop->loadNonFlushQ->Empty()) return;
    auto isMatchingLoadEntry = [&](const MtcTARequest& entry, LdNonFlushBus &req) {
        return LessEqual(entry.req.bid, entry.req.gid, entry.req.lsID, req.bid, req.gid, req.lsID) &&
            req.tid == entry.req.tid &&
            entry.status == MtcTAReqStatus::WAIT_OLDEST;
    };

    while (!vtop->loadNonFlushQ->Empty()) {
        LdNonFlushBus req = vtop->loadNonFlushQ->Read();

        for (auto& entry : loadQueue) {
            if (isMatchingLoadEntry(entry, req)) {
                if (VERBOSE_ON) {
                    std::cout << "[MTC TA Load Pipe] Load Resolved. Req: " << entry.req << endl;
                }
                DeallocateLdqEntry(entry.entryId);
                break;
            }
        }
    }
}

void MemoryCoreTA::processStoreNonFlush()
{
    if (vtop->storeNonFlushQ->Empty()) return;

    auto processMatchingStoreEntries = [this](const StNonFlushBus &req) {
        for (auto& entry : storeQueue) {
            if (!(LessEqual(entry.req.bid, entry.req.gid, entry.req.lsID, req.bid, req.gid, req.lsID) &&
                    req.tid == entry.req.tid && entry.status == MtcTAReqStatus::WAIT_OLDEST)) {
                continue;
            }
            auto updateEntryStatus = [this, &entry]() {
                if (vtop->GetConfig().enable_scb) {
                    entry.status = MtcTAReqStatus::RETIRE_SCB;
                    if (VERBOSE_ON) {
                        std::cout << "[TA Store Pipe] lsid=" << entry.req.lsID << " Req: " << entry.req
                            << " is oldest, wait to write scb" << dec << endl;
                    }
                } else {
                    entry.status = MtcTAReqStatus::PENDING;
                    entry.stage = MtcTAStage::STAGE_E2;
                    if (VERBOSE_ON) {
                        std::cout << "[TA Store Pipe] Req: " << entry.req << " is oldest, pick to E2 stage"<<dec<< endl;
                    }
                }
            };

            updateEntryStatus();
        }
    };

    while (!vtop->storeNonFlushQ->Empty()) {
        StNonFlushBus req = vtop->storeNonFlushQ->Read();
        processMatchingStoreEntries(req);
    }
}

// ---------------------------------------tileregister interface----------------------------------------------

void MemoryCoreTA::SendTRReq()
{
}

void MemoryCoreTA::ReceiveTlsData()
{
    scb->ProcessTStqReq();
}

void MemoryCoreTA::ReceiveTRData()
{
    // 收到tileRegister load data
    // (1) 回填TBuffer

    // (2) 通过reqId/Bid/Gid/rid wakeup load queue, 状态机转 E2
    if (vtop->pTileUtils->IsRingOrXbarMode(false)) {
        LoadFromRing();
        return;
    }
    int i = 0;
    while (!tileRegLdResQ->Empty() && i < 2) {
        auto res = tileRegLdResQ->Read();
        if (VERBOSE_ON) {
            cout << "MTC Recv data resp, reqId=" << res.GetRespId() << endl;
        }
        WakeupLoadQueue(res);
        ++i;
    }
}

void MemoryCoreTA::ReceiveTRRsp()
{
    if (vtop->pTileUtils->IsRingOrXbarMode(false)) {
        StoreFromRing();
        return;
    }
    if (!vtop->GetConfig().enable_scb) {
        int i = 0;
        while (!tileRegWrRspQ->Empty() && i < stBand) {
            auto res = tileRegWrRspQ->Read();
            if (VERBOSE_ON) {
                std::cout << "[MTC TA] Recv response from tile reg, reqId=" << res.GetRespId() << std::endl;
            }
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

uint32_t MemoryCoreTA::CreateTileRegLdReq(uint64_t addr, uint32_t eId, uint64_t size)
{
    auto tmpaddr = addr;
    auto req = VecTileRegLdReq();
    uint64_t originAddr = addr;
    addr = addr & (~TILE_MASK);
    if (VERBOSE_ON) {
        std::cout << "[MTC TA] Creating Tile Register ld req, reqId=" << dec << tileReqId << " , addr=0x"
                  << std::hex << addr << ", origin_addr=0x" << tmpaddr << std::endl << std::dec;
    }
    if ((addr | TILE_MASK) == static_cast<uint64_t>(-1ULL)) {
        if (VERBOSE_ON) {
            std::cout << "[MTC TA] Creating Tile Register ld req, report excetopn. Request is "
                << loadQueue[eId].req << ". Calculate addr is 0x" << addr << dec << std::endl;
        }
        // GetSim()->core->bctrl->blockROB.reportException(loadQueue[eId].req.bid, "Vector TA LD req create");
        // return UINT32_MAX;
        // TODO: check the result
        addr = 0;
    }
    req.SetReqId(tileReqId);
    req.SetSize(size);
    req.SetSrc(addr);
    if (vtop->pTileUtils->IsRingOrXbarMode(false)) {
        bool succ = LoadToRing(req) && loadMissQueue->addMissRequest(originAddr, tileReqId, eId);
        if (!succ) {
            return UINT32_MAX;
        }
    } else {
        if (loadMissQueue->addMissRequest(originAddr, tileReqId, eId)) {
            this->tileRegLdReqQ->Write(req);
        } else {
            return UINT32_MAX;
        }
    }

    return tileReqId++;
}

uint32_t MemoryCoreTA::CreateTileWriteReq(unsigned entryId, uint64_t addr, const std::vector<uint8_t>& data,
                                          uint64_t size, DataMask mask)
{
    auto tmpaddr = addr;
    VecTileRegStReq req = VecTileRegStReq();
    uint32_t reqId = tileReqId++;
    // 调整地址, 按照 Tile 的地址粒度来调整
    addr = addr & (~TILE_MASK);
    if ((addr | TILE_MASK) == static_cast<uint64_t>(-1ULL)) {
        if (VERBOSE_ON) {
            std::cout << "[MTC TA] Creating Tile Register st req, report excetopn. Request is "
                << storeQueue[entryId].req  << ". Calculate addr is " << addr << std::endl;
        }
        // GetSim()->core->bctrl->blockROB.reportException(storeQueue[entryId].req.bid, "Vector TA ST req create");
        // return UINT32_MAX;
        // TODO: check the result
        addr = 0;
    }
    req.SetReqId(reqId);
    req.SetDest(addr);
    req.SetData(data.data(), size);
    req.SetSize(size);
    req.SetDataMask(mask);
    if (vtop->pTileUtils->IsRingOrXbarMode(false)) {
        bool succ = StoreToRing(req, mask);
        if (!succ) {
            return UINT32_MAX;
        }
        if (VERBOSE_ON) {
            std::cout << "[MTC TA] Creating Ring st req, reqId=" << dec << reqId << " , addr=0x"
                    << std::hex << addr << ", size:" << size << " origin_addr=0x" << tmpaddr << std::endl << std::dec;
        }
    } else {
        this->tileRegStReqQ->Write(req);
        if (VERBOSE_ON) {
            std::cout << "[MTC TA] Creating Queue st req, reqId=" << dec << reqId << " , addr=0x"
                    << std::hex << addr << ", size:" << size << " origin_addr=0x" << tmpaddr << std::endl << std::dec;
        }
    }
    if (!vtop->GetConfig().enable_scb) {
        storeQueue[entryId].writeCyc = GetSim()->cycles;
    }
    if (VERBOSE_ON) {
        cout << "[MemoryCore TA] Store reqId=" << reqId << endl;
    }

    return reqId;
}

bool MemoryCoreTA::LoadToRing(VecTileRegLdReq req)
{
    if (!vtop->HasValidSpbReadEntry()) {
        if (VERBOSE_ON) {
            std::cout << "[MTC TA] No Valid spb" << std::endl;
        }
        return false;
    }

    std::shared_ptr<CrossStation> station = vtop->GetFreeReadStation();
    // 1: MTC 使用第二个buffer, bridge使用第一个buffer
    std::shared_ptr<Request> pkt = station->Rxreq(req.GetSrc(), 1, req.GetStid());
    pkt->SetSize(req.GetSize());
    pkt->SetBufId(req.GetReqId());
    station->WriteSpb(pkt);
    if (VERBOSE_ON) {
        std::cout << "[MTC TA]:" << std::hex << *pkt << " will be sent to ring " <<std::endl;
    }
    return true;
}

bool MemoryCoreTA::StoreToRing(VecTileRegStReq req, DataMask mask)
{
    if (!vtop->HasValidSpbWriteEntry()) {
        return false;
    }

    std::shared_ptr<CrossStation> station = vtop->GetFreeWriteStation();
    // 1: MTC 使用第二个buffer, bridge使用第一个buffer
    std::shared_ptr<Request> pkt = station->Rxdat(req.GetDest(), 1, req.GetStid());
    pkt->SetSize(req.GetSize());
    pkt->SetBufId(req.GetReqId());
    uint8_t data[MAX_TILE_DATA_BYTE];
    uint8_t *stData = req.GetData();
    for (size_t i = 0; i < MAX_TILE_DATA_BYTE; ++i) {
        data[i] = stData[i];
    }
    pkt->SetData(data);
    pkt->dmask = mask;
    station->WriteSpb(pkt);
    if (VERBOSE_ON) {
        std::cout << "[MTC TA]:" << std::hex << *pkt << " store req will be sent to ring " <<std::endl;
    }
    return true;
}

void MemoryCoreTA::LoadFromRing()
{
    for (uint32_t i = 0; i < vtop->stationReadRange; i++) {
        auto &station = vtop->stations[i];
        // 1: MTC 使用第二个buffer, bridge使用第一个buffer
        if (station->HasNoResp(1)) {
            continue;
        }
        // 1: MTC 使用第二个buffer, bridge使用第一个buffer
        std::shared_ptr<Request> pkt = station->GetDataComReq(1);

        uint8_t *data = pkt->GetData();
        uint32_t reqId = pkt->GetBufId();
        TileRegVecLdRes res;
        res.SetRespId(reqId);
        res.SetData(data);
        if (VERBOSE_ON) {
            std::cout << "[Vector TA] Recv response from ring." << *pkt << std::endl;
        }
        WakeupLoadQueue(res);
    }
}

void MemoryCoreTA::StoreFromRing()
{
    for (uint32_t i = vtop->stationReadRange; i < vtop->stations.size(); i++) {
        auto &station = vtop->stations[i];
        // 1: MTC 使用第二个buffer, bridge使用第一个buffer
        if (station->HasNoResp(1)) {
            continue;
        }
        // 1: MTC 使用第二个buffer, bridge使用第一个buffer
        std::shared_ptr<Request> pkt = station->GetDataComReq(1);

        if (VERBOSE_ON) {
            std::cout << "[MTC TA] Recv write response from ring. " << *pkt << std::endl;
        }
        if (!vtop->GetConfig().enable_scb) {
            WakeupStoreQueue(pkt->GetBufId());
        } else {
            scb->HandleTileRegRsp(pkt->GetBufId());
        }
    }
}

SimSys* MemoryCoreTA::GetSim()
{
    if (vtop == nullptr) {
        return nullptr;
    }
    return vtop->GetSim();
}

void MemoryCoreTA::Resolve()
{
    if (!(*this->resolveSignal)) {
        return;
    }

    if (VERBOSE_ON) {
        std::cout << "[MTC TA] Recv resolve signal" << std::endl;
    }
}

static bool CheckFlush(FlushBus &bus, ROBID &bid, ROBID &gid, ROBID &subId)
{
    if (bus.baseOnBid) {
        return LessEqual(bus.req.bid, bid);
    }

    return LessEqual(bus.req.bid, bus.req.gid, bus.req.rid, bid, gid, subId);
}

void MemoryCoreTA::Flush(FlushBus &bus)
{
    // TODO: Use lsID instead of rid.
    // Load/store input
    auto matchReq = [&bus](MemRequest &req) {
        if (bus.baseOnThread && req.tid != bus.req.tid) {
            return false;
        }
        if (bus.baseOnBid) {
            return LessEqual(bus.req.bid, req.bid);
        }
        return LessEqual(bus.req.bid, bus.req.gid, bus.req.rid, req.bid, req.gid, req.rid);
    };

    if (VERBOSE_ON) {
        cout << "[MTC TA] Recv flush bus " << bus << endl;
    }
    loadQ.FlushIf(matchReq);
    storeQ.FlushIf(matchReq);
    outputQ->FlushIf(matchReq);
    // TODO: Need to flush the tile queue, but there are errors. Error case is here.
    for (auto it = loadQueue.begin(); it != loadQueue.end(); it++) {
        if (!(*it).valid) {
            continue;
        }

        if (CheckFlush(bus, (*it).req.bid, (*it).req.gid, (*it).req.rid)) {
            (*it).Reset();
            ldqEntryOccCnt[(*it).entryId/vtop->GetConfig().ta_ldq_size]--;
            if (VERBOSE_ON) {
                cout << dec << "[MTC TA] flush, load bid=" << (*it).req.bid
                    << ", gid=" << (*it).req.gid
                    << ", rid=" << (*it).req.rid << endl;
            }
            (*it).Reset();
        }
    }
    for (auto it = storeQueue.begin(); it != storeQueue.end(); it++) {
        if (!(*it).valid) {
            continue;
        }

        if (CheckFlush(bus, (*it).req.bid, (*it).req.gid, (*it).req.rid)) {
            (*it).Reset();
            stqEntryOccCnt[(*it).entryId/vtop->GetConfig().ta_stq_size]--;
            if (VERBOSE_ON) {
                cout << "[MTC TA] flush, store bid=" << dec << (*it).req.bid
                    << ", gid=" << (*it).req.gid
                    << ", rid=" << (*it).req.rid << endl;
            }
            (*it).Reset();
        }
    }
}

std::ostream& operator<<(std::ostream& out, const struct MtcTAEntry& entry)
{
    out << "tag: 0x" << entry.tag << std::endl;
    ASSERT(entry.state <= MtcTAEntryState::MODIFIED);
    out << "state: " << MtcTAEntryStateInfo[entry.state];
    return out;
}

} // namespace JCore
