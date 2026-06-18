#ifndef VEC_STATS_H
#define VEC_STATS_H

#include <sstream>
#include "DataStruct/base_stats.h"

namespace JCore {

class VectorCoreStats : public NS_PLAT::BaseStats  {
public:
    VectorCoreStats() = default;
    virtual ~VectorCoreStats() {};

    uint64_t m_totalCycle = 0;
    uint64_t m_busyCycle = 0;
    uint64_t m_workingCycle = 0;
    uint64_t m_executeBlockNum = 0;
    uint32_t m_threadNum = 0;
    uint64_t m_taVisitTileRegister = 0;

    // Smapling start
    uint64_t samplingCycle = 0;
    uint64_t samplingWorkingCycle = 0;
    uint64_t samplingExeBlockNum = 0;
    uint64_t samplingTaMiss = 0;
    uint64_t samplingTaHit = 0;
    uint64_t samplingTaVisitTR = 0;

    uint64_t pendingCycle = 0;
    uint64_t pendingNum = 0;
    uint64_t stashNum = 0;
    uint64_t loadHitCacheStash = 0;
    uint64_t stashFlushCnt = 0;
    uint64_t loadHitCacheDemand = 0;
    uint64_t tileCacheEvictCnt = 0;
    uint64_t tileLoadPrefetchDropCnt = 0;
    uint64_t tileLoadPrefetchDrop1Cnt = 0;
    uint64_t tileLoadHitPrefetchRoadCnt = 0;
    uint64_t tile_load_hit_missq_cnt = 0;
    uint64_t tileLoadGoodPrefetchCnt = 0;
    uint64_t tileLoadBadPrefetchCnt = 0;
    uint64_t tileLoadTriggerPfCnt = 0;
    uint64_t tileLoadTrainPfCnt = 0;
    uint64_t tileHeadTriggerPfCnt = 0;
    uint64_t tileHeadTrainPfCnt = 0;

    uint64_t tile_total_load_inst = 0;
    uint64_t tile_total_gather_load_inst = 0;
    uint64_t tile_total_continious_load_inst = 0;
    uint64_t tile_total_load_request = 0;
    uint64_t tile_total_store_inst = 0;
    uint64_t tile_total_scatter_store_inst = 0;
    uint64_t tile_total_continious_store_inst = 0;
    uint64_t tile_total_store_request = 0;
    uint64_t tile_port0_load_request = 0;
    uint64_t tile_port1_load_request = 0;
    uint64_t tile_port0_load_byte = 0;
    uint64_t tile_port1_load_byte = 0;
    uint64_t tileTotalRdRequest = 0;
    uint64_t tileTotalWrRequest = 0;
    uint64_t tile_total_load_lat = 0;
    uint64_t tile_total_store_byte = 0;
    uint64_t tile_total_store_lat = 0;
    uint64_t tile_load_miss_cnt = 0;
    uint64_t tile_load_hit_stq = 0;
    uint64_t tileLoadHitTo = 0;
    uint64_t tileLoadHitCache = 0;
    uint64_t tile_lsu_ldq_full = 0;
    uint64_t tile_lsu_stq_full = 0;
    uint64_t tile_lsu_vab_full = 0;
    uint64_t tile_lsu_scb_full = 0;
    uint64_t tile_lsu_avg_lhq_occ = 0;
    uint64_t tile_lsu_ldq_deallocate = 0;
    uint64_t tile_lsu_avg_lhq_lifetime = 0;
    uint64_t tile_lsu_avg_stq_occ = 0;
    uint64_t tile_lsu_stq_deallocate = 0;
    uint64_t tile_lsu_avg_stq_lifetime = 0;
    uint64_t tile_lsu_scb_allocate_cnt = 0;
    uint64_t tile_lsu_scb_merge_cnt = 0;
    uint64_t tile_lsu_avg_scb_occ = 0;
    uint64_t tile_lsu_scb_deallocate = 0;
    uint64_t tile_lsu_avg_scb_lifetime = 0;
    uint64_t tile_load_to_use[5];
    uint64_t tile_lsu_stq_th_full[4];
    uint64_t gBufferBussyCyc = 0;
    uint64_t gBufferBussyGroupNum = 0;
    uint64_t grobBusyCyc = 0;
    uint64_t toCacheDrainCycle = 0;

    uint64_t vrfTagUtilization = 0;
    uint64_t vrfMapQOccupiedSize = 0;
    uint64_t vrfTagBussy = 1;
    uint64_t vrfMapQBussy = 1;

    uint64_t peBussyCyc = 0;
    uint64_t peDecStallCyc = 0;
    uint64_t peRobStallCyc = 0;
    uint64_t peMemoryBoundCyc = 0;
    uint64_t peDecIns = 0;
    uint64_t peIqStallCyc = 0;
    uint64_t peRetiredIns = 0;

    void SetPeBussy(uint64_t cycle);
    uint64_t GetPeBussy() const;
    void SetPeRegTagStall(uint64_t cycle);
    uint64_t GetPeRegTagStall() const;
    void SetPeRobStall(uint64_t cycle);
    uint64_t GetPeRobStall() const;
    void SetPeMemoryBound(uint64_t cycle);
    uint64_t GetPeMemoryBound() const;
    void SetDecIns(uint64_t insts);
    void ReduceDecIns(uint64_t insts);
    uint64_t GetDecIns() const;
    void SetPeIqStall(uint64_t cycle);
    uint64_t GetPeIqStall() const;
    void SetPeRetireIns(uint64_t insts);
    uint64_t GetPeRetireIns() const;
    using BaseStats::Reset;
    using BaseStats::report;
    void Reset();
    void Report_load_latency();
    void Report(const std::string &name, uint32_t coreId);
    void SamplingStash();
    void ReportSampling(const std::string &name);
};


} // namespace JCore

#endif