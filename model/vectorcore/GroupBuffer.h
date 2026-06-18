#ifndef BLOCKISA_MODEL_VECTORCORE_GBUFFER_H
#define BLOCKISA_MODEL_VECTORCORE_GBUFFER_H
#include <vector>

#include "vectorcore/GBufferState.h"
#include "vectorcore/VectorCommon.h"
#include "vectorcore/VectorLoopManager.h"
#include "vectorcore/LastMaskFile.h"
#include "mtccore/memcore/MemoryLoopManager.h"
#include "pe/PECommon/BlockRunInfo.h"

namespace JCore {

class VectorCore;
class MemoryCore;
class GroupBuffer : public SimObj {
public:
    VectorCore*                         top;
    MemoryCore*                         mtop;
    void                                Work();
    void                                Build(uint32_t thdNum, uint32_t depth, uint32_t fetchWidth);
    void                                Reset();
    void                                Xfer();
    void ReportStat() override {}

    // The thread allocation algorithm is implemented here.
    // Current implementation: Loop to find an available thread to write
    void                                Alloc();
    bool                                IsEmpty() const;
    void                                SetGroupComplete(ROBID &gid, uint32_t tid);
    void                                ReportCommitGroup(ROBID bid);
    bool                                PickThread(uint32_t &pickThead);
    const GBufferEntry&                 PickGroup(uint32_t pickThead);
    bool                                DrainScbData(uint32_t &bid);
    bool                                CanDispatch();
    void                                CheckRelease(uint32_t pickThead);
    void                                Dispatch();

    bool                                IsStall() const {return m_stall;}
    void                                SetStall() {m_stall = true;}
    void                                UnSetStall() {m_stall = false;}
    SimSys*                             GetSim();
    void                                SetSim(SimSys *sim);

    void                                PrintStatus();
    void                                SetFlush(FlushBus &bus);
    void                                Stats();
    void                                UpdateReadPtr(ROBID &gid, uint32_t tid);
    void                                PrintEntryStatus(const GBufferEntry& entry, uint32_t idx);

    std::shared_ptr<VectorLoopManager>          m_vectorLM;
    std::shared_ptr<LastMaskFile>               m_maskFile;
    std::shared_ptr<MemoryLoopManager>          m_memoryLM;
    OUTPUT ThdRingQ<BlkBodyFetchState>*         m_peIfuQ;
    OUTPUT std::vector<RingQueue<BlockRunInfo>*>m_workLoadQ;
    OUTPUT SimQueue<GroupIssueInfo>*            m_GBuffer2GROB;
    INPUT  SimQueue<VCore::GBufferAllocReq>*    m_lm2GBufferQ;

private:
    SimSys*                             m_sim;
    uint32_t                            m_thdWPtr = 0;
    uint32_t                            m_thdRPtr = 0;

    uint32_t                            m_thdN = 0;
    std::vector<GBufferState>           m_buffers;
    bool                                m_stall = false;
    uint32_t                            m_fetchWidth = 0;
    uint32_t                            leftGroups = 0;
    VCore::GBufferAllocReq              curReq;
};


} // namespace JCore

#endif  // BLOCKISA_MODEL_VECTORCORE_GBUFFER_H
