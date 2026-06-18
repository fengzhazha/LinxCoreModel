#include "bctrl/bfu/bfu_pbtb.h"

#include "../configs/bfu_config.h"
#include "bctrl/bfu/bfu_logger.h"
#include "bctrl/bfu/bfu_utils.h"

namespace JCore {


using namespace std;

namespace NS_CORE {

PBTB::PBTB() {}

PBTB::~PBTB() {}

void PBTB::Build() {
    array = vector<vector<UBTBEntry>>{cfg->pbtb_nset, vector<UBTBEntry>{cfg->pbtb_nway}};
    for (set_t s=0; s<cfg->pbtb_nset; s++) {
        for (way_t w=0; w<cfg->pbtb_nway; w++) {
            array[s][w].Build(cfg->btb_nbranch);
            array[s][w].set_idx = s;
            array[s][w].way_idx = w;
        }
    }
    rep = std::make_shared<NMRU>(cfg->pbtb_nset, cfg->pbtb_nway);
}

void PBTB::Predict(PtrFB const& fb, const pos_t& pos) {
    assert(fb != nullptr);
    if (pos != utils->CalcPosInFB(fb->va)) {
        // stash for short forward branch
        pos_t pre_pos = fb->sfb_info[pos].pre_pos;
        fb->sfb_info[pre_pos].pbtb_info = fb->pbtb_info;
        return;
    }
    auto& info = fb->pbtb_info;

    UBTBEntry* e = Lookup(fb);
    if (e == nullptr) {
        info.hit = false;
    } else {
        info.hit = true;
        info.set_idx = e->set_idx;
        info.way_idx = e->way_idx;
        info.hit_entry = e;
    }
}

void PBTB::UpdateAtMain(PtrFB const& fb, pos_t taken_pos) {
    auto& pbtb_info = fb->pbtb_info;
    PtrMachineInst &h = fb->machineInst[taken_pos];
    // hit: update
    if (pbtb_info.hit) {
        auto& e = array[pbtb_info.set_idx][pbtb_info.way_idx];
        if (TagMatch(fb->tag, e)) {
            addr_t tgt = h->bfuInfo->predict_taken ? h->bfuInfo->predict_tgt : utils->CalcStaticTarget(h);
            e.update(h->GetBranchType(), tgt, h->GetBundlePosPC(), utils->NextBlockPC(h));
            for (uint32_t i = 0; i < e.br.size(); i++) {
                if (!e.br[i].vld || e.br[i].pc > h->GetBundlePosPC())
                    break;
                if (e.br[i].pc == h->GetBundlePosPC()) {
                    e.br[i].scnt.inc();
                    break;
                }
                e.br[i].scnt.dec();
            }
        }
        if (cfg->debug_enable) {
            LOG_INFO_M(Unit::BFU, Stage::F4) << "PBTB update, fbid=" << dec << fb->fbid
                << "s=" << e.set_idx << ", w=" << e.way_idx << ", tag=" << fb->tag
                << hex << ", va=0x" << fb->va << ", va_pre=0x" << fb->va_prev;
        }
        logger->debug("PBTB", F4, "update, fbid=%d, s=%d, w=%d, tag=%x, va=0x%x, va_pre=0x%x\n",
                        fb->fbid, e.set_idx, e.way_idx, fb->tag, fb->va, fb->va_prev);
    } else {
        // miss: allocate
        auto& e = array[CalcSetIdx(fb->tag)][rep->getVictim(CalcSetIdx(fb->tag))];
        e.Reset();
        e.tag = fb->tag;
        rep->insert(e.set_idx, e.way_idx);
        ASSERT(h);
        ASSERT(h->bfuInfo);
        addr_t tgt = h->bfuInfo->predict_taken ? h->bfuInfo->predict_tgt : utils->CalcStaticTarget(h);
        uint32_t idx = e.update(h->GetBranchType(), tgt, h->GetBundlePosPC(), utils->NextBlockPC(h));
        e.br[idx].scnt.Reset(true);
        if (cfg->debug_enable) {
            LOG_INFO_M(Unit::BFU, Stage::F4) << "PBTB allocate, fbid=" << dec << fb->fbid
                << ", s=" << e.set_idx << ", w=" << e.way_idx << hex << ", tag=0x" << fb->tag
                << ", va=0x" << fb->va << ", va_prev=0x" << fb->va_prev;
        }
        logger->debug("PBTB", F4, "allocate, fbid=%d, s=%d, w=%d, tag=%x, va=0x%x, va_pre=0x%x\n",
                        fb->fbid, e.set_idx, e.way_idx, fb->tag, fb->va, fb->va_prev);
    }
}

UBTBEntry* PBTB::Lookup(PtrFB const& fb) {
    stats->pbtb_aaccelss ++;
    set_t s = CalcSetIdx(fb->tag);
    for (way_t w=0; w<cfg->pbtb_nway; w++) {
        if (TagMatch(fb->tag, array[s][w])) {
            if (cfg->debug_enable) {
                LOG_INFO_M(Unit::BFU, Stage::F1) << "PBTB fbid=" << dec << fb->fbid
                    << hex << ", prev_va=0x" << fb->va_prev << ", tag=0x" << fb->tag
                    << dec << " hit PBTB, s=" << s << ", w=" << w;
            }
            logger->debug("PBTB", F1, "fbid=%d, prev_va=0x%x, tag=0x%x, hit PBTB, s=%d, w=%d\n",
                            fb->fbid, fb->va_prev, fb->tag, s, w);
            rep->touch(s, w);
            return &array[s][w];
        }
    }
    if (cfg->debug_enable) {
        LOG_INFO_M(Unit::BFU, Stage::F1) << "PBTB, fbid=" << dec << fb->fbid << hex
            << ", prev_va=0x" << fb->va_prev << ", pc" << fb->va << " miss PBTB";
    }
    logger->debug("PBTB", F1, "fbid=%d, prev_va=0x%x, pc=0x%x, miss PBTB\n", fb->fbid, fb->va_prev, fb->va);
    stats->pbtb_miss ++;
    return nullptr;
}

bool PBTB::TagMatch(const addr_t& tag, UBTBEntry const& e) {
    return e.tag == tag && rep->isValid(e.set_idx, e.way_idx);
}

set_t PBTB::CalcSetIdx(addr_t va) {
    return utils->CalcFBIdx(va) & (cfg->pbtb_nset - 1);
}

}

} // namespace JCore
