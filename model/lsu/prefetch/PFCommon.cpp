#include "lsu/prefetch/PFCommon.h"

namespace JCore {

PFConfLevel incPFLevel(PFConfLevel level)
{ 
    return (level == VERY_AGGRESSIVE) ? VERY_AGGRESSIVE : PFConfLevel(level + 1);
}

PFConfLevel decPFLevel(PFConfLevel level)
{ 
    return (level == VERY_CONSERVATIVE) ? VERY_CONSERVATIVE : PFConfLevel(level - 1);
}

float calRate(uint64_t total_mole, uint64_t total_deno, uint64_t interval_mole, uint64_t interval_deno)
{
    float total_rate = total_deno == 0 ? 0 : (float(total_mole) / total_deno);
    float interval_rate = interval_deno == 0 ? 0 : (float(interval_mole) / interval_deno);
    float rate = 0.5 * total_rate + 0.5 * interval_rate;
    return rate;
}

} // namespace JCore