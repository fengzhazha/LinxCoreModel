#pragma once

#include "DataStruct/base_stats.h"

namespace JCore {

class StackStats : public NS_PLAT::BaseStats {
public:
    uint64_t preg_count;
    uint64_t cycles;
    uint64_t stall_count;
    uint64_t occupied_ptag_toal;
    uint64_t occupied_ptag_10;
    uint64_t occupied_ptag_25;
    uint64_t occupied_ptag_50;
    uint64_t occupied_ptag_75;
    uint64_t occupied_ptag_90;
    uint64_t occupied_ptag_100;
    uint64_t init_load_set;
    uint64_t stack_set;
    uint64_t stack_get;
    uint64_t renamed_block;
    uint64_t renamed_block_0;
    uint64_t renamed_block_1;
    uint64_t renamed_block_2;
    uint64_t renamed_block_3;
    uint64_t renamed_block_4;
    uint64_t renamed_block_5;
    uint64_t renamed_block_6;
    uint64_t renamed_block_7;
    uint64_t renamed_block_8;
    uint64_t renamed_block_more;
    using BaseStats::BaseStats;
    using BaseStats::Reset;
    using BaseStats::report;
    virtual ~StackStats() {};
    void Reset();
    void report();
};

} // namespace JCore
