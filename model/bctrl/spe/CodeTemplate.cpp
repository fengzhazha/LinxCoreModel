#include "bctrl/spe/CodeTemplate.h"

#include "bctrl/BCtrl.h"
#include "core/Core.h"

namespace JCore {


void CodeTemplate::Work()
{
    genTemplate();
}

void CodeTemplate::genTemplate()
{
    // TODO: Can be optimized: set true only when it is waiting for reg-data.
    genCntAtCyc = 0;
    while (genCntAtCyc < GetSim()->core->bctrl->configs.bctrl_bandwidth) {
        if (checkStall()) {
            top->statsInfo[peid].codeTemplateStall = true;
            break;
        }

        if (!ctBlock && pinst_top_ct_q->empty()) {
            decState.vld = false;
            break;
        }

        if (!ctBlock) {
            SimInst inst = pinst_top_ct_q->front();
            ctBlock = GetSim()->core->bctrl->blockROB.GetNextBlockCMDPtr(inst->bid, inst->stid);
            resetGen();
            decState.vld = true;
            decState.bid = ctBlock->bid;
            pinst_top_ct_q->pop_front();
            if (ctBlock->opcode == Opcode::OP_FEXIT) {
                uint64_t m = ctBlock->srcData[SRC1_IDX];
                uint64_t n = ctBlock->srcData[SRC2_IDX];
                for (uint64_t i = m; i <= n; i++) {
                    uint64_t idx = i % static_cast<uint64_t>(GPR::GPR_COUNT);
                    if (idx < static_cast<uint64_t>(GPR::GPR_A0)) {
                        continue;
                    }
                    localSetMask |= (1 << idx);
                    localRegCnt++;
                }
                localGetMask |= (1 << static_cast<uint32_t>(GPR::GPR_SP));
                localSetMask |= (1 << static_cast<uint32_t>(GPR::GPR_SP));
            }
        }

        if (ctBlock != nullptr && ctBlock->opcode == Opcode::OP_INVALID) {
            LOG_ERROR_M(Unit::BCC, Stage::NA) << ctBlock->Dump();
            ASSERT(false && "Unexpected ct block");
        }
        switch (ctBlock->opcode) {
            case Opcode::OP_FENTRY:
                if (!GenFEntryTemplate()) {
                    return;
                }
                break;
            case Opcode::OP_FEXIT:
                if (!GenFExitTemplate()) {
                    return;
                }
                break;
            case Opcode::OP_FRET_RA:
            case Opcode::OP_FRET_STK:
                if (!GenFRetTemplate()) {
                    return;
                }
                break;
            case Opcode::OP_MSET:
                GenMemSetTemplate();
                return;
            case Opcode::OP_MCOPY:
                GenMemCopyTemplate();
                return;
            default:
                cerr << GetOpcodeName(ctBlock->opcode) << endl;
                ASSERT(0 && "Unexpect template type. ");
        }
    }
}

SimSys *CodeTemplate::GetSim()
{
    return top->GetSim();
}

void CodeTemplate::Reset()
{

}

void CodeTemplate::Build(uint32_t pe)
{
    for (uint32_t i = 0; i < MEM_REG_NUM; i++) {
        curRegData.emplace_back();
        nextRegData.emplace_back();
        reg.emplace_back();
    }

    peid = pe;
}

void CodeTemplate::Xfer()
{
    curStage = nextStage;
    curMemcopyStage = nextMemcopyStage;
    curRegData = nextRegData;

    extendInstQ.Work();
    alignInstQ.Work();
    endingInstQ.Work();
}

void CodeTemplate::resetGen()
{
    curStage = GenMemStage::START;
    nextStage = GenMemStage::START;
    curMemcopyStage = GenMemCopyStage::GEN_ALIGN;
    nextMemcopyStage = GenMemCopyStage::GEN_ALIGN;
    genInstCnt = 0;
    decState.lsID = ROBID();
    decState.load_id = 0;
    decState.sid = 0;

    addrOffset = 0;
    srcOffset = 0;
    dstOffset = 0;
    ct_stCnt = 0;
    ct_ldCnt = 0;
    srcTotalOffset = 0;
    ct_vld = false;
    followStatus = MemCopyStatus::FOLLOW_LD;
    isGenDczvaInst = false;

    extendInstQ.Reset();
    alignInstQ.Reset();
    endingInstQ.Reset();

    for (uint32_t num = 0; num < MEM_REG_NUM; num++) {
        reg[num] = 0;
    }
    localSetMask = 0;
    localGetMask = 0;
    localRegCnt = 0;
}

void CodeTemplate::flush(FlushBus &flushReq)
{
    if (!flushReq.req.vld) {
        return;
    }

    auto flushHdrMatch = [&flushReq](std::deque<SimInst> *dc_q) {
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

    flushHdrMatch(pinst_top_ct_q);
    flushInstMatch(pinst_ct_top_q);

    if (ctBlock && (!LessEqual(flushReq.req.bid, ctBlock->bid) || ctBlock->stid != flushReq.req.stid)) {
        return;
    }

    resetGen();
    for (uint32_t num = 0; num < MEM_REG_NUM; num++) {
        reg[num] = 0;
    }
    auto match = [&flushReq](SimInst &inst) -> bool {
        if (inst->stid != flushReq.req.stid) {
            return false;
        }
        return (flushReq.baseOnBid ? LessEqual(flushReq.req.bid, inst->bid):
            LessEqual(flushReq.req.bid, flushReq.req.rid, inst->bid, inst->rid));
    };
    rf_ct_q->FlushIf(match);
    ctBlock = std::shared_ptr<BlockCommand>(nullptr);
}

bool CodeTemplate::checkStall(uint32_t inputCnt)
{
    return pinst_ct_top_q->size() + inputCnt >= MAX_BF_SIZE;
}

} // namespace JCore
