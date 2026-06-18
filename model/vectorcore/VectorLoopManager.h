#ifndef   VECTOR_LOOP_MANAGER_H
#define   VECTOR_LOOP_MANAGER_H
#include <map>
#include <vector>

#include "interface/Credit.h"
#include "ModelCommon/SimInstInfo.h"
#include "vectorcore/VectorCommon.h"
#include "vectorcore/VectorCT.h"

namespace JCore {

class VectorCore;
struct WorkingBCCReq {
    uint64_t                    groupNum;
    ShapeLoopInfo               shapelpinfo;
    uint64_t                    commitGroupNum;
    uint64_t                    dispatchGroupNum;
    uint64_t                    initCycle = 0;

    WorkingBCCReq() = default;
    WorkingBCCReq(uint64_t groupNum, ShapeLoopInfo lbinfo)
        : groupNum(groupNum), shapelpinfo(lbinfo), commitGroupNum(0), dispatchGroupNum(0) {}
};

class VectorLoopManager : public VCore::VectorCommon {
public:
    VectorLoopManager() = delete;
    VectorLoopManager(uint32_t thdN, uint32_t lanes, uint32_t workReqNum)
        : m_thdN(thdN), m_lanes(lanes), m_workDepth(workReqNum) {}
    ~VectorLoopManager() {}

    void                                    Work();
    void                                    Xfer();
    void                                    Build();
    void                                    Reset();
    void                                    ReqQueryInfo();
    void                                    RespQueryInfo();
    void                                    RespViex();
    void                                    HandleInputReg();
    void                                    ReqPtagFromOutFreeList();
    void                                    RespPtagFromOutFreeList();
    void                                    HandleOutputReg();
    void                                    HandleRegList();
    void                                    ReceiveRequest();
    void                                    SendStashReq();
    void                                    RecvStashWayId();
    void                                    RecvStashRslvWakeup();
    void                                    IssueBlockcmdToExe();
    bool                                    CheckDispEnable();
    void                                    Dispatch();
    void                                    ReportCommitGroup(const ROBID &bid, const ROBID &gid, uint32_t stid);
    SimSys*                                 GetSim();
    void                                    SetSim(SimSys *sim);
    void                                    SetFlush(FlushBus &bus);
    void                                    HeadPrefetch(BlockCommandPtr cmd) const;
    void                                    Dump();

public:
    INPUT SimQueue<BlockCommandPtr>*                            bccVecBlockCmdQ;
    OUTPUT SimQueue<BlockCommandPtr>*                           vecBCCWakeupQ;

    OUTPUT SimQueue<Credit>*                                    m_vecBCCCreditQ;

    OUTPUT SimQueue<VCore::GBufferAllocReq>*                    m_lm2GBufferQ;
    OUTPUT SimQueue<GroupAllocInfo>*                            m_lm2GROB;

    OUTPUT SimQueue<LocalFreeInfo>*                             rreq_vec_srf_q = nullptr;
    INPUT SimQueue<LocalFreeInfo>*                              rrsp_srf_vec_q = nullptr;

    OUTPUT SimQueue<LocalFreeInfo>*                             wreq_vec_srf_q = nullptr;
    INPUT SimQueue<LocalFreeInfo>*                              wrsp_srf_vec_q = nullptr;

    OUTPUT SimQueue<LocalFreeInfo>*                             req_vec_siex_q = nullptr;
    INPUT SimQueue<LocalFreeInfo>*                              rsp_siex_vec_q = nullptr;

    OUTPUT SimQueue<LocalFreeInfo>*                             data_vec_viex_q = nullptr;
    INPUT SimQueue<LocalFreeInfo>*                              data_viex_vec_q = nullptr;

    OUTPUT SimQueue<BlockCommandPtr>        *stashCmdQ;
    INPUT SimQueue<BlockCommandPtr>         *stashAllocDoneQ;
    INPUT SimQueue<BlockCommandPtr>         *stashRslvQ;

    SimSys*                                                     m_sim;

    VectorCore*                             top;
    VectorCT*                               m_pVCT;
    std::deque<BlockCommandPtr>             handleRegisterCMDQ;

    std::queue<LocalFreeInfo>               handleInputRegisterQ;
    std::queue<LocalFreeInfo>               handleOutputRegisterQ;
    std::queue<ROBID>                       lastInQ;
    std::queue<ROBID>                       lastOutQ;
    std::deque<BlockCommandPtr>             workingCMDQ;
    std::queue<LocalFreeInfo>               obtainLPtagDataQ;
    std::queue<LocalFreeWriteInfo>          writeToLPtagDataQ;
private:

    uint64_t                                m_lc0;
    uint64_t                                m_lc1;
    uint64_t                                m_lc2;
    uint64_t                                m_lb0;
    uint64_t                                m_lb1;
    uint64_t                                m_lb2;

    const uint32_t                          m_thdN;
    const uint32_t                          m_lanes;

    const uint32_t                          m_workDepth;
    std::deque<BlockCommandPtr>                  cmdMonitor;
};

} // namespace JCore

#endif