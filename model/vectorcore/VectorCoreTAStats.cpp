#include "vectorcore/VectorCoreTAStats.h"

namespace JCore {

void VectorCoreTAStats::Reset()
{
    totalLoad = 0;
    loadReqFromVec = 0;
    loadMiss = 0;
    loadHit = 0;
    storeReqFromVec = 0;
    storeHit = 0;
    storeMiss = 0;
}

void VectorCoreTAStats::Report(const std::string &name)
{
}

} // namespace JCore