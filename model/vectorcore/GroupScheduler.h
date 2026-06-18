#pragma once
#ifndef VECTOR_GROUP_SCHEDULER_H
#define VECTOR_GROUP_SCHEDULER_H

#include <vector>

#include "interface/Credit.h"
#include "ModelCommon/SimInstInfo.h"
#include "vectorcore/VectorCommon.h"
#include "vectorcore/VectorCT.h"
#include "ModelCommon/bus/VectorBridgeBus.h"

namespace JCore {

class VectorCore;
struct VecGroupInfo {
    uint64_t                                                    gprBaseAddr;
    uint64_t                                                    gprSize;
    std::vector<TileOperandPtr>                                 srcs;
    std::vector<TileOperandPtr>                                 dsts;
    uint64_t                                                    tpc;
    ROBID                                                       bid;
    ROBID                                                       gid;
    BIQType                                                     bType;
    BlockCommandPtr                                             cmd;
    bool                                                        vGatherDone = false;
    bool                                                        copyoutDone = false;

    uint64_t                                                    createCycle = 0;
    uint64_t                                                    decodeCycle = 0;
    uint64_t                                                    p1Cycle = 0;
    uint64_t                                                    exeCycle = 0;
    uint64_t                                                    completeCycle = 0;
    uint64_t                                                    retireCycle = 0;
    uint32_t                                                    tid = 0;
    uint32_t                                                    logicalGID = 0;
    uint64_t                                                    startCycle = 0;

    std::string DumpSwim() const;
    std::string Dump() const;
    VecGroupInfo() = default;
    explicit VecGroupInfo(BlockCommandPtr cmdTmp) : gprBaseAddr(0), gprSize(0), tpc(0), \
        bid(cmdTmp->bid), gid(cmdTmp->gid), bType(cmdTmp->biqType), cmd(cmdTmp)
    {
        for (auto &src: cmdTmp->srcs) {
            if (src->vld) {
                srcs.emplace_back(src);
            }
        }

        for (auto &dst: cmdTmp->dsts) {
            if (dst->vld) {
                dsts.emplace_back(dst);
            }
        }
    }
};

enum class GroupSlotState {
    ELIGIBLE = 0,
    STALLED = 1,
    SELECTED = 2,
    RESOLVED = 3,
    COUNT = 4,
};

inline std::string GetGrpSlotStateName(GroupSlotState state)
{
    static const std::array<std::string, static_cast<std::size_t>(GroupSlotState::COUNT)> GRP_STATE_NAMES = {{
        "ELIGIBLE",
        "STALLED",
        "SELECTED",
        "RESOLVED",
    }};
    const std::size_t idx = static_cast<std::size_t>(state);
    static const std::string INVALID_GRP = "invalid_grp_state";
    if (idx >= static_cast<std::size_t>(GroupSlotState::COUNT)) {
        return INVALID_GRP;
    }
    return GRP_STATE_NAMES[idx];
}

struct GroupSlotsEntry {
    VecGroupInfo                                                info;
    GroupSlotState                                              state = GroupSlotState::COUNT;
    int                                                         contextSwitchNum;
    // debug
    int                                                         stateTransitionCycle;
    bool                                                        valid;
    uint32_t                                                    vagtherCount = 0;
    uint32_t                                                    copyoutCount = 0;
    uint32_t                                                    groupslotsId;

    GroupSlotsEntry() = default;
    explicit GroupSlotsEntry(VecGroupInfo infoTmp)
        : info(infoTmp), state(GroupSlotState::ELIGIBLE), contextSwitchNum(0), stateTransitionCycle(300), valid(false)
    {}
};

struct PtagRenameEntry {
    ROBID                                                       bid;
    bool                                                        renamed;
    uint32_t                                                    totalGroupNum;
    uint32_t                                                    resolvedGroupNum;
    PtagRenameEntry() = default;
    PtagRenameEntry(ROBID bidTmp, uint32_t totalGroupNumTmp)
        : bid(bidTmp), renamed(false), totalGroupNum(totalGroupNumTmp), resolvedGroupNum(0) {}
};

class GroupScheduler : public VCore::VectorCommon {
public:
    GroupScheduler() = delete;
    GroupScheduler(uint32_t thdN, uint32_t lanes, uint32_t workReqNum)
        : m_thdN(thdN), m_lanes(lanes), m_workDepth(workReqNum) {}
    ~GroupScheduler() {}

    void                                                Work();
    void                                                Xfer() const;
    void                                                Build();
    void                                                Reset();
    void                                                ReqQueryInfo();
    void                                                RespQueryInfo() const;
    void                                                RespViex();
    void                                                HandleInputReg();
    void                                                ReqPtagFromOutFreeList();
    void                                                RespPtagFromOutFreeList();
    void                                                HandleOutputReg();
    void                                                HandleRegList();
    void                                                ReceiveRequest();
    void                                                ReceiveVGtherDone();
    uint64_t                                            FindGroupSlotsID(uint64_t groupSlotId);
    void                                                DecodeCopyoutInst(ROBID bid, ROBID gid);
    void                                                IssueBlockcmdToExe();
    bool                                                CheckDispEnable();
    void                                                Dispatch();
    void                                                GSArbiter();
    void                                                InsertGroupSlots();
    uint64_t                                            GetSlotsID(ROBID bid, ROBID gid);
    void                                                ReportCommitGroup(const ROBID &bid, const ROBID &gid) const;
    uint64_t                                            TriggerCopyout(ROBID bid, ROBID gid, uint64_t tpc);
    uint32_t                                            TriggerCopyin(ROBID bid, ROBID gid);
    void                                                TriggerVgather(uint32_t groupSlotsID);
    void                                                TriggerVscatter(uint32_t stqIndex);
    void                                                StallStatusTransition();
    uint32_t                                            GetTransactionId(uint32_t grouSlotId);
    void                                                GetSrc(ROBID bid, ROBID gid, std::vector<TileOperandPtr>& srcs);
    void                                                GetDst(ROBID bid, ROBID gid, std::vector<TileOperandPtr>& dsts);
    bool                                                CheckFirstDispatch(ROBID bid, ROBID gid);
    void                                                TriggerStatusTransition();
    void                                                Resolve(ROBID bid, ROBID gid);
    SimSys*                                             GetSim();
    void                                                SetSim(SimSys *sim);
    void                                                SetFlush(FlushBus &bus);
    void                                                Dump();
    void                                                Ptag2Dispatch();
    void                                                SetVgatherCounter(uint32_t groupSlotId, uint32_t cnt);
    void                                                VgatherCounterPlus(ROBID bid, ROBID gid);
    void                                                SetCopyoutCounter(uint32_t groupSlotId, uint32_t cnt);
    uint32_t                                            GetFreeSlotsId();
    bool                                                IsBusy(ROBID bid, ROBID gid);
    void                                                PrintGroupPipeView(VecGroupInfo &info);
    void                                                PrintGroupSwimLane(VecGroupInfo& info);
    void                                                SetSwimlineStart(ROBID bid, ROBID gid, uint32_t tid);
    void                                                SetSwimlineEnd(ROBID bid, ROBID gid);

public:
    INPUT SimQueue<BlockCommandPtr>*                    bccMCallBlockCmdQ;
    OUTPUT SimQueue<BlockCommandPtr>*                   vecMCallWakeupQ;

    OUTPUT SimQueue<VectorBridgeReq>*                   vecBridgeReqQ;
    INPUT SimQueue<VectorBridgeRsp>*                    vecBridgeRspQ;

    OUTPUT SimQueue<Credit>*                            m_vecBCCCreditQ;

    OUTPUT SimQueue<VCore::GBufferAllocReq>*            m_lm2GBufferQ;
    OUTPUT SimQueue<GroupAllocInfo>*                    m_lm2GROB;

    OUTPUT SimQueue<LocalFreeInfo>*                     gsRreqVecSrfQ = nullptr;
    INPUT SimQueue<LocalFreeInfo>*                      gsRrspSrfVecQ = nullptr;

    OUTPUT SimQueue<LocalFreeInfo>*                     gsWreqVecSrfQ = nullptr;
    INPUT SimQueue<LocalFreeInfo>*                      gsWrspSrfVecQ = nullptr;

    OUTPUT SimQueue<LocalFreeInfo>*                     gsReqVecSiexQ = nullptr;
    INPUT SimQueue<LocalFreeInfo>*                      gsRspSiexVecQ = nullptr;

    OUTPUT SimQueue<LocalFreeInfo>*                     gsDataVecViexQ = nullptr;
    INPUT SimQueue<LocalFreeInfo>*                      gsDataViexVecQ = nullptr;

    SimSys*                                             m_sim;

    VectorCore*                                         top;
    VectorCT*                                           m_pVCT;
    std::deque<BlockCommandPtr>                         handleRegisterCMDQ;

    std::queue<LocalFreeInfo>                           handleInputRegisterQ;
    std::queue<LocalFreeInfo>                           handleOutputRegisterQ;
    std::deque<ROBID>                                   lastInQ;
    std::deque<ROBID>                                   lastOutQ;
    std::deque<BlockCommandPtr>                         workingCMDQ;
    std::deque<BlockCommandPtr>                         workingGroupQ;
    std::queue<LocalFreeInfo>                           obtainLPtagDataQ;
    std::queue<LocalFreeWriteInfo>                      writeToLPtagDataQ;

    std::vector<PtagRenameEntry>                        bidBufferMcall;
    uint32_t                                            bidBufferMaxSize;

    std::vector<GroupSlotsEntry>                        groupSlots;
    uint32_t                                            groupSlotsMaxSize;

    std::deque<BlockCommandPtr>                         headBuffer;
    uint32_t                                            headBufferMaxSize;

    std::vector<VectorBridgeReq>                        vGatherQ;
    std::vector<bool>                                   freeSlotsIdList;
private:

    const uint32_t                                      m_thdN;
    const uint32_t                                      m_lanes;

    const uint32_t                                              m_workDepth;
    std::deque<BlockCommandPtr>                                 cmdMonitor;
};

} // namespace JCore

#endif