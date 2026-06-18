#pragma once

#include <vector>

#include "bctrl/bfu/bfu_component.h"
#include "bctrl/bfu/bfu_replacement.h"

namespace JCore {


namespace NS_CORE {

/* UBTB:
   Aaccelss using fetch bundle aligned PC. 
   Each entry contains prediction for the entire fetch bundle.
*/
class UBTB : public BFUComponent {
    std::vector<std::vector<UBTBEntry>> array;
    std::shared_ptr<ReplacementPolicy> rep;
public:
    UBTB();
    ~UBTB();

    void Build() override; 

    void Predict(PtrFB const& fb, pos_t pos);
    void RunAtPBTBPred(PtrFB const& fb);
    void UpdateAtMain(PtrFB const& fb, pos_t taken_pos);
    void UpdateAtResolve(PtrFB const& fb, pos_t pos);

private:
    set_t CalcSetIdx(addr_t va);
    bool TagMatch(const addr_t& tag, UBTBEntry const& e);
    UBTBEntry* Lookup(PtrFB const& fb);

};

}

} // namespace JCore
