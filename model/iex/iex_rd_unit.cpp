#include "iex/iex_rd_unit.h"

#include "core/Core.h"
#include "iex/iex.h"

namespace JCore {

// TODO: emulator 接口转移到isa/calculate，需要适配，另外，计算和写寄存器需要分开。
// 待新方案确认后，重新实现
void RdUnit::setReg(Opcode opcode, uint32_t atag, uint64_t data, OperandWidth width, OPConvertType cvt)
{
    if (!rdReg[atag].IsValid()) {
        rdReg[atag].Set(data);
        return;
    }
    // bool sign = IsSignedConvertType(cvt);
    // switch (opcode) {
    //     case Opcode::OP_RDADD:
    //         if (sign) {
    //             rdReg[atag].Set(Rdadds(OPDW_D, data, rdReg[atag].Get()));
    //         } else {
    //             rdReg[atag].Set(Rdaddu(OPDW_D, data, rdReg[atag].Get()));
    //         }
    //         break;
    //     case Opcode::OP_RDAND:
    //         rdReg[atag].Set(Rdand(data, rdReg[atag].Get()));
    //         break;
    //     case Opcode::OP_RDOR:
    //         rdReg[atag].Set(Rdor(data, rdReg[atag].Get()));
    //         break;
    //     case Opcode::OP_RDXOR:
    //         rdReg[atag].Set(Rdxor(data, rdReg[atag].Get()));
    //         break;
    //     case Opcode::OP_RDFADD:
    //         rdReg[atag].Set(Rdfadd(cvt, data, rdReg[atag].Get(), float_round_nearest_even));
    //         break;
    //     case Opcode::OP_RDFMAX:
    //         rdReg[atag].Set(Rdfmax(cvt, data, rdReg[atag].Get(), float_round_nearest_even));
    //         break;
    //     case Opcode::OP_RDMAX:
    //         if (sign) {
    //             rdReg[atag].Set(Rdmax(OPCVT_S64, data, rdReg[atag].Get()));
    //         } else {
    //             rdReg[atag].Set(Rdmax(OPCVT_U64, data, rdReg[atag].Get()));
    //         }
    //         break;
    //     case Opcode::OP_RDFMIN:
    //         rdReg[atag].Set(Rdfmin(cvt, data, rdReg[atag].Get(), float_round_nearest_even));
    //         break;
    //     case Opcode::OP_RDMIN:
    //         if (sign) {
    //             rdReg[atag].Set(Rdmin(OPCVT_S64, data, rdReg[atag].Get()));
    //         } else {
    //             rdReg[atag].Set(Rdmin(OPCVT_U64, data, rdReg[atag].Get()));
    //         }
    //         break;
    //     default:
    //         ASSERT(false && "nut support");
    // }
}

void RdUnit::setReg(Opcode opcode, ReduceRegInfo& reg, uint64_t data, OperandWidth width, OPConvertType cvt)
{
    if (!reg.IsValid()) {
        reg.Set(data);
        return;
    }
    // bool sign = isSignedConvertType(cvt);
    // switch (opcode) {
    //     case Opcode::OP_RDADD:
    //         if (sign) {
    //             reg.Set(Rdadds(OPDW_D, data, reg.Get()));
    //         } else {
    //             reg.Set(Rdaddu(OPDW_D, data, reg.Get()));
    //         }
    //         break;
    //     case Opcode::OP_RDAND:
    //         reg.Set(Rdand(data, reg.Get()));
    //         break;
    //     case Opcode::OP_RDOR:
    //         reg.Set(Rdor(data, reg.Get()));
    //         break;
    //     case Opcode::OP_RDXOR:
    //         reg.Set(Rdxor(data, reg.Get()));
    //         break;
    //     case Opcode::OP_RDFADD:
    //         reg.Set(Rdfadd(cvt, data, reg.Get(), float_round_nearest_even));
    //         break;
    //     case Opcode::OP_RDFMAX:
    //         reg.Set(Rdfmax(cvt, data, reg.Get(), float_round_nearest_even));
    //         break;
    //     case Opcode::OP_RDMAX:
    //         if (sign) {
    //             reg.Set(Rdmax(OPCVT_S64, data, reg.Get()));
    //         } else {
    //             reg.Set(Rdmax(OPCVT_U64, data, reg.Get()));
    //         }
    //         break;
    //     case Opcode::OP_RDFMIN:
    //         reg.Set(Rdfmin(cvt, data, reg.Get(), float_round_nearest_even));
    //         break;
    //     case Opcode::OP_RDMIN:
    //         if (sign) {
    //             reg.Set(Rdmin(OPCVT_S64, data, reg.Get()));
    //         } else {
    //             reg.Set(Rdmin(OPCVT_U64, data, reg.Get()));
    //         }
    //         break;
    //     default:
    //         ASSERT(false && "not support");
    // }
}

void RdUnit::Reduce(SimInst &inst)
{
    auto reduce = [this](POperandPtr &dst, SimInst &inst) {
        // reduced to global gpr
        if (dst->value < static_cast<uint32_t>(GPR::GPR_COUNT) && dst->type == OperandType::OPD_GREG) {
            ASSERT(dst->dataVld);
            for (uint32_t i = 0; i < dst->vecData.data.size(); ++i) {
                setReg(inst->opcode, dst->value, dst->vecData.data[i], inst->srcs[SRC0_IDX]->width, inst->srcs[SRC0_IDX]->cvt);
            }
        } else if (dst->type == OperandType::OPD_TLINK) {
            // reduced to t reg
            for (uint32_t i = 0; i < dst->vecData.data.size(); ++i) {
                setReg(inst->opcode, tReg, dst->vecData.data[i], inst->srcs[SRC0_IDX]->width, inst->srcs[SRC0_IDX]->cvt);
            }
        } else if (dst->type == OperandType::OPD_ULINK) {
            // reduced to u reg
            for (uint32_t i = 0; i < dst->vecData.data.size(); ++i) {
                setReg(inst->opcode, uReg, dst->vecData.data[i], inst->srcs[SRC0_IDX]->width, inst->srcs[SRC0_IDX]->cvt);
            }
        }
    };

    for (auto dst : inst->pdsts_) {
        reduce(dst, inst);
    }
}

static bool CheckEmpty(std::vector<SimQueue<SimInst>*> &pe_iex_rd_array)
{
    for (auto &rdQ : pe_iex_rd_array) {
        if (rdQ != nullptr && !rdQ->Empty()) {
            return false;
        }
    }

    return true;
}

void RdUnit::setRegFileWrite(POperandPtr& dst, const POperandPtr &instDst)
{
    dst->vecDataVld = false;
    if (instDst->value < static_cast<uint32_t>(GPR::GPR_COUNT) && instDst->type == OperandType::OPD_GREG) {
        dst->data = rdReg[instDst->value].Get();
        rdReg[instDst->value].Reset();
    } else if (instDst->type == OperandType::OPD_TLINK) {
        dst->data = tReg.Get();
        tReg.Reset();
    } else if (instDst->type == OperandType::OPD_ULINK) {
        dst->data = uReg.Get();
        uReg.Reset();
    }
}

void RdUnit::Work()
{
    rf_wr_req.Reset();
    if (CheckEmpty(pe_iex_rd_array)) {
        return;
    }

    /* find correct oldest bid insts */
    vector<SimInst> orderedRdUinstVector;
    static unordered_map<uint64_t, uint32_t> lastTpcRdBid;
    /* only allow one lastHeader at one Cycle */
    bool hasLastHeader = false;
    SimInst lastInstOldest = std::make_shared<SimInstInfo>();
    // for (uint64_t i = 0; i < pe_iex_rd_array.size() * top->core->peArray[0]->configs.pe_retire_width; i++) {
    for (uint64_t i = 0; i < pe_iex_rd_array.size(); i++) {
        uint32_t selectIndex = -1U;
        for (uint64_t j = 0; j < pe_iex_rd_array.size(); j++) {
            if (pe_iex_rd_array[j]->Empty()) {
                continue;
            }
            if (selectIndex == -1U ||
                LessROBID(pe_iex_rd_array[j]->Front()->bid, pe_iex_rd_array[selectIndex]->Front()->bid)) {
                selectIndex = j;
            }
        }
        if (selectIndex == -1U) {
            continue;
        }
        SimInst instOldest = pe_iex_rd_array[selectIndex]->Front();
        if (!lastInstOldest && instOldest->bid == lastInstOldest->bid && instOldest->pc == lastInstOldest->pc) {
            return;
        }

        if (hasLastHeader && instOldest->isLastHeader) {
            break;
        }

        auto it = lastTpcRdBid.find(instOldest->pc);
        if ((it == lastTpcRdBid.end() &&
            instOldest->bid.val == top->core->bctrl->blockROB.getOldestBlockID(instOldest->stid).val)
            || (it != lastTpcRdBid.end() &&
            (it->second + 1) % top->core->configs.block_rob_depth ==
            instOldest->bid.val % top->core->configs.block_rob_depth)) {
            orderedRdUinstVector.push_back(pe_iex_rd_array[selectIndex]->Read());
            lastTpcRdBid[instOldest->pc]  = instOldest->bid.val;
            if (instOldest->isLastHeader) {
                hasLastHeader = true;
            }
        }
        lastInstOldest = instOldest;
    }

    for (auto &inst: orderedRdUinstVector) {
        Reduce(inst);
        if (!inst->isLastHeader) {
            continue;
        }
        lastTpcRdBid.erase(inst->pc);
        PLpvInfo lpvInfo;
        for (auto &pdst : inst->pdsts_) {
            top->iq.WakeupIQTag(WakeupInfo(pdst->type, pdst->ptag, pdst->recycled),
                lpvInfo, inst->peID, inst->tid, inst->stid);
        }

        rf_wr_req = inst->GenRFReqBus(false);

        ASSERT(rf_wr_req.dsts.size() == inst->pdsts_.size());
        for (size_t i = 0; i < inst->pdsts_.size(); ++i) {
            setRegFileWrite(rf_wr_req.dsts[i], inst->pdsts_[i]);
        }

        // ready table wtire
        // if (inst->checkAlldstRanged() && top->core->configs.reno_dynamic_enable) {
        //     RFReqBus req = inst->genWRTableBus();
        //     if (req.vld) {
        //         req.dst0.dataOut = req.dst0.ptag_vld ? rf_wr_req.dst0.dataOut : req.dst0.dataOut;
        //         req.dst1.dataOut = req.dst1.ptag_vld ? rf_wr_req.dst1.dataOut : req.dst1.dataOut;
        //         top->iex_rt_wr_q->Write(req);
        //     }
        // }
    }
    return;
}

void RdUnit::Build()
{
    rdReg.resize(static_cast<uint64_t>(GPR::GPR_COUNT));
    pe_iex_rd_array.resize(GetSim()->core->GetPECount(), nullptr);
}

void RdUnit::Reset()
{
    for (auto &it : rdReg) {
        it.Reset();
    }
    tReg.Reset();
    uReg.Reset();
}

SimSys *RdUnit::GetSim()
{
    return top->GetSim();
}

} // namespace JCore
