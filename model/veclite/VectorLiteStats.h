#ifndef BLOCKISA_MODEL_VECLITE_STATS_H
#define BLOCKISA_MODEL_VECLITE_STATS_H

#include <sstream>
#include "DataStruct/base_stats.h"

namespace JCore {

class VectorLiteStats : public NS_PLAT::BaseStats  {
public:
    VectorLiteStats() = default;
    ~VectorLiteStats() override {};

    uint32_t m_coreId = 0;

    uint64_t m_totalCycle = 0;
    uint64_t m_busyCycle = 0;
    uint64_t m_workingCycle = 0;
    uint64_t m_executeBlockNum = 0;
    uint64_t m_executeUopNum = 0;

    uint64_t m_stashStallCycle = 0;
    uint64_t m_robStallCycle = 0;
    uint64_t m_siqStallCycle = 0;
    uint64_t m_uiqStallCycle = 0;
    uint64_t m_tileRegRdStallCycle = 0;
    uint64_t m_tileRegWrStallCycle = 0;
    uint64_t m_tileRegWrDataNotRdyCycle = 0;

    uint64_t m_tileRegRdCount = 0;
    uint64_t m_tileRegWrCount = 0;

    // Smapling start
    uint64_t samplingCycle = 0;
    uint64_t samplingWorkingCycle = 0;
    uint64_t samplingExeBlockNum = 0;
    uint64_t samplingTaMiss = 0;
    uint64_t samplingTaHit = 0;
    uint64_t samplingTaVisitTR = 0;

    using BaseStats::Reset;
    using BaseStats::report;
    void Reset();
    void ReportLoadLatency() const;
    void Report() const;
    void SamplingStash();
    void ReportSampling(const std::string &name) const;
};


} // namespace JCore

#endif
