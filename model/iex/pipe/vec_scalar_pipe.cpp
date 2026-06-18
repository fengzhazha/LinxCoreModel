#include "core/Core.h"
#include "iex/pipe/iex_pipe.h"
#include "iex/iex_latency.h"

#include <algorithm>

namespace JCore {

using namespace std;

constexpr uint32_t IEX_ALU_DEFAULT_EX_STAGE_NUM = 1;
constexpr uint32_t IEX_ALU_VEC_EX_STAGE_NUM = 5; // latency=6: E1-E5

void VecScalarPipe::Build(uint32_t id)
{
    pipeid = id;

    if (top->core->IsVectorIex(top->machineType)) {
        ex_stage_num = IEX_ALU_VEC_EX_STAGE_NUM;
    } else {
        ex_stage_num = IEX_ALU_DEFAULT_EX_STAGE_NUM;
    }
    i1_inst = nullptr;
    ex_inst.assign(ex_stage_num, vector<SimInst>(ex_stage_num));
    w1_inst.assign(ex_stage_num, nullptr);
    w2_inst.assign(ex_stage_num, nullptr);
    rf_wr_req.resize(ex_stage_num);
    branchMaskCount.assign(4, 1);
}

void VecScalarPipe::Reset()
{
    i1_inst = nullptr;
    ex_inst.assign(ex_stage_num, vector<SimInst>(ex_stage_num));
    w1_inst.assign(ex_stage_num, nullptr);
    w2_inst.assign(ex_stage_num, nullptr);
    sys_rd_req.Reset();
    sys_wr_req.Reset();
}

void VecScalarPipe::Work()
{
    runW2();
    runW1();
    for (int32_t stage = static_cast<int32_t>(Stage::E1) + ex_stage_num - 1; stage >= static_cast<int32_t>(Stage::E1);
         stage--) {
        runEx(static_cast<Stage>(stage));
    }
    runI1();
    runP1();
    w1_flag.erase(sim->getCycles());
    LogPrint();
}

void VecScalarPipe::moveLpv()
{
    auto pipeMoveLpv = [](SimInst &inst) {
        if (inst) {
            inst->MoveLpv();
        }
    };

    for (auto& inst : w2_inst) {
        pipeMoveLpv(inst);
    }
    for (auto& inst : w1_inst) {
        pipeMoveLpv(inst);
    }
    for (int32_t stg_i = ex_stage_num - 1; stg_i >= 0; --stg_i) {
        for (auto& inst : ex_inst.at(stg_i)) {
            pipeMoveLpv(inst);
        }
    }
    pipeMoveLpv(i1_inst);
}

void VecScalarPipe::move()
{
    // writeback stage
    w2_inst = w1_inst;
    w1_inst.assign(ex_stage_num, nullptr);

    auto move_to_w1 = [this](const SimInst& inst) {
        // find w1_inst slot
        for (uint32_t w1_idx = 0; w1_idx < ex_stage_num; ++w1_idx) {
            if (w1_inst.at(w1_idx) == nullptr) {
                w1_inst.at(w1_idx) = inst;
                break;
            }
        }
    };

    // check any instr in execution stage ready for write back
    for (int32_t stg_i = ex_stage_num - 1; stg_i >= 0; --stg_i) {
        for (uint32_t idx = 0; idx < ex_inst[stg_i].size(); ++idx) {
            SimInst inst = ex_inst[stg_i][idx];
            if (!inst) {
                continue;
            }
            if ((inst->iexLatency - 1) != static_cast<uint32_t>(stg_i + 1)) {
                continue;
            }
            ex_inst[stg_i][idx] = nullptr;
            move_to_w1(inst);
        }
    }
    for (auto& inst : ex_inst[ex_stage_num - 1]) {
        if (inst) {
            LOG_ERROR_M(top->machineType, Stage::NA) << "Last EX slot should be empty: " << inst;
            fflush(stdout);
            ASSERT(false);
        }
    }

    // execution stage
    for (int32_t stg_i = ex_stage_num - 1; stg_i > 0; --stg_i) {
        ex_inst[stg_i].swap(ex_inst[stg_i - 1]);
    }
    ex_inst[0].assign(ex_inst[0].size(), nullptr);

    // new instructions into execution
    ex_inst[0][0] = i1_inst;

    // fetch
    i1_inst = *p1_inst;
    *p1_inst = nullptr;
}

void VecScalarPipe::flush(FlushBus &flushReq)
{
    auto flush_stage = [&flushReq] (SimInst &inst) {
        if (!inst) {
            return;
        }
        if (inst->stid != flushReq.req.stid) {
            return;
        }
        if (flushReq.baseOnPE && flushReq.req.peID != inst->peID) {
            return;
        }
        bool lessEq = flushReq.baseOnBid ? LessEqual(flushReq.req.bid, inst->bid):
            LessEqual(flushReq.req.bid, flushReq.req.rid, inst->bid, inst->rid);
        if (lessEq) {
            inst = std::shared_ptr<SimInstInfo>(nullptr);
        }
    };
    flush_stage(i1_inst);
    for (uint32_t stg_i = 0; stg_i < ex_inst.size(); ++stg_i) {
        for (auto& inst : ex_inst[stg_i]) {
            flush_stage(inst);
        }
    }
    for (auto& inst : w1_inst) {
        flush_stage(inst);
    }
    for (auto& inst : w2_inst) {
        flush_stage(inst);
    }
}

bool VecScalarPipe::isPipeStallByScb(const SimInst& inst, uint32_t& noBusCfltCycles) const
{
    if (!inst) {
        return false;
    }
    if (inst->pdsts_.size() == 0) {
        return false;
    }
    constexpr uint64_t W1_LATENCY_OFFSET = 1; // I1
    const uint64_t w1Time = sim->getCycles() + W1_LATENCY_OFFSET + getExLatency(inst);
    if (noBusCfltCycles <= getExLatency(inst)) {
        noBusCfltCycles = 0;
        return (w1_flag.find(w1Time) != w1_flag.cend());
    }
    const uint32_t maxOffset = noBusCfltCycles - getExLatency(inst);
    for (uint32_t offset = 0; offset <= maxOffset; offset++) {
        if (w1_flag.find(w1Time + offset) == w1_flag.cend()) {
            noBusCfltCycles = offset;
            return false;
        }
    }
    return true;
}

void VecScalarPipe::runP1()
{
    if ((p1_inst == nullptr) || (*p1_inst == nullptr)) {
        return;
    }

    rf_rd_req.Reset();
    sys_rd_req.Reset();
    auto& inst = *p1_inst;

    auto generateRequests = [this, &inst]() {
        rf_rd_req = inst->GenRFReqBus(true);
        rf_rd_req.pipeid = pipeid;
        sys_rd_req = inst->GenSYSReqBus();
    };
    generateRequests();

    inst->pipeCycle->p1Cycle = sim->getCycles();
    inst->iexLatency = getExLatency(inst);

    auto getUpdateW1Cycle = [this](SimInst& inst, uint64_t oldW1Time) {
        uint64_t w1Time = oldW1Time;
        const uint32_t maxOffset = top->configs.simt_iex_max_ex_no_rslv_cflt - inst->iexLatency;
        for (uint32_t offset = 0; offset <= maxOffset; offset++) {
            if (w1_flag.find(w1Time + offset) == w1_flag.cend()) {
                w1Time += offset;
                inst->iexLatency += offset;
                inst->iexExtendedCycles = offset;
                break;
            }
        }
        return w1Time;
    };
    if (inst->pdsts_.size() > 0) {
        // add additional ex cycles for rslv bus cflt and record w1 time
        constexpr uint64_t W1_LATENCY_OFFSET = 1; // I1
        uint64_t w1Time = sim->getCycles() + W1_LATENCY_OFFSET + inst->iexLatency;
        if (((top->core->IsVectorIex(top->machineType)) || (top->id == MEM_IEX)) && top->configs.simt_iex_sca_rslv_cflt
            && (top->configs.simt_iex_max_ex_no_rslv_cflt > inst->iexLatency)) {
            ASSERT(top->configs.simt_iex_max_ex_no_rslv_cflt <= (ex_stage_num + 1));
            w1Time = getUpdateW1Cycle(inst, w1Time);
        }
        w1_flag.insert(w1Time);
    }

    const uint32_t latency = inst->iexLatency;
    constexpr uint32_t P1_WKUP_LATENCY = 1;

    if (latency == P1_WKUP_LATENCY) {
        PLpvInfo lpvInfo = inst->GetLpv();
        for (auto pdst : inst->pdsts_) {
            top->iq.WakeupIQTag(WakeupInfo(pdst->type, pdst->ptag, pdst->recycled),
                lpvInfo, inst->peID, inst->tid, inst->stid);
        }
    }
}

void VecScalarPipe::runI1()
{
    // do bypass from all pipes
    if (!i1_inst) {
        return;
    }
    i1_inst->pipeCycle->i1Cycle = sim->getCycles();
    // E1/E2/E3/E4/E5 to I2
    // W1 to I2
    // W2 to I2
    if (!rf_data_ret.vld) {
        return;
    }

    i1_inst->RFRetSetData(rf_data_ret);

    // wakeup
    const uint32_t latency = i1_inst->iexLatency;
    constexpr uint32_t I2_WKUP_LATENCY = 2;
    if (latency == I2_WKUP_LATENCY) {
        PLpvInfo lpvInfo = i1_inst->GetLpv();
        for (auto pdst : i1_inst->pdsts_) {
            top->iq.WakeupIQTag(WakeupInfo(pdst->type, pdst->ptag, pdst->recycled),
                lpvInfo, i1_inst->peID, i1_inst->tid, i1_inst->stid);
        }
    }

    if (!sys_data_ret.vld) {
        return;
    }
    i1_inst->RecSYSReqRet(sys_data_ret);
}

void VecScalarPipe::runEx(Stage stage)
{
    uint32_t idx = static_cast<uint32_t>(stage) - static_cast<uint32_t>(Stage::E1);
    for (auto& inst : ex_inst.at(idx)) {
        if (!inst) {
            continue;
        }
        switch (stage) {
            case Stage::E1:
                ++top->stats->sALUCycles;
                top->stats->sAluExtendCycles[inst->iexExtendedCycles] += 1;
                inst->pipeCycle->e1Cycle = sim->getCycles();
                // if (sim->perfectGetSet) {
                //     if (inst->ref.gsInfo.src0_vld) {
                //         inst->src0.data = inst->ref.gsInfo.src0_data;
                //     }
                //     if (inst->ref.gsInfo.src1_vld) {
                //         inst->src1.data = inst->ref.gsInfo.src1_data;
                //     }
                //     if (inst->ref.gsInfo.src2_vld) {
                //         inst->src2.data = inst->ref.gsInfo.src2_data;
                //     }
                //     if (inst->ref.gsInfo.src3_vld) {
                //         inst->src3.data = inst->ref.gsInfo.src3_data;
                //     }
                // }
                break;
            case Stage::E2:
                if ((inst->iexLatency - 1) >= idx + 1) {
                    inst->pipeCycle->e2Cycle = sim->getCycles();
                }
                break;
            case Stage::E3:
                if ((inst->iexLatency - 1) >= idx + 1) {
                    inst->pipeCycle->e3Cycle = sim->getCycles();
                }
                break;
            case Stage::E4:
                if ((inst->iexLatency - 1) >= idx + 1) {
                    inst->pipeCycle->e4Cycle = sim->getCycles();
                }
                break;
            case Stage::E5:
                if ((inst->iexLatency - 1) >= idx + 1) {
                    inst->pipeCycle->e5Cycle = sim->getCycles();
                }
                break;
            default:
                break;
        }

        // wakeup
        const uint32_t latency = inst->iexLatency;
        constexpr uint32_t EX_WKUP_LATENCY_OFST = 3;
        if (latency == (idx + EX_WKUP_LATENCY_OFST)) {
            PLpvInfo lpvInfo = inst->GetLpv();
            for (auto pdst : inst->pdsts_) {
                top->iq.WakeupIQTag(WakeupInfo(pdst->type, pdst->ptag, pdst->recycled),
                    lpvInfo, inst->peID, inst->tid, inst->stid);
            }
        }

        if ((inst->iexLatency - 1) == (idx + 1)) {
            executeInstr(inst, stage);
            if (OpcodeIsInnerJump(inst->opcode)) {
                ASSERT(inst->iexLatency == 2);
            }
        }
    }
}

void VecScalarPipe::executeInstr(const SimInst& inst, Stage stage)
{
    sys_wr_req.Reset();
    InstGroup grp = OpcodeManager::Inst().GetOpcodeGroup(inst->opcode);
    if (grp == InstGroup::BRANCH || grp == InstGroup::SETC) {
        if (top->configs.predMaskFake && inst->pc == top->configs.predMaskFakePC) { // 256:0x115c4/ 2560:0x11748
            unsigned index = inst->gid.val % 4;
            if (branchMaskCount[index] >= 512) {
                inst->srcs[0]->data = 0;
                branchMaskCount[index] = 1;
            } else {
                branchMaskCount[index] += 1;
            }
        }
        ExecJump(inst);
        return;
    }
    auto handleVecExec = [this](uint32_t pipeid, uint32_t lane, const SimInst& inst) {
        bool ret = inst->Execute(lane);
        if (!ret) {
            LOG_INFO_M(top->machineType, Stage::E1) << " exception inst " << inst;
            top->core->bctrl->blockROB.reportException(inst->bid, inst->stid, "ALU excution exception(SIMT)");
            return false;
        }
        return true;
    };
    // simt or scalor
    if (IsScalarInst(inst->codeLen)) {
        bool ret = inst->Execute();
        if (!ret) {
            top->core->bctrl->blockROB.reportException(inst->bid, inst->stid, "ALU excution exception(scalar)");
            LOG_INFO_M(top->machineType, Stage::E1) << " exception inst " << inst;
        }
    } else {
        auto predSrc = inst->GetPSrcPtrByType(OperandType::OPD_PREDMASK);
        uint64_t curMask = predSrc == nullptr ? inst->predMask : predSrc->data;
        bool noexcep = true;
        if (inst->dwDstType && (inst->pdsts_.size() > 0) && inst->pdsts_[0]->vecDataVld) {
            inst->pdsts_[0]->vecData.Init(static_cast<uint64_t>(OperandWidth::OPDW_D), inst->pdsts_[0]->vecData.m_lane);
        }
        for (uint32_t lane = 0; lane < inst->lanes && noexcep; ++lane) {
            if (curMask & (1ULL << lane)) {
                noexcep = handleVecExec(pipeid, lane, inst);
            }
        }
    }

    if (inst->backToCodeTemplate) {
        rf_ct_q[inst->peID]->Write(inst);
    }

    // sysset write
    if (inst->opcode == Opcode::OP_SSRSET || inst->opcode == Opcode::OP_B_DIM) {
        sys_wr_req = inst->GenSYSWFBus();
    }

    // ready table wtire
    /* FIXME: The conditions for "reduce" and "simt" might be able to be combined together. */
    // if (inst->checkAlldstRanged() && top->core->configs.reno_dynamic_enable && grp != InstGroup::REDUCE &&
    //     !top->core->IsVectorIex(top->machineType) && inst->iexType !=MEM_IEX) {
    //     RFReqBus req = inst->genWRTableBus();
    //     if (req.vld) {
    //         iex_rt_wr_q->Write(req);
    //     }
    // }
}

void VecScalarPipe::ExecJump(const SimInst& inst)
{
    inst->pipeCycle->e1Cycle = sim->getCycles();
    // if (sim->perfectGetSet) {
    //     if (inst->ref.gsInfo.src0_vld) {
    //         inst->src0.data = inst->ref.gsInfo.src0_data;
    //     }
    //     if (inst->ref.gsInfo.src1_vld) {
    //         inst->src1.data = inst->ref.gsInfo.src1_data;
    //     }
    //     if (inst->ref.gsInfo.src2_vld) {
    //         inst->src2.data = inst->ref.gsInfo.src2_data;
    //     }
    //     if (inst->ref.gsInfo.src3_vld) {
    //         inst->src3.data = inst->ref.gsInfo.src3_data;
    //     }
    // }
    // simt or scalor
    if (IsScalarInst(inst->codeLen)) {
        bool ret = inst->Execute();
        (!ret) ? top->core->bctrl->blockROB.reportException(inst->bid, inst->stid,
                                                            "BRU excute exception(scalar)") : void();
    } else {
        uint32_t jmpMask = 0;
        uint64_t curMask = inst->GetPSrcPtrByType(OperandType::OPD_PREDMASK)->data;
        const uint64_t num = 8;
        for (uint32_t lane = 0; lane < inst->lanes; ++lane) {
            if ((curMask & (1 << lane)) == 0) {
                continue;
            }
            bool ret = inst->Execute(lane);
            (!ret) ? top->core->bctrl->blockROB.reportException(inst->bid, inst->stid,
                                                                "BRU excute exception(SIMT)") : void();
            if (OpcodeIsCondInnerJump(inst->opcode) && inst->pdsts_[DST0_IDX]->data != inst->pc + num) {
                // jump situation
                jmpMask |= 1 << lane;
            }
        }
        if (OpcodeIsCondInnerJump(inst->opcode)) {
            uint32_t fallMask = curMask ^ jmpMask;
            if (fallMask == 0) {
                inst->GetPSrcPtrByType(OperandType::OPD_PREDMASK)->data = jmpMask;  // fall mask
            } else {
                inst->GetPSrcPtrByType(OperandType::OPD_PREDMASK)->data = fallMask;  // fall mask
            }
            inst->pdsts_[inst->GetDstIdxByType(OperandType::OPD_PREDMASK)]->dataVld = true;
        }

        PEResolveBus rslv = inst->GenRslvBus();
        uint32_t tid = 0;
        if (top->GetSim()->core->IsVectorIex(top->machineType)) {
            tid = inst->tid;
        } else {
            tid = inst->stid;
        }
        rslv_array[inst->peID][tid]->Write(rslv);
        return;
    }

    // ready table wtire
    // if ((inst->checkAlldstRanged() && top->core->configs.reno_dynamic_enable &&
    //     !top->core->IsVectorIex(top->machineType) && inst->iexType != MEM_IEX)) {
    //     RFReqBus req = inst->genWRTableBus();
    //     if (req.vld) {
    //         iex_rt_wr_q->Write(req);
    //     }
    // }

    // Generate PE resolved bus
    PEResolveBus rslv = inst->GenRslvBus();
    uint32_t tid = 0;
    if (top->GetSim()->core->IsVectorIex(top->machineType)) {
        tid = inst->tid;
    } else {
        tid = inst->stid;
    }
    rslv_array[inst->peID][tid]->Write(rslv);
}

void VecScalarPipe::runW1()
{
    // 1) generate rd_wr/rslv
    // 2) handle cancel
    for (auto& inst : w1_inst) {
        if (!inst) {
            continue;
        }
        inst->pipeCycle->w1Cycle = sim->getCycles();
    }
}

void VecScalarPipe::runW2()
{
    // send output bus
    for (auto& req : rf_wr_req) {
        req.Reset();
    }
    uint32_t req_cnt = 0;
    for (auto& inst : w2_inst) {
        if (!inst) {
            continue;
        }
        inst->pipeCycle->w2Cycle = sim->getCycles();

        // Generate readfile write bus
        rf_wr_req.at(req_cnt) = inst->GenRFReqBus(false);
        InstGroup grp = OpcodeManager::Inst().GetOpcodeGroup(inst->opcode);
        if (grp == InstGroup::REDUCE && inst->SrcTypeContain(OperandType::OPD_GREG)) {
            rf_wr_req.at(req_cnt).vld = false;
        }
        // Generate PE resolved bus
        if (!OpcodeIsInnerJump(inst->opcode)) {
            PEResolveBus rslv = inst->GenRslvBus();
            uint32_t tid = 0;
            if (top->GetSim()->core->IsVectorIex(top->machineType)) {
                tid = inst->tid;
            } else {
                tid = inst->stid;
            }
            rslv_array[inst->peID][tid]->Write(rslv);
        }

        req_cnt += 1;
    }
}

uint32_t VecScalarPipe::getExLatency(const SimInst& inst) const
{
    const uint32_t BRANCH_LATENCY = 2;
    uint32_t latency = ex_stage_num + 1; // default
    if (!inst) {
        return latency;
    }
    if (top->id == SCALAR_IEX) {
        // TODO
    } else if (top->core->IsVectorIex(top->machineType)) {
        // if (OpcodeIsInnerJump(inst->opcode) || OpcodeIsSetc(inst->opcode) || OpcodeIsCondInnerJump(inst->opcode)) {
        InstGroup grp = OpcodeManager::Inst().GetOpcodeGroup(inst->opcode);
        if (grp == InstGroup::BRANCH || grp == InstGroup::SETC) {
            return BRANCH_LATENCY;
        }
        if (IexLatency::GetInstExLatency(inst, latency)) {
            return latency;
        }
    }
    return latency;
}

void VecScalarPipe::LogPrint()
{
    bool logEnable = ((p1_inst != nullptr) && (*p1_inst != nullptr)) || i1_inst ||
        any_of(w1_inst.cbegin(), w1_inst.cend(), [](const SimInst& inst) {
            return inst != nullptr;
        }) || any_of(w2_inst.cbegin(), w2_inst.cend(), [](const SimInst& inst) {
            return inst != nullptr;
        }) || any_of(ex_inst.cbegin(), ex_inst.cend(), [](const vector<SimInst>& v_inst) {
            return any_of(v_inst.cbegin(), v_inst.cend(), [](const SimInst& inst) {
                return inst != nullptr;
            });
        });
    if (!logEnable) {
        return;
    }
    if ((p1_inst != nullptr) && (*p1_inst != nullptr)) {
        LOG_INFO_M(top->machineType, Stage::P1) << "[Vec Scalar  Pipe      Line" << dec << pipeid << "]: " <<
            (*p1_inst)->Dump();
    }
    if (i1_inst) {
        LOG_INFO_M(top->machineType, Stage::I1) << "[Vec Scalar  Pipe      Line" << dec << pipeid << "]: " << i1_inst->Dump();
    }
    for (uint32_t stg_i = 0; stg_i < ex_inst.size(); ++stg_i) {
        for (auto& inst : ex_inst[stg_i]) {
            if (inst) {
                LOG_INFO_M(top->machineType, static_cast<Stage>(static_cast<uint32_t>(Stage::E1) + stg_i)) <<
                    "[ALU Line" << dec << pipeid << "]: " << inst->Dump();
            }
        }
    }
    for (auto& inst : w1_inst) {
        if (inst) {
            LOG_INFO_M(top->machineType, Stage::WB) << "[Vec Scalar  Pipe      Line" << dec << pipeid << "]: " << inst->Dump();
        }
    }
    for (auto& inst : w2_inst) {
        if (inst) {
            LOG_INFO_M(top->machineType, Stage::W2) << "[Vec Scalar  Pipe      Line" << dec << pipeid << "]: " << inst->Dump();
        }
    }
}

} // namespace JCore
