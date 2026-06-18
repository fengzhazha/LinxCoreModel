#include "mtccore/memcore/MemoryCoreStats.h"

namespace JCore {

void MemoryCoreStats::Reset()
{
    m_totalCycle = 0;
    m_workingCycle = 0;
    m_executeBlockNum = 0;
    m_perThreadWorkingCycle.resize(m_threadNum);
    for (uint32_t tid = 0; tid < m_threadNum; ++tid) {
        m_perThreadWorkingCycle[tid] = 0;
    }

    m_taHit = 0;
    m_taMiss = 0;
    m_taVisitTileRegister = 0;
    tile_total_load_request = 0;
    tile_total_store_request = 0;
    tile_total_load_byte = 0;
    tile_total_load_lat = 0;
    tile_total_store_byte = 0;
    tile_total_store_lat = 0;
    for (int i = 0; i < 5; i++) {
        tile_load_to_use[i] = 0;
    }
}

void MemoryCoreStats::Report_load_latency()
{
}

void MemoryCoreStats::Report(const std::string &name)
{
    rpt->ReportTitle("MemoryCore " + name + " Statistics ");
    rpt->ReportValAndPct("Memory core load port0 req channel utilization", tile_total_load_request, m_totalCycle);
    rpt->ReportValAndPct("Memory core load port0 data channel utilization", tile_total_load_byte, m_totalCycle*256);
    rpt->ReportValAndPct("Memory core store port data channel utilization", tile_total_store_byte, m_totalCycle*256);
    if (tile_total_load_request != 0) {
        rpt->ReportVal("Mtc tile load average latency", tile_total_load_lat/tile_total_load_request);
        rpt->ReportVal("  |--Mtc load_lat_le_10_cycle", tile_load_to_use[0]);
        rpt->ReportVal("  |--Mtc load_lat_le_20_cycle", tile_load_to_use[1]);
        rpt->ReportVal("  |--Mtc load_lat_le_30_cycle", tile_load_to_use[2]);
        rpt->ReportVal("  |--Mtc load_lat_le_50_cycle", tile_load_to_use[3]);
        rpt->ReportVal("  |--Mtc load_lat_ge_50_cycle", tile_load_to_use[4]);
    } else {
        rpt->ReportVal("Mtc tile load average latency", tile_total_load_lat);
    }
    if (tile_total_store_request != 0) {
        rpt->ReportVal("Memory core store average latency", tile_total_store_lat/tile_total_store_request);
    } else {
        rpt->ReportVal("Memory core store average latency", tile_total_store_lat);
    }

    rpt->ReportVal("Memory core execute block number", m_executeBlockNum);
    rpt->ReportVal("Memory Core TA visit tile register", m_taVisitTileRegister);
    rpt->ReportVal("Memory core visit ta miss", m_taMiss);
    rpt->ReportVal("Memory core visit ta hit", m_taHit);
    rpt->ReportAvg("Memory core BPC", m_executeBlockNum, m_totalCycle);
    rpt->ReportValAndPct("Memory core utilization", m_workingCycle, m_totalCycle);
    for (uint32_t tid = 0; tid < m_perThreadWorkingCycle.size(); ++tid) {
        std::stringstream ss;
        ss << "Memory core thread" << tid << " utilization";
        rpt->ReportValAndPct(ss.str(), m_perThreadWorkingCycle[tid], m_totalCycle);
    }
}

void MemoryCoreStats::SamplingStash()
{
    samplingCycle = m_totalCycle;
    samplingWorkingCycle = m_workingCycle;
    samplingExeBlockNum = m_executeBlockNum;
    samplingTaMiss = m_taMiss;
    samplingTaHit = m_taHit;
    samplingTaHit = m_taHit;
    samplingTaVisitTR = m_taVisitTileRegister;
}

void MemoryCoreStats::ReportSampling(const std::string &name)
{
    rpt->ReportTitle("MemoryCore " + name + " Statistics ");
    rpt->ReportVal("Memory core execute block number", m_executeBlockNum - samplingExeBlockNum);
    rpt->ReportVal("Memory Core TA visit tile register", m_taVisitTileRegister - samplingTaVisitTR);
    rpt->ReportVal("Memory core visit ta miss", m_taMiss - samplingTaMiss);
    rpt->ReportVal("Memory core visit ta hit", m_taHit - samplingTaHit);
    rpt->ReportAvg("Memory core BPC", m_executeBlockNum - samplingExeBlockNum, m_totalCycle - samplingCycle);
    rpt->ReportValAndPct("Memory core utilization", m_workingCycle - samplingWorkingCycle,
                         m_totalCycle - samplingCycle);
}

} // namespace JCore