#include "iex/iex_dispatch.h"

#include "core/Core.h"
#include "iex/iex.h"

namespace JCore {


void DispatchUnit::Work() {
    // Dispatch stage
    dispatch();
    // Report statistics
    rptDispatchStats();
}

void DispatchUnit::rptDispatchStats() {
    auto cal_stats = [] (vector<IssueState> &cur_iq, vector<IssueState> &next_iq, IQStats &stats) {
        bool all_stall = true;
        for (uint32_t i = 0; i < next_iq.size(); i++) {
            if ((next_iq[i].size == cur_iq[i].size) && next_iq[i].insert_stall) {
                stats.insert_stall_count[i]++;
            } else {
                all_stall = false;
            }
            next_iq[i].insert_stall = false;
        }
        if (all_stall) {
            stats.all_insert_stall++;
        }
    };
    cal_stats(top->iq.current.cmdIQ, top->iq.next.cmdIQ, top->stats->cmd_iq);
    cal_stats(top->iq.current.aluIQ, top->iq.next.aluIQ, top->stats->alu_iq);
    cal_stats(top->iq.current.aguIQ, top->iq.next.aguIQ, top->stats->agu_iq);
    cal_stats(top->iq.current.ldaIQ, top->iq.next.ldaIQ, top->stats->lda_iq);
    cal_stats(top->iq.current.staIQ, top->iq.next.staIQ, top->stats->sta_iq);
    cal_stats(top->iq.current.stdIQ, top->iq.next.stdIQ, top->stats->std_iq);
    cal_stats(top->iq.current.bruIQ, top->iq.next.bruIQ, top->stats->bru_iq);
    cal_stats(top->iq.current.scaIQ, top->iq.next.scaIQ, top->stats->sca_iq);
}

uint32_t GetSize(const std::vector<SimQueue<SimInst>*>& pe_iex_array) {
    uint32_t sz = 0;
    for (auto &q : pe_iex_array) {
        if (q) {
            sz += q->Size();
        }
    }

    return sz;
}

void DispatchUnit::SelectMinPEIDToQueue(vector<SimQueue<SimInst>*> &array)
{
    bool found = false;
    ROBID dispatchBID;
    uint32_t pe = -1;
    unordered_map<uint32_t, uint32_t> hash;

    for (uint32_t i = 0; i < array.size(); i++) {
        for (uint32_t j = 0; j < array.size(); j++) {
            if (array[j] == nullptr) {
                continue;
            }
            if (array[j]->Empty() || hash[j] > 0 || !array[j]) {
                continue;
            }
            if (!found || (found && LessEqual(array[j]->Front()->bid, dispatchBID))) {
                pe = j;
                dispatchBID = array[j]->Front()->bid;
                found = true;
            }
        }
        if (found && hash[pe] == 0) {
            peid_q.push(pe);
            hash[pe]++;
            found = false;
        }
    }
}

void DispatchUnit::GenFlushRequest(ROBID bid, uint32_t fbid, uint64_t tpc, uint32_t peid, uint32_t stid) const
{
    FlushReq req = FlushReq();
    req.vld = true;
    req.bid = bid;
    req.fetchTPCVld = true;
    req.fetchTPC = tpc;
    req.peID = peid;
    req.immediateFlush = true;
    req.fbid = fbid;
    req.stid = stid;
    req.type = FlushType::NUKE_FLUSH;
    uint32_t brobCap = this->top->core->bctrl->blockROB.getBROBCapacity(stid);
    IncROBID(bid, brobCap);
    top->core->flushUnit->flush_stats->IntraBlockMemoryAaccelssConflict++;
    top->core->flushUnit->flush_stats->smtIntraBlockMemoryAaccelssConflictArray[req.stid]++;
    LOG_DEBUG_M(top->machineType, Stage::NA) << "[IEX DISP]: slow insert to MEMISQ, gen nuke flush, bid: " << bid;
    this->top->iex_flush_rpt_q->Write(req);
}

void DispatchUnit::dispatch() {
    bool isAllStall = true;
    uint32_t insertCnt = 0;
    uint32_t sz = 0;
    uint64_t slots = top->core->GetScalarPEDecWidth() * top->core->configs.stdPeCount;
    slots += top->core->GetVecPEDecWidth() * top->core->configs.simtPeCount;
    top->stats->slots += slots;

    sz += GetSize(pe_iex_cmd_array);
    sz += GetSize(pe_iex_alu_array);
    sz += GetSize(pe_iex_lda_array);
    sz += GetSize(pe_iex_sc_lda_array);
    sz += GetSize(pe_iex_sta_array);
    sz += GetSize(pe_iex_std_array);
    sz += GetSize(pe_iex_bru_array);
    sz += GetSize(pe_iex_vec_agu_array);
    sz += GetSize(pe_iex_vec_scalar_array);
    if (sz == 0) {
        if (!top->iq.current.isEmpty()) {
            top->stats->slots_empty += slots;
        }
        return;
    }

    auto dispatch_func = [this, &isAllStall, &insertCnt] (vector<IssueState> &cur_iq, vector<IssueState> &next_iq,
            bool isAlu, vector<SimQueue<SimInst>*> &array, DispatchInfo &info, uint64_t &stall_slots, uint64_t wPort) {
        SelectMinPEIDToQueue(array);
        while (!peid_q.empty()) {
            // Dispatch mode 2 : limit number of entry for PE
            if (this->configs.iex_dispatch_mode == 2) {
                if (info.count[peid_q.front()] >= info.maxCount) {
                    peid_q.pop();
                    continue;
                }
            }
            uint32_t size = next_iq.size();
            uint32_t dispatchID = info.dispatchID % size;
            ROBID instBid;
            uint32_t instStid = 0;
            ASSERT(array[peid_q.front()]);
            if (!array[peid_q.front()]->Empty()) {
                instBid = array[peid_q.front()]->Front()->bid;
                instStid = array[peid_q.front()]->Front()->stid;
            }
            ROBID gid;
            if (top->core->IsVectorIex(top->machineType)) {
                gid = GetSim()->core->vectorTop->GetOldestGid(array[peid_q.front()]->Front()->stid);
            } else if (top->id == MEM_IEX) {
                gid = GetSim()->core->mtcCores[0]->GetOldestGid();
            }
            ROBID bid = GetSim()->core->bctrl->blockROB.getOldestBlockID(instStid);
            BlockCommandPtr oldestCmd = GetSim()->core->bctrl->blockROB.GetBlockCMDPtr(bid, instStid);
            bool isInstNeedReplayOpcode = false;
            bool isOldestBlkMemOpcode = false;
            BlockCommandPtr instCmd = GetSim()->core->bctrl->blockROB.GetBlockCMDPtr(instBid, instStid);
            if (instCmd) {
                isInstNeedReplayOpcode = (instCmd->opcode == Opcode::OP_MSET ||
                                          instCmd->opcode == Opcode::OP_MCOPY);
            }
            if (oldestCmd) {
                isOldestBlkMemOpcode = (oldestCmd->opcode == Opcode::OP_MSET ||
                                        oldestCmd->opcode == Opcode::OP_MCOPY);
            }

            if (reservedNum == size && !isOldestBlkMemOpcode) {
                reservedNum = this->configs.iex_iq_reserve_count;
            }

            auto isqHaveEnoughSpace = [this, array](uint32_t dispatchID, uint32_t reservedSize, ROBID bid, ROBID gid,
                                    vector<IssueState> &nextIQ) {
                bool isOldestBid = (array[peid_q.front()]->Front()->bid == bid);
                bool isOldestGid = (array[peid_q.front()]->Front()->gid == gid);
                bool notReservedFull = !nextIQ[dispatchID].reservefull();
                bool notFull = !nextIQ[dispatchID].full();
                bool reservedCheck = (dispatchID < reservedSize);

                if (top->id == SCALAR_IEX) {
                    if (isOldestBid) {
                        return notFull;
                    }
                    return (reservedCheck && notReservedFull) || ((!reservedCheck) && notFull);
                } else if (top->core->IsVectorIex(top->machineType) || top->id == MEM_IEX) {
                    if (isOldestBid && isOldestGid) {
                        return notFull;
                    }
                    return (reservedCheck && notReservedFull) || ((!reservedCheck) && notFull);
                } else {
                    ASSERT(0 && "Should not be here");
                }
                    return false;
                };

            // Check stall : Every queue only could be inserted limited number of instructions
            bool stall = true;
            for (uint32_t j = 0; j < size; j++) {
                if (this->configs.iex_dispatch_mode == 1) {
                    if (next_iq[dispatchID].size - cur_iq[dispatchID].size >= wPort) {
                        dispatchID = (dispatchID + 1) % size;
                        continue;
                    }
                    // Keep one entry for the first four ALUIQ
                    if (isAlu) {
                        if (isqHaveEnoughSpace(dispatchID, top->configs.iex_aluiq_reserve_count, bid, gid, next_iq)) {
                            stall = false;
                            break;
                        }
                    } else {
                        // Keep one entry for the first IQ
                        if (isqHaveEnoughSpace(dispatchID, reservedNum, bid, gid, next_iq)) {
                            stall = false;
                            break;
                        }
                    }
                    next_iq[dispatchID].insert_stall = true;
                    dispatchID = (dispatchID + 1) % size;
                } else {
                    if (!next_iq[dispatchID].full() && next_iq[dispatchID].size - cur_iq[dispatchID].size < this->configs.max_iq_insert_num) {
                        stall = false;
                        break;
                    }
                    next_iq[dispatchID].insert_stall = true;
                    dispatchID = (dispatchID + 1) % size;
                }
            }

            isAllStall &= stall;
            if (stall) {
                SimInst minst = array[peid_q.front()]->Front();

                if (instCmd && instCmd->opcode != Opcode::OP_INVALID &&
                    isInstNeedReplayOpcode && minst->bid == bid &&
                    reservedNum == this->configs.iex_iq_reserve_count && this->configs.iex_disp_fast_reply_enable) {
                    GenFlushRequest(bid, instCmd->machineInst->bfuInfo->fbid, minst->pc, instCmd->peid, instCmd->stid);
                    reservedNum = next_iq.size();
                }

                if (OpcodeIsStore(minst->opcode) && minst->type == ST_ADDR) {
                    peid_q.pop();
                    continue;
                }
                for (auto &it : array[peid_q.front()]->GetRawReadData()) {
                    if (it->hasIqStall) {
                        continue;
                    }
                    it->hasIqStall = true;
                    // Recorded not splited instruction
                    stall_slots += it->stack_check ? 1 : 0;
                }
                peid_q.pop();
                LOG_INFO_M(top->machineType, Stage::S1) << "dispatch[" << dispatchID << "] stall";
                continue;
            };
            // Insert sucessfully
            SimInst inst = array[peid_q.front()]->Read();
            inst->iqid = dispatchID;
            next_iq[dispatchID].insert_stall = false;
            if (array[peid_q.front()]->Empty()) {
                peid_q.pop();
            }
            if (OpcodeIsStore(inst->opcode) && inst->type == ST_ADDR && inst->stack_type != StackInstType::STACK_SET) {
                int store_idx = top->iexmdb.ld_lookup(inst->pc, false);
                if (store_idx != -1) {
                    uint32_t reg = top->iexmdb.lookupReg(store_idx, inst->pc, inst->bid, inst->lsID, false);
                    if (reg != -1U) {
                        POperandPtr dst = std::make_shared<PhysicalOperand>();
                        dst->type = OperandType::LS_MDB_DEPENDENCY;
                        dst->ptag = reg;
                        inst->pdsts_.emplace_back(dst);
                    }
                }
            }
            if (OpcodeIsLoad(inst->opcode) && inst->GetPSrcPtrByType(OperandType::STACK_POINTER) == nullptr) {
                int wait_store = top->iexmdb.ld_lookup(inst->pc, true);
                if (wait_store != -1) {
                    uint32_t reg = top->iexmdb.lookupReg(wait_store, inst->pc, inst->bid, inst->lsID, true);
                    if (reg != -1U) {
                        POperandPtr src = std::make_shared<PhysicalOperand>();
                        src->type = OperandType::LS_MDB_DEPENDENCY;
                        src->ptag = reg;
                        inst->psrcs_.emplace_back(src);
                        inst->wait_store = wait_store;
                    }
                }
            }
            this->top->rtable.checkReady(inst);
            if (!inst) {
                continue;
            }
            next_iq[dispatchID].insert(inst);
            if (inst->stack_check && (!OpcodeIsStore(inst->opcode) || inst->type != ST_ADDR)) {
                ++insertCnt;
            }
            this->top->core->peArray[inst->peID]->IncS1Stats();
            if (inst->stack_type == StackInstType::STACK_GET) {
                this->top->stats->s1_stack_get_inst++;
            } else if (inst->stack_type == StackInstType::STACK_LOAD) {
                this->top->stats->s1_stack_load_inst++;
            }
            this->top->stats->ope_s1_inst_cnt++;
            if (this->configs.iex_dispatch_mode == 2) {
                info.count[inst->peID]++;
            }

            // Update next dispatch IQ ID : find the one that has minimum outstading size
            for (uint32_t j = 0; j < next_iq.size(); j++) {
                if (next_iq[info.dispatchID].size > next_iq[j].size) {
                    info.dispatchID = j;
                }
            }

            inst->pipeCycle->dispatchCycle = GetSim()->getCycles();
            inst->pipeCycle->iqCycle = GetSim()->getCycles() + 1;
            if (inst->IsReady()) {
                inst->pipeCycle->rdyCycle = GetSim()->getCycles() + 1;
            }
        }
    };

    bool picked = false;
    size_t pickedIdx = 0;
    ROBID tmp;
    for (size_t i = 0; i < pe_iex_cmd_array.size(); i++) {
        if (pe_iex_cmd_array[i] == nullptr) {
            continue;
        }
        if (pe_iex_cmd_array[i]->Empty()) {
            continue;
        }

        if (!picked) {
            picked = true;
            pickedIdx = i;
            tmp = pe_iex_cmd_array[i]->Front()->bid;
            continue;
        } else {
            ROBID frontBID = pe_iex_cmd_array[i]->Front()->bid;
            if (LessEqual(frontBID, tmp)) {
                pickedIdx = i;
                tmp = frontBID;
            }
        }
    }
    if (picked) {
        while (!pe_iex_cmd_array[pickedIdx]->Empty()) {
            SimInst inst = pe_iex_cmd_array[pickedIdx]->Front();
            if (inst->bid != tmp) {
                break;
            }
            pe_iex_cmd_array[pickedIdx]->Read();
            inst->iqid = 0;
            LOG_INFO_M(top->machineType, Stage::S1) << "iex_dispatch_cmd inst" << inst->Dump();
            top->iq.cmdIQ.Write(inst);
        }
    }

    dispatch_func(top->iq.current.aluIQ, top->iq.next.aluIQ, true, pe_iex_alu_array, aluInfo,
                  top->stats->alu_iq.slots_stall, top->aluIQWport);
    dispatch_func(top->iq.current.aguIQ, top->iq.next.aguIQ, false, pe_iex_vec_agu_array, aguInfo,
                  top->stats->agu_iq.slots_stall, top->aguIQWport);
    dispatch_func(top->iq.current.ldaIQ, top->iq.next.ldaIQ, false, pe_iex_lda_array, ldaInfo,
                  top->stats->lda_iq.slots_stall, top->ldaIQWport);

    dispatch_func(top->iq.current.ldaScIQ, top->iq.next.ldaScIQ, false, pe_iex_sc_lda_array, ldascInfo,
                  top->stats->lda_sc_iq.slots_stall, top->ldaIQWport);

    dispatch_func(top->iq.current.staIQ, top->iq.next.staIQ, false, pe_iex_sta_array, staInfo,
                  top->stats->sta_iq.slots_stall, top->staIQWport);
    dispatch_func(top->iq.current.stdIQ, top->iq.next.stdIQ, false, pe_iex_std_array, stdInfo,
                  top->stats->std_iq.slots_stall, top->stdIQWport);
    dispatch_func(top->iq.current.bruIQ, top->iq.next.bruIQ, false, pe_iex_bru_array, bruInfo,
                  top->stats->bru_iq.slots_stall, top->bruIQWport);
    dispatch_func(top->iq.current.scaIQ, top->iq.next.scaIQ, false, pe_iex_vec_scalar_array, scaInfo,
                  top->stats->sca_iq.slots_stall, top->scaIQWport);
    if (isAllStall) {
        ++this->top->stats->stall_cycle;
        ++top->stats->dispatchStall;
    }
    top->stats->slots_bandwidth += slots - insertCnt;
}

void DispatchUnit::Xfer() {
    auto moveQueue = [](const std::deque<SimInst>& queue) {
        if (queue.empty()) {
            return;
        }
        for (auto it = queue.begin(); it != queue.end(); it++) {
            (*it)->MoveLpv();
        }
    };

    auto array_xfer = [&](const std::vector<SimQueue<SimInst>*>& array) {
        for (auto &q : array) {
            if (!q) {
                continue;
            }
            auto &writeQ = q->GetRawWriteData();
            auto &readQ = q->GetRawReadData();
            moveQueue(writeQ);
            moveQueue(readQ);

            if (!q->Empty()) {
                q->setStall();
            } else {
                q->unsetStall();
            }
            q->Work();
        }
    };
    array_xfer(pe_iex_cmd_array);
    array_xfer(pe_iex_alu_array);
    array_xfer(pe_iex_lda_array);
    array_xfer(pe_iex_sc_lda_array);
    array_xfer(pe_iex_sta_array);
    array_xfer(pe_iex_std_array);
    array_xfer(pe_iex_bru_array);
    array_xfer(pe_iex_vec_agu_array);
    array_xfer(pe_iex_vec_scalar_array);
}

void DispatchUnit::flush(const FlushBus& flushReq) {
    auto match = [&flushReq] (SimInst inst) -> bool {
        if (inst->stid != flushReq.req.stid) {
            return false;
        }
        if (flushReq.baseOnPE && flushReq.req.peID != inst->peID) {
            return false;
        }
        if (flushReq.baseOnThread && flushReq.req.tid != inst->GetTid()) {
            return false;
        }
        return flushReq.baseOnBid ? LessEqual(flushReq.req.bid, inst->bid):
            LessEqual(flushReq.req.bid, flushReq.req.rid, inst->bid, inst->rid);
    };

    auto flush_func = [&match](vector<SimQueue<SimInst>*> &iq_array) {
        for (uint32_t i = 0; i < iq_array.size(); i++) {
            if (iq_array[i] == nullptr) {
                continue;
            }
            iq_array[i]->FlushIf(match);
        }
    };

    flush_func(pe_iex_cmd_array);
    flush_func(pe_iex_alu_array);
    flush_func(pe_iex_lda_array);
    flush_func(pe_iex_sc_lda_array);
    flush_func(pe_iex_sta_array);
    flush_func(pe_iex_std_array);
    flush_func(pe_iex_bru_array);
    flush_func(pe_iex_vec_agu_array);
    flush_func(pe_iex_vec_scalar_array);
}

void DispatchUnit::Build(uint32_t peCount) {
    configs.overrideDefaultConfig(GetSim()->getCfgs());

    reservedNum = configs.iex_iq_reserve_count;

    cmdInfo.count.resize(peCount);
    aluInfo.count.resize(peCount);
    aguInfo.count.resize(peCount);
    ldaInfo.count.resize(peCount);
    ldascInfo.count.resize(peCount);
    staInfo.count.resize(peCount);
    stdInfo.count.resize(peCount);
    bruInfo.count.resize(peCount);
    scaInfo.count.resize(peCount);

    cmdInfo.maxCount = top->iexCmdIqDepth * top->iexCmdIqCount / peCount;
    aluInfo.maxCount = top->iexAluIqDepth * top->iexAluIqCount / peCount;
    aguInfo.maxCount = top->iexAguIqDepth * top->iexAguIqCount / peCount;
    ldaInfo.maxCount = top->iexLdaIqDepth * top->iexLdaIqCount / peCount;
    ldascInfo.maxCount = top->iexLdaIqDepth * top->iexLdaIqCount / peCount;
    staInfo.maxCount = top->iexStaIqDepth * top->iexStaIqCount / peCount;
    stdInfo.maxCount = top->iexStdIqDepth * top->iexStdIqCount / peCount;
    bruInfo.maxCount = top->iexBruIqDepth * top->iexBruIqCount / peCount;
    scaInfo.maxCount = top->iexScaIqDepth * top->iexScaIqCount / peCount;
}

void DispatchUnit::Reset() {
    auto array_clear = [] (vector<SimQueue<SimInst>*> &array) {
        for (auto &q : array) {
            if (q) {
                q->Reset();
            }
        }
    };
    array_clear(pe_iex_cmd_array);
    array_clear(pe_iex_alu_array);
    array_clear(pe_iex_lda_array);
    array_clear(pe_iex_sc_lda_array);
    array_clear(pe_iex_sta_array);
    array_clear(pe_iex_std_array);
    array_clear(pe_iex_bru_array);
    array_clear(pe_iex_vec_agu_array);
    array_clear(pe_iex_vec_scalar_array);

    auto info_reset = [] (DispatchInfo &info) {
        for (auto &c : info.count) {
            c = 0;
        }
        info.dispatchID = 0;
    };
    info_reset(cmdInfo);
    info_reset(aluInfo);
    info_reset(ldaInfo);
    info_reset(ldascInfo);
    info_reset(staInfo);
    info_reset(stdInfo);
    info_reset(bruInfo);
    info_reset(scaInfo);
    info_reset(aguInfo);
}


void DispatchUnit::setCancel(uint32_t pipe) {
    auto cancelQueue = [pipe](const std::deque<SimInst>& queue) {
        for (auto it = queue.begin(); it != queue.end(); it++) {
            for (auto src : (*it)->psrcs_) {
                if (src->lpvInfo != nullptr && src->lpvInfo->CheckCancel(pipe)) {
                    src->ready = false;
                    src->lpvInfo->Reset();
                }
            }
        }
    };

    auto cancelArray = [&](const std::vector<SimQueue<SimInst>*>& issueQArray) {
        for (auto &issueQ : issueQArray) {
            if (!issueQ) {
                continue;
            }
            auto &writeQ = issueQ->GetRawWriteData();
            auto &readQ = issueQ->GetRawReadData();
            cancelQueue(writeQ);
            cancelQueue(readQ);
        }
    };

    cancelArray(pe_iex_cmd_array);
    cancelArray(pe_iex_alu_array);
    cancelArray(pe_iex_lda_array);
    cancelArray(pe_iex_sc_lda_array);
    cancelArray(pe_iex_sta_array);
    cancelArray(pe_iex_std_array);
    cancelArray(pe_iex_bru_array);
    cancelArray(pe_iex_vec_agu_array);
    cancelArray(pe_iex_vec_scalar_array);
}

void DispatchUnit::ReleaseEntry(ROBID bid, ROBID rid, uint32_t stid) {
    auto match = [&bid, &rid, &stid] (SimInst &inst) -> bool {
        return (inst->bid == bid && inst->rid == rid && inst->stid == stid);
    };
    auto release = [match](const std::vector<SimQueue<SimInst>*> &vecArr) {
        for (uint32_t i = 0; i < vecArr.size(); i++) {
            if (vecArr[i] == nullptr) {
                continue;
            }
            vecArr[i]->FlushIf(match);
        }
    };
    release(pe_iex_cmd_array);
    release(pe_iex_alu_array);
    release(pe_iex_lda_array);
    release(pe_iex_sc_lda_array);
    release(pe_iex_sta_array);
    release(pe_iex_std_array);
    release(pe_iex_bru_array);
    release(pe_iex_vec_agu_array);
    release(pe_iex_vec_scalar_array);
}

} // namespace JCore
