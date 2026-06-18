#ifndef BLOCKISA_MODEL_VECLITE_ROB_H
#define BLOCKISA_MODEL_VECLITE_ROB_H
#include <vector>
#include <map>

#include "core/Bus.h"
#include "veclite/VectorLiteScheduleIQ.h"

namespace JCore {
class VectorLite;

enum class VROBStatus {
    ALLOCATED,
    DISPATCHED,
    COMPLETED,
    RETIRED
};

struct VROBInfo {
    BlockCommandPtr cmd;
    VROBStatus status;
    uint32_t uopCount = 0;
    uint64_t dispatchCycle = 0;
    uint64_t dispatchLastCycle = 0;
    std::vector<VectorLiteUopT> resolvedUops;

    VROBInfo() {}
    explicit VROBInfo(const BlockCommandPtr &c) : cmd(c), status(VROBStatus::ALLOCATED) {}

    void HandleResolve(const VectorLiteUopT& uop)
    {
        resolvedUops.push_back(uop);
    }

    bool CheckComplete() const
    {
        return uopCount == resolvedUops.size();
    }
};

class VectorLiteROB {
public:
    VectorLiteROB();
    ~VectorLiteROB();
    VectorLite*                                  top = nullptr;
    void                                         Xfer() const;
    void                                         Work();
    void                                         Build();
    bool                                         CheckStall() const;
    SimSys*                                      GetSim();
    void                                         SetSim(SimSys *s);

    void                                         SetFlush(const FlushBus &bus);
    bool                                         IsIdle();
    void                                         Stats() const;
    void                                         Dump();

    INPUT SimQueue<BlockCommandPtr>              *m_fetchRobInQ;
    INPUT SimQueue<VectorLiteUopT>               *m_exeRobRslvQ;

    INPUT SimQueue<SIQInfoPtr>                   *m_siqRobDispQ;
    INPUT SimQueue<SIQInfoPtr>                   *m_siqRobDispLastQ;
    OUTPUT SimQueue<ScbDrainBus>                 *drainScbDataQ;
    INPUT SimQueue<ScbDrainBus>                  *drainScbRespQ;

    OUTPUT SimQueue<BlockCommandPtr>             *vecliteStashCompQ;
    OUTPUT SimQueue<BlockCommandPtr>             *vecliteBccWakeupQ;
private:
    SimSys*                                      sim;
    std::map<ROBID, VROBInfo>                    entries;
    uint32_t                                     maxDepth = 0;
    uint32_t                                     swimLaneId = 0;
    std::map<ROBID, uint32_t>                    swimCount;
    void                                         ReceivedCmd();
    bool                                         AllocEntry(const BlockCommandPtr &cmd);
    void                                         CheckDisp();
    void                                         UpdateDispStats(const SIQInfoPtr &sInfo);
    void                                         UpdateDispLastStats(const SIQInfoPtr &sInfo);
    void                                         CheckResolved();
    void                                         DoResolved(const VectorLiteUopT& uop);
    void                                         CheckCommit();
    bool                                         SendDrainData(const BlockCommandPtr &cmd) const;
    void                                         CheckRetired();
};
} // namespace JCore

#endif
