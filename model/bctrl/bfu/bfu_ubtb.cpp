#include "bctrl/bfu/bfu_ubtb.h"

#include "../configs/bfu_config.h"
#include "bctrl/bfu/bfu_logger.h"
#include "bctrl/bfu/bfu_utils.h"

namespace JCore {


using namespace std;

namespace NS_CORE {

UBTB::UBTB() {}

UBTB::~UBTB() {}

void UBTB::Build() {
    array = vector<vector<UBTBEntry>>{cfg->ubtb_nset, vector<UBTBEntry>{cfg->ubtb_nway}};
    for (set_t s=0; s<cfg->ubtb_nset; s++) {
        for (way_t w=0; w<cfg->ubtb_nway; w++) {
            array[s][w].Build(cfg->btb_nbranch);
            array[s][w].set_idx = s;
            array[s][w].way_idx = w;
        }
    }
    rep = std::make_shared<NMRU>(cfg->ubtb_nset, cfg->ubtb_nway);
}

void UBTB::Predict(PtrFB const& fb, pos_t pos) {
    assert(fb != nullptr);
    if (pos != utils->CalcPosInFB(fb->va)) {
        // stash for short forward branch
        pos_t pre_pos = fb->sfb_info[pos].pre_pos;
        fb->sfb_info[pre_pos].ubtb_info = fb->ubtb_info;
    }
    auto& info = fb->ubtb_info;

    UBTBEntry* e = info.hit ? &array[info.set_idx][info.way_idx] : Lookup(fb);
    if (cfg->pbtb_enable && fb->pbtb_info.hit && e == nullptr) {
        // ubtb miss, pbtb hit, use pbtb result
        set_t set_idx = CalcSetIdx(fb->va);
        way_t way_idx = rep->getVictim(set_idx);
        e = &array[set_idx][way_idx];
        *e = *(fb->pbtb_info.hit_entry);
        e->set_idx = set_idx;
        e->way_idx = way_idx;
        e->tag = fb->tag;
        rep->insert(set_idx, way_idx);
    }

    if (e == nullptr) {
        info.hit = false;
        info.taken = false;
        info.tgt = utils->NextFBPC(fb->va);
        info.path_hist.emplace_back(fb->tag, info.tgt, BranchType::BLK_BR_INVAL, false);
    } else {
        info.hit = true;
        info.set_idx = e->set_idx;
        info.way_idx = e->way_idx;
        info.taken = false;
        info.tgt = utils->NextFBPC(fb->va);
        info.taken_type = BranchType::BLK_BR_INVAL;
        for (uint32_t idx = 0; idx < e->br.size(); idx++) {
            if (!e->br[idx].vld)
                break;
            if (e->br[idx].pc < fb->va)
                continue;
            if (cfg->debug_enable) {
                LOG_INFO_M(Unit::BFU, Stage::F1) << "UBTB predict, tag=0x" << hex << fb->tag
                    << ", pred_taken=" << e->isTaken(idx) << ", pred_tgt=0x" << e->br[idx].tgt
                    << ", attr=" << GetBlockBranchTypeName(e->br[idx].attr).c_str()
                    << ", taken_pc=0x" << e->br[idx].pc << ", end_pc=0x" << e->br[idx].end_pc;
            }
            logger->debug("UBTB", F1, "tag=0x%x, pred_taken=%d, pred_tgt=0x%x, attr=0x%x, cnt_2b=%d, taken_pc=0x%x, end_pc=0x%x\n",
                                fb->tag, e->isTaken(idx), e->br[idx].tgt, e->br[idx].attr, e->br[idx].scnt, e->br[idx].pc, e->br[idx].end_pc);
            if (e->isTaken(idx)) {
                info.taken = true;
                info.tgt = e->br[idx].tgt;
                info.taken_pc = e->br[idx].pc;
                info.taken_type = e->br[idx].attr;
                info.end_pc = e->br[idx].end_pc;
                break;
            }
        }
        addr_t hash_pc = info.taken ? info.taken_pc : fb->tag;
        info.path_hist.emplace_back(hash_pc, info.tgt, info.taken_type, info.taken);
    }
    if (cfg->debug_enable) {
        LOG_INFO_M(Unit::BFU, Stage::F1) << "UBTB predict, fbid=" << dec << fb->fbid
            << hex << ", pc=0x" << fb->va << ", pred_taken=" << info.taken << ", pred_tgt=0x" << info.tgt;
    }
    logger->debug("UBTB", F1, "fbid=%d, pc=0x%x, pred_taken=%d, pred_tgt=0x%x\n", fb->fbid, fb->va, info.taken, info.tgt);
}

void UBTB::UpdateAtMain(PtrFB const& fb, pos_t taken_pos) {
    auto& ubtb_info = fb->ubtb_info;
    PtrMachineInst& h = fb->machineInst[taken_pos];
    if (ubtb_info.hit) {
        auto& e = array[ubtb_info.set_idx][ubtb_info.way_idx];
        if (TagMatch(fb->tag, e)) {
            addr_t tgt = utils->IsBranchWithOffeset(h->GetBranchType()) ? fb->sp_info.tgt[taken_pos] : h->bfuInfo->predict_tgt;
            uint32_t idx = e.update(h->GetBranchType(), tgt, h->GetBundlePosPC(), utils->NextBlockPC(h));
            for (uint32_t i = 0; i < e.br.size(); i++) {
                if (!e.br[i].vld || e.br[i].pc > h->GetBundlePosPC())
                    break;
                if (e.br[i].pc == h->GetBundlePosPC()) {
                    e.br[i].scnt.inc();
                    break;
                }
                e.br[i].scnt.dec();
            }
            if (idx != -1U) {
                if (cfg->debug_enable) {
                    LOG_INFO_M(Unit::BFU, Stage::F4) << "UBTB update, fbid=" << dec << fb->fbid
                        << ", s=" << e.set_idx << ", w=" << e.way_idx << hex << ", tag=0x" << fb->tag
                        << ", va=0x" << fb->va << ", attr=" << GetBlockBranchTypeName(h->GetBranchType()).c_str() << ", tgt=0x" << tgt
                        << ", px=0x" << h->GetBundlePosPC() << ", end_pc=0x" << utils->NextBlockPC(h);
                }
                logger->debug("UBTB", F4, "update, fbid=%d, s=%d, w=%d, tag=%x, va=0x%x, attr=%d, target=0x%x, pc=0x%x, end_pc=%x\n",
                                    fb->fbid, e.set_idx, e.way_idx, fb->tag, fb->va, h->GetBranchType(), tgt, h->GetBundlePosPC(), utils->NextBlockPC(h));
            }
        }
    } else {
        // miss: allocate
        auto& e = array[CalcSetIdx(fb->tag)][rep->getVictim(CalcSetIdx(fb->tag))];
        e.Reset();
        e.tag = fb->tag;
        rep->insert(e.set_idx, e.way_idx);
        addr_t tgt = utils->IsBranchWithOffeset(h->GetBranchType()) ? fb->sp_info.tgt[taken_pos] : h->bfuInfo->predict_tgt;
        uint32_t idx = e.update(h->GetBranchType(), tgt, h->GetBundlePosPC(), utils->NextBlockPC(h));
        e.br[idx].scnt.Reset(true);
        if (cfg->debug_enable) {
            LOG_INFO_M(Unit::BFU, Stage::F4) << dec << "UBTB allocate, fbid=" << fb->fbid
                << ", s=" << e.set_idx << ", w=" << e.way_idx << hex << ", tag=0x" << fb->tag
                << ", va=0x" << fb->va << ", attr=" << GetBlockBranchTypeName(h->GetBranchType()).c_str() << ", tgt=0x" << tgt
                << ", pc=0x" << h->GetBundlePosPC() << ", end_pc=0x" << utils->NextBlockPC(h);
        }
        logger->debug("UBTB", F4, "allocate, fbid=%d, s=%d, w=%d, tag=%x, va=0x%x, attr=%d, target=0x%x, pc=0x%x, end_pc=0x%x\n",
                                    fb->fbid, e.set_idx, e.way_idx, fb->tag, fb->va, h->GetBranchType(), tgt, h->GetBundlePosPC(), utils->NextBlockPC(h));
    }
}

void UBTB::UpdateAtResolve(PtrFB const& fb, pos_t pos) {
    // update at resolve if mainpred uses ubtb prediction
    auto& ubtb_info = fb->sfb_info[pos].vld ? fb->sfb_info[pos].ubtb_info : fb->ubtb_info;
    auto& rslv_info = fb->rslv_info;
    if (!rslv_info.taken) {
        return;
    }
    PtrMachineInst &h = fb->machineInst[rslv_info.pos];
    if (!ubtb_info.hit) {
        auto& e = array[CalcSetIdx(fb->tag)][rep->getVictim(CalcSetIdx(fb->tag))];
        e.Reset();
        e.tag = fb->tag;
        rep->insert(e.set_idx, e.way_idx);
        uint32_t idx = e.update(h->GetBranchType(), rslv_info.tgt, h->GetBundlePosPC(), utils->NextBlockPC(h));
        e.br[idx].scnt.Reset(true);
        if (cfg->debug_enable) {
            LOG_INFO_M(Unit::BFU, Stage::NA) << "UBTB allocate rslv, fbid=" << dec << fb->fbid
                << ", s=" << e.set_idx << ", w=" << e.way_idx << hex << ", tag=0x" << fb->tag
                << ", va=0x" << fb->va << ", attr=" << GetBlockBranchTypeName(h->GetBranchType()).c_str() << ", tgt=0x" << rslv_info.tgt
                << ", px=0x" << h->GetBundlePosPC() << ", end_pc=0x" << utils->NextBlockPC(h);
        }
        logger->debug("UBTB", NIL, "allocte rslv, fbid=%d, s=%d, w=%d, tag=%x, va=0x%x, attr=%d, target=0x%x, pc=0x%x, end_pc=0x%x\n",
                                    fb->fbid, e.set_idx, e.way_idx, fb->tag, fb->va, h->GetBranchType(), rslv_info.tgt, h->GetBundlePosPC(), utils->NextBlockPC(h));
        return;
    }

    auto& e = array[ubtb_info.set_idx][ubtb_info.way_idx];
    if (TagMatch(fb->tag, e)) {
        e.update(h->GetBranchType(), rslv_info.tgt, h->GetBundlePosPC(), utils->NextBlockPC(h));
        for (uint32_t i = 0; i < e.br.size(); i++) {
            if (!e.br[i].vld || e.br[i].pc > h->GetBundlePosPC()) {
                break;
            }
            if (e.br[i].pc == h->GetBundlePosPC()) {
                e.br[i].scnt.inc();
                break;
            }
            e.br[i].scnt.dec();
        }
        if (cfg->debug_enable) {
            LOG_INFO_M(Unit::BFU, Stage::NA) << "UBTB update rslv, s=" << dec << ubtb_info.set_idx
                << ", w=" << ubtb_info.way_idx << ", fbid=" << fb->fbid << hex << ", tag=0x" << fb->tag
                << ", pc=0x" << utils->CalcPC(fb->va, rslv_info.pos) << ", attr=" << GetBlockBranchTypeName(h->GetBranchType()).c_str()
                << ", tgt=0x" << rslv_info.tgt << ", taken_pc=0x" << h->GetBundlePosPC() << ", end_pc=0x" << utils->NextBlockPC(h);
        }
        logger->debug("UBTB", NIL, "update rslv, s=%d, w=%d, fbid=%d, tag=0x%x, pc=0x%x, attr=%d, target=0x%x, taken_pc=0x%x, end_pc=0x%x\n",
                        ubtb_info.set_idx, ubtb_info.way_idx, fb->fbid, fb->tag, utils->CalcPC(fb->va, rslv_info.pos), h->GetBranchType(), rslv_info.tgt, h->GetBundlePosPC(), utils->NextBlockPC(h));
    }
}

UBTBEntry* UBTB::Lookup(PtrFB const& fb) {
    stats->ubtb_aaccelss ++;
    set_t s = CalcSetIdx(fb->tag);
    for (way_t w=0; w<cfg->ubtb_nway; w++) {
        if (TagMatch(fb->tag, array[s][w])) {
            if (cfg->debug_enable) {
                LOG_INFO_M(Unit::BFU, Stage::F1) << "UBTB Lookup, fbid=" << dec << fb->fbid
                    << hex << ", pc=0x" << fb->tag << dec << " , hit UBTB, s=" << s << ", w=" << w;
            }
            logger->debug("UBTB", F1, "fbid=%d, pc=0x%x, hit UBTB, s=%d, w=%d\n", fb->fbid, fb->tag, s, w);
            rep->touch(s, w);
            return &array[s][w];
        }
    }
    if (cfg->debug_enable) {
        LOG_INFO_M(Unit::BFU, Stage::F1) << dec << "UBTB Lookup, fbid=" << fb->fbid
            << hex << ", pc=0x" << fb->va << ", miss UBTB";
    }
    logger->debug("UBTB", F1, "fbid=%d, pc=0x%x, miss UBTB\n", fb->fbid, fb->va);
    stats->ubtb_miss ++;
    return nullptr;
}

bool UBTB::TagMatch(const addr_t& tag, UBTBEntry const& e) {
    return e.tag == tag && rep->isValid(e.set_idx, e.way_idx);
}

set_t UBTB::CalcSetIdx(addr_t va) {
    return utils->CalcFBIdx(va) & (cfg->ubtb_nset - 1);
}

}

} // namespace JCore
