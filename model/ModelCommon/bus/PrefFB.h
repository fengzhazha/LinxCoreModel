#pragma once

#ifndef PREF_FB_H
#define PREF_FB_H

#include <cstdint>

namespace JCore {
struct PrefFB {
    bool                    vld = false;
    bool                    bad = false;
    bool                    late = false;
    bool                    isFromL2 = false;
    uint32_t                type;
    uint64_t                addr;
    uint64_t                total_pref_sent_to_next = 0;
    uint64_t                total_pref_useful = 0;
    uint64_t                total_pref_late = 0;
    uint64_t                total_data_req;
    uint64_t                total_dmd_miss = 0;
    uint64_t                total_dmd_miss_due_pref = 0;
    uint64_t                interval_pref_sent_to_next = 0;
    uint64_t                interval_pref_useful = 0;
    uint64_t                interval_pref_late = 0;
    uint64_t                interval_data_req;
    uint64_t                interval_dmd_miss = 0;
    uint64_t                interval_dmd_miss_due_pref = 0;
};
typedef std::shared_ptr<PrefFB> PtrPrefFB;
}
#endif