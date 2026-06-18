#include "bctrl/bfu/bfu_tage.h"

#include "../configs/bfu_config.h"
#include "bctrl/bfu/bfu_logger.h"
#include "bctrl/bfu/bfu_utils.h"

namespace JCore {


using namespace std;

namespace NS_CORE {

TAGE::TAGE() {

}

TAGE::~TAGE() {

}

void TAGE::Build() {
    ghrHistNbit = cfg->ghr_hist_nbit_base + id;
    ntable = cfg->tage_ntable;
    nway = cfg->tage_nway;
    for (auto& l : cfg->tage_hist_len) {
        hist_len.push_back(cfg->ghr_path_hist? l * ghrHistNbit : l);
    }
    tag_len = vector<msize_t>(cfg->tage_tag_len.begin(), cfg->tage_tag_len.end());
    for (auto nset_log2 : cfg->tage_nset_log2) {
        nset.push_back(1 << nset_log2);
    }

    for (idx_t i = 0; i <ntable; i++) {
        tt.emplace_back(vector<vector<TaggedEntry>>{nset[i], vector<TaggedEntry>{nway}});
        for (idx_t j = 0; j < tt[i].size(); j++) {
            for (idx_t k = 0; k < tt[i][j].size(); k++) {
                tt[i][j][k].br.resize(cfg->tage_nbranch);
            }
        }
    }
}

void TAGE::SetID(uint32_t i)
{
    id = i;
}

void TAGE::Lookup(PtrFB const& fb, const pos_t& pos) {
    if (pos != utils->CalcPosInFB(fb->va)) {
        // stash for short forward branch
        pos_t pre_pos = fb->sfb_info[pos].pre_pos;
        fb->sfb_info[pre_pos].tage_info[id] = fb->tage_info[id];
        fb->tage_info[id].Reset();
    }
    if (cfg->debug_enable) {
        LOG_INFO_M(Unit::BFU, Stage::F1) << "TAGE GHR info, pc=0x" << hex << fb->tag
            << dec << ", fbid=" << fb->fbid << ", fbid_local=" << fb->fbid_local
            << ", wptr=" << fb->ghr_info[id].wptr[0] << " " << fb->ghr_info[id].wptr[1]
            << " " << fb->ghr_info[id].wptr[2] << hex << "ghr=0x" << fb->ghr_info[id].ghr[0];
    }
    logger->debug("TAGE", F1, "GHR info, pc=0x%x, fbid=%d, fbid_local=%d, wptr=%d %d %d, ghr=0x%llx\n", 
        fb->tag , fb->fbid, fb->fbid_local, fb->ghr_info[id].wptr[0], fb->ghr_info[id].wptr[1], fb->ghr_info[id].wptr[2], fb->ghr_info[id].ghr[0]);
    for (idx_t i = 0; i <ntable; i++) {
        idx_t set_idx = calcSetIdx(fb->ghr_info[id].ghr, fb->tag, i);
        tag_t tag = calcTag(fb->ghr_info[id].ghr, fb->tag, i);
        auto& s = tt[i][set_idx];
        bool hit = false;
        way_t way_idx = 0;
        for (; way_idx < nway; way_idx++) {
            if (s[way_idx].tag == tag) {
                hit = true;
                break;
            }
        }
        // update lookupinfo
        TAGEInfo::LookupInfo info;
        if (hit) {
            getTakenInfo(info, s[way_idx], fb->va);
            info.set_idx = set_idx;
            info.way_idx = way_idx;
        } else {
            info.hit = false;
            info.set_idx = set_idx;
        }
        fb->tage_info[id].lkup_info.push_back(info);
        if (cfg->debug_enable) {
            LOG_INFO_M(Unit::BFU, Stage::F1) << hex << "TAGE Lookup, pc=0x" << fb->va
                << dec << ", fbid=" << fb->fbid << ", table=" << i << ", set_idx=" << set_idx
                << ", way_idx=" << way_idx << "hit=" << hit << hex << ", tag=0x" << tag
                << ", taken=" << info.taken << ", taken_pc=0x" << info.pc << ",tgt=0x" << info.tgt;
        }
        logger->debug("TAGE", F1, "TAGE Lookup, pc=0x%x, fbid=%d, table=%d, set_idx=%d, way_idx=%d, hit=%d, tag=0x%x, taken=%d, taken_pc=0x%x, tgt=0x%x\n", 
                      fb->va, fb->fbid, i, set_idx, way_idx, hit, tag, info.taken, info.pc, info.tgt);
    }
}

void TAGE::getTakenInfo(TAGEInfo::LookupInfo& info, TaggedEntry& e, addr_t pc) {
    info.hit = true;
    info.taken = false;
    for (uint32_t idx = 0; idx < e.br.size(); idx++) {
        if (!e.br[idx].vld)
            break;
        if (e.br[idx].pc < pc)
            continue;
        if (e.br[idx].scnt.isTaken()) {
            info.taken = true;
            info.w = e.w;
            info.attr = e.br[idx].attr;
            info.tgt = e.br[idx].tgt;
            info.pc = e.br[idx].pc;
            info.end_pc = e.br[idx].end_pc;
            break;
        }
    }
}

void TAGE::Predict(PtrFB const& fb, pos_t pos) {
    idx_t prim_idx = -1;
    idx_t altn_idx = -1;
    auto& info = fb->sfb_info[pos].vld ? fb->sfb_info[pos].tage_info[id] : fb->tage_info[id];
    for (msize_t i = 0; i <ntable; i++) {
        idx_t table_idx = ntable - 1 - i;
        if (prim_idx == -1U && info.lkup_info[table_idx].hit) { // try to find primary prediction
            prim_idx = table_idx;
        } else if ((altn_idx == -1U) && (info.lkup_info[table_idx].hit)) { // try to find alternative prediction
            altn_idx = table_idx;
        }
    }

    // update fb->tage_info
    info.prim_idx = prim_idx;
    info.altn_idx = altn_idx;
    if (prim_idx != -1U) {
        info.taken = info.lkup_info[prim_idx].taken;
        info.taken_type = info.lkup_info[prim_idx].attr;
        info.taken_pc = info.lkup_info[prim_idx].pc;
        info.tgt = info.lkup_info[prim_idx].tgt;
        info.end_pc = info.lkup_info[prim_idx].end_pc;
        info.w = info.lkup_info[prim_idx].w;
        if (cfg->debug_enable) {
            LOG_INFO_M(Unit::BFU, Stage::F4) << "TAGE predict, fbid=" << dec << fb->fbid
                << ", prim_idx=" << prim_idx << ", taken=" << info.taken
                << hex << ", taken_pc=0x" << info.taken_pc << ", tgt=0x" << info.tgt
                << ", end_pc=0x" << info.end_pc;
        }
        logger->debug("TAGE", F4, "TAGE predict, fbid=%d, prim_idx=%d, taken=%d, taken_pc=0x%x, target=0x%x, end_pc=0x%x\n",
                        fb->fbid, prim_idx, info.taken, info.taken_pc, info.tgt, info.end_pc);
    }
}

void TAGE::update(PtrFB const& fb, pos_t pos) {
    auto& tage_info = fb->sfb_info[pos].vld ? fb->sfb_info[pos].tage_info[id] : fb->tage_info[id];
    idx_t prim_idx = tage_info.prim_idx;
    if (tage_info.prim_idx == -1U)
        return;
    auto& prim_info = tage_info.lkup_info[prim_idx];
    auto& prim_entry = tt[prim_idx][prim_info.set_idx][prim_info.way_idx];
    idx_t altn_idx = tage_info.altn_idx;
    // is TAGE to be blamed (pos may mismatch)
    bool is_tage_mispredict = fb->rslv_info.mispredict;
    // update scnt from fb start pos to rslv pos
    if (fb->rslv_info.pos == -1U) {
        return;
    }
    assert(fb->rslv_info.pos < BFU_BANDWIDTH);
    PtrMachineInst &h = fb->machineInst[fb->rslv_info.pos];
    if (fb->rslv_info.taken) {
        prim_entry.update(h->GetBranchType(), fb->rslv_info.tgt, h->GetBundlePosPC(), utils->NextBlockPC(h));
        RptHashStats(prim_idx, prim_entry.tag, fb->tag, fb->rslv_info.tgt);
        for (uint32_t i = 0; i < prim_entry.br.size(); i++) {
            if (!prim_entry.br[i].vld || prim_entry.br[i].pc > h->GetBundlePosPC())
                break;
            if (prim_entry.br[i].pc == h->GetBundlePosPC()) {
                prim_entry.br[i].scnt.inc();
                if (cfg->debug_enable) {
                    LOG_INFO_M(Unit::BFU, Stage::NA) << dec << "TAGE update, fbid=" << fb->fbid
                        << ", fbid_local=" << fb->fbid_local << hex << ", tag=0x" << fb->tag
                        << dec << ", table=" << prim_idx << ", set_idx=" << prim_info.set_idx
                        << ", way_idx=" << prim_info.way_idx << hex << ", tag=0x" << prim_entry.tag
                        << ", pc=0x" << h->GetBundlePosPC() << ", rslv_taken=" << fb->rslv_info.taken;
                }
                logger->debug("TAGE", NIL, "TAGE update, fbid=%d, fbid_local=%d, fb_tag=0x%x, table=%d, set_idx=%d, way_idx=%d, tag=0x%x, pc=0x%x, rslv_taken=%d\n",
                    fb->fbid, fb->fbid_local, fb->tag, prim_idx, prim_info.set_idx, prim_info.way_idx, prim_entry.tag, h->GetBundlePosPC(), fb->rslv_info.taken);
                break;
            }
            prim_entry.br[i].scnt.dec();
        }
        // weight scnt state transition
        if (id == 0 && fb->tage_info[id+1].prim_idx != -1U) {
            auto &lookupInfo2 = fb->tage_info[id+1].lkup_info;
            auto &primIdx2 = fb->tage_info[id+1].prim_idx;
            if (h->GetBundlePosPC() == tage_info.lkup_info[prim_idx].pc && h->GetBundlePosPC() != lookupInfo2[primIdx2].pc) {
                prim_entry.w.inc();
            } else if (h->GetBundlePosPC() != tage_info.lkup_info[prim_idx].pc && h->GetBundlePosPC() == lookupInfo2[primIdx2].pc) {
                prim_entry.w.dec();
            }
        }
    }

    if (!fb->cPHTInfo.hit || id != fb->cPHTInfo.selectID) {
        return;
    }
    // update u-bit
    if (!is_tage_mispredict && altn_idx == -1U) { // correct pred, no altn
        prim_entry.u.inc();
    } else if (!is_tage_mispredict && altn_idx != -1U) {
        auto& altn_info = tage_info.lkup_info[altn_idx];
        if (prim_info.taken ^ altn_info.taken || prim_info.pc != altn_info.pc) {
            prim_entry.u.inc();
        }
    } else if (is_tage_mispredict) { // misprediction
        prim_entry.u.dec();
    }
}

void TAGE::allocate(PtrFB const& fb, pos_t pos) {
    // try to allocate a new entry on misprediction
    // 1. prim table should not be the table with longest hist
    // 2. if there is an entry with ubit=0 in the tables with logner hist, allocate
    //    else decrease ubit in the tables with longer hist
    if (fb->sfb_info[pos].vld || fb->rslv_info.pos >= BFU_BANDWIDTH) {
        return;
    }
    PtrMachineInst &h = fb->machineInst[fb->rslv_info.pos];
    idx_t prim_idx = fb->tage_info[id].prim_idx;
    bool alloc_suaccelss = false;
    for (idx_t t = (prim_idx==-1U ? 0 : prim_idx+1); t < ntable; t++) {
        set_t set_idx = fb->tage_info[id].lkup_info[t].set_idx;
        auto& s = tt[t][set_idx];
        auto new_tag = calcTag(fb->ghr_info[id].ghr, fb->tag, t);
        for (way_t w=0; w<nway; w++) {
            auto& e = s[w];
            if (e.u.isZero() || e.tag == new_tag) {
                e.Reset();
                addr_t tgt = fb->rslv_info.taken ? fb->rslv_info.tgt : utils->CalcStaticTarget(h);
                uint32_t idx = e.update(h->GetBranchType(), tgt, h->GetBundlePosPC(), utils->NextBlockPC(h));
                e.br[idx].scnt.Reset(fb->rslv_info.taken);
                e.tag = new_tag;
                e.u.Reset();
                alloc_suaccelss = true;
                RptHashStats(t, e.tag, fb->tag, tgt);
                stats->tageAllocSuaccelss++;
                if (cfg->debug_enable) {
                    LOG_INFO_M(Unit::BFU, Stage::NA) << "TAGE allocate, fbid=" << fb->fbid
                        << ", table=" << t << ", set_idx=" << set_idx << ", way_idx=" << w
                        << hex << ", tag=0x" << e.tag << ", fb_tag=0x" << fb->tag
                        << ", px=0x" << h->GetBundlePosPC() << ", taken=" << fb->rslv_info.taken;
                }
                logger->debug("TAGE", NIL, "TAGE allocate, fbid=%d, table=%d, set_idx=%d, way_idx=%d, tag=0x%x, fb_tag=0x%x, pc=0x%x, taken=%d\n",
                    fb->fbid, t, set_idx, w, e.tag, fb->tag, h->GetBundlePosPC(), fb->rslv_info.taken);
                break;
            }
        }
        if (alloc_suaccelss) {
            break;
        }
    }
    if (!alloc_suaccelss) {
        stats->tageAllocFail++;
        for (idx_t t = (prim_idx==-1U ? 0 : prim_idx+1); t<ntable; t++) {
            for (auto& e : tt[t][fb->tage_info[id].lkup_info[t].set_idx]) {
                e.u.dec();
            }
        }
    }
}

void TAGE::RptHashStats(idx_t t, tag_t tag, addr_t pc, addr_t tgt)
{
    if (!cfg->hash_tag_rpt_enable) {
        return;
    }
    std::map<uint64_t, HashPCInfo> &hashMap = stats->tageHashMap[t];
    DstInfo dstInfo = DstInfo();
    dstInfo.pc = pc;
    dstInfo.tgt = tgt;
    dstInfo.num = 1;
    if (hashMap.count(tag) == 0) {
        HashPCInfo tempInfo;
        tempInfo.pcMap.emplace(pc, dstInfo);
        tempInfo.confilictNum = 1;
        tempInfo.allocNum = 1;
        hashMap.emplace(tag, tempInfo);
        return;
    }
    if (hashMap[tag].pcMap.count(pc) != 0) {
        hashMap[tag].pcMap[pc].num++;
        hashMap[tag].allocNum++;
        return;
    }
    hashMap[tag].pcMap.emplace(pc, dstInfo);
    hashMap[tag].confilictNum++;
    hashMap[tag].allocNum++;
    stats->tageConfNum++;
}

bool TAGE::MisPrim(idx_t primIdx)
{
    return (primIdx != ntable - 1);
}

idx_t TAGE::calcSetIdx(ghr_t* hist, addr_t pc, idx_t table_idx) {
    addr_t aligned_pc = cfg->tage_index_aligned_fb? utils->CalcFBIdx(pc) : utils->CalcPCIdx(pc);
    addr_t folded_pc = utils->foldPC(aligned_pc, cfg->tage_nset_log2[table_idx]);
    addr_t folded_ghr = utils->foldHist(hist, hist_len[table_idx], cfg->tage_nset_log2[table_idx]);
    return (folded_pc ^ folded_ghr) & (nset[table_idx]-1);
}

tag_t TAGE::calcTag(ghr_t* hist, addr_t pc, idx_t table_idx) {
    addr_t aligned_pc = cfg->tage_index_aligned_fb? utils->CalcFBIdx(pc) : utils->CalcPCIdx(pc);
    addr_t folded_pc = utils->foldPC(aligned_pc >> 2, tag_len[table_idx]);
    addr_t folded_hist_tag0 = utils->foldHist(hist, hist_len[table_idx], tag_len[table_idx]);
    addr_t folded_hist_tag1 = utils->foldHist(hist, hist_len[table_idx], tag_len[table_idx]-1);
    return (folded_pc ^ folded_hist_tag0 ^ folded_hist_tag1<<1) & ((1ULL<<tag_len[table_idx])-1);
}

}

} // namespace JCore
