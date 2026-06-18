#include "bctrl/bfu/bfu_ghrq.h"

#include "../configs/bfu_config.h"
#include "bctrl/bfu/bfu_logger.h"
#include "bctrl/bfu/bfu_utils.h"

namespace JCore {
using namespace std;

namespace NS_CORE {

GHRQ::GHRQ() {}

GHRQ::~GHRQ() {}

void GHRQ::Build() {
    ghrHistNbit = cfg->ghr_hist_nbit_base + id;
    ghr_3old = cfg->ghr_3old && !cfg->ghr_path_hist;
    ndepth = cfg->ghrq_depth;
    q = std::make_shared<BitArray>(ndepth);
    bfuGhrNOld = cfg->bfu_ntaken * F3 + 1;
    wptr = std::vector<idx_t>(bfuGhrNOld, 0);
}

void GHRQ::SetID(uint32_t i)
{
    id = i;
}

void GHRQ::appendHist(vector<PathHist>& v) {
    ghr_t hist = 0; // MSB is most recent hist
    msize_t len = 0;

    for (auto& p : v) {
        if (cfg->ghr_path_hist) {
            // 3bit path history
            if (p.taken || (p.type == BranchType::BLK_BR_FALL && cfg->ghr_ft_as_taken)) {
                if (cfg->ghr_path_hist_selfloop && p.pc == p.tgt) { // self loop
                    recentHistPlus1(ghrHistNbit);
                    return;
                }
                ghr_t histTemp = (utils->foldPC(utils->CalcPCIdx(p.pc) ^ (utils->CalcPCIdx(p.tgt) >> 1), ghrHistNbit) << len);
                hist = hist | histTemp;
                len += ghrHistNbit;
                RptHashStats(p.pc, p.tgt, histTemp);
            }
        } else {
            // only consider conditional branches
            if (utils->IsCond(p.type)) {
                hist = hist | (static_cast<addr_t>(p.taken) << len);
                len ++;
            }
        }
    }
    // logger here
    if (cfg->ghr_path_hist && !cfg->ghr_ft_as_taken) {
        assert(len <= ghrHistNbit);
    }
    appendHist(hist, len);
}

void GHRQ::RptHashStats(addr_t pc, addr_t tgt, ghr_t hist)
{
    if (!cfg->hash_tag_rpt_enable) {
        return;
    }
    std::map<uint64_t, HashPCInfo> &hashMap = stats->histHashMap;
    DstInfo dstInfo = DstInfo();
    dstInfo.pc = pc;
    dstInfo.tgt = tgt;
    dstInfo.num = 1;
    if (hashMap.count(hist) == 0) {
        HashPCInfo tempInfo;
        tempInfo.pcMap.emplace(pc, dstInfo);
        tempInfo.confilictNum = 1;
        tempInfo.allocNum = 1;
        hashMap.emplace(hist, tempInfo);
        return;
    }
    if (hashMap[hist].pcMap.count(pc) != 0) {
        hashMap[hist].pcMap[pc].num++;
        hashMap[hist].allocNum++;
        return;
    }
    hashMap[hist].pcMap.emplace(pc, dstInfo);
    hashMap[hist].confilictNum++;
    hashMap[hist].allocNum++;
    stats->histConfNum++;
}

void GHRQ::appendHist(ghr_t hist, msize_t len) {
    // wptr[0] is always the most recent position after write
    // MSB of hist is the most recent T/NT
    assert(len >=0 && len <= 64);
    if (len > 0) {
        ghr_t wr_mask = (1UL<<len)-1;
        hist &= wr_mask;
        q->set(wptr[0], wptr[0]+len-1, hist);
    }
    for (int i = bfuGhrNOld - 1; i!=0; i--) {
        wptr[i] = wptr[i-1];
    }
    wptr[0] = (wptr[0] + len) % ndepth;
}

void GHRQ::recentHistPlus1(msize_t len) {
    assert(0); // fixme: rewrite the following logic since we are now using GHR_LEN > 64
    assert(cfg->ghr_path_hist);
    assert(len > 0 && len <= 64);
    auto old_hist = reverseBits(getGHR(wptr[0], len), len); // most recent in MSB
    auto new_hist = utils->LastNBit(old_hist+1, len);
    idx_t new_wptr = wptr[0] < ghrHistNbit? wptr[0]+ndepth-ghrHistNbit : wptr[0]-ghrHistNbit;
    q->set(new_wptr, new_wptr+len-1, new_hist);
    // no need to shift wptrs because PHR and 3old are exclusive!
}

ghr_t GHRQ::getGHR(idx_t rptr, msize_t len) {
    assert(rptr < ndepth && len <= 64);
    // LEN entries before current wptr
    // the most recent T/NT is the MSB of GHR
    idx_t lsb = (rptr - 1 - len + 1) % ndepth;
    idx_t msb = lsb + len - 1;
    ghr_t h = q->extract(lsb, msb);
    // reverse GHR, now the most recent T/NT is the LSB of GHR
    return reverseBits(h, len);
}

void GHRQ::getGHRInfo(PtrFB const& fb, int num_vld_fb) {
    // When using 3-old scheme, num_vld_fb represents how many valid fbs are 
    // older than the current FB (F0) and are still in the pipeline.
    // Since F3 and flush have already updated GHR, we don't care its status. 
    assert(num_vld_fb >= 0 && num_vld_fb < bfuGhrNOld);
    idx_t rptr = ghr_3old? wptr[bfuGhrNOld-1-num_vld_fb] : wptr[0];
    for (int group=0; group<BFU_GHR_NGROUP; group++) {
        fb->ghr_info[id].ghr[group] = getGHR((rptr-group*64)%ndepth, 64);
    }
    fb->ghr_info[id].wptr = wptr; // in 3-old ghr scheme, wptr is actually saved at main pred
    if (cfg->debug_enable) {
        LOG_INFO_M(Unit::BFU, Stage::F0) << "GHRQ get GHR info, rptr=" << dec << rptr
            << ", wptr[0]=" << wptr[0] << ", recent^4_ghr=0x" << hex << fb->ghr_info[id].ghr[0];
    }
    logger->debug("GHRQ", NIL, "get GHR info, rptr=%d, wptr[0]=%d, recent_64_ghr=0x%llx\n", rptr, wptr[0], fb->ghr_info[id].ghr[0]);
}

void GHRQ::recoverWptr(std::vector<idx_t> wptr) {
    this->wptr = wptr;
}

void GHRQ::runAt0CyclePred(PtrFB const& fb) {
    // use ubtb prediction to update ghr
    if (!ghr_3old) {
        appendHist(fb->ubtb_info.path_hist);
    }
}

void GHRQ::runAtMainPredFlush(PtrFB const& fb) {
    if (!ghr_3old) {
        // recover ghr state 
        recoverWptr(fb->ghr_info[id].wptr);
        // append new_hist given by main prediction
        appendHist(fb->main_info.path_hist);
    }
}

void GHRQ::runAtCallFlush(PtrFB const& fb) {
    if (!ghr_3old) {
        // recover ghr state 
        recoverWptr(fb->ghr_info[id].wptr);
        // append new_hist given by main prediction
        appendHist(fb->sp_info.path_hist);
    }
}

void GHRQ::runAtMainPred(PtrFB const& fb) {
    if (ghr_3old) {
        // save wptr before updating it
        fb->ghr_info[id].wptr = wptr;
        appendHist(fb->main_info.path_hist);
        if (cfg->debug_enable) {
            LOG_INFO_M(Unit::BFU, Stage::F4) << "GHRQ run at main pred, wptr=" << dec << wptr[0]
                << ", recent_64_ghr=0x" << hex << getGHR(wptr[0], 64);
        }
        logger->debug("GHRQ", NIL, "run at main pred, wptr=%d, recent_64_ghr=0x%llx\n", wptr[0], getGHR(wptr[0], 64));
    }
}

void GHRQ::runAtUseLB(PtrFB const& fb)
{
    if (!fb->lb_info.hit) {
        return;
    }
    fb->ghr_info[id].wptr = wptr;
    appendHist(fb->main_info.path_hist);
    logger->debug("GHRQ", NIL, "run at use loop buffer, wptr=%d, recent_64_ghr=0x%llx\n", wptr[0], getGHR(wptr[0], 64));
}

void GHRQ::runAtRedirect(PtrFB const& fb) {
    // recover ghr state
    recoverWptr(fb->ghr_info[id].wptr);
    // append new_hist given by redirection
    appendHist(fb->redir_info.path_hist);
    if (cfg->debug_enable) {
        LOG_INFO_M(Unit::BFU, Stage::NA) << "GHRQ run at redirect, wptr=" << dec << wptr[0]
            << ", recent_64_ghr=0x" << hex << getGHR(wptr[0], 64);
    }
    logger->debug("GHRQ", NIL, "run at redirect, wptr=%d, recent_64_ghr=0x%llx\n", wptr[0], getGHR(wptr[0], 64));
}

void GHRQ::runAtNuke(PtrFB const& fb) {
    recoverWptr(fb->ghr_info[id].wptr);
    appendHist(fb->replay_info.path_hist);
}

ghr_t GHRQ::reverseBits(ghr_t h, msize_t len) {
    assert(len > 0 && len <= 64);
    ghr_t rv = 0;
    for (uint32_t i = 0; i <len; i++, h>>=1) {
        rv = (rv << 1) | (h & 0x01);
    }
    return rv;
}

}


} // namespace JCore
