#ifndef  MEMORY_CORE_H
#define  MEMORY_CORE_H

#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "../configs/mtccore_config.h"
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
#include "vectorcore/GROB.h"
#include "mtccore/memcore/MemoryCoreStats.h"
#include "mtccore/memcore/MemoryCoreTA.h"
#include "mtccore/memcore/MemoryCT.h"
#include "mtccore/memcore/MemoryLoopManager.h"
#include "vectorcore/GroupBuffer.h"

namespace JCore {

const uint32_t MTC_THD_NUMBER = 4;
const uint32_t MTC_PE_NUM = 4;

enum class MtcRequestStatus {
    INVALID,
    PENDING,
    WAIT_SEND,
    WAIT_DATA,
    SUACCELSS
};

enum class MtcBlockType {
    TILEOP,
    BLOCKT,
};

enum class MtcElementType {
    UINT8,
    UINT16,
    UINT32,
    UINT64,
};

class MtcRequest {
public:
    MtcRequestStatus status = MtcRequestStatus::INVALID;
    bool valid = false;
    uint32_t cycle;
    uint32_t src;
    uint32_t size;
    std::vector<uint8_t> data;
    MtcRequest() = default;
    MtcRequest(uint32_t src, uint32_t size, uint32_t cycle)
        : status(MtcRequestStatus::PENDING), cycle(cycle), src(src), size(size) {}
};

class MtcOp {
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

struct MtcMissMemReq {
    MemRequest req;
    std::vector<uint64_t> lanes;
};

class Core;
class FakeLsu;

class MemoryCore : public RingNodeObj {
public:
    Core *core = nullptr;
    MemoryCore();
    virtual ~MemoryCore();
    void Work() override;
    void Xfer() override;
    void Reset() override;
    SimSys *GetSim() override;
    void Build();
    void Report_load_latency();
    void ReportStat();
    const MtcCoreConfig& GetConfig() const;

    uint64_t GetRequestCount(uint32_t shapeSize, uint32_t elementSize);
    void HandleBCCRequst();
    void HandleBCCResolve();
    void SplitDataToGenLdRequest(MtcOp& v, uint32_t shapeSize, uint32_t elementSize, uint32_t src);
    uint32_t GenTileRegLdRequest(uint16_t src, uint32_t size);
    uint32_t GenTileRegStRequest(uint32_t size, uint32_t dest, uint8_t *data);
    void HandleMemRequest();
    void HandleTileRegLdResponse();
    void HandleTileRegLdRetry();
    void HandleTileRegStRetry();
    void HandleTileRegStComplete();
    void Execute();
    void ExecuteOneOp(MtcOp& v);
    void ExecuteTwoTileSourceOp(MtcOp& v, std::function<uint64_t(uint64_t, uint64_t)> func);
    void ExecuteOneTileSourceOp(MtcOp& v, std::function<uint64_t(uint64_t)> func);
    void Flush();
    void Replay(FlushBus& bus);
    void SetFlush(FlushBus &bus);
    void ResolveBlock();
    // 获取连接信息
    void TxTileReq();

    void InitMemoryLoopManager();
    bool GenVecMemLdReq(MemRequest req);
    bool GenVecMemStReq(MemRequest req);
    void HandleMemLdResp();
    void HandleMemStResp();
    void InitMemoryGBuffer();
    void InitMemoryGROB();
    void InitMemoryTA();

    void HandleTileRegLoadRes();
    void HandleTileLoadRetry();
    void HandleTileRegStoreRetry();
    void HandleTileRegStoreComplete();
    void UpdateLC();
    void HandleTaMiss(const std::vector<uint64_t>& addrs, MemRequest& req);
    void HandleStoreMiss(const std::vector<uint64_t>& addrs, MemRequest& req);
    void HandleMissReturn();
    void HandleDataRet(MtcMissMemReq& missReq);
    void HandleDataRet(MemRequest& missReq);
    void HandleStoreMissReturn();
    ROBID GetOldestGid();

    void PrintCfg();
    void Dump();
    bool IsMtcBusy();

public:
    INPUT SimQueue<BlockCommandPtr>*             bccMtcBlockCmdQ;
    OUTPUT SimQueue<BlockCommandPtr>*            mtcBCCWakeupQ;

    OUTPUT std::deque<SimInst>                     *m_scaplerInstQ;
    OUTPUT std::unordered_set<uint32_t>          *m_scbCommitQ;
    INPUT  SimQueue<BCCVecFlush>                 *m_bccVectorFlushQ;
    INPUT  SimQueue<TileRegVecLdRes>             *m_tileRegVecLdResQ;
    INPUT  SimQueue<TileRegVecLdRes>             *m_tileRegVecWrResQ;
    INPUT  SimQueue<TileRegVecLdRetry>           *m_tileRegVecLdRetryQ;
    INPUT  SimQueue<TileRegVecStRetry>           *m_tileRegVecStRetryQ;
    OUTPUT SimQueue<VecTileRegLdReq>             *m_vecTileRegLdReqQ;
    OUTPUT SimQueue<VecTileRegStReq>             *m_vecTileRegStReqQ;

    /* Using for GROB */
    INNER  ThdRingQ<BlkBodyFetchState>          *m_peIfuQ;
    INNER  std::vector<RingQueue<BlockRunInfo>*> m_workLoadQ;
    INNER  SimQueue<VCore::GBufferAllocReq>     m_lm2GBufferQ;
    INNER  SimQueue<GroupAllocInfo>             m_lm2GROB;
    INNER  SimQueue<GroupIssueInfo>             m_GBuffer2GROB;
    INNER  SimQueue<ROBID>                      m_GROB2MaskFile;
    INNER  SimQueue<ScbDrainBus>                m_scbDrainBusQ;
    INNER  SimQueue<ScbDrainBus>                m_scbDrainDoneBusQ;

    // Store ID 与 Cycle 的映射对，用来清除 store Req
    INNER  std::unordered_map<uint32_t, uint32_t> m_storeCycles;
    // 记录 addr 与对应的resp的信息
    // 0x100 -> [Req0, Req1, ...]
    INNER  std::unordered_map<uint32_t, std::vector<MtcMissMemReq>> m_vecLoad;
    INNER  std::unordered_map<uint32_t, bool> m_vecRet;

    INNER  std::unordered_map<uint32_t, MemRequest> m_storeMiss;

    MemoryCoreStats                              m_stats;

    /* TODO: 临时使用，用于调试功能接口 */
    INNER  std::vector<MtcRequest>               m_ldReqArray;
    uint32_t                                     ldArraySize = 0;
    INNER  std::vector<MtcRequest>               m_stReqArray;
    uint32_t                                     stArraySize = 0;
    INNER  std::deque<MtcOp>                     m_vecOpQ;
    INNER  std::vector<uint8_t>                  m_laBuffer;
    INNER  std::set<uint32_t>                    m_freeTidQ;

    INNER  SimQueue<MemRequest>*                 taLoadQ;
    INNER  SimQueue<MemRequest>*                 taStoreQ;
    INNER  SimQueue<VecTileRegStReq>*            scbStReqQ;
    INNER  SimQueue<TileRegVecLdRes>*            scbStResQ;

    CoreInterface                           coreInterface;
    std::vector<uint8_t>                    m_taBuffer;
    std::vector<bool>                       m_taVld;
    INPUT  SimQueue<MemRequest>             *iexVcoreReqQ;
    INNER  SimQueue<MemRequest>             m_memReqQ;
    bool                                    *resolveBlock;
    OUTPUT SimQueue<MemRequest>             *vcoreIexResQ;
    INPUT  SimQueue<LdNonFlushBus>          *loadNonFlushQ;
    INPUT  SimQueue<StNonFlushBus>          *storeNonFlushQ;

    OUTPUT std::shared_ptr<SimQueue<MemRequest>>        m_vecBridgeMemReqQ;
    INPUT  std::shared_ptr<SimQueue<MemRequest>>        m_bridgeVecLdRetQ;
    INPUT  std::shared_ptr<SimQueue<MemRequest>>        m_bridgeVecStRslvQ;

    std::shared_ptr<MemoryLoopManager>      m_vectorLM;
    bool                                    m_memStall = false;
    MemRequest                              curReq;
    std::vector<MemRequest>                 m_memReqArray;
    uint32_t                                m_memReqId = 1000;
    const uint32_t                          m_memReqArraySize = 1000;
    uint32_t                                coreId = 0;
    const uint64_t                          align64Mask = 0x3f;
    const uint64_t                          cacheLineSize = 64;
    OUTPUT SimQueue<TTransMemLdReq>         *m_vecMemLdReqQ;
    INNER SimQueue<MemTTransLdRes>          *m_vecMemLdResQ;
    OUTPUT SimQueue<TTransMemStReq>         *m_vecMemStReqQ;
    INNER SimQueue<MemTTransStRes>          *m_vecMemStResQ;
    std::shared_ptr<GroupBuffer>             m_gBuffer;
    std::shared_ptr<GROB>                    m_grob;
    MtcCoreConfig                            config;
    void                                     SetVerboseOn();
private:
    void DoCommonFlush(FlushBus& bus);

    bool                                    verbose = false;
    std::shared_ptr<MemoryCT>               m_vectorCT;
    // vector core tile aaccelss unit
    std::shared_ptr<MemoryCoreTA>           m_vecta;
    std::shared_ptr<VecIEX>                 m_iex;
    std::vector<std::shared_ptr<VecPE>>     m_peArray;
    TMUConfig                               ubConfig;
    uint32_t                                m_peNum;
    uint32_t                                logicalGroups = 0;
    /* TODO: 用 configs 定义 */
    constexpr static uint32_t                           M_LANENUM = 64;
    constexpr static uint32_t                           M_BASEDATASIZE = M_LANENUM * sizeof(uint8_t);
    constexpr static uint32_t                           M_DATA_UNIT_SIZE_NUM = 4;
    static std::array<int, M_DATA_UNIT_SIZE_NUM>        gDataUnitSizeArray;
};

} // namespace JCore

#endif