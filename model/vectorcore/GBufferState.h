#ifndef BLOCKISA_MODEL_VECTORCORE_GROBSTATE_H
#define BLOCKISA_MODEL_VECTORCORE_GROBSTATE_H

#include "core/Bus.h"
#include "SimSys.h"

namespace JCore {

struct PMUInfo {
    uint64_t                createCycle = 0;
    uint64_t                f1Cycle = 0;
    uint64_t                fetchCycle = 0;
    uint64_t                f2Cycle = 0;
    uint64_t                fetchReturnCycle = 0;
    uint64_t                decodeCycle = 0;
    uint64_t                renameCycle = 0;
    uint64_t                renamedCycle = 0;
    uint64_t                issueCycle = 0;
    uint64_t                dispatchCycle = 0;
    uint64_t                iqCycle = 0;
    uint64_t                exeCycle = 0;
    uint64_t                completeCycle = 0;
    uint64_t                retireCycle = 0;
    uint64_t                coreId = 0;
};

struct GBufferEntry {
    bool                        vld = false;
    ROBID                       bid;
    ROBID                       gid;
    uint64_t                    tpc = 0;
    bool                        hasLast = false;
    /* The id of the group within the block, with each block starting from 0 */
    uint32_t                    logicalGID = 0;
    BlockCommandPtr             blockCmd = nullptr;
    /* Runtime status of the block */
    BlockStatus                 status = BlockStatus::FREE;
    PMUInfo                     pmuInfo;
    /* Dispatched result */
    uint32_t                    tid = 0;
    ShapeLoopInfo               shapelpinfo;
    uint32_t                    offset = 0;
    uint32_t                    totalCnt = 0;
    uint32_t                    dispCnt = 0;
    // uint32_t lc0-lc2 information
    GBufferEntry() : vld(false), hasLast(false), status(BlockStatus::FREE), tid(0) {}
    bool CheckOver() const
    {
        return (dispCnt == totalCnt);
    }
    bool CheckLastGroup() const
    {
        return hasLast && CheckOver();
    }
};

class GBufferState {
public:
    GBufferState();
    explicit GBufferState(uint32_t tid, uint32_t depth, uint32_t baseGid)
        : m_depth(depth), m_tid(tid), m_baseGidVal(baseGid) {}
    ~GBufferState();
public:
    bool                                Write(const GBufferEntry &entry);
    GBufferEntry&                       Front();
    GBufferEntry&                       ReadyToDisp(uint32_t m_thdN);
    void                                Read();
    void                                Build();

    bool                                IsEmpty() const;
    bool                                IsFull() const;

    void                                Reset();
    GBufferEntry&                          GetEntry(ROBID &gid);
    void                                SetStatus(const ROBID &gid, BlockStatus status);
    bool                                CanDispatch();
    SimSys*                             GetSim();
    void                                UpdateReadPtr(uint32_t tid);
    void                                UpdateReadPtr(ROBID &gid);
    void                                SetSim(SimSys *sim);
    uint32_t                            GetAliveGroupNum() const { return m_count; }
    void                                SetAliveGroupNum(uint32_t count) { m_count = count; }
    const ROBID&                        GetWritePtr() const { return m_writePtr; }
    void                                SetWritePtr(ROBID& ptr) { m_writePtr = ptr; }
    const ROBID&                        GetReadPtr() const { return m_readPtr; }
    void                                SetReadPtr(ROBID& ptr) { m_readPtr = ptr; }
    uint32_t                            GetDepth() const { return m_depth; }
    const std::vector<GBufferEntry>&       GetEntries() const { return m_entries; }
    bool                                CheckOver() { return m_entries[m_readPtr.val].CheckOver(); }
    void                                SetFlush(FlushBus &bus, uint32_t m_thdN);

    std::vector<GBufferEntry>           m_entries;
private:
    SimSys*                             m_sim;
    uint32_t                            m_depth;
    /* Alive Group in GROB */
    uint32_t                            m_count;
    uint32_t                            m_tid;
    /* Allocate pointer - Block Decode */
    ROBID                               m_writePtr;
    /* Dispatch pointer - Block Dispatch */
    ROBID                               m_readPtr;
    /* Commit pointer - Block retire */
    uint32_t                            m_baseGidVal;
};


} // namespace JCore

#endif  // BLOCKISA_MODEL_VECTORCORE_GROBSTATE_H
