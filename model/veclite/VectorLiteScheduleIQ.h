#ifndef BLOCKISA_MODEL_VECLITE_SCHEDULE_IQ_H
#define BLOCKISA_MODEL_VECLITE_SCHEDULE_IQ_H
#include <vector>
#include <map>
#include <queue>

#include "core/Bus.h"
#include "veclite/VectorLiteUop.h"

namespace JCore {

enum class SIQEntryState {
    IDLE = 0,
    DISPATCH,
    DONE,
    UNDEF = 0xff
};

struct SIQInfo {
    BlockCommandPtr cmd;
    uint32_t uopCount = 0;
    bool ready = false;
    std::vector<VectorLiteUopT> uops;  // Generated UOPs (each 512B)
    SIQEntryState state = SIQEntryState::IDLE;
    uint64_t siqCycle = 0;
    uint32_t readRegLatency = 0;

    SIQInfo() {}
    explicit SIQInfo(const BlockCommandPtr &c) : cmd(c) {}
    SIQInfo(const BlockCommandPtr &c, uint32_t cnt) : cmd(c), uopCount(cnt) {}

    void SetReady(bool rdy);

    bool CheckReady() const;

    std::string Dump() const;
};
using SIQInfoPtr = std::shared_ptr<SIQInfo>;

class VectorLite;

class VectorLiteScheduleIQ {
public:
    VectorLiteScheduleIQ();
    ~VectorLiteScheduleIQ();

    // Capacity: maximum number of BlockCommand entries
    VectorLite*                         top = nullptr;

    // Main interfaces
    void                                Xfer();
    void                                Work();
    void                                Build();
    bool                                CheckStall() const;
    SimSys*                             GetSim() const;
    void                                SetSim(SimSys *s);

    void                                SetFlush(const FlushBus &bus);
    bool                                IsIdle() const;
    void                                Stats() const;

    // Schedule IQ specific interfaces
    void                                ReceivedCmd();
    bool                                AllocEntry(const BlockCommandPtr &cmd);

    // UOP generation - all UOPs are 512B granularity
    uint32_t                            CalculateSliceCount(uint64_t totalSize) const;
    VectorLiteUopT                      GenerateUOP(const SIQInfoPtr &entry,
                                                    uint32_t sliceIdx,
                                                    uint64_t offset) const;

    INPUT SimQueue<BlockCommandPtr>     *m_fetchSiqInQ;
    INPUT SimQueue<ROBID>               *m_stashSiqWakeuQ;
    OUTPUT SimQueue<SIQInfoPtr>         *m_siqRobDispQ;
    OUTPUT SimQueue<SIQInfoPtr>         *m_siqRobDispLastQ;
    OUTPUT SimQueue<VectorLiteUopT>     *m_siqUiqInQ;

    std::function<bool(const VectorLiteUopT&)> m_uiqStallFn;

private:
    SimSys                              *sim = nullptr;
    std::deque<SIQInfoPtr>              entries;
    uint32_t                            m_siqDepth = 0;
    ROBID                               dispatchBlock = ROBID();
    bool                                dispatchBlockValid = false;

    void                                Wakeup();
    void                                WakeupEntry(ROBID &bid);
    void                                Issue();
    void                                IssueEntryStateIdle(const SIQInfoPtr &entry) const;
    void                                IssueEntryStateDispatch(const SIQInfoPtr &entry);
    void                                IssueEntryStateDone(const SIQInfoPtr &entry) const;
    void                                IssueEntry(const SIQInfoPtr &entry);
    uint32_t                            CalUOPCount(const BlockCommandPtr &cmd) const;

    uint32_t                            m_issueLimit = 0;

    // Statistics
    uint64_t                            totalTileOPsReceived = 0;
    uint64_t                            totalUOPsGenerated = 0;
    uint64_t                            totalSplits = 0;
};
} // namespace JCore

#endif
