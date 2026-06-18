#pragma once

#include <list>
#include <vector>

#include "bctrl/bfu/bfu_component.h"
#include "bctrl/bfu/bfu_replacement.h"
#include "core/Packet.h"

namespace JCore {


namespace NS_CORE {
    
class BHC : public BFUComponent {
    enum class CacheState {
        M=0, E, S, I
    };
    struct CacheEntry {
        addr_t pa_cl {0};
        CacheState state {CacheState::I};
        uint512_t data;
        set_t set_idx {0};
        way_t way_idx {0};
        // TODO: add prefetch feedback
    };

    struct MissQEntry {
        addr_t va_cl;
        addr_t pa_cl;
        uint32_t stid = -1U;
        bool sent = false;
        bool is_pf = false; // BHC request type
        time_t enqueue_time; // for timeout check
        MissQEntry(addr_t va_cl, addr_t pa_cl, uint32_t stid, bool is_pf, time_t enqueue_time)
        : va_cl(va_cl), pa_cl(pa_cl), stid(stid), sent(false), is_pf(is_pf), enqueue_time(enqueue_time) {}
    };

    std::vector<std::vector<CacheEntry>> cache;
    ReplacementPolicy* rep {nullptr};
    std::list<MissQEntry> missq;

    PtrFB stall_fb = nullptr;
    PtrFB stall_local_fb = nullptr;
    
public:
    BHC();
    ~BHC();

    void Build() override;

    bool fetch(PtrFB const& fb);
    bool prefetch(PtrHWPFInfo const& pfi);

    void SendMissReq();
    bool invalidate(addr_t const pa);
    std::pair<bool, addr_t> refill(addr_t const pa, uint512_t const& data);
        
    bool needStall();
    bool LocalFetchStall();
    void flush(uint32_t stid);
    void flushGlobal(uint32_t stid);
    void flushLocal(PtrFB fb);
    PtrFB getStallLocalFB();

private:
    set_t CalcSetIdx(addr_t const va);
    CacheEntry* Lookup(addr_t const va, addr_t const pa);
    MissQEntry* lookupMissQ(addr_t const va);
    void getHeader(PtrFB const& fb, pos_t nth_cl);
    void releaseStallFB(bool global, set_t const set, way_t const way, addr_t const pa_cl);
    bool paMatch(PtrFB const& fb, addr_t const pa_cl);
};
}   // namespace NS_CORE

} // namespace JCore
