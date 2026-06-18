#ifndef  BLOCKISA_MODEL_VECLITE_CORE_H
#define  BLOCKISA_MODEL_VECLITE_CORE_H
#include <sstream>
#include <string>

#include "../configs/vectorlitecore_config.h"
#include "veclite/VectorLiteStats.h"

#include "../tools/trace_logger/TraceLogger.h"
#include "core/Bus.h"
#include "core/interface.h"
#include "interface/InterfaceCommon.h"
#include "ISA.h"
#include "tmu/ring/RingNodeObj.h"
#include "plat/DebugLog.h"
#include "tmu/ring/CrossStation.h"
#include "veclite/VectorLiteStash.h"
#include "veclite/VectorLiteROB.h"
#include "veclite/VectorLiteScheduleIQ.h"
#include "veclite/VectorLiteUOPIQ.h"
#include "veclite/VectorLiteExE.h"
#include "veclite/VectorLiteCache.h"
#include "veclite/VectorLiteUop.h"
#include "veclite/VectorLiteStats.h"

namespace JCore {

struct WrRegReq {
    ROBID bid;
    bool dataReady;
    // vector<uint32_t> uopList;
    unsigned resolveCnt;
    WrRegReq() : bid(ROBID()), dataReady(false), resolveCnt(0) {}

    explicit WrRegReq(ROBID id) : bid(id), dataReady(false), resolveCnt(0) {}
};

class Core;
class VectorLiteTop;
class VectorLite : public RingNodeObj {
public:
    uint64_t groupMachineId = 0;
    bool lastCycleVectorBusy = false;
    uint64_t vectorBusyStartCycle = 0;
    uint32_t tileReqId = 0;

    VectorLiteTop* top = nullptr;
    Core *core = nullptr;
    explicit VectorLite(uint32_t coreId);
    ~VectorLite() override;
    void Work() override;
    void Xfer() override;
    void Reset() override;
    SimSys *GetSim() override;
    void Build();
    void BuildInterface();
    void ReportStat() override;
    const VectorLiteCoreConfig& GetConfig() const;

    void Flush();
    void SetFlush(const FlushBus &bus);

    void PrintCfg() const;
    void Dump();
    bool IsVecBusy();
    void CheckBusy();
    bool IsSrcReady(uint32_t tagId);
    void FreeSrcTileTag(uint32_t tagId);
    bool AllocSrcCache(uint32_t tagId, OperandType handType, ROBID bid, uint32_t tileSize, uint64_t address);

    void WriteTileReg();
    bool HasReadPort(bool colSum, ROBID bid, uint32_t uopCnt, uint32_t latency);
    bool HasWrCredit() const;
    bool ResolveTile(ROBID bid);
    void ResolveUop(ROBID bid);
    bool AllocWrBufferEntry(bool colSum, ROBID bid, uint32_t uopCnt);
    uint32_t GetReadyCount() const;
    void PrintStatus() const;
    void PrintBuffer() const;

    ROBID GetOldestBid() const;
    uint32_t GetCoreId() const;

    void ReceivedCmd();
    void InsertBlockCmdBuffer(const BlockCommandPtr &cmd) const;
    void DeallocBlockCmdBuffer(const BlockCommandPtr &grobCmd) const;
    void ResetStats() const;
    uint32_t GetSelectCore() const;
    uint32_t GetIdleSpace() const;

public:
    INPUT SimQueue<BCCVecFlush>             *bccVecliteFlushQ;
    INPUT SimQueue<BlockCommandPtr>         *bccVecliteBlockCmdQ;

    INNER SimQueue<BlockCommandPtr>         m_fetchStashInQ;
    INNER SimQueue<BlockCommandPtr>         m_fetchRobInQ;
    INNER SimQueue<BlockCommandPtr>         m_fetchSiqInQ;
    INNER SimQueue<ROBID>                   m_stashSiqWakeuQ;
    // 调度队列传递给 UOP 队列
    INNER SimQueue<VectorLiteUopT>          m_siqUiqInQ;
    INNER SimQueue<SIQInfoPtr>              m_siqRobDispQ;
    INNER SimQueue<SIQInfoPtr>              m_siqRobDispLastQ;
    INNER SimQueue<ScbDrainBus>             drainScbDataQ;
    INNER SimQueue<ScbDrainBus>             drainScbRespQ;
    // UOP队列传递给 EXE
    INNER SimQueue<VectorLiteUopT>          m_uiqEXEInQ;
    INNER SimQueue<VectorLiteUopT>          m_exeRobRslvQ;

    // 记录 addr 与对应的resp的信息
    INPUT  SimQueue<TileRegVecLdRes>        *m_tileRegVecLdResQ;
    INPUT  SimQueue<TileRegVecLdRes>        *m_tileRegVecWrResQ;
    OUTPUT SimQueue<VecTileRegLdReq>        *m_vecTileRegLdReqQ;
    OUTPUT SimQueue<VecTileRegStReq>        *m_vecTileRegStReqQ;

    VectorLiteStats                         m_stats;
    CoreInterface                           coreInterface;

    VectorLiteCoreConfig                    config;
    void                                    SetVerboseOn();
    std::map<uint64_t, BlockCommandPtr>     m_blockCmdBuffer;
    void                                    FlushCmdBuffer(const FlushBus& bus) const;
    void                                    DoCommonFlush(const FlushBus& bus);
private:
    bool                                    verbose = false;
    TMUConfig                               ubConfig;
    uint32_t                                m_coreId = -1U;
    std::vector<WrRegReq>                   tileRegWrBuffer;

    VectorLiteStash                         stashUnit;
    VectorLiteROB                           rob;
    VectorLiteScheduleIQ                    siq;
    VectorLiteUOPIQ                         uiq;
    VectorLiteExE                           exeUnit;
    VectorLiteCache                         tileBuffer;
};

} // namespace JCore

#endif
