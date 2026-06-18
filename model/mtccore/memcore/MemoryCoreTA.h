#ifndef MEMORY_CORE_TA_H
#define MEMORY_CORE_TA_H

#include <memory>
#include <unordered_set>
#include <vector>

#include "core/Bus.h"
#include "interface/InterfaceCommon.h"
#include "tmu/ring/RingNodeObj.h"
#include "mtccore/memcore/MemoryCoreTAStats.h"
#include "mtccore/memcore/TileSCB.h"


namespace JCore {

class MemoryCore;
class MtcRequest;

/*
 * FIXME: impl detailed vector core lsu pipeline
 */
enum class MtcTAEntryState {
    INVALID = 0,
    SHARED = 1,
    MODIFIED = 2,
};

// 优先级从高到低
enum class MtcTAWriteType {
    EVICT,
    READ_FILL,
    MTC_LDQ_REPICK,
    LOAD,
    STORE,

    TA_WRITE_END
};

enum class MtcTAReadType {
    LD_REPICK,
    LD,
};

enum class MtcTAReqStatus {
    INVALID,
    READY,
    // 等待被发出
    PENDING,
    // 已经发出
    SENT,
    // 收到 RETRY 信号
    RETRY,
    WAIT_OLDEST,
    WAIT_RSP,
    WAIT_RSV,
    RETIRE_SCB,
    WAIT_NUKE,
    SUACCELSS
};

enum class MtcTAStage {
    INVALID,
    STAGE_E1,
    STAGE_E2,
    STAGE_E3
};

struct MtcLaneAddr {
    bool sended;
    uint32_t laneId;
    uint64_t addr;
    uint64_t alignAddr;
    uint32_t bytes;
    std::vector<uint8_t> data;
    DataMask mask = DataMask(1);
    bool dataFromTile;
    bool dataFromStq;
    uint32_t hitStqRid;

    bool isValid() const
    {
        return data.size() == bytes;
    }

    MtcLaneAddr(unsigned lane_id, uint64_t base_addr, uint64_t aligned_address, unsigned byte_count)
        : sended{false}, laneId{lane_id}, addr{base_addr}, alignAddr{aligned_address}, bytes{byte_count}, data(0, 0)
        {
        dataFromTile = false;
        dataFromStq = false;
        hitStqRid = 0;
        }
};

struct MtcStMergeEntry {
    // addr should be align
    uint64_t addr;
    uint64_t reqId;
    std::vector<uint8_t> data;
    bool ready = false;

    MtcStMergeEntry(uint64_t addr, uint64_t reqId, const std::vector<uint8_t>& data, bool ready = false) : addr(addr),
        reqId(reqId), data(data), ready(ready) {};
};

enum class MtcAddrState {
    INVALID,
    HIT_CACHE,                    // hit tile cache
    MISS_SEND_REQUEST,            // tile cache miss && 请求发送
    MISS_WAIT_SEND,               // tile cache miss && 等待请求发送
    MISS_HIT_OTHER_REQUEST        // tile cache miss && hit 已经发送的miss请求
};

struct MtcAlignReqInfo {
    bool valid = false;
    uint64_t addr = 0;
    MtcAddrState stateFSM = MtcAddrState::INVALID;
    uint32_t e1Cycle = 0;
    uint32_t hitCycle = 0;
    uint32_t missReqCycle = 0;
    uint32_t missFillCycle = 0;
    int missReqEntryId;
    uint32_t missReqId;
    MtcAlignReqInfo(uint64_t a) : valid(true), addr(a) {}
    bool WaitMissFill() const
    {
        return stateFSM == MtcAddrState::MISS_SEND_REQUEST || stateFSM == MtcAddrState::MISS_HIT_OTHER_REQUEST;
    }
};

struct MtcAddrCounter {
    int addrCnt; // align 256B
    int ldToUse;
    int missLatency;
    int hitCacheCnt;
    int hitStqCnt;
    int hitScbCnt;
    int missCnt;
    int missReqCnt;
    int missMergeCnt;
};

struct MtcTARequest {
    bool valid;
    uint32_t alloc_cycle;
    uint32_t erase_cycle;
    uint32_t entryId;
    MtcTAReqStatus status;
    MtcTAStage stage;
    uint32_t src;
    uint32_t size;
    // 返回的数据填充到哪个 way
    uint32_t way;
    // evict 的时候需要用到这个
    // 只有当 evict 写回完成才可以继续
    uint32_t dependId;
    // 记录 store 发出的 cycle，来定期做清除 id
    uint64_t writeCyc;
    MtcAddrCounter addrPmuCnt;

    MtcTAWriteType type;
    MemRequest req;
    std::vector<MtcAlignReqInfo> alignAddrs; // align 256B address
    std::vector<MtcLaneAddr> laneAddrVec; // 64 lane addr, bytes
    std::vector<MtcStMergeEntry> stMergeData;
    std::vector<MtcLaneAddr> laneAddrResl;
    std::vector<MtcLaneAddr> ldLaneStateVec; // 64 lane state
    std::unordered_map<uint32_t, uint64_t>  loadIds; // for miss, reqId - addr
    std::unordered_map<uint32_t, uint64_t>  storeIds; // for miss, reqId - addr
    void Init(uint32_t index)
    {
        valid = false;
        entryId = index;
        status = MtcTAReqStatus::INVALID;
        stage = MtcTAStage::INVALID;
        src = 0;
        size = 0;
        way = 0;
        dependId = static_cast<uint32_t>(-1);
        writeCyc = 0;
        alignAddrs.clear();
        laneAddrVec.clear();
        ldLaneStateVec.clear();
        loadIds.clear();
        storeIds.clear();
        type = MtcTAWriteType::TA_WRITE_END;
        stMergeData.clear();
        laneAddrResl.clear();
        std::memset(&addrPmuCnt, 0, sizeof(addrPmuCnt));
    }
    void SetMemReq(MemRequest &req)
    {
        valid = true;
        this->req = req;
    }

    void SetAllocCycle(uint32_t cycle)
    {
        alloc_cycle = cycle;
    }

    void SetEraseCycle(uint32_t cycle)
    {
        erase_cycle = cycle;
    }

    uint32_t GetAllocCycle()
    {
        return alloc_cycle;
    }

    uint32_t GetEraseCycle()
    {
        return erase_cycle;
    }

    void Reset()
    {
        valid = false;
        status = MtcTAReqStatus::INVALID;
        stage = MtcTAStage::INVALID;
        src = 0;
        size = 0;
        way = 0;
        dependId = static_cast<uint32_t>(-1);
        writeCyc = 0;
        alignAddrs.clear();
        laneAddrVec.clear();
        ldLaneStateVec.clear();
        loadIds.clear();
        storeIds.clear();
        type = MtcTAWriteType::TA_WRITE_END;
        req = MemRequest();
        stMergeData.clear();
        laneAddrResl.clear();
        std::memset(&addrPmuCnt, 0, sizeof(addrPmuCnt));
    }
    bool operator<(const MtcTARequest& other) const
    {
        return LessROBID(other.req.rid, req.rid);
    }
    bool operator>(const MtcTARequest& other) const
    {
        return LessROBID(req.rid, other.req.rid);
    }

    std::string GetRemainAddrs()
    {
        std::stringstream ss;
        
        ss << "AlignAddrs[";
        for (size_t i = 0; i < alignAddrs.size(); ++i) {
            if (i > 0) {
                ss << ", ";
            }
            const auto& info = alignAddrs[i];
            if (info.valid) {
                ss << "0x" << std::hex << info.addr;
            } else {
                ss << "INVALID";
            }
        }
        ss << "]";
        
        return ss.str();
    }

    MtcTARequest() : valid(false), status(MtcTAReqStatus::INVALID),
                     stage(MtcTAStage::INVALID),
                     src(0), size(0), way(0),
                     dependId(static_cast<uint32_t>(-1)), writeCyc(0), type(MtcTAWriteType::TA_WRITE_END) {}
    MtcTARequest(uint32_t src, size_t way,
        uint32_t size, uint32_t dependId) : src(src), size(size), way(way),
        dependId(dependId), writeCyc(0), type(MtcTAWriteType::TA_WRITE_END) {}
};

struct MtcTAEntry {
    bool valid;  // 有效位
    uint32_t tag;
    std::vector<uint8_t> data; // 256B
    MtcTAEntryState state;
    uint64_t wMask;
    ROBID bid;
    ROBID gid;
    uint32_t aaccelss_time;  // 访问时间（LRU）
    uint64_t addr;
    uint32_t hitCnt;

    MtcTAEntry() = delete;
    explicit MtcTAEntry(uint64_t entrySize) : valid(false), tag(0), data(entrySize, 0), state(MtcTAEntryState::INVALID),
        wMask(0), bid(), gid(), aaccelss_time(0), addr(0), hitCnt(0) {}
};

std::ostream& operator<<(std::ostream& out, const struct MtcTAEntry& entry);

class MtcLaneDataTransfer {
public:
    MtcLaneDataTransfer(MtcLaneAddr& load, const MtcLaneAddr& store)
        : load_(load), store_(store) {}
    
    // 检查两个LaneAddr是否有效
    bool areValid() const
    {
        return load_.isValid() && store_.isValid();
    }
    
    // 判断store是否完全覆盖load
    bool isStoreFullyCoverLoad() const
    {
        if (!areValid()) return false;
        
        return (store_.addr <= load_.addr) &&
               (store_.addr + store_.bytes >= load_.addr + load_.bytes);
    }
    
    // 判断是否有任何重叠
    bool hasOverlap() const
    {
        if (!areValid()) return false;
        
        return !(load_.addr + load_.bytes <= store_.addr ||
                store_.addr + store_.bytes <= load_.addr);
    }
    
    struct OverlapInfo {
        uint64_t overlap_start;
        uint64_t overlap_end;
        size_t overlap_size;
        size_t load_data_offset;
        size_t store_data_offset;
        bool has_overlap;
        bool is_valid;
    };
    
    OverlapInfo getOverlapInfo() const
    {
        OverlapInfo info;
        info.is_valid = areValid();
        info.has_overlap = false;
        
        if (!info.is_valid) {
            return info;
        }
        
        info.has_overlap = hasOverlap();
        if (!info.has_overlap) {
            return info;
        }
        
        // 计算重叠区域
        info.overlap_start = std::max(load_.addr, store_.addr);
        info.overlap_end = std::min(load_.addr + load_.bytes, store_.addr + store_.bytes);
        info.overlap_size = info.overlap_end - info.overlap_start;
        
        // 计算在各自data vector中的偏移
        info.load_data_offset = info.overlap_start - load_.addr;
        info.store_data_offset = info.overlap_start - store_.addr;
        
        return info;
    }
    
    // 从store拷贝数据到load（处理重叠区域）
    bool copyFromStoreToLoad()
    {
        auto info = getOverlapInfo();
        if (!info.is_valid) {
            return false;
        }
        if (!info.has_overlap || info.overlap_size == 0) {
            std::cerr << "No overlap between load and store regions" << std::endl;
            return false;
        }
        // 检查偏移量是否在有效范围内
        if (info.load_data_offset + info.overlap_size > load_.data.size() ||
            info.store_data_offset + info.overlap_size > store_.data.size()) {
            std::cerr << "Overlap calculation error: offset out of bounds" << std::endl;
            return false;
        }
        // copy 数据
        std::memcpy(load_.data.data() + info.load_data_offset,
            store_.data.data() + info.store_data_offset, info.overlap_size);
        std::cout << "Copied " << info.overlap_size << " bytes from store to load" << std::endl;
        return true;
    }
    
    // 打印
    void printCoverageInfo() const
    {
        if (!areValid()) {
            return;
        }
        
        std::cout << "Load: addr=0x" << std::hex << load_.addr
                  << ", bytes=" << std::dec << load_.bytes << std::endl;
        std::cout << "Store: addr=0x" << std::hex << store_.addr
                  << ", bytes=" << std::dec << store_.bytes << std::endl;
        
        if (isStoreFullyCoverLoad()) {
            std::cout << "Store fully covers load" << std::endl;
        } else if (hasOverlap()) {
            auto info = getOverlapInfo();
            std::cout << "Partial overlap: " << info.overlap_size << " bytes" << std::endl;
            std::cout << "Overlap region: 0x" << std::hex << info.overlap_start
                      << " - 0x" << info.overlap_end << std::dec << std::endl;
        } else {
            std::cout << "No overlap" << std::endl;
        }
    }
private:
    MtcLaneAddr& load_;
    const MtcLaneAddr& store_;
};

// 打印LaneAddr内容
inline void printLaneAddr(const MtcLaneAddr& lane, const std::string& name)
{
    std::cout << name << ": addr=0x" << std::hex << lane.addr
              << ", bytes=" << std::dec << lane.bytes << std::endl;
    std::cout << "Data: ";
    for (size_t i = 0; i < std::min(static_cast<size_t>(10), lane.data.size()); ++i) {
        std::cout << static_cast<int>(lane.data[i]) << " ";
    }
    if (lane.data.size() > 10) {
        std::cout << "...";
    }
    std::cout << std::endl;
}

class MtcMissRequestQueue {
public:
    struct MtcMissRequestEntry {
        uint64_t address;
        uint32_t entryId;
        uint64_t requestId;
        std::vector<uint32_t> mergeEntryId;
        
        MtcMissRequestEntry(uint64_t addr, uint32_t eid, uint64_t rid);
    };

    explicit MtcMissRequestQueue(size_t depth, bool verbose);
    
    MtcMissRequestQueue(const MtcMissRequestQueue&) = delete;
    MtcMissRequestQueue& operator=(const MtcMissRequestQueue&) = delete;

    void shift(uint32_t _entryId);
    bool addMissRequest(uint64_t address, uint64_t requestId, uint32_t entryId);
    void addMergeEntryId(uint64_t address, uint32_t entryId);
    void removeOnFill(uint64_t requestId);
    bool findReqByAddress(uint64_t address, uint32_t camEid, uint32_t &eid, uint64_t &reqId);
    void GetMissReqEntryId(uint64_t reqId, uint32_t &eid, uint32_t &addr);
    void GetMergeReqEntryId(uint64_t reqId, std::vector<uint32_t> &eids);
    size_t getCurrentSize() const;
    size_t getMaxDepth() const;
    bool isEmpty() const;
    bool isFull() const;
    void clear();
    void printQueueStatus() const;

private:
    std::unordered_map<uint64_t, std::shared_ptr<MtcMissRequestEntry>> addressMap_;
    
    size_t maxDepth_;
    uint32_t nextEntryId_;
    bool verbose_ = false;
};

class MemoryCoreTA : public RingNodeObj {
public:
    // 从 vector core 来的请求写这两个 Q
    INPUT SimQueue<MemRequest>          loadQ; // TODO 改成loadInstQ
    INPUT SimQueue<MemRequest>          storeQ; // TODO 改成storeInstQ

    // 从 Tile Register 返回的请求
    INNER SimQueue<TileRegVecLdRetry>  *tileRegLdRetryQ;
    INNER SimQueue<TileRegVecStRetry>  *tileRegStRetryQ;
    INNER SimQueue<TileRegVecLdRes>    *tileRegLdResQ;
    INNER SimQueue<TileRegVecLdRes>    *tileRegWrRspQ;

    // 输出到 TileRegister 的发送请求
    INNER SimQueue<VecTileRegLdReq>   *tileRegLdReqQ;
    INNER SimQueue<VecTileRegStReq>   *tileRegStReqQ;

    // 输出到 vector core 的返回 Q
    // 连接到 m_memReqQ
    OUTPUT SimQueue<MemRequest>*       outputQ;

    INPUT SimQueue<ScbDrainBus>*       drainScbDataQ;
    OUTPUT SimQueue<ScbDrainBus>*      drainScbRespQ;

    INPUT bool*                        resolveSignal;

public:
    MemoryCoreTA();
    virtual ~MemoryCoreTA();
    SimSys *GetSim();
    uint64_t tileReqId = 0;
    void ReportStat() override {}

    void Build(MemoryCore *top, uint64_t set, uint64_t way, uint64_t nEntry, uint64_t entrySize, uint64_t loadQSize,
        uint64_t storeQSize, bool use_tile_cache);
    void Work();
    void Xfer();
    void Reset();
    void Dump();

    // 将前面的请求读入(IEX Pipe 在 E1 写入到 Q 中)
    void ReadLdReq();
    void ReadStReq();
    // pipeline
    void I2();
    void E1();
    void E2();
    void E3();
    void E4();
    void LoadHandleE2(int _index);
    void StoreHandleE2(int _index);
    void DrainEmpty();

    // TBuffer
    void decodeAddress(uint32_t address, uint32_t& tag, uint32_t& index, uint32_t& offset);
    bool IsTBufferHit(const MtcLaneAddr& lane, std::vector<uint8_t>& result);
    uint64_t GetHitWay(uint32_t address);
    void GetHitInfo(uint32_t address, uint64_t &set, uint64_t &way);
    void UpdateLRU(uint64_t set, uint64_t way);
    void printData(const uint8_t* data, size_t length);
    void FillData(uint32_t address, TileRegVecLdRes req);
    uint64_t findLRUWay(uint64_t set);
    void ResetEntry(uint64_t set, uint64_t way);
    void DrainAllEntry();
    void DumpTBufferState();

    // load queue
    void AllocLoadQueue(MemRequest &req, uint32_t tid);
    uint32_t GetLdqFreeId(uint32_t _tid);
    void ResolveLdqEntry(uint32_t id);
    void DeallocateLdqEntry(uint32_t id);
    void WakeupLoadQueue(TileRegVecLdRes req);
    void FillWakeMissLoad(uint32_t id, TileRegVecLdRes req);
    void FillWakeMergeLoad(uint32_t id, TileRegVecLdRes req);
    void DumpLoadQueue();
    void StatsCount();
    bool IsBusy();

    // store queue
    void AllocStoreQueue(MemRequest &req, uint32_t tid);
    uint32_t GetStqFreeId(uint32_t _tid);
    std::vector<uint8_t> LoadCamStq(uint32_t bid, uint32_t gid, uint32_t rid, uint64_t addr, unsigned size, bool& hit);
    void WakeupStoreQueue(uint64_t reqId);
    void StoreCamLoadQueue(const MtcTARequest& entry);
    std::vector<MtcTARequest> SortStqByRID(uint32_t gid);
    void ResolveStqEntry(uint32_t id);
    void DeallocateStqEntry(uint32_t id);
    void RetireStore();
    void DumpStoreQueue();
    bool CheckEmptyByBid(ROBID &bid);

    bool IsLdqFull(uint32_t tid);
    bool IsStqFull(uint32_t tid);
    void LdQShift();
    void StQShift();

    uint32_t StoreArbitration();
    uint32_t LoadArbitration();

    // ooo interface
    void ReceiveNonFlush();
    void processLoadNonFlush();
    void processStoreNonFlush();

    // tile register interface
    void SendTRReq();
    void ReceiveTRData();
    void ReceiveTRRsp();
    void ReceiveTlsData();
    uint32_t CreateTileWriteReq(unsigned entryId, uint64_t addr, const std::vector<uint8_t>& data,
                                uint64_t size, DataMask mask);
    uint32_t CreateTileRegLdReq(uint64_t addr, uint32_t eId, uint64_t size = 256);
    uint32_t CreateTileRegStReq(uint64_t addr, uint64_t size);
    bool LoadToRing(VecTileRegLdReq req);
    bool StoreToRing(VecTileRegStReq req, DataMask mask);
    void LoadFromRing();
    void StoreFromRing();

    // used to check store can be issued
    bool IsOldestRID(const ROBID& rid);
    bool IsOldestRID(const ROBID& gid, const ROBID& rid);

    void HandleTileRegLdRetry(TileRegVecLdRetry retry);
    void HandleTileRegStRetry(TileRegVecStRetry retry);

    void SetScbDrainState(uint32_t bid);
    void Resolve();
    void Flush(FlushBus &bus);

    const char* EnumToString(MtcTAStage stage)
    {
        switch (stage) {
            case MtcTAStage::INVALID: return "INVALID";
            case MtcTAStage::STAGE_E1: return "STAGE_E1";
            case MtcTAStage::STAGE_E2: return "STAGE_E2";
            case MtcTAStage::STAGE_E3: return "STAGE_E3";
            default: return "UNKNOWN";
        }
    }

    const char* EnumToString(MtcTAReqStatus status)
    {
        switch (status) {
            case MtcTAReqStatus::INVALID: return "INVALID";
            case MtcTAReqStatus::READY: return "READY";
            case MtcTAReqStatus::PENDING: return "PENDING";
            case MtcTAReqStatus::SENT: return "SENT";
            case MtcTAReqStatus::RETRY: return "RETRY";
            case MtcTAReqStatus::WAIT_OLDEST: return "WAIT_OLDEST";
            case MtcTAReqStatus::WAIT_RSP: return "WAIT_RSP";
            case MtcTAReqStatus::WAIT_RSV: return "WAIT_RSV";
            case MtcTAReqStatus::RETIRE_SCB: return "RETIRE_SCB";
            case MtcTAReqStatus::WAIT_NUKE: return "WAIT_NUKE";
            case MtcTAReqStatus::SUACCELSS: return "SUACCELSS";
            default: return "UNKNOWN";
        }
    }

    std::shared_ptr<TileSCB> scb;
    
private:
    // [way, nEntry]
    std::vector<std::vector<MtcTAEntry>>      tBuffer; // Vector Tile Temporal Buffer (TBuffer)，tBuffer[set_index][way]

    // 用来区分不同 store 的优先级，按序遍历就是优先级顺序，具体在 build 的地方
    std::vector<SimQueue<VecTileRegStReq>>      writeBackQ;
    std::shared_ptr<MtcMissRequestQueue> loadMissQueue;

    uint64_t threadNum = 1;
    uint64_t nSets = 0;
    uint64_t nWays = 0;
    uint64_t nEntries = 0;
    uint32_t cacheOffsetBits = 0;
    uint32_t cacheIndexBits = 0;
    uint32_t cacheTagBits = 0;
    uint64_t addrBitWidth = 0;
    uint64_t entrySizes = 0;
    uint64_t tagMask = 0;
    uint64_t idxMask = 0;
    uint64_t addrMask = 0;

    // Load/store 带宽限制(最多能发 Tile 几个请求)
    int ldBand = 0;
    int ldInPipeCnt = 0;
    int stBand = 0;
    
    // LOAD Arbitration
    int arbTid = 0;

    bool use_tile_cache = false;

    MemoryCore *vtop;
    INNER std::vector<MtcTARequest>      loadQueue;
    INNER std::vector<MtcTARequest>      storeQueue;

    uint32_t global_time = 0;
    bool startDrainScb = false;
    ROBID drainBlockId;

    std::vector<uint32_t> ldqEntryOccCnt;
    std::vector<uint32_t> stqEntryOccCnt;

    MemoryCoreTAStats stat;
};

} // namespace JCore

#endif
