#include "l2/MissQueue.h"

#include <cmath>
#include <cstdio>
#include <string>
#include "core/Core.h"
#include "core/Bus.h" // TODO: reorganize types used by various modules to a top level header
#include "l2/L2Cache.h"
#include "l2/L2Mem.h"
#include "SimSys.h"
#include "utils/error.h"
#include "interface/ConstConfig.h"

namespace JCore {

void MissQueue::Work() {
    stats_tick();
    // pick cache port
    // priority: writeback > lookup > replace
    uint64_t c_port = 0;
    while (c_port < config->cache_nport && !wback_list.empty()) {
        picked_wback.push_back(wback_list.front());
        wback_list.pop_front();
        c_port++;
    }
    while (c_port < config->cache_nport && !lookup_list.empty()) {
        MissQEntry *entry = lookup_list.front();
        entry->req_pkt->l2_pick_cycle = GetSim()->getCycles();

        picked_lookup.push_back(entry);
        lookup_list.pop_front();
        c_port++;
    }
    while (c_port < config->cache_nport && !replace_list.empty()) {
        picked_replace.push_back(replace_list.front());
        replace_list.pop_front();
        c_port++;
    }

    // Do cache operations
    for (auto e : picked_wback) {
        if (comb_map.count(e->addr) == 0 || (gets_map.count(e->addr) == 1 && e->share == false)) {
            bool r = l2m->writeback(e);
            if (!r) {
                bool hit = false;
                // Data bypass
                // Scan evict_q
                for (auto &t : evict_q) {
                    if (t.addr == e->addr) {
                        hit = true;
                        if (e->dirty) {
                            t.dirty = true;
                            t.data = e->data;
                        }
                        if (!e->update) {
                            if (config->l1d_l2_inclusion_policy != "WI") {
                                ASSERT(t.hold_set.count(e->src) == 1);
                                t.hold_set.erase(e->src);
                            }
                        }
                    }
                }
                // Scan evict entry
                if (!hit) {
                    if (evict_map.count(e->addr) > 0) {
                        ASSERT(evict_map.count(e->addr) == 1);
                        MissQEntry* t = evict_map[e->addr];
                        if (e->dirty) {
                            t->dirty = true;
                            t->data = e->data;
                        }
                        if (!e->update) {
                            if (config->l1d_l2_inclusion_policy != "WI") {
                                ASSERT(t->dst_set.count(e->src) == 1);
                                t->dst_set.erase(e->src);
                            }
                        }
                    } else {
                        sendMemReq(e->idx, e->addr, true, e->pref | e->req_pkt->hasPref(), false, &e->data, e->peId);
                        if (verbose) {
                            info("write through  0x%lx", e->addr);
                        }
                        if (verbose) {
                            LOG_DEBUG(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX,
                                      LogLevel::CRITICAL, "[L2] MissQueue. write through  0x%lx", e->addr);
                        }
                        if (!(e->pref || e->req_pkt->hasPref())) {
                            e->checkseq = false;
                            if (prefQ.count(e->addr) != 0) {
                                prefQ.erase(e->addr);
                            }
                            update_state(e, S_REPLACE);
                        } else {
                            update_state(e, S_EMPTY);
                        }
                    }
                }
            }
        }
    }
    if (config->l1d_l2_inclusion_policy == "WI") {
        for (auto it = picked_wback.begin(); it != picked_wback.end();) {
            if ((*it)->state != S_WBACK) {
                if (verbose) {
                    info("clear empty entry in picked_wback 0x%lx", (*it)->addr);
                }
                if (verbose) {
                    LOG_DEBUG(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                              "[L2] MissQueue. clear empty entry in picked_wback 0x%lx", (*it)->addr);
                }
                it = picked_wback.erase(it);
            } else {
                it++;
            }
        }
    }
    for (auto e : picked_lookup) {
        e->hit = l2m->lookup(e);
        e->lookuped = true;
    }
    for (auto e : picked_replace) {
        L2Entry r_e;
        l2m->replace(e, &r_e);
        stats->interval_l2_replace_count++;
        if (stats->interval_l2_replace_count % config->dpref_feedback_interval == 0) {
            top->updateL2Prefetcher = true;
        }
        setPollFilter(e->addr, false);
        if (r_e.valid) {
            if (r_e.prefetch & !r_e.demand) {
                // Bad prefetch
                if (r_e.pref_type == PFTYPE_STRIDE) {
                    stats->pref_stride_bad_count++;
                } else if (r_e.pref_type == PFTYPE_STREAM) {
                    stats->pref_stream_bad_count++;
                } else if (r_e.pref_type == PFTYPE_SW_L1D || r_e.pref_type == PFTYPE_SW_L1I || r_e.pref_type == PFTYPE_SW_L2) {
                    stats->pref_sw_pref_bad_count++;
                } else {
                    ASSERT(0);
                }
            }

            if (r_e.hold_set.size() != 0) {
                evict_q.push_back(r_e);
            } else if (r_e.dirty) {
                // Writeback to memory
                sendMemReq(e->idx, r_e.addr, true, e->pref | e->req_pkt->hasPref(), false, &r_e.data, e->peId);
            }
            // Update pollution filter when cacheline is replaced by perfetch
            if (e->pref) {
                setPollFilter(r_e.addr, true);
            }
        }
        if (verbose) {
            info("picked_replace: addr 0x%lx, []=", r_e.addr);
            std::for_each(r_e.hold_set.begin(), r_e.hold_set.end(), [](const int &i) {printf("%d ", i);});
            printf("\n");
        }
        if (verbose) {
            std::string info;
            for (auto &i : r_e.hold_set) {
                info += std::to_string(i) + " ";
            }
            LOG_DEBUG(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                      "[L2] MissQueue. picked_replace: addr 0x%lx, []= %s", r_e.addr, info.c_str());
        }
    }

    while (!resp_list.empty()) {
        picked_resp.push_back(resp_list.front());
        resp_list.pop_front();
    }
    while (!snp_list.empty()) {
        picked_inv.push_back(snp_list.front());
        snp_list.pop_front();
    }
    while (!evict_list.empty()) {
        picked_evict.push_back(evict_list.front());
        evict_list.pop_front();
    }

    uint64_t normalCnt = config->missq_depth - free_list.size() - cur_pref_count +
                          l1pkt_q.size() - cur_pref_count_in_l1q;
    GetSim()->l2Free = (free_list.size() == config->missq_depth || normalCnt == 0);
}

void MissQueue::update_state(MissQEntry *e, MissQState s, bool tolist) {
    if (verbose) {
        info("%s: addr 0x%lx idx %d from %s to %s. tolist = %d. src = %d\n", __func__, e->addr, e->idx, state_str(e->state), state_str(s), tolist, e->src);
    }
    if (verbose) {
        LOG_DEBUG(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                  "[L2] MissQueue. %s: addr 0x%lx idx %d from %s to %s. tolist = %d. src = %d",
                  __func__, e->addr, e->idx, state_str(e->state), state_str(s), tolist, e->src);
    }
    e->state = s;
    if (!tolist) return;
    switch (s) {
    case S_EMPTY:
        if (config->sanity_check) pending_list.remove(e);
        e->checkPref = false;
        free_list.push_back(e);
        if (e->pref) {
            ASSERT(cur_pref_count>0);
            cur_pref_count --;
        }
        break;
    case S_LOOKUP:
        lookup_list.push_back(e);
        break;
    case S_MISS:
        break;
    case S_REPLACE:
        replace_list.push_back(e);
        break;
    case S_SNP:
        snp_list.push_back(e);
        ASSERT(comb_map.count(e->addr) == 0);
        comb_map.emplace(e->addr, e);
        if (!e->write) gets_map.emplace(e->addr, e);
        break;
    case S_RESP:
        resp_list.push_back(e);
        break;
    case S_WBACK:
        wback_list.push_back(e);
        break;
    case S_EVICT:
        evict_list.push_back(e);
        ASSERT(comb_map.count(e->addr) == 0);
        comb_map.emplace(e->addr, e);
        break;
    default:
        fprintf(stderr, "Error: %s. Unexpected state %d\n", __func__, s);
    }
}

bool MissQueue::full() {
    return free_list.empty();
}

bool MissQueue::pfl2Full() {
    return (full() || prefStall());
}

bool MissQueue::prefStall() {
    return (cur_pref_count + cur_pref_count_in_l1q + config->deny_l2_pref_thr >= config->missq_depth);
}

bool MissQueue::stall() {
    return l1pkt_q.size() >= free_list.size();
}

bool MissQueue::normalReqFull() {
    if (stats->interval_dprefl2_to_mem_count * config->pref_accurary_per_1 / 100 > stats->interval_dprefl2_useful_count) {
        return full();
    }
    // Intervel prefetch accuracy more than 75%, reserve for prefetch
    uint64_t normal_cnt = config->missq_depth - free_list.size() - cur_pref_count + l1pkt_q.size() - cur_pref_count_in_l1q;
    return (full() || normal_cnt + checkPrefReserve() >= config->missq_depth);
}

bool MissQueue::normalStall() {
    uint64_t normal_cnt = config->missq_depth - free_list.size() - cur_pref_count + l1pkt_q.size() - cur_pref_count_in_l1q;
    return (normal_cnt + checkPrefReserve() >= config->missq_depth);
}

uint64_t MissQueue::checkPrefReserve(void) {
    uint64_t reserve_cnt = 0;
    if (stats->interval_dprefl2_to_mem_count != 0) {
        if (stats->interval_dprefl2_useful_count >= stats->interval_dprefl2_to_mem_count * config->pref_accurary_per_3 / 100) {
            reserve_cnt = config->deny_l2_normal_thr_3;
        } else if (stats->interval_dprefl2_useful_count >= stats->interval_dprefl2_to_mem_count * config->pref_accurary_per_2 / 100) {
            reserve_cnt = config->deny_l2_normal_thr_2;
        } else if (stats->interval_dprefl2_useful_count >= stats->interval_dprefl2_to_mem_count * config->pref_accurary_per_1 / 100) {
            reserve_cnt = config->deny_l2_normal_thr_1;
        }
    }
    return reserve_cnt;
}

void MissQueue::pushL1Pkt(PtrPacket pkt) {
    pkt->l2_inputQ_cycle = GetSim()->getCycles();
    l1pkt_q.push_back(pkt);
    if (pkt->isPref()) {
        ASSERT(cur_pref_count_in_l1q < l1pkt_q.size());
        cur_pref_count_in_l1q++;
        prefQ.emplace(pkt->addr, pkt->addr);
    }
}

void MissQueue::pushRespPkt(PtrPacket pkt) {
    snp_resp_q.push_back(pkt);
}

bool MissQueue::getL2Pkt(PtrPacket &pkt) {
    if (l2pkt_q.empty()) return false;

    pkt = l2pkt_q.front();
    l2pkt_q.pop_front();
    return true;
}

uint32_t MissQueue::getLsuTypeIndex(LSUType lsuTypeId)
{
    uint32_t typeIndex = 0;
    switch (lsuTypeId) {
        case LSUType::SCALAR_LSU:
            typeIndex = 0;
            break;
        case LSUType::VECTOR_LSU:
            typeIndex = 1;
            break;
        case LSUType::MEMORY_LSU:
            typeIndex = 2;
            break;
        default:
            ASSERT(0);
            break;
    }
    return typeIndex;
}

void MissQueue::sendMemReq(int tid, uint64_t addr, bool write, bool pref, bool inst, uint512_t *pdata, uint32_t peid) {
    GFUMemReq req;
    req.vld = true;
    req.lsuTypeId = lsuTypeId;
    if (peid == sim->core->GetMtcPeID()) {
        req.lsuTypeId = LSUType::MEMORY_LSU;
    }
    uint32_t typeIndex = getLsuTypeIndex(req.lsuTypeId);
    req.tid = (typeIndex << TID_TYPE_OFFSET) + (uint32_t)tid;
    req.size = 64;
    req.prefetch = pref;
    req.is_store = write;
    req.is_inst  = inst;
    req.addr = addr;
    req.requestCycle = sim->getCycles();
    if (write) {
        pdata->copyTo(req.data);
    }
    if ((m_vCoreReq || m_transferReq) && top->l2SocReqQ != nullptr) {
        req.transferReq = m_transferReq;
        req.vCoreReq = m_vCoreReq;
        top->l2SocReqQ->Write(req);
        m_transferReq = false;
        m_vCoreReq = false;
        return;
    }
    l2_mem_q->Write(req);
    LOG_INFO_M(Unit::LSU, Stage::NA) << "L2 MissQ send memory req. addr: 0x" << hex << addr << ". inst " << boolalpha
                                     << inst << " . tid: 0x" << req.tid << (write ? pdata->toStr() : "(NULL)");
    if (verbose) {
        info("send memory req. addr 0x%lx. write %d. inst %d. tid 0x%x. data %s\n", addr, write, inst, req.tid, write ? (pdata->toStr()) : "(null)");
    }
    if (verbose) {
        LOG_DEBUG(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                  "[L2] MissQueue. send memory req. addr 0x%lx. write %d. inst %d. tid 0x%x. data %s",
                  addr, write, inst, req.tid, write ? (pdata->toStr()) : "(null)");
    }
}

void MissQueue::handleL1Pkt() {
    uint64_t nreq = 0;

    // Handle evict
    while (nreq < config->input_nport && !evict_q.empty() && !free_list.empty()) {
        L2Entry &l2e = evict_q.front();
        // Allocate and init new entry
        MissQEntry* e = allocEntry();
        e->addr = l2e.addr;
        e->data = l2e.data;
        e->dirty = l2e.dirty;
        e->dst_set = l2e.hold_set;

        e->wait = false;
        if (miss_filter.count(e->addr) == 0) {
            // no conf
            std::deque<MissQEntry*> wait_list = {e};
            miss_filter.emplace(e->addr, wait_list);
        } else {
            auto &wait_list = miss_filter[e->addr];
            auto head = wait_list.front();
            if (head->lookuped) {
                // Insert second
                auto it = wait_list.begin();
                it++;
                wait_list.insert(it, e);
                e->wait = true;
            } else {
                // Insert first
                auto first = wait_list.begin();
                wait_list.insert(first, e);
                head->wait = true;

                // Delete head if already in lookup_list
                for (auto it = lookup_list.begin(); it != lookup_list.end(); it++) {
                    if (*it == head) {
                        lookup_list.erase(it);
                        break;
                    }
                }
            }
            if (!head->lookuped && verbose) {
                info("%s: addr 0x%x is delayed by evict. idx = %d\n", __func__, head->addr, head->idx);
            }
            if (!head->lookuped && verbose) {
                LOG_DEBUG(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                          "[L2] MissQueue. %s: addr 0x%x is delayed by evict. idx = %d",
                          __func__, head->addr, head->idx);
            }
        }

        update_state(e, S_EVICT, !e->wait);
        evict_map.emplace(e->addr, e);
        evict_q.pop_front();
        if (e->pref) {
            ASSERT(cur_pref_count<=config->missq_depth);
            cur_pref_count ++;
        }
        nreq++;
    }

    // Handle: invalidate resp, getshare resp
    while (!snp_resp_q.empty()) {
        PtrPacket resp = snp_resp_q.front();
        snp_resp_q.pop_front();
        ASSERT((resp->addr & 0x3f) == 0);
        ASSERT(resp->isResp());
        uint64_t addr = resp->addr;
        bool dirty = resp->isDirty();
        // The request should not update cache
        doWBCombine(addr, resp->id, dirty, &resp->data, true);
    }

    auto getL1Req = [this](std::deque<PtrPacket> &l1_q)->PtrPacket {
        PtrPacket pkt = nullptr;
        auto pick_it = l1_q.begin();
        pkt = *pick_it;
        if (pkt->isPref()) {
            ASSERT(cur_pref_count_in_l1q > 0);
            cur_pref_count_in_l1q--;
        }
        l1_q.erase(pick_it);
        return pkt;
    };

    // Handle input: read req, write req, upgrade req, writeback
    while (nreq < config->input_nport && !l1pkt_q.empty() && !free_list.empty()) {
        PtrPacket pkt = getL1Req(l1pkt_q);
        pkt->l2_recv_cycle = GetSim()->getCycles();
        stats->l1_l2_input_reqs++;
        stats->l1_l2_input_latency += (GetSim()->getCycles() - pkt->l1_out_cycle);
        ASSERT((pkt->addr & 0x3f) == 0);

        bool dirty = pkt->isDirty();
        bool share = pkt->isShare();
        bool writeback = pkt->isWriteBack() || pkt->isResp();

        // Fast writeback combine
        if (writeback) {
            uint64_t addr = pkt->addr;
            // The request should not update cache
            if (comb_map.count(addr) != 0 && !(gets_map.count(addr) != 0 && share == false)) {
                doWBCombine(addr, pkt->id, dirty, &pkt->data, false);
                nreq++;
                continue;
            }
        }

        MissQEntry *e = allocEntry();
        // Init info
        e->src     = pkt->id;
        e->addr    = pkt->addr & (uint64_t)~0x3f;
        e->dirty   = dirty;
        e->write   = pkt->isWrite();
        e->share   = share;
        e->upgrade = pkt->isUpgrade();
        e->dup     = false;
        e->tid     = pkt->tid;
        e->inst    = pkt->isInst();
        e->pref    = pkt->isPref();
        e->req_pkt = pkt;
        e->alloc_cycle = GetSim()->getCycles();
        e->transferReq = pkt->transferReq;
        e->vCoreReq = pkt->vCoreReq;
        e->coreId = pkt->coreId;
        e->reqId = pkt->reqId;
        e->peId = pkt->peId;
        e->stid = pkt->stid;
        if (config->sanity_check) {
            pending_list.push_back(e);
        }
        if (writeback && dirty) {
            e->data = pkt->data;
        }
        if (writeback) {
            update_state(e, S_WBACK);
        } else {
            checkSeq(e, pkt->addr);
            if (e->wait) {
                e->req_pkt->l2_hit_missfilter_cycle = GetSim()->getCycles();
            }
            update_state(e, S_LOOKUP, !e->wait);
        }
        nreq++;

        if (pkt->isPref()) {
            pref_set.insert(e->addr);
        } else if (pkt->isRead()) {
            if (pref_set.count(pkt->addr)) {
                stats->pfl2_late_count++;
                pref_set.erase(pkt->addr);
            }
        }

        // PMU
        if (writeback) {
            stats->writeback_req_count++;
        } else if (e->upgrade) {
            stats->upgrade_req_count++;
        } else if (e->write) {
            stats->write_req_count++;
        } else {
            stats->read_req_count++;
        }
        if (e->pref) {
            ASSERT(cur_pref_count<=config->missq_depth);
            cur_pref_count ++;
        }
    }
}

void MissQueue::handleLookup() {
    for (auto e : picked_lookup) {
        uint64_t stime = e->req_pkt->l2_miss ? e->req_pkt->l2MissCycle : e->req_pkt->l2_recv_cycle;
        stats->l2_pick_latency += (GetSim()->getCycles() - stime);
        stats->l2_pick_reqs++;
        e->req_pkt->l2_issue_cycle = GetSim()->getCycles();
        if (e->hit) {
            if (e->pref) {
                stats->pref_hit_count++;
                update_state(e, S_EMPTY);
                freeSeq(e);
                pref_set.erase(e->addr);
            } else {
                if (e->dst_set.size() == 0) {
                    update_state(e, S_RESP);
                } else {
                    update_state(e, S_SNP);
                }
            }
        } else {
            update_state(e, S_MISS);
            m_transferReq = e->transferReq;
            m_vCoreReq = e->vCoreReq;
            sendMemReq(e->idx, e->addr, false, e->pref|e->req_pkt->hasPref(), e->inst, nullptr, e->peId);
            ASSERT(e->idx != -1);
            if (e->pref) {
                stats->interval_dprefl2_to_mem_count++;
            }
            e->req_pkt->l2_miss = true;
            e->req_pkt->l2_miss_due_pref = lookupPollFilter(e->addr);
            // PMU
            if (e->inst) {
                stats->inst_miss_count++;
            } else {
                if (e->pref) {
                    stats->pref_miss_count++;
                } else {
                    stats->interval_dmd_miss_count++;
                    if (e->req_pkt->l2_miss_due_pref) {
                        stats->interval_dmd_miss_due_pref_count++;
                    }
                    if (e->req_pkt->hasPref()) {
                        stats->data_pref_miss_count++;
                    } else {
                        stats->data_dmd_miss_count++;
                        if (e->req_pkt->isRead()) {
                            stats->dmd_load_miss_count++;
                        }
                    }
                }
            }
        }
    }
}

void MissQueue::handleReplace() {
    while (!picked_replace.empty()) {
        MissQEntry *e = picked_replace.front();
        picked_replace.pop_front();
        ASSERT(e->state == S_REPLACE);
        if (e->pref) {
            update_state(e, S_EMPTY);
            freeSeq(e);
        } else {
            update_state(e, S_RESP);
        }
    }
}

PtrPacket getRespPkt(uint8_t cmd, int tid)
{
    PtrPacket pkt;
    if (cmd == Packet::ReadReq) {
        pkt = Packet::createPkt(Packet::ReadResp, tid);
    } else if (cmd == Packet::WriteReq) {
        pkt = Packet::createPkt(Packet::WriteResp, tid);
    } else if (cmd == Packet::UpgradeReq) {
        pkt = Packet::createPkt(Packet::UpgradeResp, tid);
        pkt->setExcl();
    } else if (cmd == Packet::GetShareReq) {
        pkt = Packet::createPkt(Packet::GetShareResp, tid);
    } else if (cmd == Packet::Prefetch) {
        pkt = Packet::createPkt(Packet::Prefetch, tid);
    } else {
        ASSERT(0 && "not support");
    }
    return pkt;
}

void MissQueue::handleMemResp() {
    ASSERT(!mem_l2_q->Empty());
    GFUMemReq resp = mem_l2_q->Read();
    uint32_t typeIndex = getLsuTypeIndex(resp.lsuTypeId);
    ASSERT(resp.tid >= (typeIndex << TID_TYPE_OFFSET));
    resp.tid = (resp.tid - (typeIndex << TID_TYPE_OFFSET));
    // MTCcore 非inst的直接回给L1, 不走L2.  //GFUMemReq 加个标记 bypassL2
    if (resp.bypassL2) {
        ASSERT(resp.cmd == Packet::ReadReq);
        PtrPacket pkt = getRespPkt(resp.cmd, resp.tid);
        pkt->addr = resp.addr;
        pkt->size = resp.size;
        assert(pkt->size == 256);
        pkt->tid = resp.tid;
        pkt->mtc_data.copyFrom(resp.data);
        top->l2_l1_q->Write(pkt);
        if (verbose) {
            info("[L2] MissQueue. recv memory resp. addr 0x%lx, tid 0x%x, data %s\n",
                resp.addr, resp.tid, uint2048_t(resp.data).toStr());
        }
        return;
    }
    assert(resp.tid >= 0 && resp.tid < config->missq_depth);
    MissQEntry *e = &queue[resp.tid];
    ASSERT(e->state == S_MISS);
    LOG_INFO_M(Unit::LSU, Stage::NA) << "L2 Recv memory resp. addr: 0x" << hex << resp.addr << ", tid: 0x" << resp.tid
                                     << ", data: " << uint512_t(resp.data).toStr();
    if (verbose) {
        info("recv memory resp. addr 0x%lx, tid 0x%x, data %s\n", resp.addr, resp.tid, uint512_t(resp.data).toStr());
    }
    if (verbose) {
        LOG_DEBUG(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                  "[L2] MissQueue. recv memory resp. addr 0x%lx, tid 0x%x, data %s",
                  resp.addr, resp.tid, uint512_t(resp.data).toStr());
    }

    e->data = resp.data;
    e->req_pkt->l2MissCycle = resp.requestCycle;
    e->req_pkt->memRntCycle = sim->getCycles();
    stats->l2_mem_latency += (GetSim()->getCycles() - e->req_pkt->l2MissCycle);
    stats->l2_mem_reqs++;
    if (!resp.is_store) {
        stats->l2_mem_read_reqs++;
        stats->soc_RD_size += resp.size;
    } else {
        stats->l2_mem_write_reqs++;
        stats->soc_WR_size += resp.size;
    }

    stats->soc_latency += resp.socReturnCycle - resp.socAaccelptCycle;
    bool preft = e->pref;
    if ((preft && prefQ.count(resp.addr) != 0) || !preft) {
        update_state(e, S_REPLACE);
    } else if (preft && prefQ.count(resp.addr) == 0) {
        freeSeq(e);
    }

    if (e->pref) {
        pref_set.erase(e->addr);
    }
}

void MissQueue::HandleL2Pkt() {
    uint64_t npkt = 0;
    MissQEntry *e;

    // Response
    while (npkt < config->l2l1_nport && !picked_resp.empty()) {
        e = picked_resp.front();
        picked_resp.pop_front();
        PtrPacket pkt;
        if (e->upgrade) {
            pkt = Packet::createUpgradeResp(e->tid);
        } else {
            pkt = Packet::createRWResp(e->write, e->excl, e->tid);
        }
        pkt->id   = e->src;
        pkt->addr = e->addr;
        pkt->data = e->data;
        pkt->size = 64;
        pkt->l2_miss = e->req_pkt->l2_miss;
        pkt->getPktCycle(e->req_pkt);
        pkt->l2RntCycle = sim->getCycles();
        pkt->transferReq = e->transferReq;
        pkt->vCoreReq = e->vCoreReq;
        pkt->coreId = e->coreId;
        pkt->reqId = e->reqId;
        pkt->stid = e->stid;
        if (pkt->l2_pref_late) {
            stats->interval_dprefl2_late_count++;
        }
        if (pkt->l2_pref_useful) {
            stats->interval_dprefl2_useful_count++;
        }
        // Statistics Latency of pkt that L2 response to L1.
        stats->resp_l1_reqs++;
        stats->resp_l1_latency += (GetSim()->getCycles() - pkt->l1_out_cycle);
        stats->resp_l1_l2_input_latency += (pkt->l2_recv_cycle - pkt->l1_out_cycle);
        stats->resp_l2_queue_full_latency += (pkt->l2_inputQ_cycle - pkt->l1_out_cycle);
        stats->resp_l2_input_port_latency += (pkt->l2_recv_cycle - pkt->l2_inputQ_cycle);
        stats->resp_l2_pick_latency += (pkt->l2_issue_cycle - pkt->l2_recv_cycle);
        if (pkt->l2_missfilter_wakeup_cycle > 0) {
            stats->resp_l2_wait_missq_latency += (pkt->l2_missfilter_wakeup_cycle - pkt->l2_hit_missfilter_cycle);
            stats->resp_l2_cache_port_latency += (pkt->l2_pick_cycle - pkt->l2_missfilter_wakeup_cycle);
        } else {
            stats->resp_l2_cache_port_latency += (pkt->l2_pick_cycle - pkt->l2_recv_cycle);;
        }
        if (pkt->memRntCycle > 0) {
            stats->resp_l2_miss_latency += (pkt->memRntCycle - pkt->l2MissCycle);
            stats->resp_l2_latency += (GetSim()->getCycles() - pkt->memRntCycle);
        } else {
            if (pkt->l2_snp_wakeup > 0) {
                stats->resp_l2_wait_snp_latency += (pkt->l2_snp_wakeup - pkt->l2_wait_snp);
                stats->resp_l2_latency += (GetSim()->getCycles() - pkt->l2_snp_wakeup);
            } else {
                stats->resp_l2_latency += (GetSim()->getCycles() - pkt->l2_issue_cycle);
            }
        }

        if (e->req_pkt->hasPref()) {
            pkt->setPref();
            pkt->user_type = e->req_pkt->user_type;
        }
        if (!pkt->isUpgrade() && e->dup) {
            pkt->setDup();
        }
        l2pkt_q.push_back(pkt);

        update_state(e, S_EMPTY);
        freeSeq(e);
        if (verbose) {
            info("resp packet to %d. ", pkt->id);
            cout << *pkt<<" "<<pkt->transferReq;
            printf("\n");
        }
        if (verbose) {
            LOG_DEBUG_STRUCT(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                             *pkt, "[L2] MissQueue. resp packet to %d. pkt->transferReq: %d, pkt: ",
                             pkt->id, pkt->transferReq);
        }
        npkt++;
    }

    // Inv
    while (npkt < config->l2l1_nport && !picked_inv.empty()) {
        e = picked_inv.front();
        picked_inv.pop_front();
        if (e->dst_set.size() != 0) {
            for (auto dst : e->dst_set) {
                PtrPacket pkt;
                if (e->write || e->upgrade) {
                    pkt = Packet::createInvReq(e->tid);
                } else {
                    pkt = Packet::createGetSReq(e->tid);
                }
                pkt->addr = e->addr;
                pkt->id   = dst;
                pkt->l2MissCycle = 0;
                pkt->memRntCycle = 0;
                pkt->l2RntCycle = sim->getCycles();
                pkt->stid = e->stid;
                l2pkt_q.push_back(pkt);
                if (verbose) {
                    info("entry %d send packet:", e->idx);
                    cout << *pkt << "\n";
                }
                if (verbose) {
                    LOG_DEBUG_STRUCT(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                                     *pkt, "[L2] MissQueue. entry %d send packet: ", e->idx);
                }
            }
            npkt++;
        }
    }

    // Evict
    while (npkt < config->l2l1_nport && !picked_evict.empty()) {
        e = picked_evict.front();
        picked_evict.pop_front();
        if (e->dst_set.size() != 0) {
            for (auto dst : e->dst_set) {
                PtrPacket pkt = Packet::createInvReq(e->tid);
                pkt->addr = e->addr;
                pkt->id   = dst;
                pkt->l2MissCycle = 0;
                pkt->memRntCycle = 0;
                pkt->stid = e->stid;
                l2pkt_q.push_back(pkt);
                if (verbose) {
                    info("entry %d send packet:", e->idx);
                    cout << *pkt << "\n";
                }
                if (verbose) {
                    LOG_DEBUG_STRUCT(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                                     *pkt, "[L2] MissQueue. entry %d send packet:", e->idx);
                }
            }
            npkt++;
        }
        // size == 0 means wb has already finished
    }
}

void MissQueue::Reset() {
    for (uint64_t i = 0; i < config->missq_depth; i++) {
        queue[i].state = S_EMPTY;
        queue[i].idx = i;
        queue[i].wait = false;
    }
    free_list.clear();
    resp_list.clear();
    lookup_list.clear();
    replace_list.clear();
    snp_list.clear();
    comb_map.clear();
    gets_map.clear();
    picked_lookup.clear();
    picked_replace.clear();
    picked_resp.clear();
    picked_inv.clear();
    l1pkt_q.clear();
    l2pkt_q.clear();
    snp_resp_q.clear();
    pending_list.clear();

    for (uint64_t i = 0; i < config->missq_depth; i++) {
        free_list.push_back(&queue[i]);
    }
    cur_pref_count = 0;
    cur_pref_count_in_l1q = 0;
}

void MissQueue::Xfer() {
    if (!picked_resp.empty() || !picked_inv.empty() || !picked_evict.empty()) {
        HandleL2Pkt();
    }
    if (!picked_lookup.empty()) {
        handleLookup();
    }

    if (!l1pkt_q.empty() || !evict_q.empty() || !snp_resp_q.empty()) {
        // read req, write req, upgrade req, writeback, invalidate resp, getshare resp
        handleL1Pkt();
    }
    if (!picked_wback.empty()) {
        handleWBack();
    }
    if (!picked_replace.empty()) {
        handleReplace();
    }
    if (!mem_l2_q->Empty()) {
        handleMemResp();
    }
    if (top->socL2RspQ != nullptr && !top->socL2RspQ->Empty()) {
        GFUMemReq resp = top->socL2RspQ->Read();
        ASSERT(resp.tid >= 0 && resp.tid < config->missq_depth);
        MissQEntry *e = &queue[resp.tid];
        ASSERT(e->state == S_MISS);
        if (verbose) {
            info("recv memory resp. addr 0x%lx,tid %d, transId %d, is_store=%d.\n",
                resp.addr, resp.tid, resp.transId, resp.is_store);
        }
        if (verbose) {
            LOG_DEBUG(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                      "[L2] MissQueue. recv memory resp. addr 0x%lx, tid 0x%x", resp.addr, resp.transId);
        }
        e->data = resp.data;
        update_state(e, S_REPLACE);
    }

    picked_lookup.clear();
}

SimSys* MissQueue::GetSim() {
    return sim;
}

void MissQueue::Build() {
    verbose = config->verbose;

    for (uint64_t i = 0; i < config->missq_depth; i++) {
        queue.push_back(MissQEntry());
    }
    poll_filter.resize(config->cache_poll_filter_depth, false);
    uint64_t low_width = config->cache_poll_filter_bit1 - config->cache_poll_filter_bit0 + 1;
    uint64_t high_width = config->cache_poll_filter_bit3 - config->cache_poll_filter_bit2 + 1;
    ASSERT(low_width == high_width && low_width == log2(config->cache_poll_filter_depth));
    low_mask = pow(2, config->cache_poll_filter_bit1 + 1) - pow(2, config->cache_poll_filter_bit0);
    high_mask = pow(2, config->cache_poll_filter_bit3 + 1) - pow(2, config->cache_poll_filter_bit2);


}

MissQueue::MissQEntry* MissQueue::allocEntry(void) {
    ASSERT(!free_list.empty());
    // Get free entry from free_list
    MissQEntry *e = free_list.front();
    free_list.pop_front();
    ASSERT(e->state == S_EMPTY);
    e->update = false;
    e->share  = false;
    e->lookuped = false;
    e->dst_set.clear();
    e->checkseq = true;
    return e;
}

void MissQueue::handleWBack(void) {
    while (!picked_wback.empty()) {
        MissQEntry *e = picked_wback.front();
        picked_wback.pop_front();
        if (e->state != S_WBACK) {
            cout<<"Cycle "<<dec<<GetSim()->getCycles()<<" correctBCount "<<GetSim()->correctBCount << endl;
            cout<< *e->req_pkt<<endl;
        }
        ASSERT(e->state == S_WBACK);
        if (comb_map.count(e->addr) != 0) {
            doWBCombine(e->addr, e->src, e->dirty, &e->data, false);
            update_state(e, S_EMPTY);
        } else {
            if (e->update) {
                update_state(e, S_RESP);
            } else {
                update_state(e, S_EMPTY);
            }
        }
    }
}

void MissQueue::doWBCombine(uint64_t addr, int src, bool dirty, uint512_t *pdata, bool snpResp) {
    ASSERT(comb_map.count(addr) != 0);
    if (verbose) {
        info("doWBCombine. addr 0x%lx, src %d, dirty %d, snoopResp %d\n", addr, src, dirty, snpResp);
    }
    if (verbose) {
        LOG_DEBUG(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                  "[L2] MissQueue. doWBCombine. addr 0x%lx, src %d, dirty %d, snoopResp %d", addr, src, dirty, snpResp);
    }
    MissQEntry *e = comb_map[addr];
    if (e->dst_set.count(src) == 1) { // if dst_set.count(src) = 0, as wb happen in Work()
        e->dst_set.erase(src);
        if (dirty) {
            e->data = *pdata;
            e->dirty = true;
        }
    }

    if (e->state == S_SNP) {
        if (e->dst_set.size() == 0) {
            if (!snp_list.empty()) {
                auto it = std::find(snp_list.begin(), snp_list.end(), e);
                if (it != snp_list.end()) {
                    snp_list.erase(it);
                }
            }
            if (!picked_inv.empty()) {
                auto it = std::find(picked_inv.begin(), picked_inv.end(), e);
                if (it != picked_inv.end()) {
                    picked_inv.erase(it);
                }
            }

            if (e->dirty) {
                // Data should be written to cache if dirty
                update_state(e, S_WBACK);
                e->update = true;
                if (!snpResp && e->src != src) {
                    l2m->freeSnoopFilter(e->addr, (uint32_t)src);
                }
            } else {
                update_state(e, S_RESP);
                e->req_pkt->l2_snp_wakeup = GetSim()->getCycles();
            }
            comb_map.erase(addr);
            if (!e->write) gets_map.erase(addr);
        }
    } else if (e->state == S_EVICT) {
        if (e->dst_set.size() == 0) {
            if (!evict_list.empty()) {
                auto it = std::find(evict_list.begin(), evict_list.end(), e);
                if (it != evict_list.end()) {
                    evict_list.erase(it);
                }
            }
            if (!picked_evict.empty()) {
                auto it = std::find(picked_evict.begin(), picked_evict.end(), e);
                if (it != picked_evict.end()) {
                    picked_evict.erase(it);
                }
            }

            if (e->dirty) {
                // Writeback to memory if dirty data
                sendMemReq(e->idx, e->addr, true, e->pref | e->req_pkt->hasPref(), false, &e->data, e->peId);
            }
            // Update state
            update_state(e, S_EMPTY);
            comb_map.erase(addr);
            evict_map.erase(addr);
            freeSeq(e);
        }
    } else {
        fprintf(stderr, "%s:%d: unexpected state %d\n", __func__, __LINE__, e->state);
        exit(1);
    }
}

void MissQueue::checkSeq(MissQEntry *e, uint64_t addr) {
    e->wait = false;
    if (miss_filter.count(addr) == 0) {
        std::deque<MissQEntry*> wait_list = {e};
        miss_filter.emplace(e->addr, wait_list);
    } else {
        std::deque<MissQEntry*> &wait_list  = miss_filter[addr];
        if (!e->pref) {
            for (auto it : wait_list) {
                if (it->pref) {
                    if (!it->checkPref) {
                        e->req_pkt->l2_pref_late = true;
                        it->checkPref = true;
                    }
                    break;
                }
            }
        }
        wait_list.push_back(e);
        e->wait = true;
        stats->block_seq_count++;
    }
}

void MissQueue::freeSeq(MissQEntry *e) {
    if (!e->checkseq) return;
    ASSERT(miss_filter.count(e->addr) != 0);
    auto &wait_list = miss_filter[e->addr];
    ASSERT(wait_list.front() == e);
    wait_list.pop_front();
    if (wait_list.size() == 0) {
        miss_filter.erase(e->addr);
    } else {
        auto wake_e = wait_list.front();
        wake_e->wait = false;
        wake_e->req_pkt->l2_missfilter_wakeup_cycle = GetSim()->getCycles();
        // Insert to ready list
        update_state(wake_e, wake_e->state);
    }
}

bool MissQueue::checkCombine(uint64_t addr) {
    return comb_map.count(addr) != 0;
}

void MissQueue::stats_tick(void) {
    if (full()) {
        if (verbose) {
            info("%s", "full\n");
        }
        if (verbose) {
            LOG_DEBUG(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                      "[L2] MissQueue. %s", "full");
        }
        stats->missq_full_cycles++;
    }

    if (config->sanity_check) {
        // sanity check
        if (!pending_list.empty()) {
            uint64_t cur_tick = GetSim()->getCycles();
            if ((cur_tick - pending_list.front()->alloc_cycle) > 5000UL) {
                info("%s", "pkt exists for more than 5000 cycles. ");
                cout << pending_list.front()->req_pkt;
                printf("\n");
                fflush(stdout);
                ASSERT(0);
            }
        }
    }

    // Statistics for depth of queue.
    uint32_t types = stats->depthStatsVec.size();
    uint32_t step = config->missq_depth / types;
    uint32_t occ = config->missq_depth - free_list.size();
    if (occ == 0) {
        return;
    }

    for (uint32_t i = 1; i <= types; ++i) {
        if (occ <= step * i) {
            ++stats->depthStatsVec[i - 1];
            break;
        }
    }
    stats->depthStats += occ;
    ++stats->cycles;
}

bool MissQueue::replacePref() {
    MissQEntry *delRntry = nullptr;
    uint64_t longestTime = 0;
    for (auto e : picked_lookup) {
        if (!e->pref) {
            continue;
        }
        uint64_t stime = e->req_pkt->l2_miss ? e->req_pkt->l2MissCycle : e->req_pkt->l2_recv_cycle;
        uint64_t duration = GetSim()->getCycles() - stime;
        if (!delRntry || duration > longestTime) {
            delRntry = e;
            longestTime = duration;
        }
    }

    if (delRntry) {
        update_state(delRntry, S_EMPTY);
        freeSeq(delRntry);
        pref_set.erase(delRntry->addr);
        return true;
    }

    return false;
}

void MissQueue::throwPref() {
    uint64_t recv_pref = cur_pref_count + cur_pref_count_in_l1q;
    uint64_t pref_thr =  config->missq_depth * config->pref_l2_throw_thr / 100;
    if (recv_pref <= pref_thr) {
        return;
    }

    uint64_t pref_throw_count = 0;
    for (auto it = l1pkt_q.begin(); it != l1pkt_q.end(); ) {
        if (recv_pref - pref_throw_count <= pref_thr) {
            return;
        }
        if ((*it)->isPref()) {
            it = l1pkt_q.erase(it);
            ASSERT(cur_pref_count_in_l1q>0);
            cur_pref_count_in_l1q--;
            pref_throw_count++;
        } else {
            it++;
        }
    }

    for (auto e : picked_lookup) {
        if (recv_pref - pref_throw_count <= pref_thr) {
            return;
        }
        if (!e->pref) {
            continue;
        }
        update_state(e, S_EMPTY);
        freeSeq(e);
        pref_set.erase(e->addr);
        pref_throw_count++;
    }
}

uint64_t MissQueue::getPollFilterIndex(uint64_t addr)
{
    uint64_t low_data = (addr & low_mask) >> config->cache_poll_filter_bit0;
    uint64_t high_data = (addr & high_mask) >> config->cache_poll_filter_bit2;
    uint64_t index = low_data ^ high_data;
    return index;
}

void MissQueue::setPollFilter(uint64_t addr, bool dst)
{
    poll_filter[getPollFilterIndex(addr)] = dst;
}

bool MissQueue::lookupPollFilter(uint64_t addr)
{
    return poll_filter[getPollFilterIndex(addr)];
}

} // namespace JCore