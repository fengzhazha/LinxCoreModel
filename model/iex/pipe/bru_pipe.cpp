#include "core/Core.h"
#include "iex/pipe/iex_pipe.h"

namespace JCore {


using namespace std;

void BRUPipe::Build(uint32_t id) {
    pipeid = id;

    i1_inst = std::make_shared<SimInstInfo>();
    i2_inst = std::make_shared<SimInstInfo>();
    e1_inst = std::make_shared<SimInstInfo>();
}

void BRUPipe::Reset() {
    i1_inst = std::shared_ptr<SimInstInfo>(nullptr);
    i2_inst = std::shared_ptr<SimInstInfo>(nullptr);
    e1_inst = std::shared_ptr<SimInstInfo>(nullptr);
}

void BRUPipe::Work() {
    runE1();
    runI2();
    runI1();
    runP1();
    LogPrint();
}

void BRUPipe::moveLpv() {
    auto pipeMoveLpv = [](SimInst &inst) {
        if (inst) {
            inst->MoveLpv();
        }
    };

    pipeMoveLpv(e1_inst);
    pipeMoveLpv(i2_inst);
    pipeMoveLpv(i1_inst);
}

void BRUPipe::move() {
    e1_inst = i2_inst;
    i2_inst = i1_inst;
    i1_inst = *p1_inst;
}

void BRUPipe::flush(FlushBus &flushReq) {
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
    flush_stage(i2_inst);
    flush_stage(e1_inst);
}

void BRUPipe::runP1() {
    if ((p1_inst == nullptr) || (*p1_inst == nullptr)) {
        return;
    }
    (*p1_inst)->pipeCycle->p1Cycle = sim->getCycles();
}

void BRUPipe::runI1() {
    rf_rd_req.Reset();
    if (!i1_inst) {
        return;
    }
    i1_inst->pipeCycle->i1Cycle = sim->getCycles();

    rf_rd_req = i1_inst->GenRFReqBus(true);
    rf_rd_req.pipeid = pipeid;

    sys_rd_req = i1_inst->GenSYSReqBus();
}

void BRUPipe::runI2() {
    if (!i2_inst) {
        return;
    }
    i2_inst->pipeCycle->i2Cycle = sim->getCycles();

    if (!rf_data_ret.vld) {
        return;
    }
    i2_inst->RFRetSetData(rf_data_ret);

    if (!sys_data_ret.vld) {
        return;
    }
    i2_inst->RecSYSReqRet(sys_data_ret);
}

void BRUPipe::runE1() {
    rf_wr_req.Reset();
    if (!e1_inst) {
        return;
    }
    e1_inst->pipeCycle->e1Cycle = sim->getCycles();
    // if (sim->perfectGetSet) {
    //     if (e1_inst->ref.gsInfo.src0_vld) {
    //         e1_inst->src0.data = e1_inst->ref.gsInfo.src0_data;
    //     }
    //     if (e1_inst->ref.gsInfo.src1_vld) {
    //         e1_inst->src1.data = e1_inst->ref.gsInfo.src1_data;
    //     }
    //     if (e1_inst->ref.gsInfo.src2_vld) {
    //         e1_inst->src2.data = e1_inst->ref.gsInfo.src2_data;
    //     }
    //     if (e1_inst->ref.gsInfo.src3_vld) {
    //         e1_inst->src3.data = e1_inst->ref.gsInfo.src3_data;
    //     }
    // }
    // simt or scalor
    if (IsScalarInst(e1_inst->codeLen)) {
        bool ret = e1_inst->Execute();
        (!ret) ? top->core->bctrl->blockROB.reportException(e1_inst->bid, e1_inst->stid,
            "BRU excute exception(scalar)") : void();
    } else {
        uint32_t jmpMask = 0;
        auto predSrc = e1_inst->GetPSrcPtrByType(OperandType::OPD_PREDMASK);
        uint64_t curMask = predSrc == nullptr ? e1_inst->predMask : predSrc->data;
        const uint64_t num = 8;
        for (uint32_t lane = 0; lane < e1_inst->lanes; ++lane) {
            if ((curMask & (1 << lane)) == 0) {
                continue;
            }
            bool ret = e1_inst->Execute(lane);
            (!ret) ? top->core->bctrl->blockROB.reportException(e1_inst->bid, e1_inst->stid,
                "BRU excute exception(SIMT)") : void();
            if (OpcodeIsCondInnerJump(e1_inst->opcode) && e1_inst->pdsts_[DST0_IDX]->data != e1_inst->pc + num) {
                // jump situation
                jmpMask |= 1 << lane;
            }
        }
        if (OpcodeIsCondInnerJump(e1_inst->opcode)) {
            uint32_t fallMask = curMask ^ jmpMask;
            auto predMaskDst = e1_inst->pdsts_[e1_inst->GetDstIdxByType(OperandType::OPD_PREDMASK)];
            if (fallMask == 0) {
                predMaskDst->data = jmpMask;  // fall mask
            } else {
                predMaskDst->data = fallMask;  // fall mask
            }
            predMaskDst->dataVld = true;
        }

        PEResolveBus rslv = e1_inst->GenRslvBus();
        uint32_t tid = 0;
        if (top->GetSim()->core->IsVectorIex(top->machineType)) {
            tid = e1_inst->tid;
        } else {
            tid = e1_inst->stid;
        }
        rslv_array[e1_inst->peID][tid]->Write(rslv);
        return;
    }
    // Generate register write bus
    RFReqBus &req = rf_wr_req;
    req = e1_inst->GenRFReqBus(false);

    // ready table wtire
    // if ((e1_inst->checkAlldstRanged() && top->core->configs.reno_dynamic_enable &&
    //     !top->core->IsVectorIex(e1_inst->iexType) && e1_inst->iexType != MEM_IEX)) {
    //     RFReqBus req = e1_inst->genWRTableBus();
    //     if (req.vld) {
    //         iex_rt_wr_q->Write(req);
    //     }
    // }

    // Generate PE resolved bus
    uint32_t tid = 0;
    if (top->GetSim()->core->IsVectorIex(top->machineType)) {
        tid = e1_inst->tid;
    } else {
        tid = e1_inst->stid;
    }
    PEResolveBus rslv = e1_inst->GenRslvBus();
    rslv_array[e1_inst->peID][tid]->Write(rslv);
}

void BRUPipe::LogPrint()
{
    bool logEnable = ((p1_inst != nullptr) && (*p1_inst != nullptr)) || i1_inst || i2_inst || e1_inst;
    if (!logEnable) {
        return;
    }
    if ((p1_inst != nullptr) && (*p1_inst != nullptr)) {
        LOG_INFO_M(top->machineType, Stage::P1) << "[BRU Line" << dec << pipeid << "] " << (*p1_inst)->Dump();
    }
    if (i1_inst) {
        LOG_INFO_M(top->machineType, Stage::I1) << "[BRU Line" << dec << pipeid << "] " << i1_inst->Dump();
    }
    if (i2_inst) {
        LOG_INFO_M(top->machineType, Stage::I2) << "[BRU Line" << dec << pipeid << "] " << i2_inst->Dump();
    }
    if (e1_inst) {
        LOG_INFO_M(top->machineType, Stage::E1) << "[BRU Line" << dec << pipeid << "] " << e1_inst->Dump();
    }
}

} // namespace JCore
