#pragma once

#include "bctrl/bfu/bfu_component.h"

namespace JCore {


namespace NS_CORE {
class RAS : public BFUComponent{
    struct SpecEntry {
        bool vld = false; 
        idx_t nos = 0; // next on stack
        addr_t tgt = -1U;
        idx_t cmt_ptr = 0; // ptr to cmt table
    };
public:
    idx_t spec_wptr = 0; // next write ptr
    idx_t spec_tos = 0;  // top of stack
    idx_t spec_bos = 0;  // bottom of stack
    std::vector<SpecEntry> spec_table;

    // idx_t cmt_bos = 0; // FIXME: do we need bos for commit table? (if ras empty is not a big problem)
    idx_t cmt_alloc_ptr = 0;
    std::vector<addr_t> cmt_table;

// public:
    RAS();
    ~RAS();
    void Build() override;

    addr_t Predict(PtrFB const& fb, pos_t pos);
    
    void runAt0CyclePred(PtrFB const& fb, pos_t pos);
    void runAtLB0CyclePred(PtrFB const& fb);
    void runAtMainPredFlush(PtrFB const& fb);
    void runCallFlush(PtrFB const& fb, pos_t pos);
    void runAtMainPred(PtrFB const& fb);
    void runAtNuke(PtrMachineInst const& h, PtrFB const& fb);
    void runAtRedirect(PtrFB const& fb);

    void RunAtCommit(PtrMachineInst const& h, PtrFB const& fb);

    bool needStall();
private:
    void backup(PtrFB const& fb);
    void restore(PtrFB const& fb);
    void handleCall(addr_t next_pc);
    void handleReturn();
    idx_t incPtr(idx_t const& ptr);
    idx_t decPtr(idx_t const& ptr);
    uint64_t valids();
};

}

} // namespace JCore
