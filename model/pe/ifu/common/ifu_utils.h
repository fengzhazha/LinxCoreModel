#pragma once
#include "bctrl/bfu/bfu_common.h"

namespace JCore {


namespace PE_IFU {

class IFUUtils {
public:
    static inline uint64_t calcAddr(uint64_t tpc) { return tpc & ADDR_MASK; }
};
}

} // namespace JCore
