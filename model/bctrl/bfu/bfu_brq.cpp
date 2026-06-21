#include "bctrl/bfu/bfu_brq.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <tuple>

#include "../configs/bfu_config.h"
#include "bctrl/bfu/bfu.h"
#include "bctrl/bfu/bfu_utils.h"
#include "core/Core.h"

namespace JCore {

using namespace std;

namespace NS_CORE {

namespace {

bool IsRecoverableStart(PtrMachineInst const& inst, seq_t hid)
{
    if (!inst || !inst->bfuInfo || !inst->bfuInfo->spInfo || !inst->bfuInfo->vld ||
        inst->bfuInfo->hid != hid) {
        return false;
    }
    return !inst->bfuInfo->spInfo->isInst || inst->opcode == Opcode::OP_BSTART;
}

bool IsFBIDOlder(seq_t fbid, seq_t fbid_local, seq_t ref_fbid, seq_t ref_fbid_local)
{
    return fbid < ref_fbid || (fbid == ref_fbid && fbid_local < ref_fbid_local);
}

} // namespace

bool BRQ::push(PtrFB const& fb) {
    int vld_mhdr_cnt = 0;

    // push MHdrs to backend
    // (TODO: add a BHQ)
    pos_t pos_start = utils->CalcPosInFB(fb->va);
    pos_t pos_taken = (fb->main_info.taken || fb->end) ? fb->main_info.end_pos : BFU_BANDWIDTH;
    assert(pos_start <= pos_taken);
    uint32_t stid = fb->stid;
    PtrMachineInst h = stashH[stid];
    // mark the first machineInst redirect
    // push from FB start position to the taken position
    bool mispredRoute = false;
    if (cfg->debug_enable) {
        LOG_INFO_M(Unit::BFU, Stage::NA) << "BRQ push fb, fbid=" << std::dec << fb->fbid << ", fbid_local="
                                         << fb->fbid_local << ", stid=" << std::dec << fb->stid;
    }
    auto deliverBStop = [this, &vld_mhdr_cnt, stid](bool needBstop, PtrFB const& fb, pos_t pos) {
        if (!needBstop) {
            return;
        }
        stashH[stid]->bfuInfo->fetch_end = true;
        stashH[stid]->bfuInfo->spInfo->hasBend = true;
        SimInst bstop = utils->GenBstop(stashH[stid]);
        bstop->bfuInfo->fbid = fb->fbid;
        bstop->bfuInfo->fbid_local = fb->fbid_local;
        bstop->pc = fb->machineInst[pos]->pc;
        vld_mhdr_cnt++;
        bfu->DeliverInst(bstop);
    };
    for (pos_t pos = pos_start; pos < BFU_BANDWIDTH; pos++) {
        if (utils->MhdrInvalid(fb->machineInst[pos])) {
            continue;
        }
        if (cfg->debug_enable) {
            auto &inst = fb->machineInst[pos];
            LOG_INFO_M(Unit::BFU, Stage::NA) << "stashH: " << (stashH[stid] ? stashH[stid]->Dump() : "nullptr");
            if (stashH[stid]) {
                LOG_INFO_M(Unit::BFU, Stage::NA) << "stash needs bstop=" << boolalpha << utils->BstopNeed(stashH[stid])
                                                 << ", hasBend=" << stashH[stid]->bfuInfo->spInfo->hasBend;
            }
            LOG_INFO_M(Unit::BFU, Stage::NA) << "Inst: " << inst->Dump() << ", isbstartOrBend=" << boolalpha
                                             << utils->IsBendOrBstart(inst->bfuInfo->spInfo) << ", isInst="
                                             << inst->bfuInfo->spInfo->isInst;
        }
        // come acoss bstart/bend
        if (stashH[stid] && fb->machineInst[pos]->bfuInfo->vld && utils->IsBendOrBstart(fb->machineInst[pos]->bfuInfo->spInfo)) {
            if ((!stashH[stid]->bfuInfo->nuke_after_redirect && stashH[stid]->bfuInfo->resolved &&
                stashH[stid]->bfuInfo->resolve_mispredict && !stashH[stid]->bfuInfo->predict_taken) ||
                /* 在 arbitrate 里可能会切分，设置 spInfo 的 inst 和 last 都是 true，但是 Opcode 还不变，所以这里暂时先这样屏蔽 */
                (fb->machineInst[pos]->bfuInfo->spInfo->cut && fb->machineInst[pos]->opcode != Opcode::OP_BSTOP)) {
                mispredRoute = true;
            }
        }
        if (cfg->debug_enable) {
            LOG_INFO_M(Unit::BFU, Stage::NA) << "mispredRoute=" << boolalpha << mispredRoute;
        }

        fb->machineInst[pos]->bfuInfo->predict_at_once = fb->predict_at_once;
        if (fb->machineInst[pos]->bfuInfo->spInfo->isInst) {
            if (fb->machineInst[pos]->opcode == Opcode::OP_BSTOP && stashH[stid] != nullptr && mispredRoute) {
                bfu->DeliverInst(fb->machineInst[pos]);
            }
            if (fb->machineInst[pos]->opcode == Opcode::OP_BSTOP && stashH[stid] != nullptr) {
                stashH[stid]->bfuInfo->fetch_end = true;
                stashH[stid] = nullptr;
            }
            fb->machineInst[pos]->bfuInfo->resolved = true;
            stats->fetchMinstCnt++;
        } else {
            stats->fetchHdrCnt++;
            if (stashH[stid]) {
                deliverBStop(utils->BstopNeed(stashH[stid]) || !stashH[stid]->bfuInfo->spInfo->hasBend, fb, pos);
                stashH[stid]->bfuInfo->nuke_after_redirect = false;
                stashH[stid] = nullptr;
            }
        }
        if (!fb->machineInst[pos]->bfuInfo->spInfo->isInst && !mispredRoute) {
            stashH[stid] = fb->machineInst[pos];
            if (stashH[stid]->bfuInfo->predict_taken) {
                h = stashH[stid];
            }
        } else if (!fb->machineInst[pos]->bfuInfo->spInfo->isInst) {
            stashH[stid] = nullptr;
        }
        if (!mispredRoute) {
            vld_mhdr_cnt++;
            bfu->DeliverInst(fb->machineInst[pos]);
        } else {
            fb->machineInst[pos]->bfuInfo->spInfo->isLast = true;
            fb->machineInst[pos]->bfuInfo->spInfo->isInst = true;
            fb->machineInst[pos]->bfuInfo->hid = h->bfuInfo->hid;
            fb->machineInst[pos]->bfuInfo->resolved = true;
            if (stashH[stid]) {
                deliverBStop(utils->BstopNeed(stashH[stid]) || !stashH[stid]->bfuInfo->spInfo->hasBend, fb, pos);
                stashH[stid]->bfuInfo->nuke_after_redirect = false;
            }
            if (vld_mhdr_cnt > 0) {
                ASSERT(fb->stid < brq.size());
                brq[fb->stid].push_back(fb);
                assert(brq.size() <= cfg->brq_depth);
            }
            return false;
        }

        if (pos == pos_taken) {
            // block fetch done, calculate block size
            if (fb->end && stashH[stid] && (utils->BstopNeed(stashH[stid]) ||
                                            !stashH[stid]->bfuInfo->spInfo->hasBend)) {
                stashH[stid]->bfuInfo->fetch_end = true;
                stashH[stid]->bfuInfo->spInfo->hasBend = true;
                SimInst bstop = utils->GenBstop(stashH[stid]);
                bstop->pc = utils->NextBlockPC(stashH[stid]->GetBundlePosPC()) + stashH[stid]->bfuInfo->spInfo->bsize;
                bfu->DeliverInst(bstop);
                stashH[stid] = nullptr;
            }
            break;
        }

        if (stashH[stid] && stashH[stid]->bfuInfo->resolved && stashH[stid]->bfuInfo->resolve_mispredict && !stashH[stid]->bfuInfo->nuke_after_redirect &&
            stashH[stid]->bfuInfo->hid != fb->machineInst[pos]->bfuInfo->hid && fb->machineInst[pos]->bfuInfo->vld) {
            if (cfg->debug_enable) {
                LOG_INFO_M(Unit::BFU, Stage::F4) << "brq redirect route, throw away fb fbid="
                    << dec << fb->fbid << " fbid_local=" << fb->fbid_local << ", pos=" << pos
                    << ", pos=" << pos << ", pos_taken: " << pos_taken
                    << ", end=" << fb->end << ", need stop: " << utils->BstopNeed(stashH[stid])
                    << "; resolved header: fbid=" << stashH[stid]->bfuInfo->fbid << ", fbid_local=" << stashH[stid]->bfuInfo->fbid_local
                    << ", hid=" << stashH[stid]->bfuInfo->hid << ", pc=0x" << std::hex << stashH[stid]->pc
                    << ", bundlePC=0x" << stashH[stid]->GetBundlePosPC();
            }
            logger->debug("BRQ", F4, "redirect route, throw away fb fbid=\n", fb->fbid);
            break;
        }
    }

    // push FB to BRQ
    if (vld_mhdr_cnt > 0) {
        ASSERT(fb->stid < brq.size());
        brq[fb->stid].push_back(fb);
        assert(brq[fb->stid].size() <= cfg->brq_depth);
        if (cfg->debug_enable) {
            LOG_INFO_M(Unit::BFU, Stage::F4) << dec << " push FB to BRQ, fbid=" << fb->fbid
                << ", fbid_local=" << fb->fbid_local << ", stid=" << fb->stid
                << ", va=0x" << hex << fb->va
                << dec << ", BRQ size=" << brq[fb->stid].size();
        }
        logger->debug("BRQ", F4, "push FB to BRQ, fbid=%d, fbid_local=%d, va=0x%x, size=%d\n",
                        fb->fbid, fb->fbid_local, fb->va, brq[fb->stid].size());
        if (cfg->bcache_verbose) {
            cout << "[BFU BRQ]: push FB to BRQ fbid="<<dec<<fb->fbid<<", va=0x"<<hex<<fb->va<<", brq_size"
                <<dec<<brq[fb->stid].size()<<" fb_spec_wptr:"<<dec<<fb->ras_info.spec_wptr<<", is_from_lb:"<<boolalpha<<fb->lb_info.hit<<endl;
        }
    } else {
        LOG_INFO_M(Unit::BFU, Stage::F4) << "BRQ skip concat-only FB, fbid=" << dec << fb->fbid
                                         << ", fbid_local=" << fb->fbid_local << ", va=0x" << hex << fb->va << dec
                                         << ", BRQ size=" << brq[fb->stid].size();
        logger->debug("BRQ", F4, "skip concat-only FB, fbid=%d, fbid_local=%d, va=0x%x, size=%d\n",
                        fb->fbid, fb->fbid_local, fb->va, brq[fb->stid].size());
    }

    if (h && h->bfuInfo->predict_taken && h->bfuInfo->spInfo->bsizeVld && !h->bfuInfo->spInfo->isInst) {
        PtrFB taken_fb = (*getFBByFbid(h->bfuInfo->fbid, h->bfuInfo->fbid_local, h->stid));
        pos_t taken_pos = utils->CalcPosInFB(h->GetBundlePosPC(), taken_fb->va);
        bfu->UpdateUBTB(taken_fb, taken_pos);
    }
    return true;
}

bool BRQ::ResolveHeader(PtrMachineInst const& machineInst) {
    if (!machineInst->bfuInfo->fetch_end && machineInst->bfuInfo->resolve_mispredict) {
        return false;
    }
    if (!machineInst->IsCond() && !machineInst->IsFallthrough()) {
        machineInst->bfuInfo->resolve_taken = true; // A BIG HACK! This should be done in the backend.
    }
    machineInst->bfuInfo->resolve_time = bfu->GetSim()->getCycles();
    machineInst->bfuInfo->resolved = true;
    if (machineInst->IsCond()) {
        machineInst->bfuInfo->resolve_tgt =  machineInst->bfuInfo->resolve_taken ? utils->CalcStaticTarget(machineInst) : utils->NextBlockPC(machineInst);
    }

    auto fb = *(getFBByHdr(machineInst));

    // check redirection
    if (machineInst->bfuInfo->resolve_mispredict) {
        // TODO: order redirection in the same cycle by header age
        assert(redirect_fb == nullptr);
        redirect_fb = fb;
        // If h is a header with concat, it's PC is the va of the concat block. So we need to find the real branch part.
        fb->redir_info = RedirectInfo();
        fb->redir_info.pos = utils->CalcPosInFB(machineInst->GetBundlePosPC(), fb->va);
        fb->redir_info.resolve_taken = machineInst->bfuInfo->resolve_taken;
        fb->redir_info.tgt = machineInst->bfuInfo->resolve_tgt;
        machineInst->bfuInfo->predict_taken = machineInst->bfuInfo->resolve_taken;
        if (machineInst->IsCond()) {
            fb->redir_info.tgt = machineInst->bfuInfo->resolve_taken ? utils->CalcStaticTarget(machineInst) : utils->NextBlockPC(machineInst);
        }
        machineInst->bfuInfo->resolve_tgt = fb->redir_info.tgt;
        // mark the mhdrs after the mispredict pos as invalid
        // TODO (sufang): try removing the following code
        bool bend = false;
        for (pos_t pos=fb->redir_info.pos+1; pos<BFU_BANDWIDTH; pos++) {
            if (utils->MhdrInvalid(fb->machineInst[pos])) {
                continue;
            }
            if (utils->IsBendOrBstart(fb->machineInst[pos]->bfuInfo->spInfo) && !bend) {
                bend = true;
                fb->machineInst[pos]->bfuInfo->spInfo->isInst = true;
                fb->machineInst[pos]->bfuInfo->spInfo->isLast = true;
                fb->machineInst[pos]->bfuInfo->resolved = true;
            }
            if (bend)
                fb->machineInst[pos]->bfuInfo->vld = false;
        }
        fb->recoverFB(fb->redir_info.pos);
    }

    // check if all valid headers have been resolved
    resolveFetchBundle(fb);

    addr_t tgt = machineInst->bfuInfo->resolve_mispredict ? redirect_fb->redir_info.tgt : machineInst->bfuInfo->resolve_tgt;
    if (cfg->debug_enable) {
        LOG_INFO_M(Unit::BFU, Stage::NA) << "BRQ resolve header, pc=0x" << hex << machineInst->pc
            << ", bundlePC=0x" << machineInst->GetBundlePosPC()
            << dec << ", hid=" << machineInst->bfuInfo->hid << ", fbid=" << fb->fbid << ", fbid_local=" << fb->fbid_local
            << ", mispred=" << machineInst->bfuInfo->resolve_mispredict << ", taken=" << machineInst->bfuInfo->resolve_taken
            << ", tgt=0x" << hex << tgt << dec << ", cmt_cnt=" << bfu->GetCommitCnt()
            << ", isInst: "<< machineInst->bfuInfo->spInfo->isInst;
    }
    logger->debug("BRQ", NIL, "resolve header, va=0x%x, hid=%d, fbid=%d, fbid_local=%d, mispred=%d, taken=%d, tgt=0x%x, cmt_cnt=%d\n",
                    machineInst->GetBundlePosPC(), machineInst->bfuInfo->hid, fb->fbid, fb->fbid_local, machineInst->bfuInfo->resolve_mispredict, machineInst->bfuInfo->resolve_taken, tgt, bfu->GetCommitCnt());
    if (cfg->bcache_verbose) {
        cout << "[BFU BRQ]: resolve header, va=0x"<<hex<<machineInst->GetBundlePosPC()<<", fbid="<<dec<< fb->fbid <<", rslv_tgt=0x"<<hex<<tgt;
        cout<<" fb_spec_wptr:"<<dec<<fb->ras_info.spec_wptr<<", is_from_lb:"<<boolalpha<<fb->lb_info.hit<<endl;
    }

    return true;
}

void BRQ::ReportFlush(seq_t fbid, seq_t fbid_local, uint64_t pc, uint32_t stid) {
    if (IsOlderThanFront(fbid, fbid_local, stid)) {
        if (cfg->debug_enable) {
            LOG_INFO_M(Unit::BFU, Stage::NA) << "Skip stale queued nuke, fbid=" << dec << fbid
                << ", fbid_local=" << fbid_local << ", stid=" << stid << ", pc=0x" << hex << pc;
        }
        return;
    }
    auto it = getFBByFbid(fbid, fbid_local, stid);
    while (true) {
        PtrFB fb = (*it);
        pos_t pos = fb->main_info.end_pos >= BFU_BANDWIDTH ? BFU_BANDWIDTH - 1 : fb->main_info.end_pos;
        for ( ; pos >= utils->CalcPosInFB(fb->va); --pos) {
            uint64_t cur_pc = utils->CalcPC(fb->va, pos);
            if (!utils->MhdrInvalid(fb->machineInst[pos]) && pc == utils->NextBlockPC(cur_pc)) {
                ASSERT(top->intf.be_bfu_nuke == nullptr);
                top->intf.be_bfu_nuke = fb->machineInst[pos];
                return;
            }
            if (pos == utils->CalcPosInFB(fb->va)) {
                break;
            }
        }

        ASSERT((*it)->stid < brq.size());
        if (it == brq[(*it)->stid].begin()) {
            break;
        }
        --it;
    }
    cerr<<dec<<"fbid:"<<fbid<<" fbid_local:"<<fbid_local<<" pc:0x"<<hex<<pc<<endl;
    assert(0 && "Can't find a valid header to nuke");
}

PtrFB BRQ::CommitHeader(PtrMachineInst const& h) {
    h->bfuInfo->committed = true;
    // mark all previous FB in BRQ as committed
    deque<PtrFB>::iterator it;
    it = getFBByHdr(h);

    // TODO: assert this is the right FB
    ASSERT(h->stid < brq.size());
    assert(it != brq[h->stid].end());
    for_each(brq[h->stid].begin(), it, [this](PtrFB& e) {
        pos_t pos_start = utils->CalcPosInFB(e->va);
        for (pos_t pos = pos_start; pos < BFU_BANDWIDTH; pos++) {
            if (utils->MhdrInvalid(e->machineInst[pos])) {
                continue;
            }
            e->machineInst[pos]->bfuInfo->resolved = true;
            e->machineInst[pos]->bfuInfo->committed = true;
        }
        e->committed = true;
    });

    stats->retired_header ++;
    stats->retired_header_total_lat += (h->bfuInfo->commit_time - h->bfuInfo->fetch_time);
    if (h->bfuInfo->resolve_mispredict) {
        stats->retired_mispred ++;
        if (h->IsCond()) {
            stats->retired_mispred_direction ++;
        } else {
            stats->retired_mispred_target ++;
        }
        if (h->bfuInfo->first_after_nuke) {
            stats->retired_misp_first_after_nuke ++;
        }
    }
    if (h->bfuInfo->resolved && h->bfuInfo->resolve_taken) {
        stats->bpc_discontinue_count++;
    }

    switch (h->GetBranchType()) {
        case BranchType::BLK_BR_FALL:
            stats->rtiredHdrFall++;
            if (h->bfuInfo->resolve_mispredict) {
                stats->rtiredHdrFallMiss++;
            }
            break;
        case BranchType::BLK_BR_COND:
            stats->rtiredHdrCond++;
            if (h->bfuInfo->resolve_mispredict) {
                stats->rtiredHdrCondMiss++;
            }
            break;
        case BranchType::BLK_BR_DIRECT:
            stats->rtiredHdrDir++;
            if (h->bfuInfo->resolve_mispredict) {
                stats->rtiredHdrDirMiss++;
            }
            break;
        case BranchType::BLK_BR_IND:
            stats->rtiredHdrInDir++;
            if (h->bfuInfo->resolve_mispredict) {
                stats->rtiredHdrInDirMiss++;
            }
            break;
        case BranchType::BLK_BR_CALL:
            stats->rtiredHdrCall++;
            if (h->bfuInfo->resolve_mispredict) {
                stats->rtiredHdrCallMiss++;
            }
            break;
        case BranchType::BLK_BR_ICALL:
            stats->rtiredHdrInCall++;
            if (h->bfuInfo->resolve_mispredict) {
                stats->rtiredHdrInCallMiss++;
            }
            break;
        case BranchType::BLK_BR_RET:
            stats->rtiredHdrRet++;
            if (h->bfuInfo->resolve_mispredict) {
                stats->rtiredHdrRetMiss++;
            }
            break;
        default:
            cout << "Branch Type is " << static_cast<int>(h->GetBranchType()) << endl;
            assert(0 && "Not support");
    }

    if (cfg->dump_mispreds) {
        if (mispreds.find(h->GetBundlePosPC()) == mispreds.end()) {
            MispInfo misp_info { h->GetBundlePosPC(), 0, 0, GetBlockBranchTypeName(h->GetBranchType()).c_str() }; // (pc, cnt, misp, type)
            mispreds[h->GetBundlePosPC()] = misp_info;
        }
        mispreds[h->GetBundlePosPC()].cnt ++;
        if (h->bfuInfo->resolve_mispredict) {
            mispreds[h->GetBundlePosPC()].misp ++;
        }
    }

    return *it;
}

void BRQ::resolveFetchBundle(PtrFB const& fb) {
    for (pos_t pos=0; pos<BFU_BANDWIDTH; pos++) {
        if (utils->MhdrInvalid(fb->machineInst[pos])) {
            continue;
        }
        auto& h = fb->machineInst[pos];
        if (cfg->debug_enable) {
            LOG_INFO_M(Unit::BFU, Stage::NA) << dec <<  "BRQ resolve FB, fbid=" << fb->fbid
                << ", fbid_local=" << fb->fbid_local << ", pc=0x" << hex << utils->CalcPC(fb->va, pos)
                << dec << ", vld" << h->bfuInfo->vld << ", resolved=" << h->bfuInfo->resolved;
        }
        logger->debug("BRQ", NIL, "resolve FB, fbid=%d, fbid_local=%d, pc=0x%x, vld=%d, resolved=%d\n",
                        fb->fbid, fb->fbid_local, utils->CalcPC(fb->va, pos), h->bfuInfo->vld, h->bfuInfo->resolved);
        if (h->bfuInfo->vld && !h->bfuInfo->resolved) return;
    }
    fb->resolved = true;
    if (cfg->update_mode == 2) updateResolveInfo(fb);
}

void BRQ::CommitFetchBundle() {
    // retire 1 FB per cycle and update the predictors
    for (auto &q : brq) {
        while (!q.empty() && q.front()->committed) {
            auto& fb = q.front();
            if (cfg->update_mode == 1) {
                updateResolveInfo(fb);
            }
            if (cfg->debug_enable) {
                LOG_INFO_M(Unit::BFU, Stage::NA) << dec << "BRQ commit FB, fbid=" << fb->fbid
                    << ", fbid_local=" << hex << ", va=0x" << fb->va;
            }
            logger->debug("BRQ", NIL, "commit FB, fbid=%d, fbid_local=%d, va=0x%x\n", fb->fbid, fb->fbid_local, fb->va);
            q.pop_front();
            stats->retired_fb ++;
        }
    }
}

bool BRQ::checkResolve(PtrFB const& fb) {
    for (pos_t pos=utils->CalcPosInFB(fb->va); pos<BFU_BANDWIDTH; pos++) {
        if (utils->MhdrInvalid(fb->machineInst[pos])) {
            continue;
        }
        if (!fb->machineInst[pos]->bfuInfo->resolved) {
            fb->resolved = false;
            return false;
        }
    }
    fb->resolved = true;
    return true;
}

void BRQ::UpdateInorder() {
    if (cfg->update_mode != 0)
        return;
    for (auto &q: brq) {
        if (q.empty()) {
            continue;
        }

        auto it = q.begin();
        for (; it != q.end(); it++) {
            if ((*it)->updated)
                continue;
            bool resolved = checkResolve(*it);
            if (resolved) {
                updateResolveInfo(*it);
            } else break;
        }
    }
}

void BRQ::updateResolveInfo(PtrFB const& fb) {
    // TODO: update resolve info at resolve time
    fb->updated = true;
    fb->rslv_info.mispredict = false;
    fb->rslv_info.taken = false;
    // check all resolved mhdrs in the bundle
    for (pos_t pos=utils->CalcPosInFB(fb->va); pos<BFU_BANDWIDTH; pos++) {
        if (utils->MhdrInvalid(fb->machineInst[pos]) || fb->machineInst[pos]->bfuInfo->spInfo->isInst) {
            continue;
        }
        auto& h = fb->machineInst[pos];
        if (cfg->update_mode == 1) {
            assert(h->bfuInfo->resolved && h->bfuInfo->committed);
        } else {
            assert(h->bfuInfo->resolved);
        }
        fb->rslv_info.mispredict = (fb->redir_info.pos == pos);
        if (h->bfuInfo->resolve_taken || fb->rslv_info.mispredict) {
            fb->rslv_info.taken = h->bfuInfo->resolve_taken;
            fb->rslv_info.pos = pos;
            fb->rslv_info.tgt = h->IsCond() ? fb->sp_info.tgt[pos] : h->bfuInfo->resolve_tgt;
            break;
        }
    }
    // update predictors
    bfu->UpdatePredictors(fb);
}

PtrFB BRQ::tryRedirect() {
    PtrFB ret_fb = nullptr;
    if (redirect_fb) {
        assert(redirect_fb->redir_info.pos < BFU_BANDWIDTH);
        ret_fb = redirect_fb;
        // re-calculate new hist for the redirected bundle
        bool taken = ret_fb->redir_info.resolve_taken;
        pos_t pos = ret_fb->redir_info.pos;
        addr_t pc = utils->CalcPC(ret_fb->va, pos);
        addr_t tgt = taken? ret_fb->redir_info.tgt : utils->NextBlockPC(pc);
        addr_t hash_pc = ret_fb->redir_info.resolve_taken ? pc : ret_fb->tag;
        ret_fb->redir_info.path_hist.emplace_back(hash_pc, tgt, ret_fb->sp_info.attr[pos], taken);
        redirect_fb->flushPos = redirect_fb->redir_info.pos;
        auto &h = redirect_fb->machineInst[redirect_fb->redir_info.pos];
        // Flush from the end of the block
        auto fb = (*getBodyEndFBByHdr(h));
        bool found = false;
        for (pos_t pos = utils->CalcPosInFB(fb->va); pos < BFU_BANDWIDTH; pos++) {
            if (utils->MhdrInvalid(fb->machineInst[pos]))
                continue;
            if (fb->machineInst[pos]->bfuInfo->hid != h->bfuInfo->hid && found) {
                fb->machineInst[pos]->bfuInfo->vld = false;
            } else if (fb->machineInst[pos]->bfuInfo->hid == h->bfuInfo->hid) {
                found = true;
            }
        }
        if (cfg->debug_enable) {
            LOG_INFO_M(Unit::BFU, Stage::NA) << "Redirect flush header fbid=" << std::dec << redirect_fb->fbid
                << ", stid=" << std::dec << redirect_fb->stid << ", fbid_local=" << redirect_fb->fbid_local
                << ", hid=" << redirect_fb->hid << std::hex << ", va=0x" << redirect_fb->va << std::dec
                << ", flush pos=" << redirect_fb->flushPos << std::hex << ", flush pc=0x" << h->pc;
        }
        flush(fb);
    }
    redirect_fb = nullptr;
    return ret_fb;
}

bool BRQ::HasRedirect()
{
    if (redirect_fb) {
        return true;
    }
    return false;
}

PtrFB BRQ::nukeflush(PtrMachineInst const& h) {
    // nukeFlush: PE replays nuke block and BFU fetches next block. Current FB may be splitted!
    auto fb = *(getFBByHdr(h));
    pos_t nuke_pos = utils->CalcPosInFB(h, fb->va);
    fb->flushPos = nuke_pos;
    if (cfg->debug_enable) {
        LOG_INFO_M(Unit::BFU, Stage::NA) << "Nuke flush header fbid=" << std::dec << h->bfuInfo->fbid
            << ", fbid_local=" << h->bfuInfo->fbid_local << ", hid=" << h->bfuInfo->hid
            << std::hex << ", px=0x" << h->pc;
    }
    flush(fb);

    // TODO: do we need the following code?
    for (pos_t pos=nuke_pos+1; pos<BFU_BANDWIDTH; pos++) {
        if (fb->machineInst[pos] == nullptr) {
            continue;
        }
        fb->machineInst[pos]->bfuInfo->vld = false;
    }
    fb->recoverFB(nuke_pos);

    // get bstart
    PtrMachineInst relate_h = h->bfuInfo->spInfo->isInst ? getBStartMhdrByFbid(h->bfuInfo->hid, h->stid) : h;
    relate_h->bfuInfo->fetch_end = false;
    relate_h->bfuInfo->nuke_after_redirect = false;
    uint32_t stid = h->stid;
    stashH[stid] = relate_h;
    // calculate next pc: here the nuke header does not need to be resolved
    if (relate_h->bfuInfo->resolved) {
        // when a resolved header is nuke flushed, it should have been triggered bru flush (if any)
        // h->resolve_tgt = h->resolve_tgt;
        // mark machineInst as non-resolved, since we don't allow multiple resolve in BIFU
        if (!relate_h->IsFallthrough() && !h->bfuInfo->spInfo->afterBranch && !relate_h->IsDirect()) {
            relate_h->bfuInfo->resolved = false;
            if (fb->redir_info.pos < BFU_BANDWIDTH && fb->redir_info.pos > nuke_pos) {
                fb->redir_info = RedirectInfo();
            }
        } else if (!relate_h->IsFallthrough()) {
            relate_h->bfuInfo->predict_taken = relate_h->bfuInfo->resolve_taken;
            relate_h->bfuInfo->predict_tgt = relate_h->bfuInfo->resolve_tgt;
            relate_h->bfuInfo->nuke_after_redirect = true;
        }
    } else if (relate_h->bfuInfo->predict_taken) {
        // not resolved yet, use predict tgt
        relate_h->bfuInfo->resolve_tgt = relate_h->bfuInfo->predict_tgt;
        relate_h->bfuInfo->resolve_taken = relate_h->bfuInfo->predict_taken;
    } else {
        relate_h->bfuInfo->resolve_tgt = utils->NextBlockPC(relate_h);
        relate_h->bfuInfo->resolve_taken = false;
    }
    // TODO: move the following logic elsewhere
    // One fb may be nuke flushed for multiple times. Clear the replay_info before appending to GHR
    fb->replay_info = RedirectInfo();
    fb->replay_info.pos = nuke_pos;
    assert(nuke_pos < BFU_BANDWIDTH);
    PtrFB fb_start = *(getFBByHdr(relate_h));
    if (!utils->CheckEqual(fb, fb_start)) {
        fb_start->replay_info = RedirectInfo();
    }
    bool taken = relate_h->bfuInfo->predict_taken;
    addr_t tgt = relate_h->bfuInfo->predict_tgt;
    addr_t hash_pc = relate_h->bfuInfo->predict_taken ? relate_h->bfuInfo->predict_tgt : fb_start->tag;
    fb_start->replay_info.path_hist.emplace_back(hash_pc, tgt, relate_h->GetBranchType(), taken);

    return fb;
}

void BRQ::flush(PtrFB const& fb) {
    ASSERT(fb->stid < brq.size());
    auto it = find(brq[fb->stid].begin(), brq[fb->stid].end(), fb);
    if (it == brq[fb->stid].end()) {
        cerr<<dec<<"fbid="<<fb->fbid<<" fbid_local="<<fb->fbid_local<<endl;
    }
    assert(it != brq[fb->stid].end());
    if (cfg->debug_enable) {
        LOG_INFO_M(Unit::BFU, Stage::NA) << std::dec << "Flush BRQ from fbid=" << fb->fbid
            << ", fbid_local=" << fb->fbid_local << ", hid=" << fb->hid
            << std::hex << ", pc=0x" << fb->va << ", stid=" << dec << fb->stid;
    }
    brq[fb->stid].erase(it+1, brq[fb->stid].end()); // do we need to check if it is the last?
    if (cfg->debug_enable) {
        if (!brq[fb->stid].empty()) {
            LOG_INFO_M(Unit::BFU, Stage::NA) << std::dec << "brq front fb fbid=" << brq[fb->stid].front()->fbid
                << ", fbid_local=" << brq[fb->stid].front()->fbid_local << ", hid=" << brq[fb->stid].front()->hid
                << std::hex << ", " << "pc=0x" << brq[fb->stid].front()->va;
            LOG_INFO_M(Unit::BFU, Stage::NA) << std::dec << "brq end fb fbid=" << brq[fb->stid].back()->fbid
                << ", fbid_local=" << brq[fb->stid].back()->fbid_local << ", hid=" << brq[fb->stid].back()->hid
                << std::hex << ", " << "pc=0x" << brq[fb->stid].back()->va;
        }
    }
    uint32_t stid = fb->stid;
    stashH[stid] = nullptr;
}

bool BRQ::isStall(size_t reserve, uint32_t stid) {
    ASSERT(stid < brq.size());
    return brq[stid].size() + reserve > cfg->brq_depth;
}

bool BRQ::HasFBByHdr(PtrMachineInst const& h) const
{
    if (!h || !h->bfuInfo || h->stid >= brq.size()) {
        return false;
    }
    auto it = find_if(brq[h->stid].begin(), brq[h->stid].end(), [&h](PtrFB const& fb) {
        return fb->fbid == h->bfuInfo->fbid && fb->fbid_local == h->bfuInfo->fbid_local;
    });
    return it != brq[h->stid].end();
}

bool BRQ::IsOlderThanFront(PtrMachineInst const& h) const
{
    if (!h || !h->bfuInfo) {
        return false;
    }
    return IsOlderThanFront(h->bfuInfo->fbid, h->bfuInfo->fbid_local, h->stid);
}

bool BRQ::IsOlderThanFront(seq_t fbid, seq_t fbid_local, uint32_t stid) const
{
    if (stid >= brq.size() || brq[stid].empty()) {
        return false;
    }
    auto const& front = brq[stid].front();
    return IsFBIDOlder(fbid, fbid_local, front->fbid, front->fbid_local);
}

deque<PtrFB>::iterator BRQ::getFBByHdr(PtrMachineInst const& h) {
    ASSERT(h->stid < brq.size());
    auto it = find_if(brq[h->stid].begin(), brq[h->stid].end(), [&h](PtrFB& fb){
        return (fb->fbid == h->bfuInfo->fbid && fb->fbid_local == h->bfuInfo->fbid_local);
    });
    if (it == brq[h->stid].end()) {
        if (cfg->debug_enable) {
            LOG_INFO_M(Unit::BFU, Stage::NA) << std::dec << "Exceptional fb fbid=" << h->bfuInfo->fbid
                << ", fbid_local=" << h->bfuInfo->fbid_local << ", hid=" << h->bfuInfo->hid
                << std::hex << ", pc=0x" << h->pc << ", bundle pc=0x" << h->GetBundlePosPC();
            if (!brq[h->stid].empty()) {
                LOG_INFO_M(Unit::BFU, Stage::NA) << std::dec << "brq front fb fbid=" << brq[h->stid].front()->fbid
                    << ", fbid_local=" << brq[h->stid].front()->fbid_local << ", hid=" << brq[h->stid].front()->hid
                    << std::hex << ", " << "pc=0x" << brq[h->stid].front()->va;
                LOG_INFO_M(Unit::BFU, Stage::NA) << std::dec << "brq end fb fbid=" << brq[h->stid].back()->fbid
                    << ", fbid_local=" << brq[h->stid].back()->fbid_local << ", hid=" << brq[h->stid].back()->hid
                    << std::hex << ", " << "pc=0x" << brq[h->stid].back()->va;
            }
        }
        cerr << dec << "fbid=" << h->bfuInfo->fbid << " fbid_local=" << h->bfuInfo->fbid_local <<
            " pc=0x" << hex << h->GetBundlePosPC() << " hid=" << dec << h->bfuInfo->hid << endl;
    }
    assert(it != brq[h->stid].end());
    return it;
}

deque<PtrFB>::iterator BRQ::getFBByFbid(seq_t fbid, seq_t fbid_local, uint32_t stid) {
    ASSERT(stid < brq.size());
    auto it = find_if(brq[stid].begin(), brq[stid].end(), [&fbid, &fbid_local](PtrFB& fb){
        return (fb->fbid == fbid && fb->fbid_local == fbid_local);
    });
    if (it == brq[stid].end()) {
        cerr << dec << "fbid=" << fbid << " fbid_local=" << fbid_local << " stid=" << stid << endl;
    }
    assert(it != brq[stid].end());
    return it;
}

deque<PtrFB>::iterator BRQ::getGlobalFBByFbid(seq_t fbid, uint32_t stid) {
    ASSERT(stid < brq.size());
    auto it = find_if(brq[stid].begin(), brq[stid].end(), [fbid](PtrFB& fb){
        return (fb->fbid == fbid && fb->global);
    });
    if (it == brq[stid].end()) {
        cerr<<dec<<"fbid="<<fbid<<" fbid_local=0"<<endl;
    }
    assert(it != brq[stid].end());
    return it;
}

deque<PtrFB>::iterator BRQ::getFBStartByHid(seq_t hid, uint32_t stid) {
    ASSERT(stid < brq.size());
    auto it = find_if(brq[stid].begin(), brq[stid].end(), [hid](PtrFB& fb){
        for (pos_t pos = 0; pos < BFU_BANDWIDTH; pos++) {
            if (IsRecoverableStart(fb->machineInst[pos], hid))
                return true;
        }
        return false;
    });
    if (it == brq[stid].end()) {
        cerr<<dec<<"hid="<<hid<<""<<endl;
    }
    assert(it != brq[stid].end());
    return it;
}

PtrMachineInst BRQ::getBStartMhdrByFbid(seq_t hid, uint32_t stid) {
    ASSERT(stid < brq.size());
    for (auto it = brq[stid].begin(); it != brq[stid].end(); it++) {
        auto fb = *it;
        pos_t pos_end = fb->main_info.end_pos;
        for (pos_t pos = utils->CalcPosInFB(fb->va); pos <= pos_end && pos < BFU_BANDWIDTH; pos++) {
            if (fb->machineInst[pos] == nullptr)
                continue;
            if (IsRecoverableStart(fb->machineInst[pos], hid))
                return fb->machineInst[pos];
        }
    }
    ASSERT(false && "Can't find bstart in BRQ!")
        << " hid=" << dec << hid
        << " stid=" << stid
        << " brq_depth=" << brq[stid].size();
    return nullptr;
}

deque<PtrFB>::iterator BRQ::getBodyEndFBByHdr(PtrMachineInst const& h) {
    auto it = getFBByHdr(h);
    bool cur_fb = true;
    ASSERT(h->stid < brq.size());
    while (it != brq[h->stid].end()) {
        PtrFB fb = *it;
        bool hid_match = false;
        bool found = false;
        pos_t pos = cur_fb ? utils->CalcPosInFB(h->GetBundlePosPC(), fb->va) : utils->CalcPosInFB(fb->va);
        for (; pos < BFU_BANDWIDTH; pos++) {
            if (utils->MhdrInvalid(fb->machineInst[pos]))
                continue;
            if (fb->machineInst[pos]->bfuInfo->hid != h->bfuInfo->hid) {
                found = true;
                if (!hid_match) {
                    assert(it != brq[h->stid].begin());
                    it--;
                }
                break;
            }
            hid_match = true;
        }
        if (found || next(it) == brq[h->stid].end())
            break;
        it++;
        cur_fb = false;
    }
    if (it == brq[h->stid].end()) {
        cerr<<dec<<"fbid="<<h->bfuInfo->fbid<<" fbid_local="<<h->bfuInfo->fbid_local<<" pc=0x"<<hex<<h->GetBundlePosPC()<<endl;
    }
    assert(it != brq[h->stid].end());
    return it;
}

void BRQ::dumpMispreds() {
    // sort by misp#
    vector<MispInfo> mispreds_vec;
    for (auto& kv : mispreds) {
        if (kv.second.misp) mispreds_vec.push_back(kv.second);
    }
    sort(mispreds_vec.begin(), mispreds_vec.end(), [](MispInfo& x, MispInfo& y) {
        return x.misp > y.misp;
    });
    // dump to file
    ofstream ofile(cfg->mispreds_file);
    if (ofile.is_open()) {
        for (auto& i : mispreds_vec) {
            ofile << "0x" << hex << i.pc  << " " << i.t << " " << dec << i.cnt << " " << i.misp << " " << endl;
        }
    } else {
        cerr << "Unable to open mispreds file" << endl;
        exit(-1);
    }
}

void BRQ::resetMispreds() {
    mispreds.clear();
}

void BRQ::Build()
{
    brq.resize(bfu->GetSim()->core->configs.scalar_smt_thread);
    stashH.resize(bfu->GetSim()->core->configs.scalar_smt_thread, nullptr);
}

}


} // namespace JCore
