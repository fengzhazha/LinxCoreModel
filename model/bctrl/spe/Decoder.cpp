#include "bctrl/spe/Decoder.h"

#include "bctrl/BCtrl.h"
#include "core/Core.h"
#include "DFX/InstTracer.h"

namespace JCore {


void Decoder::Work()
{
    if (checkStall()) {
        top->statsInfo[peid].decoderStall = true;
        return;
    }

    std::stringstream oss;
    for (uint32_t pos = 0; pos < GetSim()->core->bctrl->configs.bctrl_bandwidth; ++pos) {
        if (pinst_top_dec_q->empty()) {
            for (auto& dec : decState) {
                dec.vld = false;
            }
            break;
        }

        SimInst inst = pinst_top_dec_q->front();
        if (OpcodeInInstGroup(inst->opcode, InstGroup::BLOCK_SPLIT) && inst->opcode != Opcode::OP_BSTOP) {
            PushBlockSplitInst(inst, oss);
            uint32_t stid = inst->stid;
            decState[stid].sid = 0;
            decState[stid].load_id = 0;
            decState[stid].lsID = ROBID();
        }
        DecodeInst(inst, oss);
        inst->sid = inst->startSID;
        inst->load_id = inst->startLoadID;
        pinst_top_dec_q->pop_front();
    }
    if (!oss.str().empty()) {
        LOG_INFO_M(Unit::BCC, Stage::D1) << oss.str();
    }
}

bool Decoder::PushBlockSplitInst(SimInst &inst, std::stringstream &oss)
{
    oss << "block split ";
    // In order queue
    pblock_split_dc_top_q[inst->stid]->emplace_back(inst);
    inst->pipeCycle->decodeCycle = GetSim()->getCycles();

    return true;
}

bool Decoder::DecodeInst(SimInst &simInst, std::stringstream &oss)
{
    simInst->ConvertPOperand();
    auto& decS = decState[simInst->stid];
    simInst->iexType = ExecEngineTyp::SCALAR_IEX;
    simInst->lsID = decS.lsID;
    simInst->sid = simInst->startSID + decS.sid;
    simInst->load_id = simInst->startLoadID + decS.load_id;

    decS.vld = true;
    decS.bid = simInst->bid;

    if (simInst->opcode == Opcode::OP_INVALID) {
        // For unrecognized opcode, report BROB exception
        GetSim()->core->bctrl->blockROB.reportException(simInst->bid, simInst->stid, "unrecognized instruction");
        LOG_ERROR_M(Unit::BCC, Stage::D1) << " Report block exception at unrecognized instruction: "
            << "BPC " << hex << simInst->bpc << " TPC 0x" << simInst->pc << " bin 0x" << simInst->binary << " BID "
            << dec << simInst->bid;
        return false;
    }

    if (simInst->opcode == Opcode::OP_SSRSET) {
        SimInst dinst = nullptr;
        top->GetSim()->core->iex[SCALAR_IEX]->ssrsetOrderQ.Write(dinst);
    }
    oss << " " << simInst->Dump() << "|";

    if (OpcodeManager::Inst().GetOpcodeGroup(simInst->opcode) == InstGroup::CACHE_MAINTAIN) {
        simInst->storeSplit = false;
    }

    pinst_dc_top_q->emplace_back(simInst);
    top->statsInfo[simInst->peID].decodeInstCnt++;

    if (FitMovrOpt(simInst)) {
        // MoveOffset info = MoveOffset();
        // info.srcAtag = dinst->src0.atag;
        // info.imm = dinst->imm.vld ? dinst->imm.data : 0;
        // if (dinst->opcode == Opcode::OP_SUBI) info.imm = -dinst->imm.data;
        // top->top->blockRenameUnit.setMoveBpcMap(dinst->bpc, dinst->dst0.atag, info);
    }

    if (GetSim()->GetViewManager(simInst->stid)->config.printPipeView || GetSim()->core->pTracer->IsEnabled()) {
        simInst->pipeCycle->f0Cycle = simInst->bfuInfo->create_time;
        if (simInst->bfuInfo->create_time == 0 && simInst->bfuInfo->f1_time > 0) {
            simInst->pipeCycle->f0Cycle = simInst->bfuInfo->f1_time - 1;
        }
        simInst->pipeCycle->f1Cycle = simInst->bfuInfo->f1_time;
        if (simInst->bfuInfo->f1_time == 0 && simInst->bfuInfo->create_time > 0) {
            simInst->pipeCycle->f1Cycle = simInst->bfuInfo->create_time + 1;
        }
        simInst->pipeCycle->f2Cycle = simInst->bfuInfo->bhc_fetch_time;
        simInst->pipeCycle->f3Cycle = simInst->bfuInfo->ib_time;
        simInst->pipeCycle->decodeCycle = GetSim()->getCycles();
        if (GetSim()->core->configs.bp_mode != 0) {
            const uint32_t perfectbpOffsetF0 = 4;
            const uint32_t perfectbpOffsetF1 = 3;
            const uint32_t perfectbpOffsetF2 = 2;
            const uint32_t perfectbpOffsetF3 = 1;
            simInst->pipeCycle->f0Cycle = simInst->bfuInfo->create_time =
                simInst->pipeCycle->decodeCycle - perfectbpOffsetF0;
            simInst->pipeCycle->f1Cycle = simInst->bfuInfo->f1_time =
                simInst->pipeCycle->decodeCycle - perfectbpOffsetF1;
            simInst->pipeCycle->f2Cycle = simInst->bfuInfo->bhc_fetch_time =
                simInst->pipeCycle->decodeCycle - perfectbpOffsetF2;
            simInst->pipeCycle->f3Cycle = simInst->bfuInfo->ib_time =
                simInst->pipeCycle->decodeCycle - perfectbpOffsetF3;
        }
    }
    auto header = GetSim()->core->bctrl->blockROB.getBlockHeader(simInst->bid, simInst->stid);

    // assign load/store id
    // DC.ZVA 也需要走 STA Pipe 进 LSU，所以这里也分配 lsid
    if (OpcodeIsLoad(simInst->opcode) || OpcodeIsStore(simInst->opcode) || OpcodeIsDCZVA(simInst->opcode)) {
        IncROBID(decS.lsID, LSU_ID_RANGE);
        if (OpcodeIsStore(simInst->opcode) || OpcodeIsDCZVA(simInst->opcode)) {
            decS.sid++;
            /* load/store Count 会用来更新 BROB non-flush ptr，需要保留 */
            header->storeCount++;
        } else {
            decS.load_id++;
            header->loadCount++;
        }
    }

    return true;
}

SimSys *Decoder::GetSim()
{
    return sim;
}

void Decoder::Reset()
{
    decState.resize(GetSim()->core->configs.scalar_smt_thread);
    for (auto &dec : decState) {
        dec.lsID = ROBID();
        dec.load_id = 0;
        dec.sid = 0;
    }

    pinst_top_dec_q->clear();
    for (auto &headerQ : pblock_split_dc_top_q) {
        headerQ->clear();
    }
    pinst_dc_top_q->clear();
}

void Decoder::Xfer()
{

}

void Decoder::Build(uint32_t pe)
{
    peid = pe;
    Reset();
}

void Decoder::flush(FlushBus &flushReq)
{
    auto flushHdrMatch = [&flushReq, this](std::deque<SimInst> *dc_q) {
        for (auto it = dc_q->begin(); it != dc_q->end(); ) {
            if (LessEqual(flushReq.req.bid, (*it)->bid) && flushReq.req.stid == (*it)->stid) {
                it = dc_q->erase(it);
            } else {
                it++;
            }
        }
    };

    auto flushInstMatch = [&flushReq, this](std::deque<SimInst> *dc_q) {
        for (auto it = dc_q->begin(); it != dc_q->end(); ) {
            if (LessEqual(flushReq.req.bid, (*it)->bid) && flushReq.req.stid == (*it)->stid) {
                it = dc_q->erase(it);
            } else {
                it++;
            }
        }
    };

    flushHdrMatch(pinst_top_dec_q);
    for (auto &headerQ : pblock_split_dc_top_q) {
        flushHdrMatch(headerQ);
    }
    flushInstMatch(pinst_dc_top_q);
    auto &decS = decState[flushReq.req.stid];
    if (decS.vld && LessEqual(flushReq.req.bid, decS.bid)) {
        decS.sid = 0;
        decS.load_id = 0;
        decS.lsID = ROBID();
    }

    if (flushReq.req.fetchTPCVld && flushReq.req.peID == peid) {
        BHeader header = GetSim()->core->bctrl->blockROB.getBlockHeader(flushReq.req.bid, flushReq.req.stid);
        if (flushReq.baseOnBid) {
            decS.sid = 0;
            decS.load_id = 0;
            decS.lsID = ROBID();
            return;
        }
        const uint32_t rid = flushReq.req.rid.val;
        SimInst inst = top->top->prob[flushReq.req.stid]->getNextEntry(rid).inst;

        decS.lsID = flushReq.req.lsID;
        decS.sid = header->storeCount;
        decS.load_id = header->loadCount;
    }
}

bool Decoder::checkStall(uint32_t inputCnt)
{
    return pinst_dc_top_q->size() + inputCnt >= MAX_BF_SIZE;
}

bool Decoder::FitMovrOpt(SimInst &inst)
{
    if (inst->dsts.empty() || inst->srcs.empty()) {
        return false;
    }
    if (inst->dsts[DST0_IDX]->type != OperandType::OPD_GREG || inst->srcs[SRC0_IDX]->type != OperandType::OPD_GREG) {
        return false;
    }

    if (OpcodeIsMovr(inst->opcode)) {
        return true;
    }
    if (OpcodeCanBeMov0(inst->opcode) && (inst->srcs.size() > SRC1_IDX && OperandTypeIsImm(inst->srcs[SRC1_IDX]->type)
        && inst->srcs[SRC1_IDX]->data == 0)) {
        return true;
    }
    return false;
}

} // namespace JCore
