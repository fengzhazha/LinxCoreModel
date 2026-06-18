#pragma once

#include "DataStruct/base_stats.h"

namespace JCore {

class IFUStats : public NS_PLAT::BaseStats {
public:
    /* \brief L1 ICache aaccelss number */
    uint64_t l1_aaccelss;
    /* \brief L1 ICache miss number */
    uint64_t l1_miss;
    /* \brief L1 ICache aaccelss total latency */
    uint64_t l1_total_lat;
    uint64_t l1Refill;
    /* \brief L1 ICache request to L2 number */
    uint64_t l2_aaccelss;
    /* \brief L2 Invalid L1 cacheline. */
    uint64_t l2_invalid;

    /* \brief Inner branch predict number */
    uint64_t pred_count;
    /* \brief Inner branch predict correct number */
    uint64_t corr_count;
    /* \brief Inner branch predict miss number */
    uint64_t mis_count;
    /* \brief Inner branch predictor not cover number */
    uint64_t nc_count;

    /* \brief IFU merge fetchReq two TPC consecutive blocks number. */
    uint64_t ifu_merge;

    uint64_t missBufferStall;
    uint64_t branch;
    uint64_t cond;
    uint64_t direct;

    using BaseStats::BaseStats;
    using BaseStats::Reset;
    using BaseStats::report;
    void Reset() { BaseStats::Reset(sizeof(IFUStats)); }
    void report(const std::string& name);
    void ReportVec(const std::string& name);
};

} // namespace JCore
