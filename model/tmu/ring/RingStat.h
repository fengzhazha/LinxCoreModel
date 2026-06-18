#ifndef RING_STAT_H
#define RING_STAT_H

#include <vector>

#include "DataStruct/base_stats.h"
#include "Request.h"

namespace JCore {

class RingStat : public NS_PLAT::BaseStats  {
public:
    uint64_t cycles = 0;
    uint64_t totalPktCnt = 0;
    uint64_t totalRingCycle = 0;
    uint64_t totalLdReqPktCnt = 0;
    uint64_t totalLdReqRespRingCycle = 0;

    std::map<int, std::string> nodeToPEMap;
    std::vector<uint64_t> numCubeLoadReqs;
    std::vector<uint64_t> numCubeStoreReqs;
    std::vector<uint64_t> numVectorLoadReqs;
    std::vector<uint64_t> numVectorStoreReqs;
    std::vector<uint64_t> numTMALoadReqs;
    std::vector<uint64_t> numTMAStoreReqs;

    std::vector<uint64_t> numPackets;
    std::vector<uint64_t> sumIdealHops;
    std::vector<uint64_t> sumRealHops;
    std::vector<uint64_t> sumOnRingLatency;
    std::vector<uint64_t> sumRingTraversalLatency;
    std::vector<uint64_t> sumOffRingLatency;

    std::vector<std::vector<uint64_t>> nodeOccuiedCycles;
    std::vector<std::vector<uint64_t>> linkOccuiedCycles;
    std::vector<std::vector<uint64_t>> nodeStashCycles;
    std::vector<std::vector<uint64_t>> linkMultiRoundOccuiedCycles;
    uint64_t totalCubeRunningCycle = 0;
    std::vector<std::vector<uint64_t>> nodeOccuiedCubeCycle;
    std::vector<std::vector<uint64_t>> linkOccuiedCubeCycle;
    std::vector<std::vector<uint64_t>> linkMultiRoundOccuiedCubeCycle;
    uint64_t totalMtcRunningCycle = 0;
    std::vector<std::vector<uint64_t>> nodeOccuiedMtcCycle;
    std::vector<std::vector<uint64_t>> linkOccuiedMtcCycle;
    std::vector<std::vector<uint64_t>> linkMultiRoundOccuiedMtcCycle;
    uint64_t totalVecRunningCycle = 0;
    std::vector<std::vector<uint64_t>> nodeOccuiedVecCycle;
    std::vector<std::vector<uint64_t>> linkOccuiedVecCycle;
    std::vector<std::vector<uint64_t>> linkMultiRoundOccuiedVecCycle;

    std::vector<std::vector<uint64_t>> offRingBackPressure;
    // sampling start
    uint64_t samplingCycles = 0;
    uint64_t samplingTotalPktCnt = 0;
    uint64_t samplingTotalRingCycle = 0;
    uint64_t samplingTotalLdReqPktCnt = 0;
    uint64_t samplingTotalLdReqRespRingCycle = 0;
    std::vector<std::vector<uint64_t>> samplingNodeOccuiedCycles;

    using BaseStats::BaseStats;
    using BaseStats::Reset;
    using BaseStats::report;
    void Init(uint32_t nodeNum);
    void Reset();
    void ReportBothRing(RingStat ccRingStat);
    void Report(std::string &name);
    void SamplingStash();
    void ReportSampling(std::string name);
};


} // namespace JCore

#endif