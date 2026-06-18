#pragma once

#include <vector>

#include "bctrl/bfu/bfu_component.h"
#include "bctrl/bfu/bfu_replacement.h"

namespace JCore {


namespace NS_CORE {

/* PBTB:
   Aaccelss using va of previous fb and tag using va of current fb. 
*/
class PBTB : public BFUComponent {
    std::vector<std::vector<UBTBEntry>> array;
    std::shared_ptr<ReplacementPolicy> rep;

public:
    PBTB();
    ~PBTB();

    void Build() override; 

    void Predict(PtrFB const& fb, const pos_t& pos);
    void UpdateAtMain(PtrFB const& fb, pos_t taken_pos);

private:
    set_t CalcSetIdx(addr_t va);
    bool TagMatch(const addr_t& tag, UBTBEntry const& e);
    UBTBEntry* Lookup(PtrFB const& fb);
};

}

} // namespace JCore
