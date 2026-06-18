#pragma once

#include <deque>
#include <vector>

#include "bctrl/bfu/bfu_component.h"
#include "bctrl/bfu/bfu_replacement.h"

namespace JCore {


namespace NS_CORE {

class BTLB : public BFUComponent {
    struct BTLBEntry {
        addr_t va_pg;
        addr_t pa_pg;

        set_t set_idx;
        way_t way_idx;
    };
    struct MissQEntry {
        set_t set_idx;
        way_t way_idx;

        addr_t va_pg;
        MissQEntry(set_t s, way_t w, addr_t a) : set_idx(s), way_idx(w), va_pg(a) {};
    };
    set_t nset;
    way_t nway;
    std::vector<std::vector<BTLBEntry>> array;
    std::deque<MissQEntry> missq;
    std::shared_ptr<ReplacementPolicy> rep;

    PtrFB stall_fb;
    bool need_retry = false;

public:
    BTLB();
    ~BTLB();

    void Build() override;
    void Lookup(PtrFB const& fb);
    bool hitMissQ(PtrFB const& fb);
    void SendMissReq(PtrFB const& fb);

    void flush();
    void handleRefill(addr_t pa);

    bool isStall();
    void retry();

private:
    set_t calcSetIdx(addr_t va);
    bool TagMatch(PtrFB const& fb, BTLBEntry const& e);

    
};

}

} // namespace JCore
