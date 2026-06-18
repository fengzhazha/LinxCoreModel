#ifndef STATION_STAT_H
#define STATION_STAT_H

#include "DataStruct/base_stats.h"
#include "interface/ConstConfig.h"
#include "Request.h"

namespace JCore {

class StationStat : public NS_PLAT::BaseStats  {
public:
    std::string nodeName;
    uint32_t id;
    uint64_t cycles = 0;
    uint64_t ldReqCntOfstart = 0;
    uint64_t stReqCntOfstart = 0;
    uint64_t ldReqCntOfEnd = 0;
    uint64_t stReqCntOfEnd = 0;
    uint64_t ldRespCntOfStart = 0;
    uint64_t ldRespCntOfEnd = 0;
    uint64_t stRespCntOfStart = 0;
    uint64_t stRespCntOfEnd = 0;

    uint64_t ldReqCntOfstartSize = 0;
    uint64_t stReqCntOfstartSize = 0;
    uint64_t ldReqCntOfEndSize = 0;
    uint64_t stReqCntOfEndSize = 0;
    uint64_t ldRespCntOfStartSize = 0;
    uint64_t ldRespCntOfEndSize = 0;
    uint64_t stRespCntOfStartSize = 0;
    uint64_t stRespCntOfEndSize = 0;

    uint64_t totalReq2RespLatency = 0;

    uint64_t spbOccupiedCycle[static_cast<size_t>(BufferType::COUNT)] = {0};
    uint64_t spbFullCycle[static_cast<size_t>(BufferType::COUNT)] = {0};
    uint64_t spbOccupiedSize[static_cast<size_t>(BufferType::COUNT)] = {0};
    uint64_t mgbOccupiedCycle[static_cast<size_t>(BufferType::COUNT)] = {0};
    uint64_t mgbFullCycle[static_cast<size_t>(BufferType::COUNT)] = {0};
    uint64_t mgbOccupiedSize[static_cast<size_t>(BufferType::COUNT)] = {0};

    uint64_t ringBusyCW[static_cast<size_t>(ChannelType::COUNT)] = {0};
    uint64_t ringBusyCC[static_cast<size_t>(ChannelType::COUNT)] = {0};
    uint64_t onRingCCBackPressure[static_cast<size_t>(ChannelType::COUNT)] = {0};
    uint64_t offRingCCBackPressure[static_cast<size_t>(ChannelType::COUNT)] = {0};
    uint64_t onRingCWBackPressure[static_cast<size_t>(ChannelType::COUNT)] = {0};
    uint64_t offRingCWBackPressure[static_cast<size_t>(ChannelType::COUNT)] = {0};
    uint64_t ccNotOnRingByVecFromCore[static_cast<size_t>(ChannelType::COUNT)] = {0};
    uint64_t ccNotOnRingByCubeFromCore[static_cast<size_t>(ChannelType::COUNT)] = {0};
    uint64_t ccNotOnRingByTmaFromCore[static_cast<size_t>(ChannelType::COUNT)] = {0};
    uint64_t ccNotOnRingByVec[static_cast<size_t>(ChannelType::COUNT)] = {0};
    uint64_t ccNotOnRingByCube[static_cast<size_t>(ChannelType::COUNT)] = {0};
    uint64_t ccNotOnRingByTma[static_cast<size_t>(ChannelType::COUNT)] = {0};
    uint64_t ccNotOnRingTotal[static_cast<size_t>(ChannelType::COUNT)] = {0};
    uint64_t cwNotOnRingByVec[static_cast<size_t>(ChannelType::COUNT)] = {0};
    uint64_t cwNotOnRingByCube[static_cast<size_t>(ChannelType::COUNT)] = {0};
    uint64_t cwNotOnRingByTma[static_cast<size_t>(ChannelType::COUNT)] = {0};
    uint64_t cwNotOnRingByVecFromCore[static_cast<size_t>(ChannelType::COUNT)] = {0};
    uint64_t cwNotOnRingByCubeFromCore[static_cast<size_t>(ChannelType::COUNT)] = {0};
    uint64_t cwNotOnRingByTmaFromCore[static_cast<size_t>(ChannelType::COUNT)] = {0};
    uint64_t cwNotOnRingTotal[static_cast<size_t>(ChannelType::COUNT)] = {0};
    uint64_t reqTotalCnt[static_cast<size_t>(ChannelType::COUNT)] = {0};
    uint64_t dataTotalCnt[static_cast<size_t>(ChannelType::COUNT)] = {0};
    uint64_t frqReadOccupiedCycle = 0;
    uint64_t frqReadFullCycle = 0;
    uint64_t frqReadOccupiedSize = 0;
    uint64_t frqWriteOccupiedCycle = 0;
    uint64_t frqWriteFullCycle = 0;
    uint64_t frqWriteOccupiedSize = 0;
    uint64_t onRingCycle = 0;
    uint64_t offRingCycle = 0;

    // sampling start
    uint64_t samplingCycles = 0;
    uint64_t samplingLdReqCntOfstart = 0;
    uint64_t samplingStReqCntOfstart = 0;
    uint64_t samplingLdReqCntOfEnd = 0;
    uint64_t samplingStReqCntOfEnd = 0;
    uint64_t samplingLdRespCntOfStart = 0;
    uint64_t samplingLdRespCntOfEnd = 0;
    uint64_t samplingStRespCntOfStart = 0;
    uint64_t samplingStRespCntOfEnd = 0;

    uint64_t samplingLdReqCntOfstartSize = 0;
    uint64_t samplingStReqCntOfstartSize = 0;
    uint64_t samplingLdReqCntOfEndSize = 0;
    uint64_t samplingStReqCntOfEndSize = 0;
    uint64_t samplingLdRespCntOfStartSize = 0;
    uint64_t samplingLdRespCntOfEndSize = 0;
    uint64_t samplingStRespCntOfStartSize = 0;
    uint64_t samplingStRespCntOfEndSize = 0;

    uint64_t samplingTotalReq2RespLatency = 0;
    uint64_t samplingSpbOccupiedCycle[static_cast<size_t>(BufferType::COUNT)] = {0};
    uint64_t samplingSpbFullCycle[static_cast<size_t>(BufferType::COUNT)] = {0};
    uint64_t samplingSpbOccupiedSize[static_cast<size_t>(BufferType::COUNT)] = {0};
    uint64_t samplingMgbOccupiedCycle[static_cast<size_t>(BufferType::COUNT)] = {0};
    uint64_t samplingMgbFullCycle[static_cast<size_t>(BufferType::COUNT)] = {0};
    uint64_t samplingMgbOccupiedSize[static_cast<size_t>(BufferType::COUNT)] = {0};
    uint64_t samplingFrqReadOccupiedCycle = 0;
    uint64_t samplingFrqReadFullCycle = 0;
    uint64_t samplingFrqReadOccupiedSize = 0;
    uint64_t samplingFrqWriteOccupiedCycle = 0;
    uint64_t samplingFrqWriteFullCycle = 0;
    uint64_t samplingFrqWriteOccupiedSize = 0;
    uint64_t samplingOnRingCycle = 0;
    uint64_t samplingOffRingCycle = 0;
    using BaseStats::BaseStats;
    using BaseStats::Reset;
    using BaseStats::report;
    void Reset();
    void Report(std::string &name);
    void SamplingStash();
    void ReportSampling(std::string name);
};


} // namespace JCore

#endif