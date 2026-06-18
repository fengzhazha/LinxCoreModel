#include "bctrl/bfu/bfu_ibtb.h"

#include "../configs/bfu_config.h"
#include "bctrl/bfu/bfu_logger.h"
#include "bctrl/bfu/bfu_utils.h"

namespace JCore {


using namespace std;

namespace NS_CORE {

IBTB::IBTB() {}
IBTB::~IBTB() {}

void IBTB::Build() {
    for (auto& nset_log2 : cfg->ibtb_nset_log2) {
        ibtb_nset.push_back(1<<nset_log2);
    }
    for (auto& l : cfg->ibtb_hist_len) {
        ibtb_hist_len.push_back(cfg->ghr_path_hist? l * cfg->ghr_hist_nbit_base : l);
    }
    tt.emplace_back(vector<vector<IBTBEntry>>{ibtb_nset[0], vector<IBTBEntry>{1}});
    for (idx_t i=1; i <cfg->ibtb_ntable; i++) {
        tt.emplace_back(vector<vector<IBTBEntry>>{ibtb_nset[i], vector<IBTBEntry>{cfg->ibtb_nway}});
    }
}

pos_t IBTB::getFirstIndirectPos(PtrFB const& fb) {
    for (pos_t pos=utils->CalcPosInFB(fb->va); pos<BFU_BANDWIDTH; pos++) {
        if (utils->IsIndirect(fb->sp_info.attr[pos])) return pos;
    }
    return -1;
}

void IBTB::Lookup(PtrFB const& fb) {
    
}

void IBTB::Predict(PtrFB const& fb, addr_t pc) {
    auto& ibtb_info = fb->ibtb_info;
    auto& ghr_info = fb->ghr_info[0];
    // Lookup base table
    ibtb_info.lkup_info.clear();
    ibtb_info.lkup_info.push_back(IBTBInfo::LookupInfo{calcBaseSetIdx(pc), 0, true});

    // Lookup tagged tables
    for (idx_t i=1; i <cfg->ibtb_ntable; i++) {
        set_t set_idx = calcTaggedSetIdx(pc, ghr_info.ghr, i);
        auto& s = tt[i][set_idx];
        tag_t tag = calcTag(pc, ghr_info.ghr, i);
        bool hit = false;
        way_t way_idx = 0;
        for (way_idx=0; way_idx<cfg->ibtb_nway; way_idx++) {
            if (s[way_idx].tag == tag) {
                hit = true;
                break;
            }
        }
        ibtb_info.lkup_info.push_back(IBTBInfo::LookupInfo{set_idx, way_idx, hit});
    }

    // get target
    for (idx_t i = 0; i <cfg->ibtb_ntable; i++) {
        if (ibtb_info.lkup_info[i].hit) {
            ibtb_info.table_idx = i;
            ibtb_info.tgt = tt[i][ibtb_info.lkup_info[i].set_idx][ibtb_info.lkup_info[i].way_idx].tgt;
        }
    }

    if (cfg->debug_enable) {
        LOG_INFO_M(Unit::BFU, Stage::F4) << "IBTB predict, fbid=" << dec << fb->fbid
            << hex << ", indir_pc=0x" << pc << ", tgt=0x" << ibtb_info.tgt
            << dec << ", table=" << ibtb_info.table_idx << ", s=" << ibtb_info.lkup_info[ibtb_info.table_idx].set_idx
            << ", w=" << ibtb_info.lkup_info[ibtb_info.table_idx].way_idx;
    }
    logger->debug("IBTB", F4, "Predict, fbid=%d, indir_pc=0x%x, tgt=0x%x, table=%d, s=%d, w=%d\n",
        fb->fbid, pc, ibtb_info.tgt, ibtb_info.table_idx, 
        ibtb_info.lkup_info[ibtb_info.table_idx].set_idx, ibtb_info.lkup_info[ibtb_info.table_idx].way_idx);
}

void IBTB::update(PtrFB const& fb, pos_t pos) {
    if (!fb->rslv_info.taken || fb->rslv_info.pos >= BFU_BANDWIDTH)
        return;
    PtrMachineInst &h = fb->machineInst[fb->rslv_info.pos];
    if (!h || !utils->IsIndirect(h->GetBranchType())) {
        return;
    }
    auto& info = fb->sfb_info[pos].vld ? fb->sfb_info[pos].ibtb_info : fb->ibtb_info;
    
    bool is_mispred = fb->rslv_info.mispredict;
    // update ubit, tgt and allocate new entry
    if (info.table_idx == 0) { // base table
        // update base table tgt on misprediction
        if (is_mispred) {
            tt[0][info.lkup_info[0].set_idx][0].tgt = fb->rslv_info.tgt;
        }
        // allocate on both correct or misprediction
        allocate(fb);
    } else { // tagged table
        // update ucnt
        auto& lkup_info = info.lkup_info[info.table_idx];
        auto& e = tt[info.table_idx][lkup_info.set_idx][lkup_info.way_idx];
        if (is_mispred) e.u.dec();
        else e.u.inc();
        // allocate only on misprediction
        if (is_mispred) {
            allocate(fb);
        }
    }

    if (cfg->debug_enable) {
        LOG_INFO_M(Unit::BFU, Stage::NA) << "IBTB update, fbid=" << dec << fb->fbid
            << ", misp=" << is_mispred << hex << ", pc=0x" << h->GetBundlePosPC() << dec
            << ", table_idx=" << info.table_idx << hex << ", tgt=0x" << fb->rslv_info.tgt << ", pred_tgt=0x" << info.tgt;
    }
    logger->debug("IBTB", NIL, "update, fbid=%d, misp=%d, pc=0x%x table_idx=%d, tgt=0x%x, pred_tgt=0x%x\n", 
            fb->fbid, is_mispred, h->GetBundlePosPC(), info.table_idx, fb->rslv_info.tgt, info.tgt);
    
}

void IBTB::allocate(PtrFB const& fb) {
    auto& info = fb->ibtb_info;
    if (info.table_idx == cfg->ibtb_ntable - 1) return;
    // allocate a new entry for current FB
    // 1. try to find an empty entry in the tables with longer hist
    // 2. if no empty (ucnt=0) entry is found, decrease ucnt of candidate entries
    bool alloc_suaccelss = false;
    for (idx_t t=info.table_idx+1; t<cfg->ibtb_ntable; t++) {
        auto& s = tt[t][info.lkup_info[t].set_idx];
        for (auto& e : s) {
            if (e.u.isZero()) {
                e.tag = calcTag(fb->machineInst[fb->rslv_info.pos]->GetBundlePosPC(), fb->ghr_info[0].ghr, t);
                e.tgt = fb->rslv_info.tgt;
                e.u.Reset();
                alloc_suaccelss = true;
                break;
            }
        }
        if (alloc_suaccelss) break;
    }
    if (!alloc_suaccelss) {
        for (idx_t t=info.table_idx+1; t<cfg->ibtb_ntable; t++) {
            for (auto& e : tt[t][info.lkup_info[t].set_idx]) {
                e.u.dec();
            }
        }
    }
}

set_t IBTB::calcBaseSetIdx(addr_t pc) {
    // TODO: use hashed PC?
    return utils->CalcPCIdx(pc) & (ibtb_nset[0] - 1);
}

set_t IBTB::calcTaggedSetIdx(addr_t pc, ghr_t* ghr, idx_t table_idx) {
    assert(table_idx != 0);
    addr_t folded_pc = utils->foldPC(utils->CalcPCIdx(pc), cfg->ibtb_nset_log2[table_idx]);
    addr_t folded_ghr = utils->foldHist(ghr, ibtb_hist_len[table_idx], cfg->ibtb_nset_log2[table_idx]);
    return folded_pc ^ folded_ghr;
}

tag_t IBTB::calcTag(addr_t pc, ghr_t* ghr, idx_t table_idx) {
    assert(table_idx != 0);
    addr_t pc_shift = utils->CalcPCIdx(pc) >> 2;
    addr_t folded_pc = utils->foldPC(pc_shift, cfg->ibtb_tag_len[table_idx]);
    addr_t folded_hist_tag0 = utils->foldHist(ghr, ibtb_hist_len[table_idx], cfg->ibtb_tag_len[table_idx]);
    addr_t folded_hist_tag1 = utils->foldHist(ghr, ibtb_hist_len[table_idx], cfg->ibtb_tag_len[table_idx]-1);
    return utils->LastNBit((folded_pc ^ folded_hist_tag0 ^ folded_hist_tag1), cfg->ibtb_tag_len[table_idx]);
}

}

} // namespace JCore
