#include "lsu/lsu_stats.h"

namespace JCore {

void LSUStats::report(const std::string& name) {
    uint64_t pref_count = pref_stream_count + pref_stride_count;
    uint64_t pref_bad_count = pref_bad_stream_count + pref_bad_stride_count;
    rpt->ReportTitle(name + " Statistics");
    rpt->ReportValAndPct("L2 Stall L1D Cycles And Percent", l2_stall_l1d_cycles, cycles);
    rpt->ReportVal("Load Requests", total_load_request);
    rpt->ReportVal("Store Requests", total_store_request);
    rpt->ReportVal("Aaccelss Addr numbers", addrCount);
    rpt->ReportValAndPct("Load miss count", load_miss_count, total_load_request);
    rpt->ReportValAndPct("Ldq miss count", ldq_miss_count + load_miss_count, total_load_request);
    rpt->ReportVal("Packets Sent To L2", dcache_l2_pkt_count);
    rpt->ReportValAndPct("  |-- Read Packets Sent To L2", dcache_l2_readpkt_count, dcache_l2_pkt_count);
    rpt->ReportValAndPct("    |-- Demand Read Packets", dcache_l2_demand_readpkt_count, dcache_l2_pkt_count);
    rpt->ReportValAndPct("    |-- Prefetch Read Packets", dcache_l2_pref_readpkt_count, dcache_l2_pkt_count);
    rpt->ReportValAndPct("  |-- Repeadted L1 Read Packets Filtered By MissQ", dcache_l2_missq_filt_count, dcache_l2_pkt_count);
    rpt->ReportValAndPct("    |-- Demand Packets Filtered By MissQ", dcache_l2_demand_missq_filt_count, dcache_l2_pkt_count);
    rpt->ReportValAndPct("    |-- Prefetch Packets Filtered By MissQ", dcache_l2_pref_missq_filt_count, dcache_l2_pkt_count);
    rpt->ReportValAndPct("  |-- Write Packets Sent To L2", dcache_l2_writepkt_count, dcache_l2_pkt_count);
    rpt->ReportValAndPct("  |-- Response Packets Sent To L2", dcache_l2_resppkt_count, dcache_l2_pkt_count);
    float avg_ld_lat = (total_load_request? float(total_load_latency) / total_load_request : -2) + 2;
    rpt->ReportVal("Average Load Latency", avg_ld_lat);
    float avg_input_lat = (ld_input_reqs ? float(ld_input_latency) / ld_input_reqs : 0);
    rpt->ReportVal("  |-- Average Input Latency", avg_input_lat);
    float avg_pick_lat = (ldq_pick_reqs ? float(ldq_pick_latency) / ldq_pick_reqs : 0);
    rpt->ReportVal("  |-- Average Pick Latency", avg_pick_lat);
    rpt->ReportVal("L2 Invalid L1DCache Useful Cacheline", dcache_l2_inv_useful);
    rpt->ReportVal("L1D Evict Useful Cacheline", dcache_l1_evict_useful);
    rpt->ReportValAndPct("Load Cancel Counts", ld_cancel, total_load_request);
    rpt->ReportValAndPct("Cancel For waiting LDQ picking", ldq_wait_cancel, ld_cancel);
    rpt->ReportValAndPct("Cancel For waiting store", ld_wait_store_cancel, ld_cancel);
    rpt->ReportValAndPct("Cancel For not hitting", ld_not_hit_cancel, ld_cancel);
    rpt->ReportValAndPct("Cancel For crossing cacheline", ldq_crossline_cancel, ld_cancel);
    float avg_ldl2_lat = (ld_l2_reqs ? float(ld_l2_latency) / ld_l2_reqs : 0);
    float avg_ldl2miss_lat = (ld_l2miss_reqs ? float(ld_l2miss_latency) / ld_l2miss_reqs : 0);
    float avg_ldl2hit_lat = (ld_l2hit_reqs ? float(ld_l2hit_latency) / ld_l2hit_reqs : 0);
    rpt->ReportVal("Average L2 Latency", avg_ldl2_lat);
    rpt->ReportVal("  |-- Average L2 Hit Latency", avg_ldl2hit_lat);
    rpt->ReportVal("  |-- Average L2 Miss Latency", avg_ldl2miss_lat);
    rpt->ReportVal("Load-Store conflicts", store_load_conflicts);
    rpt->ReportVal("MDB release count", mdb_release_count);
    uint64_t mdb_stall_count = mdb_suaccelss_count + mdb_fail_count;
    rpt->ReportValAndPct("MDB stall suaccelss", mdb_suaccelss_count, mdb_stall_count);
    rpt->ReportValAndPct("MDB stall fail", mdb_fail_count, mdb_stall_count);
    rpt->ReportVal("MDB fail wait pending", mdb_fail_wait_cycle);
    rpt->ReportAvg("MDB average fail wait pending", mdb_fail_wait_cycle, mdb_fail_count);
    rpt->ReportVal("Load Queue Full Cycles", ldq_full_cycles);
    rpt->ReportVal("Store Queue Full Cycles", stq_full_cycles);
    rpt->ReportVal("SCB Full Cycles", scb_full_cycles);
    rpt->ReportVal("SCB Lookup Count", scb_lookup_count);
    rpt->ReportVal("SCB Miss Count", scb_miss_count);
    rpt->ReportVal("Prefetch Count", pref_count);
    rpt->ReportVal("  |-- Stream Count", pref_stream_count);
    rpt->ReportVal("  |-- Stride Count", pref_stride_count);
    rpt->ReportVal("  |-- Sofware Prefetch Count", pref_sw_inst);
    rpt->ReportVal("Prefetch Bad Count", pref_bad_count);
    rpt->ReportVal("  |-- Stream Bad Count", pref_bad_stream_count);
    rpt->ReportVal("  |-- Stride Bad Count", pref_bad_stride_count);
    rpt->ReportVal("  |-- SW Pref Inst Bad Count", pref_bad_sw_inst);
    rpt->ReportVal("Prefetch Late Count", pref_late_count);
    float ldq_avg_occ = (float)ldq_total_occupied / cycles;
    rpt->ReportVal("ldq average occupied entries", ldq_avg_occ);
    rpt->ReportValAndPct("  |-- occ <= 10%",  ldq_occupied_10, ldq_occupied_count);
    rpt->ReportValAndPct("  |-- occ <= 25%",  ldq_occupied_25, ldq_occupied_count);
    rpt->ReportValAndPct("  |-- occ <= 50%",  ldq_occupied_50, ldq_occupied_count);
    rpt->ReportValAndPct("  |-- occ <= 75%",  ldq_occupied_75, ldq_occupied_count);
    rpt->ReportValAndPct("  |-- occ <= 90%",  ldq_occupied_90, ldq_occupied_count);
    rpt->ReportValAndPct("  |-- occ >  90%", ldq_occupied_100, ldq_occupied_count);
    float stq_avg_occ = (float)stq_total_occupied / cycles;
    rpt->ReportVal("stq average occupied entries", stq_avg_occ);
    rpt->ReportValAndPct("  |-- occ <= 10%",  stq_occupied_10, stq_occupied_count);
    rpt->ReportValAndPct("  |-- occ <= 25%",  stq_occupied_25, stq_occupied_count);
    rpt->ReportValAndPct("  |-- occ <= 50%",  stq_occupied_50, stq_occupied_count);
    rpt->ReportValAndPct("  |-- occ <= 75%",  stq_occupied_75, stq_occupied_count);
    rpt->ReportValAndPct("  |-- occ <= 90%",  stq_occupied_90, stq_occupied_count);
    rpt->ReportValAndPct("  |-- occ >  90%", stq_occupied_100, stq_occupied_count);
}

void LSUStats::Reset()
{
    cycles = 0;
    l2_stall_l1d_cycles = 0;
    // Load
    total_load_request = 0;
    total_load_latency = 0;
    ldq_full_cycles = 0;
    ldq_miss_count = 0;
    load_miss_count = 0;
    load_wait_pick_cycles = 0;
    load_wait_store_cycles = 0;
    load_queue_depth = 0;
    ldq_total_occupied = 0;
    ldq_occupied_10 = 0;
    ldq_occupied_25 = 0;
    ldq_occupied_50 = 0;
    ldq_occupied_75 = 0;
    ldq_occupied_90 = 0;
    ldq_occupied_100 = 0;
    ldq_occupied_count = 0;
    stq_total_occupied = 0;
    stq_occupied_10 = 0;
    stq_occupied_25 = 0;
    stq_occupied_50 = 0;
    stq_occupied_75 = 0;
    stq_occupied_90 = 0;
    stq_occupied_100 = 0;
    stq_occupied_count = 0;
    ld_input_latency = 0;
    ld_input_reqs = 0;
    ldq_pick_latency = 0;
    ldq_pick_reqs = 0;
    ld_l2_latency = 0;
    ld_l2_reqs = 0;
    ld_l2miss_latency = 0;
    ld_l2miss_reqs = 0;
    ld_l2hit_latency = 0;
    ld_l2hit_reqs = 0;
    ld_cancel = 0;
    ldq_crossline_cancel = 0;
    ldq_wait_cancel = 0;
    ld_wait_store_cancel = 0;
    ld_not_hit_cancel = 0;
    // MDB
    mdb_release_count = 0;
    mdb_suaccelss_count = 0;
    mdb_fail_count = 0;
    mdb_fail_wait_cycle = 0;
    // Store
    total_store_request = 0;
    stq_full_cycles = 0;
    store_load_conflicts = 0;
    store_queue_depth = 0;
    // SCB
    scb_full_cycles = 0;
    scb_lookup_count = 0;
    scb_miss_count = 0;
    // DCache
    dcache_refill_count = 0;
    dcache_upgrade_count = 0;
    dcache_inv_count = 0;
    dcache_l2_inv_useful = 0;
    dcache_l1_evict_useful = 0;
    dcache_gets_count = 0;
    dcache_l2_pkt_count = 0;
    dcache_l2_readpkt_count = 0;
    dcache_l2_demand_readpkt_count = 0;
    dcache_l2_pref_readpkt_count = 0;
    dcache_l2_missq_filt_count = 0;
    dcache_l2_demand_missq_filt_count = 0;
    dcache_l2_pref_missq_filt_count = 0;
    dcache_l2_resppkt_count = 0;
    dcache_l2_writepkt_count = 0;
    // Prefetch
    pref_stream_count = 0;
    pref_stride_count = 0;
    pref_sw_inst = 0;
    pref_bad_stream_count = 0;
    pref_bad_stride_count = 0;
    pref_bad_sw_inst = 0;
    pref_late_count = 0;

    fdp_stream_inc = 0;
    fdp_stream_dec = 0;

    addrCount = 0;
}

} // namespace JCore