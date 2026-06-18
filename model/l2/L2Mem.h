#pragma once

#include "../configs/l2_config.h"
#include "l2/l2_stats.h"
#include "l2/L2types.h"
#include "l2/MissQueue.h"
#include "ModelSpec.h"

#define L2_DC_IDX 4096
#define L2_DC_WAY 4

namespace JCore {

class L2Cache;
class L2Mem : public SimObj {
private:
    L2Entry **cache;

    uint64_t addr2tag(uint64_t a) { return a >> 6; }
    uint64_t addr2idx(uint64_t a) { return (a >> 6) & (config->cache_nset - 1); }
    uint64_t tag2addr(uint64_t t) { return t << 6; }
    L2Entry* find(uint64_t addr, bool lru);

    template<typename... ARGS>
    void info(ARGS... args) {
        printf("[L2] L2Mem. ");
        printf(args...);
    }

    void dumpIndex(int index);
public:
    bool verbose;
    std::shared_ptr<L2Config> config { nullptr };
    L2Cache *top;
    std::shared_ptr<L2Stats> stats;
    virtual ~L2Mem();
    void Work() override;
    void Reset() override;
    void Xfer() override;
    SimSys *GetSim() override;
    void ReportStat() override {}

    void Build(void);

    using MissQEntry = MissQueue::MissQEntry;

    bool writeback(MissQEntry *e);
    bool lookup(MissQEntry *e);
    void replace(MissQEntry *e, L2Entry *r_e);
    void freeSnoopFilter(uint64_t addr, uint32_t src);
};

} // namespace JCore
