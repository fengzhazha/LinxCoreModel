#pragma once

#include "DataStruct/base_stats.h"

namespace JCore {

class FlushStats : public NS_PLAT::BaseStats {
public:
    uint64_t IntraBlockMisprediction = 0;
    uint64_t InterBlockMisprediction = 0;
    uint64_t IntraBlockMemoryAaccelssConflict = 0;
    uint64_t InterBlockMemoryAaccelssConflict = 0;
    std::vector<uint64_t> smtIntraBlockMispredictionArray;
    std::vector<uint64_t> smtInterBlockMispredictionArray;
    std::vector<uint64_t> smtIntraBlockMemoryAaccelssConflictArray;
    std::vector<uint64_t> smtInterBlockMemoryAaccelssConflictArray;
    uint64_t efficientInterConflict = 0;
    uint64_t efficientIntraConflict = 0;
    uint64_t efficientInerMisprediction = 0;
    uint64_t efficientIntraMisprediction = 0;
    uint64_t efficientBlockReplay = 0;
    uint64_t LoadQueueStall = 0;
    uint64_t StoreQueueStall = 0;
    uint64_t IssueQueueStall = 0;
    uint64_t stackGetCorrect = 0;
    uint64_t stackGetIncorrect = 0;

    using BaseStats::BaseStats;
    virtual ~FlushStats() {};
    using BaseStats::Reset;
    using BaseStats::report;
    void Reset();
    void report();
    void ReportFlush();
    void InitSmtPmu(uint32_t sthreadNum);
};

} // namespace JCore
