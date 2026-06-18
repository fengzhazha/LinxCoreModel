#ifndef TILE_REG_STAT_H
#define TILE_REG_STAT_H

#include "DataStruct/base_stats.h"

namespace JCore {

class TileRegStat : public NS_PLAT::BaseStats  {
public:
    uint64_t ldReqCnt = 0;
    uint64_t ldConfCnt = 0;
    uint64_t totalLdSize = 0;
    uint64_t stReqCnt = 0;
    uint64_t stConfCnt = 0;
    uint64_t totalStSize = 0;
    uint64_t allocTaWayCnt = 0;
    uint64_t allocToWayCnt = 0;
    uint64_t hitTaWayCnt = 0;
    uint64_t hitToWayCnt = 0;
    uint64_t tileTagAaccelss = 0;
    uint64_t effectiveToWayCnt = 0;
    uint64_t freeWayCnt = 0;
    uint64_t flushWayCnt = 0;

    // sampling start
    uint64_t samplingLdReqCnt = 0;
    uint64_t samplingLdConfCnt = 0;
    uint64_t samplingTotalLdSize = 0;
    uint64_t samplingStReqCnt = 0;
    uint64_t samplingStConfCnt = 0;
    uint64_t samplingTotalStSize = 0;

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