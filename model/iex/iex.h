#ifndef  BLOCKISA_MODEL_IEX_H
#define  BLOCKISA_MODEL_IEX_H
#pragma once

#include <vector>

#include "../configs/iex_config.h"
#include "iex/iex_bn.h"
#include "iex/iex_dispatch.h"
#include "iex/iex_iq.h"
#include "iex/iex_mdb.h"
#include "iex/iex_rd_unit.h"
#include "iex/iex_rf.h"
#include "iex/iex_stat.h"
#include "iex/pipe/iex_pipe.h"
#include "iex/rtable.h"
#include "interface/TileRegTTransLdRes.h"
#include "interface/TileRegTTransStRetry.h"
#include "interface/TTransTileRegLdReq.h"
#include "interface/TTransTileRegStReq.h"
#include "ModelSpec.h"
#include "SimSys.h"
#include "pe/PEBase.h"
#include "ModelCommon/SimInstInfo.h"
#include "plat/DebugLog.h"

namespace JCore {


/* PE Instruction Executing State */
struct IEXState {
    /* Stall signal per lane */
    /* \brief Stall for load/store lane
     * This is due to back pressure non-speculative loads and store queue full */
    bool                                        nonSpecMemStall;
    /* \brief Stall for load/store lane
     * This is due to back pressure from speculative loads and store queue full */
    bool                                        specMemStall;

    void                                        Build(IEXConfig const &configs, uint32_t peCount);
    void                                        Reset();
    void                                        flush(FlushBus &flushReq);
};

struct ROBState;
class VectorCore;
class LoadStoreUnit;

class IEX : public SimObj {
private:
    IEXState                                    current;
    IEXState                                    next;

    /* IEX Pipes */
    /* \brief command instruction pipe */
    std::vector<CMDPipe>                        cmdPipe;
    /* \brief arithmetic instruction pipe */
    std::vector<ALUPipe>                        aluPipe;
    /* \brief arithmetic instruction pipe */
    std::vector<LDAPipe>                        ldaPipe;

    std::vector<LDAPipe>                        ldascPipe;
    /* \brief arithmetic instruction pipe */
    std::vector<STAPipe>                        staPipe;
    /* \brief arithmetic instruction pipe */
    std::vector<STDPipe>                        stdPipe;
    /* \brief arithmetic instruction pipe */
    std::vector<BRUPipe>                        bruPipe;
    /* \beief vector agu instruction pipe */
    std::vector<AGUPipe>                        aguPipe;
    /* \brief vector scalar instruction pipe */
    std::vector<VecScalarPipe>                  scaPipe;

    /* Bypass Network */
    BypassNetwork                               bn;

    /* Bypass Crossbar */
    std::vector<SimInst*>                         rfInsts;
    std::vector<SimInst*>                         exeInsts;
    std::vector<SimInst*>                         commitInsts;
    std::vector<SimInst*>                         stages;
    /* \brief Pending VRF release instructions */
    std::vector<SimInst>                          m_pendingVrfReleaseInsts;

    /* \brief Pre-release VRF at RD (I1) stage */
    void                                        PreReleaseVRF();
    void                                        pipeMove();

    void                                        requestLSU();
    void                                        receiveFromLSU();

public:
    IEX() {};
    virtual ~IEX();
    IEXConfig                                       configs;
    Core                                            *core;
    VectorCore                                      *vecCore;
    size_t                                          peCount;
    std::shared_ptr<IEXStats>                       stats;
    std::shared_ptr<DebugLog>                       debugLogger = nullptr;
    std::string                                     name;
    std::vector<std::vector<ROBState*>>             currentROBs;
    std::vector<std::vector<ROBState*>>             nextROBs;
    std::vector<std::vector<SimQueue<MemReqBus>*>>  iex_lsu_lda_array;
    std::vector<std::vector<SimQueue<MemReqBus>*>>  iex_lsu_sta_array;
    std::deque<SimInst>*                              iexScalperInstQ;
    std::vector<SimInst>*                             scalper_iex_inst_q;

    std::vector<std::vector<SimQueue<MemReqBus>*>>  iex_lsu_std_array;
    std::vector<std::vector<SimQueue<MemReqBus>*>>  lsu_iex_lret_array;
    SimQueue<ResolveBlockBus>                       *iex_brob_rslvblk = nullptr;
    SimQueue<FlushReq>                              *iex_flush_rpt_q;
    SimQueue<RFReqBus>                              *iex_rt_wr_q;
    OUTPUT SimQueue<SimInst>                          *cmdIQBISQ;
    /* \brief Resolved instructions array from IEX to each thread of each PE */
    std::vector<std::vector<SimQueue<PEResolveBus>*>>           iex_pe_rslv_array;
    SimQueue<SimInst>                                 ssrsetOrderQ;
    OUTPUT std::vector<SimQueue<SimInst>*>            rf_ct_q;

    ExecEngineTyp                                   id = SCALAR_IEX;
    uint32_t                                        coreId = 0;
    deque<SimInst>                                    brRlsvQ;

    /* \brief Dispatch unit */
    DispatchUnit                                    dispatchUnit;
    /* \brief Ready table */
    ReadyState                                      rtable;

    IssueQueue                                      iq;

    IEXMDB                                          iexmdb;

    std::vector<int>                                tileLdCredit;

    uint64_t                                        cmdIQStallCycle = 0;
    uint64_t                                        aluIQStallCycle = 0;
    uint64_t                                        aguIQStallCycle = 0;
    uint64_t                                        ldaIQStallCycle = 0;
    uint64_t                                        staIQStallCycle = 0;
    uint64_t                                        stdIQStallCycle = 0;
    uint64_t                                        bruIQStallCycle = 0;
    uint64_t                                        scaIQStallCycle = 0;

    uint64_t                                        cmdIQStallCnt = 0;
    uint64_t                                        aluIQStallCnt = 0;
    uint64_t                                        aguIQStallCnt = 0;
    uint64_t                                        ldaIQStallCnt = 0;
    uint64_t                                        staIQStallCnt = 0;
    uint64_t                                        stdIQStallCnt = 0;
    uint64_t                                        bruIQStallCnt = 0;
    uint64_t                                        scaIQStallCnt = 0;

    uint64_t                                        iexCmdIqCount = 0;
    uint64_t                                        iexAluIqCount = 0;
    uint64_t                                        iexAguIqCount = 0;
    uint64_t                                        iexLdaIqCount = 0;
    uint64_t                                        iexStaIqCount = 0;
    uint64_t                                        iexStdIqCount = 0;
    uint64_t                                        iexBruIqCount = 0;
    uint64_t                                        iexScaIqCount = 0;

    uint64_t                                        iexCmdIqDepth = 0;
    uint64_t                                        iexAluIqDepth = 0;
    uint64_t                                        iexAguIqDepth = 0;
    uint64_t                                        iexLdaIqDepth = 0;
    uint64_t                                        iexStaIqDepth = 0;
    uint64_t                                        iexStdIqDepth = 0;
    uint64_t                                        iexBruIqDepth = 0;
    uint64_t                                        iexScaIqDepth = 0;

    uint64_t                                        iexCmdPickCount = 0;
    uint64_t                                        iexAluPickCount = 0;
    uint64_t                                        iexAluHeterPickCount = 0;
    uint64_t                                        iexAguPickCount = 0;
    uint64_t                                        iexLdaPickCount = 0;
    uint64_t                                        iexStaPickCount = 0;
    uint64_t                                        iexStdPickCount = 0;
    uint64_t                                        iexBruPickCount = 0;
    uint64_t                                        iexScaPickCount = 0;

    uint64_t                                        cmdIQWport = 0;
    uint64_t                                        aluIQWport = 0;
    uint64_t                                        aguIQWport = 0;
    uint64_t                                        ldaIQWport = 0;
    uint64_t                                        staIQWport = 0;
    uint64_t                                        stdIQWport = 0;
    uint64_t                                        bruIQWport = 0;
    uint64_t                                        scaIQWport = 0;
    uint64_t                                        simtMask = INVALID_PC;
    /* RF Stage: Read Files */
    RegFile                                     rf;
    RdUnit                                      rd;

    std::vector<SimQueue<TTransTileRegLdReq>*>  tTransTileRegLdReqArray;
    std::vector<SimQueue<TTransTileRegStReq>*>  tTransTileRegStReqArray;
    SimQueue<MemRequest>                        *iexVcoreReqQ;
    bool                                        *resolveBlock;
    SimQueue<MemRequest>                        *vcoreIexResQ;
    SimQueue<MemRequest>                        *vcoreIexLdResQ;
    SimQueue<MemRequest>                        *vcoreIexStResQ;
    SimQueue<VecTileRegStReq>                   *iexScbReqQ;
    SimQueue<TileRegVecLdRes>                   *scbIexRspQ;
    //----------------------------------------
    void                            Build();
    void                            BuildSupplement();
    void                            Reset();
    void                            resetStats();
    void                            Work();
    void                            Xfer();
    SimSys                          *GetSim();
    void                            ReportStat() override;
    BypassNetwork&                  GetBN() {return bn;}
    void                            InstWakeupIQ(SimInst &inst);
    void                            doStats();
    std::shared_ptr<LoadStoreUnit>  GetLSU();
    void                            ldapip_resolve(SimInst inst);
    void                            agupip_resolve(SimInst inst);
    //----------------------------------------
    void                            setMemStall(bool spec, bool stall);
    void                            setMemWakeup(MemReqBus const &mem);
    void                            setMemResolve(MemReqBus const &mem);
    void                            setMemData(MemReqBus const & mem);
    void                            SetTileLsuPipeView(const MemRequest& mem);
    bool                            waitingGet();
    bool                            checkSimtAluStall(const SimInst& inst, uint32_t pipeIdx);
    bool                            checkSimtScaStall(const SimInst& inst, uint32_t pipeIdx);
    bool                            checkLdStall(uint32_t stid);
    void GetLsustatsMembound(bool& anyload, bool& l1miss, bool& l2miss, bool& stqfull, uint32_t stid);
    bool GetLsuload_to_use_enable();
    bool GetLsucheckStoreStall(uint32_t size, uint32_t stid);
    bool GetLsucheckLoadCltStall(uint64_t addr, uint32_t width, uint32_t stid);
    void BackToPipe(bool& pipeFull);

    //----------------------------------------
    /* \brief set ready table */
    void                            SetRegReadyTable(OperandType type, uint32_t ptag, bool ready,
                                                     PLpvInfo lpvInfo = nullptr, uint32_t peID = 0, uint32_t tid = 0);
    /* \brief Execute Stage */
    void                            execute();
    /* \brief Write Back Stage */
    void                            writeback();

    // TODO: uint32_t GetSimtMask() {return simtMask;} void SetSimtMask(uint32_t mask) {simtMask = mask;}

    void                            setFlush(FlushBus &flushReq);
    /* \brief Flush count of instructions record in isq */
    void                            flushIQPECount();
    /* \brief Flush pipe line */
    void                            flushPipe(FlushBus &flushReq);
    void                            checkIQStall();
    void                            updateIQStall();

    void                            setPipeCancel(uint32_t pipeID);
    void                            ResetRdyTable(OperandType type, uint32_t ptag, uint32_t peid = 0, uint32_t tid = 0);
    void                            reportCancel(MemReqBus &memReqBus);
    void                            SubReleaseIQEntryI2();
    void                            SubReleaseIQEntryAfterIssue();
    void                            releaseIQEntry();

    //----------------------------------------
    /* \brief Resolve branch for setbpc instruction */
    bool                            branchResolve(SimInst &inst);
    /* \brief Resolve branch by load compare */
    void                            loadBranchResolve(SimInst &inst);
    /* \brief Resolve branch for inner jump instruction */
    void                            innerBranchResolve(SimInst &inst);
    void                            ResolveFunc(SimInst& inst, uint64_t dataOut);
    /* \brief Set local ptag ready in rename stage */
    void                            setPtagReady(uint32_t lptag, PLpvInfo &lpvInfo, bool ready);
    // TODO: delete
    uint64_t                        getGPR(uint32_t ptag);
    void                            setGPR(uint32_t ptag, uint64_t data);
    template <class T>
    bool                            CheckForPipeSpace(vector<T> &pipe);
    void                            HandleVcoreResp();
    void                            HandleVecResp();
    void                            HandleMtcResp();
    void                            HandleLd(MemRequest &bus);
    void                            VecLoadWakeUp(MemRequest &req);
    void                            HandleUbLdResp(MemRequest &bus);
    void                            HandleUbStResp(MemRequest &bus);
    uint32_t                        GetPipeActiveTLoad();
    uint32_t                        GetPipeActiveLoad();
    uint32_t                        GetPipeMtcActiveTLoad();
    uint32_t                        GetPipeMtcActiveLoad();
    uint32_t                        GetPipeActiveStore();
    bool                            IsLastLoadStore(const SimInst &inst, const IDBus &oldestBus);
    void                            TloadSendSeqToTileScb(MemReqBus const &mem, SimInst &inst, bool islast) const;
    void                            PrintTlsPipeViewLog(SimInst &inst, MemReqBus mem);
    void                            RegConflictStat();
};


} // namespace JCore
#endif
