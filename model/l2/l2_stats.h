#pragma once

#include <vector>

#include "DataStruct/base_stats.h"
#include "utils/util.h"

#define DEPTH_TYPES 4

namespace JCore {

class L2Stats : public NS_PLAT::BaseStats {
public:
    uint64_t l1_l2_input_latency = 0;
    uint64_t l1_l2_input_reqs = 0;
    uint64_t l2_pick_latency = 0;
    uint64_t l2_pick_reqs = 0;
    uint64_t l2_mem_latency = 0;
    uint64_t l2_mem_reqs = 0;
    uint64_t l2_mem_read_reqs = 0;
    uint64_t l2_mem_write_reqs = 0;
    uint64_t resp_l1_reqs = 0;
    uint64_t resp_l1_latency = 0;
    uint64_t resp_l1_l2_input_latency = 0;
    uint64_t resp_l2_queue_full_latency = 0;
    uint64_t resp_l2_input_port_latency = 0;
    uint64_t resp_l2_pick_latency = 0;
    uint64_t resp_l2_cache_port_latency = 0;
    uint64_t resp_l2_wait_missq_latency = 0;
    uint64_t resp_l2_wait_snp_latency = 0;
    uint64_t resp_l2_miss_latency = 0;
    uint64_t resp_l2_latency = 0;

    uint64_t inst_req_count = 0;
    uint64_t data_req_count = 0;
    uint64_t dmd_load_req_count = 0;
    uint64_t pfl1_req_count = 0;
    uint64_t dmd_load_miss_count = 0;
    uint64_t store_req_count = 0;
    uint64_t load_req_count = 0;
    uint64_t hpref_req_count = 0;
    uint64_t writeback_req_count = 0;
    uint64_t read_req_count = 0;
    uint64_t write_req_count = 0;
    uint64_t upgrade_req_count = 0;
    uint64_t read_resp_cycles = 0;
    uint64_t write_resp_cycles = 0;
    uint64_t upgrade_resp_cycles = 0;
    uint64_t missq_full_cycles = 0;
    uint64_t inst_miss_count = 0;
    uint64_t data_dmd_miss_count = 0;
    uint64_t data_pref_miss_count = 0;
    uint64_t total_l2_replace_count = 0;
    uint64_t total_dprefl2_to_mem_count = 0;
    uint64_t total_dprefl2_useful_count = 0;
    uint64_t total_dprefl2_late_count = 0;
    uint64_t total_dmd_miss_count = 0;
    uint64_t total_dmd_miss_due_pref_count = 0;
    uint64_t interval_l2_replace_count = 0;
    uint64_t interval_dprefl2_to_mem_count = 0;
    uint64_t interval_dprefl2_useful_count = 0;
    uint64_t interval_dprefl2_late_count = 0;
    uint64_t intreval_data_req_count = 0;
    uint64_t interval_dmd_miss_count = 0;
    uint64_t interval_dmd_miss_due_pref_count = 0;

    uint64_t pref_stride_bad_count = 0;
    uint64_t pref_stream_bad_count = 0;
    uint64_t pref_sw_pref_bad_count = 0;
    uint64_t block_seq_count = 0;
    uint64_t pref_miss_count = 0;
    uint64_t pref_hit_count = 0;
    uint64_t pfl2_late_count = 0;
    uint64_t depthStats = 0;
    uint64_t cycles = 0;
    std::vector<uint64_t> depthStatsVec;
    uint64_t soc_latency = 0;
    uint64_t soc_RD_size = 0;
    uint64_t soc_WR_size = 0;
    uint64_t cache_port_stall = 0;
    uint64_t wait_stall = 0;

    using BaseStats::BaseStats;
    using BaseStats::Reset;
    using BaseStats::report;
    void Reset();
    void report(const std::string& name);
    void ReportSoc();
};

} // namespace JCore