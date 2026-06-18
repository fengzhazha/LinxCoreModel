#include "mtccore/memcore/MemoryCoreTAStats.h"

namespace JCore {

void MemoryCoreTAStats::Reset()
{
    totalLoad = 0;
    loadReqFromVec = 0;
    loadMiss = 0;
    loadHit = 0;
    storeReqFromVec = 0;
    storeHit = 0;
    storeMiss = 0;
}

void MemoryCoreTAStats::Report(const std::string &name)
{
}

} // namespace JCore