#include "bctrl/bfu/bfu_btlb.h"

#include "../configs/bfu_config.h"
#include "bctrl/bfu/bfu.h"
#include "bctrl/bfu/bfu_logger.h"
#include "bctrl/bfu/bfu_utils.h"
#include "bctrl/bfu/common/mem_req.h"

namespace JCore {


using namespace std;

namespace NS_CORE {

BTLB::BTLB() {}

BTLB::~BTLB() {}

void BTLB::Build() {
    nset = cfg->btlb_nset;
    nway = cfg->btlb_nway;
    array = vector<vector<BTLBEntry>>{nset, vector<BTLBEntry>{nway}};
    for (set_t s=0; s<nset; s++) {
        for (way_t w=0; w<nway; w++) {
            array[s][w].set_idx = s;
            array[s][w].way_idx = w;
        }
    }
    rep = std::make_shared<NMRU>(nset, nway);
}

void BTLB::Lookup(PtrFB const& fb) {
    assert(fb != nullptr);
    set_t set_idx = calcSetIdx(fb->va);
    BTLBEntry* hit_entry = nullptr;
    for (auto& e : array[set_idx]) {
        if (TagMatch(fb, e)) {
            hit_entry = &e;
            break;
        }
    }
    if (hit_entry != nullptr) { // update fb->pa on hit
        fb->pa = utils->VA2PA(hit_entry->pa_pg, fb->va);
        rep->touch(hit_entry->set_idx, hit_entry->way_idx);
        logger->debug("BTLB", F1, "fbid=%d hit btlb, va=0x%x, pa=0x%x\n", fb->fbid, fb->va, fb->pa);
    } else { // stall
        stall_fb = fb;
        if (!hitMissQ(fb)) {
            if (bfu->L2CAvailable()) {
                SendMissReq(fb);
            } else {
                need_retry = true;
                logger->debug("BTLB", F1, "fbid=%d miss btlb, MMU not available, need retry\n", fb->fbid);
            }
        } else {
            logger->debug("BTLB", F1, "fbid=%d hit missq, va=0x%x\n", fb->fbid, fb->va);
        }
    }

    stats->btlb_aaccelss ++;
    if (hit_entry == nullptr) stats->btlb_miss ++;
}

void BTLB::SendMissReq(PtrFB const& fb) {
    addr_t va_pg = utils->AlignToPG(fb->va);
    if (va_pg == 0) {
        return;
    }
    set_t set_idx = calcSetIdx(fb->va);
    way_t victim_way_idx = rep->getVictim(set_idx);
    missq.emplace_back(set_idx, victim_way_idx, va_pg);
    auto mreq = make_shared<MemReq>(MemReq::BTLB, va_pg);
    logger->debug("BTLB", F1, "fbid=%d send BTLB miss req, va=0x%x, MMU req sid=%d\n", fb->fbid, fb->va, mreq->getSID());
}

bool BTLB::hitMissQ(PtrFB const& fb) {
    assert(missq.size() <= cfg->btlb_nostd);
    for (auto& e : missq) {
        if (e.va_pg == utils->AlignToPG(fb->va)) return true;
    }
    return false;
}

void BTLB::flush() {
    stall_fb = nullptr;
    need_retry = false;
}

void BTLB::handleRefill(addr_t va_pg) {
    auto it = find_if(missq.begin(), missq.end(), [&](MissQEntry& e){ return va_pg == e.va_pg; });
    assert(it != missq.end());

    if (stall_fb != nullptr && va_pg == utils->AlignToPG(stall_fb->va)) {
        // update TLB
        addr_t pa_pg = va_pg;
        array[it->set_idx][it->way_idx].va_pg = pa_pg;
        array[it->set_idx][it->way_idx].pa_pg = pa_pg;
        rep->insert(it->set_idx, it->way_idx);
        // update fb
        stall_fb->pa = utils->VA2PA(pa_pg, stall_fb->va);
        // release stall state
        stall_fb = nullptr;
        logger->debug("BTLB", F1, "handle BTLB refill, va_pg=0x%x, pa_pg=0x%x\n", va_pg, pa_pg);
    }
    // TODO: should we allocate the refilled line on a mispredicted refill
    missq.erase(it);
}

bool BTLB::isStall() {
    return stall_fb != nullptr || missq.size() >= cfg->btlb_nostd;
}

void BTLB::retry() {
    if (need_retry && bfu->L2CAvailable()) {
        assert(stall_fb);
        SendMissReq(stall_fb);
        need_retry = false;
    }
}

set_t BTLB::calcSetIdx(addr_t va) {
    return (va>>12) & (cfg->btlb_nset - 1);
}

bool BTLB::TagMatch(PtrFB const& fb, BTLBEntry const& e) {
    return utils->AlignToPG(fb->va) == e.va_pg && rep->isValid(e.set_idx, e.way_idx);
}

}

} // namespace JCore
