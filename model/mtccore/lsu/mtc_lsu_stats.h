#pragma once

#include "DataStruct/base_stats.h"

namespace JCore {

class MtcLSUStats : public NS_PLAT::BaseStats {
public:
    uint64_t cycles = 0;
    uint64_t l2_stall_l1d_cycles = 0;
    // Load
    uint64_t total_load_request = 0;
    uint64_t total_load_latency = 0;
    uint64_t ldq_full_cycles = 0;
    uint64_t ldqStallCycles = 0;
    uint64_t tileScbStallCycles = 0;
    uint64_t sendMemReqStallCycles = 0;
    uint64_t ldq_miss_count = 0;
    uint64_t load_miss_count = 0;
    uint64_t load_wait_pick_cycles = 0;
    uint64_t load_wait_store_cycles = 0;
    uint64_t load_queue_depth = 0;
    uint64_t ldq_total_occupied = 0;
    uint64_t ldq_occupied_10 = 0;
    uint64_t ldq_occupied_25 = 0;
    uint64_t ldq_occupied_50 = 0;
    uint64_t ldq_occupied_75 = 0;
    uint64_t ldq_occupied_90 = 0;
    uint64_t ldq_occupied_100 = 0;
    uint64_t ldq_occupied_count = 0;
    uint64_t stq_total_occupied = 0;
    uint64_t stq_occupied_10 = 0;
    uint64_t stq_occupied_25 = 0;
    uint64_t stq_occupied_50 = 0;
    uint64_t stq_occupied_75 = 0;
    uint64_t stq_occupied_90 = 0;
    uint64_t stq_occupied_100 = 0;
    uint64_t stq_occupied_count = 0;
    uint64_t ld_input_latency = 0;
    uint64_t ld_input_reqs = 0;
    uint64_t ldq_pick_latency = 0;
    uint64_t ldq_pick_reqs = 0;
    uint64_t ld_l2_latency = 0;
    uint64_t ld_l2_reqs = 0;
    uint64_t ld_l2miss_latency = 0;
    uint64_t ld_l2miss_reqs = 0;
    uint64_t ld_l2hit_latency = 0;
    uint64_t ld_l2hit_reqs = 0;
    uint64_t ld_cancel = 0;
    uint64_t ldq_crossline_cancel = 0;
    uint64_t ldq_wait_cancel = 0;
    uint64_t ld_wait_store_cancel = 0;
    uint64_t ld_not_hit_cancel = 0;
    // MDB
    uint64_t mdb_release_count = 0;
    uint64_t mdb_suaccelss_count = 0;
    uint64_t mdb_fail_count = 0;
    uint64_t mdb_fail_wait_cycle = 0;
    // Store
    uint64_t total_store_request = 0;
    uint64_t stq_full_cycles = 0;
    uint64_t store_load_conflicts = 0;
    uint64_t store_queue_depth = 0;
    // SCB
    uint64_t scb_full_cycles = 0;
    uint64_t scb_lookup_count = 0;
    uint64_t scb_miss_count = 0;
    // DCache
    uint64_t dcache_refill_count = 0;
    uint64_t dcache_upgrade_count = 0;
    uint64_t dcache_inv_count = 0;
    uint64_t dcache_l2_inv_useful = 0;
    uint64_t dcache_l1_evict_useful = 0;
    uint64_t dcache_gets_count = 0;
    uint64_t dcache_l2_pkt_count = 0;
    uint64_t dcache_l2_readpkt_count = 0;
    uint64_t dcache_l2_demand_readpkt_count = 0;
    uint64_t dcache_l2_pref_readpkt_count = 0;
    uint64_t dcache_l2_missq_filt_count = 0;
    uint64_t dcache_l2_demand_missq_filt_count = 0;
    uint64_t dcache_l2_pref_missq_filt_count = 0;
    uint64_t dcache_l2_resppkt_count = 0;
    uint64_t dcache_l2_writepkt_count = 0;
    // Prefetch
    uint64_t pref_stream_count = 0;
    uint64_t pref_stride_count = 0;
    uint64_t pref_sw_inst = 0;
    uint64_t pref_bad_stream_count = 0;
    uint64_t pref_bad_stride_count = 0;
    uint64_t pref_bad_sw_inst = 0;
    uint64_t pref_late_count = 0;

    uint64_t fdp_stream_inc = 0;
    uint64_t fdp_stream_dec = 0;

    uint64_t addrCount = 0;

    using BaseStats::BaseStats;
    using BaseStats::Reset;
    using BaseStats::report;
    void Reset();
    void report(const std::string& name);
};


} // namespace JCore