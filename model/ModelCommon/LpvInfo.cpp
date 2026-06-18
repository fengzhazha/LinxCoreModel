#include "LpvInfo.h"

namespace JCore {
void SetLpv(PLpvInfo src, PLpvInfo &dst, bool force)
{
    if (!src) {
        if (force) {
            dst = nullptr;
        }
        return;
    }

    if (force) {
        dst = std::make_shared<LpvInfo>(*src);
        return;
    }

    if (!dst) {
        dst = std::make_shared<LpvInfo>(*src);
    } else {
        dst->Set(*src);
    }
}
}