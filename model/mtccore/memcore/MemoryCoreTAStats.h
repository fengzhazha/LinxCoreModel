#ifndef MEMORY_CORE_TA_STATS_H
#define MEMORY_CORE_TA_STATS_H

#include <cstdint>

#include "DataStruct/base_stats.h"

namespace JCore {

class MemoryCoreTAStats : public NS_PLAT::BaseStats {
public:
    uint64_t totalLoad = 0;
    uint64_t totaGatherlLoad = 0;
    uint64_t totaContiniouslLoad = 0;
    uint64_t loadReqFromVec = 0;
    uint64_t loadMiss = 0;
    uint64_t loadHit = 0;
    uint64_t storeReqFromVec = 0;
    uint64_t storeHit = 0;
    uint64_t storeMiss = 0;
    uint64_t wayConflict = 0;

    MemoryCoreTAStats() = default;
    virtual inline ~MemoryCoreTAStats() {};

    using BaseStats::Reset;
    void Reset();
    void Report(const std::string &name);
};

} // namespace JCore

#endif