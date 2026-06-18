#include "mtccore/lsu/store_unit/MtcStoreUnit.h"
#include "mtccore/lsu/MtcLoadStoreUnit.h"
#include "ModelCommon/SimInstInfo.h"

namespace JCore {

void MtcStoreUnit::Reset(void)
{
    ROBID clr;
    clr.val = 0;
    clr.wrap = false;

    lastId = IDBus();
    stq.Reset();
    for (auto &it: top->iex_lsu_sta_array) {
        it->Reset();
    }
    for (auto &it: top->iex_lsu_std_array) {
        it->Reset();
    }

    for (auto &it: top->lsu_pe_rslv_array)
        it->Reset();

    lookup_lu_su_q->clear();
    lookup_su_lu_q->clear();
    bypass_su_lu_q->clear();

    lsu_iex_sid_q.clear();
}

void MtcStoreUnit::Build(void)
{
    stq.top = this;
    stq.sim = sim;
    stq.pConfigs = pConfigs;

    uint32_t count = pConfigs->stq_depth;
    if (top->typeId == LSUType::SCALAR_LSU) {
        count = pConfigs->scalar_stq_depth;
    }
    stq.Build(count);
}

void MtcStoreUnit::Work(void)
{
    // load request for stq
    handleLoadReq();

    // Oldest store is stalled
    checkDeadLock();

    // store commit
    getCommitID();

    // store dispatch
    store();

    // Handle for MDB response
    mdbCheck();

    stq.SUStats_tick();

    GetSim()->stqFree = stq.Empty();
}

void MtcStoreUnit::store()
{
    struct DispatchIdx {
        ExecEngineTyp iexTyp;
        uint32_t idx;
        DispatchIdx(){};
        DispatchIdx(ExecEngineTyp a, uint32_t b) : iexTyp(a), idx(b) {}
    };
    // Oldest first
    std::list<DispatchIdx> dispathQ;
    dispathQ.clear();
    // Handle sta
    for (uint32_t i = 0; i < top->iex_lsu_sta_array.size(); ++i) {
        if (top->iex_lsu_sta_array[i]->Empty())
            continue;
        auto it = dispathQ.begin();
        for (bool stop = false; it != dispathQ.end() && !stop; ++it) {
            MemReqBus st1 = top->iex_lsu_sta_array[it->idx]->Front();
            MemReqBus st2 = top->iex_lsu_sta_array[i]->Front();
            stop = LessEqual(st2.bid, st2.lsID, st1.bid, st1.lsID);
        }
        ExecEngineTyp iex = (top->typeId == LSUType::SCALAR_LSU) ? SCALAR_IEX : MEM_IEX;
        dispathQ.insert(it, DispatchIdx(static_cast<ExecEngineTyp>(iex), i));
    }

    uint32_t iexNum = static_cast<uint32_t>(ExecEngineTyp::IEX_NUM) + top->core->configs.simtPeCount;
    for (auto &idx : dispathQ) {
        auto &iex_lsu_sta_q = top->iex_lsu_sta_array[idx.idx];
        while (!iex_lsu_sta_q->Empty()) {
            MemReqBus req = iex_lsu_sta_q->Front();
            ASSERT(req.tid != -1U);
            bool suaccelss = insertStq(req);
            if (suaccelss) {
                iex_lsu_sta_q->Read();
                for (uint32_t i = 0; top->core->configs.perfect_load_store && i < iexNum; i++) {
                    uint32_t coreId = (i == SIMT_IEX) ? 0 :
                        static_cast<uint32_t>(i) - static_cast<uint32_t>(ExecEngineTyp::IEX_NUM);
                    std::shared_ptr<IEX> iex = (top->core->IsVectorIex(static_cast<ExecEngineTyp>(i))) ?
                        GetSim()->core->vectorTop->GetIex(coreId) : GetSim()->core->iex[i];
                    iex->iq.wakeupLda(req.ref_id);
                }
                LOG_INFO_M(Unit::LSU, Stage::NA) << "Store Dispatch: Get Store Request(addr) from PE["
                    << req.peID << "] "<< req;
            } else {
                break;
            }
        }
    }

    ASSERT(atomic_lu_su_q);
    while (!atomic_lu_su_q->Empty()) {
        MemReqBus req = atomic_lu_su_q->Front();
        ASSERT(req.tid != -1U);
        bool suaccelss = insertStq(req);
        if (!suaccelss) {
            break;
        }
        atomic_lu_su_q->Read();
    }

    dispathQ.clear();
    for (uint32_t iex = 0; iex < IEX_NUM; iex++) {
        // Handle std
        for (uint32_t i = 0; i < top->iex_lsu_std_array.size(); ++i) {
            if (top->iex_lsu_std_array[i]->Empty())
                continue;
            auto it = dispathQ.begin();
            for (bool stop = false; it != dispathQ.end() && !stop; ++it) {
                MemReqBus st1 = top->iex_lsu_std_array[it->idx]->Front();
                MemReqBus st2 = top->iex_lsu_std_array[i]->Front();
                stop = LessEqual(st2.bid, st2.lsID, st1.bid, st1.lsID);
            }
            dispathQ.insert(it, DispatchIdx(static_cast<ExecEngineTyp>(iex), i));
        }
    }
    for (auto &idx : dispathQ) {
        auto &iex_lsu_std_q = top->iex_lsu_std_array[idx.idx];
        while (!iex_lsu_std_q->Empty()) {
            MemReqBus req = iex_lsu_std_q->Front();
            ASSERT(req.tid != -1U);
            bool suaccelss = insertStq(req);
            if (suaccelss) {
                iex_lsu_std_q->Read();
                ++top->stats->total_store_request;
                top->CountAddr(req.addr);
                LOG_INFO_M(Unit::LSU, Stage::NA) << "Store Dispatch: Get Store Request(data) from PE["
                    << req.peID << "] "<< req;
            } else {
                break;
            }
        }
    }
    for (auto &tTransMemStReqQ : top->tTransMemStReqArray) {
        while (!tTransMemStReqQ->Empty()) {
            TTransMemStReq req = tTransMemStReqQ->Front();
            MemReqBus bus;
            bus.vld = true;
            bus.addr = req.GetAddr();
            bus.size = req.GetSize();
            bus.tTransId = req.GetReqId();
            req.GetData(bus.memData);
            bus.tTransReq = true;
            bus.type = ST_ALL;
            ASSERT(bus.tid != -1U);
            bool suaccelss = insertStq(bus);
            if (!suaccelss) {
                break;
            }
            tTransMemStReqQ->Read();
            ++top->stats->total_store_request;
            top->CountAddr(bus.addr);
            LOG_INFO_M(Unit::LSU, Stage::NA) << "Store Dispatch: Get TTrans Request(data) from PE["
                << bus.peID << "] "<< bus;
        }
    }
}

bool MtcStoreUnit::CheckFlushStall(ROBID &oldestBID)
{
    if (core->bctrl->blockROB.needFlush(oldestBID, 0)) {
        if ((lastId.bid == oldestBID && stq_stall_cyc < MTC_STQ_COMMIT_STALL) ||
            !lastId.vld || lastId.bid != oldestBID) {
            lastId.vld = true;
            lastId.bid = oldestBID;
            stq_stall_cyc = (!lastId.vld || lastId.bid != oldestBID) ? 1 : stq_stall_cyc+1;
            return true;
        }
    }

    stq_stall_cyc = 0;
    return false;
}

static IDBus GetScalarOldestInfo(std::vector<std::shared_ptr<PEBase>> &peArray, uint32_t start,
    uint32_t size, ROBID &oldestId)
{
    IDBus oldCmt;
    uint32_t end = start + size;
    for (uint32_t pe = start; pe < end; ++pe) {
        IDBus cmtBus = peArray[pe]->GetRetireID(0);
        if (!cmtBus.vld || cmtBus.bid != oldestId) {
            continue;
        }

        oldCmt = (!oldCmt.vld || LessEqual(cmtBus.bid, cmtBus.lsID, oldCmt.bid, oldCmt.lsID)) ? cmtBus : oldCmt;
    }

    return oldCmt;
}

static IDBus GetSimtOldestInfo(std::vector<std::shared_ptr<PEBase>> &peArray, uint32_t start,
    uint32_t size, ROBID &oldestId)
{
    IDBus oldCmt;
    uint32_t end = start + size;
    for (uint32_t pe = start; pe < end; ++pe) {
        IDBus cmtBus = peArray[pe]->GetRetireID();
        if (!cmtBus.vld || cmtBus.gid != oldestId) {
            continue;
        }
        oldCmt = (!oldCmt.vld || LessEqual(cmtBus.bid, cmtBus.gid, cmtBus.lsID,
            oldCmt.bid, oldCmt.gid, oldCmt.lsID)) ? cmtBus : oldCmt;
    }

    return oldCmt;
}

bool MtcStoreUnit::checkCommit(IDBus &commitBus)
{
    ROBID oldestBID = core->bctrl->blockROB.getOldestBlockID(0);
    ROBID oldestGid = top->core->mtcCores[0]->GetOldestGid();
    if (CheckFlushStall(oldestBID)) {
        return false;
    }

    IDBus oldCmt;
    if (top->typeId == LSUType::SCALAR_LSU) {
        uint32_t startIdx = 0;
        uint32_t size = top->core->configs.stdPeCount;
        oldCmt = GetScalarOldestInfo(top->core->peArray, startIdx, size, oldestBID);
    } else if (top->typeId == LSUType::VECTOR_LSU) {
        uint32_t startIdx = top->core->configs.stdPeCount;
        uint32_t size = top->core->configs.simtPeCount;
        oldCmt = GetSimtOldestInfo(top->core->peArray, startIdx, size, oldestGid);
    } else if (top->typeId == LSUType::MEMORY_LSU) {
        uint32_t startIdx = top->core->configs.stdPeCount + top->core->configs.simtPeCount;
        uint32_t size = top->core->configs.memPeCount;
        oldCmt = GetSimtOldestInfo(top->core->peArray, startIdx, size, oldestGid);
    } else {
        ASSERT(0 && "not support");
    }

    commitBus = oldCmt;
    if (!commitBus.vld) {
        commitBus.vld = true;
        commitBus.bid = oldestBID;
        commitBus.bid = oldestGid;
        commitBus.lsID.val = 0;
        commitBus.lsID.wrap = false;
    }
    return true;
}

void MtcStoreUnit::getCommitID()
{
    IDBus commitBus = IDBus();
    if (checkCommit(commitBus)) {
        // Store in stq wait to commit
        stq.retire(commitBus);
    }

    // sort and commit store to SCB
    stq.commit();

    // TODO: BSB window sliding
    while (!lsu_iex_sid_q.empty()) {
        lsu_iex_sid_q.pop_front();
    }
}

bool MtcStoreUnit::insertStq(MemReqBus &req)
{
    bool inserted = false;
    if (req.type == ST_ALL) {
        if (!stq.full()) {
            stq.insert(req);
            detect_su_lu_q->push_back(req);
            ASSERT(req.tid != -1U);
            top->lsu_pe_rslv_array[req.peID]->Write(req);
            inserted = true;
        }
    } else {
        bool merged = stq.mergeStore(req);
        if (merged) {
            ASSERT(req.tid != -1U);
            top->lsu_pe_rslv_array[req.peID]->Write(req);
            inserted = true;
        } else if (!stq.full()) {
            stq.insert(req);
            detect_su_lu_q->push_back(req);
            inserted = true;
        }
    }

    if (inserted && (req.type == ST_ALL || req.type == ST_DATA)) {
        UpdateSTValid(req.reqData.positionVld, req.addr, req.size, true, req.tag);
        wakeup_su_lu_q->push_back(req);
    }

    return inserted;
}

void MtcStoreUnit::handleLoadReq()
{
    // load look up stq
    while (!lookup_lu_su_q->empty()) {
        auto loadReq = lookup_lu_su_q->front();
        lookup_lu_su_q->pop_front();
        stq.getData(loadReq);
        lookup_su_lu_q->push_back(loadReq);
    }

    // Cehck if wait for store
    while (!wait_lu_su_q->empty()) {
        MemReqBus bus = wait_lu_su_q->front();
        wait_lu_su_q->pop_front();
        stq.checkWait(bus);
        wait_su_lu_q->push_back(bus);
    }
}

void MtcStoreUnit::Xfer(void)
{
    stq.Xfer();
}

void MtcStoreUnit::flush(FlushBus flushReq)
{
    auto match = [&flushReq] (MemReqBus &bus) -> bool {
        return flushReq.match(bus);
    };

    for (auto simQ: top->iex_lsu_sta_array) {
        simQ->FlushIf(match);
    }
    for (auto simQ: top->iex_lsu_std_array) {
        simQ->FlushIf(match);
    }

    for (auto simQ: top->lsu_pe_rslv_array)
        simQ->FlushIf(match);
    stq.flush(flushReq);
}

SimSys *MtcStoreUnit::GetSim(void)
{
    return sim;
}

bool MtcStoreUnit::checkStall()
{
    return stq.full();
}

bool MtcStoreUnit::checkStall(uint32_t size)
{
    return stq.full(size);
}

void MtcStoreUnit::mdbCheck(void)
{
    while (!lookup_mdb_su_q->empty()) {
        MDBBus bus = lookup_mdb_su_q->front();
        lookup_mdb_su_q->pop_front();

        // Check if it is exit
        MemReqBus hitReq = stq.mdbCheck(bus.stInfo);
        if (!hitReq.vld)
            continue;
        UpdateSTValid(hitReq.reqData.positionVld, hitReq.addr, hitReq.size, true, hitReq.tag);
        wakeup_su_lu_q->push_back(hitReq);
    }
}

void MtcStoreUnit::checkDeadLock()
{
    if (!stq.stall()) {
        return;
    }

    ROBID oldestBID = top->core->bctrl->blockROB.getOldestBlockID(0);
    IDBus oldRetire;
    for (uint32_t pe = 0; pe < top->core->peArray.size(); ++pe) {
        oldRetire = top->core->peArray[pe]->GetRetireID();
        if (oldRetire.vld && oldRetire.bid == oldestBID) {
            break;
        }
        oldRetire.vld = false;
    }
    if (!oldRetire.vld || oldRetire.bid != oldestBID || !oldRetire.isLoadStore || oldRetire.isLoad) {
        return;
    }

    MemReqBus yongestStore = stq.checkDeadLock(oldRetire);
    if (!yongestStore.vld) {
        return;
    }

    FlushReq flushReq;
    flushReq.vld = true;
    flushReq.iexTyp = oldRetire.iexTyp;
    BlockCommandPtr cmd = top->core->bctrl->blockROB.GetBlockCMDPtr(oldestBID, 0);
    flushReq.noSplitBlk = false;
    if (cmd && cmd->opcode != Opcode::OP_INVALID) {
        ROBID bid = oldestBID;
        AddROBID(bid, 1, top->core->configs.block_rob_depth);
        cmd = top->core->bctrl->blockROB.GetBlockCMDPtr(bid, 0);
        if (cmd == nullptr) return;
        flushReq.bid = bid;
        flushReq.fetchTPCVld = true;
        flushReq.fetchTPC = cmd->bpc;
        flushReq.peID = cmd->peid;
        flushReq.immediateFlush = true;
        flushReq.fbid = cmd->machineInst->bfuInfo->fbid;
        flushReq.fbid_local = cmd->machineInst->bfuInfo->fbid_local;
        top->core->flushUnit->flush_stats->IntraBlockMemoryAaccelssConflict++;
        flushReq.type = FlushType::NUKE_FLUSH; // TODO: 当最老块是模板块时，最老的 store 指令开始写入 SCB，load 指令可能会拿错数据
        top->stats->store_load_conflicts++;
        top->lsu_flush_rpt_q->Write(flushReq);
        LOG_INFO_M(Unit::LSU, Stage::NA) << "load-store flush " << ((flushReq.type == FlushType::INNER_FLUSH) ? "inner" : "inter")
            << "load " << oldRetire;
        return;
    }
    flushReq.bid = oldRetire.bid;
    flushReq.rid = oldRetire.rid;
    flushReq.tid = oldRetire.tid;
    flushReq.fetchTPCVld = true;
    flushReq.fetchTPC = oldRetire.tpc;
    flushReq.peID = oldRetire.peID;
    flushReq.lsID = oldRetire.lsID;
    flushReq.fbid = oldRetire.fbid;
    auto inst = dynamic_pointer_cast<VecPE>(GetSim()->core->peArray[oldRetire.peID])
                ->prob[oldRetire.tid]->getROBEntry(oldRetire.rid.val).inst;
    // if current instruction do not need the local reg, then we need to flush from the one before
    flushReq.tSeq = oldRetire.tSeq;
    if (inst->DstTypeContain(OperandType::OPD_TLINK)) {
        flushReq.tSeq = GetPrevRegSeq(GetSim(), flushReq.tSeq, OperandType::OPD_TLINK, flushReq.peID, flushReq.tid);
    }
    flushReq.uSeq = oldRetire.uSeq;
    if (inst->DstTypeContain(OperandType::OPD_ULINK)) {
        flushReq.uSeq = GetPrevRegSeq(GetSim(), flushReq.uSeq, OperandType::OPD_ULINK, flushReq.peID, flushReq.tid);
    }
    flushReq.predSeq = oldRetire.predSeq;
    if (inst->DstTypeContain(OperandType::OPD_PREDMASK)) {
        flushReq.predSeq = GetPrevRegSeq(GetSim(), flushReq.predSeq, OperandType::OPD_PREDMASK, flushReq.peID, flushReq.tid);
    }
    flushReq.fbid_local = oldRetire.fbid_local;
    top->core->flushUnit->flush_stats->IntraBlockMemoryAaccelssConflict++;
    top->core->flushUnit->flush_stats->InterBlockMemoryAaccelssConflict++;
    flushReq.type = FlushType::INNER_FLUSH;
    if (top->typeId == LSUType::MEMORY_LSU) {
        ASSERT(flushReq.type != FlushType::INNER_FLUSH);
    }
    top->lsu_flush_rpt_q->Write(flushReq);
    top->stats->store_load_conflicts++;
    LOG_INFO_M(Unit::LSU, Stage::NA) << "load-store flush " << ((flushReq.type == FlushType::INNER_FLUSH) ? "inner" : "inter")
        << "load " << oldRetire;
}

} // namespace JCore