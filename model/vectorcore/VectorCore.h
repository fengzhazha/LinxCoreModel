#ifndef   VECTOR_CORE_H
#define  VECTOR_CORE_H
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "../configs/vectorcore_config.h"
#include "../tools/trace_logger/TraceLogger.h"
#include "core/Bus.h"
#include "core/interface.h"
#include "iex/vec_iex.h"
#include "interface/InterfaceCommon.h"
#include "ISA.h"
#include "tmu/ring/RingNodeObj.h"
#include "vectorcore/vpe/VecPE.h"
#include "plat/DebugLog.h"
#include "tmu/ring/CrossStation.h"
#include "lsu/lsu_stats.h"
#include "vectorcore/GroupBuffer.h"
#include "vectorcore/VectorCoreStats.h"
#include "vectorcore/VectorCoreTA.h"
#include "vectorcore/VectorCT.h"
#include "vectorcore/VectorLoopManager.h"
#include "vectorcore/GroupScheduler.h"
#include "vectorcore/GROB.h"
#include "iex/iex.h"
#include "ModelCommon/bus/VectorBridgeBus.h"

namespace JCore {

const uint32_t VECTOR_WL_NUMBER = 4;
const uint32_t VECTOR_PE_NUM = 4;

enum class VecRequestStatus {
    INVALID,
    PENDING,
    WAIT_SEND,
    WAIT_DATA,
    SUACCELSS
};

enum class VecBlockType {
    TILEOP,
    BLOCKT,
};

enum class VecElementType {
    UINT8,
    UINT16,
    UINT32,
    UINT64,
};

class VecRequest {
public:
    VecRequestStatus status = VecRequestStatus::INVALID;
    bool valid = false;
    uint32_t cycle;
    uint32_t src;
    uint32_t size;
    std::vector<uint8_t> data;
    VecRequest() = default;
    VecRequest(uint32_t src, uint32_t size, uint32_t cycle)
        : status(VecRequestStatus::PENDING), cycle(cycle), src(src), size(size) {}
};

class VecOp {
public:
    std::vector<int> loadRequestId;
    std::vector<int> storeRequestId;
    bool executeComplete = false;
    ROBID bid;
    uint32_t tag = 0;
    uint32_t dst;
    uint32_t src0;
    uint32_t src1;
    uint32_t width;
    uint64_t shapeSize;
    TileOp opcode;

    /* Using for trace-PostSimulator */
    Tid tid;
    uint64_t tileOpId;
    uint64_t startCycle;
    uint64_t endCycle;
};

struct MissMemReq {
    MemRequest req;
    std::vector<uint64_t> lanes;
};
struct TileSrcAddr {
    ROBID bid;
    uint32_t startAddr;
    uint32_t size;

    TileSrcAddr(ROBID b, uint32_t addr, uint32_t s)
        : bid(b), startAddr(addr), size(s) {}
};
class Core;
class VectorTop;
class VectorCore : public RingNodeObj {
public:
    uint64_t groupMachineId = 0;
    bool lastCycleVectorBusy = false;
    uint64_t vectorBusyStartCycle = 0;

    VectorTop* top;
    Core *core = nullptr;
    explicit VectorCore(uint32_t coreId);
    virtual ~VectorCore();
    void Work() override;
    void Xfer() override;
    void Reset() override;
    SimSys *GetSim() override;
    void Build();
    void Report_load_latency();
    void ReportStat();
    const VectorCoreConfig& GetConfig() const;

    uint64_t GetRequestCount(uint32_t shapeSize, uint32_t elementSize);
    void HandleBCCRequst();
    void HandleBCCResolve();
    void SplitDataToGenLdRequest(VecOp& v, uint32_t shapeSize, uint32_t elementSize, uint32_t src);
    uint32_t GenTileRegLdRequest(uint16_t src, uint32_t size);
    uint32_t GenTileRegStRequest(uint32_t size, uint32_t dest, uint8_t *data);
    void HandleMemRequest();
    void HandleTileRegLdResponse();
    void HandleTileRegLdRetry();
    void HandleTileRegStRetry();
    void HandleTileRegStComplete();
    void Execute();
    void ExecuteOneOp(VecOp& v);
    void ExecuteTwoTileSourceOp(VecOp& v, std::function<uint64_t(uint64_t, uint64_t)> func);
    void ExecuteOneTileSourceOp(VecOp& v, std::function<uint64_t(uint64_t)> func);
    void Flush();
    void Replay(FlushBus& bus);
    void SetFlush(FlushBus &bus);
    void ResolveBlock();
    // 获取连接信息
    void TxTileReq();

    void InitVectorLoopManager();
    void InitGroupScheduler();
    bool GenVecMemLdReq(MemRequest req);
    bool GenVecMemStReq(MemRequest req);
    void HandleMemLdResp();
    void HandleMemStResp();
    void InitVectorGBuffer();
    void InitVectorGROB();
    void InitVectorTA();
    void BuildSIMTIEX();
    void ConnectSIMTIEXIntf();

    void HandleTileRegLoadRes();
    void HandleTileLoadRetry();
    void HandleTileRegStoreRetry();
    void HandleTileRegStoreComplete();
    void UpdateLC();
    void HandleTaMiss(const std::vector<uint64_t>& addrs, MemRequest& req);
    void HandleStoreMiss(const std::vector<uint64_t>& addrs, MemRequest& req);
    void HandleMissReturn();
    void HandleDataRet(MissMemReq& missReq);
    void HandleDataRet(MemRequest& missReq);
    void HandleStoreMissReturn();
    ROBID GetOldestBid(uint32_t stid);
    ROBID GetOldestGid(uint32_t stid);

    void PrintCfg();
    void Dump();
    bool IsVecBusy();
    void CheckBusy();

    void InsertBlockCmdBuffer(BlockCommandPtr cmd);
    void DeallocBlockCmdBuffer(BlockCommandPtr grobCmd);
    void ReportIEXStats();
    void ResetStats();
    uint64_t IexLdPipeCount();
    std::shared_ptr<IEX> GetIex();
    std::shared_ptr<VecPE> GetPE();
    void BuildPE();
    void SetPE(std::shared_ptr<VecPE> pe);
    void ReportVectorKeyStat();
    void ReportVectorTopDown();
    void BuildSrf();
    void ConnectSRFIntf();
    void BuildVrf();
    void RptVecRenameStats();
    uint32_t GetSelectCore() const;
    void ReportLoadPortUse(uint64_t vecBusyCycle);
    void ReportPEStat();
    uint32_t GetIdleSpace() const;
    void RegisterTileAddr(ROBID bid, uint32_t stid, uint32_t addr, uint32_t size);
    bool IsInAddrRange(ROBID bid, uint32_t stid, uint32_t addr);
    bool DeleteBidEntry(ROBID bid, uint32_t stid);
    void LoadWakeUp(MemRequest &req);

public:
    INPUT SimQueue<BlockCommandPtr>*                            bccVecBlockCmdQ;
    OUTPUT SimQueue<BlockCommandPtr>*                           vecBCCWakeupQ;
    INPUT SimQueue<BlockCommandPtr>*                            bccMCallBlockCmdQ;
    OUTPUT SimQueue<BlockCommandPtr>*                           vecMCallWakeupQ;

    INPUT SimQueue<VectorBridgeRsp>*                            vecBridgeRspQ;
    INPUT SimQueue<VectorBridgeReq>*                            vecBridgeReqQ;

    INPUT SimQueue<TileOperandPtr>               *toClearQ;

    INPUT  SimQueue<BCCVecFlush>                 *m_bccVectorFlushQ;
    INPUT  SimQueue<TileRegVecLdRes>             *m_tileRegVecLdResQ;
    INPUT  SimQueue<TileRegVecLdRes>             *m_tileRegVecWrResQ;
    INPUT  SimQueue<TileRegVecLdRetry>           *m_tileRegVecLdRetryQ;
    INPUT  SimQueue<TileRegVecStRetry>           *m_tileRegVecStRetryQ;
    OUTPUT SimQueue<Credit>                      *m_vecBCCCreditQ;
    OUTPUT SimQueue<VecTileRegLdReq>             *m_vecTileRegLdReqQ;
    OUTPUT SimQueue<VecTileRegStReq>             *m_vecTileRegStReqQ;

    /* Using for GROB */
    INNER  ThdRingQ<BlkBodyFetchState>          *m_peIfuQ;
    INNER  std::vector<RingQueue<BlockRunInfo>*> m_workLoadQ;
    INNER  SimQueue<VCore::GBufferAllocReq>     m_lm2GBufferQ;
    INNER  SimQueue<GroupAllocInfo>             m_lm2GROB;
    INNER  SimQueue<GroupIssueInfo>             m_GBuffer2GROB;
    INNER  SimQueue<MaskReleaseInfo>            m_GROB2MaskFile;
    INNER  std::vector<SimQueue<ScbDrainBus>>   m_scbDrainBusQ;
    INNER  std::vector<SimQueue<ScbDrainBus>>   m_scbDrainDoneBusQ;
    // TODO: STID
    INNER  SimQueue<ScbDrainBus>                m_storeDoneBusQ;

    // Store ID 与 Cycle 的映射对，用来清除 store Req
    INNER  std::unordered_map<uint32_t, uint32_t> m_storeCycles;
    // 记录 addr 与对应的resp的信息
    // 0x100 -> [Req0, Req1, ...]
    INNER  std::unordered_map<uint32_t, std::vector<MissMemReq>> m_vecLoad;
    INNER  std::unordered_map<uint32_t, bool> m_vecRet;

    INNER  std::unordered_map<uint32_t, MemRequest> m_storeMiss;

    VectorCoreStats                              m_stats;
    std::shared_ptr<LGPRRF>                      m_lgprRF;
    std::shared_ptr<VRFRename>                   m_vrfRename;

    /* TODO: 临时使用，用于调试功能接口 */
    INNER  std::vector<VecRequest>               m_ldReqArray;
    uint32_t                                     ldArraySize = 0;
    INNER  std::vector<VecRequest>               m_stReqArray;
    uint32_t                                     stArraySize = 0;
    INNER  std::deque<VecOp>                     m_vecOpQ;
    INNER  std::vector<uint8_t>                  m_laBuffer;

    INNER  SimQueue<MemRequest>*            taLoadQ;
    INNER  SimQueue<MemRequest>*            taStoreQ;

    CoreInterface                           coreInterface;
    std::vector<uint8_t>                    m_taBuffer;
    std::vector<bool>                       m_taVld;
    INPUT  SimQueue<MemRequest>             *iexVcoreReqQ;
    INNER  SimQueue<MemRequest>             m_memReqQ;
    bool                                    *resolveBlock;
    OUTPUT SimQueue<MemRequest>             *vcoreIexResQ;
    OUTPUT SimQueue<MemRequest>             *vcoreIexLdResQ;
    OUTPUT SimQueue<MemRequest>             *vcoreIexStResQ;
    INPUT  SimQueue<LdNonFlushBus>          *loadNonFlushQ;
    INPUT  SimQueue<StNonFlushBus>          *storeNonFlushQ;

    OUTPUT std::shared_ptr<SimQueue<MemRequest>>        m_vecBridgeMemReqQ;
    INPUT  std::shared_ptr<SimQueue<MemRequest>>        m_bridgeVecLdRetQ;
    INPUT  std::shared_ptr<SimQueue<MemRequest>>        m_bridgeVecStRslvQ;

    std::shared_ptr<VectorLoopManager>      m_vectorLM;
    std::shared_ptr<GroupScheduler>         m_vectorGS;
    bool                                    m_memStall = false;
    MemRequest                              curReq;
    std::vector<MemRequest>                 m_memReqArray;
    uint32_t                                m_memReqId = 1000;
    const uint32_t                          m_memReqArraySize = 1000;
    uint32_t                                m_coreId = 0;
    const uint64_t                          align64Mask = 0x3f;
    const uint64_t                          cacheLineSize = 64;
    unsigned                                ldRingport0Id = -1;
    OUTPUT SimQueue<TTransMemLdReq>         *m_vecMemLdReqQ;
    INNER SimQueue<MemTTransLdRes>          *m_vecMemLdResQ;
    OUTPUT SimQueue<TTransMemStReq>         *m_vecMemStReqQ;
    INNER SimQueue<MemTTransStRes>          *m_vecMemStResQ;
    std::shared_ptr<GroupBuffer>            m_gBuffer;
    std::shared_ptr<GROB>                   m_grob;
    VectorCoreConfig                        config;
    void                                    SetVerboseOn();
    std::shared_ptr<VectorCoreTA>           m_vecta;
    std::map<uint64_t, BlockCommandPtr>     m_blockCmdBuffer;
    std::vector<uint64_t>                   m_blockCmdBufferIDPool;
    void                                    FlushCmdBuffer(FlushBus& bus);
private:
    void DoCommonFlush(FlushBus& bus);

    bool                                    verbose = false;
    std::shared_ptr<VectorCT>               m_vectorCT;
    // vector core tile aaccelss unit
    std::shared_ptr<IEX>                    m_iex;
    std::shared_ptr<VecPE>                  m_pe;
    TMUConfig                               ubConfig;
    uint32_t                                logicalGroups = 0;
    /* TODO: 用 configs 定义 */
    constexpr static uint32_t                                         M_LANENUM = 64;
    constexpr static uint32_t                                         M_BASEDATASIZE = M_LANENUM * sizeof(uint8_t);
    constexpr static uint32_t                                         M_DATA_UNIT_SIZE_NUM = 4;
    static std::array<int, M_DATA_UNIT_SIZE_NUM>                      gDataUnitSizeArray;
    std::vector<std::map<ROBID, std::vector<TileSrcAddr>>>            srcAddrMap;
};

} // namespace JCore

#endif
