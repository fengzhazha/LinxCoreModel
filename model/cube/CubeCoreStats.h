#ifndef CUBE_STATS_H
#define CUBE_STATS_H

#include <map>
#include <algorithm>
#include "DataStruct/base_stats.h"

namespace JCore {


constexpr uint64_t CUBE_ALU_WIDTH = 16 * 16 * 16;

class CycleRange {
public:
    std::vector<std::pair<uint64_t, uint64_t>> cycleRange;
    uint64_t totalCycle = 0;
    void UnionRanges();
};

class CubeCoreStats : public NS_PLAT::BaseStats  {
public:
    std::map<uint64_t, uint64_t> cubeThreadUsedCycles;
    std::map<uint64_t, uint64_t> cubeLoadLatencyMap;
    std::map<uint64_t, uint64_t> cubeLoadReqLatencyMap;
    uint64_t exeTileOpCount = 0;
    uint64_t totalCycles = 0;
    double   totalCubeUsed = 0;
    uint64_t totalLoadCycle = 0;
    uint64_t renameCycle = 0;
    uint64_t retireCycle = 0;
    uint64_t headerOverHeadCycle = 0;
    uint64_t totalExeUopCount = 0;
    uint64_t totalLoadCount = 0;
    uint64_t totalLoadReqCycle = 0;
    uint64_t totalStoreCount = 0;
    uint64_t totalWorkingCycle = 0;
    uint64_t totalUopCount = 0;
    uint64_t toCoreCount = 0;
    uint64_t accTotalUse = 0;
    uint64_t l0aTotalUse = 0;
    uint64_t l0bTotalUse = 0;
    uint64_t l0cTotalUse = 0;
    uint64_t l0AUseCnt = 0;
    uint64_t l0BUseCnt = 0;
    uint64_t l0CUseCnt = 0;
    uint64_t l0AUseVld = 0;
    uint64_t l0BUseVld = 0;
    uint64_t l0CUseVld = 0;
    uint64_t mainChainCannotPickCycle = 0;
    uint64_t pickSubChainUopCount = 0;
    uint64_t cubeBusyCycle = 0;
    uint64_t maddBusyCycle = 0;
    uint64_t cubeLoadBusyCycle = 0;
    uint64_t acccvtBusyCycle = 0;

    // every tileop start from the first L0CWR and end at the last UOP L0CWR
    uint64_t totalUopExeCycles = 0;

    // every tileop start from the first TMU To Core and end at the last TMU To Core
    uint64_t totalToCoreCycles = 0;
    // sampling start
    uint64_t samplingCycles = 0;
    uint64_t samplingTotalCubeUsed = 0;
    uint64_t samplingExeTileOpCount = 0;
    std::map<uint64_t, uint64_t> samplingThreadUsedCycles;
    CycleRange loadRange;
    CycleRange mmadRange;
    CycleRange acccvtRange;

    using BaseStats::BaseStats;
    using BaseStats::Reset;
    using BaseStats::report;
    void Reset();
    void Report(std::string &name);
    void SamplingStash();
    void ReportSmapling(std::string name);
    void ReportUopLoadDistribute();
    void ReportReqLoadDistribute();
    std::pair<uint64_t, uint64_t> CycleRangeOverLap(CycleRange& range0, CycleRange& range1) const;
};


} // namespace JCore

#endif