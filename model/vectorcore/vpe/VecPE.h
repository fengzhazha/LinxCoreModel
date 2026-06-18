#pragma once

#ifndef VEC_PE_H
#define VEC_PE_H

#include "core/Bus.h"
#include "../configs/vpe_config.h"
#include "pe/ifu/iside/pe_ifu.h"
#include "VecPEDecode.h"
#include "VecPERename.h"
#include "VecPEROB.h"
#include "VecPEStats.h"
#include "pe/PEBase.h"
#include "pe/PECommon/OPEState.h"

namespace JCore {

class SimSys;
class VecPE : public PEBase {
public:
    OPEState peMInstStats;
    std::string name;
    uint32_t vecCoreId = 0;
    SimSys* sim;
    VPEConfig configs;
    PE_IFU::PEIFU ifu;
    VecPEDecode decoder;
    VecPERename rename;
    std::vector<VecPEROB*> prob;
    VectorCore* vectorCore;
    std::shared_ptr<VecPEStats> stats;

    std::shared_ptr<LGPRRF> lgprRF;
    std::shared_ptr<VRFRename> vrfRename;

    OUTPUT SimQueue<SimInst>              *pe_iex_std_q;
    OUTPUT SimQueue<SimInst>              *pe_iex_alu_q;
    // vector core address generate pipe(both load and store)
    OUTPUT SimQueue<SimInst>              *pe_iex_vec_agu_q;
    // scalar pipe on vector(scalar instruction and branch)
    OUTPUT SimQueue<SimInst>              *pe_iex_vec_scalar_q;

    /* \brief Resolved instructions queue from IEX to each thread of each thread */
    INPUT std::vector<SimQueue<PEResolveBus>*>  iex_pe_rslv_array;
    /* \brief Resolved instructions queue from LSU to each thread */
    INPUT SimQueue<MemReqBus>               *lsu_pe_rslv_array;

    /* \brief The Information from PE to IFU */
    ThdRingQ<BlkBodyFetchState>             pe_ifu_q;
    /* \brief The decode bundle from IFU to Decode. */
    ThdSimQ<DecodeBundle>                   ifuDecThdQ;
    /* \brief The decode bundle from IFU to Decode. */
    std::vector<RingQueue<BlockRunInfo>>    workThdQ;

    /* \brief Branch prediction destination queue，VPE 不支持预测，后续删除 */
    RingQueue<BrqEntry>                     brq;

    INNER SimQueue<SimInst>                 dec_ren_q;
    /* \brief Aaccelss memory */
    INPUT SimQueue<PtrPacket>               *l2_ifu_q;
    /* \brief Send request packets from IFU to L2. */
    OUTPUT SimQueue<PtrPacket>              *ifu_l2_q;
    /* \brief Send response packets from IFU to L2. */
    OUTPUT SimQueue<PtrPacket>              *snp_l2_q;

    INPUT SimQueue<LdNonFlushBus>           *pe_load_nf_q;
    INPUT SimQueue<StNonFlushBus>           *pe_store_nf_q;

    OUTPUT SimQueue<uint64_t>               *stack_error_pc_q;

    void Work() override;
    void Xfer() override;
    void Reset() override;
    void ReportStat() override;
    SimSys *GetSim() override;

public:
    VecPE(const std::string& nameVal, uint32_t vecCoreIdVal);
    ~VecPE();

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
    void                                    SetNonFlush();
private:
};

} // namespace JCore

#endif  // BLOCKISA_MODEL_PE_VECPE_H
