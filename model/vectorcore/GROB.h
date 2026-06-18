#ifndef BLOCKISA_MODEL_VECTORCORE_GROB_H
#define BLOCKISA_MODEL_VECTORCORE_GROB_H
#include <vector>
#include <map>

#include "vectorcore/VectorCommon.h"
#include "interface/Credit.h"


namespace JCore {

class VectorCore;
class MemoryCore;
struct MaskReleaseInfo;

struct GroupInfo {
    uint64_t createCycle = 0;
    uint64_t decodeCycle = 0;
    uint64_t p1Cycle = 0;
    uint64_t exeCycle = 0;
    uint64_t completeCycle = 0;
    uint64_t retireCycle = 0;
    uint32_t tid = 0;
    uint32_t stid = -1U;
    uint32_t logicalGID = 0;
    ROBID bid;
    ROBID gid;
    BlockStatus status = BlockStatus::FREE;
    std::string DumpSwim() const;
    std::string Dump() const;
};

struct GROBState {
    bool vld = false;
    bool retired = false;
    uint32_t tag = 0;
    ROBID bid;
    ROBID gid;
    uint32_t stid = -1U;
    uint64_t bpc = 0;
    uint64_t tpc = 0;
    uint32_t cnt = 0;
    uint32_t cmt = 0;
    // For debug
    uint64_t dest = 0;
    uint64_t tileId = 0;
    bool destVld = false;
    uint64_t createCycle = 0;
    uint64_t startCycle = 0;
    uint64_t insertGroupBufferCycle = 0;
    // TODO: To optimization for executing.
    std::map<ROBID, GroupInfo> groupInfo;
    BlockCommandPtr blockCmd = nullptr;
    bool CheckOver()
    {
        return cmt == cnt;
    }
};

class GROB {
public:
    GROB();
    ~GROB();
    VectorCore*                         top = nullptr;
    MemoryCore*                         mtop = nullptr;
    uint64_t                            machineId = 0;
    INPUT SimQueue<GroupAllocInfo>*     m_lm2GROB;
    INPUT SimQueue<GroupIssueInfo>*     m_GBuffer2GROB;
    OUTPUT SimQueue<MaskReleaseInfo>*   m_GROB2MaskFile;
    OUTPUT SimQueue<Credit>*            m_vecBCCCreditQ;
    OUTPUT std::vector<SimQueue<ScbDrainBus>*> m_drainBusQ;
    INPUT std::vector<SimQueue<ScbDrainBus>*> m_drainRespBusQ;
    OUTPUT SimQueue<BlockCommandPtr>*   stashCompQ;

    void                                Work();
    void                                Build(uint32_t depth, uint32_t fetchWidth);

    void                                Alloc();
    void                                Alloc(GroupAllocInfo &allocInfo);
    void                                PrintGroupPipeView(const GroupInfo& info, uint32_t stid);
    void                                PrintGroupSwimLane(const GroupInfo& info);
    void                                CommitGroup(GROBState &robEntry, uint32_t &maxCmt);
    void                                Commit();
    void                                Retire();
    void                                Resolve(GROBState &robEntry);
    void                                ReportCommitGroup(const ROBID &bid, const ROBID &gid, uint32_t stid);
    bool                                CheckStall(uint32_t reserved, uint32_t stid);
    // bool                                CheckRslv(ROBID &bid);
    bool                                Find(const ROBID &bid, const ROBID &gid, uint32_t stid);

    SimSys*                             GetSim();
    void                                SetSim(SimSys *sim);

    void                                SetFlush(FlushBus &bus);
    void                                Dispatch();
    void                                CreateGroup(GroupIssueInfo &gInfo);
    void                                ReportAllocMinst(ROBID &bid, ROBID &gid, uint32_t stid);
    ROBID                               GetOldestGid(uint32_t stid);
    ROBID                               GetOldestBid(uint32_t stid);
    bool                                CheckAlloc(const ROBID &bid, uint32_t stid);
    bool                                IsIdle(uint32_t stid);
    void                                Stats();
    void                                ChecPEBussy(uint32_t stid);
    void                                SetGroupStartCycle(ROBID bid, uint64_t cycle, uint32_t stid);
private:
    SimSys*                             sim;
    const uint64_t                      groupThreadId = 10;
    std::vector<std::map<ROBID, GROBState>>          m_entries;
    uint32_t                            depth = 0;
    std::vector<uint32_t>               size;
    uint32_t                            fetchWidth = 0;
};


} // namespace JCore

#endif  // BLOCKISA_MODEL_VECTORCORE_GROB_H
