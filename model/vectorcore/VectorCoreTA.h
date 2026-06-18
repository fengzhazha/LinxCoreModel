#ifndef VECTOR_CORE_TA_H
#define VECTOR_CORE_TA_H

#include <memory>
#include <unordered_set>
#include <vector>
#include <deque>

#include "core/Bus.h"
#include "interface/InterfaceCommon.h"
#include "tmu/ring/RingNodeObj.h"
#include "VecSCB.h"
#include "vectorcore/VectorCoreTAStats.h"

namespace JCore {

class VectorCore;
class VecRequest;

/*
 * FIXME: impl detailed vector core lsu pipeline
 */
enum class TAEntryState {
    INVALID = 0,
    SHARED = 1,
    MODIFIED = 2,
};

// 优先级从高到低
enum class TAWriteType {
    EVICT,
    READ_FILL,
    LDQ_REPICK,
    LOAD,
    STORE,

    TA_WRITE_END
};

enum class TAReadType {
    LD_REPICK,
    LD,
};

enum class VecTAReqStatus {
    INVALID,
    READY,
    // 等待被发出
    PENDING,
    // 已经发出
    SENT,
    // 收到 RETRY 信号
    RETRY,
    WAIT_CREDIT,
    WAIT_WAKEUP,
    WAIT_OLDEST,
    WAIT_RSP,
    WAIT_RSV,
    RETIRE_SCB,
    WAIT_NUKE,
    SEND_REQ_TO_TMA,        // VSCATTER: 已发送VSCATTER请求到TMA
    RECEIVE_DBID_DONE,      // VSCATTER: 收到DBID响应，准备发送地址tiles
    SENDING_ADDR_TILES,     // VSCATTER: 正在发送地址tiles（中间状态）
    SEND_ADDR_TILE_DONE,    // VSCATTER: 所有地址tiles发送完成，准备发送数据tiles
    SENDING_DATA_TILES,     // VSCATTER: 正在发送数据tiles（中间状态）
    SEND_DATA_TILE_DONE,    // VSCATTER: 所有数据tiles发送完成
    SUACCELSS
};

enum class VecTAStage {
    INVALID,
    STAGE_E1,
    STAGE_E2,
    STAGE_E3
};

struct LaneAddr {
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
    uint32_t stid = -1U;

    bool isValid() const
    {
        return data.size() == bytes;
    }

    LaneAddr(unsigned lane_id, uint64_t base_addr, uint64_t aligned_address, unsigned byte_count, uint32_t id)
        : sended{false}, laneId{lane_id}, addr{base_addr}, alignAddr{aligned_address}, bytes{byte_count}, data(0, 0),
        stid(id)
        {
        dataFromTile = false;
        dataFromStq = false;
        hitStqRid = 0;
        }
};

struct StMergeEntry {
    // addr should be align
    uint64_t addr;
    uint64_t reqId;
    std::vector<uint8_t> data;
    bool ready = false;

    StMergeEntry(uint64_t addr, uint64_t reqId, const std::vector<uint8_t>& data, bool ready = false) : addr(addr),
                                                                                                        reqId(reqId),
                                                                                                        data(data),
                                                                                                        ready(ready) {};
};

enum class AddrState {
    INVALID,
    HIT_CACHE,                    // hit tile cache
    MISS_SEND_REQUEST,            // tile cache miss && 请求发送
    MISS_WAIT_SEND,               // tile cache miss && 等待请求发送
    MISS_HIT_OTHER_REQUEST        // tile cache miss && hit 已经发送的miss请求
};

struct VecAlignReqInfo {
    bool valid = false;
    uint64_t addr = 0;
    AddrState stateFSM = AddrState::INVALID;
    uint32_t e1Cycle = 0;
    uint32_t hitCycle = 0;
    uint32_t missReqCycle = 0;
    uint32_t missFillCycle = 0;
    int missReqEntryId;
    uint32_t missReqId;
    uint32_t stid = -1U;
    VecAlignReqInfo(uint64_t a, uint32_t id) : valid(true), addr(a), stid(id) {}
    bool WaitMissFill() const
    {
        return stateFSM == AddrState::MISS_SEND_REQUEST || stateFSM == AddrState::MISS_HIT_OTHER_REQUEST;
    }
};

struct AddrCounter {
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

struct VecTARequest {
    bool valid = false;
    bool isVgather = false;
    bool isVscatter = false;
    uint32_t groupSlotId = 0;
    uint32_t bpqID = 0;   // VSCATTER: TMA BPQ entry ID (packed: bpqID1 in low 16 bits, bpqID2 in high 16 bits)
    uint16_t bpqID1 = 0;  // Unpacked bpqID1 (for first data tile)
    uint16_t bpqID2 = 0;  // Unpacked bpqID2 (for second data tile, valid only for 64-bit data)
    bool hasDualBPQ = false;  // Flag indicating dual BPQ entries (64-bit VSCATTER)
    uint32_t alloc_cycle = 0;
    uint32_t erase_cycle = 0;
    uint32_t entryId = 0;
    VecTAReqStatus status = VecTAReqStatus::INVALID;
    VecTAStage stage = VecTAStage::INVALID;
    uint32_t src = 0;
    uint32_t size = 0;
    // 返回的数据填充到哪个 way
    uint32_t way = 0;
    // evict 的时候需要用到这个
    // 只有当 evict 写回完成才可以继续
    uint32_t dependId = 0;
    // 记录 store 发出的 cycle，来定期做清除 id
    uint64_t writeCyc = 0;
    AddrCounter addrPmuCnt;

    // VSCATTER: tile 发送计数
    uint32_t totalAddrTiles = 0;   // 需要发送的地址tile总数（根据位宽决定：32位=1, 64位=2）
    uint32_t addrTilesSent = 0;    // 已发送的地址 tile 数量
    uint32_t totalDataTiles = 0;   // 需要发送的数据tile总数（根据位宽决定：32位=1, 64位=2）
    uint32_t dataTilesSent = 0;    // 已发送的数据 tile 数量

    TAWriteType type;
    MemRequest req;
    std::vector<VecAlignReqInfo> alignAddrs; // align 256B address
    std::vector<LaneAddr> laneAddrVec; // 64 lane addr, bytes
    std::vector<StMergeEntry> stMergeData;
    std::vector<LaneAddr> laneAddrResl;
    std::vector<LaneAddr> ldLaneStateVec; // 64 lane state
    std::unordered_map<uint32_t, uint64_t>  loadIds; // for miss, reqId - addr
    std::unordered_map<uint32_t, uint64_t>  storeIds; // for miss, reqId - addr
    unsigned oriAddrsCnt = 0;
    unsigned checkAddrCnt = 0;

    void Init(uint32_t index)
    {
        valid = false;
        isVgather = false;
        isVscatter = false;
        entryId = index;
        status = VecTAReqStatus::INVALID;
        stage = VecTAStage::INVALID;
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
        oriAddrsCnt = 0;
        checkAddrCnt = 0;
        type = TAWriteType::TA_WRITE_END;
        stMergeData.clear();
        laneAddrResl.clear();
        std::memset(&addrPmuCnt, 0, sizeof(addrPmuCnt));
        totalAddrTiles = 0;
        addrTilesSent = 0;
        totalDataTiles = 0;
        dataTilesSent = 0;
    }
    void SetMemReq(MemRequest &req)
    {
        valid = true;
        this->req = req;
    }

    void SetVgather()
    {
        isVgather = true;
    }

    bool GetVgather() const
    {
        return isVgather;
    }

    void SetVscatter()
    {
        isVscatter = true;
    }

    bool GetVscatter() const
    {
        return isVscatter;
    }

    void SetGroupSlotId(uint32_t tmp)
    {
        groupSlotId = tmp;
    }

    uint32_t GetGroupSlotId() const
    {
        return groupSlotId;
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

    // VSCATTER: 检查是否所有地址tiles已发送
    bool IsAddrTilesSent() const
    {
        return (totalAddrTiles > 0 && addrTilesSent == totalAddrTiles);
    }

    // VSCATTER: 检查是否所有数据tiles已发送
    bool IsDataTilesSent() const
    {
        return (totalDataTiles > 0 && dataTilesSent == totalDataTiles);
    }

    void Reset()
    {
        valid = false;
        status = VecTAReqStatus::INVALID;
        stage = VecTAStage::INVALID;
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
        isVgather = false;
        isVscatter = false;
        groupSlotId = 0;
        oriAddrsCnt = 0;
        checkAddrCnt = 0;
        type = TAWriteType::TA_WRITE_END;
        req = MemRequest();
        stMergeData.clear();
        laneAddrResl.clear();
        std::memset(&addrPmuCnt, 0, sizeof(addrPmuCnt));
        totalAddrTiles = 0;
        addrTilesSent = 0;
        totalDataTiles = 0;
        dataTilesSent = 0;
    }

    bool operator<(const VecTARequest& other) const
    {
        return LessROBID(other.req.rid, req.rid);
    }
    bool operator>(const VecTARequest& other) const
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

    VecTARequest() : valid(false), status(VecTAReqStatus::INVALID),
                     stage(VecTAStage::INVALID),
                     src(0), size(0), way(0),
                     dependId(static_cast<uint32_t>(-1)), writeCyc(0), type(TAWriteType::TA_WRITE_END) {}
    VecTARequest(uint32_t src, size_t way,
                 uint32_t size, uint32_t dependId) : src(src), size(size), way(way),
                                                     dependId(dependId), writeCyc(0), type(TAWriteType::TA_WRITE_END) {}
};

struct TAEntry {
    bool valid;  // 有效位
    uint32_t tag;
    std::vector<uint8_t> data; // 256B
    TAEntryState state;
    uint64_t wMask;
    ROBID bid;
    uint32_t stid = -1U;
    ROBID gid;
    uint32_t aaccelss_time;  // 访问时间（LRU）
    bool isStash;
    uint64_t addr;
    uint32_t hitCnt;

    void Reset()
    {
        valid = false;
        tag = 0;
        state = TAEntryState::INVALID;
        wMask = 0;
        aaccelss_time = 0;
        isStash = false;
        addr = 0;
        bid.wrap = false;
        bid.val = 0;
        gid.wrap = false;
        gid.val = 0;
        hitCnt = 0;
    }

    TAEntry() = delete;
    explicit TAEntry(uint64_t entrySize) : valid(false), tag(0), data(entrySize, 0), state(TAEntryState::INVALID),
        wMask(0), bid(), gid(), aaccelss_time(0), isStash(false), addr(0), hitCnt(0) {}
};

struct StashFillInfo {
    uint32_t addr;
    uint32_t fillCycle = 0;
    bool  hit;
    bool notHit;
    StashFillInfo(uint32_t a, uint32_t cycle)
        : addr(a), fillCycle(cycle), hit(false), notHit(false) {}
};

std::ostream& operator<<(std::ostream& out, const struct TAEntry& entry);

class LaneDataTransfer {
public:
    LaneDataTransfer(LaneAddr& load, const LaneAddr& store)
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
    std::string PrintCoverageInfo() const;
private:
    LaneAddr& load_;
    const LaneAddr& store_;
};

// 打印LaneAddr内容
inline void printLaneAddr(const LaneAddr& lane, const std::string& name)
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

class MissRequestQueue {
public:
    struct MissRequestEntry {
        uint64_t address;
        uint32_t entryId;
        uint64_t requestId;
        std::vector<uint32_t> mergeEntryId;

        MissRequestEntry(uint64_t addr, uint32_t eid, uint64_t rid);
    };

    explicit MissRequestQueue(size_t depth, uint32_t scalarCnt);

    MissRequestQueue(const MissRequestQueue&) = delete;
    MissRequestQueue& operator=(const MissRequestQueue&) = delete;

    bool addMissRequest(uint64_t address, uint32_t stid, uint64_t requestId, uint32_t entryId);
    void addMergeEntryId(uint64_t address, uint32_t stid, uint32_t entryId);
    void removeOnFill(uint64_t requestId, uint32_t stid);
    bool FindReqByAddress(uint64_t address, uint32_t stid, uint32_t camEid, uint32_t &eid, uint64_t &reqId);
    bool FindReqByAddress(uint64_t address, uint32_t stid);
    bool PrefetchCam(uint64_t address, uint32_t stid);
    void GetMissReqEntryId(uint64_t reqId, uint32_t stid, uint32_t &eid, uint32_t &addr);
    void GetMergeReqEntryId(uint64_t reqId, uint32_t stid, std::vector<uint32_t> &eids);
    size_t getCurrentSize(uint32_t stid) const;
    size_t getMaxDepth() const;
    bool isEmpty(uint32_t stid) const;
    bool isFull(uint32_t stid) const;
    void clear(uint32_t stid);
    void printQueueStatus() const;
    void Dump() const
    {
        std::cout << "======= DUMP MISS QUEUE =======" << std::endl;
        for (uint32_t stid = 0; stid < addressMap_.size(); ++stid) {
            auto &addrMp = addressMap_[stid];
            std::cout << std::dec << "STID: " << stid << std::endl;
            for (auto &e : addrMp) {
                std::cout << "addr: 0x" << std::hex << e.second->address << ", entryId: " << std::dec
                          << e.second->entryId << ", reqId: " << std::dec << e.second->requestId << std::endl;
                if (e.second->mergeEntryId.empty()) {
                    std::cout << "\tMergeId Empty" << std::endl;
                }
                for (auto &mergeId : e.second->mergeEntryId) {
                    std::cout << "\t" << mergeId << std::endl;
                }
            }
        }
        std::cout << "====== END DUMP MISS QUEUE =====" << std::endl;
    }

private:
    std::vector<std::unordered_map<uint64_t, std::shared_ptr<MissRequestEntry>>> addressMap_;

    size_t maxDepth_;
    std::vector<uint32_t> nextEntryId_;
};

class VectorCoreTA : public RingNodeObj {
public:
    uint32_t resolveCnt = 0;
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
    OUTPUT SimQueue<MemRequest>*       ldOutputQ;
    OUTPUT SimQueue<MemRequest>*       stOutputQ;

    INPUT std::vector<SimQueue<ScbDrainBus>*>       drainScbDataQ;
    OUTPUT std::vector<SimQueue<ScbDrainBus>*>      drainScbRespQ;
    OUTPUT SimQueue<ScbDrainBus>*                   storeDataCompRespQ;

    OUTPUT std::shared_ptr<SimQueue<MemRequest>>    m_vecBridgeMemReqQ;
    INPUT  std::shared_ptr<SimQueue<MemRequest>>    m_bridgeVecLdRetQ;
    INPUT  std::shared_ptr<SimQueue<MemRequest>>    m_bridgeVecStRslvQ;

    INPUT bool*                        resolveSignal;

    std::vector<uint64_t>              stQ_credit;
    uint64_t                           ldQ_credit;

    std::vector<std::vector<uint64_t>>              prefetchReqQ;
    std::vector<std::vector<uint64_t>>              hprefetchReqQ;
    std::shared_ptr<MissRequestQueue>  prefetchMissQueue;

public:
    VectorCoreTA();
    virtual ~VectorCoreTA();
    SimSys *GetSim();
    uint64_t tileReqId = 0;
    void ReportStat() override {}

    void Build(VectorCore *top, uint64_t set, uint64_t way, uint64_t entrySize, uint64_t loadQSize,
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
    void DrainEmpty(uint32_t stid);
    //stash mode
    void ReceiveStashReq(uint32_t addr, TileRegVecLdRes req);
    uint64_t GetBlockWay(uint32_t address, bool &hitTO);
    void SendStashResp();
    // TBuffer
    void decodeAddress(uint32_t address, uint32_t& tag, uint32_t& index, uint32_t& offset);
    bool IsTBufferHit(const LaneAddr& lane, std::vector<uint8_t>& result, bool& hitStash);
    bool IsNormalCacheHit(uint64_t addr);
    bool IsNormalCacheHit(const LaneAddr& lane, std::vector<uint8_t>& result, bool& hitStash);
    uint64_t GetHitWay(uint32_t address);
    void GetHitInfo(uint32_t address, uint64_t &set, uint64_t &way);
    void UpdateLRU(uint64_t set, uint64_t way);
    void printData(const uint8_t* data, size_t length);
    void FillData(uint32_t address, TileRegVecLdRes req);
    uint64_t findLRUWay(uint64_t set);
    void ResetEntry(uint64_t set, uint64_t way);
    void DrainAllEntry();
    void DumpTBufferState();
    void WriteStashReq(uint64_t addr, uint32_t stid);
    void WriteHeadStashReq(uint64_t addr, uint32_t stid);
    void Prefetch(uint32_t stid);
    void ProcessPrefetchQ(std::vector<uint64_t> &reqQ, uint32_t stid);
    void TrainPf(uint64_t addr, uint64_t stride, ROBID bid, uint32_t stid);
    void TrainHPf(uint64_t addr, uint64_t stride, ROBID bid, uint32_t stid);
    void PrefetchFillWakeMissLoad(uint32_t id);
    void WakeupPrefetchQueue(TileRegVecLdRes req);
    void DrainTBuffer(ROBID &bid, uint32_t stid);
    bool PrefetchCamCache(uint32_t addr, uint32_t stid);

    // load queue
    void AllocLoadQueue(MemRequest &req);
    uint32_t GetLdqFreeId();
    void ResolveLdqEntry(uint32_t id, uint32_t stid);
    void DeallocateLdqEntry(uint32_t id);
    void WakeupLoadQueue(TileRegVecLdRes req);
    void FillWakeMissLoad(uint32_t id, TileRegVecLdRes req);
    void FillWakeMergeLoad(uint32_t id, TileRegVecLdRes req);
    void DumpLoadQueue();
    void StatsCount();
    bool IsBusy();

    // PF queue
    bool IsInHPfQ(uint64_t addr, uint32_t stid);
    void EraseAddrHPfQ(uint64_t addr, uint32_t stid);
    bool IsInPfQ(uint64_t addr, uint32_t stid);
    void EraseAddrPfQ(uint64_t addr, uint32_t stid);

    // store queue
    void AllocStoreQueue(MemRequest &req, uint32_t tid);
    uint32_t GetStqFreeId(uint32_t _tid);
    std::vector<uint8_t> LoadCamStq(uint32_t bid, uint32_t gid, uint32_t rid, uint64_t addr, unsigned size, bool& hit,
        uint32_t stid);
    void WakeupStoreQueue(uint64_t reqId);
    void StoreCamLoadQueue(const VecTARequest& entry);
    std::vector<VecTARequest> SortStqByRID(uint32_t gid);
    void ResolveStqEntry(uint32_t id, uint32_t stid);
    void DeallocateStqEntry(uint32_t id);
    void RetireStore();
    void AppendToRetireEntry(const VecTARequest& entry);
    void DumpStoreQueue();
    bool CheckEmptyByBid(ROBID &bid, uint32_t stid);

    bool IsLdqFull();
    bool IsStqFull(uint32_t tid);

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
    uint32_t CreateTileWriteReq(unsigned entryId, uint64_t addr, const std::vector<uint8_t>& data,
                                uint64_t size, DataMask mask, uint32_t stid);
    uint32_t CreateTileRegLdReq(uint64_t addr, uint32_t eId, uint32_t stid, uint64_t size = 256);
    uint32_t CreateTileRegStReq(uint64_t addr, uint64_t size);
    bool LoadToRing(VecTileRegLdReq req) const;
    bool StoreToRing(VecTileRegStReq req, DataMask mask);
    void LoadFromRing();
    void StoreFromRing();
    void ProcessDbidResponse();  // VSCATTER: 处理 DBID 响应
    void ProcessVscatterTileSending();  // VSCATTER: 处理 tile 发送状态转换
    void SendVscatterAddrTiles(uint16_t bpqID1, uint32_t txnId);  // VSCATTER: 发送单个地址Tile
    void SendVscatterDataTiles(uint32_t bpqID, uint32_t txnId);  // VSCATTER: 发送单个数据Tile
    void DoLoadWakeUp(MemRequest &req) const;

    // used to check store can be issued
    bool IsOldestRID(const ROBID& rid);
    bool IsOldestRID(const ROBID& gid, const ROBID& rid);

    void HandleTileRegLdRetry(TileRegVecLdRetry retry);
    void HandleTileRegStRetry(TileRegVecStRetry retry);

    void SetScbDrainState(uint32_t bid);
    void Resolve();
    void Flush(FlushBus &bus);
    bool CheckLoadQEmpty() const
    {
        if (ldqEntryOccCnt != 0) {
            return false;
        } else {
            return true;
        }
    }

    /* Bridge interface */
    void ReceiveBridgeResp();

    const char* EnumToString(VecTAStage stage)
    {
        switch (stage) {
            case VecTAStage::INVALID: return "INVALID";
            case VecTAStage::STAGE_E1: return "STAGE_E1";
            case VecTAStage::STAGE_E2: return "STAGE_E2";
            case VecTAStage::STAGE_E3: return "STAGE_E3";
            default: return "UNKNOWN";
        }
    }

    const char* EnumToString(VecTAReqStatus status)
    {
        switch (status) {
            case VecTAReqStatus::INVALID: return "INVALID";
            case VecTAReqStatus::READY: return "READY";
            case VecTAReqStatus::PENDING: return "PENDING";
            case VecTAReqStatus::SENT: return "SENT";
            case VecTAReqStatus::RETRY: return "RETRY";
            case VecTAReqStatus::WAIT_CREDIT: return "WAIT_CREDIT";
            case VecTAReqStatus::WAIT_WAKEUP: return "WAIT_WAKEUP";
            case VecTAReqStatus::WAIT_OLDEST: return "WAIT_OLDEST";
            case VecTAReqStatus::WAIT_RSP: return "WAIT_RSP";
            case VecTAReqStatus::WAIT_RSV: return "WAIT_RSV";
            case VecTAReqStatus::RETIRE_SCB: return "RETIRE_SCB";
            case VecTAReqStatus::WAIT_NUKE: return "WAIT_NUKE";
            case VecTAReqStatus::SEND_REQ_TO_TMA: return "SEND_REQ_TO_TMA";
            case VecTAReqStatus::RECEIVE_DBID_DONE: return "RECEIVE_DBID_DONE";
            case VecTAReqStatus::SENDING_ADDR_TILES: return "SENDING_ADDR_TILES";
            case VecTAReqStatus::SEND_ADDR_TILE_DONE: return "SEND_ADDR_TILE_DONE";
            case VecTAReqStatus::SENDING_DATA_TILES: return "SENDING_DATA_TILES";
            case VecTAReqStatus::SEND_DATA_TILE_DONE: return "SEND_DATA_TILE_DONE";
            case VecTAReqStatus::SUACCELSS: return "SUACCELSS";
            default: return "UNKNOWN";
        }
    }
private:
    unsigned threadNum = 1;
    // [way, nEntry]
    std::vector<std::vector<TAEntry>>      tBuffer; // Vector Tile Temporal Buffer (TBuffer)， tBuffer[set_index][way]
    std::vector<std::vector<TAEntry>>      inputCache;

    std::vector<SimQueue<VecTileRegStReq>>      writeBackQ;
    std::shared_ptr<MissRequestQueue> loadMissQueue;
    std::deque<std::shared_ptr<Request>> stashRespQueue;
    std::deque<ROBID> pendingEvict;
    uint64_t nSets = 0;
    uint64_t nWays = 0;
    uint64_t nTaSets = 0;
    uint64_t nTaWays = 0;
    uint32_t cacheOffsetBits = 0;
    uint32_t cacheIndexBits = 0;
    uint64_t entrySizes = 0;
    uint64_t addrMask = 0;

    // Load/store 带宽限制(最多能发 Tile 几个请求)
    int ldBand = 0;
    int ldInPipeCnt = 0;
    int stBand = 0;
    unsigned ldqPerThreSize = 0;
    unsigned stqPerThreSize = 0;

    // LOAD Arbitration
    int arbTid = 0;
    unsigned scbThreadPtr = 0;
    std::map<unsigned, SimQueue<VecTARequest>> retireScbMap_;

    bool use_tile_cache = false;

    VectorCore *vtop;
    INNER std::vector<VecTARequest>      loadQueue;
    INNER std::vector<VecTARequest>      storeQueue;

    uint32_t global_time = 0;
    bool startDrainScb = false;
    std::vector<ROBID> drainBlockId;
    // uint32_t drainStid;
    bool dumpCacheData = false;

    uint32_t ldqEntryOccCnt;
    std::vector<uint32_t> stqEntryOccCnt;

    std::shared_ptr<VecSCB> scb;
    VectorCoreTAStats stat;
};

} // namespace JCore

#endif
