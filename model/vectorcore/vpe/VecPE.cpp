#include "VecPE.h"

#include "core/Core.h"

namespace JCore {

VecPE::VecPE(const std::string& nameVal, uint32_t vecCoreIdVal)
    : name(nameVal), vecCoreId(vecCoreIdVal)
{}

void VecPE::Work()
{
    decoder.Work();
    rename.Work();
    ifu.Work();

    for (auto &rob : prob) {
        rob->Work();
    }
    SetNonFlush();
}

void VecPE::Xfer()
{
    for (auto &rob : prob) {
        rob->Xfer();
    }

    decoder.Xfer();
    ifuDecThdQ.Work();
    rename.Xfer();
    ifu.Xfer();
}

void VecPE::Reset()
{

}

void VecPE::ReportStat()
{
    if (stats) {
        stats->ReportVec(name);
    }
}

SimSys* VecPE::GetSim()
{
    return sim;
}

VecPE::~VecPE()
{
    ReleaseVecPtr(prob);
}

void VecPE::Build()
{
    configs.overrideDefaultConfig(GetSim()->getCfgs());
    stats = std::make_shared<VecPEStats>(GetSim()->getRpt());
    threadCnt = configs.threadNum;
    pe_ifu_q = ThdRingQ<BlkBodyFetchState>(configs.threadNum, configs.pe_bbuf_depth);
    workThdQ.resize(configs.threadNum);
    for (uint32_t i = 0; i < configs.threadNum; i++) {
        workThdQ[i] = RingQueue<BlockRunInfo>(configs.pe_bbuf_depth);
    }
    prob.clear();
    for (uint32_t i = 0; i < threadCnt; ++i) {
        VecPEROB *rob = new VecPEROB();
        prob.emplace_back(rob);
        prob[i]->top = this;
        prob[i]->Build(i);
    }

    decoder.vpeTop = this;
    decoder.ifuDecThdQ = &ifuDecThdQ;
    decoder.workThdQ.resize(configs.threadNum);
    for (uint32_t i = 0; i < configs.threadNum; ++i) {
        decoder.workThdQ[i] = &workThdQ[i];
    }
    decoder.dec_ren_q = &dec_ren_q;
    decoder.Build();

    rename.vpeTop = this;
    rename.sim = sim;
    rename.dec_ren_q = &dec_ren_q;
    rename.lgprRF = lgprRF;
    rename.vrfRename = vrfRename;
    rename.Build();

    workThdQ.resize(configs.threadNum);
    for (uint32_t i = 0; i < configs.threadNum; i++) {
        workThdQ[i] = RingQueue<BlockRunInfo>(configs.pe_bbuf_depth);
    }
    ifuDecThdQ.Build(configs.threadNum, configs.inst_buffer_size);

    brq = RingQueue<BrqEntry>(16); // 后续需要删除
    ifu.sim = GetSim();
    ifu.machineType = MachineType::VIFU;
    ifu.top = this; // 待定。
    ifu.pe_ifu_q = &pe_ifu_q;
    ifu.ifuDecThdQ = &ifuDecThdQ;
    ifu.brq = &brq;
    ifu.ifu_l2_q = ifu_l2_q;
    ifu.snp_l2_q = snp_l2_q;
    ifu.l2_ifu_q = l2_ifu_q;
    ifu.Build(peID, configs.decodeWidth, configs.decodeWidth);

    stats->logicROBStall.resize(threadCnt, 0);
    stats->mapQStall.resize(threadCnt, 0);

    stats->vecThreadRobBussyCyc.resize(threadCnt, 1);
    stats->vecThreadRobBussyInstNum.resize(threadCnt, 0);

    peMInstStats.Reset(core->configs.block_rob_depth);
}

IDBus VecPE::GetCommitID(uint32_t tid)
{
    return prob[tid]->getCommitID();
}

IDBus VecPE::GetRetireID(uint32_t tid)
{
    return prob[tid]->getRetireID();
}

IDBus VecPE::GetRetireID()
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

ROBIDBus VecPE::GetOldestLSID(uint32_t tid)
{
    ASSERT(tid != -1U);
    return prob[tid]->GetOldestLSID();
}

void VecPE::PrintROBStatus()
{
    for (auto &rob : prob) {
        rob->PrintStatus();
    }
}

void VecPE::SetMemWakeup(MemReqBus mem)
{
    if (!mem.vld || mem.stack_vld) return;
}

void VecPE::SetMemData(MemReqBus mem)
{
    mem.peID = peID;
    std::shared_ptr<IEX> iex;
    if (GetSim()->core->IsVectorIex(mem.iexTyp)) {
        iex = GetSim()->core->vectorTop->GetIex(coreId);
    } else {
        iex = GetSim()->core->iex[mem.iexTyp];
    }
    iex->setMemData(mem);
}

void VecPE::SetNeedFlush(ROBID bid, ROBID rid, ROBID lsID, uint32_t tid)
{
    prob[tid]->setNeedFlush(bid, rid, lsID);
}

void VecPE::Flush(FlushBus flushReq)
{
    ifu.setFlush(flushReq);
    decoder.Flush(flushReq);
    rename.Flush(flushReq);
}

void VecPE::ReportNuke(FlushBus &flushReq)
{
    if (!flushReq.req.vld) {
        return;
    }
}

void VecPE::ResetBlockInstStats(uint32_t bid)
{
    peMInstStats.ReleaseEntry(bid);
}

void VecPE::IncS1Stats()
{
    stats->s1_inst_cnt++;
}

void VecPE::IncIBStallStats()
{
    stats->ibStall++;
}

void VecPE::SetMemResolve(MemReqBus mem)
{
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

void VecPE::SetNonFlush()
{
    for (uint32_t tid = 0; tid < prob.size(); ++tid) {
        IDBus oldestBus = prob[tid]->GetCommitBusID();
        if (!oldestBus.vld) {
            continue;
        }

        StNonFlushBus stNonFlushSignal;
        stNonFlushSignal.peid = peID;
        stNonFlushSignal.tid = tid;
        stNonFlushSignal.bid = oldestBus.bid;
        stNonFlushSignal.gid = oldestBus.gid;
        stNonFlushSignal.rid = oldestBus.rid;
        stNonFlushSignal.lsID = oldestBus.lsID;
        pe_store_nf_q->Write(stNonFlushSignal);

        LdNonFlushBus ldNonFlushLdSignal;
        ldNonFlushLdSignal.peid = peID;
        ldNonFlushLdSignal.tid = tid;
        ldNonFlushLdSignal.bid = oldestBus.bid;
        ldNonFlushLdSignal.gid = oldestBus.gid;
        ldNonFlushLdSignal.rid = oldestBus.rid;
        ldNonFlushLdSignal.lsID = oldestBus.lsID;
        pe_load_nf_q->Write(ldNonFlushLdSignal);
    }
}
} // namespace JCore
