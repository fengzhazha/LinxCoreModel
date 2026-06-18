#include "VecPEROB.h"

#include <cassert>

#include "core/Core.h"
#include "VecPE.h"
#include "iex/iex.h"
#include "utils/obj_print.h"
#include "DFX/InstTracer.h"

namespace JCore {


using namespace std;
void VecPEROB::Build(uint32_t tid)
{
    m_tid = tid;
    peID = top->peID;
    uint64_t robDepth = top->configs.vpeROBDepth;
    current.entry.clear();
    next.entry.clear();
    for (uint32_t i = 0; i < robDepth; i++) {
        current.entry.emplace_back();
        next.entry.emplace_back();
        current.entry[i].inst = nullptr;
        next.entry[i].inst = nullptr;
    }
    config.overrideDefaultConfig(GetSim()->getCfgs());
    tileLoadCredit = config.ta_ldq_size;
    tileStoreCredit = config.ta_stq_size;
    tcmap.Reset(OperandType::OPD_TLINK);
    ucmap.Reset(OperandType::OPD_ULINK);
    predcmap.Reset(OperandType::OPD_PREDMASK);
    vrfRename = top->vrfRename;

    VectorCoreConfig vcConfig;
    vcSidWindow.resize(vcConfig.ta_stq_size);
    for (uint64_t i = 0; i < vcSidWindow.size(); i++) {
        vcSidWindow[i] = false;
    }
    IncROBID(ldCanIssMaxSid, top->configs.vcSidMax);
    IncROBID(oldestRetiredSid, top->configs.vcSidMax);
    AddROBID(windowEndSid, vcConfig.ta_stq_size, top->configs.vcSidMax);
}

void VecPEROB::Reset()
{
    VectorCoreConfig vcConfig;
    vcSidWindow.resize(vcConfig.ta_stq_size);
    for (uint64_t i = 0; i < vcSidWindow.size(); i++) {
        vcSidWindow[i] = false;
    }
    ldCanIssMaxSid.val = 0;
    ldCanIssMaxSid.wrap = false;
    oldestRetiredSid.val = 0;
    oldestRetiredSid.wrap = false;
    windowEndSid.val = 0;
    windowEndSid.wrap = false;
    IncROBID(ldCanIssMaxSid, top->configs.vcSidMax);
    IncROBID(oldestRetiredSid, top->configs.vcSidMax);
    AddROBID(windowEndSid, vcConfig.ta_stq_size, top->configs.vcSidMax);

    current.Reset();
    next.Reset();
    tcmap.Reset(OperandType::OPD_TLINK);
    ucmap.Reset(OperandType::OPD_ULINK);
    predcmap.Reset(OperandType::OPD_PREDMASK);
}

void VecPEROB::Work()
{
    if (!current.size) {
        return;
    }
    AssignLdStId();
    // ((VecStats *)top->stats)->total_rob_size += current.size;
    commit();
    dealloc();
    PrintStatus();
    Stats();
}

bool VecPEROB::IsInLdWindow(SimInst &inst)
{
    if (GetSim()->GetCore()->configs.load_ooo_enable) {
        return true;
    }

    if (inst->CheckOOOLoad() || inst->vcSid < ldCanIssMaxSid) {
        return true;
    } else {
        return false;
    }
}

bool VecPEROB::IsInStWindow(SimInst &inst)
{
    if ((inst->vcSid <= windowEndSid) && (inst->vcSid >= oldestRetiredSid)) {
        return true;
    } else {
        return false;
    }
}

void VecPEROB::Xfer()
{

    next.stall = (next.size + top->configs.decodeWidth) > top->configs.vpeROBDepth;
    current = next;

    iex_pe_rslv_q->Work();
    PEResolve();
    tileLdWindow.clear();
    tileStWindow.clear();
    ASSERT(current.size != 1869897576);
}

SimSys *VecPEROB::GetSim()
{
    return top->GetSim();
}

void VecPEROB::setNeedFlush(ROBID bid, ROBID rid, ROBID lsID)
{
    if (!next.entry[rid.val].vld ||
        next.entry[rid.val].bid != bid ||
        next.entry[rid.val].inst->lsID != lsID) {
            return;
    }
    if (next.entry[rid.val].status == INST_RETIRED) {
        for (ROBID temRid = rid; temRid.val != next.commitPtr.val; IncROBID(temRid, next.entry.size())) {
            if (next.entry[rid.val].status == INST_RETIRED) {
                next.osdSize++;
                next.entry[temRid.val].status = INST_NEEDFLUSH;
            }
        }
        next.commitPtr = rid;
    }
    next.entry[rid.val].status = INST_NEEDFLUSH;
}

bool VecPEROB::getNeedFlush(ROBID bid, ROBID rid, ROBID lsID)
{
    if (bid != current.entry[rid.val].bid || lsID != current.entry[rid.val].inst->lsID) {
        return false;
    }
    if (current.entry[rid.val].status == INST_NEEDFLUSH) {
        return true;
    }
    return false;
}

ROBID VecPEROB::getOldestRID()
{
    return current.entry[current.commitPtr.val].inst->rid;
}

bool VecPEROB::IsInLdStWindow(SimInst inst)
{
    ASSERT(OpcodeIsLoad(inst->opcode) || OpcodeIsStore(inst->opcode));
    if (OpcodeIsLoad(inst->opcode)) {
        auto it = tileLdWindow.find(inst->gid);
        if (it != tileLdWindow.end()) {
            bool find = false;
            for (auto instEntry : it->second) {
                if (inst->gid == instEntry->gid && inst->rid == instEntry->rid) {
                    find = true;
                    break;
                }
            }
            return find;
        } else {
            return false;
        }
    }
    if (OpcodeIsStore(inst->opcode)) {
        auto it = tileStWindow.find(inst->gid);
        if (it != tileStWindow.end()) {
            bool find = false;
            for (auto instEntry : it->second) {
                if (inst->gid == instEntry->gid && inst->rid == instEntry->rid) {
                    find = true;
                    break;
                }
            }
            return find;
        } else {
            return false;
        }
    }
    return false;
}

IDBus VecPEROB::getCommitID()
{
    IDBus bus;
    bus.vld = false;
    if (current.size != 0) {
        bus.vld = true;
        bus.bid = current.entry[current.deallocPtr.val].bid;
        bus.rid = current.deallocPtr;
        bus.iexTyp = top->iexType;
        if (current.entry[current.deallocPtr.val].inst)
            bus.lsID = current.entry[current.deallocPtr.val].inst->lsID;
    }
    return bus;
}

void VecPEROB::SetCommitBusID(SimInst &inst)
{
    commitIdBus.vld = true;
    commitIdBus.peID = top->peID;
    commitIdBus.bid = inst->bid;
    commitIdBus.gid = inst->gid;
    commitIdBus.rid = inst->rid;
    commitIdBus.tid = inst->tid;
    commitIdBus.tpc = inst->pc;
    commitIdBus.lsID = inst->lsID;
    commitIdBus.iexTyp = top->iexType;
}

IDBus VecPEROB::GetCommitBusID()
{
    return commitIdBus;
}

IDBus VecPEROB::getRetireID()
{
    IDBus bus;
    bus.vld = true;
    bus.bid = current.entry[current.commitPtr.val].bid;
    bus.gid = current.entry[current.commitPtr.val].gid;
    bus.rid = current.commitPtr;
    bus.tpc = current.entry[current.commitPtr.val].tpc;
    bus.peID = top->peID;
    bus.coreId = top->coreId;
    SimInst &inst = current.entry[current.commitPtr.val].inst;
    if (next.entry[current.commitPtr.val].status == INST_FREE) {
        bus.vld = false;
    } else if (inst) {
        bus.bid = inst->bid;
        bus.rid = inst->rid;
        bus.tid = inst->tid;
        bus.tpc = inst->pc;
        bus.lsID = inst->lsID;
        bus.lid = inst->load_id;
        bus.sid = inst->sid;
        bus.tSeq = inst->tSeq;
        bus.uSeq = inst->uSeq;
        bus.predSeq = inst->predSeq;
        bus.isTld = inst->opcode == Opcode::OP_TLD;
        // Deadlock check
        bus.isLoadStore = ((OpcodeIsLoad(inst->opcode) || OpcodeIsStore(inst->opcode))) &&
                        !(!inst->stack_check && inst->stack_type == StackInstType::STACK_GET);
        if (next.entry[current.commitPtr.val].status == INST_COMPLETED ||
            next.entry[current.commitPtr.val].status == INST_RETIRED) {
                bus.isLoadStore = false;
        }
        bus.isLoad = OpcodeIsLoad(inst->opcode) &&
                     !(!inst->stack_check && inst->stack_type == StackInstType::STACK_GET);
        bus.pipeID = inst->iqid;
        if (inst->bfuInfo) {
            bus.fbid = inst->bfuInfo->fbid;
            bus.fbid_local = inst->bfuInfo->fbid_local;
        }
        bus.first = inst->first;
    } else {
        bus.isLoadStore = false;
    }
    bus.iexTyp = top->iexType;
    return bus;
}

void VecPEROB::SetOldestTileLd(SimInst &inst)
{
}
void VecPEROB::SetOldestTileSt(SimInst &inst)
{
}

bool VecPEROB::checkDeadlock()
{
    return next.entry[current.commitPtr.val].status != INST_COMPLETED;
}

ROBIDBus VecPEROB::GetOldestLSID()
{
    ROBIDBus bus = ROBIDBus();
    if (current.entry[current.commitPtr.val].status != INST_FREE) {
        bus.vld = true;
        bus.id = current.entry[current.commitPtr.val].inst->lsID;
    }
    return bus;
}

void VecPEROB::allocROB(SimInst &inst)
{
    PROBEntry e;
    e.vld = true;
    e.status = INST_ALLOCATED;
    e.stack_complete = false;
    e.tpc = inst->pc;
    e.bid = inst->bid;
    e.gid = inst->gid;
    e.last = inst->isLastInBlock;
    e.rid = next.allocPtr;
    inst->rid = e.rid;
    e.inst = inst;
    next.entry[next.allocPtr.val] = e;

    uint32_t vecPEStartIdx = top->core->configs.stdPeCount;
    uint32_t vecPECnt = top->core->configs.simtPeCount;
    uint32_t memPEStartIdx = vecPEStartIdx + vecPECnt;
    uint32_t memPECnt = top->core->configs.memPeCount;
    if (m_grob && (peID >= vecPEStartIdx && peID < memPEStartIdx)) {
        if (inst->biqType != BIQType::MCALL_IQ) {
            m_grob->ReportAllocMinst(inst->bid, inst->gid, inst->stid);
        }
    } else if (m_mtcgrob && (peID >= memPEStartIdx && peID < memPEStartIdx + memPECnt)) {
        if (inst->biqType != BIQType::TMA_IQ) {
            m_mtcgrob->ReportAllocMinst(inst->bid, inst->gid, inst->stid);
        }
    }

    uint64_t robDepth = top->configs.vpeROBDepth;
    IncROBID(next.allocPtr, robDepth);
    next.size++;
    next.osdSize++;
    if (top->core->IsVecPe(top->machineType)) {
        LOG_INFO_M(Unit::VECTOR, Stage::D1) << "alloc pe rob, " << inst->Dump() << dec << ", occ:" << next.size;
    } else if (top->core->IsMtcPe(peID)) {
        LOG_INFO_M(Unit::MTC, Stage::D1) << "alloc pe rob, " << inst->Dump() << dec << ", occ:" << next.size;
    } else {
        LOG_INFO_M(Unit::BCC, Stage::D1) << "alloc pe rob, " << inst->Dump() << dec << ", occ:" << next.size;
    }

    if (next.size > robDepth) {
        LOG_ERROR_M(top->machineType, Stage::NA) << "wrong at " << inst;
        ASSERT(0 && "PE rob entry overlow");
    }
}

void VecPEROB::SetIssued(ROBID rid)
{
    next.entry[rid.val].inst->issued = true;
}

void VecPEROB::AssignLdStId()
{
    size_t loadEntry = 0;
    size_t storeEntry = 0;
    uint64_t robDepth = top->configs.vpeROBDepth;
    size_t startEntryId = current.commitPtr.val;

    auto processInstruction = [&](const PROBEntry& entry) -> bool {
        if (!entry.vld) return false;
        if (entry.status == INST_NEEDFLUSH) return false;
        if (entry.status == INST_FAULT) return false;
        if (entry.status == INST_FREE) return false;
        if (!OpcodeIsLoad(entry.inst->opcode) && !OpcodeIsStore(entry.inst->opcode)) return false;

        return true;
    };

    auto processLoadInstruction = [&](const PROBEntry& entry) -> bool {
        if (top->core->IsVecPe(peID)) {
            if (loadEntry >= top->core->vectorTop->GetTileLdqSize() - 1) {
                return true;
            }
        } else {
            if (loadEntry >= top->core->mtcCores[0]->GetConfig().ta_ldq_size - 1) {
                return true;
            }
        }

        auto it = tileLdWindow.find(entry.inst->gid);
        if (it == tileLdWindow.end()) {
            std::vector<SimInst> vec = {entry.inst};
            tileLdWindow.emplace(entry.inst->gid, vec);
        } else {
            tileLdWindow[entry.inst->gid].push_back(entry.inst);
        }
        loadEntry++;

        return false;
    };

    auto processStoreInstruction = [&](const PROBEntry& entry) -> bool {
        if (top->core->IsVecPe(peID)) {
            if (storeEntry >= top->core->vectorTop->GetTileStqSize() - 1) {
                return true;
            }
        } else {
            if (storeEntry >= top->core->mtcCores[0]->GetConfig().ta_stq_size - 1) {
                return true;
            }
        }

        auto it = tileStWindow.find(entry.inst->gid);
        if (it == tileStWindow.end()) {
            std::vector<SimInst> vec = {entry.inst};
            tileStWindow.emplace(entry.inst->gid, vec);
        } else {
            tileStWindow[entry.inst->gid].push_back(entry.inst);
        }
        storeEntry++;

        return false;
    };

    for (size_t i = 0; i < robDepth; i++) {
        PROBEntry entry = current.entry[startEntryId];
        startEntryId = (startEntryId + 1) % robDepth;
        if (!processInstruction(entry)) {
            continue;
        }

        bool shouldBreak = false;

        if (OpcodeIsLoad(entry.inst->opcode)) {
            shouldBreak = processLoadInstruction(entry);
        } else if (OpcodeIsStore(entry.inst->opcode)) {
            shouldBreak = processStoreInstruction(entry);
        }

        if (shouldBreak) {
            break;
        }
    }
}

void VecPEROB::PEResolve()
{
    while (!iex_pe_rslv_q->Empty()) {
        PEResolveBus peResolve = iex_pe_rslv_q->Read();
        if (peResolve.isComplete) {
            CompleteROB(peResolve);
            continue;
        }
        if (peResolve.isIssue) {
            issueROB(peResolve);
            continue;
        }
        if (peResolve.isSrcRead) {
            PreReleaseSrcROB(peResolve);
            continue;
        }
        if (peResolve.isLDCancel) {
            if (next.entry[peResolve.rid.val].status != INST_NEEDFLUSH &&
                next.entry[peResolve.rid.val].status != INST_FAULT &&
                next.entry[peResolve.rid.val].status != INST_FREE) {
                next.entry[peResolve.rid.val].status = INST_RENAMED;
            }
        }
    }
}

void VecPEROB::issueROB(const PEResolveBus &peResolve)
{
    uint32_t rid = peResolve.rid.val;
    if (next.entry[rid].status == INST_NEEDFLUSH) {
        return;
    }
    next.entry[rid].status = INST_ISSUED;

    top->stats->curr_cycle_issued_cnt++;
}

void VecPEROB::PreReleaseSrcROB(const PEResolveBus &peResolve)
{
    const uint32_t rid = peResolve.rid.val;
    SimInst& inst = current.entry[rid].inst;
    if (top->core->configs.simtEnable && top->core->IsVectorIex(inst->iexType)) {
        uint32_t rtid = 0;
        if (inst->iexType == MEM_IEX) {
            rtid = inst->tid + top->core->configs.simtPeCount * top->core->configs.threadCount;
        } else {
            rtid = inst->tid;
        }
        for (auto &psrc : inst->psrcs_) {
            if (OperandTypeIsVReg(psrc->type) && !psrc->reuse && !psrc->released) {
                if (vrfRename->PreRelease(rtid, inst->stid, inst, psrc->type, psrc->ptag, psrc->recycled)) {
                    psrc->released = true;
                }
            }
        }
    }
}

bool VecPEROB::CheckStackComplate(const PEResolveBus &peResolve, uint32_t rid)
{
    if (!current.entry[rid].stack_complete) {
        next.entry[rid].stack_complete = true;
        for (size_t i = 0; i < next.entry[rid].inst->pdsts_.size(); i++) {
            CheckDstDataOutVld(next.entry[rid].inst->pdsts_[i], peResolve.dsts[i]);
        }
        if (GetSim()->GetViewManager(0)->config.printPipeView || GetSim()->core->pTracer->IsEnabled()) {
            next.entry[rid].inst->pipeCycle = peResolve.pipe_cycle;
            next.entry[rid].inst->iq_name = peResolve.iq_name;
            next.entry[rid].inst->pipeCycle->completeCycle = GetSim()->getCycles() + 1;
        }

        LOG_INFO_M(top->machineType, Stage::CT) << current.entry[rid].inst << " stack-get completed";
        return false;
    }
    return true;
}

bool VecPEROB::CheckStack(const PEResolveBus &peResolve, uint32_t rid)
{
    // Stack load check only
    if (peResolve.stack) {
        if (!CheckStackComplate(peResolve, rid)) {
            return false;
        }

        for (size_t i = 0; current.entry[rid].inst->pdsts_.size(); i++) {
            if (!CheckDstDataOut(current.entry[rid].inst->pdsts_[i], peResolve.dsts[i]->data, peResolve, rid)) {
                return false;
            }
        }

        LOG_INFO_M(top->machineType, Stage::CT) << current.entry[rid].inst << " stack-get correctly completed";
        top->core->flushUnit->flush_stats->stackGetCorrect++;
    } else if (!current.entry[rid].inst->stack_check &&
               current.entry[rid].inst->stack_type == StackInstType::STACK_GET) {
        top->core->flushUnit->flush_stats->stackGetCorrect++;
    }
    return true;
}

bool VecPEROB::CheckDstDataOut(const POperandPtr &dst, uint64_t peResolvDataOut,
    const PEResolveBus &peResolve, uint32_t rid)
{
    if (dst->dataVld && peResolvDataOut != dst->data) {
        FlushReq req;
        req.vld = true;
        req.bid = peResolve.bid;
        req.rid = peResolve.rid;
        req.tid = m_tid;
        ASSERT(peResolve.stid != -1U);
        req.stid = peResolve.stid;
        req.fetchTPCVld = true;
        req.fetchTPC = current.entry[rid].inst->pc;
        req.peID = peID;
        req.lsID = current.entry[rid].inst->lsID;
        req.type = FlushType::INNER_FLUSH;
        req.tSeq = current.entry[rid].inst->tSeq;
        req.uSeq = current.entry[rid].inst->uSeq;
        req.predSeq = current.entry[rid].inst->predSeq;
        req.fbid = current.entry[rid].inst->bfuInfo->fbid;
        req.fbid_local = current.entry[rid].inst->bfuInfo->fbid_local;
        // req.noSplitBlk = current.entry[rid].inst->noSplitBlk;
        req.firstInst = current.entry[rid].inst->first;
        req.iexTyp = current.entry[rid].inst->iexType;
        if (top->core->IsVectorIex(req.iexTyp)) {
            ASSERT(0);
        }
        top->ReportBlockFlush(req);
        next.entry[rid].status = INST_NEEDFLUSH;
        top->core->flushUnit->flush_stats->IntraBlockMemoryAaccelssConflict++;
        top->core->flushUnit->flush_stats->smtIntraBlockMemoryAaccelssConflictArray[req.stid]++;
        top->core->flushUnit->flush_stats->stackGetIncorrect++;
        top->stack_error_pc_q->Write(current.entry[rid].inst->pc);
        return false;
    }
    return true;
}

void VecPEROB::CheckDstDataOutVld(POperandPtr &dst, const POperandPtr &resolveDst)
{
    dst->dataVld = resolveDst->dataVld;
    if (dst->dataVld) {
        dst->data = resolveDst->data;
        if (dst->vecDataVld) {
            dst->vecData = resolveDst->vecData;
        }
    }
}

void VecPEROB::PrintTlsPipeViewLog(SimInst &inst, uint64_t addr)
{
    InstPipeViewPtr instInfo = std::make_shared<InstPipeViewInfo>();
    instInfo->bid = inst->bid.val;
    instInfo->cycleInfo = std::make_shared<CycleInfo>();
    instInfo->cycleInfo->instStartCycle = inst->pipeCycle->instStartCycle;
    instInfo->cycleInfo->sendToScalperCycle = inst->pipeCycle->sendToScalperCycle;
    instInfo->cycleInfo->sendToTileReqCycle = inst->pipeCycle->sendToTileReqCycle;
    instInfo->cycleInfo->genPrefetchCycle = inst->pipeCycle->genPrefetchCycle;
    instInfo->cycleInfo->prefetchDataRetCycle = inst->pipeCycle->prefetchDataRetCycle;
    instInfo->cycleInfo->genLoadReadReqCycle = inst->pipeCycle->genLoadReadReqCycle;
    instInfo->cycleInfo->tileDataRetCycle = inst->pipeCycle->tileDataRetCycle;
    instInfo->cycleInfo->genStoreReqCycle = inst->pipeCycle->genStoreReqCycle;
    instInfo->cycleInfo->loadDataReturnCycle = inst->pipeCycle->loadDataReturnCycle;
    instInfo->cycleInfo->tlsCompleteCycle = GetSim()->getCycles();
    instInfo->cycleInfo->retireCycle = GetSim()->getCycles() + 1;
    std::stringstream oss;
    oss << "TStore ";
    oss << " Write GM 0x" << std::hex << addr;
    instInfo->label =  oss.str();
    GetSim()->GetViewManager(inst->stid)->RecordMinst(instInfo);
}

void VecPEROB::CompleteROB(const PEResolveBus &peResolve)
{
    uint32_t rid = peResolve.rid.val;
    if (next.entry[rid].status == INST_NEEDFLUSH) {
        LOG_DEBUG_M(Unit::VECTOR, Stage::CT) << "CompleteROB need flush b:"
                                             << peResolve.bid.val <<":G0"  <<":R" << peResolve.rid.val;
        return;
    }

    if (current.entry[rid].inst->opcode == Opcode::OP_TSD) {
        next.entry[rid].inst->subInstCnt = current.entry[rid].inst->subInstCnt - 1;
        if (GetSim()->GetViewManager(next.entry[rid].inst->stid)->config.printPipeView) {
            PrintTlsPipeViewLog(current.entry[rid].inst, peResolve.dsts[DST0_IDX]->data);
        }

        LOG_INFO_M(Unit::MIEX, Stage::BROB) << " CompleteROB TStore bid: " << dec <<  peResolve.bid.val
                                         << " rid " << dec << rid
                                         << " dataOut0 :0x " << hex << peResolve.dsts[DST0_IDX]->data
                                         << " subInstCnt " << dec << next.entry[rid].inst->subInstCnt;

        if (next.entry[rid].inst->subInstCnt == 0) {
            next.entry[rid].status = INST_COMPLETED;
            return;
        }
    }

    if (!CheckStack(peResolve, rid)) {
        LOG_DEBUG_M(Unit::VECTOR, Stage::CT) << current.entry[rid].inst << " !CheckStack(peResolve, rid)";
        return;
    }

    if (!peResolve.isPipeStore) {
        next.entry[rid].status = INST_COMPLETED;
    }
    if (next.branchVld && next.branchPtr == peResolve.rid) {
        setBranch(false);
    }

    if (peResolve.srcMVld) {
        // next.entry[rid].inst->srcM.vld = true;
        // next.entry[rid].inst->srcM.simtMask = peResolve.simtMask;
    }

    // if (peResolve.dstMvld) {
        // next.entry[rid].inst->dstM.dataOut_vld = true;
        // next.entry[rid].inst->dstM.dataOut = peResolve.dstMval;
    // }

    SimInst& inst = current.entry[rid].inst;
    if (top->core->configs.simtEnable && top->core->IsVectorIex(inst->iexType) &&
        (top->core->configs.vrf_release_mode == 1U)) {
        uint32_t rtid = 0;
        if (inst->iexType == MEM_IEX) {
            rtid = inst->tid + top->core->configs.simtPeCount * top->core->configs.threadCount;
        } else {
            rtid = inst->tid;
        }
        for (auto &psrc : inst->psrcs_) {
            if (OperandTypeIsVReg(psrc->type) && !psrc->reuse && !psrc->released) {
                if (vrfRename->PreRelease(rtid, inst->stid, inst, psrc->type, psrc->ptag, psrc->recycled)) {
                    psrc->released = true;
                }
            }
        }
    }

    for (size_t i = 0; i < next.entry[rid].inst->pdsts_.size(); i++) {
        CheckDstDataOutVld(next.entry[rid].inst->pdsts_[i], peResolve.dsts[i]);
    }
    next.entry[rid].inst->pipeCycle = peResolve.pipe_cycle;
    next.entry[rid].inst->iq_name = peResolve.iq_name;
    next.entry[rid].inst->pipeCycle->completeCycle = GetSim()->getCycles() + 1;
    LOG_INFO_M(top->machineType, Stage::CT) << current.entry[rid].inst << " completed";
}

void VecPEROB::setROBInst(SimInst &inst)
{
    next.entry[inst->rid.val].inst = inst;
    return;
}

void VecPEROB::setLastBlockEnd()
{
    ROBID rid = getAllocPtr();
    SubROBID(rid, 1, next.entry.size());
    PROBEntry &uop = next.entry[rid.val];
    if (uop.status == INST_FREE || !uop.inst) {
        return;
    }
    if (uop.inst->opcode == Opcode::OP_TLD || uop.inst->opcode == Opcode::OP_TSD) {
        return;
    }
    uop.inst->isLastInBlock = true;
    uop.last = true;
}

void VecPEROB::CommitInsn()
{
    LOG_INFO_M(top->machineType, Stage::CT) << " full block retired dealloc entry " << dec << next.deallocPtr
        << current.entry[next.deallocPtr.val].inst->Dump() <<" rob size " << dec << current.size << " osd size " << dec
        << current.osdSize;

    SimInst inst = current.entry[next.deallocPtr.val].inst;
    PLpvInfo lpvInfo;
    CheckTagVld(inst, lpvInfo);

    next.entry[next.deallocPtr.val].status = INST_FREE;
    next.entry[next.deallocPtr.val].vld = false;
    IncROBID(next.deallocPtr, next.entry.size());
    ASSERT(next.size>0);
    next.size--;
}

void VecPEROB::ReportLocalRegGroupCommit(ROBID bid, ROBID gid)
{
    for (auto &threadSGPR : top->rename.sgprRenameUnit) {
        for (auto &handSGPR : threadSGPR) {
            handSGPR.ReportGroupCommit(bid, gid);
        }
    }
    for (auto &threadMask : top->rename.predMaskRenameUnit) {
        threadMask.ReportGroupCommit(bid, gid);
    }
}

void VecPEROB::ReportLocalRegBlockCommit(ROBID bid)
{
    for (auto &threadSGPR : top->rename.sgprRenameUnit) {
        for (auto &handSGPR : threadSGPR) {
            handSGPR.ReportBlockCommit(bid);
        }
    }
    for (auto &threadMask : top->rename.predMaskRenameUnit) {
        threadMask.ReportBlockCommit(bid);
    }
}

void VecPEROB::CommitBlock(PROBEntry &uop)
{
    BlockCommandPtr cmd = GetSim()->core->bctrl->blockROB.GetBlockCMDPtr(uop.inst->bid, uop.inst->stid);
    LOG_INFO_M(top->machineType, Stage::CT) << " commit block" << dec << uop.inst->bid <<" " << uop.inst; // todo fix
    cur_inst_cm_cnt = 0;
    // dealloc ROB entries
    GetSim()->core->bctrl->blockROB.reportRealBsize(uop.bid.val, &(top->peMInstStats), uop.inst->stid);
    top->peMInstStats.instCommitStats[uop.bid.val]->totalCommitInst = 0;
    for (uint32_t i = 0; i < current.entry.size(); i++) {
        if (current.entry[next.deallocPtr.val].vld
            && current.entry[next.deallocPtr.val].bid == uop.inst->bid) {
            CommitInsn();
        }
    }
    if (cmd == nullptr || IsBlockTypeParallel(cmd->blockType)) {
        return;
    }

    bool tmp = ((uop.inst->iexType == MEM_IEX) && ((uop.inst->opcode == Opcode::OP_TLD) || (uop.inst->opcode == Opcode::OP_TSD)));
    if ((peID == GetSim()->core->GetMtcPEIndex()) && tmp) {
        BlockCommandPtr cmd = GetSim()->core->bctrl->blockROB.GetBlockCMDPtr(uop.inst->bid, uop.inst->stid);
        cmd->cmdExecCompleted = true;
        ASSERT(GetSim()->core->mtcCores[0] != nullptr);
        GetSim()->core->mtcCores[0]->mtcBCCWakeupQ->Write(cmd);
    }
    top->PEBase::SetBlockComplete(uop.inst->bid, uop.inst->stid);
    CleanCMAP(uop.inst->bid);
    ReportLocalRegBlockCommit(uop.inst->bid);

    if (uop.inst->terminate) {
        top->PEBase::SetTerminate(uop.inst->bid, uop.inst->stid);
    }
}

void VecPEROB::CommitGroup(PROBEntry &uop)
{
    LOG_INFO_M(top->machineType, Stage::CT) << " commit group" << dec << uop.inst->gid <<" " << uop.inst; // todo fix
    cur_inst_cm_cnt = 0;
    // dealloc ROB entries
    // 单个group 不应该上报指令统计，应该在整个块结束之后。
    GetSim()->core->bctrl->blockROB.reportRealBsize(uop.bid.val, &(top->peMInstStats), uop.inst->stid);
    for (uint32_t i = 0; i < current.entry.size(); i++) {
        if (current.entry[next.deallocPtr.val].vld &&
            current.entry[next.deallocPtr.val].bid == uop.inst->bid &&
            current.entry[next.deallocPtr.val].gid == uop.inst->gid &&
            current.entry[next.deallocPtr.val].inst->stid == uop.inst->stid) {
            CommitInsn();
        }
    }
    uint32_t vecPEStartIdx = top->core->configs.stdPeCount;
    uint32_t vecPECnt = top->core->configs.simtPeCount;
    uint32_t memPEStartIdx = vecPEStartIdx + vecPECnt;
    uint32_t memPECnt = top->core->configs.memPeCount;
    if (m_grob && (peID >= vecPEStartIdx && peID < memPEStartIdx) && (uop.inst->biqType != BIQType::MCALL_IQ)) {
        m_grob->ReportCommitGroup(uop.inst->bid, uop.inst->gid, uop.inst->stid);
    } else if (m_mtcgrob && (peID >= memPEStartIdx && peID < memPEStartIdx + memPECnt)) {
        m_mtcgrob->ReportCommitGroup(uop.inst->bid, uop.inst->gid, uop.inst->stid);
    }

    // 清除对应的 CMAP 数据
    CleanGroupCMAP(uop.inst->bid, uop.inst->gid);
    ReportLocalRegGroupCommit(uop.inst->bid, uop.inst->gid);

    if (uop.inst->terminate) {
        top->PEBase::SetTerminate(uop.inst->bid, uop.inst->stid);
    }
}

void VecPEROB::CleanCMAP(ROBID bid)
{
    CleanCMAP(tcmap, bid);
    CleanCMAP(ucmap, bid);
    CleanCMAP(predcmap, bid);
}

void VecPEROB::CleanCMAP(RelateCmap& cmap, ROBID bid)
{
    RelateCmap newCmap;
    newCmap.type = cmap.type;
    RelateInfo info;
    while (!cmap.empty()) {
        info = cmap.read();
        if (info.bid == bid) {
            if (!info.kill) {
               SetRegReadyTable(cmap, info, SCALAR_IEX);
            }
            continue;
        }
        newCmap.write(info);
    }
    cmap = newCmap;
}

void VecPEROB::CleanGroupCMAP(ROBID bid, ROBID gid)
{
    CleanGroupCMAP(tcmap, bid, gid);
    CleanGroupCMAP(ucmap, bid, gid);
    CleanGroupCMAP(predcmap, bid, gid);
}

void VecPEROB::CleanGroupCMAP(RelateCmap& cmap, ROBID bid, ROBID gid)
{
    RelateCmap newCmap;
    RelateInfo info;
    newCmap.type = cmap.type;
    while (!cmap.empty()) {
        info = cmap.read();
        if (info.bid == bid && info.gid == gid) {
            if (info.kill) continue;
            if (top->core->IsVecPe(peID)) {
                SetRegReadyTable(cmap, info, SIMT_IEX);
            } else if (peID >= top->core->configs.stdPeCount + top->core->configs.simtPeCount) {
                SetRegReadyTable(cmap, info, MEM_IEX);
            }
            continue;
        }
        newCmap.write(info);
    }
    cmap = newCmap;
}

void VecPEROB::CommitLast(PROBEntry &uop)
{
    if (top->core->IsVectorIex(uop.inst->iexType) || ((uop.inst->iexType == MEM_IEX) &&
                   (uop.inst->biqType != BIQType::TMA_IQ))) {
        CommitGroup(uop);
    // 走MTC的Tload需要等ring的回复以后才能提交block
    } else if (uop.inst->opcode == Opcode::OP_TLD && GetSim()->core->configs.mtc_wait_ring_rsp &&
        (m_tid == 0)) {
        if (GetSim()->core->mtcCores[0]->m_scbCommitQ->count(uop.inst->bid.val) > 0) {
            CommitBlock(uop);
            GetSim()->core->mtcCores[0]->m_scbCommitQ->erase(uop.inst->bid.val);
        }
    } else {
        CommitBlock(uop);
    }
}

void VecPEROB::CheckCAExecRes(PROBEntry &uop, BlockCommandPtr &cmd, SimInst &inst)
{
    if (cmd && !IsBlockTypeParallel(cmd->blockType)) {
        GetSim()->GetVerifyManager(inst->stid)->RecordLoopManagerLastBlock(uop.inst->bid.val);
    }
    // if reduced to local gpr, do not send request to rdunit
    // do it in UOP::Execute
    if (OpcodeInInstGroup(uop.inst->opcode, InstGroup::REDUCE) && inst->DstTypeContain(OperandType::OPD_GREG)) {
        pe_iex_rd_q->Write(uop.inst);
    }
}

void VecPEROB::IncrementStats(PROBEntry &uop)
{
    // increment stats
    top->stats->minsts++;
    std::shared_ptr<IEX> iex;
    if (top->core->IsVectorIex(uop.inst->iexType)) {
        iex = top->core->vectorTop->GetIex(top->coreId);
    } else {
        iex = top->core->iex[uop.inst->iexType];
    }
    ++iex->stats->slots_retired;
    if (OpcodeIsLoad(uop.inst->opcode)) {
        top->stats->retired_load++;
    } else if (OpcodeIsStore(uop.inst->opcode)) {
        top->stats->retired_store++;
    } else if (OpcodeIsInnerJump(uop.inst->opcode)) {
        top->stats->retired_innerJump++;
    }

    if (OpcodeIsLoad(uop.inst->opcode)) {
        ++top->stats->retiredLoadInst;
    } else if (OpcodeIsStore(uop.inst->opcode)) {
        ++top->stats->retiredStoreInst;
    } else {
        // ALU
        ++top->stats->retiredAluInst;
        if (IsScalarInst(uop.inst->codeLen)) {
            ++top->stats->retiredSAluInst;
        } else {
            ++top->stats->retiredVAluInst;
        }
    }

    if (IsScalarInst(uop.inst->codeLen)) {
        top->stats->retiredScalar++;
    } else {
        top->stats->retiredVector++;
    }

    if (OpcodeIsLoad(uop.inst->opcode) || OpcodeIsStore(uop.inst->opcode)) {
        // TODO: Gather/Scatter
    }

    if (OpcodeIsDivSqrt(uop.inst->opcode)) {
        top->stats->retiredDivSqrt++;
    }

    auto checkMdbRelease = [uop](std::shared_ptr<IEX> iex) {
        iex->iexmdb.releaseConf(uop.inst->wait_store, uop.inst->addrWakeuped);
    };
    if (OpcodeIsLoad(uop.inst->opcode) && uop.inst->wait_store != -1) {
        checkMdbRelease(iex);
    }

    // When the super-block becomes the oldest, the commited registers should be
    // marked as inst_retired to avoid wrapping by rid.
    for (auto &pdst : uop.inst->pdsts_) {
        if (pdst->type == OperandType::OPD_GREG) {
            iex->rtable.setPtagRetire(pdst->ptag);
        }
    }
}

void VecPEROB::HandleEnd(PROBEntry &uop, SimInst &inst)
{
    // exit_group is sim_end
    if (!top->core->IsVectorIex(inst->iexType) && (inst->iexType != MEM_IEX)) {
        if (uop.inst->terminate) {
            top->core->bctrl->blockROB.reportTerminate(inst->bid, inst->stid);
        }
    }
}

void VecPEROB::commit()
{
    // commit ROB entries, no need to keep T registers
    uint32_t bandwith = top->configs.retireWidth;
    for (uint32_t i = 0; i < bandwith; i++) {
        PROBEntry uop = current.entry[next.commitPtr.val];
        if (uop.vld && uop.inst) {
            if (OpcodeIsLoad(uop.inst->opcode)) {
                top->stats->sLoadInstsCycles += (IsScalarInst(uop.inst->codeLen)) ? 0 : 1;
                top->stats->vLoadInstsCycles += (IsScalarInst(uop.inst->codeLen)) ? 1 : 0;
            } else if (OpcodeIsStore(uop.inst->opcode)) {
                top->stats->sStoreInstsCycles += (IsScalarInst(uop.inst->codeLen)) ? 0 : 1;
                top->stats->vStoreInstsCycles += (IsScalarInst(uop.inst->codeLen)) ? 1 : 0;
            } else {
                top->stats->sAluInstsCycles += (IsScalarInst(uop.inst->codeLen)) ? 0 : 1;
                top->stats->vAluInstsCycles += (IsScalarInst(uop.inst->codeLen)) ? 1 : 0;
            }
        }

        if (!(uop.vld && uop.status == INST_COMPLETED)) {
            break;
            LOG_ERROR << " retire entry fail " << dec << next.commitPtr <<" " << uop.inst
                << " size " << dec << current.size << " osd size " << dec << current.osdSize;
        }

        LOG_INFO_M(top->machineType, Stage::RE) << " retire entry " << dec << next.commitPtr <<" " << uop.inst
            << " size " << dec << current.size << " osd size " << dec << current.osdSize;
        next.entry[next.commitPtr.val].status = INST_RETIRED;
        if (GetSim()->GetViewManager(next.entry[next.commitPtr.val].inst->stid)->config.printPipeView &&
            (uop.inst->opcode != Opcode::OP_TLD) && (uop.inst->opcode != Opcode::OP_TSD)) {
            next.entry[next.commitPtr.val].inst->pipeCycle->retireCycle = GetSim()->getCycles() + 1;
        }
        if (uop.inst->first) {
            cur_inst_cm_cnt = 0;
        }
        cur_inst_cm_cnt++;
        retire_inst_cnt++;
        // Record the number of different types of minsts
        ReportInstCnt(uop.inst);
        RecordTrace(uop.inst);

        ReportLocalRegStats(uop.inst, uop.bid.val, &(top->peMInstStats));
        SimInst &inst = next.entry[next.commitPtr.val].inst;
        ASSERT(next.osdSize>0);
        next.osdSize--;
        IncrementStats(uop);

        BlockCommandPtr cmd = GetSim()->core->bctrl->blockROB.GetBlockCMDPtr(uop.inst->bid, uop.inst->stid);
        CheckCAExecRes(uop, cmd, inst);
        next.commitBid = inst->bid;
        uint64_t robDepth =  top->configs.vpeROBDepth;
        if (OpcodeIsStore(inst->opcode)) {
                youngest_noncommit_sid = uop.inst->sid;
                tileStoreCredit++;
        }

        if (OpcodeIsLoad(inst->opcode)) {
            tileLoadCredit++;
        }
        IncROBID(next.commitPtr, robDepth);

        const uint32_t maxInstCount = 512;
        if (!uop.last && cur_inst_cm_cnt >= maxInstCount &&
            !GetSim()->core->bctrl->blockROB.needFlush(uop.bid, uop.inst->stid)) {
            cur_inst_cm_cnt = 0;
            break;
        }
    }
}

void VecPEROB::dealloc()
{
    // dealloc ROB entries, no need to keep T registers
    uint32_t bandwith = top->configs.retireWidth;
    for (uint32_t i = 0; i < bandwith; i++) {
        PROBEntry uop = current.entry[next.deallocPtr.val];
        if (uop.vld && uop.status == INST_RETIRED) {
            LOG_INFO_M(top->machineType, Stage::RE) << " ROB: check retire dealloc entry "
                << next.deallocPtr << " " << current.entry[next.deallocPtr.val].inst <<" rob size "
                << dec << current.size << " osd size " << current.osdSize;
            if (uop.inst->stack) {
                top->core->sRenameUnit->stackRetire(uop.inst->bid, uop.inst->rid, uop.inst->lsID);
            }
            if (peID >= GetSim()->core->configs.stdPeCount) {
                SetCommitBusID(uop.inst);
            }
            ReleaseRelative(uop);
            HandleEnd(uop, uop.inst);
            MinstResVerify(uop);
            if (uop.inst->opcode != Opcode::OP_BSTOP) {
                MinstPipeView(uop);
            }
            if (uop.last && !GetSim()->core->bctrl->blockROB.needFlush(uop.bid, uop.inst->stid) && \
                    uop.inst->biqType == BIQType::MCALL_IQ) {
                CommitLast(uop);
                top->vectorCore->m_vectorGS->Resolve(uop.inst->bid, uop.inst->gid);
                break;
            } else if (uop.last && !GetSim()->core->bctrl->blockROB.needFlush(uop.bid, uop.inst->stid) && \
                    uop.inst->biqType != BIQType::MCALL_IQ) {
                CommitLast(uop);
                break;
            }
            if (uop.inst->biqType == BIQType::MCALL_IQ && uop.last) {
                top->vectorCore->m_vectorGS->Resolve(uop.inst->bid, uop.inst->gid);
                break;
            }
            next.entry[next.deallocPtr.val].status = INST_FREE;
            next.entry[next.deallocPtr.val].vld = false;
            IncROBID(next.deallocPtr, next.entry.size());
            ASSERT(next.size>0);
            next.size--;
        } else {
            break;
        }
    }
}

bool VecPEROB::CheckFlushReqBid(FlushBus &flushReq)
{
    if ((flushReq.baseOnBid && LessEqual(flushReq.req.bid, next.commitBid)) ||
        (!flushReq.baseOnBid && LessROBID(flushReq.req.bid, next.commitBid))) {
        return true;
    }
    return false;
}

void VecPEROB::CheckFlushReq(FlushBus &flushReq, const ROBID fbid)
{
    if (flushReq.baseOnBid) {
        top->peMInstStats.ReleaseEntry(fbid.val);
    }

    ROBID bid = top->core->bctrl->blockROB.getOldestBlockID(flushReq.req.stid);
    if (CheckFlushReqBid(flushReq) || (bid == next.commitBid && bid == next.commitBid)) {
        cur_inst_cm_cnt = 0;
    }
    if (CheckFlushReqBid(flushReq)) {
        top->peMInstStats.instCommitStats[bid.val]->totalCommitInst = 0;
    }
}

void VecPEROB::HandleNextEntryVldAndLessEq(ROBID &old_alloc, ROBID &next_alloc, const ROBID &ptr, bool &found,
    SimInst &oldestFlushInst)
{
    if (!found) {
        /* (old_alloc, next_alloc] is the range of flushed non-retired Minst */
        old_alloc = next.allocPtr;
        next_alloc = ptr;
        next.allocPtr = ptr;
        found = true;
        const uint32_t tid = next.entry[ptr.val].inst->tid;
        auto &sgpr = top->rename.sgprRenameUnit[tid];
        sgpr[SGPRType2Idx(OperandType::OPD_TLINK)].SetCurSeq(next.entry[ptr.val].inst->tSeq);
        sgpr[SGPRType2Idx(OperandType::OPD_ULINK)].SetCurSeq(next.entry[ptr.val].inst->uSeq);
        top->rename.predMaskRenameUnit[tid].SetCurSeq(next.entry[ptr.val].inst->predSeq);
        if (!oldestFlushInst || LessEqual(next.entry[ptr.val].inst->bid, next.entry[ptr.val].inst->rid,
            oldestFlushInst->bid, oldestFlushInst->rid)) {
                oldestFlushInst = next.entry[ptr.val].inst;
        }
    }
}

// TODO:: Set simt flag for module
ExecEngineTyp VecPEROB::GetIexType()
{
    if (peID < GetSim()->core->configs.stdPeCount) {
        return SCALAR_IEX;
    }

    if (peID >= (GetSim()->core->configs.stdPeCount + GetSim()->core->configs.simtPeCount)) {
        return MEM_IEX;
    }
    return SIMT_IEX;
}

void VecPEROB::FlushRelativeReg(const FlushBus &flushReq, RelateCmap &cmap)
{
    ExecEngineTyp iexTpye = GetIexType();
    while (!cmap.empty()) {
        bool localNeedFlush = flushReq.baseOnBid ? LessEqual(flushReq.req.bid, cmap.back().bid) :
                        LessROBID(flushReq.req.bid, cmap.back().bid);
        if (!localNeedFlush) {
            break;
        }
        RelateInfo info = cmap.pop_back();
        SetRegReadyTable(cmap, info, iexTpye);
    }
}

void VecPEROB::CheckNextEntryStatus(const ROBID &ptr)
{
    if (next.entry[ptr.val].status == INST_ALLOCATED ||
        next.entry[ptr.val].status == INST_RENAMED ||
        next.entry[ptr.val].status == INST_ISSUED ||
        next.entry[ptr.val].status == INST_COMPLETED ||
        next.entry[ptr.val].status == INST_NEEDFLUSH) {
        ASSERT(next.osdSize>0);
        next.osdSize--;
    }

    next.stall = false;
    next.entry[ptr.val].vld = false;
}

void VecPEROB::CheckTagVld(SimInst &inst, PLpvInfo &lpvInfo)
{
    std::shared_ptr<IEX> iex;
    if (GetSim()->core->IsVectorIex(inst->iexType)) {
        iex = GetSim()->core->vectorTop->GetIex(top->coreId);
    } else {
        iex = GetSim()->core->iex[inst->iexType];
    }
    for (auto &pdst : inst->pdsts_) {
        if (OperandTypeIsLocalReg(pdst->type)) {
            iex->SetRegReadyTable(pdst->type, pdst->ptag, false, lpvInfo, inst->peID, inst->tid);
        }
    }
}

void VecPEROB::CheckDstTagVld(SimInst &inst)
{
    // for (auto &pdst : inst->pdsts_) {
    //     if (pdst->type == OperandType::OPD_GREG && pdst->renamed) {
    //         top->core->bctrl->blockRenameUnit.setMapTableData(pdst->ptag, 0, false);
    //     }
    // }
    PLpvInfo lpvInfo;
    CheckTagVld(inst, lpvInfo);

    std::shared_ptr<IEX> iex;
    if (GetSim()->core->IsVectorIex(inst->iexType)) {
        iex = GetSim()->core->vectorTop->GetIex(top->coreId);
    } else {
        iex = GetSim()->core->iex[inst->iexType];
    }
    iex->rtable.setROBReadyTable(inst->peID, inst->rid.val, lpvInfo, false);
}

void VecPEROB::SetNextBranchVld(bool hasInnerBR, const ROBID lastInnerPtr)
{
    if (hasInnerBR) {
        next.branchVld = true;
        next.branchPtr = lastInnerPtr;
    } else {
        next.branchVld = false;
    }
}

void VecPEROB::HandleNextEntryVldAndFound(FlushBus &flushReq, const ROBID ptr, const ROBID fbid)
{
    CheckNextEntryStatus(ptr);
    SimInst inst = next.entry[ptr.val].inst;
    // Update reference pointer
    // if (inst->ref.gsInfo.ref_vld && !flushReq.baseOnBid && fbid == next.entry[ptr.val].bid) {
    //     GetSim()->refInfo.refTrace.entry[next.entry[ptr.val].bid.val].decGSRPtr();
    // }
    // if (inst->ref.lsInfo.ref_vld && !flushReq.baseOnBid && fbid == next.entry[ptr.val].bid) {
    //     GetSim()->refInfo.refTrace.entry[inst->bid.val].decLSRPtr();
    //     if (OpcodeIsStore(inst->opcode)) {
    //         GetSim()->recoverRefStq(inst->ref.lsInfo.id);
    //     }
    // }

    if (inst->pdsts_.size() > DST1_IDX && inst->pdsts_[DST1_IDX]->type == OperandType::OPD_GREG && inst->renamed &&
        inst->pdsts_[DST1_IDX]->value == 1) {
        next.stackVld = true;
    }

    CheckDstTagVld(inst);

    next.entry[ptr.val].status = INST_FREE;
}

void VecPEROB::SetNextVal()
{
    if (!next.size) {
        next.commitPtr = next.allocPtr;
        next.deallocPtr = next.allocPtr;
        next.stackVld = true;
    }
    if (!next.osdSize) {
        next.commitPtr = next.allocPtr;
    }

    next.stall = next.size + top->configs.decodeWidth > top->configs.vpeROBDepth;
}

bool VecPEROB::GetRobPreAllocStall(unsigned tidNum, unsigned bundleInstNum)
{
    // only For vector/mtc
    // bundleInstNum: fetch buddle max inst number
    ASSERT(bundleInstNum > 0);
    return current.size + (bundleInstNum * (tidNum + 1)) > top->configs.vpeROBDepth;
}

bool VecPEROB::GetRobRealAllocStall()
{
    return next.size >= top->configs.vpeROBDepth;
}

void VecPEROB::HandleROBID(FlushBus &flushReq, ROBID fbid, ROBID frid, ROBID ptr)
{
    ROBID resetPtr = next.deallocPtr;
    for (uint32_t i = 0; i < next.entry.size(); ++i) {
        if (!next.entry[resetPtr.val].vld) break;
        bool lessEq = flushReq.baseOnBid ? LessEqual(fbid, next.entry[ptr.val].bid):
            LessEqual(fbid, frid, next.entry[ptr.val].bid, next.entry[ptr.val].rid);
        if (OpcodeIsInnerJump(next.entry[resetPtr.val].inst->opcode) && lessEq &&
            (next.entry[resetPtr.val].status == INST_RENAMED ||
             next.entry[resetPtr.val].status == INST_ISSUED)) {
            next.branchVld = true;
            next.branchPtr = resetPtr;
        }
        IncROBID(resetPtr, next.entry.size());
    }
}

void VecPEROB::flush(FlushBus flushReq)
{
    // flush the ROB state
    ROBID fbid = flushReq.req.bid;
    ROBID frid = flushReq.req.rid;
    uint32_t stid = flushReq.req.stid;

    bool found = false;
    bool overCommitPtr = false;
    ROBID old_alloc;
    ROBID next_alloc;
    bool hasInnerBR = false;
    ROBID lastInnerPtr;

    CheckFlushReq(flushReq, fbid);

    ROBID ptr = next.deallocPtr;
    SimInst oldestFlushInst = std::shared_ptr<SimInstInfo>(nullptr);
    for (uint32_t i = 0; i < next.entry.size(); i++) {
        if (ptr == next.commitPtr) overCommitPtr = true;
        bool lessEq = false;
        if (next.entry[ptr.val].inst->stid == stid) {
            lessEq = flushReq.baseOnBid ? LessEqual(fbid, next.entry[ptr.val].bid):
            LessEqual(fbid, frid, next.entry[ptr.val].bid, next.entry[ptr.val].rid);
        }
        if (LessEqual(fbid, next.entry[ptr.val].bid)) {
            top->peMInstStats.instCommitStats[next.entry[ptr.val].bid.val]->totalCommitInst = 0;
        }
        if (next.entry[ptr.val].vld && lessEq) {
            HandleNextEntryVldAndLessEq(old_alloc, next_alloc, ptr, found, oldestFlushInst);
            // Just for inner perfectBP debug
            if (OpcodeIsStore(next.entry[ptr.val].inst->opcode)) {
                youngest_noncommit_sid = next.entry[ptr.val].inst->sid;
                tileStoreCredit++;
            }

            if (OpcodeIsStore(next.entry[ptr.val].inst->opcode) &&
                top->core->IsVecPe(next.entry[ptr.val].inst->peID)) {
                UpdateTileLsuWindowPos(next.entry[ptr.val].inst->vcSid);
            }

            if (OpcodeIsLoad(next.entry[ptr.val].inst->opcode)) {
                tileLoadCredit++;
            }
            if (flushReq.baseOnBid || fbid != next.entry[ptr.val].bid) {
                top->peMInstStats.ReleaseEntry(next.entry[ptr.val].bid.val);
            }
            if (!overCommitPtr) next.commitPtr = ptr;
        } else if (next.entry[ptr.val].vld && (next.entry[ptr.val].status == INST_ALLOCATED ||
                    next.entry[ptr.val].status == INST_RENAMED || next.entry[ptr.val].status == INST_ISSUED)
                    && OpcodeIsInnerJump(next.entry[ptr.val].inst->opcode)) {
            hasInnerBR = true;
            lastInnerPtr = ptr;
        }

        if (next.entry[ptr.val].vld && found) {
            ASSERT(next.size>0);
            next.size--;
            if (next.branchVld && ptr==next.branchPtr) {
                SetNextBranchVld(hasInnerBR, lastInnerPtr);
            }
            HandleNextEntryVldAndFound(flushReq, ptr, fbid);
        }
        IncROBID(ptr, next.entry.size());
    }

    SetNextVal();

    HandleROBID(flushReq, fbid, frid, ptr);

    // Relative register flush
    FlushRelativeReg(flushReq, tcmap);
    FlushRelativeReg(flushReq, ucmap);
    FlushRelativeReg(flushReq, predcmap);
    needFlush = false;
}

void VecPEROB::PrintStatus()
{
    if (LoggerManager::GetManager().level > LoggerLevel::DETAIL) {
        return;
    }
    uint64_t robDepth = top->configs.vpeROBDepth;
    LOG_DETAIL_M(top->machineType, Stage::ROB) << "==================================================";
    LOG_DETAIL_M(top->machineType, Stage::ROB) << "PEID:" << std::dec << peID << " tid:" << m_tid << ", size "
        << current.size <<" osdSize " << current.osdSize;
    if (current.size == 0) {
        return;
    }
    for (uint32_t i = 0; i < robDepth; i++) {
        uint32_t ptr = (current.deallocPtr.val+i) % robDepth;
        if (!current.entry[ptr].vld) {
            continue;
        }
        std::stringstream oss;
        oss << "\t|-- " << current.entry[ptr].Dump();
        if (current.commitPtr.val == ptr) {
            oss << "<- oldest";
        }
        if (current.entry[ptr].inst->isLastInBlock) {
            oss << "<- stop of B" << current.entry[ptr].bid;
        }
        LOG_DETAIL_M(top->machineType, Stage::ROB) << oss.str();
    }
    LOG_DETAIL_M(top->machineType, Stage::ROB) << "==================================================";
}

void VecPEROB::CheckReg(PROBEntry &uop, RelateCmap &cmap, RelateInfo &info)
{
    PLpvInfo lpvInfo;
    SimInst &inst = uop.inst;
    std::shared_ptr<IEX> iex;
    if (GetSim()->core->IsVectorIex(uop.inst->iexType)) {
        iex = GetSim()->core->vectorTop->GetIex(top->coreId);
    } else {
        iex = GetSim()->core->iex[uop.inst->iexType];
    }
    auto releaseReg = [this](PROBEntry &uop, RelateCmap &cmap, RelateInfo &info) {
        top->rename.ReportLocalKill(cmap.type, info.seq.val, info.tid);
    };
    LOG_INFO_M(Unit::VECTOR, Stage::RE) << "Reg retire " << inst->Dump();
    if (uop.last || uop.inst->threadSwitchEnd ||
        (!cmap.empty() && (cmap.back().bid != inst->bid || cmap.back().gid != inst->gid))) {
        while (!cmap.empty()) {
            info = cmap.read();
            if (!(info.bid == inst->bid && info.gid == inst->gid)) {
                break;
            }
            if (info.kill) {
                continue;
            }
            switch (cmap.type) {
                case OperandType::OPD_TLINK:
                case OperandType::OPD_ULINK:
                case OperandType::OPD_PREDMASK:
                    iex->SetRegReadyTable(cmap.type, info.tag, false, lpvInfo, info.peid, info.tid);
                    ASSERT(info.bid == inst->bid && info.gid == inst->gid);
                    releaseReg(uop, cmap, info);
                    break;
                case OperandType::OPD_VTLINK:
                case OperandType::OPD_VULINK:
                case OperandType::OPD_VMLINK:
                case OperandType::OPD_VNLINK:
                    iex->SetRegReadyTable(cmap.type, info.tag, false, lpvInfo);
                    releaseReg(uop, cmap, info);
                    break;
                default:
                    break;
            }
        }
    }
}

void VecPEROB::CheckRelativeReg(PROBEntry &uop, RelateInfo &info)
{
    CheckReg(uop, tcmap, info);
    CheckReg(uop, ucmap, info);
    CheckReg(uop, predcmap, info);
}

void VecPEROB::SetRegReadyTable(RelateCmap &cmap, RelateInfo &info, ExecEngineTyp iexTyp)
{
    PLpvInfo lpvInfo;
    std::shared_ptr<IEX> iex;
    if (GetSim()->core->IsVectorIex(iexTyp)) {
        iex = GetSim()->core->vectorTop->GetIex(top->coreId);
    } else {
        iex = GetSim()->core->iex[iexTyp];
    }

    switch (cmap.type) {
        case OperandType::OPD_TLINK:
        case OperandType::OPD_ULINK:
        case OperandType::OPD_VTLINK:
        case OperandType::OPD_VULINK:
        case OperandType::OPD_VMLINK:
        case OperandType::OPD_VNLINK:
        case OperandType::OPD_PREDMASK:
            iex->SetRegReadyTable(cmap.type, info.tag, false, lpvInfo, info.peid, info.tid);
            break;
        default:
            break;
    }
}

void VecPEROB::ReleaseFunc(PROBEntry &uop, RelateCmap &cmap, RelateInfo &info)
{
    bool release = uop.last || uop.inst->threadSwitchEnd;
    ASSERT(info.peid >= top->core->configs.stdPeCount);
    cmap.write(info);
    top->rename.RepLocalRetired(cmap.type, info.seq, false, info.tid);
    release |= (cmap.size() - 1 >= LOGIC_UT_COUNT_4);
    if (release && !lock) {
        RelateInfo rInfo = cmap.read();
        if (!(rInfo.kill)) {
            SetRegReadyTable(cmap, rInfo, uop.inst->iexType);
        }

        top->rename.ReportLocalKill(cmap.type, rInfo.seq.val, rInfo.tid);
    }
}

void VecPEROB::ReportLocalRegKill(PROBEntry &uop, RelateCmap &cmap, RelateInfo &info)
{
    if (info.peid < top->core->configs.stdPeCount) {
        // kill only valid for vector register
        top->core->bctrl->blockROB.reportException(uop.bid, uop.inst->stid, "Register killed by scalarPE");
        return;
    }
    bool found = false;
    RelateCmap newCmap;
    newCmap.type = cmap.type;
    bool recycled = false;
    while (!cmap.empty()) {
        RelateInfo tInfo = cmap.read();
        if (tInfo.tag == info.tag) {
            ASSERT(!found);
            found = true;
            tInfo.kill = true;
            recycled = tInfo.recycled;
        }
        newCmap.write(tInfo);
    }
    cmap = newCmap;
    if (found) {
        LOG_ERROR << "ReportLocalRegKill vrfRename kill";
        top->vrfRename->Kill(uop.inst->tid, uop.inst->stid, uop.inst, cmap.type, info.seq.val, recycled);
    }
}

void VecPEROB::SetRelateInfo(RelateInfo &info, uint32_t tag, ROBID &seq)
{
    info.vld = true;
    info.tag = tag;
    info.seq = seq;
}

void VecPEROB::ReleaseRelative(PROBEntry &uop)
{
    PLpvInfo lpvInfo;
    SimInst &inst = uop.inst;
    RelateInfo info = RelateInfo();
    info.bid = inst->bid;
    info.gid = inst->gid;
    info.peid = inst->peID;
    info.tid = inst->tid;
    info.rid = inst->rid;
    std::shared_ptr<IEX> iex;
    if (GetSim()->core->IsVectorIex(uop.inst->iexType)) {
        iex = GetSim()->core->vectorTop->GetIex(top->coreId);
    } else {
        iex = GetSim()->core->iex[uop.inst->iexType];
    }
    if (GetSim()->core->IsVectorIex(uop.inst->iexType) || (uop.inst->iexType == MEM_IEX)) {
        info.logic_long = true;
    }
    uint32_t rtid;
    if (uop.inst->iexType == MEM_IEX) {
        rtid = inst->tid + top->core->configs.simtPeCount * top->core->configs.threadCount;
    } else {
        rtid = inst->tid;
    }
    CheckRelativeReg(uop, info);

    if (GetSim()->core->configs.simtEnable) {
        for (auto &psrc : inst->psrcs_) {
            if (OperandTypeIsVReg(psrc->type) && !psrc->reuse && !psrc->released) {
                vrfRename->Kill(rtid, inst->stid, inst, psrc->type, psrc->ptag, psrc->recycled);
                psrc->released = true;
            }
        }
    }

    ASSERT(inst->peID >= top->core->configs.stdPeCount);
    for (auto &pdst : inst->pdsts_) {
        switch (pdst->type) {
            case OperandType::OPD_TLINK:
                SetRelateInfo(info, pdst->ptag, inst->tSeq);
                ReleaseFunc(uop, tcmap, info);
                break;
            case OperandType::OPD_ULINK:
                SetRelateInfo(info, pdst->ptag, inst->uSeq);
                ReleaseFunc(uop, ucmap, info);
                break;
            case OperandType::OPD_VTLINK:
            case OperandType::OPD_VULINK:
            case OperandType::OPD_VMLINK:
            case OperandType::OPD_VNLINK: {
                if (GetSim()->core->configs.simtEnable) {
                    bool end = uop.last || uop.inst->threadSwitchEnd;
                    vrfRename->Retire(rtid, inst->stid, inst, pdst->ptag, pdst->recycled, end);
                }
                break;
            }
            case OperandType::OPD_PREDMASK:
                SetRelateInfo(info, pdst->ptag, inst->predSeq);
                ReleaseFunc(uop, predcmap, info);
                break;
            default:
                break;
        }
    }

    iex->rtable.setROBReadyTable(inst->peID, inst->rid.val, lpvInfo, false);
}

void VecPEROB::HandleInstCntByType(const Opcode &opcode, ROBID instBid)
{
    top->peMInstStats.instCommitStats[instBid.val]->totalCommitInst++;
    InstGroup grp = OpcodeManager::Inst().GetOpcodeGroup(opcode);
    top->peMInstStats.instCommitStats[instBid.val]->instGroupCommitInstNum[grp]++;
}

void VecPEROB::ReportInstCnt(const SimInst &inst)
{
    if (!inst) {
        return;
    }
    ROBID instBid = inst->bid;
    Opcode opcode = inst->opcode;

    if (inst->autogen) {  // automatically generated bend are not counted in the number of minsts
        return;
    }
    if (inst->isSysStateInst) {  // the system-state insts are not counted in the number of minsts
        return;
    }
    HandleInstCntByType(opcode, instBid);
}

void VecPEROB::RecordTrace(const SimInst &inst) const
{
    if (!top->core->configs.dump_inst_trace || !inst) {
        return;
    }
    string peName;
    if (peID < top->core->configs.stdPeCount) {
        peName = "scalar";
    } else if (top->core->IsVecPe(peID)) {
        peName = "vector";
    } else if (peID >= top->core->configs.stdPeCount + top->core->configs.simtPeCount) {
        peName = "mtc";
    } else {
        ASSERT(false);
    }
    auto info = make_shared<InstDumpInfo>(inst);
    info->peName = peName;
    top->core->pTracer->Push(inst->bid, info);
    top->core->pTracer->PushInstrEvent(inst->bid, inst, InstrEvent::COMMIT);
}

void VecPEROB::ReportLocalRegStats(const SimInst &inst, const uint32_t bid, OPEState *opeState)
{
    if (!inst) {
        return;
    }

    auto calLocalSrcRegRobDist = [&bid, opeState](POperandPtr &src) {
        if (!src->IsLocalReg()) {
            return;
        }
        const uint64_t linkMaxSize = 8;
        ASSERT(src->value < linkMaxSize && "The index of local reg must be smaller than 8!");
        opeState->instRelativeRegStats[bid]->regRelativeDist[src->type][src->value]++;
        opeState->instRelativeRegStats[bid]->regRobDist[src->type][src->value] += src->localRegRobDist;
    };

    if (!inst->psrcs_.empty()) {
        for (auto &psrc : inst->psrcs_) {
            if (OperandTypeIsLocalReg(psrc->type)) {
                calLocalSrcRegRobDist(psrc);
            }
        }
    }

}

void VecPEROB::MinstResVerify(PROBEntry &entry)
{
    InstVerifyInfo info;
    info.isReferenc = false;
    info.autoGen = entry.inst->autogen;
    info.terminate = entry.last;

    info.bid = entry.inst->bid.val;
    info.tid = entry.inst->tid;
    info.lgid = entry.inst->logicalGID;
    info.gid = entry.inst->gid.val;
    info.rid = entry.inst->rid.val;
    info.coreId = peID;
    info.tpc = entry.inst->pc;
    info.opcode = entry.inst->opcode;
    // 可能存在48bit 双输出指令，后续适配。
    info.data = entry.inst->pdsts_.empty() ? 0 : entry.inst->pdsts_[DST0_IDX]->data;

    if (IsScalarInst(entry.inst->codeLen)) {
        if ((!top->core->IsVectorIex(top->iexType)) && (top->iexType != MEM_IEX)) {
            GetSim()->GetVerifyManager(entry.inst->stid)->RecordMinst(info);
        } else {
            GetSim()->GetVerifyManager(entry.inst->stid)->RecordPARMinst(info);
        }
    } else {
        // Normal simt inst
        for (uint32_t lane = 0; lane < top->core->configs.simtLane; ++lane) {
            if (lane < entry.inst->lanes) {
                info.data = entry.inst->pdsts_.empty() ? 0 :
                                (entry.inst->pdsts_[0]->vecDataVld ? entry.inst->pdsts_[0]->vecData.Get(lane) : 0);
            } else {
                info.data = 0;
            }
            info.lane = lane;
            info.isLastLane = (lane == top->core->configs.simtLane - 1);
            info.isSIMTMinst = true;
            GetSim()->GetVerifyManager(entry.inst->stid)->RecordPARMinst(info);
        }
    }
}

void VecPEROB::MinstPipeView(PROBEntry &entry)
{
    InstPipeViewPtr instInfo = std::make_shared<InstPipeViewInfo>();
    instInfo->bid = entry.bid.val;
    instInfo->tpc = entry.tpc;
    instInfo->logicGid = entry.inst->logicalGID;
    instInfo->cycleInfo = std::make_shared<CycleInfo>(*(entry.inst->pipeCycle));
    instInfo->label = MachineName(top->machineType) + " " + to_string(top->coreId) + entry.inst->DumpPipeViewInfo();
    if (IsScalarInst(entry.inst->codeLen)) {
        if ((top->iexType != SIMT_IEX) && (top->iexType != MEM_IEX)) {
            GetSim()->GetViewManager(entry.inst->stid)->RecordMinst(instInfo);
        } else if ((entry.inst->opcode == Opcode::OP_TLD) || (entry.inst->opcode == Opcode::OP_TSD)) {
            GetSim()->GetViewManager(entry.inst->stid)->RecordMinst(instInfo);
        } else {
            GetSim()->GetViewManager(entry.inst->stid)->RecordPARMinst(instInfo);
        }
    } else {
        GetSim()->GetViewManager(entry.inst->stid)->RecordPARMinst(instInfo);
    }
}

void VecPEROB::UpdateTileLsuWindowPos(ROBID sid)
{
    LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "update T-" << m_tid << ", sid=" << sid.val << " oldestRetiredSid:" <<
        oldestRetiredSid.val;
    if (!GetSim()->GetCore()->configs.load_ooo_enable) {
        ASSERT(oldestRetiredSid.val == sid.val);
    }
    IncROBID(oldestRetiredSid, top->configs.vcSidMax);
    IncROBID(windowEndSid, top->configs.vcSidMax);

    for (uint64_t i = 0; i < vcSidWindow.size()-1; i++) {
        vcSidWindow[i] = vcSidWindow[i+1];
    }
    vcSidWindow[vcSidWindow.size()-1] = false;

    ldCanIssMaxSid = oldestRetiredSid;
    for (uint64_t i = 0; i < vcSidWindow.size(); i++) {
        if (vcSidWindow[i] == true) {
            IncROBID(ldCanIssMaxSid, top->configs.vcSidMax);
        } else {
            break;
        }
    }
}

void VecPEROB::UpdateTileLsuIssueWindowPos(SimInst &inst)
{
    assert(inst->vcSid >= oldestRetiredSid && inst->vcSid <= windowEndSid);
    if ((inst->vcSid.val - oldestRetiredSid.val) >= vcSidWindow.size()) {
        vcSidWindow.at(inst->vcSid.val + top->configs.vcSidMax - oldestRetiredSid.val) = true;
    } else {
        vcSidWindow.at(inst->vcSid.val - oldestRetiredSid.val) = true;
    }

    ldCanIssMaxSid = oldestRetiredSid;
    for (uint64_t i = 0; i < vcSidWindow.size(); i++) {
        if (vcSidWindow[i] == true) {
            IncROBID(ldCanIssMaxSid, top->configs.vcSidMax);
        } else {
            break;
        }
    }
}

void VecPEROB::Stats()
{
    if (!current.size) {
        return;
    }

    if (GetSim()->core->IsVecPe(top->machineType) || GetSim()->core->IsMtcPe(peID)) {
        ++top->stats->vecThreadRobBussyCyc[m_tid];
        top->stats->vecThreadRobBussyInstNum[m_tid] += current.size;
        ++top->stats->vecTotalRobBussyCyc;
        top->stats->vecTotalRobBussyInstNum += current.size;
    }
}

} // namespace JCore
