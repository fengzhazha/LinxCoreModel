#ifndef   MEMORY_LOOP_MANAGER_H
#define   MEMORY_LOOP_MANAGER_H
#include <map>
#include <vector>

#include "mtccore/memcore/MemoryCommon.h"
#include "mtccore/memcore/MemoryCT.h"
#include "vectorcore/VectorCommon.h"

namespace JCore {

class MemoryCore;
struct MtcWorkingBCCReq {
    uint64_t                    groupNum;
    ShapeLoopInfo               shapelpinfo;
    uint64_t                    commitGroupNum;
    uint64_t                    dispatchGroupNum;
    uint64_t                    initCycle = 0;

    MtcWorkingBCCReq() = default;
    MtcWorkingBCCReq(uint64_t groupNum, ShapeLoopInfo lbinfo)
        : groupNum(groupNum), shapelpinfo(lbinfo), commitGroupNum(0), dispatchGroupNum(0) {}
};

struct MtcBCCTLoadStoreReq {
    uint32_t                size;
    TileBridgeBus           tile;
    bool                    isLoad = false;
    uint32_t                peID = 0;
    ExecEngineTyp           iexTyp = SCALAR_IEX;
    uint32_t                pipeID = 0; // Use for load-to-use.
    uint64_t                tid = 0;
    ROBID                   bid;
    uint64_t                tpc;
    uint64_t                addr;
    BlockCommandPtr         cmd;
};

struct MtcTloadTmpInfo {
    uint32_t                subCnt = 0;
    bool                    lastInstFin = true;
    ROBID                   currentRid;
};

struct FakeTcopyEntry {
    BlockCommandPtr cmd;
    uint32_t timer;
    FakeTcopyEntry(BlockCommandPtr cmdPtr, uint32_t cycle) : cmd(cmdPtr), timer(cycle) {}
};
class MemoryLoopManager : public MCore::MemoryCommon {
public:
    MemoryLoopManager() = delete;
    MemoryLoopManager(uint32_t thdN, uint32_t lanes, uint32_t workReqNum)
        : m_thdN(thdN), m_lanes(lanes), m_workDepth(workReqNum) {}
    ~MemoryLoopManager() {}

    void                                   Work();
    void                                   Xfer();
    void                                   Build();
    void                                   Reset();
    void                                   ReqQueryInfo();
    void                                   RespQueryInfo();
    void                                   RespViex();
    void                                   HandleInputReg();
    void                                   ReqPtagFromOutFreeList();
    void                                   RespPtagFromOutFreeList();
    void                                   HandleOutputReg();
    void                                   HandleRegList();
    void                                   ReceiveRequest();
    bool                                   CheckDispEnable();
    void                                   Dispatch();
    void                                   ReportCommitGroup(const ROBID &bid, const ROBID &gid, uint32_t stid);
    SimSys*                                GetSim();
    void                                   SetSim(SimSys *sim);
    void                                   SetFlush(FlushBus &bus);
    void                                   ProduceTloadInsts(MtcBCCTLoadStoreReq req);
    void                                   ProduceTStoreInst(MtcBCCTLoadStoreReq req, uint64_t addr, SimInst& inst) const;
    void                                   ProduceTStoreReq(MtcBCCTLoadStoreReq req);
    void                                   ProduceNormTStoreReq(MtcBCCTLoadStoreReq req);
    void                                   SendTLoadStoreInstReq();
    void                                   TstoreSendReadTileReq(SimInst &inst) const;
    void                                   InitTloadTstoreReq(BlockCommandPtr cmd, MtcBCCTLoadStoreReq& req) const;
    void                                   FakePickTcopy();
    void                                   SendPrefetchReqToSoc(SimInst inst);
    INPUT SimQueue<BlockCommandPtr>*                            bccMtcBlockCmdQ;
    OUTPUT SimQueue<BlockCommandPtr>*                           mtcBCCWakeupQ;
    OUTPUT SimQueue<VCore::GBufferAllocReq>*                    m_lm2GBufferQ;

   // INPUT  SimQueue<BCCVecReq>*                                 m_bccVecReqQ;
    OUTPUT std::deque<SimInst>*                                     m_scalperInstQ;
    // OUTPUT SimQueue<MCore::GBufferAllocReq>*                    m_lm2GBufferQ;

    OUTPUT SimQueue<GroupAllocInfo>*                            m_lm2GROB;
    // OUTPUT SimQueue<VecBCCRslv>*                                m_vecBCCRslvQ;
    OUTPUT SimQueue<LocalFreeInfo>*                             rreq_vec_srf_q = nullptr;
    INPUT SimQueue<LocalFreeInfo>*                              rrsp_srf_vec_q = nullptr;

    OUTPUT SimQueue<LocalFreeInfo>*                             wreq_vec_srf_q = nullptr;
    INPUT SimQueue<LocalFreeInfo>*                              wrsp_srf_vec_q = nullptr;

    OUTPUT SimQueue<LocalFreeInfo>*                             req_vec_siex_q = nullptr;
    INPUT SimQueue<LocalFreeInfo>*                              rsp_siex_vec_q = nullptr;

    OUTPUT SimQueue<LocalFreeInfo>*                             data_vec_viex_q = nullptr;
    INPUT SimQueue<LocalFreeInfo>*                              data_viex_vec_q = nullptr;
    SimSys*                                                     m_sim;

    MemoryCore*                             top;
    MemoryCT*                               m_pVCT;
    std::queue<BlockCommandPtr>             handleRegisterCMDQ;
    std::queue<LocalFreeInfo>               handleInputRegisterQ;
    std::queue<LocalFreeInfo>               handleOutputRegisterQ;
    std::queue<ROBID>                       lastInQ;
    std::queue<ROBID>                       lastOutQ;
    std::queue<BlockCommandPtr>             workingCMDQ;
    std::queue<LocalFreeInfo>               obtainLPtagDataQ;
    std::queue<LocalFreeWriteInfo>          writeToLPtagDataQ;
    std::deque<SimInst>                       tloadInstQ;
    std::deque<SimInst>                       tstoreInstQ;
    std::vector<FakeTcopyEntry>             tcopyCmdEntries;
private:
    uint64_t                                m_lc0;
    uint64_t                                m_lc1;
    uint64_t                                m_lc2;
    uint64_t                                m_lb0;
    uint64_t                                m_lb1;
    uint64_t                                m_lb2;
    const uint32_t                          m_thdN;
    const uint32_t                          m_lanes;
    MtcTloadTmpInfo                         m_tloadInfo;
    const uint32_t                          m_workDepth;
};

} // namespace JCore

#endif