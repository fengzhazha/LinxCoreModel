#include "core/Core.h"
#include "iex/pipe/iex_pipe.h"

namespace JCore {


using namespace std;

void CMDPipe::Build(uint32_t id)
{
    pipeid = id;

    i1_inst = std::make_shared<SimInstInfo>();
    i2_inst = std::make_shared<SimInstInfo>();
    e1_inst = std::make_shared<SimInstInfo>();
    w1_inst = std::make_shared<SimInstInfo>();
}

void CMDPipe::Reset()
{
    i1_inst = std::shared_ptr<SimInstInfo>(nullptr);
    i2_inst = std::shared_ptr<SimInstInfo>(nullptr);
    e1_inst = std::shared_ptr<SimInstInfo>(nullptr);
    w1_inst = std::shared_ptr<SimInstInfo>(nullptr);
}

void CMDPipe::Work()
{
    runW1();
    runE1();
    runI2();
    if (CheckTileRegStall()) {
        return;
    }
    runI1();
    runP1();
    LogPrint();
}

void CMDPipe::moveLpv()
{
    auto pipeMoveLpv = [](SimInst &inst) {
        if (inst) {
            inst->MoveLpv();
        }
    };

    pipeMoveLpv(w1_inst);
    pipeMoveLpv(e1_inst);
    pipeMoveLpv(i2_inst);
    pipeMoveLpv(i1_inst);
}

void CMDPipe::move()
{
    if (CheckTileRegStall()) {
        w1_inst = nullptr;
        return;
    }
    w1_inst = e1_inst;
    e1_inst = i2_inst;
    i2_inst = i1_inst;
    i1_inst = *p1_inst;
    *p1_inst = nullptr;
}

void CMDPipe::flush(FlushBus &flushReq)
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
    flush_stage(i2_inst);
    flush_stage(e1_inst);
    flush_stage(w1_inst);
}

void CMDPipe::runP1()
{
    if ((p1_inst == nullptr) || (*p1_inst == nullptr)) {
        return;
    }
    (*p1_inst)->pipeCycle->p1Cycle = sim->getCycles();
}

void CMDPipe::runI1()
{
    rf_rd_req.Reset();
    if (!i1_inst) {
        return;
    }
    i1_inst->pipeCycle->i1Cycle = sim->getCycles();

    rf_rd_req = i1_inst->GenRFReqBus(true);
    rf_rd_req.pipeid = pipeid;
}

void CMDPipe::runI2()
{
    // do bypass from all pipes
    if (!i2_inst) {
        return;
    }
    i2_inst->pipeCycle->i2Cycle = sim->getCycles();

    i2_inst->RFRetSetData(rf_data_ret);
}

void CMDPipe::runE1()
{
    if (!e1_inst) {
        return;
    }
    e1_inst->pipeCycle->e1Cycle = sim->getCycles();
    e1_inst->Execute();
    if (CheckTileRegStall()) {
        return;
    }
    top->cmdIQBISQ->Write(e1_inst);
}

void CMDPipe::runW1()
{
    if (!w1_inst) {
        return;
    }
    w1_inst->pipeCycle->w1Cycle = sim->getCycles();

    // Generate PE resolved bus
    PEResolveBus rslv = w1_inst->GenRslvBus();
    uint32_t tid = 0;
    if (top->GetSim()->core->IsVectorIex(top->machineType)) {
        tid = w1_inst->tid;
    } else {
        tid = w1_inst->stid;
    }
    rslv_array[w1_inst->peID][tid]->Write(rslv);
}

void CMDPipe::LogPrint()
{
    bool logEnable = ((p1_inst != nullptr) && (*p1_inst != nullptr)) || i1_inst || i2_inst || e1_inst || w1_inst;
    if (!logEnable) {
        return;
    }
    if ((p1_inst != nullptr) && (*p1_inst != nullptr)) {
        LOG_DEBUG_M(top->machineType, Stage::P1) << "[CMD Line" << dec << pipeid << "] " << (*p1_inst)->Dump();
    }
    if (i1_inst) {
        LOG_DEBUG_M(top->machineType, Stage::I1) << "[CMD Line" << dec << pipeid << "] " << i1_inst->Dump();
    }
    if (i2_inst) {
        LOG_DEBUG_M(top->machineType, Stage::I2) << "[CMD Line" << dec << pipeid << "] " << i2_inst->Dump();
    }
    if (e1_inst) {
        LOG_DEBUG_M(top->machineType, Stage::E1) << "[CMD Line" << dec << pipeid << "] " << e1_inst->Dump();
    }
    if (w1_inst) {
        LOG_DEBUG_M(top->machineType, Stage::WB) << "[CMD Line" << dec << pipeid << "] " << w1_inst->Dump();
    }
}

bool CMDPipe::CheckTileRegStall()
{
    return e1_inst != nullptr && (e1_inst->opcode == Opcode::OP_B_IOT) &&
                                 GetSim()->core->bctrl->blockIssueQueueUnit.CheckTileRegStall(e1_inst);
}

} // namespace JCore
