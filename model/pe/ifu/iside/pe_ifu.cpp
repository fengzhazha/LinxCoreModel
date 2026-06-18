#include "pe/ifu/iside/pe_ifu.h"

#include "core/Core.h"

namespace JCore {


using namespace std;

namespace PE_IFU {
void IFUComponent::RegisterComponent(PEIFU* ifu)
{
    this->sim = ifu->GetSim();
    this->ifu = ifu;
    this->configs = &ifu->configs;
    this->stats = ifu->stats;
    this->id = ifu->ifuID;
    this->utils = ifu->utils;
    Build();
}

PEIFU::PEIFU()
{
    sp.peifu = this;
}

PEIFU::~PEIFU()
{
    DeletePtr(stats);
}

void PEIFU::Dump() const
{
    cout << "CT pe_ifu_q Dump ++++++++++++++++++++" << endl;
    for (uint64_t i = 0; i < 4; i++) {
        for (uint64_t j = 0; j < pe_ifu_q->Size(i); j++) {
            auto& item = pe_ifu_q->At(j, i);
            cout << "CT pe_ifu_q[" << i << " tid][" << j << "]" << endl;
            cout << "bid" << item.bid << endl;
            cout << "gid" << item.gid << endl;
        }
        cout << endl;
        for (auto &fetchReq : pipe[i].fetchReq) {
            if (!fetchReq) {
                continue;
            }
            for (uint32_t j = 0; j < fetchReq->instCnt; ++j) {
                cout << "IFU block " << fetchReq->bid << " Group " << fetchReq->gid << " fid: " << fetchReq->fid
                    << " STID: " << dec << fetchReq->stid
                    << " start TPC: 0x" << hex << fetchReq->tpc << " current tpc: 0x" << fetchReq->tpcArr[j]
                    << " bin: 0x" << fetchReq->data[j]
                    << " isize: " << dec << fetchReq->isize[j] << endl;
            }
        }
    }
}

void PEIFU::Work()
{
    if (configs.inst_bp_mode == 1) {
        RunPerfIfu();
        return;
    }
    RunAtPre();

    RunF3();
    RunF2();
    RunF1();
    RunF0();

    PipeCtrl();
    PipeMove();

    icache.SendMissReq();
}

void PEIFU::Xfer()
{
    f4SimBuffer.Work();
    f5SimBuffer.Work();
}

void PEIFU::Reset()
{
    pipe.clear();
    pipe.resize(NSTAGE);
    for (uint32_t i = 0; i < pipe.size(); i++) {
        pipe[i].Build(configs.ifu_fetch_req_num);
    }
    genFetchReqPtr.clear();
    pe_ifu_q->ResetAll();
    pre_fetch_q.ResetAll();
    rahq.Reset();
    f4SimBuffer.Reset();
    f5SimBuffer.Reset();
    ifuDecThdQ->ResetAll();
    m_curF0ThreadId = 0;

    merge.clear();
    mergeBin.clear();
    mergeSize.clear();
    mergeGid.clear();
    mergeStid.clear();
}

void PEIFU::Build(uint32_t peID, uint64_t stdDecWidth, uint64_t simtDecWidth)
{
    constexpr uint32_t IFU_PKT_ID_OFFSET = 16;
    this->stdDecWidth = stdDecWidth;
    this->simtDecWidth = simtDecWidth;
    this->ifuID = peID;
    this->memID = this->ifuID + IFU_PKT_ID_OFFSET;
    this->logicalGroups = top->GetThread();
    configs.overrideDefaultConfig(GetSim()->getCfgs());
    this->verbose = configs.verbose;
    if (configs.inst_bp_mode == 1) {
        configs.icache_perfect = true;
    }
    stats = new IFUStats(GetSim()->getRpt());
    pipe.resize(NSTAGE);
    for (uint32_t i = 0; i < pipe.size(); i++) {
        pipe[i].Build(configs.ifu_fetch_req_num);
    }
    icache.RegisterComponent(this);
    ibp.RegisterComponent(this);
    preDecodor.RegisterComponent(this);
    sp.RegisterComponent(this);

    pre_fetch_q = ThdRingQ<FetchInfo>(this->logicalGroups, configs.icache_prefq_depth);
    genFetchReqPtr.resize(logicalGroups);
    for (auto &p : genFetchReqPtr) {
        p = 0;
    }
    rahq = RingQueue<FetchReqPtr>(configs.ifu_buffer_depth);
    m_curF0ThreadId = 0;

    merge = std::vector<bool>(top->GetThread(), false);
    mergeBin = std::vector<uint64_t>(top->GetThread(), 0);
    mergeSize = std::vector<uint32_t>(top->GetThread(), 0);
    mergeGid = std::vector<ROBID>(top->GetThread(), ROBID());
    mergeStid = std::vector<uint32_t>(top->GetThread(), -1U);
    hasPrefMap = RingQueue<uint64_t>(configs.ifu_pref_done_map_depth);
    f2PipeWait.resize(top->GetThread());
}

void PEIFU::StallEnable()
{
    ifustall = true;
    f4SimBuffer.setStall();
    f5SimBuffer.setStall();
}

void PEIFU::StallDisable()
{
    ifustall = false;
    f4SimBuffer.unsetStall();
    f5SimBuffer.unsetStall();
}

void PEIFU::setFlush(FlushBus &flushReq)
{
    LOG_INFO_M(top->machineType, Stage::NA) << "[IFU" << dec << ifuID << " Flush        Stage]: flush request " << flushReq;
    auto matchCtrl = [&flushReq] (BlkBodyFetchState control) -> bool {
        if (!control.vld) {
            return false;
        } else {
            return control.stid == flushReq.req.stid && LessEqual(flushReq.req.bid, control.bid);
        }
    };
    auto matchReq = [this, &flushReq] (FetchReqPtr const& fetchReq) -> bool {
        if (!fetchReq) {
            return false;
        }
        if (LessEqual(flushReq.req.bid, fetchReq->bid) && fetchReq->stid == flushReq.req.stid) {
            LOG_INFO_M(top->machineType, Stage::NA) << "[IFU" << dec << ifuID << " Flush        Stage]: flush rahq " << *(fetchReq);
            return true;
        }
        return false;
    };
    auto matchBundle = [&flushReq] (DecodeBundle &bundle) -> bool {
        if (!bundle.vld) {
            return false;
        } else {
            return LessEqual(flushReq.req.bid, bundle.bid) && bundle.stid == flushReq.req.stid;
        }
    };
    rahq.FlushIf(matchReq);

    std::vector<int> flushTid;
    if (flushReq.baseOnThread) {
        ASSERT(flushReq.req.tid >= 0 && flushReq.req.tid < top->GetThread());
        flushTid.push_back(flushReq.req.tid);
    } else {
        for (uint32_t tid = 0; tid < top->GetThread(); ++tid) {
            flushTid.push_back(tid);
        }
    }

    f4SimBuffer.FlushIf(matchBundle);
    f5SimBuffer.FlushIf(matchBundle);

    auto prefFlush = [this, &flushReq] (FetchInfo &info) -> bool {
        return (info.bid == flushReq.req.bid) && info.stid == flushReq.req.stid;
    };

    for (uint32_t tid : flushTid) {
        pre_fetch_q.FlushIf(prefFlush, tid);
        pe_ifu_q->FlushIf(matchCtrl, tid);
        ifuDecThdQ->FlushIf(matchBundle, tid);
        genFetchReqPtr[tid] = (genFetchReqPtr[tid] < pe_ifu_q->Size(tid)) ? genFetchReqPtr[tid] : pe_ifu_q->Size(tid);
    }
    for (uint32_t pipeId = PF0; pipeId < NSTAGE; ++pipeId) {
        auto& pi = pipe[pipeId];
        if (pi.state == INVALID) {
            pi.Flush();
            continue;
        }
        uint32_t flushNum = 0;
        for (uint32_t i = 0; i < configs.ifu_fetch_req_num; i++) {
            if (!pi.fetchReq[i]) {
                flushNum++;
                continue;
            }
            if (pi.fetchReq[i]->stid == flushReq.req.stid && LessEqual(flushReq.req.bid, pi.fetchReq[i]->bid)) {
                pi.fetchReq[i] = nullptr;
                flushNum++;
            }
        }
        if (flushNum == configs.ifu_fetch_req_num) {
            pi.state = INVALID;
            if (pipeId == PF2) {
                for (uint32_t tid : flushTid) {
                    merge[tid] = false;
                    mergeBin[tid] = 0;
                    mergeSize[tid] = 0;
                    mergeGid[tid] = ROBID();
                    mergeStid[tid] = -1U;
                }
            }
        }
    }
    for (auto &f2Wait : f2PipeWait) {
        if (f2Wait && f2Wait->stid == flushReq.req.stid && LessEqual(flushReq.req.bid, f2Wait->bid)) {
            f2Wait = nullptr;
        }
    }
    if (icache.stallPtr) {
        if (icache.stallPtr->stid == flushReq.req.stid && LessEqual(flushReq.req.bid, icache.stallPtr->bid)) {
            icache.stallPtr = nullptr;
        }
    }
}

void PEIFU::flushFront(ROBID fbid, bool flush, uint32_t tpc, uint32_t tid)
{
    LOG_INFO_M(top->machineType, Stage::F3) << ": flushFront bid:" << fbid << " flush:" << flush << " tpc:" << hex
        << tpc << " tid:" << tid;
    ASSERT(tid >= 0 && tid < top->GetThread());
    auto handleReq = [this, flush, tpc, fbid] (uint32_t i, uint32_t j, uint32_t& flushNum) {
        if (i != PF3) {
            bool equal = flush ? true : (fbid == pipe[i].fetchReq[j]->bid);
            if (pipe[i].state != INVALID && equal) {
                pipe[i].fetchReq[j] = nullptr;
                flushNum++;
            }
        } else {
            bool younger = (pipe[i].fetchReq[j]->bid > fbid);
            bool equal = (fbid == pipe[i].fetchReq[j]->bid);
            bool bigerTpc = (pipe[i].fetchReq[j]->tpc > tpc);
            bool setflush = flush ? (younger || (equal && bigerTpc)) : (equal && bigerTpc);
            if (pipe[i].state != INVALID && setflush) {
                pipe[i].fetchReq[j] = nullptr;
                flushNum++;
            }
        }
    };
    for (uint32_t i = PF3; i >= PF0 && i < NSTAGE; i--) {
        if (pipe[i].state == INVALID) {
            pipe[i].Flush();
            continue;
        }
        uint32_t flushNum = 0;
        for (uint32_t j = 0; j < configs.ifu_fetch_req_num; j++) {
            if (!pipe[i].fetchReq[j]) {
                flushNum++;
                continue;
            }
            handleReq(i, j, flushNum);
        }
        if (flushNum == configs.ifu_fetch_req_num) {
            pipe[i].state = INVALID;
        }
    }

    while (rahq.Size() > 0) {
        ASSERT(0);
        bool equal = flush ? true : fbid == rahq.Front()->bid;
        if (equal || !rahq.Front()) {
            rahq.Read();
        } else {
            break;
        }
    }

    if (flush) {
        auto req = pe_ifu_q->Front(tid);
        pe_ifu_q->Read(tid);
        if (genFetchReqPtr[tid] > 0) {
            genFetchReqPtr[tid]--;
        } else {
            flush = false;
        }
    }

    if (flush) {
        for (uint32_t i = 0; i <= genFetchReqPtr[tid]; i++) {
            if (i >= pe_ifu_q->Size(tid)) {
                break;
            }
            BlkBodyFetchState &blkState = pe_ifu_q->At(i, tid);
            blkState.fetchTPC = blkState.startTPC;
        }
        genFetchReqPtr[tid] = 0;
        merge[tid] = false;
        mergeBin[tid] = 0;
        mergeSize[tid] = 0;
        mergeGid[tid] = ROBID();
        mergeStid[tid] = -1U;
    }

    auto prefFlush = [this, &fbid] (FetchInfo &info) -> bool {
        return (info.bid == fbid);
    };
    for (uint32_t tid = 0; tid < top->GetThread(); ++tid) {
        pre_fetch_q.FlushIf(prefFlush, tid);
    }
}

void PEIFU::flushByLoad(FlushFrontInfo &flushInfo)
{
    auto matchBundle = [&flushInfo] (DecodeBundle &bundle) -> bool {
        if (!bundle.vld) {
            return false;
        } else {
            return flushInfo.bid == bundle.bid && flushInfo.gid == bundle.gid && flushInfo.stid == bundle.stid;
        }
    };
    auto matchCtrl = [&flushInfo] (BlkBodyFetchState control) -> bool {
        if (!control.vld) {
            return false;
        } else {
            return flushInfo.bid == control.bid && flushInfo.gid == control.gid && flushInfo.stid == control.stid;
        }
    };
    f4SimBuffer.FlushIf(matchBundle);
    f5SimBuffer.FlushIf(matchBundle);
    ifuDecThdQ->FlushIf(matchBundle);
    pe_ifu_q->FlushIf(matchCtrl);

    FlushGidFront(flushInfo, true);
}

void PEIFU::FlushGidFront(FlushFrontInfo &flushInfo, bool gidOnly)
{
    LOG_INFO_M(top->machineType, Stage::F3) << " FlushGidFront bid:" << flushInfo.bid
        << " flush:" << flushInfo.flush << " tpc:" << hex << flushInfo.tpc << " tid:" << flushInfo.tid;
    ROBID &fbid = flushInfo.bid;
    ROBID &fgid = flushInfo.gid;
    bool flush = flushInfo.flush;
    bool flushf3 = flushInfo.flushf3;
    uint64_t tpc = flushInfo.tpc;
    uint32_t tid = flushInfo.tid;
    uint64_t fid = flushInfo.fid;
    uint32_t stid = flushInfo.stid;
    ASSERT(tid >= 0 && tid < top->GetThread());
    auto flushTidGidMatch = [&gidOnly, &flushInfo] (uint32_t threadID, ROBID groupID, uint32_t stid) -> bool {
        return threadID == flushInfo.tid && stid == flushInfo.stid &&
                (!gidOnly || (gidOnly && groupID == flushInfo.gid));
    };

    auto handleReq = [this, flush, flushf3, tpc, &fbid, &fgid, fid, stid] (uint32_t i, uint32_t j, uint32_t& flushNum) {
        if (i != PF3 || flushf3) {
            bool equal = (fbid == pipe[i].fetchReq[j]->bid) && (fgid == pipe[i].fetchReq[j]->gid) &&
                         (stid == pipe[i].fetchReq[j]->stid);
            if (pipe[i].state != INVALID && equal) {
                pipe[i].fetchReq[j] = nullptr;
                flushNum++;
            }
        } else {
            bool equal = ((fbid == pipe[i].fetchReq[j]->bid) && (fgid == pipe[i].fetchReq[j]->gid) &&
                          (stid == pipe[i].fetchReq[j]->stid));
            bool bigerFid = (pipe[i].fetchReq[j]->fid > fid);
            if (pipe[i].state != INVALID && equal && bigerFid) {
                pipe[i].fetchReq[j] = nullptr;
                flushNum++;
            }
        }
    };
    for (uint32_t i = PF3; i >= PF0 && i < NSTAGE; i--) {
        if (pipe[i].state == INVALID) {
            pipe[i].Flush();
            continue;
        }
        uint32_t flushNum = 0;
        for (uint32_t j = 0; j < configs.ifu_fetch_req_num; j++) {
            if (!pipe[i].fetchReq[j]) {
                flushNum++;
                continue;
            }
            handleReq(i, j, flushNum);
        }
        if (flushNum == configs.ifu_fetch_req_num) {
            pipe[i].state = INVALID;
        }
    }

    for (uint32_t i = 0; i < f2PipeWait.size(); ++i) {
        if (f2PipeWait[i] != nullptr && flushTidGidMatch(i, f2PipeWait[i]->gid, f2PipeWait[i]->stid)) {
            f2PipeWait[i] = nullptr;
        }
    }

    auto rahqFlush = [this, &flushInfo] (FetchReqPtr &fetchReq) -> bool {
        return fetchReq && fetchReq->bid == flushInfo.bid && fetchReq->gid == flushInfo.gid &&
               fetchReq->stid == flushInfo.stid;
    };
    rahq.FlushIf(rahqFlush);

    // todo: stid
    if (flushTidGidMatch(tid, mergeGid[tid], mergeStid[tid])) {
        merge[tid] = false;
        mergeBin[tid] = 0;
        mergeSize[tid] = 0;
        mergeGid[tid] = ROBID();
        mergeStid[tid] = -1U;
    }

    if (flush) {
        while (!pe_ifu_q->Empty(tid)) {
            BlkBodyFetchState blkState = pe_ifu_q->Front(tid);
            if ((fbid != blkState.bid) || (fgid != blkState.gid) || (stid != blkState.stid)) {
                break;
            }
            pe_ifu_q->Read(tid);
        }

        if (genFetchReqPtr[tid] > 0) {
            genFetchReqPtr[tid]--;
        } else {
            flush = false;
        }
    }

    if (flush) {
        for (uint32_t i = 0; i <= genFetchReqPtr[tid]; i++) {
            if (i >= pe_ifu_q->Size(tid)) {
                break;
            }
            BlkBodyFetchState &blkState = pe_ifu_q->At(i, tid);
            ASSERT(blkState.stid == stid);
            blkState.fetchTPC = blkState.startTPC;
        }
        genFetchReqPtr[tid] = 0;
    }

    auto prefFlush = [this, &flushInfo] (FetchInfo &info) -> bool {
        return (info.bid == flushInfo.bid) && (info.stid == flushInfo.stid);
    };
    for (uint32_t tid = 0; tid < top->GetThread(); ++tid) {
        pre_fetch_q.FlushIf(prefFlush, tid);
    }
}

void PEIFU::flushGidFrontByMiss(uint32_t tid, uint32_t stid)
{
    ASSERT(tid >= 0 && tid < top->GetThread());
    auto handleReq = [this, tid, stid] (uint32_t i, uint32_t j, uint32_t& flushNum) {
        if (pipe[i].state != INVALID && pipe[i].fetchReq[j]->m_threadId == tid && pipe[i].fetchReq[j]->stid == stid) {
            pipe[i].fetchReq[j] = nullptr;
            flushNum++;
        }
    };

    for (uint32_t i = PF1; i >= PF0 && i < NSTAGE; i--) {
        if (pipe[i].state == INVALID) {
            pipe[i].Flush();
            continue;
        }

        uint32_t flushNum = 0;
        for (uint32_t j = 0; j < configs.ifu_fetch_req_num; j++) {
            if (!pipe[i].fetchReq[j]) {
                flushNum++;
                continue;
            }
            handleReq(i, j, flushNum);
        }

        if (flushNum == configs.ifu_fetch_req_num) {
            pipe[i].state = INVALID;
        }
    }

    auto rahqFlush = [this, &tid, &stid] (FetchReqPtr &fetchReq) -> bool {
        if (!(fetchReq && fetchReq->m_threadId == tid && fetchReq->stid == stid)) {
            return false;
        }
        return true;
    };
    rahq.FlushIf(rahqFlush);
}

void PEIFU::releaseFeCtrlQ(ROBID bid, uint32_t tid, uint32_t stid)
{
    BlkBodyFetchState blkState = pe_ifu_q->Front(tid);
    if (blkState.bid != bid || blkState.stid != stid) {
        cout<<"Cycle "<<dec<<GetSim()->getCycles()<<" correctBCount "<<dec<<GetSim()->correctBCount<<endl;
        cout<<"thread id " << tid << " blkstate bid "<<dec<<blkState.bid.val<<" releaseFeCtrlQ bid " <<bid.val<<endl;
    }
    ASSERT(blkState.bid == bid && blkState.stid == stid && "Error update IFU fetechControlQ");
    pe_ifu_q->Read(tid);
    genFetchReqPtr[tid]--;

    while (genFetchReqPtr[tid] > 0) {
        BlkBodyFetchState blkState = pe_ifu_q->Front(tid);
        if (!blkState.vld) {
            break;
        }
        if (blkState.stid != stid) {
            break;
        }
        if (blkState.endTPC != INST_MAX_PC) {
            pe_ifu_q->Read(tid);
            genFetchReqPtr[tid]--;
        } else {
            break;
        }
    }
}

SimSys *PEIFU::GetSim()
{
    return sim;
}

void PEIFU::ReportVec(const std::string& name)
{
    stringstream s;
    s << name << " Detail Stats(IFU)";
    stats->ReportVec(s.str());
}

void PEIFU::ReportStat()
{
    stringstream s = stringstream();
    s << "IFU " << ifuID;
    stats->report(s.str());
}

bool PEIFU::IsIfuIdle()
{
    if (rahq.Size() > 0) {
        return false;
    }
    for (uint32_t tid = 0; tid < configs.ifu_thread_num; ++tid) {
        if (pe_ifu_q->Size(tid) > 0 || f4SimBuffer.Size() || f5SimBuffer.Size() || ifuDecThdQ->Size(tid) > 0) {
            return false;
        }
    }
    for (auto &pi : pipe) {
        if (pi.state != INVALID) {
            return false;
        }
    }
    return true;
}

void PEIFU::RunPerfIfu()
{
    auto getNonEmptyInputQTid = [this]()->bool {
        for (uint32_t tid = 0; tid < configs.ifu_thread_num; ++tid) {
            if (!pe_ifu_q->Empty(tid)) {
                return tid;
            }
        }
        return configs.ifu_thread_num;
    };

    auto tid = getNonEmptyInputQTid();
    if ((tid != configs.ifu_thread_num) || pipe[PF3].state == STALL) {
        if (pipe[PF3].state != STALL) {
            pipe[PF3].state = VALID;
            GenPerfFetchReq(tid);
        }
        InsertPerfToIb();
        if (pipe[PF3].state == STALL) {
            return;
        }
    }
}

void PEIFU::RunAtPre()
{
    RecvInstPkt();
}

void PEIFU::RunF3()
{
    if (ifustall) {
        return;
    }

    auto state = pipe[PF3].state;
    if (state != VALID) {
        return;
    }
    for (uint32_t i = 0; i < configs.ifu_fetch_req_num; i++) {
        auto& fetchReq = pipe[PF3].fetchReq[i];
        if (!fetchReq) {
            continue;
        }
        sp.Work(fetchReq);
    }
}

void PEIFU::RunF2()
{
    if (ifustall) {
        return;
    }

    for (uint32_t tid = 0; tid < logicalGroups; ++tid) {
        HandlePreFetch(tid);
    }

    auto handleMiss = [this] (FetchReqPtr& fetchReq) {
        if (icache.CanSendReq() && !configs.icache_perfect) {
            if (!icache.lookUpMissQ(fetchReq->tpc)) {
                if (!icache.insertMissQ(fetchReq->tpc, false)) {
                    top->PEBase::SetBlockException(fetchReq->bid, fetchReq->stid, "IFU F2 insert error");
                }
            }
            icache.stallPtr = fetchReq;
            fetchReq->status = ICACHE_WAIT;
        } else {
            fetchReq->status = ICACHE_MISS;
        }
    };
    auto state = pipe[PF2].state;
    uint32_t fetchWidth = configs.ifu_fetch_width;
    for (auto &f2wait : f2PipeWait) {
        if (f2wait && f2wait->status ==  ICACHE_MISS) {
            handleMiss(f2wait);
        }
    }
    if (state == INVALID) {
        HandleRahqReq();
        return;
    }
    for (uint32_t i = 0; i < configs.ifu_fetch_req_num; i++) {
        auto& fetchReq = pipe[PF2].fetchReq[i];
        if (!fetchReq) {
            continue;
        }

        if (fetchReq->status == ICACHE_HIT) {
            if (merge[fetchReq->m_threadId]) {
                fetchReq->merge = true;
                fetchReq->mergeBin = mergeBin[fetchReq->m_threadId];
                fetchReq->mergeSize = mergeSize[fetchReq->m_threadId];
                merge[fetchReq->m_threadId] = false;
                mergeBin[fetchReq->m_threadId] = 0;
                mergeSize[fetchReq->m_threadId] = 0;
                mergeGid[fetchReq->m_threadId] = ROBID();
                mergeStid[fetchReq->m_threadId] = -1U;
            }

            bool check = (fetchReq->instCnt == 0);
            icache.getCacheData(fetchReq, fetchWidth);
            ASSERT(!pipe[PF2].pipeDone);
            ASSERT(check);
            pipe[PF2].pipeDone = true;
            // Last inst need to merge.
            if (fetchReq->merge) {
                merge[fetchReq->m_threadId] = true;
                mergeBin[fetchReq->m_threadId] = fetchReq->mergeBin;
                mergeSize[fetchReq->m_threadId] = fetchReq->mergeSize;
                mergeGid[fetchReq->m_threadId] = fetchReq->gid;
                mergeStid[fetchReq->m_threadId] = fetchReq->stid;
                fetchReq->merge = false;
                fetchReq->mergeBin = 0;
                fetchReq->mergeSize = 0;
            }
            fetchReq->status = ICACHE_GET;
            LOG_INFO_M(top->machineType, Stage::F2) << "Get FetchData of ICache at BPC 0x" << hex << fetchReq->bpc
                <<", TPC 0x"<<fetchReq->tpc<<", mask "<<dec<<fetchReq->mask;
            stats->l1_total_lat += (GetSim()->getCycles() - fetchReq->pipe_cycle->f1Cycle);
            fetchReq->pipe_cycle->f2Cycle = GetSim()->getCycles();
        } else if (fetchReq->status == ICACHE_MISS) {
            // process
            handleMiss(fetchReq);
            if (configs.ifu_thread_fetch_enable) {
                MoveF2Wait(fetchReq);
                pipe[PF2].state = INVALID;
            }
        } else {
            if (fetchReq && fetchReq->status == ICACHE_WAIT) {
                icache.stallPtr = fetchReq;
            }
            HandleRahqReq();
        }
    }
}

void PEIFU::MoveF2Wait(FetchReqPtr &fetchReq)
{
    ASSERT(!f2PipeWait[fetchReq->m_threadId]);

    flushGidFrontByMiss(fetchReq->m_threadId, fetchReq->stid);
    WaitForFetch(fetchReq->m_threadId);
    f2PipeWait[fetchReq->m_threadId] = std::move(fetchReq);
}

void PEIFU::RestoreF2()
{
    if (pipe[PF2].state == STALL) {
        return;
    }

    for (uint32_t i = 0; i < configs.ifu_fetch_req_num; i++) {
        for (uint32_t tid = 0; tid < f2PipeWait.size(); ++tid) {
            if (!f2PipeWait[tid] || f2PipeWait[tid]->status != ICACHE_HIT) {
                continue;
            }

            uint64_t startAddr = utils->calcAddr(f2PipeWait[tid]->tpc);
            uint64_t fetchTpc = startAddr + configs.icache_way_size;
            ContinueFetch(tid, fetchTpc, f2PipeWait[tid]->bid, false, f2PipeWait[tid]->stid);
            pipe[PF2].state = STALL;
            pipe[PF2].pipeDone = false;
            pipe[PF2].fetchReq[i] = std::move(f2PipeWait[tid]);
            break;
        }
    }
}

void PEIFU::RunF1()
{
    if (ifustall) {
        return;
    }

    auto handleIdle = [this] (FetchReqPtr& fetchReq, uint32_t& fetchWidth) {
        fetchReq->EnableFetchData(configs.ifu_fetch_width / WIDTH_16);
        CheckPrefQFront(fetchReq->tpc, fetchReq->m_threadId);
        if (icache.lookupTag(utils->calcAddr(fetchReq->tpc))) {
            fetchReq->status = ICACHE_HIT;
            LOG_INFO_M(top->machineType, Stage::F1) << ": ICache Hit at " << *fetchReq;
        } else {
            fetchReq->status = ICACHE_MISS;
            LOG_INFO_M(top->machineType, Stage::NA) << ": ICache Miss at " << *fetchReq;
        }
    };
    auto state = pipe[PF1].state;
    if (state == INVALID) {
        LookUpRahqReq();
        return;
    }
    for (uint32_t i = 0; i < configs.ifu_fetch_req_num; i++) {
        if (!pipe[PF1].fetchReq[i]) {
            continue;
        }
        if (pipe[PF1].pipeDone) {
            LookUpRahqReq();
            continue;
        }
        auto& fetchReq = pipe[PF1].fetchReq[i];
        uint32_t fetchWidth = configs.ifu_fetch_width;

        if (configs.icache_perfect) {
            fetchReq->EnableFetchData(configs.ifu_fetch_width / WIDTH_16);
            fetchReq->status = ICACHE_HIT;
        } else if (fetchReq->status == ICACHE_IDEL) {
            handleIdle(fetchReq, fetchWidth);
        } else {
            LookUpRahqReq();
        }
        if (fetchReq && fetchReq->pipe_cycle->f1Cycle == 0) {
            fetchReq->pipe_cycle->f1Cycle = GetSim()->getCycles();
        }
    }
}

void PEIFU::RunF0()
{
    if (ifustall) {
        return;
    }

    if (!rahq.Full()) {
        GenFetchReq();
    }

    if (pipe[PF0].state != INVALID) {
        return;
    }
    for (uint32_t i = 0; i < configs.ifu_fetch_req_num; ++i) {
        while (!rahq.Empty() && !rahq.Front()) {
            rahq.Read();
        }
        if (rahq.Empty()) {
            break;
        }

        pipe[PF0].state = VALID;
        pipe[PF0].fetchReq[i] = rahq.Read();

        if (pipe[PF0].fetchReq[i]->first &&
            pipe[PF0].fetchReq[i]->blkSrcType != BlkSrcType::PARALLEL_FETCH) {
            top->PEBase::SetBlockRunning(pipe[PF0].fetchReq[i]->bid, BlockStatus::RUNNING, pipe[PF0].fetchReq[i]->stid);
        }
    }
}

void PEIFU::PipeCtrl()
{
    f5SimBuffer.unsetStall();
    if (!f5SimBuffer.Empty() && ifuDecThdQ->Full(f5SimBuffer.Front().tid)) {
        f5SimBuffer.setStall();
    }

    f4SimBuffer.unsetStall();
    if (!f4SimBuffer.Empty() && f5SimBuffer.getStall()) {
        f4SimBuffer.setStall();
    }

    if (pipe[PF3].state != INVALID) {
        // InstBuffer stall.
        uint32_t bundleSizeSum = 0;
        for (uint32_t i = 0; i < configs.ifu_fetch_req_num; i++) {
            if (!pipe[PF3].fetchReq[i]) {
                continue;
            }
            if (!pipe[PF3].fetchReq[i]->merge) {
                bundleSizeSum += pipe[PF3].fetchReq[i]->instCnt;
            }
        }

        if (f4SimBuffer.Size() + bundleSizeSum > configs.ibuf_depth || f4SimBuffer.getStall()) {
            pipe[PF3].state = STALL;
        } else {
            pipe[PF3].state = VALID;
        }
    }

    vector<bool> hasReqOfPipe(NSTAGE, false);
    uint32_t pipeid = 0;
    for (auto &pi : pipe) {
        bool hasReq = false;
        for (uint32_t i = 0; i < configs.ifu_fetch_req_num; i++) {
            hasReq |= (pi.fetchReq[i] != nullptr);
        }
        hasReqOfPipe[pipeid] = hasReq;
        pipeid++;
    }
    if (icache.GetDataStall()) {
        pipe[PF2].state = hasReqOfPipe[PF2] ? STALL : INVALID;
    } else if (pipe[PF2].state == STALL) {
        pipe[PF2].state = hasReqOfPipe[PF2] ? VALID : INVALID;
    }

    if (icache.LookupStall()) {
        pipe[PF1].state = hasReqOfPipe[PF2] ? STALL : INVALID;
        pipe[PF0].state = hasReqOfPipe[PF2] ? STALL : INVALID; // TODO: Move to separate
    } else if (pipe[PF1].state == STALL) {
        pipe[PF1].state = hasReqOfPipe[PF1] ? VALID : INVALID;
        pipe[PF0].state = hasReqOfPipe[PF0] ? VALID : INVALID;
    } else if (pipe[PF0].state == STALL) {
        pipe[PF0].state = VALID;
    }
}

void PEIFU::MoveOneStage(Pipe& src, Pipe& dst)
{
    if (dst.state != STALL) {
        if (src.state == STALL) {
            dst = Pipe(configs.ifu_fetch_req_num);
        } else {
            src.pipeDone = false;
            dst = src;
            src = Pipe(configs.ifu_fetch_req_num);
        }
    } else {
        if (src.state != INVALID) {
            src.state = STALL;
        }
    }
}

void PEIFU::PipeMove()
{
    InsertToIB();
    InsertToF5();
    if (pipe[PF3].state == VALID) {
        for (uint32_t i = 0; i < configs.ifu_fetch_req_num; i++) {
            if (!pipe[PF3].fetchReq[i] || pipe[PF3].fetchReq[i]->instCnt == 0) {
                continue;
            }
            InsertToF4(pipe[PF3].fetchReq[i]);
        }
    }
    MoveOneStage(pipe[PF2], pipe[PF3]);
    RestoreF2();
    MoveOneStage(pipe[PF1], pipe[PF2]);
    MoveOneStage(pipe[PF0], pipe[PF1]);
    if (pipe[PF0].state != STALL) {
        pipe[PF0] = Pipe(configs.ifu_fetch_req_num);
    }
}

void PEIFU::setPrefetchQ(uint64_t tpc, uint32_t bsize, uint32_t instSize, ROBID &bid, uint32_t tid, uint32_t stid)
{
    if (!configs.icache_pref_enable || configs.icache_perfect) {
        return;
    }
    if (bsize == 0) {
        return;
    }
    if (bsize == NUM8) {
        pre_fetch_q.Reset(tid);
    }
    CheckPrefQFull(tid);
    uint64_t startAddr = utils->calcAddr(tpc);
    uint64_t endAddr = utils->calcAddr((tpc + instSize * (bsize - 1)));
    for (uint64_t addr = startAddr; addr <= endAddr; addr = addr + configs.icache_way_size) {
        FetchInfo info;
        info.bid = bid;
        info.addr = addr;
        info.stid = stid;
        pre_fetch_q.Write(info, tid);
    }
}

void PEIFU::setPrefetchQ(uint64_t tpc, uint32_t nBytes, ROBID &bid, uint32_t tid, uint32_t stid)
{
    if (!configs.icache_pref_enable || configs.icache_perfect) {
        return;
    }

    CheckPrefQFull(nBytes / configs.icache_way_size, tid);
    uint64_t startAddr = utils->calcAddr(tpc);
    uint64_t endAddr = utils->calcAddr(startAddr + nBytes);

    for (uint64_t addr = startAddr; addr < endAddr; addr = addr + configs.icache_way_size) {
        LOG_INFO_M(top->machineType, Stage::F1) << "Set Prefetch 0x" << hex << addr;
        FetchInfo info;
        info.bid = bid;
        info.addr = addr;
        info.stid = stid;
        pre_fetch_q.Write(info, tid);
    }
}

void PEIFU::HandlePreFetch(uint32_t tid)
{
    for (uint32_t i = 0; !pre_fetch_q.Empty(tid) && !icache.GetDataStall() && i < configs.ifu_pref_num;++i) {
        FetchInfo info = pre_fetch_q.Front(tid);
        uint64_t preTag = info.addr;
        if (!icache.lookupTag(preTag)) {
            if (!icache.CanSendPref()) {
                break;
            }

            if (!icache.lookUpMissQ(preTag)) {
                icache.insertMissQ(preTag, true);
                LOG_INFO_M(top->machineType, Stage::F1) << ": Insert MissQ. Prefetch 0x"<<hex<<preTag;
            }
        }
        pre_fetch_q.Read(tid);
        const uint32_t nextLineOffset = configs.icache_way_size * configs.ifu_pref_num;
        const uint32_t prefSize = configs.icache_way_size;
        setPrefetchQ(preTag + nextLineOffset, prefSize, info.bid, tid, info.stid);
    }
}

void PEIFU::CheckPrefQFull(uint32_t tid)
{
    if (pre_fetch_q.Size(tid) < configs.icache_prefq_depth - NUM4) {
        return;
    }
    const uint32_t num = 2;
    for (uint64_t i = 0; i < uint64_t(configs.icache_prefq_depth / num); i++) {
        pre_fetch_q.Read(tid);
    }
}

void PEIFU::CheckPrefQFull(uint32_t size, uint32_t tid)
{
    if (pre_fetch_q.Size(tid) + size <= configs.icache_prefq_depth) {
        return;
    }

    ASSERT(0);
    uint32_t popSize = pre_fetch_q.Size(tid) + size - configs.icache_prefq_depth;
    for (uint64_t i = 0; i < popSize; i++) {
        pre_fetch_q.Read(tid);
    }
}

void PEIFU::CheckPrefQFront(uint64_t tpc, uint32_t tid)
{
    if (pre_fetch_q.Size(tid) == 0) {
        return;
    }
    uint64_t addr = utils->calcAddr(tpc);
    if (pre_fetch_q.Front(tid).addr == addr) {
        pre_fetch_q.Read(tid);
    }
}

void PEIFU::RecvInstPkt()
{
    l2_ifu_q->Work();
    if (l2_ifu_q->Empty()) {
        return;
    }
    PtrPacket pkt = l2_ifu_q->Read();
    L1Entry entry;
    PtrPacket resp;
    const uint32_t val = 2;
    const uint32_t val64 = 64;
    if (pkt->isInv()) {
        entry = icache.invd(pkt->addr);
        stats->l2_invalid++;
        sleepHitReq(pkt->addr);
        if (entry.state == CS_INV) {
            return;
        }
        resp = Packet::createInvResp((ifuID << val) + 1);
    } else if (pkt->isGetS()) {
        entry = icache.getS(pkt->addr);
        if (entry.state == CS_INV) {
            return;
        }
        resp = Packet::createGetSResp((ifuID << val) + 1);
    } else if (pkt->isRead()) {
        icache.releaseMissQ(pkt->addr, pkt->hasPref());
        if (pkt->isDup()) {
            return;
        }
        entry = icache.refill(pkt->addr, pkt->data.bits);
        wakeUpMissReq(pkt->addr);

        if (entry.state != CS_INV) {
            sleepHitReq(entry.addr);
        }
        if (entry.state != CS_INV && configs.l1i_l2_inclusion_policy != "WI") {
            resp = Packet::createWBPkt(false);
        } else {
            return;
        }
    } else if (pkt->isUpgrade()) {
        icache.upgrade(pkt->addr);
        return;
    }

    if (entry.addr != 0) {
        resp->tid = (ifuID << val) + 1;
        resp->id = memID;
        resp->addr = entry.addr;
        resp->size = val64;
        resp->data = entry.data;
        sendInstPkt(resp, resp->isResp());
    }
}

void PEIFU::wakeUpMissReq(uint64_t addr)
{
    addr = utils->calcAddr(addr);
    for (uint32_t i = PF2; i >= PF0 && i < NSTAGE; i--) {
        PipeState state = pipe[i].state;
        if (state == INVALID) {
            continue;
        }
        for (uint32_t j = 0; j < configs.ifu_fetch_req_num; j++) {
            FetchReqPtr fetchReq = pipe[i].fetchReq[j];
            if (!fetchReq || utils->calcAddr(fetchReq->tpc) != addr) {
                continue;
            }
            if (fetchReq->status == ICACHE_WAIT || fetchReq->status == ICACHE_MISS) {
                fetchReq->status = ICACHE_HIT;
                fetchReq->EnableFetchData(configs.ifu_fetch_width / WIDTH_16);
            }
        }
    }

    for (uint32_t i = 0; i < rahq.Size(); i++) {
        FetchReqPtr fetchReq = rahq[i];
        if (!fetchReq) {
            continue;
        }
        if (utils->calcAddr(fetchReq->tpc) != addr) {
            continue;
        }
        if (fetchReq->status == ICACHE_WAIT || fetchReq->status == ICACHE_MISS) {
            fetchReq->status = ICACHE_HIT;
            fetchReq->EnableFetchData(configs.ifu_fetch_width / WIDTH_16);
        }
    }

    if (configs.ifu_thread_fetch_enable) {
        for (uint32_t tid = 0; tid < f2PipeWait.size(); ++tid) {
            if (!f2PipeWait[tid] || utils->calcAddr(f2PipeWait[tid]->tpc) != addr) {
                continue;
            }

            f2PipeWait[tid]->status = ICACHE_HIT;
            f2PipeWait[tid]->EnableFetchData(configs.ifu_fetch_width / WIDTH_16);
        }
    }
}

void PEIFU::sleepHitReq(uint64_t addr)
{
    addr = utils->calcAddr(addr);
    for (uint32_t i = PF2; i >= PF0 && i < NSTAGE; i--) {
        PipeState state = pipe[i].state;
        if (state == INVALID) {
            continue;
        }
        for (uint32_t j = 0; j < configs.ifu_fetch_req_num; j++) {
            if (!pipe[i].fetchReq[j]) {
                continue;
            }
            FetchReqPtr fetchReq = pipe[i].fetchReq[j];
            if (utils->calcAddr(fetchReq->tpc) != addr || fetchReq->status == ICACHE_GET) {
                continue;
            }

            fetchReq->status = ICACHE_MISS;
            fetchReq->EnableFetchData(configs.ifu_fetch_width / WIDTH_16);
        }
    }

    for (uint32_t i = 0; i < rahq.Size(); i++) {
        FetchReqPtr fetchReq = rahq[i];
        if (!fetchReq) {
            continue;
        }
        if (utils->calcAddr(fetchReq->tpc) == addr && fetchReq->status != ICACHE_GET) {
            fetchReq->status = ICACHE_MISS;
            fetchReq->EnableFetchData(configs.ifu_fetch_width / WIDTH_16);
        }
    }

    if (configs.ifu_thread_fetch_enable) {
        for (uint32_t tid = 0; tid < f2PipeWait.size(); ++tid) {
            if (!f2PipeWait[tid] || utils->calcAddr(f2PipeWait[tid]->tpc) != addr) {
                continue;
            }

            if (f2PipeWait[tid]->status != ICACHE_GET) {
                f2PipeWait[tid]->status = ICACHE_MISS;
                f2PipeWait[tid]->EnableFetchData(configs.ifu_fetch_width / WIDTH_16);
            }
        }
    }
}

void PEIFU::sendInstPkt(PtrPacket pkt, bool resp)
{
    pkt->l1_out_cycle = GetSim()->getCycles();
    pkt->peId = top->peID;

    if (resp) {
        snp_l2_q->Write(pkt);
    } else {
        ifu_l2_q->Write(pkt);
    }
}

bool PEIFU::CheckFetchReq(uint32_t pickThread)
{
    if (pipe[PF3].state == STALL && pipe[PF3].fetchReq[0]->m_threadId == pickThread) {
        return false;
    }
    if (pe_ifu_q->Size(pickThread) == 0 ||
        genFetchReqPtr[pickThread] >= pe_ifu_q->Size(pickThread)) {
        return false;
    }
    if (rahq.Size() >= configs.ifu_buffer_depth) {
        return false;
    }
    BlkBodyFetchState &blkState = pe_ifu_q->At(genFetchReqPtr[pickThread], pickThread);
    if (blkState.waitForBranch) {
        return false;
    }
    return true;
}

bool PEIFU::PickThread(uint32_t &pickThread)
{
    for (uint32_t i = 0; i < logicalGroups; ++i) {
        if (CheckFetchReq(m_curF0ThreadId)) {
            pickThread = m_curF0ThreadId;
            m_curF0ThreadId = (m_curF0ThreadId + 1) % logicalGroups;
            return true;
        }
        m_curF0ThreadId = (m_curF0ThreadId + 1) % logicalGroups;
    }

    return false;
}

bool PEIFU::IsInPrefMap(uint64_t tpc)
{
    if (hasPrefMap.Find(tpc)) {
        return true;
    }

    return false;
}

void PEIFU::InsertPrefMap(uint64_t tpc)
{
    ASSERT(!IsInPrefMap(tpc));
    if (hasPrefMap.Full()) {
        hasPrefMap.Read();
    }

    hasPrefMap.Write(tpc);
}

void PEIFU::UpdatePrefMap(uint64_t tpc)
{
    RelasePrefMap(tpc);
    hasPrefMap.Write(tpc);
}

void PEIFU::RelasePrefMap(uint64_t tpc)
{
    auto matchPop = [tpc](uint64_t pc) {
        return pc == tpc;
    };

    hasPrefMap.FlushIf(matchPop);
}

void PEIFU::GenFetchReq()
{
    uint32_t threadId = 0;
    if (!PickThread(threadId)) {
        return;
    }

    uint64_t fetchWidth = configs.ifu_fetch_width;
    uint64_t cacheWaySize = configs.icache_way_size;
    uint64_t leftCacheLine = configs.icache_way_size;
    uint64_t leftOver = INST_MAX_PC;
    BlkBodyFetchState &blkState = pe_ifu_q->At(genFetchReqPtr[threadId], threadId);
    if (!blkState.vld) {
        pe_ifu_q->Read(threadId);
        return;
    }
    FetchReqPtr fetchReq = std::make_shared<FetchReqBus>();
    static uint64_t fid = 0;
    bool popFeCtrlQ = false;

    fetchReq->vld = true;
    fetchReq->merge = false;
    fetchReq->tpc = blkState.fetchTPC;
    fetchReq->bpc = blkState.bpc;
    fetchReq->bid = blkState.bid;
    fetchReq->gid = blkState.gid;
    fetchReq->stid = blkState.stid;
    fetchReq->logicalGID = blkState.logicalGID;
    fetchReq->shapelpinfo = blkState.shapelpinfo;
    fetchReq->fid = fid++;
    fetchReq->hsize = blkState.hsize;
    fetchReq->blockNoBranch = blkState.noBranch;
    fetchReq->bundleNoBranch = false;
    fetchReq->first = blkState.first;
    blkState.first = false;
    fetchReq->last = false;
    fetchReq->blkSrcType = blkState.blkSrcType;
    fetchReq->biqType = blkState.biqType;
    fetchReq->endTPC = blkState.endTPC;
    fetchReq->m_threadId = threadId;

    auto genFetchBranchAndBEnd = [this, &popFeCtrlQ, &leftOver, &fetchWidth](BlkBodyFetchState& blkState,
                                                                             FetchReqPtr const& fetchReq) {
        if (blkState.fetchTPC == blkState.endTPC) {
            popFeCtrlQ = (genFetchReqPtr[fetchReq->m_threadId] == 0) ? true : false;
            fetchReq->last = true;
        }
        if (leftOver <= fetchWidth) {
            popFeCtrlQ = (genFetchReqPtr[fetchReq->m_threadId] == 0) ? true : false;
            fetchReq->last = true;
            fetchReq->bundleNoBranch = true;
        } else {
            fetchReq->last = false;
            fetchReq->bundleNoBranch = true;
        }
    };

    if (!blkState.noBranch || blkState.endTPC == INST_MAX_PC) {
        // Predict inner branch and bend instructions
        if ((blkState.endTPC < blkState.fetchTPC)) {
            top->PEBase::SetBlockException(blkState.bid, blkState.stid, "IFU jump to fetch TPC error");
            LOG_ERROR_M(top->machineType, Stage::NA) << "Report block exception when"
                << " jumping to exceptional TPC, BID " << dec << blkState.bid.val <<" BPC 0x" <<hex<<blkState.bpc
                << " start TPC 0x" << hex << blkState.startTPC << " end TPC 0x" << hex << blkState.endTPC
                << " fetch TPC 0x" << hex << blkState.fetchTPC;
            return;
        }

        leftCacheLine = cacheWaySize - (blkState.fetchTPC & (cacheWaySize - 1)); // 8
        fetchWidth = (leftCacheLine > fetchWidth) ? fetchWidth : leftCacheLine;
        leftOver = blkState.endTPC - blkState.fetchTPC;
        fetchReq->mask = (leftOver > fetchWidth) ? fetchWidth : leftOver;
        genFetchBranchAndBEnd(blkState, fetchReq);
        blkState.fetchTPC += fetchWidth;
    } else {
        ASSERT("Error: Unexpect Block Run State.\n");
    }
    LOG_INFO_M(top->machineType, Stage::F0) << "Generate thread " << threadId
        << " fetch request: " << *fetchReq
        << " start tpc: 0x" << hex << blkState.startTPC << " fetchTPC: 0x" << blkState.fetchTPC
        << " fetchWidth: " << dec << fetchWidth << " leftOver: " << dec << leftOver
        << " fetchReq->mask: " << fetchReq->mask;
    if (fetchReq->last) {
        if (popFeCtrlQ) {
            pe_ifu_q->Read(threadId);
            genFetchReqPtr[threadId] = 0;
        } else {
            genFetchReqPtr[threadId]++;
        }
        if (fetchReq->mask < fetchWidth) {
            ASSERT(0 && "no merge");
            mergeFetchReq(fetchReq, fetchWidth);
        }
    }
    fetchReq->pipe_cycle->f0Cycle = GetSim()->getCycles();
    rahq.Write(fetchReq);

    if (fetchReq->first) {
        if (IsInPrefMap(fetchReq->tpc)) {
            UpdatePrefMap(fetchReq->tpc);
        } else {
            const uint32_t nextLineOffset = 0;
            const uint32_t prefSize = 128;
            setPrefetchQ(fetchReq->tpc + nextLineOffset, prefSize, fetchReq->bid, threadId, fetchReq->stid);
            InsertPrefMap(fetchReq->tpc);
        }
    }
}

void PEIFU::WaitForFetch(uint32_t tid)
{
    BlkBodyFetchState &blkState = pe_ifu_q->At(genFetchReqPtr[tid], tid);
    blkState.waitForBranch = true;
}

void PEIFU::ContinueFetch(uint32_t tid, uint64_t fetchTpc, ROBID &bid, bool pref, uint32_t stid)
{
    BlkBodyFetchState &blkState = pe_ifu_q->At(genFetchReqPtr[tid], tid);
    blkState.waitForBranch = false;
    blkState.fetchTPC = fetchTpc;
    ASSERT(blkState.stid == stid);
    ASSERT(blkState.bid == bid);

    if (pref) {
        const uint32_t nextLineOffset = 0;
        const uint32_t prefSize = 128;
        setPrefetchQ(fetchTpc + nextLineOffset, prefSize, bid, tid, stid);
    }
}

void PEIFU::mergeFetchReq(FetchReqPtr const& fetchReq, uint32_t fetchWidth)
{
    uint32_t threadId = fetchReq->m_threadId;
    if (!configs.enable_merge) {
        return;
    }
    if (pe_ifu_q->Size(threadId) == 0 || genFetchReqPtr[threadId] >= pe_ifu_q->Size(threadId)) {
        return;
    }
    BlkBodyFetchState &blkState = pe_ifu_q->At(genFetchReqPtr[threadId], threadId);
    if (!blkState.noBranch || blkState.endTPC == INST_MAX_PC) {
        return;
    }
    if (blkState.fetchTPC != (fetchReq->tpc + fetchReq->mask) ||
        fetchReq->endTPC == INST_MAX_PC || blkState.fetchTPC == blkState.endTPC) {
        return;
    }
    bool popFeCtrlQ = false;
    uint32_t leftOver;
    fetchReq->merge = true;
    fetchReq->mask1 = fetchReq->mask;
    fetchWidth = fetchWidth - fetchReq->mask;
    fetchReq->nextBid = blkState.bid;
    fetchReq->nextBPC = blkState.bpc;
    fetchReq->nextLast = false;
    fetchReq->nextBlkSrcType = blkState.blkSrcType;
    fetchReq->nextEndTPC = blkState.endTPC;

    leftOver = blkState.endTPC - blkState.fetchTPC;
    fetchReq->mask = fetchReq->mask1 + ((leftOver > fetchWidth) ? fetchWidth : leftOver);
    if (leftOver <= fetchWidth) {
        popFeCtrlQ = (genFetchReqPtr[threadId] == 0) ? true : false;
        fetchReq->nextLast = true;
    } else {
        blkState.fetchTPC += fetchWidth;
        fetchReq->nextLast = false;
    }
    if (fetchReq->nextLast) {
        if (popFeCtrlQ) {
            pe_ifu_q->Read(threadId);
            genFetchReqPtr[threadId] = 0;
        } else {
            genFetchReqPtr[threadId]++;
        }
    }
    stats->ifu_merge++;
}

void PEIFU::LookUpRahqReq()
{
    auto printLookUpInfo = [this](FetchReqPtr const& fetchReq, bool iCacheHit, bool rahqReq) {
        if (iCacheHit) {
            cout << "[IFU" << dec << ifuID << " F1        Stage]: ";
            if (rahqReq) {
                cout << "FetchReq in RAHQ ICache Hit. ";
            } else {
                cout << "ICache Hit at Pipe.";
            }
        } else {
            cout << "[IFU" << dec << ifuID << " F1        Stage]: ";
            if (rahqReq) {
                cout << "FetchReq in RAHQ ICache Miss. ";
            } else {
                cout << "ICache Miss at Pipe.";
            }
        }
        cout << *(fetchReq) << endl;
    };

    auto processLookUpRahqReq = [this, &printLookUpInfo](FetchReqPtr const& fetchReq, bool rahqReq) {
        CheckPrefQFront(fetchReq->tpc, fetchReq->m_threadId);
        if (icache.lookupTag(utils->calcAddr(fetchReq->tpc))) {
            fetchReq->status = ICACHE_HIT;
        } else {
            fetchReq->status = ICACHE_MISS;
        }
        fetchReq->EnableFetchData(configs.ifu_fetch_width / WIDTH_16);
    };

    for (uint32_t i = 0; i < configs.ifu_fetch_req_num; i++) {
        if (pipe[PF0].fetchReq[i] && pipe[PF0].fetchReq[i]->status == ICACHE_IDEL) {
            processLookUpRahqReq(pipe[PF0].fetchReq[i], false);
            break;
        }
    }

    for (uint32_t i = 0; i < rahq.Size(); i++) {
        FetchReqPtr fetchReq = rahq[i];
        if (!fetchReq) {
            continue;
        }
        if (fetchReq->status == ICACHE_IDEL) {
            processLookUpRahqReq(fetchReq, true);
            break;
        }
    }
}

void PEIFU::HandleRahqReq()
{
    auto processRahqReq = [this](FetchReqPtr const& fetchReq) {
        if (!icache.lookUpMissQ(fetchReq->tpc) && !configs.icache_perfect) {
            if (!icache.insertMissQ(fetchReq->tpc, false)) {
                top->PEBase::SetBlockException(fetchReq->bid, fetchReq->stid, "IFU pipe insert missq error");
            }
        }
        fetchReq->status = ICACHE_WAIT;
    };

    if (!icache.CanSendPref()) {
        return;
    }
    for (uint32_t i = PF1; i >= PF0 && i < NSTAGE; i--) {
        PipeState state = pipe[i].state;
        if (state == INVALID) {
            continue;
        }
        for (uint32_t j = 0; j < configs.ifu_fetch_req_num; j++) {
            if (!pipe[i].fetchReq[j]) {
                continue;
            }
            FetchReqPtr fetchReq = pipe[i].fetchReq[j];
            if (fetchReq->status == ICACHE_MISS && icache.CanSendPref()) {
                processRahqReq(fetchReq);
                return;
            }
        }
    }

    for (uint32_t i = 0; i < rahq.Size(); i++) {
        FetchReqPtr fetchReq = rahq[i];
        if (!fetchReq) {
            continue;
        }
        if (fetchReq->status == ICACHE_MISS && icache.CanSendPref()) {
            if (!icache.lookUpMissQ(fetchReq->tpc)) {
                if (!icache.insertMissQ(fetchReq->tpc, false)) {
                    top->PEBase::SetBlockException(fetchReq->bid, fetchReq->stid, "IFU RAHQ insert missq error");
                }
            }
            fetchReq->status = ICACHE_WAIT;
            return;
        }
    }
}


void PEIFU::InsertToF4(FetchReqPtr const& fetchReq)
{
    DecodeBundle bundle = DecodeBundle(fetchReq->data.size());
    bundle.vld = fetchReq->vld;
    bundle.tpc = fetchReq->tpc;
    bundle.bid = fetchReq->bid;
    bundle.gid = fetchReq->gid;
    bundle.stid = fetchReq->stid;
    bundle.logicalGID = fetchReq->logicalGID;
    bundle.shapelpinfo = fetchReq->shapelpinfo;
    bundle.tid = fetchReq->m_threadId;
    bundle.bpc = fetchReq->bpc;
    bundle.fid = fetchReq->fid;
    bundle.endTPC = fetchReq->endTPC;
    bundle.mask = fetchReq->instCnt;
    bundle.last = fetchReq->last;
    bundle.first = fetchReq->first;
    bundle.blkSrcType = fetchReq->blkSrcType;
    bundle.biqType = fetchReq->biqType;
    bundle.isize = fetchReq->isize;
    bundle.tpcArr = fetchReq->tpcArr;
    for (uint32_t i = 0; i < fetchReq->instCnt; ++i) {
        bundle.entry[i] = fetchReq->data[i];
    }
    bundle.blockNoBranch = fetchReq->blockNoBranch;
    bundle.bundleNoBranch = fetchReq->blockNoBranch;
    fetchReq->pipe_cycle->f3Cycle = GetSim()->getCycles();
    bundle.pipe_cycle = fetchReq->pipe_cycle;

    uint32_t fetchReqTid = fetchReq->m_threadId;
    f4SimBuffer.Write(bundle);
    LOG_INFO_M(top->machineType, Stage::F3) << "Insert 1 bundle to F4 statge."
        << *fetchReq << " F4 readQ size " << dec << f4SimBuffer.Size() << " Wsize " << dec
        << f4SimBuffer.SizeW() << " Tid " << dec << fetchReqTid;
}

void PEIFU::InsertToF5()
{
    if (f5SimBuffer.getStall()) {
        return;
    }

    for (uint32_t i = 0; i < configs.ifu_fetch_req_num && !f4SimBuffer.Empty(); i++) {
        DecodeBundle bundle = f4SimBuffer.Read();
        bundle.pipe_cycle->f4Cycle = GetSim()->getCycles();
        f5SimBuffer.Write(bundle);
        LOG_INFO_M(top->machineType, Stage::F4) << "Insert 1 bundle to F5 statge."
            << "(Block " << bundle.bid << " Group " << bundle.gid <<  " fid " << bundle.fid << ")"
            << " F5 readQ size " << dec << f4SimBuffer.Size() << " Wsize " << dec
            << f4SimBuffer.SizeW() << " Tid " << dec << bundle.tid;
    }
}

void PEIFU::InsertToIB()
{
    auto &readQ = f5SimBuffer.GetRawReadData();
    uint32_t cnt = 0;
    for (auto it = readQ.cbegin(); cnt < configs.ifu_fetch_req_num && it != readQ.cend();) {
        DecodeBundle bundle = *(it);
        if (ifuDecThdQ->Full(bundle.tid)) {
            ++it;
            continue;
        }
        it = readQ.erase(it);
        ++cnt;
        bundle.pipe_cycle->f5Cycle = GetSim()->getCycles();
        // TODO: ifu stall pmu, after impl ifuDecStall, then change the condition below
        if (false) {
            top->IncIBStallStats();
        }
        ifuDecThdQ->Write(bundle, bundle.tid);
        LOG_INFO_M(top->machineType, Stage::F5) << "Insert 1 bundle to instBuffer."
            << "(Block " << bundle.bid << " Group " << bundle.gid <<  " fid " << bundle.fid << ")"
            << " InstBuffer readQ size " << dec << ifuDecThdQ->Size(bundle.tid) << " Wsize " << dec
            << ifuDecThdQ->SizeW(bundle.tid) << " Tid " << dec << bundle.tid;
    }
}

void PEIFU::GenPerfFetchReq(uint32_t tid)
{
    uint64_t fetchWidth = configs.ifu_fetch_width;
    BlkBodyFetchState &blkState = pe_ifu_q->At(0, tid);
    if (!blkState.vld || blkState.hyperTrace.instQ.size() == 0) {
        pe_ifu_q->Read(tid);
        pipe[PF3].state = INVALID;
        return;
    }
    uint32_t instWidth = WIDTH_16;
    FetchReqPtr fetchReq = std::make_shared<FetchReqBus>();
    fetchReq->vld = true;
    fetchReq->tpc = blkState.hyperTrace.instQ.front().tpc;
    fetchReq->bpc = blkState.bpc;
    fetchReq->bid = blkState.bid;
    fetchReq->hsize = blkState.hsize;
    fetchReq->blockNoBranch = blkState.noBranch;
    fetchReq->blkSrcType = blkState.blkSrcType;
    fetchReq->endTPC = blkState.endTPC;
    fetchReq->first = blkState.first;
    blkState.first = false;
    fetchReq->btype = blkState.btype;
    if (fetchReq->first) {
        if (fetchReq->blkSrcType != BlkSrcType::PARALLEL_FETCH) {
            top->PEBase::SetBlockRunning(fetchReq->bid, BlockStatus::RUNNING, fetchReq->stid);
        }
    }
    fetchReq->bundleNoBranch = true;
    fetchReq->last = false;
    while (fetchReq->mask < fetchWidth) {
        fetchReq->mask += instWidth;
        auto refInst = blkState.hyperTrace.instQ.front();
        blkState.hyperTrace.instQ.pop_front();
        if (refInst.jump) {
            fetchReq->bundleNoBranch = false;
            fetchReq->bp_dst = blkState.hyperTrace.instQ.front().tpc;
            break;
        }
        if (blkState.hyperTrace.instQ.size() == 0) {
            fetchReq->last = true;
            pe_ifu_q->Read(tid);
            break;
        }
    }
    pipe[PF3].fetchReq.push_back(fetchReq);
    fetchReq->EnableFetchData(fetchWidth / instWidth);
    icache.getCacheData(fetchReq, fetchWidth);
}

void PEIFU::InsertPerfToIb()
{
    if (pipe[PF3].state == INVALID) {
        return;
    }
    uint32_t decodeWidth = top->peID < sim->core->configs.stdPeCount ? stdDecWidth : simtDecWidth;
    for (uint32_t i = 0; i < configs.ifu_fetch_req_num; i++) {
        if (!pipe[PF3].fetchReq[i]) {
            continue;
        }
        uint32_t instSize =  WIDTH_64;
        uint32_t bundleSize = (pipe[PF3].fetchReq[i]->mask - 1) / (decodeWidth * instSize) + 1;
        if (ifuDecThdQ->Size(pipe[PF3].fetchReq[i]->m_threadId) + bundleSize > configs.ibuf_depth) {
            pipe[PF3].state = STALL;
        } else {
            InsertToF4(pipe[PF3].fetchReq[i]);
            pipe[PF3].state = INVALID;
        }
    }
}

}

} // namespace JCore
