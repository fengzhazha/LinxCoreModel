#include "VecPERename.h"

#include "core/Core.h"
#include "VecPE.h"

namespace JCore {
using namespace std;
void VecPERename::Work()
{
    Rename();
    ToDispatch();
}

void VecPERename::Xfer()
{
    dec_ren_q->Work();
}

void VecPERename::Reset()
{

}

void VecPERename::ReportStat()
{

}

SimSys* VecPERename::GetSim()
{
    return vpeTop->sim;
}

void VecPERename::Build()
{
    uint32_t threadCnt = vpeTop->GetThread();
    sgprRenameUnit = std::vector<std::vector<LocalRegMgr>>(threadCnt,
                     std::vector<LocalRegMgr>(SGPR_HAND_COUNT));
    predMaskRenameUnit = std::vector<LocalRegMgr>(threadCnt);
    for (uint32_t tid = 0; tid < threadCnt; ++tid) {
        auto &tgpr = sgprRenameUnit[tid][SGPRType2Idx(OperandType::OPD_TLINK)];
        auto &ugpr = sgprRenameUnit[tid][SGPRType2Idx(OperandType::OPD_ULINK)];
        tgpr.Init(sim->core->configs.simt_local_reg_t, vpeTop->configs.vpeROBDepth,
            OperandType::OPD_TLINK, vpeTop->peID, sim->core->configs.scalar_local_reg_width);
        ugpr.Init(sim->core->configs.simt_local_reg_u, vpeTop->configs.vpeROBDepth,
            OperandType::OPD_ULINK, vpeTop->peID, sim->core->configs.scalar_local_reg_width);
        predMaskRenameUnit[tid].Init(sim->core->configs.simt_local_reg_p, vpeTop->configs.vpeROBDepth,
            OperandType::OPD_PREDMASK, vpeTop->peID, sim->core->configs.scalar_local_reg_width);
        tgpr.sim = sim;
        ugpr.sim = sim;
        tgpr.SetTid(tid);
        ugpr.SetTid(tid);
        predMaskRenameUnit[tid].sim = sim;
        predMaskRenameUnit[tid].SetTid(tid);
    }
}

void VecPERename::Rename()
{
    if (dec_ren_q->Empty()) {
        return;
    }
    uint64_t width = vpeTop->configs.renameWidth;
    // 重命名宽度目前按照指令个数来。有需要也可以加operand 的个数。
    for (uint64_t i = 0; i < width && !dec_ren_q->Empty() && !CheckStall(dec_ren_q->Front()); i++) {
        SimInst inst = dec_ren_q->Front();
        bool exception = false;
        for (auto &psrc : inst->psrcs_) {
            if (!RenameSrcPOperand(inst, psrc)) {
                dec_ren_q->Read();
                exception = true;
                break;
            }
        }
        if (exception) {
            continue;
        }
        auto &sgprRename = sgprRenameUnit[inst->tid];
        inst->tSeq = sgprRename[SGPRType2Idx(OperandType::OPD_TLINK)].GetCurSeq();
        inst->uSeq = sgprRename[SGPRType2Idx(OperandType::OPD_ULINK)].GetCurSeq();
        inst->predSeq = predMaskRenameUnit[inst->tid].GetCurSeq();
        for (auto &pdst : inst->pdsts_) {
            if (!RenameDstPOperand(inst, pdst)) {
                dec_ren_q->Read();
                exception = true;
                break;
            }
        }
        if (exception) {
            continue;
        }
        dec_ren_q->Read();
        inst->pipeCycle->renameCycle = GetSim()->getCycles();
        inst->pipeCycle->d3Cycle = inst->pipeCycle->renameCycle + 1ULL;
        d2_s1_q.push_back(inst);
    }
}

void VecPERename::ToDispatch()
{
    dec_ren_q->unsetStall();
    while (!d2_s1_q.empty() && !dec_ren_q->getStall()) {
        if (DispatchToVIexQ(d2_s1_q.front())) {
            d2_s1_q.pop_front();
        } else {
            dec_ren_q->setStall();
            break;
        }
    }
}

bool VecPERename::RenameSrcPOperand(SimInst &inst, POperandPtr &operand)
{
    bool exception = false;
    switch (operand->type) {
        case OperandType::OPD_TLINK:
        case OperandType::OPD_ULINK: {
            auto &sgprRename = sgprRenameUnit[inst->tid];
            RecordInfo sgprInfo = sgprRename[SGPRType2Idx(operand->type)].Dispatch(operand->value);
            if (!sgprInfo.vld || sgprInfo.kill || inst->bid != sgprInfo.bid || inst->gid != sgprInfo.gid) {
                operand->ready = true;
            }
            if (!operand->reuse && inst->bid == sgprInfo.bid && inst->gid == sgprInfo.gid) {
                sgprRename[SGPRType2Idx(operand->type)].MarkNeedKill(operand->value);
            }
            operand->renamed = true;
            operand->ptag = sgprInfo.tag;
            // rename-lookup
            break;
        }
        case OperandType::OPD_VTLINK:
        case OperandType::OPD_VULINK:
        case OperandType::OPD_VMLINK:
        case OperandType::OPD_VNLINK: {
            // rename-lookup
            VRFMapEntry vrfEntry = vrfRename->Dispatch(inst->tid, inst->stid, inst, operand->type, operand->value + 1);
            operand->ptag = vrfEntry.tag;
            operand->pair = vrfEntry.pair;
            operand->renamed = true;
            operand->ready = operand->ready ? operand->ready
                : (vrfEntry.markNeedKill || vrfEntry.kill || !vrfEntry.CheckIDMatch(inst->bid, inst->gid));
            if (!operand->reuse && vrfEntry.CheckIDMatch(inst->bid, inst->gid)) {
                vrfRename->MarkNeedKill(inst->tid, inst->stid, operand->type, operand->value + 1);
            }
            break;
        }
        case OperandType::OPD_PREDMASK: {
            // rename-lookup
            RecordInfo sgprInfo = predMaskRenameUnit[inst->tid].DispatchPred(inst->bid, inst->gid, operand->ptag);
            operand->ptag = sgprInfo.tag;
            operand->renamed = true;
            operand->ready = sgprInfo.ready ? true : operand->ready;
            break;
        }
        case OperandType::OPD_RI:
            // operand->ptag = vpeTop->lgprRF->lFreeListInput.LookupPtag(inst->bid, operand->value);
            operand->ptag = vpeTop->lgprRF->lFreeListInput.DispatchPtag(inst->bid, operand->value, inst->stid);
            operand->data = vpeTop->lgprRF->lFreeListInput.GetPtagData(inst->bid, operand->value, inst->stid);
            operand->dataVld = true;
            operand->ready = true;
            operand->renamed = true;
            break;
        case OperandType::OPD_RO:
            // operand->ptag = vpeTop->lgprRF->lFreeListOutput.LookupPtag(inst->bid, operand->value);
            operand->ptag = vpeTop->lgprRF->lFreeListOutput.DispatchPtag(inst->bid, operand->value, inst->stid);
            operand->ready = true;
            operand->renamed = true;
            break;
        case OperandType::OPD_TA_REG:
        case OperandType::OPD_TB_REG:
        case OperandType::OPD_TC_REG:
        case OperandType::OPD_TD_REG:
        case OperandType::OPD_TE_REG:
        case OperandType::OPD_TF_REG:
        case OperandType::OPD_TG_REG:
        case OperandType::OPD_TH_REG: {
            // 可以暂时当立即数处理？
            // LTAR 暂时没有实现，故直接拿blockCmd，后续删除。应该在vectorCore 或者vPE 保存这些数据
            BlockCommandPtr cmd = GetSim()->core->bctrl->blockROB.GetBlockCMDPtr(inst->bid, inst->stid);
            uint64_t idx = static_cast<uint64_t>(operand->type) - static_cast<uint64_t>(OperandType::OPD_TA_REG);
            ASSERT(idx < cmd->srcs.size());
            operand->data = cmd->srcs[idx]->baseAddr;
            operand->dataVld = true;
            operand->ready = true;
            break;
        }
        case OperandType::OPD_TO_REG:
        case OperandType::OPD_TO1_REG:
        case OperandType::OPD_TO2_REG:
        case OperandType::OPD_TO3_REG:
        case OperandType::OPD_TO_GPR_REG: {
            BlockCommandPtr cmd = GetSim()->core->bctrl->blockROB.GetBlockCMDPtr(inst->bid, inst->stid);
            std::vector<TileOperandPtr> dsts = cmd->dsts;
            if (IsBlockTypeMCall(cmd->blockType)) {
                // TODO: stid
                vpeTop->vectorCore->m_vectorGS->GetDst(inst->bid, inst->gid, dsts);
            }
            TileOperandPtr dst = GetTOOperand(dsts, operand->type, cmd->blockType);
            ASSERT(dst != nullptr);
            operand->data = dst->baseAddr;
            operand->dataVld = true;
            operand->ready = true;
            operand->dataType = dst->tileInfo->dataType;
            break;
        }
        case OperandType::OPD_SIMM:
        case OperandType::OPD_UIMM:
            break;
        case OperandType::OPD_ZERO:
            operand->dataVld = true;
            operand->data = 0;
            break;
        case OperandType::OPD_SYS: {
            // execute 时，之前发请求读写
            break;
        }
        case OperandType::OPD_LB0:
        case OperandType::OPD_LB1:
        case OperandType::OPD_LB2:
        case OperandType::OPD_LC0:
        case OperandType::OPD_LC1:
        case OperandType::OPD_LC2: {
            SetLOOPRegPOperandVld(inst, operand);
            break;
        }
        case OperandType::OPD_GREG:
        case OperandType::OPD_TILE_TLINK:
        case OperandType::OPD_TILE_ULINK:
        case OperandType::OPD_TILE_MLINK:
        case OperandType::OPD_TILE_NLINK:
        case OperandType::OPD_TILE_ACC:
        case OperandType::OPD_TILE_STACK:
        case OperandType::OPD_TILE_DLINK:
        case OperandType::OPD_CARG:
        default:
            LOG_ERROR_M(Unit::VECTOR, Stage::NA) << "inst: " << inst->Dump()
                                                 << " exception, operand: " << operand->Dump();
            exception = true;
            vpeTop->SetBlockException(inst->bid, inst->stid, "VecPE Unexpected src operandtype");
            break;
    }
    operand->renamed = true;
    return !exception;
}

void VecPERename::SetLOOPRegPOperandVld(SimInst &inst, POperandPtr &operand)
{
    // LCx 在参与运算前根据LBx，groupID, laneNum, laneId 计算得到，计算方式封装在Block内
    BlockCommandPtr cmd = GetSim()->core->bctrl->blockROB.GetBlockCMDPtr(inst->bid, inst->stid);
    switch (operand->type) {
        case OperandType::OPD_LB0: {
            operand->dataVld = true;
            operand->data = cmd->lb0;
            break;
        }
        case OperandType::OPD_LB1: {
            operand->dataVld = true;
            operand->data = cmd->lb1;
            break;
        }
        case OperandType::OPD_LB2: {
            operand->dataVld = true;
            operand->data = cmd->lb2;
            break;
        }
        case OperandType::OPD_LC0: {
            operand->dataVld = true;
            if (inst->codeLen != EncodeLen::ENL_V) {
                // Scalar
                operand->data = cmd->GetLC0(inst->gid.val, inst->shapelpinfo.m_lanes, 0);
                break;
            }
            // Vector
            operand->vecDataVld = true;
            if (inst->biqType == BIQType::MCALL_IQ && inst->autogen) {
                for (uint64_t i = 0; i < inst->lanes; i++) {
                    uint64_t lc = i;
                    operand->vecData.Set(lc, i, static_cast<uint32_t>(operand->width));
                }
            } else {
                for (uint64_t i = 0; i < inst->lanes; i++) {
                    uint64_t lc = cmd->GetLC0(inst->gid.val, inst->shapelpinfo.m_lanes, i);
                    operand->vecData.Set(lc, i, static_cast<uint32_t>(operand->width));
                }
            }
            break;
        }
        case OperandType::OPD_LC1: {
            operand->dataVld = true;
            if (inst->codeLen == EncodeLen::ENL_V) {
                operand->vecDataVld = true;
                for (uint64_t i = 0; i < inst->lanes; i++) {
                    uint64_t lc = cmd->GetLC1(inst->gid.val, inst->shapelpinfo.m_lanes, i);
                    operand->vecData.Set(lc, i, static_cast<uint32_t>(operand->width));
                }
            } else {
                operand->data = cmd->GetLC1(inst->gid.val, inst->shapelpinfo.m_lanes, 0);
            }
            break;
        }
        case OperandType::OPD_LC2: {
            operand->dataVld = true;
            if (inst->codeLen == EncodeLen::ENL_V) {
                operand->vecDataVld = true;
                uint64_t lc = cmd->GetLC2(inst->gid.val, inst->shapelpinfo.m_lanes, 0);
                operand->vecData.Set(lc, 0, static_cast<uint32_t>(operand->width));
            } else {
                operand->data = cmd->GetLC2(inst->gid.val, inst->shapelpinfo.m_lanes, 0);
            }
            break;
        }
        default:
            ASSERT(false && "undefined loop reg");
            break;
    }
}

bool VecPERename::RenameDstPOperand(SimInst &inst, POperandPtr &operand)
{
    bool exception = false;
    switch (operand->type) {
        case OperandType::OPD_TLINK:
        case OperandType::OPD_ULINK: {
            auto &sgprRename = sgprRenameUnit[inst->tid];
            operand->renamed = true;
            operand->ptag = sgprRename[SGPRType2Idx(operand->type)].Alloc(inst->bid, inst->rid, inst->gid, 1);
            break;
        }
        case OperandType::OPD_VTLINK:
        case OperandType::OPD_VULINK:
        case OperandType::OPD_VMLINK:
        case OperandType::OPD_VNLINK: {
            // rename-lookup
            VRFMapEntry vrfInfo = VRFMapEntry();
            vrfInfo.vld = true;
            vrfInfo.pair = inst->dwDstType;
            vrfInfo.bid = inst->bid;
            vrfInfo.rid = inst->rid;
            vrfInfo.gid = inst->gid;
            vrfInfo.tid = inst->tid;
            ASSERT(inst->stid != -1U);
            vrfInfo.stid = inst->stid;
            vrfInfo.rType = operand->type;
            vrfRename->Alloc(vrfInfo, inst);
            operand->ptag = vrfInfo.tag;
            operand->pair = vrfInfo.pair;
            operand->renamed = true;
            break;
        }
        case OperandType::OPD_PREDMASK:
            // rename-lookup
            operand->renamed = true;
            operand->ptag = predMaskRenameUnit[inst->tid].Alloc(inst->bid, inst->rid, inst->gid, 1);
            break;
        case OperandType::OPD_RO:
            operand->ptag = vpeTop->lgprRF->lFreeListOutput.LookupPtag(inst->bid, operand->value, inst->stid);
            operand->ready = true;
            break;
        case OperandType::OPD_SYS:
            // execute 时，之前发请求读写
            break;
        case OperandType::OPD_TA_REG:
        case OperandType::OPD_TB_REG:
        case OperandType::OPD_TC_REG:
        case OperandType::OPD_TD_REG:
        case OperandType::OPD_TE_REG:
        case OperandType::OPD_TF_REG:
        case OperandType::OPD_TG_REG:
        case OperandType::OPD_TH_REG:
        case OperandType::OPD_TO_REG:
        case OperandType::OPD_TO1_REG:
        case OperandType::OPD_TO2_REG:
        case OperandType::OPD_TO3_REG:
        case OperandType::OPD_SIMM:
        case OperandType::OPD_UIMM:
        case OperandType::OPD_LB0:
        case OperandType::OPD_LB1:
        case OperandType::OPD_LB2:
        case OperandType::OPD_LC0:
        case OperandType::OPD_LC1:
        case OperandType::OPD_LC2:
        case OperandType::OPD_GREG:
        case OperandType::OPD_ZERO:
        case OperandType::OPD_TILE_TLINK:
        case OperandType::OPD_TILE_ULINK:
        case OperandType::OPD_TILE_MLINK:
        case OperandType::OPD_TILE_NLINK:
        case OperandType::OPD_TILE_ACC:
        case OperandType::OPD_TILE_STACK:
        case OperandType::OPD_TILE_DLINK:
        case OperandType::OPD_CARG:
        case OperandType::OPD_RI:
        default:
            exception = true;
            vpeTop->SetBlockException(inst->bid, inst->stid, "VecPE Unexpected dst operandtype");
            break;
    }
    return !exception;
}

bool VecPERename::DispatchToVIexQ(SimInst &inst)
{
    bool suaccelss = false;
    InstGroup grp = OpcodeManager::Inst().GetOpcodeGroup(inst->opcode);
    switch (grp) {
        case InstGroup::BLOCK_SPLIT:
        case InstGroup::BLOCK_OFFSET:
        case InstGroup::BLOCK_IO:
        case InstGroup::BLOCK_ATTR:
        case InstGroup::BLOCK_HINT:
        case InstGroup::BLOCK_ARGUMENT:
            if (inst->opcode == Opcode::OP_BSTOP) {
                break;
            }
            LOG_ERROR << "VPE Unsupport block's inst: " << inst->Dump();
            ASSERT(false && "VPE Unsupport block split inst");
            break;
        case InstGroup::BRANCH:
            inst->innerBranch = true;
            // fall through
        case InstGroup::SETC:
            suaccelss = InsertToVecScalarPipe(inst);
            break;
        case InstGroup::LOAD:
            suaccelss = InsertToVecAguPipe(inst);
            break;
        case InstGroup::STORE: {
            ASSERT(inst->stack_type != StackInstType::STACK_SET);
            if (inst->storeSplit) {
                suaccelss = InsertToVecAguStdPipes(inst);
            } else {
                suaccelss = InsertToVecAguPipe(inst);
            }
            break;
        }
        case InstGroup::ARITHMETIC:
        case InstGroup::ARITHMETIC_FP:
        case InstGroup::COMPARE:
        case InstGroup::COMPARE_FP:
        case InstGroup::PC:
        case InstGroup::IMMEDIATE:
        case InstGroup::MOVE:
        case InstGroup::MULTICYCLE:
        case InstGroup::BIT:
        case InstGroup::GQM:
        case InstGroup::COMPOUND:
        case InstGroup::PREFETCH:
        case InstGroup::ATOMIC:
        case InstGroup::EXECUTE_CONTROL:
        case InstGroup::EXTEND:
        case InstGroup::CACHE_MAINTAIN:
        case InstGroup::GMO:
        case InstGroup::SSR:
        case InstGroup::MAX_MIN:
        case InstGroup::CONVERT:
        case InstGroup::REDUCE:
        case InstGroup::SHUFFLE:
        case InstGroup::CT_CUSTOM: {
            if (IsScalarInst(inst->codeLen)) {
                suaccelss = InsertToVecScalarPipe(inst);
            } else {
                suaccelss = InsertToVecAluPipe(inst);
            }
            break;
        }
        case InstGroup::SPECIAL:{
            if (IsScalarInst(inst->codeLen)) {
                suaccelss = InsertToVecScalarPipe(inst);
            } else {
                suaccelss = InsertToVecAluPipe(inst);
            }
            break;
        }
        default:
            ASSERT(false);
            break;
    }

    return suaccelss;
}

bool VecPERename::InsertToVecScalarPipe(const SimInst &inst)
{
    auto &dispatchQ = vpeTop->pe_iex_vec_scalar_q;
    if (dispatchQ->getStall()) {
        return false;
    }
    dispatchQ->Write(inst);
    return true;
}

bool VecPERename::InsertToVecAguPipe(const SimInst &inst)
{
    auto &dispatchQ = vpeTop->pe_iex_vec_agu_q;
    if (dispatchQ->getStall()) {
        return false;
    }
    dispatchQ->Write(inst);
    return true;
}

bool VecPERename::InsertToVecAluPipe(const SimInst &inst)
{
    auto &dispatchQ = vpeTop->pe_iex_alu_q;
    if (dispatchQ->getStall()) {
        return false;
    }
    dispatchQ->Write(inst);
    return true;
}

bool VecPERename::InsertToVecAguStdPipes(const SimInst &inst)
{
    bool stall = vpeTop->pe_iex_vec_agu_q->getStall() || vpeTop->pe_iex_std_q->getStall();
    if (stall) {
        return false;
    }
    SimInst sta = std::make_shared<SimInstInfo>(*inst);
    SimInst std = std::make_shared<SimInstInfo>(*inst);
    // sta does not need data
    // 目前在解码阶段，已经将全部store 类指令的 srcD 放在了src0
    // FIXME: invalid srcD or erase it for sta?
    ASSERT(sta->psrcs_.size() > 0);
    sta->psrcs_.erase(sta->psrcs_.begin());

    sta->stack_type = StackInstType::NORMAL;  // only std do stack rename
    sta->type = ST_ADDR;
    std->type = ST_DATA;     // std needs addr for route
    // std->dst0.sptag_vld = (std->stack_type == StackInstType::STACK_SET);
    vpeTop->pe_iex_vec_agu_q->Write(sta);
    vpeTop->pe_iex_std_q->Write(std);
    GetSim()->core->iex[SCALAR_IEX]->stats->slots += 2;
    return true;
}

bool VecPERename::CheckStall(SimInst const& inst)
{
    uint32_t sgprTCount = 0;
    uint32_t sgprUCount = 0;
    uint32_t predMaskCount = 0;
    vector<uint8_t> vrfReqNum = {};
    for (auto &pdst : inst->pdsts_) {
        if (pdst->type == OperandType::OPD_TLINK) {
            ++sgprTCount;
        }
        if (pdst->type == OperandType::OPD_ULINK) {
            ++sgprUCount;
        }
        if (pdst->type == OperandType::OPD_PREDMASK) {
            ++predMaskCount;
        }
        if (OperandTypeIsVReg(pdst->type)) {
            const uint32_t regNumPair = 2U;
            const uint32_t regNumSingle = 1U;
            vrfReqNum.emplace_back(inst->dwDstType ? regNumPair : regNumSingle);
        }
    }
    ASSERT(sgprTCount <= 1);
    ASSERT(sgprUCount <= 1);
    auto &sgprRename = sgprRenameUnit[inst->tid];
    if (sgprTCount > 0 && sgprRename[SGPRType2Idx(OperandType::OPD_TLINK)].CheckStall(sgprTCount)) {
        return true;
    }
    if (sgprUCount > 0 && sgprRename[SGPRType2Idx(OperandType::OPD_ULINK)].CheckStall(sgprUCount)) {
        return true;
    }
    if (predMaskCount > 0 && predMaskRenameUnit[inst->tid].CheckStall(1)) {
        return true;
    }
    if (vrfReqNum.size() == 0) {
        return false;
    }
    if (vrfRename->MapQStall(inst->tid, inst->stid, vrfReqNum.size())) {
        vpeTop->stats->mapQStall[inst->tid % vpeTop->GetThread()]++;
        return true;
    }
    if (vrfRename->MultiStall(inst, vrfReqNum)) {
        vpeTop->stats->vptagStall++;
        return true;
    }
    return false;
}

void VecPERename::Flush(FlushBus flushReq)
{
    for (uint32_t tid = 0; tid < vpeTop->GetThread(); ++tid) {
        if (flushReq.baseOnThread && flushReq.req.tid != tid) {
            continue;
        }
        for (auto &sgprRename : sgprRenameUnit[tid]) {
            sgprRename.flush(flushReq);
        }
        predMaskRenameUnit[tid].flush(flushReq);
    }
    vrfRename->Flush(flushReq);
}

void VecPERename::RepLocalRetired(OperandType type, ROBID &seq, bool isDealloc, uint32_t tid)
{
    switch (type) {
        case OperandType::OPD_TLINK:
        case OperandType::OPD_ULINK:
            sgprRenameUnit[tid][SGPRType2Idx(type)].ReportRetired(seq.val, isDealloc);
            break;
        case OperandType::OPD_PREDMASK:
            predMaskRenameUnit[tid].ReportRetired(seq.val, isDealloc);
            break;
        default:
            break;
    }
}

void VecPERename::ReportLocalKill(OperandType type, uint32_t seq, uint32_t tid)
{
    switch (type) {
        case OperandType::OPD_TLINK:
        case OperandType::OPD_ULINK:
            sgprRenameUnit[tid][SGPRType2Idx(type)].ReportRetired(seq, true);
            break;
        case OperandType::OPD_PREDMASK:
            predMaskRenameUnit[tid].ReportRetired(seq, true);
            break;
        default:
            break;
    }
}
}
