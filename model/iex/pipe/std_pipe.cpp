#include "core/Core.h"
#include "iex/pipe/iex_pipe.h"

namespace JCore {


using namespace std;
void STDPipe::Build(uint32_t id) {
    pipeid = id;

    i1_inst = std::make_shared<SimInstInfo>();
    i2_inst = std::make_shared<SimInstInfo>();
    e1_inst = std::make_shared<SimInstInfo>();
}

void STDPipe::Reset() {
    i1_inst = std::shared_ptr<SimInstInfo>(nullptr);
    i2_inst = std::shared_ptr<SimInstInfo>(nullptr);
    e1_inst = std::shared_ptr<SimInstInfo>(nullptr);
}

void STDPipe::Work() {
    runE1();
    runI2();
    runI1();
    runP1();
    LogPrint();
}

void STDPipe::moveLpv() {
    auto pipeMoveLpv = [](SimInst &inst) {
        if (inst) {
            inst->MoveLpv();
        }
    };

    pipeMoveLpv(e1_inst);
    pipeMoveLpv(i2_inst);
    pipeMoveLpv(i1_inst);
}

void STDPipe::move() {
    e1_inst = i2_inst;
    i2_inst = i1_inst;
    i1_inst = *p1_inst;
    *p1_inst = nullptr;
}

void STDPipe::flush(FlushBus &flushReq) {
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

void STDPipe::runP1() {
    if ((p1_inst == nullptr) || (*p1_inst == nullptr)) {
        return;
    }
    (*p1_inst)->pipeCycle->p1Cycle = sim->getCycles();
}

void STDPipe::runI1() {
    rf_rd_req.Reset();
    if (!i1_inst) {
        return;
    }
    i1_inst->pipeCycle->i1Cycle = sim->getCycles();

    rf_rd_req = i1_inst->GenRFReqBus(true);
    rf_rd_req.pipeid = pipeid;
    sys_rd_req = i1_inst->GenSYSReqBus();
}

void STDPipe::runI2() {
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

void STDPipe::runE1() {
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
                                                            "STD execute exception(scalar).") : void();
        // Generate store data request bus
        uint32_t peCnt = GetSim()->core->configs.stdPeCount + GetSim()->core->configs.simtPeCount;
        MemReqBus req = e1_inst->GenMemReq(peCnt);
        if (req.vld) {
            req.realReqCnt = 1;
            req.laneSet.insert(0);
            std_req_q[e1_inst->stid]->Write(req);
        }
    } else {
        uint64_t addr = -1U;
        auto predSrc = e1_inst->GetPSrcPtrByType(OperandType::OPD_PREDMASK);
        uint64_t curMask = predSrc == nullptr ? e1_inst->predMask : predSrc->data;
        std::unordered_map<uint64_t, std::set<uint32_t>> addrLaneMap;
        for (uint32_t lane = 0; lane < e1_inst->lanes; ++lane) {
            if (curMask & (1 << lane)) {
                bool ret = e1_inst->Execute(lane);
                (!ret) ? top->core->bctrl->blockROB.reportException(e1_inst->bid, e1_inst->stid,
                                                                    "STD execute exception(SIMT)") : void();
                ++e1_inst->realReqCnt;
                addr = e1_inst->pdsts_[DST0_IDX]->vecData.Get(lane, static_cast<uint32_t>(OperandWidth::OPDW_D));
                addrLaneMap[addr].insert(lane);
            }
        }

        uint32_t peCnt = GetSim()->core->configs.stdPeCount + GetSim()->core->configs.simtPeCount;
        for (auto it : addrLaneMap) {
            MemReqBus req = e1_inst->GenMemReq(peCnt, *it.second.begin());
            req.realReqCnt = e1_inst->realReqCnt;
            if (req.vld) {
                req.simtLane = *it.second.begin();
                req.laneSet = it.second;
                std_req_q[e1_inst->stid]->Write(req);
            }
        }
    }

    // Generate register write bus for local regfile
    rf_wr_req = e1_inst->GenRFReqBus(false);
}

void STDPipe::LogPrint()
{
    bool logEnable = ((p1_inst != nullptr) && (*p1_inst != nullptr)) || i1_inst || i2_inst || e1_inst;
    if (!logEnable) {
        return;
    }
    if ((p1_inst != nullptr) && (*p1_inst != nullptr)) {
        LOG_DEBUG_M(top->machineType, Stage::P1) << "[STD Line" << dec << pipeid << "] " << (*p1_inst)->Dump();
    }
    if (i1_inst) {
        LOG_DEBUG_M(top->machineType, Stage::I1) << "[STD Line" << dec << pipeid << "] " << i1_inst->Dump();
    }
    if (i2_inst) {
        LOG_DEBUG_M(top->machineType, Stage::I2) << "[STD Line" << dec << pipeid << "] " << i2_inst->Dump();
    }
    if (e1_inst) {
        LOG_DEBUG_M(top->machineType, Stage::E1) << "[STD Line" << dec << pipeid << "] " << e1_inst->Dump();
    }
}

} // namespace JCore
