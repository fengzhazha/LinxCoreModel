#include "vectorcore/VectorCoreStats.h"

namespace JCore {

void VectorCoreStats::Reset()
{
    m_totalCycle = 0;
    m_busyCycle = 0;
    m_workingCycle = 0;
    m_executeBlockNum = 0;

    pendingCycle = 0;
    pendingNum = 0;
    stashNum = 0;
    loadHitCacheStash = 0;
    loadHitCacheDemand = 0;
    stashFlushCnt = 0;
    tileCacheEvictCnt = 0;
    tileLoadPrefetchDropCnt = 0;
    tileLoadPrefetchDrop1Cnt = 0;
    tileLoadHitPrefetchRoadCnt = 0;
    tile_load_hit_missq_cnt = 0;
    tileLoadTriggerPfCnt = 0;
    tileLoadTrainPfCnt = 0;
    tileHeadTriggerPfCnt = 0;
    tileHeadTrainPfCnt = 0;
    tileLoadBadPrefetchCnt = 0;
    m_taVisitTileRegister = 0;
    tile_total_load_inst = 0;
    tile_total_gather_load_inst = 0;
    tile_total_continious_load_inst = 0;
    tile_total_load_request = 0;
    tile_total_store_inst = 0;
    tile_total_scatter_store_inst = 0;
    tile_total_continious_store_inst = 0;
    tile_total_store_request = 0;
    tile_port0_load_request = 0;
    tile_port1_load_request = 0;

    tile_total_load_lat = 0;
    tile_total_store_byte = 0;
    tile_total_store_lat = 0;
    tile_load_miss_cnt = 0;
    tile_load_hit_stq = 0;
    tileLoadHitTo = 0;
    tileLoadHitCache = 0;
    tile_lsu_ldq_full = 0;
    tile_lsu_stq_full = 0;
    tile_lsu_vab_full = 0;
    tile_lsu_scb_full = 0;
    tile_lsu_avg_lhq_occ = 0;
    tile_lsu_ldq_deallocate = 0;
    tile_lsu_avg_lhq_lifetime = 0;
    tile_lsu_avg_stq_occ = 0;
    tile_lsu_stq_deallocate = 0;
    tile_lsu_avg_stq_lifetime = 0;
    tile_lsu_scb_allocate_cnt = 0;
    tile_lsu_scb_merge_cnt = 0;
    tile_lsu_avg_scb_occ = 0;
    tile_lsu_scb_deallocate = 0;
    tile_lsu_avg_scb_lifetime = 0;
    gBufferBussyCyc = 0;
    gBufferBussyGroupNum = 0;
    grobBusyCyc = 0;
    toCacheDrainCycle = 0;

    for (int i = 0; i < 4; i++) { // clear each thread
        tile_lsu_stq_th_full[i] = 0;
    }
    for (int i = 0; i < 5; i++) {
        tile_load_to_use[i] = 0;
    }
}

void VectorCoreStats::Report_load_latency()
{
}

void VectorCoreStats::Report(const std::string &name, uint32_t coreId)
{
    std::stringstream titleStr;
    titleStr << "Vector Core " << coreId << " Detail Statistics(" << name << ")";
    rpt->ReportTitle(titleStr.str());
    rpt->ReportVal("Vector tile load execute inst cnt", tile_total_load_inst);
    rpt->ReportVal("  |--Vec tile load gather inst cnt", tile_total_gather_load_inst);
    rpt->ReportVal("  |--Vec tile load continious inst cnt", tile_total_continious_load_inst);
    rpt->ReportVal("Vector tile load req cnt", tile_total_load_request);
    rpt->ReportVal("Vector tile store execute inst cnt", tile_total_store_inst);
    rpt->ReportVal("  |--Vec tile store scatter inst cnt", tile_total_scatter_store_inst);
    rpt->ReportVal("  |--Vec tile store continious inst cnt", tile_total_continious_store_inst);
    rpt->ReportVal("Vector tile store req cnt", tile_total_store_request);
    rpt->ReportValAndPct("Vector core load port0 req channel utilization", tile_port0_load_request, m_totalCycle);
    rpt->ReportValAndPct("Vector core load port1 req channel utilization", tile_port1_load_request, m_totalCycle);
    rpt->ReportVal("Tile Total read req cnt", tileTotalRdRequest);
    rpt->ReportVal("Tile Total Write req cnt", tileTotalWrRequest);
    if (tile_total_load_inst != 0) {
        rpt->ReportVal("Vector tile load average latency", tile_total_load_lat/tile_total_load_inst);
        rpt->ReportVal("  |--Vec load_lat_le_10_cycle", tile_load_to_use[0]);
        rpt->ReportVal("  |--Vec load_lat_le_20_cycle", tile_load_to_use[1]);
        rpt->ReportVal("  |--Vec load_lat_le_30_cycle", tile_load_to_use[2]);
        rpt->ReportVal("  |--Vec load_lat_le_50_cycle", tile_load_to_use[3]);
        rpt->ReportVal("  |--Vec load_lat_ge_50_cycle", tile_load_to_use[4]);
    } else {
        rpt->ReportVal("Vector tile load average latency", 0.0);
        rpt->ReportVal("  |--Vec load_lat_le_10_cycle", 0.0);
        rpt->ReportVal("  |--Vec load_lat_le_20_cycle", 0.0);
        rpt->ReportVal("  |--Vec load_lat_le_30_cycle", 0.0);
        rpt->ReportVal("  |--Vec load_lat_le_50_cycle", 0.0);
        rpt->ReportVal("  |--Vec load_lat_ge_50_cycle", 0.0);
    }

    if (tile_total_store_inst != 0) {
        rpt->ReportVal("Vector core store average latency", tile_total_store_lat/tile_total_store_inst);
    } else {
        rpt->ReportVal("Vector core store average latency", 0.0);
    }
    rpt->ReportValAndPct("Vec load miss cnt", tile_load_miss_cnt, tile_total_load_request);
    rpt->ReportValAndPct("  |--Vec load merge missq cnt", tile_load_hit_missq_cnt, tile_total_load_request);
    rpt->ReportValAndPct("  |--Vec load merge prefetchq cnt", tileLoadHitPrefetchRoadCnt, tile_total_load_request);
    rpt->ReportValAndPct("  |--Vec load miss req cnt", (tile_load_miss_cnt - tile_load_hit_missq_cnt -
        tileLoadHitPrefetchRoadCnt), tile_total_load_request);
    rpt->ReportValAndPct("Vec load hit cnt", (tileLoadHitCache + tile_load_hit_stq), tile_total_load_request);
    rpt->ReportValAndPct("  |--Vec load hit cache cnt", tileLoadHitCache, tile_total_load_request);
    rpt->ReportValAndPct("    |--Vec load hit stash cnt", loadHitCacheStash, tile_total_load_request);
    rpt->ReportValAndPct("    |--Vec load hit demand cnt", loadHitCacheDemand, tile_total_load_request);
    rpt->ReportValAndPct("  |--Vec load hit stq cnt", tile_load_hit_stq, tile_total_load_request);
    rpt->ReportVal("Vec prefetch hit demand missq drop cnt", tileLoadPrefetchDropCnt);
    rpt->ReportVal("Vec prefetch hit cache drop cnt", tileLoadPrefetchDrop1Cnt);
    rpt->ReportVal("Vec load train pf count", tileLoadTrainPfCnt);
    rpt->ReportVal("Vec head train pf count", tileHeadTrainPfCnt);
    rpt->ReportVal("Vec load trigger pf count", tileLoadTriggerPfCnt);
    rpt->ReportVal("Vec head trigger pf count", tileHeadTriggerPfCnt);
    rpt->ReportVal("Vector core stash count", stashNum);
    rpt->ReportValAndPct("  |--Vec load bad stash rate", tileLoadBadPrefetchCnt, tile_total_load_request);
    rpt->ReportValAndPct("  |--Vec load hit road rate", tileLoadHitPrefetchRoadCnt, tile_total_load_request);
    rpt->ReportValAndPct("  |--Vec load hit cache rate", loadHitCacheStash, tile_total_load_request);
    rpt->ReportValAndPct("Vec load stash coverage", (loadHitCacheStash + tileLoadHitPrefetchRoadCnt),
        tile_total_load_request);
    rpt->ReportValAndPct("Vector core stash avg cycle", pendingCycle, stashNum);
    rpt->ReportValAndPct("Vector core stash avg count", pendingNum, stashNum);
    rpt->ReportVal("Tile cache evict cnt", tileCacheEvictCnt);
    rpt->ReportVal("Prefetch table entry flush cnt(block resolve)", stashFlushCnt);
    rpt->ReportValAndPct("Vec load hit to cache cnt", tileLoadHitTo, tile_total_load_request);
    rpt->ReportAvg("Vec ldq avg occ", tile_lsu_avg_lhq_occ, m_totalCycle);
    rpt->ReportAvg("Vec ldq avg lifetime", tile_lsu_avg_lhq_lifetime, tile_lsu_ldq_deallocate);
    rpt->ReportVal("Vec ldq full cnt", tile_lsu_ldq_full);

    rpt->ReportAvg("Vec stq avg occ", tile_lsu_avg_stq_occ, m_totalCycle);
    rpt->ReportAvg("Vec stq avg lifetime", tile_lsu_avg_stq_lifetime, tile_lsu_stq_deallocate);
    rpt->ReportVal("Vec stq full cnt", tile_lsu_stq_full);
    rpt->ReportVal("Vec stq thread0 full cnt", tile_lsu_stq_th_full[0]);
    rpt->ReportVal("Vec stq thread1 full cnt", tile_lsu_stq_th_full[1]);
    rpt->ReportVal("Vec stq thread2 full cnt", tile_lsu_stq_th_full[2]);
    rpt->ReportVal("Vec stq thread3 full cnt", tile_lsu_stq_th_full[3]);

    rpt->ReportVal("Vec scb allocate cnt", tile_lsu_scb_allocate_cnt);
    rpt->ReportVal("Vec scb merge cnt", tile_lsu_scb_merge_cnt);
    rpt->ReportVal("Vec scb deallocate cnt", tile_lsu_scb_deallocate);
    rpt->ReportVal("Vec scb full cnt", tile_lsu_scb_full);
    rpt->ReportVal("To Cache data syncing cycle", toCacheDrainCycle);
    rpt->ReportAvg("Vec scb avg occ", tile_lsu_avg_scb_occ, m_totalCycle);
    rpt->ReportAvg("Vec scb avg lifetime", tile_lsu_avg_scb_lifetime, tile_lsu_scb_deallocate);
    rpt->ReportAvg("Vector core BPC", m_executeBlockNum, m_totalCycle);
    rpt->ReportValAndPct("Vector core utilization", m_workingCycle, m_totalCycle);
}

void VectorCoreStats::SamplingStash()
{
    samplingCycle = m_totalCycle;
    samplingWorkingCycle = m_workingCycle;
    samplingExeBlockNum = m_executeBlockNum;
    samplingTaVisitTR = m_taVisitTileRegister;
}

void VectorCoreStats::ReportSampling(const std::string &name)
{
    rpt->ReportTitle("VectorCore " + name + " Statistics ");
    rpt->ReportVal("Vector core execute block number", m_executeBlockNum - samplingExeBlockNum);
    rpt->ReportVal("Vector Core TA visit tile register", m_taVisitTileRegister - samplingTaVisitTR);
    rpt->ReportAvg("Vector core BPC", m_executeBlockNum - samplingExeBlockNum, m_totalCycle - samplingCycle);
    rpt->ReportValAndPct("Vector core utilization", m_workingCycle - samplingWorkingCycle,
                         m_totalCycle - samplingCycle);
}

void VectorCoreStats::SetPeBussy(uint64_t cycle)
{
    peBussyCyc += cycle;
}

uint64_t VectorCoreStats::GetPeBussy() const
{
    return peBussyCyc;
}

void VectorCoreStats::SetPeRegTagStall(uint64_t cycle)
{
    peDecStallCyc += cycle;
}

uint64_t VectorCoreStats::GetPeRegTagStall() const
{
    return peDecStallCyc;
}

void VectorCoreStats::SetPeRobStall(uint64_t cycle)
{
    peRobStallCyc += cycle;
}

uint64_t VectorCoreStats::GetPeRobStall() const
{
    return peRobStallCyc;
}

void VectorCoreStats::SetPeMemoryBound(uint64_t cycle)
{
    peMemoryBoundCyc += cycle;
}

uint64_t VectorCoreStats::GetPeMemoryBound() const
{
    return peMemoryBoundCyc;
}

void VectorCoreStats::SetDecIns(uint64_t insts)
{
    peDecIns += insts;
}

void VectorCoreStats::ReduceDecIns(uint64_t insts)
{
    peDecIns -= insts;
}

uint64_t VectorCoreStats::GetDecIns() const
{
    return peDecIns;
}

void VectorCoreStats::SetPeIqStall(uint64_t cycle)
{
    peIqStallCyc += cycle;
}

uint64_t VectorCoreStats::GetPeIqStall() const
{
    return peIqStallCyc;
}

void VectorCoreStats::SetPeRetireIns(uint64_t insts)
{
    peRetiredIns += insts;
}

uint64_t VectorCoreStats::GetPeRetireIns() const
{
    return peRetiredIns;
}

} // namespace JCore