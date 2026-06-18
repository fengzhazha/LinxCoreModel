#ifndef MEM_STATS_H
#define MEM_STATS_H

#include <sstream>
#include <vector>

#include "DataStruct/base_stats.h"

namespace JCore {

class MemoryCoreStats : public NS_PLAT::BaseStats  {
public:
    MemoryCoreStats() = default;
    virtual ~MemoryCoreStats() {};

    uint64_t m_totalCycle = 0;
    uint64_t m_workingCycle = 0;
    uint64_t m_executeBlockNum = 0;
    uint32_t m_threadNum = 0;
    uint64_t m_taMiss = 0;
    uint64_t m_taHit = 0;
    uint64_t m_taVisitTileRegister = 0;
    std::vector<uint64_t> m_perThreadWorkingCycle;

    // Smapling start
    uint64_t samplingCycle = 0;
    uint64_t samplingWorkingCycle = 0;
    uint64_t samplingExeBlockNum = 0;
    uint64_t samplingTaMiss = 0;
    uint64_t samplingTaHit = 0;
    uint64_t samplingTaVisitTR = 0;

    uint64_t tile_total_load_request = 0;
    uint64_t tile_total_store_request = 0;
    uint64_t tile_total_load_byte = 0;
    uint64_t tile_total_load_lat = 0;
    uint64_t tile_total_store_byte = 0;
    uint64_t tile_total_store_lat = 0;
    uint64_t tile_load_cache_hit = 0;
    uint64_t tile_lsu_ldq_full = 0;
    uint64_t tile_lsu_stq_full = 0;
    uint64_t tile_lsu_vab_full = 0;
    uint64_t tile_lsu_scb_full = 0;
    uint64_t tile_lsu_avg_lhq_occ = 0;
    uint64_t tile_lsu_avg_lhq_lifetime = 0;
    uint64_t tile_lsu_avg_stq_occ = 0;
    uint64_t tile_lsu_avg_stq_lifetime = 0;
    uint64_t tile_lsu_avg_scb_occ = 0;
    uint64_t tile_lsu_avg_scb_lifetime = 0;

    uint64_t tile_load_to_use[5];

    using BaseStats::Reset;
    using BaseStats::report;
    void Reset();
    void Report_load_latency();
    void Report(const std::string &name);
    void SamplingStash();
    void ReportSampling(const std::string &name);
};


} // namespace JCore

#endif