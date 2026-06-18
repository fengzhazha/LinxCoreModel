#include "l2/l2_stats.h"

namespace JCore {

void L2Stats::report(const std::string& name) {
    uint64_t data_miss_count = data_pref_miss_count + data_dmd_miss_count;
    uint64_t pref_bad_count = pref_stride_bad_count + pref_stream_bad_count;
    uint64_t l2_pref_count = pref_hit_count + pref_miss_count;
    
    float avg_resp_lat = (resp_l1_reqs ? float(resp_l1_latency) / resp_l1_reqs : 0);
    float avg_input_lat = (resp_l1_reqs ? float(resp_l1_l2_input_latency) / resp_l1_reqs : 0);
    float avg_queue_full_lat = (resp_l1_reqs ? float(resp_l2_queue_full_latency) / resp_l1_reqs : 0);
    float avg_input_port_lat = (resp_l1_reqs ? float(resp_l2_input_port_latency) / resp_l1_reqs : 0);
    float avg_pick_lat = (resp_l1_reqs ? float(resp_l2_pick_latency) / resp_l1_reqs : 0);
    float avg_cahce_stall_lat = (resp_l1_reqs ? float(resp_l2_cache_port_latency) / resp_l1_reqs : 0);
    float avg_waitfilter_stall_lat = (resp_l1_reqs ? float(resp_l2_wait_missq_latency) / resp_l1_reqs : 0);
    float avg_l2_miss_lat = (resp_l1_reqs ? float(resp_l2_miss_latency) / resp_l1_reqs : 0);
    float avg_resp_to_l1_lat = (resp_l1_reqs ? float(resp_l2_latency) / resp_l1_reqs : 0);
    float avg_wait_snp_lat = (resp_l1_reqs ? float(resp_l2_wait_snp_latency) / resp_l1_reqs : 0);

    rpt->ReportTitle(name + " Statistics");
    rpt->ReportVal("L2 Response To L1 Requests", resp_l1_reqs);
    rpt->ReportVal("L2 Average Response L1 Latency", avg_resp_lat);
    rpt->ReportValAndPctFl("  |-- L2 Average Input Latency", avg_input_lat, avg_resp_lat);
    rpt->ReportValAndPctFl("     |-- L2 Average Queue Full Stall", avg_queue_full_lat, avg_resp_lat);
    rpt->ReportValAndPctFl("     |-- L2 Average Input Port Stall", avg_input_port_lat, avg_resp_lat);
    rpt->ReportValAndPctFl("  |-- L2 Average Pick Latency", avg_pick_lat, avg_resp_lat);
    rpt->ReportValAndPctFl("     |-- L2 Average Cache Port Stall", avg_cahce_stall_lat, avg_resp_lat);
    rpt->ReportValAndPctFl("     |-- L2 Average Wait MissFilter Stall", avg_waitfilter_stall_lat, avg_resp_lat);
    rpt->ReportValAndPctFl("  |-- L2 Average Miss Latency", avg_l2_miss_lat, avg_resp_lat);
    rpt->ReportValAndPctFl("  |-- L2 Average Snoop Stall", avg_wait_snp_lat, avg_resp_lat);
    rpt->ReportValAndPctFl("  |-- L2 Response Latency", avg_resp_to_l1_lat, avg_resp_lat);
    float avg_l2mem_lat = (l2_mem_reqs ? float(l2_mem_latency) / l2_mem_reqs : 0);
    float avg_soc_lat = (l2_mem_reqs ? float(soc_latency) / l2_mem_reqs : 0);
    rpt->ReportVal("L2 Request Memory", l2_mem_reqs);
    rpt->ReportVal("L2 Average Mem Latency", avg_l2mem_lat);
    rpt->ReportVal("Soc Latency", avg_soc_lat);
    rpt->ReportVal("l2_mem_Soc_RD_reqs", l2_mem_read_reqs);
    rpt->ReportVal("l2_mem_SOC_RD_Size", soc_RD_size);
    rpt->ReportVal("l2_mem_write_reqs", l2_mem_write_reqs);
    rpt->ReportVal("l2_mem_SOC_WR_Size", soc_WR_size);

    rpt->ReportVal("L1 Demand Load Requests", dmd_load_req_count);
    rpt->ReportValAndPct("L1 Demand Load Miss", dmd_load_miss_count, dmd_load_req_count);
    rpt->ReportVal("Data Requests", data_req_count);
    rpt->ReportVal("  |-- L1 Demand Load Count", dmd_load_req_count);
    rpt->ReportVal("  |-- L1 Demand Store Count", store_req_count);
    rpt->ReportVal("  |-- L1 Prefetch Count", pfl1_req_count);
    rpt->ReportValAndPct("Data Miss Count", data_miss_count, data_req_count);
    rpt->ReportValAndPct("  |-- L1 Demand Miss", data_dmd_miss_count, data_req_count);
    rpt->ReportValAndPct("  |-- L1 Pref Miss", data_pref_miss_count, data_req_count);
    rpt->ReportVal("Inst Requests", inst_req_count);
    rpt->ReportValAndPct("Inst Miss Count", inst_miss_count, inst_req_count);
    rpt->ReportVal("Block Header Prefetch Count", hpref_req_count);
    rpt->ReportVal("MissQueue Full Cycles", missq_full_cycles);
    total_dprefl2_to_mem_count += interval_dprefl2_to_mem_count;
    total_dprefl2_useful_count += interval_dprefl2_useful_count;
    total_dprefl2_late_count += interval_dprefl2_useful_count;
    total_dmd_miss_count += interval_dmd_miss_count;
    total_dmd_miss_due_pref_count += interval_dmd_miss_due_pref_count;
    rpt->ReportVal("L2 DPrefetches Sent to Mem", total_dprefl2_to_mem_count);
    rpt->ReportValAndPct("  |-- L2 DPrefetches useful count and accuracy", total_dprefl2_useful_count, total_dprefl2_to_mem_count);
    rpt->ReportValAndPct("    |-- L2 DPrefetches Late Count", total_dprefl2_late_count, total_dprefl2_useful_count);
    rpt->ReportVal("L2 Demand Miss", total_dmd_miss_count);
    rpt->ReportValAndPct("  |-- L2 Demand Miss Caused By Prefetch", total_dmd_miss_due_pref_count, total_dmd_miss_count);
    rpt->ReportVal("L2 Prefetch Count", l2_pref_count);
    rpt->ReportValAndPct("  |-- Hit Count", pref_hit_count, l2_pref_count);
    rpt->ReportValAndPct("  |-- Miss Count", pref_miss_count, l2_pref_count);
    rpt->ReportVal("L2 Prefetch Bad Count", pref_bad_count);
    rpt->ReportVal("  |-- Stride Bad Count", pref_stride_bad_count);
    rpt->ReportVal("  |-- Stream Bad Count", pref_stream_bad_count);
    rpt->ReportVal("  |-- Software Pref Bad Count", pref_sw_pref_bad_count);
    rpt->ReportVal("L2 Prefetch Late Count", pfl2_late_count);
    float depthStatsAvg = (float)depthStats / cycles;
    rpt->ReportVal("L2 average occupied entries", depthStatsAvg);
    uint32_t types = depthStatsVec.size();
    if (types > 0) {
        uint32_t step = 100 / types;
        for (uint32_t i = 0; i < types; ++i) {
            uint32_t occ = (i + 1) * step;
            std::string str = "  |-- occ <= " + std::to_string(occ) + "%";
            rpt->ReportValAndPct(str,  depthStatsVec[i], cycles);
        }
    }
    return;
}

void L2Stats::ReportSoc()
{
    rpt->ReportTitle("L2 Cache Statistics");
    float avg_l2mem_lat = (l2_mem_reqs ? float(l2_mem_latency) / l2_mem_reqs : 0);
    float avg_soc_lat = (l2_mem_reqs ? float(soc_latency) / l2_mem_reqs : 0);
    rpt->ReportVal("L2 Request Memory", l2_mem_reqs);
    rpt->ReportVal("L2 Average Mem Latency", avg_l2mem_lat);
    rpt->ReportVal("Soc Latency", avg_soc_lat);
    rpt->ReportVal("l2_mem_Soc_RD_reqs", l2_mem_read_reqs);
    rpt->ReportVal("l2_mem_SOC_RD_Size", soc_RD_size);
    rpt->ReportVal("l2_mem_write_reqs", l2_mem_write_reqs);
    rpt->ReportVal("l2_mem_SOC_WR_Size", soc_WR_size);
}

void L2Stats::Reset()
{
    l1_l2_input_latency = 0;
    l1_l2_input_reqs = 0;
    l2_pick_latency = 0;
    l2_pick_reqs = 0;
    l2_mem_latency = 0;
    l2_mem_reqs = 0;

    resp_l1_reqs = 0;
    resp_l1_latency = 0;
    resp_l1_l2_input_latency = 0;
    resp_l2_queue_full_latency = 0;
    resp_l2_input_port_latency = 0;
    resp_l2_pick_latency = 0;
    resp_l2_cache_port_latency = 0;
    resp_l2_wait_missq_latency = 0;
    resp_l2_wait_snp_latency = 0;
    resp_l2_miss_latency = 0;
    resp_l2_latency = 0;

    inst_req_count = 0;
    data_req_count = 0;
    dmd_load_req_count = 0;
    pfl1_req_count = 0;
    dmd_load_miss_count = 0;
    store_req_count = 0;
    load_req_count = 0;
    hpref_req_count = 0;
    writeback_req_count = 0;
    read_req_count = 0;
    write_req_count = 0;
    upgrade_req_count = 0;
    read_resp_cycles = 0;
    write_resp_cycles = 0;
    upgrade_resp_cycles = 0;
    missq_full_cycles = 0;
    inst_miss_count = 0;
    data_dmd_miss_count = 0;
    data_pref_miss_count = 0;
    total_l2_replace_count = 0;
    total_dprefl2_to_mem_count = 0;
    total_dprefl2_useful_count = 0;
    total_dprefl2_late_count = 0;
    total_dmd_miss_count = 0;
    total_dmd_miss_due_pref_count = 0;
    interval_l2_replace_count = 0;
    interval_dprefl2_to_mem_count = 0;
    interval_dprefl2_useful_count = 0;
    interval_dprefl2_late_count = 0;
    intreval_data_req_count = 0;
    interval_dmd_miss_count = 0;
    interval_dmd_miss_due_pref_count = 0;

    pref_stride_bad_count = 0;
    pref_stream_bad_count = 0;
    pref_sw_pref_bad_count = 0;
    block_seq_count = 0;
    pref_miss_count = 0;
    pref_hit_count = 0;
    pfl2_late_count = 0;
    depthStats = 0;
    cycles = 0;
    soc_latency = 0;
    cache_port_stall = 0;
    wait_stall = 0;

    ReleaseVecEle(depthStatsVec);
    depthStatsVec = std::vector<uint64_t>(DEPTH_TYPES);
}

} // namespace JCore