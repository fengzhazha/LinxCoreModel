#pragma once

#include <list>
#include <vector>

#include "core/Bus.h"

namespace JCore {

class MtcPrefetcher;

class MtcStride {
public:
    MtcPrefetcher  *prefetcher;
    SimSys     *sim;
    std::deque<uint64_t> *pfl1_q;
    std::deque<uint64_t> *pfl2_q;

    bool    train(MemReqBus &req);
    void    feedback(PtrPrefFB);
    void    Reset(void);
    void    Build(void);
    void    Work(void);
    void    Xfer(void);
private:
    const int N = 16;
    const int DEPTH_TI_THRE = 4;
    uint32_t pfl1_depth;
    uint32_t pfl2_depth;

    struct Entry {
        uint64_t pc;
        uint64_t addr;
        uint64_t stride;
        int      stage;
        bool     line_delta;
        uint64_t pref_time;
        int      depth; // 1,2,3
        bool     fix_depth;
        int      ti_list[3];
    };

    typedef std::shared_ptr<Entry> PtrEntry;

    uint64_t pfl1_count;
    uint64_t pfl1_bad_count;
    uint64_t pfl1_count_n;
    uint64_t pfl1_bad_count_n;
    bool     stride_disable;
    uint64_t interval_count;
    uint64_t interval_thr = 5000;

    std::list<PtrEntry> mru_list;
    std::deque<uint64_t> l1_filter;
    std::deque<uint64_t> l2_filter;
    bool filter_line(std::deque<uint64_t> &filter, uint64_t addr);
};

} // namespace JCore