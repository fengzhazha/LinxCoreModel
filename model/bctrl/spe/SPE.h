#pragma once

#ifndef SPE_H
#define SPE_H

#include "ModelSpec.h"
#include "DCTop.h"
#include "SPEROB.h"
#include "SPEStats.h"
#include "../configs/spe_config.h"
#include "bctrl/spe/SPERename.h"
#include "pe/PEBase.h"
#include "pe/PECommon/OPEState.h"

namespace JCore {
class SPE : public PEBase {
public:
    uint64_t peSeqId = 0;
    SimSys *sim;
    DCTop dcTop;
    SPERename d2Stage;
    std::vector<SPEROB*> prob;
    SPEConfig configs;
    std::shared_ptr<SPEStats> stats;
    SimQueue<SimInst> dec_ren_q;

    OPEState                               peMInstStats;

    OUTPUT std::vector<SimQueue<SimInst>*> pe_iex_cmd_array;
    OUTPUT std::vector<SimQueue<SimInst>*> pe_iex_alu_array;
    OUTPUT std::vector<SimQueue<SimInst>*> pe_iex_lda_array;
    OUTPUT std::vector<SimQueue<SimInst>*> pe_iex_sta_array;
    OUTPUT std::vector<SimQueue<SimInst>*> pe_iex_std_array;
    OUTPUT std::vector<SimQueue<SimInst>*> pe_iex_bru_array;

    OUTPUT SimQueue<uint32_t>               *rtable_release_q;
    /* \brief Resolved instructions queue from LSU to each PE */
    INPUT SimQueue<MemReqBus>               *lsu_pe_rslv_array;
    /* \brief Resolved instructions queue from IEX to each thread of each PE */
    INPUT std::vector<SimQueue<PEResolveBus>*>  iex_pe_rslv_array;

    OUTPUT SimQueue<uint64_t>               *stack_error_pc_q;

    INPUT SimQueue<SimInst> *f5_spe_inst_q;

    INNER SimQueue<SimInst> dctop_rename_q;

    void Work() override;
    void Xfer() override;
    void Reset() override;
    void ReportStat() override;
    SimSys *GetSim() override;
    SPE() = default;
    ~SPE();

    void DoStats();
    void HandleStoreResolve();

    void                                    Build() override;
    IDBus                                   GetCommitID(uint32_t tid) override;
    IDBus                                   GetRetireID(uint32_t tid)  override;
    IDBus                                   GetRetireID() override;
    ROBIDBus                                GetOldestLSID(uint32_t tid) override;
    void                                    PrintROBStatus() override;
    void                                    SetMemWakeup(MemReqBus mem) override;
    void                                    SetMemData(MemReqBus mem) override;
    void                                    SetNeedFlush(ROBID bid, ROBID rid, ROBID lsID, uint32_t tid) override;
    void                                    Flush(FlushBus flushReq) override;
    void                                    ReportNuke(FlushBus &flushReq) override;
    void                                    ResetBlockInstStats(uint32_t bid) override;
    void                                    IncS1Stats() override;
    void                                    IncIBStallStats() override;

    void                                    SetMemResolve(MemReqBus mem);
    bool                                    IsPEIdle();
};
}
#endif