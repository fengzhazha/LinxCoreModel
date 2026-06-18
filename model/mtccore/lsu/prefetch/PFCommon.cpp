#include "mtccore/lsu/prefetch/PFCommon.h"

namespace JCore {

MTCPFConfLevel incPFLevel(MTCPFConfLevel level)
{
    return (level == MTCVERY_MTCAGGRESSIVE) ? MTCVERY_MTCAGGRESSIVE : MTCPFConfLevel(level + 1);
}

MTCPFConfLevel decPFLevel(MTCPFConfLevel level)
{
    return (level == MTCVERY_MTCCONSERVATIVE) ? MTCVERY_MTCCONSERVATIVE : MTCPFConfLevel(level - 1);
}

float mtccalRate(uint64_t total_mole, uint64_t total_deno, uint64_t interval_mole, uint64_t interval_deno)
{
    float totalrate = total_deno == 0 ? 0 : (float(total_mole) / total_deno);
    float intervalrate = interval_deno == 0 ? 0 : (float(interval_mole) / interval_deno);
    float rate = 0.5 * totalrate + 0.5 * intervalrate;
    return rate;
}

} // namespace JCore