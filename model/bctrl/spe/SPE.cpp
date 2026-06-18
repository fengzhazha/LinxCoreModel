#include "SPE.h"

#include "core/Core.h"

namespace JCore {
void SPE::Work()
{
    dcTop.Work();
    d2Stage.Work();

    for (auto &rob : prob) {
        rob->Work();
    }
    DoStats();
    HandleStoreResolve();
}

void SPE::Xfer()
{
    dcTop.Xfer();
    d2Stage.Xfer();
    dec_ren_q.Work();
    for (auto &rob : prob) {
        rob->Xfer();
    }
}

void SPE::Reset()
{

}

void SPE::ReportStat()
{

}

SimSys* SPE::GetSim()
{
    return sim;
}

SPE::~SPE()
{
    ReleaseVecPtr(prob);
}

void SPE::Build()
{
    configs.overrideDefaultConfig(GetSim()->getCfgs());
    stats = std::make_shared<SPEStats>(GetSim()->getRpt());
    threadCnt = core->configs.scalar_smt_thread;
    machineType = MachineType::SPE;

    dcTop.sim = sim;
    dcTop.dec_ren_q = &dec_ren_q;
    dcTop.top = this;
    dcTop.Build();

    d2Stage.top = this;
    d2Stage.sim = sim;
    d2Stage.dec_ren_q = &dec_ren_q;
    d2Stage.Build();

    prob.clear();
    for (uint32_t i = 0; i < threadCnt; ++i) {
        SPEROB *rob = new SPEROB();
        prob.emplace_back(rob);
        prob[i]->speTop = this;
        prob[i]->m_tid = i;
        prob[i]->Build();
    }
    peMInstStats.Reset(GetSim()->core->configs.block_rob_depth);
    stats->logicROBStall.resize(threadCnt, 0);
    stats->mapQStall.resize(threadCnt, 0);
}

void SPE::Flush(FlushBus flushReq)
{
    if (!flushReq.req.vld) {
        return;
    }
    uint32_t stid = flushReq.req.stid;
    prob[stid]->flush(flushReq);
    auto matchPERslv = [&flushReq, this] (PEResolveBus peRslv) -> bool {
        if (peRslv.stid != flushReq.req.stid) {
            return false;
        }
        if (flushReq.baseOnBid){
            if (LessEqual(flushReq.req.bid, peRslv.bid)) {
                return true;
            }
        } else {
            if (LessEqual(flushReq.req.bid, flushReq.req.rid, peRslv.bid, peRslv.rid)) {
                return true;
            }
        }
        return false;
    };

    iex_pe_rslv_array[flushReq.req.stid]->FlushIf(matchPERslv);

    dcTop.flush(flushReq);
    d2Stage.Flush(flushReq);

    auto flushInst = [&flushReq](SimInst inst)->bool {
        if (inst->stid != flushReq.req.stid) {
            return false;
        }
        return LessEqual(flushReq.req.bid, inst->bid);
    };
    dec_ren_q.FlushIf(flushInst);
}

void SPE::DoStats()
{
    stats->cycles ++;
    stats->slots_total += configs.decodeWidth;

    stats->robDepth = prob[0]->getROBSize();
    stats->max_rob_size = (stats->max_rob_size > prob[0]->getROBSize()) ? stats->max_rob_size : prob[0]->getROBSize();
}

void SPE::HandleStoreResolve()
{
    while (!lsu_pe_rslv_array->Empty()) {
        MemReqBus memResp = lsu_pe_rslv_array->Read();
        ASSERT(memResp.tid != -1U);
        SetMemResolve(memResp);
        SetMemWakeup(memResp);
    }
}

IDBus SPE::GetCommitID(uint32_t tid)
{
    return prob[tid]->getCommitID();
}

IDBus SPE::GetRetireID(uint32_t tid)
{
    return prob[tid]->getRetireID();
}

IDBus SPE::GetRetireID()
{
    IDBus oldest;
    for (uint32_t tid = 0; tid < prob.size(); ++tid) {
        IDBus idBus = prob[tid]->getRetireID();
        if (!idBus.vld) {
            continue;
        }
        if (!oldest.vld || LessEqual(idBus.bid, idBus.gid, idBus.rid, oldest.bid, oldest.gid, oldest.rid)) {
            oldest = idBus;
        }
    }
    return oldest;
}

ROBIDBus SPE::GetOldestLSID(uint32_t tid)
{
    ASSERT(tid != -1U);
    return prob[tid]->GetOldestLSID();
}

void SPE::PrintROBStatus()
{
    for (auto &rob : prob) {
        rob->PrintStatus();
    }
}

void SPE::SetMemWakeup(MemReqBus mem)
{
    if (!mem.vld || mem.stack_vld) return;
}

void SPE::SetMemData(MemReqBus mem)
{
    mem.peID = peID;
    std::shared_ptr<IEX> iex = GetSim()->core->iex[mem.iexTyp];
    iex->setMemData(mem);
}

void SPE::SetNeedFlush(ROBID bid, ROBID rid, ROBID lsID, uint32_t tid)
{
    prob[tid]->setNeedFlush(bid, rid, lsID);
}

void SPE::ReportNuke(FlushBus &flushReq)
{
    if (!flushReq.req.vld) {
        return;
    }
}

void SPE::ResetBlockInstStats(uint32_t bid)
{
    peMInstStats.ReleaseEntry(bid);
}

void SPE::IncS1Stats()
{
    stats->s1_inst_cnt++;
}

void SPE::IncIBStallStats()
{
    stats->ibStall++;
}

void SPE::SetMemResolve(MemReqBus mem) {
    if (!mem.vld || (mem.stack_vld && mem.is_load)) return;

    uint32_t rid = mem.rid.val;
    PROBStatus status = prob[mem.tid]->getROBEntry(rid).status;
    if (status == INST_RETIRED || status == INST_COMPLETED || status == INST_NEEDFLUSH) {
        return;
    }

    PROBEntry &e = prob[mem.tid]->getNextEntry(rid);
    e.inst->retLane += mem.laneSet.size();
    if (e.inst->retLane < mem.realReqCnt) {
        return;
    }

    e.status = INST_COMPLETED;
    e.inst->pipeCycle->completeCycle = GetSim()->getCycles() + 1;
    LOG_INFO_M(machineType, Stage::NA) << "[VecPE" << peID << "]: " << "store resolve. " << mem;
}

bool SPE::IsPEIdle()
{
    if (dcTop.statsInfo[peID].allocROBCnt == 0) {
        for (auto &rob : prob) {
            if (rob->getROBOsdSize() != 0) {
                return false;
            }
        }
        return true;
    }
    return false;
}
}