#pragma once

#include <list>

#include "bctrl/bfu/bfu_component.h"

namespace JCore {


namespace NS_CORE {

class HWPF : public BFUComponent {    
    std::list<PtrHWPFInfo> tgt_list;
public:
    void Build() override;
    void flush(uint32_t stid);
    void observeFetch(PtrFB const& fb);
    PtrHWPFInfo getTarget();
};

}

} // namespace JCore
