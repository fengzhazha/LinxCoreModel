#ifndef BLOCKISA_CUBE_CORE_H
#define BLOCKISA_CUBE_CORE_H

#include <vector>

#include "../configs/cube_config.h"
#include "../tools/trace_logger/TraceLogger.h"
#include "Common.h"
#include "core/Bus.h"
#include "cube/CubeCoreStats.h"
#include "interface/Credit.h"
#include "interface/InterfaceCommon.h"
#include "tmu/ring/RingNodeObj.h"
#include "SimSys.h"
#include "plat/DebugLog.h"

namespace JCore {


const uint64_t cubeThreadNum = 1;
const uint64_t invalid64Bits = 0xffffffffffffffff;

enum class MMStage {
    IDLE,
    LOAD,
    EXE,
    STORE,
    COMPLETE
};

enum class TileFormat {
    N,
    Z,
};

enum class ACCCVTMode {
    NORM,
    NZ2ND,
    NZ2DN
};

enum class DataPackageSize {
    DATA_SIZE256B = 0,
    DATA_SIZE512B,
    DATA_SIZE1024B,
    DATA_SIZE2048B,
};
class CubeUop;
class ACCCVTUop;
using CubeUopPtr = std::shared_ptr<CubeUop>;
using ACCCVTUopPtr = std::shared_ptr<ACCCVTUop>;

class BufferPreWriteInfo {
public:
    // pre free bufferc when uop is the last loop in loopk, pre write to acc and take effect at realWriteDoneCyle
    bool preWrite = false;
    bool realWriteDoneCyle = 0;
};

class L0Buffers {
public:
    uint64_t tileRegAddr = -1U;
    uint32_t bid = 0;
    uint32_t stid = 0;
    bool wrote = false;
    const uint64_t tileSize = 2048;
    std::vector<uint8_t> data;
    std::vector<std::pair<uint64_t, uint64_t>> index;
    bool ready = false;
    bool free = true;
    bool alreadyGen = false;
    uint64_t dependenceCount = 0;
    uint64_t loadCount = 0;
    uint64_t readyCount = 0;
    uint64_t loopKBound = 0;
    uint64_t loopKCount = 0;
    bool readyForCpyOut = false;
    uint32_t bufferIdx = 0; // for debug
    std::vector<std::vector<uint64_t>> dataLayout;
    L0Buffers():data(tileSize / sizeof(uint8_t), 0) {}
    ~L0Buffers() {}
    void FreeAndReset()
    {
        bid = 0;
        wrote = false;
        free = true;
        ready = false;
        dependenceCount = 0;
        alreadyGen = false;
        loadCount = 0;
        readyCount = 0;
        loopKBound = 0;
        loopKCount = 0;
        readyForCpyOut = false;
        std::fill(data.begin(), data.end(), 0);
        index.clear();
    }

    void OnlyReset()
    {
        wrote = false;
        ready = false;
        dependenceCount = 0;
        alreadyGen = false;
        loadCount = 0;
        readyCount = 0;
        loopKBound = 0;
        loopKCount = 0;
        readyForCpyOut = false;
        std::fill(data.begin(), data.end(), 0);
    }

    uint64_t Load(uint64_t offset, uint64_t elementwise)
    {
        if (offset + elementwise > data.size()) {
            throw std::out_of_range("Load operation out of bounds");
        }
        uint64_t res = 0;
        const uint64_t wise = 8;
        for (size_t i = 0; i < elementwise; i++) {
            res |= data[offset + i] << (wise * i);
        }
        return res;
    }

    void Store(uint64_t offset, uint64_t elementwise, uint64_t wrtieData)
    {
        if (offset + elementwise > data.size()) {
            throw std::out_of_range("Store operation out of bounds");
        }
        const uint64_t wise = 8;
        const uint64_t mask = 0xff;
        for (size_t i = 0; i < elementwise; i++) {
            data[offset + i] = (wrtieData >> (i * wise)) & mask;
        }
    }
};

class CubeRobInfo {
public:
    BlockCommandPtr blockCmd = nullptr;
    bool renamed = false;
    bool renameACC = false;
    bool genUop = false;
};

class L0CRenameInfo {
public:
    bool     needFree = false;
    uint32_t lastRenameBid = 0;
    uint64_t lastRslvBid = 0xffffffffffffffff;
    uint32_t dependBid = 0;
    std::map<std::pair<uint64_t, uint64_t>, L0Buffers*> allocMap;
};

class RowMaxBuffer {
public:
    bool ready = false;
    std::vector<uint64_t> rowMaxValue;
};

class CubeUop {
public:
    uint64_t uid = 0;
    uint32_t accChainId = 0;
    uint32_t bid = 0;
    uint32_t stid = -1U;
    uint64_t uopIndex = 0;
    uint64_t globleUopIndex = 0;
    bool preWrite = false;
    bool complete = false;
    bool readyForIssue = false;
    bool fromACC = false;
    uint64_t srcAMaskR = 0;
    uint64_t srcAMaskC = 0;
    uint64_t srcBMaskR = 0;
    uint64_t srcBMaskC = 0;
    std::pair<uint64_t, uint64_t> srcA;
    std::pair<uint64_t, uint64_t> srcB;
    std::pair<uint64_t, uint64_t> srcC;
    std::pair<uint64_t, uint64_t> dst;
    bool srcValidA = false;
    bool srcValidB = false;
    bool srcValidC = false;
    bool allocL0A = false;
    bool allocL0B = false;
    bool allocLeftScale = false;
    bool allocRightScale = false;
    bool needAllocBufferC = false;
    bool allocBufferC = false;
    uint64_t l0ABufferIndex = 0;
    uint64_t l0BBufferIndex = 0;
    uint64_t leftScaleIndex = 0;
    uint64_t rightScaleIndex = 0;
    uint64_t BuffeCrIndex = 0;
    bool genL0ALoad = true;
    bool genL0BLoad = true;
    bool genL0CLoad = false;
    bool genLeftScaleLoad = true;
    bool genRightScaleLoad = true;
    bool srcReadyA = false;
    bool srcReadyB = false;
    bool srcReadyC = false;
    uint64_t endCycle = 0;
    uint64_t startCycle = 0;
    uint64_t issueCycle = 0;
    uint64_t loadGenCycle = 0;
    uint64_t renameCycle = 0;
    uint64_t waitingForLoadCycle = 0;
    uint64_t srcAReadyCycle = 0;
    uint64_t srcBReadyCycle = 0;
    uint64_t srcCReadyCycle = 0;
    uint64_t calcStartCycle = 0;
    uint64_t l0abRdCycle = 0;
    uint64_t bufcWrCycle = 0;
    uint64_t commitCycle = 0;
    uint64_t retireCycle = 0;
    uint64_t uopLoadCount = 0;
    uint64_t uopLoadGenCount = 0;
    bool stashEnable = false;
    uint32_t stashWayId = -1U;

    CubeUop();
    ~CubeUop();
};

class ACCCVTDependInfo {
public:
    std::pair<uint64_t, uint64_t> index;
    uint64_t row = 0;
    uint64_t col = 0;
    uint64_t tileOffset = 0;
    bool ready = false;
};
class ACCCVTUop {
public:
    bool srcRdy = false;
    uint64_t gIndex = 0;
    uint32_t bid = 0;
    uint64_t row = 0;
    std::vector<ACCCVTDependInfo> dependentTileIdx;
    std::vector<uint8_t> data;
    uint64_t offset = 0;
    uint64_t storeSize = 0;
    uint64_t ACCCVTStartCycle = 0;
    uint64_t ACCCVTRenameCycle = 0;
    uint64_t ACCCVTIssueCycle = 0;
    uint64_t ACCCVTArbCycle = 0;
    uint64_t ACCCVTWaitCycle = 0;
    uint64_t ACCCVTSrcRdyCycle = 0;
    uint64_t ACCCVTSrcDataCycle = 0;
    uint64_t ACCCVTFixPipeCycle = 0;
    uint64_t commitCycle = 0;
    uint32_t stid = -1U;
};

class SystolicArray {
public:
    const uint64_t delay = 7;
    bool working = false;
    uint64_t endCycle = 0;
};

class MatrixManager {
public:
    uint64_t bufferLoopMWidth = 8;
    uint64_t bufferLoopNWidth = 1;
    uint64_t tileDataSize = 512;
    uint64_t srcATileDataSize = 512;
    uint64_t srcBTileDataSize = 512;
    uint64_t tileSize = 16;
    uint64_t tileSizeM = 16;
    uint64_t tileSizeK = 0;
    uint64_t tileSizeN = 16;
    const uint64_t dataSize = 256;
    const uint64_t tileRegDataSize = 64;
    const uint64_t L0BufferMaxSize = 64 * 1024;
    bool renamed = false;
    bool renameACC = false;
    bool genUop = false;
    bool needRowMax = false;
    uint32_t dependBid;
    uint64_t totalUopCount = 0;
    uint64_t sendToCalcUopCount = 0;
    uint64_t retireUopCount = 0;
    uint64_t src0 = 0;
    DataType src0Type = DataType::INT8;
    uint64_t src1 = 0;
    DataType src1Type = DataType::INT8;
    uint64_t src2 = 0;
    DataType src2Type = DataType::INT8;
    uint64_t srcMXLeft = 0;
    uint64_t srcMXRight = 0;
    DataType mxType = DataType::INT8;
    uint64_t dst = 0;
    DataType dstType = DataType::FP32;
    uint64_t dst1 = 0;
    bool needAcc = false;
    bool needScale = false;
    bool needLoadTileC = false;
    bool toAcc = true;
    uint64_t m = 0;
    uint64_t k = 0;
    uint64_t n = 0;
    uint64_t mappingNum = 1;
    uint64_t srcElementWidth = 0;
    uint64_t srcRightElementWidth = 0;
    uint64_t dstElementWidth = 0;
    const uint64_t accElementWidth = 4;
    const DataType accType = DataType::FP32;
    uint64_t calculateStartCycle = 0;
    uint64_t issueToROBCycle = 0;
    uint64_t tileM = 0;
    uint64_t tileN = 0;
    uint64_t tileK = 0;
    uint64_t l0ASize = 0;
    uint64_t l0BSize = 0;
    uint64_t firstUopL0CwrCycle = 0;
    bool     firstTOCoreInit = false;
    uint64_t firstTMUToCoreCycle = 0;
    uint64_t toCoreCycle = 0;
    bool storeComplete = false;
    uint64_t totalStoreCount = 0;
    uint64_t alreadyStoreCount = 0;
    MMStage  stage = MMStage::IDLE;
    TileOp op = TileOp::TINVALID;
    uint64_t tileOpId = 0;
    ROBID bid = ROBID();
    uint64_t bpc = 0;
    uint32_t tag = 0;
    uint64_t ACC_flag = 0;
    uint32_t ACC_size = 0;
    BlockCommandPtr blockCmd = nullptr;
    LayOut ACCCVT_type = LayOut::NORM; // ACCCVT mode (support NORMAL/NZ2ND/NZ2DN)
    bool ACCCVT_canon = false; // ACCCVT mode (canon/normal)
    std::pair<uint64_t, uint64_t> loadRange = {0, 0};
    std::pair<uint64_t, uint64_t> mmadRange = {0, 0};
    std::pair<uint64_t, uint64_t> acccvtRange = {0, 0};

    void SetManagerState(BlockCommandPtr cmd, uint64_t setATileSizeRow = 0,
                         uint64_t setSrcATileDataSize = 0, uint64_t setSrcBTileDataSize = 0);

    void Reset()
    {
        retireUopCount = 0;
        totalUopCount = 0;
        firstUopL0CwrCycle = 0;
        stage = MMStage::IDLE;
        alreadyStoreCount = 0;
        totalStoreCount = 0;
        storeComplete = false;
        firstTOCoreInit = false;
        firstTMUToCoreCycle = 0;
    }

    std::string DumpSwimInfo() const
    {
        std::stringstream oss;
        oss << TASK_LABEL << bid;
        oss << blockCmd->Dump();
        return oss.str();
    }
};

class Core;

class TileLoadInfo {
public:
    uint32_t bid = 0;
    uint32_t stid = 0;
    uint64_t accFlg = 0;
    uint64_t tileOpIndex = 0;
    uint64_t offset = 0;
    L0Buffers* bufferPtr = nullptr;
    CubeTileRegLdReq req;
    std::shared_ptr<CubeUop> uop = nullptr;
    std::string bufferType = "none";
};

class L0CBuffer {
public:
    const uint64_t L0CBufferSize = 128 * 1024;
    bool valid = false;
    uint64_t shapeM = 0;
    uint64_t shapeN = 0;
    uint64_t writeUpBound = 0;
    uint64_t writeCount = 0;
    std::vector<uint8_t> buffer;
    std::map<uint64_t, std::pair<uint64_t, uint64_t>> tileOpAddrMap;
    L0CBuffer(): buffer(L0CBufferSize) {}
    void Init(MatrixManager& mm)
    {
        valid = true;
        shapeM = mm.m;
        shapeN = mm.n;
        writeUpBound = mm.tileM *  mm.tileN;
    }

    void Reset()
    {
        valid = false;
        shapeM = 0;
        shapeN = 0;
        writeUpBound = 0;
        writeCount = 0;
    }

    uint64_t Load(uint64_t offset, uint64_t elementwidth)
    {
        uint64_t res = 0;
        const uint64_t width = 8;
        for (size_t i = 0; i < elementwidth; i++) {
            res |= buffer[offset + i] << (width * i);
        }
        return res;
    }
};

class ChainInfo {
public:
    uint64_t renamedUopCount = 0;
    uint64_t chainTotalUopCount = 0;
    uint64_t issueUopCount = 0;
    uint64_t chainSize = 0;
    uint64_t chainLoadSendCount = 0;
    uint64_t remainLoadCount = 0;
    void Reset()
    {
        chainTotalUopCount = 0;
        issueUopCount = 0;
        chainLoadSendCount = 0;
        remainLoadCount = 0;
    }
};

class L0AllocInfo {
public:
    uint64_t bid = 0;
    uint64_t allocCycle = 0;
    uint64_t deAllocCycle = 0;
    std::pair<uint64_t, uint64_t> index;
};

struct ChainTileInfo {
    uint64_t bid = 0;
    uint32_t stid = 0;

    ChainTileInfo() {}
    ChainTileInfo(uint64_t bid, uint32_t stid) : bid(bid), stid(stid) {}
};

class CubeCore : public RingNodeObj {
public:
    bool perfectLoad = false;
    bool perfectScaleLoad = false;
    bool lastCycleCubeBusy = false;
    uint64_t cubeBusyStartCycle = 0;

    uint64_t coreIndex = 0;
    uint64_t setATileSizeRow = 16;
    uint64_t setATileSizeCol = 0;
    uint64_t setBTileSizeCol = 16;
    uint64_t setSrcATileDataSize = 512;
    uint64_t setSrcBTileDataSize = 512;
    const uint64_t mainChainSendLoadNum = 16;
    const uint64_t subChainSendLoadNum = 8;
    bool enableLRU = true;
    bool pipeViewVerbose = true;
    bool showTMUPipeView = true;
    bool printL0AllocInfo = false;
    uint64_t globleUopIndex = 0;
    Core *core = nullptr;
    const uint64_t maxChainNum = 2;
    uint64_t cubeChainSize = 16;
    uint64_t cubeL0Size = 64 * 1024;
    uint64_t cubeACCSize = 128 * 1024;
    const uint64_t sendReqNum = 1;
    const uint64_t sendUopNum = 1;
    const uint64_t ldRspHandleNum = 4;
    const uint64_t reqIdMaxNum = 16;
    const uint64_t dataSize = 256;
    uint64_t L0ASize = 128;
    uint64_t L0BSize = 128;
    uint64_t ACCSize = 128;
    const uint64_t bufferCCount = 8;
    uint64_t fifoMaxSize = 32;
    uint64_t tileLdFifoMaxSize = 64;
    bool limitSubChainIsqEntry = false;
    uint64_t subChainIsqEntryNum = 8;
    uint64_t ACCFifoMaxSize = 32;
    const uint64_t robMaxSize = 32;
    uint64_t uopExeCycle = 11;
    const uint64_t uopRdL0Cycle = 2;
    const uint64_t uopWrBufcCycle = 6;
    const uint64_t retireListSize = 256;
    bool rrPickFlg = false;
    int needReadRFArb = -1;
    int needWriteRFArb = -1;
    std::map<uint64_t, ChainInfo> chainInfo;
    explicit CubeCore():CubeBCCCreditQ(cubeThreadNum),
                        CubeTileLdReqQ(cubeThreadNum),
                        TileCubeLdResQ(cubeThreadNum),
                        TileRegCubeLdRetryQ(cubeThreadNum),
                        CubeTileStReqQ(cubeThreadNum),
                        TileRegCubeStRetryQ(cubeThreadNum),
                        BCCCubeFlushQ(cubeThreadNum),
                        cubeStateMachine(cubeThreadNum),
                        threadWorkingCycle(cubeThreadNum, 0),
                        uopExeFifo(cubeThreadNum),
                        loadIdPool(cubeThreadNum),
                        usedLoadIdPool(cubeThreadNum),
                        residualLoadIdPool(cubeThreadNum),
                        storeIdPool(cubeThreadNum),
                        tileLoadReqBuffer(cubeThreadNum),
                        tileStoreReqBuffer(cubeThreadNum),
                        systolicArrays(cubeThreadNum),
                        L0A(cubeThreadNum, std::vector<L0Buffers>(L0ASize)),
                        L0B(cubeThreadNum, std::vector<L0Buffers>(L0BSize)),
                        leftScaleBuffer(8),
                        rightScaleBuffer(8),
                        BufferC(cubeThreadNum, std::vector<L0Buffers>(bufferCCount)),
                        tmpBufferC(cubeThreadNum),
                        L0C(cubeThreadNum),
                        ACC(ACCSize),
                        TileLoadReqIdMap(cubeThreadNum) {};
    virtual ~CubeCore();

    // IO ports
    INPUT SimQueue<BlockCommandPtr>*  bccCubeBlockCmdQ;
    OUTPUT SimQueue<BlockCommandPtr>* cubeBCCWakeupQ;

    OUTPUT                  std::vector<SimQueue<Credit>*> CubeBCCCreditQ;
    INPUT                   std::vector<SimQueue<CubeTileRegLdReq>*> CubeTileLdReqQ;
    OUTPUT                  std::vector<SimQueue<TileRegCubeLdRes>*> TileCubeLdResQ;
    INPUT                   std::vector<SimQueue<TileRegVecLdRetry>*> TileRegCubeLdRetryQ;
    INPUT                   std::vector<SimQueue<CubeTileRegStReq>*> CubeTileStReqQ;
    INPUT                   std::vector<SimQueue<TileRegCubeStRetry>*> TileRegCubeStRetryQ;
    INPUT                   std::vector<SimQueue<BCCCubeFlush>*> BCCCubeFlushQ;
    INPUT                   SimQueue<TileRegCubeLdRes>* tileRegCubeWrResQ;

    // inner simqueue

    SimSys                              *GetSim();
    void                                Build();
    void                                Reset() final;
    void                                Work() final;
    void                                Xfer() final;
    void ReportStat() override;
    bool                                     oooIssue = true;
    bool                                     cubeDataCheck = false;
    CUBEConfig                               cfgs;
    std::shared_ptr<CubeCoreStats>           cubeStats;
    std::vector<std::shared_ptr<CubeCore>>*       cubeCores = nullptr;
    std::shared_ptr<std::map<uint64_t, std::map<uint32_t, std::vector<uint64_t>>>> mutiCoreChainInfo;
    std::shared_ptr<DebugLog>                debugLogger = nullptr;
    CubeRobInfo                              preRobInfo;
    MatrixManager                            MM;
    std::vector<SimQueue<CubeRobInfo>>            cubeROB;
    std::deque<ChainTileInfo>                     exeTileOpChain;
    std::vector<std::deque<uint64_t>>             extantAccChainId;
    std::vector<std::map<uint64_t, SimQueue<CubeRobInfo>>>     chainROB;
    std::vector<MatrixManager>                    cubeStateMachine;
    uint64_t                                 accNum = 0;
    uint64_t                                 l0AEntryUseNum = 0;
    uint64_t                                 l0BEntryUseNum = 0;
    uint64_t                                 AACCELntryUseNum = 0;
    double                                   preCubeUsage = 0;
    double                                   cubeUsage = 0;
    std::vector<uint64_t>                         threadWorkingCycle;
    std::vector<std::map<uint64_t, SimQueue<CubeUopPtr>>>  CubeUopBuffer;
    SimQueue<CubeUopPtr>      CubeUopFifo;
    std::vector<std::map<uint64_t, SimQueue<CubeUopPtr>>>  TileLdUopBuffer;
    SimQueue<CubeUopPtr>      tileLdUopFifo;
    SimQueue<std::shared_ptr<ACCCVTUop>>          ACCCVTUopBuffer;
    SimQueue<std::shared_ptr<ACCCVTUop>>          ACCCVTUopFifo;
    SimQueue<std::shared_ptr<ACCCVTUop>>          FixPipeFifo;
    std::vector<SimQueue<CubeUopPtr>>             uopExeFifo;
    std::vector<std::deque<uint64_t>>                  loadIdPool;
    std::vector<std::deque<std::pair<uint64_t, ChainTileInfo>>>    usedLoadIdPool;
    std::vector<std::deque<std::pair<uint64_t, ChainTileInfo>>>    residualLoadIdPool;
    std::vector<std::deque<uint64_t>>                  storeIdPool;
    std::vector<SimQueue<TileLoadInfo>>           tileLoadReqBuffer;
    std::vector<SimQueue<CubeTileRegStReq>>       tileStoreReqBuffer;
    std::vector<SystolicArray>                    systolicArrays;
    std::vector<std::vector<L0Buffers>>                L0A;
    std::map<uint64_t, L0AllocInfo>               l0aAllocInfo;
    std::vector<uint64_t>                         l0aLru;
    std::vector<std::vector<L0Buffers>>                L0B;
    std::map<uint64_t, L0AllocInfo>               l0bAllocInfo;
    std::vector<uint64_t>                         l0bLru;
    std::vector<L0Buffers>                        leftScaleBuffer;
    std::vector<uint64_t>                         leftScaleLru;
    std::vector<L0Buffers>                        rightScaleBuffer;
    std::vector<uint64_t>                         rightScaleLru;
    std::vector<std::vector<L0Buffers>>                BufferC;
    bool                                     freeAndAllocBufferC = false;
    uint64_t                                 freeAndAllocIndex = -1;
    std::vector<L0Buffers>                        tmpBufferC;
    std::vector<L0CBuffer>                        L0C;
    std::vector<L0Buffers>                        ACC;
    uint64_t                                 cubeResaveReqCount = 0;
    uint64_t                                 cubeRsvlCount = 0;
    std::vector<std::map<uint64_t, TileLoadInfo>>      TileLoadReqIdMap;
    std::vector<std::map<uint64_t, std::map<std::shared_ptr<CubeUop>, std::shared_ptr<CubeUop>>>> globalUopMap;
    std::vector<std::map<uint32_t, MatrixManager>>     bidCubeSMMap;
    std::vector<std::map<uint32_t, L0CRenameInfo>>             l0cRenameInfoMap;
    std::vector<std::map<uint32_t, std::vector<uint64_t>>>          cubeVerifyDataMap;
    std::vector<std::map<uint64_t, L0CRenameInfo>>             l0cRenameAccTagMap;
    std::vector<CubeRobInfo>                      retireList;
    std::deque<uint64_t>                     blockCmdBufferIDPool;
    std::vector<std::map<uint64_t, RowMaxBuffer>> rowMaxBufferMap;
    void               HandleTileLdRes();
    bool               IsResidualLoad(uint64_t threadId, uint64_t reqId, uint64_t bid, uint32_t stid);
    void               UpdataResidualLdPool(uint64_t threadId, uint64_t reqId, uint64_t bid, uint32_t stid);
    void               UpdataUsedLdPool(uint64_t threadId, uint64_t reqId, uint64_t bid, uint32_t stid);
    void               SetLoopMWidth(uint64_t bid, uint32_t stid);
    void               GenUop2Buffer(uint64_t bid, uint32_t stid);
    void               GenInnerUop(uint64_t bid, size_t i, size_t j,
                                   std::vector<std::shared_ptr<CubeUop>>& headUop, uint64_t& index, uint32_t stid);
    void               GenLoopKUop(uint64_t bid, size_t i, size_t j, size_t k,
                                   std::vector<std::shared_ptr<CubeUop>>& headUop, uint64_t& index, uint32_t stid);
    void               SendUop2Fifo();
    void               SendTileLdUop();
    bool               SendTileLdUopByBid(uint64_t bid, uint32_t stid);
    void               SendUopThreadWork(uint64_t thread);
    bool               SendUopChainBid(uint64_t bid, uint32_t stid);
    void               UopFifoWork();
    void               TileLdRename();
    bool               TileLdRenameAB(uint64_t threadId, CubeUopPtr uop, bool preRenameA, bool preRenameB,
                                      bool preRenameLeftScale, bool preRenameRightScale);
    bool               UopOOOIssue();
    void               PickMainChainUop();
    void               GenUopLoadReq(uint64_t threadId, std::shared_ptr<CubeUop> uop);
    uint64_t           GetLdId(uint64_t threadId, uint64_t bid, uint32_t stid);
    uint64_t           GetStId(uint64_t threadId);
    bool               FindSameATile(uint64_t threadId, std::shared_ptr<CubeUop> uop);
    bool               FindSameBTile(uint64_t threadId, std::shared_ptr<CubeUop> uop);
    bool               FindSameLetfScaleTile(std::shared_ptr<CubeUop> uop);
    bool               FindSameRightScaleTile(std::shared_ptr<CubeUop> uop);
    bool               HitL0A(uint64_t threadId, CubeUopPtr uop);
    bool               HitL0B(uint64_t threadId, CubeUopPtr uop);
    void               UpdateL0ALRU(uint64_t id);
    void               UpdateL0BLRU(uint64_t id);
    void               UpdateLeftScaleLRU(uint64_t id);
    void               UpdateRightScaleLRU(uint64_t id);
    bool               CanAllocL0A(uint64_t threadId);
    bool               CanAllocL0B(uint64_t threadId);
    bool               CanAllocLeftScale();
    bool               CanAllocRightScale();
    bool               CanAllocBufferC(uint64_t threadId);
    bool               CheckAlloc(std::shared_ptr<CubeUop> uop);
    void               AllocL0A(uint64_t threadId, std::shared_ptr<CubeUop> uop);
    void               AllocL0B(uint64_t threadId, std::shared_ptr<CubeUop> uop);
    void               AllocLeftScale(std::shared_ptr<CubeUop> uop);
    void               AllocRightScale(std::shared_ptr<CubeUop> uop);
    void               SetL0BEntryMapping(std::shared_ptr<CubeUop> uop, uint64_t l0bIndex);
    void               SetLeftScaleEntryMapping(std::shared_ptr<CubeUop> uop, uint64_t leftScaleIndex);
    void               SetRightScaleEntryMapping(std::shared_ptr<CubeUop> uop, uint64_t rightScaleIndex);
    void               AllocBufferC(uint64_t threadId, std::shared_ptr<CubeUop> uop);
    void               FreeL0BufferEntry(std::shared_ptr<CubeUop> uop);
    void               PreWeakUpBufferc(std::shared_ptr<CubeUop> uop);
    void               AllocAndRenameC(uint64_t threadId, std::shared_ptr<CubeUop> uop);
    void               AllocAndRename(uint64_t threadId, std::shared_ptr<CubeUop> uop);
    void               FreeL0A(uint64_t threadId, std::shared_ptr<CubeUop> uop);
    void               LRUFreeL0A(uint64_t threadId, std::shared_ptr<CubeUop> uop);
    void               FreeL0ANormal(uint64_t threadId, std::shared_ptr<CubeUop> uop);
    void               FreeL0B(uint64_t threadId, std::shared_ptr<CubeUop> uop);
    void               LRUFreeL0B(uint64_t threadId, std::shared_ptr<CubeUop> uop);
    void               FreeL0BNormal(uint64_t threadId, std::shared_ptr<CubeUop> uop);
    void               FreeLeftScaleBuffer(std::shared_ptr<CubeUop> uop);
    void               FreeRightScaleBuffer(std::shared_ptr<CubeUop> uop);
    void               GenL0ALoadReq(uint64_t threadId, std::shared_ptr<CubeUop> uop);
    void               GenL0BLoadReq(uint64_t threadId, std::shared_ptr<CubeUop> uop);
    void               GenLeftScaleLoadReq(std::shared_ptr<CubeUop> uop);
    void               GenRightScaleLoadReq(std::shared_ptr<CubeUop> uop);
    void               GenBufferCLoadReq(uint64_t threadId, std::shared_ptr<CubeUop> uop);
    uint64_t           GetTileAAddr(std::shared_ptr<CubeUop> uop);
    uint64_t           GetTileBAddr(std::shared_ptr<CubeUop> uop);
    void               GetACCTileData(std::shared_ptr<CubeUop> uop);
    void               MoveData2L0Buffer(TileLoadInfo loadInfo, uint8_t* dataPtr);
    void               SendTileLoad2TileReg();
    void               L0BufferWork();
    bool               CheckUopReady(uint64_t threadId, std::shared_ptr<CubeUop> uop);
    bool               CheckSrcAReady(uint64_t threadId, uint64_t bufferIndex);
    bool               CheckSrcBReady(uint64_t threadId, uint64_t bufferIndex);
    bool               CheckSrcCReady(uint64_t threadId, std::shared_ptr<CubeUop>);
    void               SendL0CStReq2TileReg();
    void               SetFlush(FlushBus &signal);
    void               BlockFlush(CubeRobInfo blkInfo);
    void               FlushDeallocACC(CubeRobInfo blkInfo, uint32_t stid);
    void               FlushUopBuffer(uint64_t bid, uint32_t stid);
    void               FlushTileLdUopBuffer(uint64_t bid, uint32_t stid);
    void               FlushLoadReq(uint64_t bid, uint32_t stid);
    void               FlushUopFifo(uint64_t bid, uint32_t stid);
    void               ISQRollBackL0EntryStat(std::shared_ptr<CubeUop> uop);
    void               CubeUnitRollBackL0EntryStat(const std::shared_ptr<CubeUop>& uop);
    void               FlushUopExeFifo(uint64_t bid, uint32_t stid);
    void               FilterFlushedLoadRes(uint64_t bid, uint32_t stid);
    bool               NeedFlush(ROBID bid, CubeRobInfo cubeRobInfo, uint32_t stid) const;
    void               ExeFifoWork();
    void               ExeFifoThreadWork(uint64_t threadId);
    void               AllocL0C(uint64_t threadId);
    void               FreeL0AL0B();
    void               CalcTileMul(uint64_t threadId, std::shared_ptr<CubeUop> uop);
    void               MaskLeftTile(std::shared_ptr<CubeUop> uop, std::vector<std::vector<uint64_t>>& tileA);
    void               MaskRightTile(std::shared_ptr<CubeUop> uop, std::vector<std::vector<uint64_t>>& tileB);
    void               WriteBufferC(uint64_t threadId, std::shared_ptr<CubeUop> uop, std::vector<std::vector<uint64_t>>& tileC);
    void               PreWriteACC(uint64_t threadId, L0Buffers& tileC);
    void               PaddingTile(std::vector<std::vector<uint64_t>>& targetVector, L0Buffers& srcBuffer,
                                   DataType datatype, TileFormat format);
    std::vector<uint64_t>   GenCubeVerifyData(uint64_t bid);
    void               GenTileVerifyData(L0Buffers& bufferC, uint32_t stid);
    void               PrintRingPkt(std::string stage, std::shared_ptr<Request> const &pkt);
    void               PrintPipeViewLog(std::shared_ptr<CubeUop> uop, uint64_t threadId);
    void               PrintACCCVTPipeView(std::shared_ptr<ACCCVTUop> uop);
    void               PrintCubeUopPipeView(std::shared_ptr<CubeUop> uop);
    void               HandleCubeStResp();
    void               PushDFX(uint64_t threadId, uint32_t stid);
    void               FetchReq2ROB();
    void               ROBWork();
    void               ChainRobRename(uint64_t accChainId, uint32_t stid);
    void               ROBRename(CubeRobInfo& info);
    void               AllocACC(CubeRobInfo& info);
    void               SetACCToZero(CubeRobInfo& info);
    void               FreeACC(MatrixManager& mm);
    void               RenameACC(CubeRobInfo& info);
    void               FindFreeACC(uint64_t i, uint64_t j, uint32_t bid, uint32_t stid);
    void               GenUop(CubeRobInfo& info);
    void               GenACCCVTUop(uint64_t bid, uint32_t stid);
    void               GenACCCVTUopNormal(uint64_t bid, uint64_t& gIndex, uint32_t stid);
    void               GenACCCVTUopNZ2ND(uint64_t bid, uint64_t& gIndex, uint32_t stid);
    void               FindDependTileNZ2ND(uint64_t tileM, uint64_t tileN, uint64_t row, std::shared_ptr<ACCCVTUop> uop,
                            uint32_t stid);
    void               FindDependTileNZ2DN(uint64_t tileM, uint64_t tileN, uint64_t col, std::shared_ptr<ACCCVTUop> uop,
                            uint32_t stid);
    void               GenACCCVTUopNZ2DN(uint64_t bid, uint64_t& gIndex, uint32_t stid);
    void               SendACCCVTUop2Fifo();
    void               ACCCVTUopWork();
    void               FixPipeWork();
    bool               CheckACCCVTSrcReady(std::shared_ptr<ACCCVTUop> uop);
    void               GenACCCVTStore(std::shared_ptr<ACCCVTUop> uop);
    void               TileOpRslv(uint64_t accFlg, uint32_t stid);
    void               GenACCCVTData(uint8_t* dataPtr, std::shared_ptr<ACCCVTUop> uop);
    void               GenACCCVTDataNormal(uint8_t* dataPtr, std::shared_ptr<ACCCVTUop> uop);
    void               GenACCCVTDataNZ2ND(uint8_t* dataPtr, std::shared_ptr<ACCCVTUop> uop);
    void               GenACCCVTDataNZ2DN(uint8_t* dataPtr, std::shared_ptr<ACCCVTUop> uop);
    bool               IsCubeBusy();
    void               CheckBusy();
    bool               CheckLoadBusy() const;
    TileLoadInfo       PickLoadReq();
    bool               PickLoadInfo(TileLoadInfo& info);
    void               PrintL0AAllocInfo();
    void               PrintL0BAllocInfo();
    bool               CanGetBlkCmdFromBissue(BlockCommandPtr cmd);
    bool               FindExtantChainId(std::vector<uint64_t> chainIdArray, BlockCommandPtr cmd) const;
    void               InitRowMaxBuffer(uint64_t bid, uint32_t stid);
    void               GenRowMaxStore();
    void               GenRowMaxStReq(uint64_t bid, uint32_t stid);
    void               CubeCoresFetchBlockCmd() const;
    uint64_t           CalcOtherChainIsqUsed(uint64_t flg);
    void               SetSwimLaneStartCycle(uint64_t bid, uint32_t stid);
    void               SetTileOpLdStartCycle(uint64_t bid, uint32_t stid);
    void               SetAcccvtStartCycle(uint64_t bid, uint32_t stid);
    void               PickTileOp();
    void               PickExeTileOp();
    bool               IsExeTileOpSendAllLd();
    bool               IsFP4(DataType type) const;
    bool               IsDataTypeDouble(DataType type) const;
};

std::ostream& operator<<(std::ostream& out, CubeUop &uop);
std::ostream& operator<<(std::ostream& out, L0Buffers &buffer);

} // namespace JCore

#endif
