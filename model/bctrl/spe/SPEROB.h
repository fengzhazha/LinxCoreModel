#pragma once

#ifndef SPE_ROB_H
#define SPE_ROB_H

#include "ModelCommon/ROBID.h"
#include "ModelCommon/SimInstInfo.h"
#include "pe/PECommon/PROBCommon.h"
#include "core/Bus.h"

namespace JCore {
struct OPEState;
class SPE;
class SPEROB {
private:
    RelateCmap                      tcmap;
    RelateCmap                      ucmap;
    RelateCmap                      predcmap;
    uint32_t                        cur_inst_cm_cnt = 0;
    std::map<ROBID, std::vector<SimInst>> tileLdWindow;
    std::map<ROBID, std::vector<SimInst>> tileStWindow;
    IDBus oldestLdBus;
    IDBus oldestStBus;

public:
    SPEROB() {};
    ~SPEROB() {};
    uint32_t                        peID;
    uint32_t                        m_tid;
    SPE                             *speTop;
    ROBState                        current;
    ROBState                        next;
    /* \brief Resolved instructions queue from IEX to current thread of current PE */
    SimQueue<PEResolveBus>          *iex_pe_rslv_q;
    SimQueue<SimInst>                 *pe_iex_rd_q;
    SimQueue<RFReqBus>              *pe_rf_wr_req;
    bool                            needFlush = false;
    bool                            lock = false;
    uint32_t                        retire_inst_cnt = 0;
    uint32_t                        youngest_noncommit_sid = 0;
    uint32_t                        tileLoadCredit = 0;
    uint32_t                        tileStoreCredit = 0;
    IDBus                           commitIdBus;
    std::vector<bool>               vcSidWindow;
    ROBID                           oldestRetiredSid;
    ROBID                           windowEndSid;
    ROBID                           ldCanIssMaxSid;
    void                            Build();
    void                            Reset();
    void                            Work();
    void                            Xfer();
    SimSys                          *GetSim();

    //----------------------------------------
    ROBState&                       getCurrent() { return current; }
    ROBState&                       getNext() { return next; }

    void                            setNeedFlush(ROBID bid, ROBID rid, ROBID lsID);
    bool                            getNeedFlush(ROBID bid, ROBID rid, ROBID lsID);
    IDBus                           getCommitID();
    IDBus                           getRetireID();
    IDBus                           GetCommitBusID();
    void                            SetOldestTileLd(SimInst &inst);
    void                            SetOldestTileSt(SimInst &inst);
    bool                            checkDeadlock();
    ROBIDBus                        GetOldestLSID();
    void                            DumpRob();

    ROBID                           getOldestRID();
    bool                            IsInLdStWindow(SimInst inst);

    uint32_t                        getROBSize() { return current.size; }
    uint32_t                        getROBOsdSize() { return current.osdSize; }
    PROBEntry&                      getROBEntry(uint32_t ttag) { return current.entry[ttag]; }
    PROBEntry&                      getNextEntry(uint32_t ttag) { return next.entry[ttag]; }
    ROBID                           getAllocPtr() { return next.allocPtr; }
    ROBID                           getDeallocPtr() { return next.deallocPtr; }
    bool                            getROBStall() { return current.stall; }
    uint32_t                        getRobSize() {return current.size;}
    void                            setStack(bool vld) { next.stackVld = vld; }
    bool                            stackEnabled() { return next.stackVld; }
    void                            setROBInst(SimInst &inst);
    bool                            branchEnabled() { return next.branchVld; }
    ROBID                           getBranchPtr() { return next.branchPtr; }
    void                            setBranch(bool branch) { next.branchVld = branch; }
    void                            setBranchPtr(ROBID branchPtr) { next.branchPtr = branchPtr; }
    void                            setPipeid(ROBID rid, uint32_t pipeID) { next.entry[rid.val].inst->iqid = pipeID;}
    void                            SetIsqId(ROBID rid, uint64_t isqId) { next.entry[rid.val].inst->isqId = isqId; }
    void                            SetIsqPicker(ROBID rid, uint64_t picker)
    {
        next.entry[rid.val].inst->isqPickerId = picker;
    }
    void                            SetIssued(ROBID rid);
    bool                            CheckPrevIssued(ROBID rid)
    {
        SubROBID(rid, 1, next.entry.size());
        return (!next.entry[rid.val].vld) || (next.entry[rid.val].inst->issued);
    }
    void                            setLastBlockEnd();
    void                            ReleaseRelative(PROBEntry &uop);
    void                            ReportLocalRegKill(PROBEntry &uop, RelateCmap &cmap, RelateInfo &info);

    //----------------------------------------
    /* \brief PE ROB allocate */
    void                            allocROB(SimInst &inst);
    /* \brief Tells ROB the instruction has been issued */
    void                            issueROB(const PEResolveBus &peResolve);
    /* \brief Tells ROB the instruction has read the src VRF */
    void                            PreReleaseSrcROB(const PEResolveBus &peResolve);
    /* \brief PE ROB complete */
    void                            CompleteROB(const PEResolveBus &peResolve);
     /* \brief  */
    void                            PEResolve();

    //----------------------------------------
    void                            commit();
    void                            dealloc();

    void                            flush(FlushBus flushReq);
    void                            RecordTrace(const SimInst &inst) const;

    //----------------------------------------
    void                            PrintStatus();
    void                            ReportInstCnt(const SimInst &inst);
    void                            ReportLocalRegStats(const SimInst &inst, const uint32_t bid, OPEState *opeState);
    bool                            CheckStackComplate(const PEResolveBus &peResolve, uint32_t rid);
    bool                            CheckStack(const PEResolveBus &peResolve, uint32_t rid);
    bool                            CheckDstDataOut(const POperandPtr &dst, uint64_t peResolvDataOut,
                                        const PEResolveBus &peResolve, uint32_t rid);
    void                            CheckDstDataOutVld(POperandPtr &dst, const POperandPtr &resolveDst);
    void                            HandleInstCntByType(const Opcode &opcode, ROBID instBid);
    void                            CheckRelativeReg(PROBEntry &uop, RelateInfo &info);
    void                            CheckReg(PROBEntry &uop, RelateCmap &cmap, RelateInfo &info);
    void                            ReleaseFunc(PROBEntry &uop, RelateCmap &cmap, RelateInfo &info);
    void                            SetRelateInfo(RelateInfo &info, uint32_t tag, ROBID &seq);
    void                            SetRegReadyTable(RelateCmap &cmap, RelateInfo &info, ExecEngineTyp iexTyp);
    void                            CommitInsn();
    void                            ReportLocalRegBlockCommit(ROBID bid, uint32_t stid);
    void                            CommitBlock(PROBEntry &uop);
    void                            CleanCMAP(ROBID bid);
    void                            CleanCMAP(RelateCmap& cmap, ROBID bid);
    void                            CleanGroupCMAP(ROBID bid, ROBID gid);
    void                            CleanGroupCMAP(RelateCmap& cmap, ROBID bid, ROBID gid);
    void                            CommitLast(PROBEntry &uop);
    void                            HandleEnd(PROBEntry &uop, SimInst &inst);
    void                            IncrementStats(PROBEntry &uop);
    bool                            CheckFlushReqBid(FlushBus &flushReq);
    void                            CheckFlushReq(FlushBus &flushReq, const ROBID fbid);
    void                            HandleNextEntryVldAndLessEq(ROBID &old_alloc, ROBID &next_alloc, const ROBID &ptr,
                                        bool &found, SimInst &oldestFlushInst);
    ExecEngineTyp                   GetIexType();
    void                            FlushRelativeReg(const FlushBus &flushReq, RelateCmap &cmap);
    void                            CheckTagVld(SimInst &inst, PLpvInfo &lpvInfo);
    void                            CheckDstTagVld(SimInst &inst);
    void                            CheckNextEntryStatus(const ROBID &ptr);
    void                            SetNextBranchVld(bool hasInnerBR, const ROBID lastInnerPtr);
    void                            HandleNextEntryVldAndFound(FlushBus &flushReq, const ROBID ptr, const ROBID fbid);
    void                            SetNextVal();
    void                            HandleROBID(FlushBus &flushReq, ROBID fbid, ROBID frid, ROBID ptr);
    void                            AssignLdStId();
    void                            CheckCAExecRes(PROBEntry &uop, SimInst &inst);
    void                            MinstResVerify(PROBEntry &entry);
    void                            MinstPipeView(PROBEntry &entry);
    bool                            IsInStWindow(SimInst &inst);
    bool                            IsInLdWindow(SimInst &inst);
    void                            Stats();

    void                            PrintTlsPipeViewLog(SimInst &inst, uint64_t addr);
};
} // namespace JCore
#endif