#pragma once

#include <memory>
#include <vector>

#include "core/Bus.h"
#include "lsu/lsu_stats.h"
#include "ModelSpec.h"
#include "SimSys.h"

namespace JCore {

class LoadStoreUnit;
class Core;

/* Virtual class that buffers all loads and stores in a block */
struct MemSet {
    ROBID                               oldest;
    std::map<ROBID, MemReqBus>          memSet;
};

struct FakeLSU {
    Core                                *core;
    LoadStoreUnit                       *top;
    bool                                verbose;
    /* Read & write set for all blocks */
    std::vector<MemSet>                 fakeLSU;
    std::vector<MemSet>                 fakeST;
    std::shared_ptr<LSUStats>           stats;
    std::vector<SimQueue<MemReqBus>>    pe_ret_data_q;
    std::vector<SimQueue<MemReqBus>>    pe_resolve_q;
    std::vector<SimQueue<MemReqBus>>    pe_wakeup_q;
    void                                Work();
    void                                returnPE();
    bool                                mergeStore(MemReqBus &bus);
};

} // namespace JCore