#ifndef MODEL_BRIDGETABLE_TILEBRIDGE_H
#define MODEL_BRIDGETABLE_TILEBRIDGE_H

#include <vector>
#include <memory>
#include <string>
#include <queue>
#include <algorithm>
#include <type_traits>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include "SimSys.h"
#include "core/Bus.h"
#include "tmu/TileUtils.h"
#include "GFUSim.h"
#include "tmu/ring/RingNodeObj.h"

#include "interface/TTransTileRegStReq.h"
#include "interface/TTransTileRegLdReq.h"
#include "interface/TileRegTTransLdRes.h"
#include "../configs/tilebridge_config.h"
#include "tmu/ring/CrossStation.h"

namespace JCore {
namespace GFSIM {
namespace TileBridge {

constexpr uint64_t MAX_GLOBAL_MEMORY_CACHELINE_SIZE = 256;
constexpr uint64_t FRACTAL_ROW_BYTES = 32;
constexpr uint64_t FRACTAL_ROW_NUM = 16;
class BridgePairQ;
enum class MemoryUnit {
    GM,
    TR
};

enum class Direction {
    GM2TR,  /* TLOAD */
    TR2GM,  /* TSTORE */
    GM2GM,  /* No used for tile bridge */
    TR2TR   /* TMOV */
};

enum class ExecStatus {
    NO_START,
    WAIT_OTHER_BPQ,

    // for vscatter fake
    SEND_DBID_RSP,
    WAIT_D_A_VEC,

    GET_ADDR_TILE,
    WAIT_ADDR_TILE,
    SEND_READ,
    WAIT_READ_DATA,
    SEND_WRITE,
    WAIT_WRITE_RESOLVE,
    COMPLETE
};

enum class CacheAaccelssType {
    READ_TO_NWCB,   /* MGATHER Tag Hit */
    READ_TO_SWCB,   /* Tag Miss and Evict */
    WRITE_TO_BDB,   /* MSCATTER Tag Hit */
};

struct CacheAaccelssOp {
    CacheAaccelssType         type;
    std::vector<uint64_t>   bdbIds;
    uint64_t                bpqId;
    uint64_t                readReqId;

    uint64_t                readAddr;
    /* using for victim bdb */
    uint64_t                writeAddr;
};

template <typename K, typename V>
class OrderedMap {
public:
    // 插入（或更新）
    void Insert(const K& key, const V& value)
    {
        insertionOrder.remove(key);
        insertionOrder.push_back(key);
        map[key] = value;
    }

    // 返回第一个插入的键值对
    std::pair<K, V> Front()
    {
        if (insertionOrder.empty()) {
            throw std::runtime_error("LinkedHashMap is empty: front() called on empty map");
        }
        return std::make_pair(insertionOrder.front(), map.at(insertionOrder.front()));
    }

    // 查找（O(1)）
    bool Find(const K& key, V& out) const
    {
        auto it = map.find(key);
        if (it != map.end()) {
            out = it->second;
            return true;
        }
        return false;
    }

    // 删除
    void Erase(const K& key)
    {
        auto it = map.find(key);
        if (it != map.end()) {
            map.erase(it);
            insertionOrder.remove(key);
        }
    }

    // 是否为空
    bool Empty() const { return map.empty(); }

    // 大小
    size_t Size() const { return map.size(); }

    void Clear()
    {
        map.clear();
        insertionOrder.clear();
    }

private:
    std::unordered_map<K, V> map;           // 快速查找
    std::list<K> insertionOrder;            // 保持插入顺序
};

struct PMUDataBase {
    uint64_t tloadNum = 0;
    uint64_t tloadWriteNum = 0;
    uint64_t tloadWriteDataSize = 0;
    uint64_t tloadWriteValidDataSize = 0;
    std::map<uint32_t, uint64_t> tloadReadStat;

    uint64_t tstoreNum = 0;
    uint64_t tstoreReadNum = 0;
    uint64_t tstoreReadDataSize = 0;
    uint64_t tstoreWriteValidDataSize = 0;
    std::map<uint32_t, uint64_t> tstoreWriteStat;
    
    uint64_t tcNum = 0; // count for tmov
    uint64_t tcReadNum = 0;
    uint64_t tcWriteNum = 0;
    uint64_t tcReadDataSize = 0;
    uint64_t outstandingTloadNum = 0;
    uint64_t outstandingTstoreNum = 0;
    uint64_t outstandingTmovNum = 0;
    uint64_t outstandingMgatherNum = 0;
    uint64_t outstandingMscatterNum = 0;
    uint64_t outstandingVgatherNum = 0;
    uint64_t outstandingVscatterNum = 0;

    uint64_t mgatherNum = 0;
    uint64_t mgatherReadTileNum = 0;
    uint64_t mgatherReadTagReqNum = 0;
    uint64_t mgatherReadTagValidDataSize = 0;
    uint64_t mgatherSendReadMemReqNum = 0;
    uint64_t mgatherWriteNum = 0;
    uint64_t mgatherWriteSize = 0;

    uint64_t vgatherNum = 0;
    uint64_t vgatherReadTileNum = 0;
    uint64_t vgatherReadTagReqNum = 0;
    uint64_t vgatherSendReadMemReqNum = 0;
    uint64_t vgatherWriteNum = 0;

    uint64_t mscatterNum = 0;
    uint64_t mscatterReadTagReqNum = 0;
    uint64_t mscatterSendReadMemReqNum = 0;

    uint64_t vscatterNum = 0;
    uint64_t vscatterReadTagReqNum = 0;
    uint64_t vscatterSendReadMemReqNum = 0;

    uint64_t tagReqDuplicateHitInflightCnt = 0;
    uint64_t tagReqFirstHitThenMissCnt = 0;

    // TLOAD Top Down PMU Events
    uint64_t idleCycles = 0;
    uint64_t efficientCycles = 0;
    uint64_t startingCycles = 0;
    uint64_t latBoundCycles = 0; // Latency
    uint64_t ostBoundCycles = 0; // Outstanding
    uint64_t varBoundCycles = 0; // Variance (of SoC Read)
    uint64_t varOstBoundCycles = 0;
    uint64_t integCycles = 0;
    uint64_t egressBlockCycles = 0;
};

struct PMUData : public PMUDataBase {
    uint64_t bpqNonEmptryCycles = 0;
    uint64_t bpq75pCycles = 0;
    uint64_t bpqFullCycles = 0;

    uint64_t tlbdbNonEmptyCycles = 0;
    uint64_t tlbdb75pCycles = 0;
    uint64_t tlbdbFullCycles = 0;

    uint64_t tsbdbNonEmptyCycles = 0;
    uint64_t tsbdb75pCycles = 0;
    uint64_t tsbdbFullCycles = 0;

    uint64_t srfbNonEmptyCycles = 0;
    uint64_t srfb75pCycles = 0;
    uint64_t srfbFullCycles = 0;
    uint64_t srfbEntryNum = 0;
    uint64_t srfbEntryReadTotalCycles = 0;

    uint64_t nwcbNonEmptyCycles = 0;
    uint64_t nwcb75pCycles = 0;
    uint64_t nwcbFullCycles = 0;

    uint64_t swcbNonEmptyCycles = 0;
    uint64_t swcb75pCycles = 0;
    uint64_t swcbFullCycles = 0;
    uint64_t swcbEntryNum = 0;
    uint64_t swcbEntryWriteTotalCycles = 0;
};

struct ReadReq {
    uint64_t                                readAddr = -1ULL;
    uint64_t                                readSize = -1ULL;
};

struct WriteReq {
    uint64_t    writeAddr = -1ULL;
    uint64_t    readAddr = -1ULL;

    uint64_t    writeSize = -1ULL;
    uint64_t    validWriteSize = -1ULL;
    uint64_t    validD1TRSize = -1ULL;
    uint64_t    validD2TRSize = -1ULL;
    uint64_t    baseAddrGM = -1ULL;
    uint64_t    strideGM = -1ULL;
    uint64_t    baseAddrTR0 = -1ULL;
    /*
     * Using for TLOAD (ND2NZ, ND2ZN, DN2NZ, DN2ZN), TSTORE (ND2NZ)
     * A single Ring aaccelss has a bandwidth of 256B, and a fractal size is 512B. A fractal requires two RingTrans
     * operations, where RingTrans represents half a fractal.
     *
     * To facilitate implementation within the bridge and avoid using different implementations for layouts like
     * ND2NZ and DN2ZN, Row and Col are redefined here.
     * For GM, we refer to the continuous direction as D1 and the non-continuous direction as D2;
     * under ND, Col corresponds to D1, and Row corresponds to D2.
     */
    uint64_t    d1TR = -1ULL;
    uint64_t    d2TR = -1ULL;
    uint64_t    d1GM = -1ULL;
    uint64_t    d2GM = -1ULL;
    uint64_t    d1GMIdx = -1ULL;
    uint64_t    d2GMIdx = -1ULL;
    uint64_t    d1TRIdx = -1ULL;
    uint64_t    d2TRIdx = -1ULL;
    /*
     * Used for norm layout, it indicates the offset of the current write
     * within this row (ND2ND is row, DN2DN is column), in bytes.
     */
    uint64_t    d1Offset = -1ULL;
    double      dataSize = -1.0F;
    double      srcDataSize = -1.0F;
    DataType    srcType = DataType::INVALID;
    DataType    dstType = DataType::INVALID;
    bool         vld = true; // state for Vgather
    std::vector<bool>   writeMask = std::vector<bool>(MAX_GLOBAL_MEMORY_CACHELINE_SIZE, false);
};

/* ========== Bridge Data Buffer ========== */
struct DataBlock {
    bool                    vld = false;
    uint64_t                baseAddr;
    uint64_t                bpqId;
    std::vector<uint8_t>    data;
    DataBlock() = default;
    DataBlock(uint64_t addr, uint64_t id, uint16_t size)
        : vld(true), baseAddr(addr), bpqId(id)
    {
        data = std::vector<uint8_t>(size);
    };
};

class BridgeDataBuffer {
public:
    BridgeDataBuffer(uint32_t bdbSize, uint64_t dataSize);

    uint64_t                AllocDataBuffer(uint64_t baseAddr);
    uint64_t                AllocDataBuffer(uint64_t baseAddr, uint64_t bpqId);
    void                    WriteData(uint64_t bdbId, uint64_t addr, uint16_t size, uint8_t* data);
    std::vector<uint8_t>    ReadData(uint64_t addr, uint16_t size, uint64_t alignByte);
    DataBlock&              ReadDataBlock(uint64_t bdbId);
    void                    ReleaseBuffer(std::unordered_set<uint64_t> bdbIds);
    void                    ReleaseBuffer(uint64_t bpqId);
    void                    ClearBuffer();
    uint32_t                GetInvalidEntryCounter();
    uint32_t                GetAliveEntryCounter();
    bool                    CheckBufferHit(uint64_t bpqId, uint64_t addr, uint64_t alignByte);
private:
    uint64_t                m_dataSize;
    std::vector<DataBlock>  m_dataBuffer;
    uint32_t                m_aliveEntryCounter = 0;
};

/* ========== Read Fill Buffer ========== */
enum class RFBEntryStatus {
    WAIT,
    TRANSMITED,
    FLUSHED,
    RETURNED,
};

struct SwitchInfo {
    uint64_t                                bpqId;
    std::vector<uint8_t>                    data;
    /* dstOffset : (srcOffset, writeSize) */
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>>  switchMask;
};

struct RFBEntry {
    bool                        vld = false;
    uint64_t                    addr;
    uint64_t                    size;
    RFBEntryStatus              status = RFBEntryStatus::WAIT;
    uint64_t                    bpqId;
    TileOp                      op;
    uint64_t                    stid = -1U;

    /* using for flush */
    ROBID                       bid;

    std::vector<uint64_t>       bdbId;

    /* Pipeview */
    uint64_t                    waitCycle = 0;
    uint64_t                    txedCycle = 0;
    uint64_t                    returnCycle = 0;

    Direction                   memDir;

    std::string Dump()
    {
        std::stringstream oss;
        if (memDir == Direction::GM2TR || memDir == Direction::GM2GM) {
            oss << "GM";
        } else if (memDir == Direction::TR2TR || memDir == Direction::TR2GM) {
            oss << "Tile";
        } else {
            oss << "Unkown";
        }
        oss << " Read 0x" << std::hex << addr;
        oss << " Size " << std::dec << size << "B";
        return oss.str();
    }
};

struct ReadFillBufferConfig {
    SimSys* sim;
    uint64_t readGranularity;
    uint64_t bdbDataSize;
    uint32_t maxSendReadNum;
    std::shared_ptr<BridgeDataBuffer> bdb;
    std::shared_ptr<BridgeDataBuffer> tsBDB;
    MemoryUnit memUnit;
    TileUtils* util;
    uint64_t reqNum;
    uint32_t id;
    PMUData& pmuData;
};

struct MPBElem {
    uint64_t                                rfbId;
    std::shared_ptr<std::vector<uint8_t>>   data;
    std::vector<uint64_t>                   bdbIds;
    uint64_t                                bpqId;
    uint64_t                                readAddr;

    /* using for VSCATTER/MSCATTER */
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> switchMask;
};

class ReadFillBuffer;

class MemoryPreBuffer : public SimObj {
public:
    MemoryPreBuffer(SimSys *sim, std::weak_ptr<BridgePairQ> bpq, std::shared_ptr<BridgeDataBuffer> bdb,
                    std::shared_ptr<ReadFillBuffer> srfb, TileBridgeConfig config);
    void Work() override;
    void Xfer() override;
    void Reset() override;
    SimSys *GetSim() override;
    void ReportStat() override;
    bool Empty();
    bool PushData(MPBElem elem);
    void FillWCBAndBDB();

    OUTPUT std::shared_ptr<DelayQueue<SwitchInfo>>  m_switchNetwork;

private:
    SimSys                              *m_sim;
    std::weak_ptr<BridgePairQ>          m_bpq;
    std::shared_ptr<BridgeDataBuffer>   m_bdb;
    std::shared_ptr<ReadFillBuffer>     m_srfb;
    TileBridgeConfig                    m_config;
    SimQueue<MPBElem>                   m_memoryPreBuffer;
};

class ReadFillBuffer : public RingNodeObj {
public:
    ReadFillBuffer() = delete;
    explicit ReadFillBuffer(ReadFillBufferConfig& config);

    void        Work() override;
    void        Xfer() override;
    void        Reset() override;
    SimSys*     GetSim() override;
    void        ReportStat() override;
    void        Flush(FlushBus &bus);
    void        PrintStatus();
    uint32_t    GetInvalidEntryCounter();
    uint32_t    GetAliveEntryCounter();
    uint32_t    GetTxedEntryCounter();
    bool        CheckInFlightReq(uint64_t addr);
    void        ReleaseEntry(uint64_t rfbId);
    void        PrintPipeViewLog(uint64_t bpqId, uint64_t stid);

    INPUT  std::shared_ptr<SimQueue<RFBEntry>>          m_bpqRFBQ;
    OUTPUT std::shared_ptr<SimQueue<RFBEntry>>          m_rfbBpqQ;

    INPUT  SimQueue<GFUMemReq>                  *m_memRFBRspQ;
    OUTPUT SimQueue<GFUMemReq>                  *m_rfbMemReqQ;

    INPUT  SimQueue<TileRegTTransLdRes>         *m_tileRegRFBLdResQ;
    OUTPUT SimQueue<TTransTileRegLdReq>         *m_rfbTileRegLdReqQ;

    std::weak_ptr<BridgePairQ>                  m_bpq;
    std::weak_ptr<MemoryPreBuffer>              m_memoryPreBuffer;
private:
    void                ReciveReadReqFromBPQ();
    void                SendReadMemReq(uint64_t entryId, RFBEntry& entry);
    TTransTileRegLdReq  CreateTileLdReq(uint64_t reqId, uint64_t addr, uint64_t size) const;
    void                SendReadTileReq(uint64_t entryId, RFBEntry& entry);
    void                SendReadReq();
    void                ReciveTileDataAndUpdateStatus();
    void                RefillTileData(uint32_t repId, uint8_t *data);
    void                ReciveMemDataAndUpdateStatus();
    void                ReciveDataAndUpdateStatus();
    void                PrintPipeViewLog(RFBEntry& entry);

    /*
     * (Control by parameter)
     * For SRFB(GM->DBD), this value is determined by the SOC aaccelss parameter and is currently set to 64B.
     * For NRFB(TileReg->DBD), this value corresponds to the size of ring, which is currently 128B(1024b)
     */
    const uint64_t                              m_readGranularity;
    const uint64_t                              m_bdbDataSize;
    const uint32_t                              m_outStandingReqNum;

    const uint32_t                              m_maxSendReadNum; // for SRFB is GM; for NRFB is TR

    std::vector<RFBEntry>                       m_rfbQ;
    std::shared_ptr<BridgeDataBuffer>           m_bdb;
    std::shared_ptr<BridgeDataBuffer>           m_tsBDB;
    SimSys                                      *m_sim;
    uint32_t                                    m_aliveRFBEntryCounter = 0;
    std::deque<uint32_t>                        m_pendingReqFifo;
    uint32_t                                    id;
    PMUData&                                    m_pmuData;

    std::unordered_map<uint64_t, std::vector<InstPipeViewPtr>>  m_pipeviewInfo;

    MemoryUnit                                  m_memUnit;
    int                                         needReadRFArb = -1;
};

/* ========== Write Coalesce Buffer ========== */
enum class WCBEntryStatus {
    NO_START,
    WAIT_READ,
    INTERGRATING,
    DATA_READY,
    TRANSMITED,
    FLUSHED,
    COMPLETE
};

struct BDBReadReq {
    uint64_t bpqId;
    uint64_t readAddr;
    uint64_t readSize;
    uint64_t writeOffset;
    uint64_t elementNum;
    std::bitset<FRACTAL_ROW_BYTES> writeMask;
};

struct WCBEntry {
    bool            vld = false;
    size_t          entryId = 0;
    WriteReq        writeReq;
    TileOp          op;
    WCBEntryStatus  status = WCBEntryStatus::NO_START;
    uint64_t        bpqId;
    LayOut          layout;

    uint64_t        tileReqId;
    uint64_t        stid = -1U;

    /* using for flush */
    ROBID           bid;
    std::vector<uint8_t>    data;
    std::queue<BDBReadReq>  bdbReadReqBuffer;
    std::vector<bool>       writeMask = std::vector<bool>(MAX_GLOBAL_MEMORY_CACHELINE_SIZE, false);

    /* the data size received by the mgather from the switch */
    uint32_t                    receiveDataSize = 0;

    /* Pipeview */
    uint64_t                    startCycle = 0;
    uint64_t                    intergratingCycle = 0;
    uint64_t                    dataReadyCycle = 0;
    uint64_t                    txedCycle = 0;
    uint64_t                    busSendCycle = 0;
    uint64_t                    receiveDBIDCycle = 0;
    uint64_t                    completeCycle = 0;
    MemoryUnit                  memDir;

    std::string Dump()
    {
        std::stringstream oss;
        if (memDir == MemoryUnit::GM) {
            oss << "GM";
        } else {
            oss << "Tile";
        }
        oss << " Write 0x" << std::hex << writeReq.writeAddr;
        return oss.str();
    }
};

struct WriteCoalesceBufferConfig {
    SimSys* sim;
    std::shared_ptr<BridgeDataBuffer>   bdb;
    MemoryUnit memUnit;
    TileUtils* util;
    uint64_t reqNum;
    uint64_t bdbDataSize;
    uint64_t id;
    PMUData& pmuData;
};

class WriteCoalesceBuffer : public RingNodeObj {
public:
    WriteCoalesceBuffer() = delete;
    explicit WriteCoalesceBuffer(WriteCoalesceBufferConfig& config);

    void        Work() override;
    void        Xfer() override;
    void        Reset() override;
    SimSys*     GetSim() override;
    void        ReportStat() override;
    void        Flush(FlushBus &bus);
    void        PrintStatus();
    uint32_t    GetInvalidEntryCounter();
    uint32_t    GetAliveEntryCounter();
    uint32_t    GetTxedEntryCounter();
    bool        IsWriteRingBlocked() const;
    bool        IsWriteReady() const;
    void        PrintPipeViewLog(uint64_t bpqId, uint64_t stid);

    INPUT  std::shared_ptr<SimQueue<WCBEntry>>          m_bpqWCBAllocQ;
    INPUT  std::shared_ptr<DelayQueue<SwitchInfo>>      m_switchNetwork;
    OUTPUT std::shared_ptr<SimQueue<WCBEntry>>          m_wcbBpqQ;

    OUTPUT SimQueue<TTransTileRegStReq>             *m_wcbTileRegStReqQ;
    INPUT  SimQueue<TileRegTTransLdRes>             *m_tileRegWCBWrResQ;

    OUTPUT SimQueue<GFUMemReq>                      *m_wcbMemReqQ;
    OUTPUT SimQueue<GFUMemReq>                      *m_wcbMemRspQ;

    std::weak_ptr<BridgePairQ>                      m_bpq;
private:
    void                ReceiveWriteReqFromBPQ();
    void                ReceiveSwitchNetReq();
    void                SplitReadBDBReq();
    void                CollectData();
    void                SendWriteReq();
    void                SendWriteReqToMem(size_t idx, WCBEntry& entry);
    TTransTileRegStReq  CreateTileStReq(uint64_t reqId, uint64_t addr, const std::vector<uint8_t>& data, uint64_t size,
                                        DataMask mask, uint32_t stid) const;
    bool                SendWriteReqToTileReg(size_t idx, WCBEntry& entry);
    void                ReceiveTileWriteResp();
    void                ReceiveGmWriteResp();
    void                ResolveEntry();
    void                PrintPipeViewLog(WCBEntry& entry);

    std::vector<WCBEntry>                           m_wcbQ;
    std::shared_ptr<BridgeDataBuffer>               m_bdb;
    SimSys                                          *m_sim;
    uint32_t                                        m_outStandingReqNum = 0;
    uint64_t                                        m_bdbDataSize = 0;
    uint32_t                                        m_aliveWCBEntryCounter = 0;
    uint32_t                                        id;
    PMUData&                                        m_pmuData;

    std::unordered_map<uint64_t, std::vector<InstPipeViewPtr>>  m_pipeviewInfo;

    std::deque<uint64_t>                            m_pendingReqFifo;
    MemoryUnit                                      m_memUnit;

    int                                             needWriteRFArb = -1;
    /* DFX only, indicate current cycle want to write ring */
    bool                                            m_hasReadyWrite = false;
    /* DFX only, indicate current cycle want to write ring but blocked */
    bool                                            m_ringBlocked = false;
};

/* ========== Bridge Pair Queue ========= */

struct TileBridgeArg {
    TileOp                  op;
    DataType                srcType;
    DataType                dstType;
    LayOut                  layout;
    // Global momery 第一维：ND时表示列个数， DN时表示行个数
    uint32_t                d1GM = 0;
    uint32_t                originalD1GM = 0;
    // Global momery 第二维：ND时表示行个数， DN时表示列个数
    uint32_t                d2GM = 0;
    // Global momery 中第二维中开头两个元素的距离
    uint32_t                strideGM = 0;
    // Tile register 中形态的列个数
    uint32_t                d1TR = 0;
    // Tile register 中形态的行个数
    uint32_t                d2TR = 0;
    uint32_t                originalD1TR = 0;
    uint32_t                originalD2TR = 0;
    // Global momery 中的基址
    uint64_t                baseAddrGM = -1;
    // Tile register 中的基址
    uint64_t                baseAddrTR0 = 0;
    /* Only use by mscatter */
    uint64_t                baseAddrTR1 = 0;
    // TMOV base addr of the dst tile reg
    uint64_t                baseAddrDstTR = 0;
    // FIXME: ptag of dst tile reg
    uint64_t                ptag = 0;

    // Vgather 和 Vscatter
    std::vector<bool>       predicateMask;
    uint32_t groupSlotId = 0;
    uint32_t tid = 0;
    uint32_t stid = -1U;

    void InitByMemReqBus(BlockCommandPtr cmd);
};

enum class BPQAttr {
    ORIGINAL,
    /* For ND always split by col; For DN always split by row; report dst ready to bissue */
    SUB_INST,
    /* aggregator of SUB_INST, report inst resolve to brob and release bdb */
    SUB_INST_LEAD,
    /* For ND always split by row; For DN always split by col; */
    SUB_ROW,
    /* aggregator of SUB_ROW and release bdb */
    SUB_ROW_LEAD,
    /* Only for tstore normal layout, split col */
    SUB_COL,
    /* aggregator of SUB_COL, report inst resolve to brob and release bdb */
    SUB_COL_LEAD,
};

enum class ReadReqStatus {
    NOSTART,
    ISSUED,
};

struct ReadReqPipeView {
    uint64_t                    startCycle = 0;
    uint64_t                    firstPickCycle = 0;
    uint64_t                    hitCycle = 0;
    uint64_t                    missCycle = 0;
    uint64_t                    caqCycle = 0;
    uint64_t                    bdbReadCycle = 0;
    uint64_t                    bdbWriteCycle = 0;
    uint64_t                    rfbCycle = 0;
    uint64_t                    retireCycle = 0;
    explicit ReadReqPipeView(uint64_t startCycle) : startCycle(startCycle) {}
};

struct BPQEntry {
    uint64_t                bpqID = 0;
    ROBID                   bid;
    TileBridgeArg           tileArg;
    Direction               direction;
    ExecStatus              pairStatus = ExecStatus::NO_START;
    BPQAttr                 attr = BPQAttr::ORIGINAL;

    std::vector<ReadReq>    readReqs;
    std::vector<WriteReq>   writeReqs;

    /* Using for MGather */
    /* addr with size */
    std::vector<std::pair<uint64_t, uint32_t>>  readOffsetReqs;
    std::vector<std::pair<uint64_t, uint32_t>>  readDataReqs;
    std::vector<ReadReqPipeView>                readReqTagPipeViews;
    uint32_t                                    receiveReadCnt = 0;
    uint32_t                                    sendSrcTileReadCnt = 0;
    std::vector<uint64_t>                       dataAddrs;
    std::vector<bool>                           readReqSentAttemped;
    std::vector<ReadReqStatus>                  readReqStatus;
    std::vector<uint64_t>                       readReqSendCnt;
    /* Using for mscatter/vscatter, records the storage location of the data tile in the TS BDB. */
    std::vector<uint64_t>                       tsBDBIds;

    uint64_t                sendReadCounter = 0;
    uint64_t                resolveReadCounter = 0;
    uint64_t                sendWriteCounter = 0;
    uint64_t                resolveWriteCounter = 0;

    bool                        hasSendResolve = false;
    uint64_t                    bdbRelatedBPQNum = 0;           // dedicate for SUB_ROW_LEAD and SUB_INST_LEAD
    uint64_t                    activeRowBPQNum = 0;            // dedicate for SUB_INST and SUB_INST_LEAD
    uint64_t                    activeInstBPQNum = 0;           // dedicate for SUB_INST_LEAD
    std::shared_ptr<BPQEntry>   instLeadBPQPtr;
    std::shared_ptr<BPQEntry>   rowLeadBPQPtr;

    /* Using for pipeview */
    uint64_t                startCycle = 0;
    uint64_t                waitOtherBPQCycle = 0;
    uint64_t                genReadCycle = 0;
    uint64_t                waitReadCycle = 0;
    uint64_t                genWriteCycle = 0;
    uint64_t                waitWriteCycle = 0;
    uint64_t                completeCycle = 0;

    BlockCommandPtr         blockCmd = nullptr;

    std::string Dump()
    {
        std::stringstream oss;
        oss << TASK_LABEL << bid.val;
        oss << blockCmd->Dump();
        return oss.str();
    }

    std::string DumpSwimLane()
    {
        std::stringstream oss;
        oss << TASK_LABEL << bid.val;
        oss << blockCmd->Dump();
        return oss.str();
    }
};
using BPQEntryPtr = std::shared_ptr<BPQEntry>;

struct BridgePairQConfig {
    SimSys                                  *sim;
    TileUtils                               *util;
    uint64_t                                id;
    TileBridgeConfig&                       config;
    std::shared_ptr<BridgeDataBuffer>       bdb;
    std::shared_ptr<BridgeDataBuffer>       tsBDB;
    std::shared_ptr<ReadFillBuffer>         m_nrfb;
    std::shared_ptr<ReadFillBuffer>         m_srfb;
    std::shared_ptr<WriteCoalesceBuffer>    m_nwcb;
    std::shared_ptr<WriteCoalesceBuffer>    m_swcb;
    PMUData&                                pmuData;
};

enum class FakeChannelStatus {
    NO_START,
    SEND_READ,
    WAIT_READ_DATA,
    COLLECT_DATA,
    SEND_WRITE,
    WAIT_WRITE_RESP,
    COMPLETE,
};

struct GatherDataEntry {
    RFBEntryStatus              status = RFBEntryStatus::WAIT;
    uint32_t                    socDataTid = -1;
    uint64_t                    originalAddr = -1;
    uint8_t                     gatherData[MAX_TILE_DATA_BYTE] = {};  // 获取的单次soc回复数据内容
    uint64_t                    gatherDataSize = -1; // 获取的数据类型size (默认Uint32)
    uint64_t                    txCycle = 0;
    uint64_t                    receiveCycle = 0;
};

struct FakeChannelEntry {
    uint64_t                    bpqId;
    TileOp                      op;
    FakeChannelStatus           status = FakeChannelStatus::NO_START;
    uint64_t                    laneMask;
    uint64_t                    startCycle = 0;
    uint64_t                    completeCycle = 0;
    uint64_t                    sendLaneNum = 0;
    uint64_t                    rcvLaneNum = 0;
    uint64_t                    baseAddrDstTR = 0;
    uint64_t                    gatherDataSize = -1; // 获取的数据类型size (默认Uint32)
    uint8_t data[MAX_LANE_DATA_SIZE] = {0}; // 完整的2048bits， 4 * 256B = 1024B
    uint64_t   countWriteTileNum = 0;
    std::vector<WriteReq>       writeReqs;
    std::vector<GatherDataEntry> dataEntry;
};

class FakeChannelNode {
public:
    void Work();
    void SetBridgeMemReqQ(BridgePairQ *bridgePairQ,
        SimQueue<GFUMemReq> *bridgeMemReqQ, SimQueue<GFUMemReq> *bridgeMemRspQ);
    void SendSocReq(uint64_t sendIndex, FakeChannelEntry &channelEntry,  GatherDataEntry &gatherData);
    void ReceiveSocRsp();
    void CollectTileWriteData();
    void SendChannelEntryMsg();
    void ReceiveTileWriteResp();
    bool SendTileWriteData(FakeChannelEntry &channelEntry, WriteReq &writeEntry) const;
    void SendWriteTileData();
    TileUtils *pTileUtils;
    TileBridgeConfig  m_config;
    std::map<uint64_t, FakeChannelEntry> bpqDataInfo; // key: uint64_t bpqId;
    OUTPUT SimQueue<TTransTileRegStReq>   *bridgeTileRegStReqQ;
private:
    INPUT SimQueue<GFUMemReq>             *m_bridgeMemReqQ = nullptr;
    OUTPUT SimQueue<GFUMemReq>            *m_memBridgeRspQ = nullptr;
    BridgePairQ *bpqInst = nullptr;
};

struct TagReq {
    uint64_t    readReqId;
    BPQEntryPtr entry;
};

struct TagConfig {
    SimSys                              *sim;
    TileBridgeConfig&                   config;
    std::shared_ptr<BridgeDataBuffer>   bdb;
    std::shared_ptr<ReadFillBuffer>     srfb;
    std::weak_ptr<BridgePairQ>          bpq;
    PMUDataBase&                        pmuData;
};

class Tag : public SimObj {
public:
    explicit Tag(TagConfig& conf);
    void Work() override;
    void Xfer() override;
    void Reset() override;
    SimSys *GetSim() override;
    void ReportStat() override;
    void Clear();
    bool HasDataInflight(uint64_t readAddr);
    void PrintPipeViewLog(BPQEntryPtr entry, uint64_t readReqId);

    INPUT  std::shared_ptr<SimQueue<TagReq>>        m_bpqTagReadReqQ;
    OUTPUT std::shared_ptr<SimQueue<RFBEntry>>      m_tagSRFBQ;
    OUTPUT std::shared_ptr<SimQueue<CacheAaccelssOp>> m_cacheAaccelssQ;

private:
    std::vector<uint64_t>                       Alloc(uint64_t addr);
    std::vector<uint64_t>                       Alloc(uint64_t addr, std::vector<uint64_t>& bdbInfo);
    void                                        Erase(uint64_t addr);
    bool                                        Read(uint64_t addr, std::vector<uint64_t>& bdbInfo);
    bool                                        CheckVictim();
    std::pair<uint64_t, std::vector<uint64_t>>  GetVictim();
    std::pair<uint64_t, std::vector<uint64_t>>  ReplaceVictim(uint64_t addr);
    void                                        HandleReadReq();

    SimSys                                      *m_sim;
    TileBridgeConfig                            m_config;
    std::shared_ptr<BridgeDataBuffer>           m_bdb;
    std::shared_ptr<ReadFillBuffer>             m_srfb;
    std::weak_ptr<BridgePairQ>                  m_bpq;
    /* {Addr : (TL BDB id)} */
    OrderedMap<uint64_t, std::vector<uint64_t>> m_orderedMap;
    PMUDataBase&                                m_pmuData;
};

class BridgePairQ : public RingNodeObj {
public:
    explicit BridgePairQ(BridgePairQConfig& config);
    void Work() override;
    void Xfer() override;
    void Reset() override;
    SimSys *GetSim() override;
    void ReportStat() override;
    void Flush(FlushBus &bus);
    bool IsBusy();
    bool IsTloadBusy();
    bool IsTstoreBusy();
    bool IsTmovBusy();
    bool IsMgatherBusy();
    bool IsMscatterBusy();
    bool IsVgatherBusy();
    bool IsVscatterBusy();
    void IncOutstandingTileOpNum(TileOp op);
    void DecOutstandingTileOpNum(TileOp op);
    void PrintStatus();
    uint64_t GetBPQSize();
    uint64_t RemainBPQSize(bool isTstore);
    BPQEntryPtr GetOldestBpqPtr(uint32_t stid);
    void SendFackChannelReq();
    void        ReleaseCAQAvailCnt();
    void        AcquireCAQAvailCnt();
    void        ReleaseSRFBAvailCnt();
    void        AcquireSRFBAvailCnt();
    void        ReleaseSWCBAvailCnt();
    void        AcquireSWCBAvailCnt();
    BPQEntryPtr GetBpqPtr(uint64_t bpqId);
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> GetSwitchMask(uint64_t bpqId, uint64_t baseAddr);

    INPUT  SimQueue<MemReqBus>                          *m_lsuBridgeTloadQ;
    INPUT  SimQueue<MemReqBus>                          *m_lsuBridgeTstoreQ;

    OUTPUT SimQueue<BlockCommandPtr>                    *tmaBCCWakeupQ;
    OUTPUT SimQueue<BlockCommandPtr>                    *tauBCCWakeupQ;
    INPUT  SimQueue<BlockCommandPtr>                    *bccTAUBlockCmdQ;

    OUTPUT std::shared_ptr<SimQueue<RFBEntry>>          m_bpqNRFBQ;
    OUTPUT std::shared_ptr<SimQueue<WCBEntry>>          m_bpqNWCBQ;
    OUTPUT std::shared_ptr<SimQueue<RFBEntry>>          m_bpqSRFBQ;
    OUTPUT std::shared_ptr<SimQueue<WCBEntry>>          m_bpqSWCBQ;

    INPUT  std::shared_ptr<SimQueue<RFBEntry>>          m_nRFBBpqQ;
    INPUT  std::shared_ptr<SimQueue<WCBEntry>>          m_nWCBBpqQ;
    INPUT  std::shared_ptr<SimQueue<RFBEntry>>          m_sRFBBpqQ;
    INPUT  std::shared_ptr<SimQueue<WCBEntry>>          m_sWCBBpqQ;

    /* Using for MGather */
    INPUT  SimQueue<TileRegTTransLdRes>                 *m_tileRegBPQLdResQ;
    OUTPUT SimQueue<TTransTileRegLdReq>                 *m_bpqTileRegLdReqQ;
    /* Using for VGather和 VScatter */
    INPUT   SimQueue<VectorBridgeReq>                   *m_vectorBridgeBPQReqQ;
    OUTPUT  SimQueue<VectorBridgeRsp>                   *m_vectorBridgeBPQRspQ;

    OUTPUT std::shared_ptr<SimQueue<TagReq>>            m_bpqTagReadReqQ;
    OUTPUT std::shared_ptr<SimQueue<CacheAaccelssOp>>     m_cacheAaccelssQ;
    OUTPUT std::shared_ptr<DelayQueue<SwitchInfo>>      m_switchNetwork;

    FakeChannelNode                                     fakeChannelNode;
    std::shared_ptr<Tag>                                m_tag;

    std::unordered_map<uint64_t, std::bitset<MAX_GLOBAL_MEMORY_CACHELINE_SIZE>> m_mgatherReadTagDataUsage;

    void SetBPQEntryState(uint32_t bpqID);
    uint64_t GetCoreId() const
    {
        return id;
    }
    std::shared_ptr<WriteCoalesceBuffer> GetNWcb() {return m_nwcb;}
private:
    void SplitTloadAndTstoreSubRowBPQ(std::vector<BPQEntryPtr>& subInstBPQArray, const uint64_t splitD2TR);
    void SplitTmovSubRowBPQ(std::vector<BPQEntryPtr>& subInstBPQArray, const uint64_t splitD2TR);
    void SplitSubRowBPQ(BlockCommandPtr cmd, std::vector<BPQEntryPtr>& subInstBPQArray);
    std::shared_ptr<std::vector<BPQEntryPtr>> SplitTLoadSubInstBPQ(MemReqBus& memReq) const;
    bool ReceiveTLoadReq();
    void SplitTStoreNormSubColBPQ(BPQEntryPtr entry);
    bool ReceiveTStoreReq();
    bool ReceiveTMovReq();
    void SplitGatherBPQ(BPQEntryPtr entry);
    bool ReceiveMGatherReq();
    void SplitScatterBPQ(BPQEntryPtr entry);
    bool ReceiveMScatterReq();
    bool ReceiveVectorMsg();
    void ReceiveMemReq();

    void HandleTLoadBPQ(BPQEntryPtr entry);
    void HandleTMovBPQ(BPQEntryPtr entry);
    void HandleTStoreBPQ(BPQEntryPtr entry);
    void HandleGatherBPQ(BPQEntryPtr entry);
    void HandleMScatterBPQ(BPQEntryPtr entry);
    void HandleVScatterBPQ(BPQEntryPtr entry);
    void InsertSplitedBPQ();
    TTransTileRegLdReq CreateTileLdReq(uint32_t reqId, uint64_t addr, uint64_t size) const;
    void SendReadTileReq(BPQEntryPtr entry);
    void ReceiveTileData();
    void HandleGatherReadSrc();

    bool SendOneReadReq2RFB(BPQEntryPtr bpqEntry);
    bool SendOneWriteReq2WCB(BPQEntryPtr bpqEntry);
    uint32_t GetRFBRemainSize(Direction direction);
    uint32_t GetWCBRemainSize(Direction direction);
    uint32_t GetBDBRemainSize(Direction direction, uint32_t pendingBDBReqNum);
    void SendReadWriteReqNormal();
    BPQEntryPtr PickEntry();
    void SendReadWriteReqMemoryMode();
    void SendTagReq(BPQEntryPtr entry);
    void SendReadWriteReq();
    void StubVectorSendVgather() const;
    void StubProcessVecBridgeMsg();
    void ReceiveRFBReadRslv();
    void ReceiveWCBWriteRslv();

    void PrintPipeViewLog(BPQEntryPtr entry);
    void LogSwimLane(BPQEntry& entry);
    bool ResolveSplitBPQ(BPQEntryPtr entry, SimQueue<BlockCommandPtr>& wakeUpQ);
    void ResolveBlock();

    void HandleVscatterTileTrans();
    void SendDBidResponse(BPQEntryPtr entry);
    void ReceiveVscatterDataAddr();

    // 方案3: VSCATTER stub - 保留CHIFlit打包/解包，内部短路
    void GenerateVscatterStubTilesPlan3(BPQEntryPtr entry);  // 生成并注入stub tiles
    void InjectStubAddrTilePlan3(BPQEntryPtr entry);  // 注入Addr Tile
    void InjectStubDataTilePlan3(BPQEntryPtr entry);  // 注入Data Tile

    uint64_t                                            id;
    TileBridgeConfig                                    m_config;
    std::deque<uint64_t>                                m_tidPool;
    std::list<BPQEntryPtr>                              m_bpq;
    std::priority_queue<uint64_t, std::vector<uint64_t>, std::greater<uint64_t>>    m_bpqIDPool;
    std::map<uint64_t, BPQEntryPtr>                     m_bpqFakeChannel;
    std::shared_ptr<BridgeDataBuffer>                   m_bdb;
    std::shared_ptr<BridgeDataBuffer>                   m_tsBDB;
    std::shared_ptr<ReadFillBuffer>                     m_nrfb;
    std::shared_ptr<ReadFillBuffer>                     m_srfb;
    std::shared_ptr<WriteCoalesceBuffer>                m_nwcb;
    std::shared_ptr<WriteCoalesceBuffer>                m_swcb;
    SimSys                                              *m_sim;
    PMUData&                                            m_pmuData;
    std::list<BPQEntryPtr>                              m_splitedBPQList;
    std::list<BPQEntryPtr>                              m_splitedTStoreBPQList;
    std::vector<std::list<ROBID>>                       m_tloadBidOrderList;

    /* Memory Mode Resource Management */
    uint64_t                                            m_caqAvailCnt = 0;
    uint64_t                                            m_srfbAvailCnt = 0;
    uint64_t                                            m_swcbAvailCnt = 0;
    uint64_t                                            m_mpbAvailCnt = 0;

    uint64_t                                            m_tloadCnt = 0;
    uint64_t                                            m_tstoreCnt = 0;
    uint64_t                                            m_tmovCnt = 0;
    uint64_t                                            m_mgatherCnt = 0;
    uint64_t                                            m_vgatherCnt = 0;
    uint64_t                                            m_mscatterCnt = 0;
    uint64_t                                            m_vscatterCnt = 0;
};

/* ========== Bridge Table ========== */
class TileBridge : public RingNodeObj {
public:
    TileBridge(SimSys *sim, TileUtils *util, uint32_t coreId);

    void Work() override;
    void Xfer() override;
    void Reset() override;
    SimSys *GetSim() override;
    void ReportStat() override;
    void Flush(FlushBus &bus);
    void Build();
    bool IsBusy();
    void CheckBusy();
    bool IsTloadBusy();
    bool IsWriteRingBlocked() const;
    bool IsTstoreBusy();
    bool IsTmovBusy();
    bool IsMgatherBusy();
    bool IsMscatterBusy();
    bool IsVgatherBusy();
    bool IsVscatterBusy();
    PMUData& GetPMUData();
    uint64_t GetBPQSize();

    bool lastCycleTMABusy = false;
    uint64_t tmaBusyStartCycle = 0;

    /* Bridge <-> LSU */
    INPUT  SimQueue<MemReqBus>                          *m_lsuBridgeTloadQ;
    INPUT  SimQueue<MemReqBus>                          *m_lsuBridgeTstoreQ;

    /* Bridge <-> BCC */
    OUTPUT SimQueue<BlockCommandPtr>                    *tmaBCCWakeupQ;
    OUTPUT SimQueue<BlockCommandPtr>                    *tauBCCWakeupQ;
    INPUT  SimQueue<BlockCommandPtr>                    *bccTAUBlockCmdQ;

    /* Bridge <-> TileReg */
    INPUT  SimQueue<TileRegTTransLdRes>                 *m_tileRegBridgeLdResQ;
    INPUT  SimQueue<TileRegTTransLdRes>                 *m_tileRegBridgeWrResQ;
    OUTPUT SimQueue<TTransTileRegLdReq>                 *m_bridgeTileRegLdReqQ;

    OUTPUT SimQueue<TTransTileRegStReq>                 *m_bridgeTileRegStReqQ;
    /* Bridge <-> Mem */
    INPUT  SimQueue<GFUMemReq>                          *m_memBridgeRspQ;
    OUTPUT SimQueue<GFUMemReq>                          *m_bridgeMemReqQ;
    /* Bridge <-> Vector */
    INPUT   SimQueue<VectorBridgeReq>                   *m_vectorBridgeReqQ;
    OUTPUT  SimQueue<VectorBridgeRsp>                   *m_vectorBridgeRspQ;

    TileBridgeConfig                                    m_config;

    uint32_t                                            id;
private:
    void PrintStatus();
    void RecordPMUData();
    void RecordTloadTopDownPMUData();
    void RecordTloadTopDownFetching(BPQEntryPtr bpqEntry);
    BPQEntryPtr GetOldestBpqPtr(uint32_t stid);
    void AaccelssBDB();

    INNER std::shared_ptr<SimQueue<RFBEntry>>           m_bpqNRFBQ;
    INNER std::shared_ptr<SimQueue<WCBEntry>>           m_bpqNWCBQ;
    INNER std::shared_ptr<SimQueue<RFBEntry>>           m_bpqSRFBQ;
    INNER std::shared_ptr<SimQueue<WCBEntry>>           m_bpqSWCBQ;

    INNER std::shared_ptr<SimQueue<RFBEntry>>           m_nRFBBpqQ;
    INNER std::shared_ptr<SimQueue<WCBEntry>>           m_nWCBBpqQ;
    INNER std::shared_ptr<SimQueue<RFBEntry>>           m_sRFBBpqQ;
    INNER std::shared_ptr<SimQueue<WCBEntry>>           m_sWCBBpqQ;

    INNER std::shared_ptr<SimQueue<TagReq>>             m_bpqTagReadReqQ;
    INNER std::shared_ptr<SimQueue<CacheAaccelssOp>>      m_cacheAaccelssQ;
    INNER std::shared_ptr<DelayQueue<SwitchInfo>>       m_switchNetwork;

    std::shared_ptr<BridgePairQ>                        m_bpq;
    /* TileReg -> BDB */
    std::shared_ptr<ReadFillBuffer>                     m_nrfb;
    /* BDB -> TileReg */
    std::shared_ptr<WriteCoalesceBuffer>                m_nwcb;
    /* Global Mem -> BDB */
    std::shared_ptr<ReadFillBuffer>                     m_srfb;
    /* BDB -> Global Mem */
    std::shared_ptr<WriteCoalesceBuffer>                m_swcb;
    std::shared_ptr<BridgeDataBuffer>                   m_bdb;
    std::shared_ptr<BridgeDataBuffer>                   m_tsBDB;
    std::shared_ptr<Tag>                                m_tag;
    PMUData                                             m_pmuData;

    std::shared_ptr<MemoryPreBuffer>                    m_memoryPreBuffer;

    SimSys                                              *m_sim;
};
}
}
}
#endif  // MODEL_BRIDGETABLE_TILEBRIDGE_H
