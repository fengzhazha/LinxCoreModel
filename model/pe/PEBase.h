#pragma once

#ifndef PE_BASE_H
#define PE_BASE_H

#include "include/ModelSpec.h"
#include "core/Bus.h"
#include "ModelCommon/bus/ResolveBlockBus.h"
#include "ModelCommon/bus/MemReqBus.h"
#include "ModelCommon/bus/FlushBus.h"

namespace JCore {
class SimSys;
class Core;
class PEBase : public SimObj {
public:
    PEBase();
    virtual ~PEBase();
    SimSys                      *sim;
    Core                        *core;
    uint64_t                    peID = 0;
    uint64_t                    coreId = 0;
    std::string                 peLable = "";
    ExecEngineTyp               iexType = ExecEngineTyp::SCALAR_IEX;

    uint64_t                    threadCnt = 1;

    OUTPUT SimQueue<FlushReq>   *pe_flush_rpt_q;

    virtual void                Work() override;
    virtual void                Xfer() override;
    virtual void                Reset() override;
    virtual void                ReportStat() override;
    virtual SimSys              *GetSim() override;

    virtual void                Build() = 0;

    virtual IDBus               GetCommitID(uint32_t tid) = 0;
    virtual IDBus               GetRetireID(uint32_t tid) =0;
    virtual IDBus               GetRetireID() =0;
    virtual ROBIDBus            GetOldestLSID(uint32_t tid)=0;
    virtual void                PrintROBStatus() = 0;

    virtual void                SetMemWakeup(MemReqBus mem) =0;
    virtual void                SetMemData(MemReqBus mem) =0;
    virtual void                SetNeedFlush(ROBID bid, ROBID rid, ROBID lsID, uint32_t tid) =0;
    virtual void                Flush(FlushBus flushReq) = 0;
    virtual void                ReportNuke(FlushBus &flushReq) =0;
    virtual void                ResetBlockInstStats(uint32_t bid) = 0;
    virtual void                IncS1Stats() = 0;
    virtual void                IncIBStallStats() = 0;

    uint32_t                    GetThread() const;
    void                        SetBlockComplete(ROBID bid, uint32_t stid);
    /* \brief tell the BROB the current block is touching exception */
    void                        SetBlockException(ROBID bid, uint32_t stid, const char *info);
    /* \brief tell the BROB the current block is running */
    void                        SetBlockRunning(ROBID bid, BlockStatus status, uint32_t stid);
    /* \brief tell the BROB the current block is terminating program flow */
    void                        SetTerminate(ROBID bid, uint32_t stid);
    /* \brief report block flush by PE misprediction //TODO: replace by minst-level flush or issueQ replay */
    void                        ReportBlockFlush(FlushReq req);
};
}
#endif