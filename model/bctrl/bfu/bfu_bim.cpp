#include "bctrl/bfu/bfu_bim.h"

#include "../configs/bfu_config.h"
#include "bctrl/bfu/bfu_logger.h"
#include "bctrl/bfu/bfu_utils.h"

namespace JCore {


using namespace std;

namespace NS_CORE {

BIM::BIM() {}
BIM::~BIM() {}

void BIM::Build() {
    nset = cfg->bim_nset;
    bt = vector<BIMEntry>{nset};
    for (uint32_t i = 0; i < bt.size(); i++) {
        bt[i].br.resize(cfg->tage_nbranch);
    }
}

void BIM::Lookup(PtrFB const& fb, const pos_t& pos) {
    if (cfg->light_core_enable) {
        return;
    }
    assert(fb);
    if (pos != utils->CalcPosInFB(fb->va)) {
        // stash for short forward branch
        pos_t pre_pos = fb->sfb_info[pos].pre_pos;
        fb->sfb_info[pre_pos].bim_info = fb->bim_info;
        fb->bim_info.Reset();
    }
    fb->bim_info.set_idx = CalcSetIdx(fb->tag);
}

void BIM::Predict(PtrFB const& fb, pos_t pos) {
    if (cfg->light_core_enable) {
        return;
    }
    auto& bim_info = fb->sfb_info[pos].vld ? fb->sfb_info[pos].bim_info : fb->bim_info;
    set_t idx = bim_info.set_idx;
    addr_t va = utils->CalcPC(fb->va, pos);
    bim_info.taken = false;
    for (uint32_t i = 0; i < bt[idx].br.size(); i++) {
        if (!bt[idx].br[i].vld)
            break;
        if (bt[idx].br[i].pc < va)
            continue;
        if (bt[idx].br[i].scnt.isTaken()) {
            bim_info.taken = true;
            bim_info.taken_type = bt[idx].br[i].attr;
            bim_info.taken_pc = bt[idx].br[i].pc;
            bim_info.tgt = bt[idx].br[i].tgt;
            bim_info.end_pc = bt[idx].br[i].end_pc;
            break;
        }
    }
    if (cfg->debug_enable) {
        LOG_INFO_M(Unit::BFU, Stage::F4) << dec << " BIM predict, fbid=" << fb->fbid
            << ", set_idx=" << fb->bim_info.set_idx << ", taken=" << fb->bim_info.taken
            << hex << ", taken_pc=0x" << fb->bim_info.taken_pc << ", tgt=0x" << fb->bim_info.tgt
            << ", end_pc=0x" << fb->bim_info.end_pc;
    }
    logger->debug("BIM", F4, "BIM Predict, fbid=%d, set_idx=%d, taken=%d, taken_pc=0x%x, tgt=0x%x, end_pc=0x%x\n", 
                    fb->fbid, fb->bim_info.set_idx, fb->bim_info.taken, fb->bim_info.taken_pc, fb->bim_info.tgt, fb->bim_info.end_pc);
}

void BIM::update(PtrFB const& fb, pos_t pos) {
    if (cfg->light_core_enable) {
        return;
    }
    auto& bim_info = fb->sfb_info[pos].vld ? fb->sfb_info[pos].bim_info : fb->bim_info;
    if (fb->rslv_info.pos == -1U) {
        return;
    }
    assert(fb->rslv_info.pos < BFU_BANDWIDTH);
    PtrMachineInst &h = fb->machineInst[fb->rslv_info.pos];
    if (bim_info.set_idx == -1U || !fb->rslv_info.taken)
        return;
    auto& e = bt[bim_info.set_idx];
    e.update(h->GetBranchType(), fb->rslv_info.tgt, h->GetBundlePosPC(), utils->NextBlockPC(h));
    for (uint32_t i = 0; i < e.br.size(); i++) {
        if (!e.br[i].vld || e.br[i].pc > h->GetBundlePosPC()) {
            break;
        }
        if (h->GetBundlePosPC() == e.br[i].pc) {
            e.br[i].scnt.inc();
            if (cfg->debug_enable) {
                LOG_INFO_M(Unit::BFU, Stage::NA) << dec << "BIM update, fbid=" << fb->fbid
                    << ", fbid_local=" << fb->fbid_local << hex << ", fb_tag=0x" << fb->tag
                    << dec << ", set_idx=" << bim_info.set_idx << ", taken=" << fb->rslv_info.taken
                    << ", pos=" << fb->rslv_info.pos;
            }
            logger->debug("BIM", NIL, "BIM update, fbid=%d, fbid_local=%d, fb_tag=0x%x, set_idx=%d, taken=%d, pos=%d\n", 
                fb->fbid, fb->fbid_local, fb->tag, bim_info.set_idx, fb->rslv_info.taken, fb->rslv_info.pos);
            break;
        }
        e.br[i].scnt.dec();
    }
}

set_t BIM::CalcSetIdx(addr_t pc) {
    return utils->CalcFBIdx(pc) % nset;
}

}

} // namespace JCore
