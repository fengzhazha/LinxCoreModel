#include "cube/CubeCoreStats.h"

namespace JCore {


void CubeCoreStats::Reset()
{
    exeTileOpCount = 0;
    cubeThreadUsedCycles.clear();
}

void CubeCoreStats::Report(std::string &name)
{
    loadRange.UnionRanges();
    mmadRange.UnionRanges();
    acccvtRange.UnionRanges();
    rpt->ReportTitle("CubeCore " + name + " Statistics");
    uint64_t totalWorkingCycles = cubeThreadUsedCycles[0];
    uint64_t totalCycle = totalCycles;
    const uint64_t flopsTimes = 2;
    const uint64_t coefficient = 1024;
    const double freq = 1.65;
    double flops = (static_cast<double>(totalCubeUsed) * static_cast<double>(flopsTimes) /
                   static_cast<double>(totalCycle)) / static_cast<double>(coefficient) * freq;
    rpt->ReportVal("Cube core TFLOPS(1.65GHz)", flops);
    rpt->ReportValAndPct("Cube core alu utilization", totalCubeUsed, totalCycle);
    rpt->ReportValAndPct("Cube core utilization(Run TileOp, header)", totalWorkingCycles, totalCycles);
    rpt->ReportValAndPct("Cube core L0CWR ratio", totalExeUopCount, totalWorkingCycles);
    rpt->ReportValAndPct("Cube core load wait ratio", totalWorkingCycles - totalExeUopCount,
                         totalWorkingCycles);
    rpt->ReportValAndPct("Cube core load port data channel utilization", toCoreCount, totalToCoreCycles);
    rpt->ReportAvg("Cube core uop load average used cycle", totalLoadCycle, totalExeUopCount);
    rpt->ReportAvg("Cube core load req average used cycle", totalLoadReqCycle, totalLoadCount);
    rpt->ReportVal("Cube core exe tileop count", exeTileOpCount);
    rpt->ReportAvg("Cube core average acc use", accTotalUse, totalWorkingCycles);
    rpt->ReportAvg("Cube core average l0a entry use", l0aTotalUse, totalWorkingCycles);
    rpt->ReportAvg("Cube core average l0b entry use", l0bTotalUse, totalWorkingCycles);
    rpt->ReportAvg("Cube core average l0c entry use", l0cTotalUse, totalWorkingCycles);
    rpt->ReportAvg("Cube core average l0a entry use busy", l0AUseCnt, l0AUseVld);
    rpt->ReportAvg("Cube core average l0b entry use busy", l0BUseCnt, l0BUseVld);
    rpt->ReportAvg("Cube core average l0c entry use busy", l0CUseCnt, l0CUseVld);
    rpt->ReportVal("Cube core exe uop count", totalUopCount);
    rpt->ReportVal("Cube core total load count", totalLoadCount);
    rpt->ReportVal("Cube core total store count", totalStoreCount);
    rpt->ReportValAndPct("Cube core cannot pick main chain uop ratio", mainChainCannotPickCycle, totalWorkingCycles);
    rpt->ReportValAndPct("Cube core pick cub chain uop ratio", pickSubChainUopCount, mainChainCannotPickCycle);
    rpt->ReportVal("Cube busy cycle", cubeBusyCycle);
    rpt->ReportValAndPct("Cube core mmad busy cycle", maddBusyCycle, cubeBusyCycle);
    rpt->ReportValAndPct("Cube core load busy cycle", cubeLoadBusyCycle, cubeBusyCycle);
    rpt->ReportValAndPct("Cube core store busy cycle", acccvtBusyCycle, cubeBusyCycle);
    rpt->ReportVal("Cube core tileop mmad cycle range", mmadRange.totalCycle);
    rpt->ReportVal("Cube core tileop load cycle range", loadRange.totalCycle);
    rpt->ReportVal("Cube core tileop store cycle range", acccvtRange.totalCycle);
    auto loadMmadOverLap = CycleRangeOverLap(loadRange, mmadRange);
    rpt->ReportValAndPct("Cube core load and mmad cycle overlap", loadMmadOverLap.first, loadMmadOverLap.second);
}

void CubeCoreStats::SamplingStash()
{
    samplingCycles = totalCycles;
    samplingTotalCubeUsed = totalCubeUsed;
    samplingExeTileOpCount = exeTileOpCount;
    samplingThreadUsedCycles = cubeThreadUsedCycles;
}

void CubeCoreStats::ReportSmapling(std::string name)
{
    rpt->ReportTitle("CubeCore " + name + " Statistics");
    uint64_t totalWorkingCycles = cubeThreadUsedCycles[0] - samplingThreadUsedCycles[0];
    uint64_t totalCycle = totalCycles - samplingCycles;
    rpt->ReportValAndPct("Cube core utilization", totalCubeUsed - samplingTotalCubeUsed,
                         CUBE_ALU_WIDTH * totalCycle);
    rpt->ReportVal("Cube core exe tileop count", exeTileOpCount - samplingExeTileOpCount);
    rpt->ReportAvg("Cube core tileop average used cycle", exeTileOpCount - samplingExeTileOpCount,
                   totalWorkingCycles);
}

void CubeCoreStats::ReportUopLoadDistribute()
{
    if (cubeLoadLatencyMap.size() == 0) {
        return;
    }
    std::cout<<"Cube core uop load latency distribute: ";
    auto it = cubeLoadLatencyMap.end()--;
    uint64_t maxLatency = (*it).first;
    for (size_t i = 1; i <= maxLatency; i++) {
        if (cubeLoadLatencyMap.find(i) != cubeLoadLatencyMap.end()) {
            std::cout<<cubeLoadLatencyMap[i]<<" ";
        } else {
            std::cout<<"0 ";
        }
    }
    std::cout<<std::endl;
}

void CubeCoreStats::ReportReqLoadDistribute()
{
    if (cubeLoadReqLatencyMap.size() == 0) {
        return;
    }
    std::cout<<"Cube core req load latency distribute: ";
    auto it = cubeLoadReqLatencyMap.end()--;
    uint64_t maxLatency = (*it).first;
    for (size_t i = 1; i <= maxLatency; i++) {
        if (cubeLoadReqLatencyMap.find(i) != cubeLoadReqLatencyMap.end()) {
            std::cout<<cubeLoadReqLatencyMap[i]<<" ";
        } else {
            std::cout<<"0 ";
        }
    }
    std::cout<<std::endl;
}

void CycleRange::UnionRanges()
{
    if (cycleRange.size() == 0) {
        return;
    }
    std::vector<std::pair<uint64_t, uint64_t>> cycleRangeRes;
    cycleRangeRes.push_back(cycleRange[0]);
    auto sortFun = [](const std::pair<uint64_t, uint64_t>& rangeA, const std::pair<uint64_t, uint64_t>& rangeB) {
        return rangeA.first < rangeB.first;
    };
    std::sort(cycleRange.begin(), cycleRange.end(), sortFun);
    for (size_t i = 0; i < cycleRange.size();) {
        if (std::max(cycleRange[i].first, cycleRangeRes[cycleRangeRes.size() - 1].first) <=
            std::min(cycleRange[i].second, cycleRangeRes[cycleRangeRes.size() - 1].second)) {
            uint64_t left = std::min(cycleRange[i].first, cycleRangeRes[cycleRangeRes.size() - 1].first);
            uint64_t right = std::max(cycleRange[i].second, cycleRangeRes[cycleRangeRes.size() - 1].second);
            cycleRangeRes[cycleRangeRes.size() - 1].first = left;
            cycleRangeRes[cycleRangeRes.size() - 1].second = right;
            i++;
        } else {
            cycleRangeRes.push_back(cycleRange[i]);
        }
    }
    cycleRange = cycleRangeRes;
    for (size_t i = 0; i < cycleRange.size(); i++) {
        totalCycle += cycleRange[i].second - cycleRange[i].first + 1;
    }
}

std::pair<uint64_t, uint64_t> CubeCoreStats::CycleRangeOverLap(CycleRange& range0, CycleRange& range1) const
{
    uint64_t i = 0;
    uint64_t j = 0;
    std::pair<uint64_t, uint64_t> res(0, 0);
    while (i < range0.cycleRange.size() && j < range1.cycleRange.size()) {
        uint64_t left = std::max(range0.cycleRange[i].first, range1.cycleRange[j].first);
        uint64_t right = std::min(range0.cycleRange[i].second, range1.cycleRange[j].second);
        if (left <= right) {
            res.first += right - left + 1;
        }
        if (range0.cycleRange[i].second < range1.cycleRange[j].second) {
            i++;
        } else {
            j++;
        }
    }
    res.second = std::min(range1.totalCycle, range0.totalCycle);
    return res;
}

} // namespace JCore
