#pragma once

#include "../configs/iex_config.h"
#include "iex/iex_stat.h"
#include "iex/iex_iq.h"
#include "iex/pipe/iex_pipe.h"
#include "ModelCommon/SimInstInfo.h"
#include "ModelCommon/bus/MemRequest.h"

namespace JCore {
class IssueQueue;
class VAB {
public:
    VAB() {};
    ~VAB() {};
    std::vector<MemRequest*>    entry;
    SimInst                     originInst;
    std::vector<bool>           entry_valid;
    uint32_t                    reqSize = 0;
    uint32_t                    curSize = 0;
    bool                        Stall = false;
    bool                        Pending = false;
    IssueQueue                  *iex_iq_top;
    SimSys                      *sim;

    SimSys                      *GetSim() { return sim; }
    void                        split(uint64_t addr);
    void                        pushVab(SimInst inst);
    SimInst                       popVab();
    void                        Build(uint32_t size);
    void                        Reset();
    MemRequest*                 selectOne();
    bool                        isGather(MemRequest bus);
    bool                        resp2vab(MemRequest req);
    void                        dumpVab();
    void                        Resolve(MemRequest req);
};

} // namespace JCore
