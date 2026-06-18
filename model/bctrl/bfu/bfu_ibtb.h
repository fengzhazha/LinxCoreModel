#pragma once
#include <vector>

#include "bctrl/bfu/bfu_component.h"

namespace JCore {


namespace NS_CORE {

class IBTB : public BFUComponent {
    struct IBTBEntry {
        addr_t tgt = 0;
        tag_t tag = -1;
        UCnt u;
    };
    std::vector<std::vector<std::vector<IBTBEntry>>> tt;
    std::vector<msize_t> ibtb_nset;
    std::vector<msize_t> ibtb_hist_len;
    
public:
    IBTB();
    ~IBTB();  

    void Build() override;
    void Lookup(PtrFB const& fb);
    void Predict(PtrFB const& fb, addr_t pc);
    void update(PtrFB const& fb, pos_t pos);

private:
    void allocate(PtrFB const& fb);
    pos_t getFirstIndirectPos(PtrFB const& fb);

    set_t calcBaseSetIdx(addr_t pc);
    set_t calcTaggedSetIdx(addr_t pc, ghr_t* ghr, idx_t table_idx);
    tag_t calcTag(addr_t pc, ghr_t* ghr, idx_t table_idx);
};

}

} // namespace JCore
