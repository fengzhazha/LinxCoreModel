#include "vectorcore/GroupScheduler.h"

#include "core/Core.h"
#include "vectorcore/GROB.h"

namespace JCore {

void GroupScheduler::Build()
{
    bidBufferMaxSize = 1;
    groupSlotsMaxSize = 32;
    headBufferMaxSize = 32;
    freeSlotsIdList.resize(groupSlotsMaxSize);
    for (uint64_t i = 0; i < groupSlotsMaxSize ; i++) {
        freeSlotsIdList[i] = true;
    }
}

void GroupScheduler::Work()
{
    StallStatusTransition();
    Dispatch();
    GSArbiter();
    Ptag2Dispatch();
    HandleInputReg();
    HandleOutputReg();
    HandleRegList();
    InsertGroupSlots();
    IssueBlockcmdToExe();
    ReceiveRequest();
    ReceiveVGtherDone();
    Dump();
}

void GroupScheduler::Xfer() const
{
}

void GroupScheduler::TriggerStatusTransition()
{
    for (uint32_t i = 0; i < groupSlots.size(); i++) {
        if (groupSlots[i].stateTransitionCycle == 0 && groupSlots[i].state == GroupSlotState::STALLED) {
            groupSlots[i].state = GroupSlotState::ELIGIBLE;
        } else if (groupSlots[i].stateTransitionCycle > 0 && groupSlots[i].state == GroupSlotState::STALLED) {
            groupSlots[i].stateTransitionCycle--;
        }
    }
}

void GroupScheduler::ReqQueryInfo()
{
    uint32_t i = 0;
    while (!handleInputRegisterQ.empty() && i < top->core->configs.input_reg_num) {
        LocalFreeInfo info = handleInputRegisterQ.front();
        handleInputRegisterQ.pop();
        gsRreqVecSrfQ->Write(info);
        ASSERT(info.stid != -1U);
        ++i;

        LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << top->m_coreId << "] GS: Query info. bid "
            << info.bid << " offset " << info.offset;
    }
}

void GroupScheduler::RespQueryInfo() const
{
    while (!gsRrspSrfVecQ->Empty()) {
        LocalFreeInfo ptagInfo = gsRrspSrfVecQ->Read();

        LocalFreeInfo info = ptagInfo;
        gsDataVecViexQ->Write(info);
        LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << top->m_coreId
            << "] GS: Response info and write to vector iex. bid "
            << ptagInfo.bid << " offset " << ptagInfo.offset << " ptag " << dec << ptagInfo.ptag
            << " data 0x" << ptagInfo.data;
    }
}

void GroupScheduler::RespViex()
{
    while (!gsDataViexVecQ->Empty()) {
        LocalFreeInfo info = gsDataViexVecQ->Read();
        lastInQ.push_back(info.bid);
        LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << top->m_coreId
            << "] GS: Response last from iex. bid " << info.bid;
    }
}

void GroupScheduler::HandleInputReg()
{
    RespViex();
    RespQueryInfo();
    ReqQueryInfo();
}

void GroupScheduler::ReqPtagFromOutFreeList()
{
    while (!handleOutputRegisterQ.empty()) {
        LocalFreeInfo info = handleOutputRegisterQ.front();
        handleOutputRegisterQ.pop();
        gsWreqVecSrfQ->Write(info);
        LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << top->m_coreId
            << "] GS: Get ptag from freelist. bid " << info.bid << " offset " << info.offset;
    }
}

void GroupScheduler::RespPtagFromOutFreeList()
{
    while (!gsWrspSrfVecQ->Empty()) {
        LocalFreeInfo info = gsWrspSrfVecQ->Read();
        if (!info.last) {
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << top->m_coreId << "] GS: RO Response from freelist."
                << " bid " << info.bid << " offset" << info.offset << " ptag" << info.ptag;
        } else {
            lastOutQ.push_back(info.bid);
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << top->m_coreId << "] GS: RO Response from freelist."
                << " bid " << info.bid << " last";
        }
    }
}

void GroupScheduler::HandleOutputReg()
{
    RespPtagFromOutFreeList();
    ReqPtagFromOutFreeList();
}

void GroupScheduler::HandleRegList()
{
    while (!handleRegisterCMDQ.empty()) {
        BlockCommandPtr cmd = handleRegisterCMDQ.front();
        handleRegisterCMDQ.pop_front();

        // Input reg list
        for (uint32_t i = 0; i < cmd->srcPtag.size(); i++) {
            LocalFreeInfo info;
            info.vld = true;
            info.bid = cmd->bid;
            info.stid = cmd->stid;
            info.ptag = cmd->srcPtag[i];
            info.data = cmd->srcData[i];
            info.offset = i;
            handleInputRegisterQ.push(info);
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << top->m_coreId
                << "] GS: Handle input list. bid " << info.bid << " offset " << info.offset;
        }
        LocalFreeInfo in;
        in.vld = true;
        in.bid = cmd->bid;
        in.stid = cmd->stid;
        in.last = true;
        handleInputRegisterQ.push(in);

        // Output reg list
        for (uint32_t i = 0; i < cmd->dstPtag.size(); i++) {
            LocalFreeInfo info;
            info.vld = true;
            info.bid = cmd->bid;
            info.stid = cmd->stid;
            info.ptag = cmd->dstPtag[i];
            info.offset = i;
            handleOutputRegisterQ.push(info);
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << top->m_coreId
                << "] GS: Handle output list. Bid " << info.bid << " offset " << info.offset;
        }
        LocalFreeInfo info;
        info.vld = true;
        info.bid = cmd->bid;
        info.stid = cmd->stid;
        info.last = true;
        handleOutputRegisterQ.push(info);

        workingCMDQ.push_back(cmd);
    }

    HandleInputReg();
    HandleOutputReg();
}

void GroupScheduler::ReceiveRequest()
{
    if (!bccMCallBlockCmdQ->Empty()) {
        BlockCommandPtr cmd = bccMCallBlockCmdQ->Read();
        assert(!IsBusy(cmd->bid, cmd->gid));
        cmdMonitor.push_back(cmd);
        LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << top->m_coreId << "] [GS]: recv :" << cmd->Dump();
    }
}

bool GroupScheduler::IsBusy(ROBID bid, ROBID gid)
{
    bool find = false;
    for (uint32_t i = 0; i < groupSlots.size(); i++) {
        if (groupSlots[i].valid && groupSlots[i].info.bid == bid && groupSlots[i].info.gid == gid) {
            find = true;
        }
    }
    return find;
}

void GroupScheduler::ReceiveVGtherDone()
{
    if (!vecBridgeRspQ->Empty()) {
        LOG_INFO_M(Unit::VECTOR, Stage::NA) << "ReceiveVGtherDone() ";
        VectorBridgeRsp rspCmd = vecBridgeRspQ->Read();
        ASSERT(rspCmd.groupSlotId >= 0 && rspCmd.groupSlotId < groupSlotsMaxSize);
        uint64_t find = FindGroupSlotsID(rspCmd.groupSlotId);
        ASSERT(find != groupSlotsMaxSize);
        ASSERT(groupSlots[find].vagtherCount > 0);
        --groupSlots[find].vagtherCount;
        if (groupSlots[find].vagtherCount == 0) {
            groupSlots[find].info.vGatherDone = true;
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << "mcall vgather done slot " << dec << rspCmd.groupSlotId
                << " " << groupSlots[find].info.cmd->Dump();
        }
    }
}

uint64_t GroupScheduler::FindGroupSlotsID(uint64_t groupSlotId)
{
    uint64_t find = groupSlotsMaxSize;
    for (uint64_t i = 0; i < groupSlots.size() ; i++) {
        if (groupSlots[i].groupslotsId == groupSlotId) {
            find = i;
            break;
        }
    }
    return find;
}

void GroupScheduler::IssueBlockcmdToExe()
{
    if (cmdMonitor.empty()) {
        return;
    }

    // case1 cmd_bid hit bidBuffer && hasrenamed) -> bypass ptag rename
    // case2 (cmd_bid miss bidBuffer) ->
    // case3 {if(bidBuffer not full) -> insert cmd_bid2bidBuffer;}
    // case4 else -> return
    for (auto it = cmdMonitor.cbegin(); it != cmdMonitor.cend(); ++it) {
        BlockCommandPtr cmd = *it;
        cmd->execMachineId = top->machineId;
        bool hit = false;
        bool renamed = false;

        for (uint32_t i = 0; i < bidBufferMcall.size(); i++) {
            if (bidBufferMcall[i].bid == cmd->bid) {
                hit = true;
                renamed = bidBufferMcall[i].renamed;
            }
        }

        if (hit && renamed) {
            if (top->m_grob->CheckStall(1, cmd->stid)) {
                return;
            }
            workingGroupQ.push_back(cmd);
            cmdMonitor.erase(it);
            return;
        } else if (!hit && bidBufferMcall.size() < bidBufferMaxSize) {
            if (top->m_grob->CheckStall(1, cmd->stid)) {
                return;
            }
            for (auto &src : cmd->srcs) {
                if (src->vld) {
                    uint32_t base = src->baseAddr & (~TILE_MASK);
                    uint32_t size = src->size;
                    top->RegisterTileAddr(cmd->bid, base, size, cmd->stid);
                }
            }
            const uint32_t maxReadLGrp = 12;
            const uint32_t maxWrtiteLGrp = 4;
            if (top->m_lgprRF->CheckStallInput(maxReadLGrp)) {
                return;
            }
            if (top->m_lgprRF->CheckStallOutput(maxWrtiteLGrp)) {
                return;
            }
            handleRegisterCMDQ.push_back(cmd);
            bidBufferMcall.push_back(PtagRenameEntry(cmd->bid, cmd->GetGroupNum(top->core->configs.simtLane)));
            cmdMonitor.erase(it);
            return;
        } else {
            return;
        }
    }
}

void GroupScheduler::Dump()
{
    if (LoggerManager::GetManager().level > LoggerLevel::DETAIL) {
        return;
    }
    LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "===== vec GS Dump start: =====";
    int j = 0;
    for (auto it = cmdMonitor.cbegin(); it != cmdMonitor.cend(); ++it) {
        BlockCommandPtr cmd = *it;
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "cmdMonitor entry_" << j << " bid: " << cmd->bid.val
            << " gid: " << cmd->gid.val;
        j++;
    }

    for (uint32_t i = 0; i < handleRegisterCMDQ.size(); i++) {
        BlockCommandPtr cmd = handleRegisterCMDQ[i];
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "handleRegisterCMDQ entry_" << i << " bid: " << cmd->bid.val
            << " gid: " << cmd->gid.val;
    }

    while (!handleOutputRegisterQ.empty()) {
        LocalFreeInfo info = handleOutputRegisterQ.front();
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "handleOutputRegisterQ " << " bid: " << info.bid;
        break;
    }

    while (!handleInputRegisterQ.empty()) {
        LocalFreeInfo info = handleInputRegisterQ.front();
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "handleInputRegisterQ " << " bid: " << info.bid;
        break;
    }

    auto& writeQ = gsRreqVecSrfQ->GetRawWriteData();
    auto& readQ = gsRreqVecSrfQ->GetRawReadData();

    for (uint32_t i = 0; i < writeQ.size(); i++) {
        LocalFreeInfo info = writeQ[i];
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "rreq_vec_srf_q  writeQ" << " bid: " << info.bid
            << " offset " << info.offset;
    }

    for (uint32_t i = 0; i < readQ.size(); i++) {
        LocalFreeInfo info = readQ[i];
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "rreq_vec_srf_q  readQ" << " bid: " << info.bid
            << " offset " << info.offset;
    }

    auto& rrspSrfVecWriteQ = gsRrspSrfVecQ->GetRawWriteData();
    auto& rrspSrfVecReadQ = gsRrspSrfVecQ->GetRawReadData();

    for (uint32_t i = 0; i < rrspSrfVecWriteQ.size(); i++) {
        LocalFreeInfo info = rrspSrfVecWriteQ[i];
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "gsRrspSrfVecQ  writeQ" << " bid: " << info.bid << " offset "
            << info.offset;
    }

    for (uint32_t i = 0; i < rrspSrfVecReadQ.size(); i++) {
        LocalFreeInfo info = rrspSrfVecReadQ[i];
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "gsRrspSrfVecQ  readQ" << " bid: " << info.bid << " offset "
            << info.offset;
    }

    while (!gsDataVecViexQ->Empty()) {
        LocalFreeInfo info = gsDataVecViexQ->Front();
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "data_vec_viex_q " << " bid: " << info.bid;
        break;
    }

    while (!gsDataViexVecQ->Empty()) {
        LocalFreeInfo info = gsDataViexVecQ->Front();
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "data_viex_vec_q " << " bid: " << info.bid;
        break;
    }

    for (auto it = lastInQ.begin(); it != lastInQ.end(); ++it) {
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "lastInQ:" << " bid: " << it->val;
    }

    for (auto it = lastOutQ.begin(); it != lastOutQ.end(); ++it) {
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "lastOutQ:" << " bid: " << it->val;
    }

    for (uint32_t i = 0; i < workingGroupQ.size(); i++) {
        BlockCommandPtr cmd = workingGroupQ[i];
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "workingGroupQ entry_" << i << " bid: " << cmd->bid.val
            << " gid: " << cmd->gid.val;
    }

    for (uint32_t i = 0; i < workingCMDQ.size(); i++) {
        BlockCommandPtr cmd = workingCMDQ[i];
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "workingCMDQ entry_" << i << " bid: " << cmd->bid.val
            << " gid: " << cmd->gid.val;
    }

    for (uint32_t i = 0; i < groupSlots.size(); i++) {
        if (groupSlots[i].valid) {
            LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << std::dec << "groupSlots Entry_" << i
                << " bid:" << groupSlots[i].info.bid.val << " gid:" << groupSlots[i].info.gid.val
                << "cmd bid:" << groupSlots[i].info.cmd->bid.val << "cmd gid:" << groupSlots[i].info.cmd->gid.val
                << " status:" << GetGrpSlotStateName(groupSlots[i].state) << " slotsId:" << groupSlots[i].groupslotsId;
                if (groupSlots[i].info.vGatherDone) {
                    LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "vGatherDone";
                }
                if (groupSlots[i].info.copyoutDone) {
                    LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "copyOutDone";
                }
        }
    }

    uint32_t index = 0;
    for (auto it = vGatherQ.begin(); it != vGatherQ.end(); ++it) {
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "vGatherQ Entry_" << index << " groupslots id:" << it->groupSlotId;
        index++;
    }

    std::stringstream ss;
    ss << "freeSlotsIdList:";
    for (uint64_t i = 0; i < groupSlotsMaxSize; i++) {
        ss << "index:" << i;
        if (freeSlotsIdList[i]) {
            ss << " <1> ";
        } else {
            ss << " <0> ";
        }
    }
    ss << std::endl;
    LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << ss.str();
    LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "===== vec GSDump end: =====";
}

bool GroupScheduler::CheckDispEnable()
{
    return !lastInQ.empty() && !lastOutQ.empty() && lastInQ.front() == lastOutQ.front();
}

static void SetCyclesInfo(GroupPipeViewPtr groupInfo, const VecGroupInfo &info)
{
    groupInfo->cycleInfo = std::make_shared<CycleInfo>();
    groupInfo->cycleInfo->createCycle = info.createCycle;
    groupInfo->cycleInfo->decodeCycle = info.decodeCycle;
    groupInfo->cycleInfo->p1Cycle = info.p1Cycle;
    groupInfo->cycleInfo->e1Cycle = info.exeCycle;
    groupInfo->cycleInfo->completeCycle = info.completeCycle;
}

std::string VecGroupInfo::Dump() const
{
    std::stringstream out;
    out << "Vec Group[B" << std::dec << bid.val << ",G" << gid.val << "]";
    return out.str();
}

void GroupScheduler::PrintGroupPipeView(VecGroupInfo& info)
{
    GroupPipeViewPtr groupInfo = std::make_shared<GroupPipeViewInfo>();
    groupInfo->bid = info.bid.val;
    groupInfo->gid = info.gid.val;
    groupInfo->logicGid = info.gid.val;
    SetCyclesInfo(groupInfo, info);
    groupInfo->cycleInfo->retireCycle = GetSim()->getCycles() + 1;
    groupInfo->label = info.Dump();
    GetSim()->GetViewManager(info.cmd->stid)->RecordPARGroupCompleted(groupInfo);
}

void GroupScheduler::InsertGroupSlots()
{
    if (groupSlots.size() >= groupSlotsMaxSize || workingGroupQ.size() == 0) {
        return;
    }
    BlockCommandPtr cmd = workingGroupQ.front();
    workingGroupQ.pop_front();
    VecGroupInfo info(cmd);
    info.startCycle = GetSim()->getCycles();
    GroupSlotsEntry gsEntry(info);
    gsEntry.valid = true;
    gsEntry.groupslotsId = GetFreeSlotsId();
    groupSlots.push_back(gsEntry);
}

uint32_t GroupScheduler::GetFreeSlotsId()
{
    uint64_t find = groupSlotsMaxSize;
    for (uint64_t i = 0; i < groupSlotsMaxSize ; i++) {
        if (freeSlotsIdList[i]) {
            freeSlotsIdList[i] = false;
            find = i;
            break;
        }
    }
    assert(find != groupSlotsMaxSize);
    return find;
}

void GroupScheduler::Ptag2Dispatch()
{
    if (workingCMDQ.empty() || !CheckDispEnable()) {
        return;
    }
    lastInQ.pop_front();
    lastOutQ.pop_front();
    BlockCommandPtr cmd = workingCMDQ.front();
    workingCMDQ.pop_front();
    for (uint32_t i = 0; i < bidBufferMcall.size(); i++) {
        if (bidBufferMcall[i].bid == cmd->bid) {
            bidBufferMcall[i].renamed = true;
        }
    }
    workingGroupQ.push_back(cmd);
}

// loop Ctrl 分发
void GroupScheduler::Dispatch()
{
    if (headBuffer.empty()) {
        return;
    }
    BlockCommandPtr cmd = headBuffer.front();
    headBuffer.erase(headBuffer.begin());
    VCore::GBufferAllocReq gBufferReq;
    uint32_t lb0 = cmd->lb0 == 0 ? 1 : cmd->lb0;
    uint32_t lb1 = cmd->lb1 == 0 ? 1 : cmd->lb1;
    uint32_t lb2 = cmd->lb2 == 0 ? 1 : cmd->lb2;
    uint64_t shapeSize = lb0 * lb1 * lb2;
    uint64_t lastGroupNum = shapeSize % m_lanes;

    ShapeLoopInfo shapeInfo = ShapeLoopInfo(1, 1, 1, 1, 1, lastGroupNum, m_lanes);
    if (lastGroupNum != 0 && (cmd->gid.val + 1) == cmd->GetGroupNum(top->core->configs.simtLane)) {
        shapeInfo.validLaneNum = lastGroupNum;
    }
    shapeInfo.dimReduction = cmd->IsReduceDimension();
    gBufferReq.vld = true;
    gBufferReq.blockCmd = cmd;
    gBufferReq.shapelpinfo = shapeInfo;
    m_lm2GBufferQ->Write(gBufferReq);
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "Dispatch mcall group to ifu " << gBufferReq.blockCmd->Dump();
}

void GroupScheduler::GSArbiter()
{
    if (groupSlots.empty()) {
        return;
    }

    if (headBuffer.size() == headBufferMaxSize) {
        return;
    }

    for (uint32_t i = 0; i < groupSlots.size(); i++) {
        if (groupSlots[i].state == GroupSlotState::ELIGIBLE) {
            headBuffer.push_back(groupSlots[i].info.cmd);
            groupSlots[i].state = GroupSlotState::SELECTED;
            groupSlots[i].info.vGatherDone = false;
            return;
        }
    }
}

void GroupScheduler::SetVgatherCounter(uint32_t groupSlotId, uint32_t cnt)
{
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << " SetVgatherCounter groupSlotId:" << groupSlotId << " cnt" << cnt;
    uint64_t find = groupSlotsMaxSize;
    for (uint64_t i = 0; i < groupSlots.size() ; i++) {
        if (groupSlots[i].groupslotsId == groupSlotId) {
            ASSERT(groupSlots[i].vagtherCount == 0);
            groupSlots[i].vagtherCount = cnt;
            find = i;
            break;
        }
    }
    assert(find != groupSlotsMaxSize);
}

void GroupScheduler::VgatherCounterPlus(ROBID bid, ROBID gid)
{
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "VgatherCounterPlus";
    uint64_t find = groupSlotsMaxSize;

    for (uint64_t i = 0; i < groupSlots.size() ; i++) {
        if (groupSlots[i].info.bid == bid && groupSlots[i].info.gid == gid) {
            groupSlots[i].vagtherCount++;
            find = i;
            break;
        }
    }
    assert(find != groupSlotsMaxSize);
}

void GroupScheduler::SetCopyoutCounter(uint32_t groupSlotId, uint32_t cnt)
{
    LOG_INFO_M(Unit::VECTOR, Stage::NA)  << " SetCopyoutCounter groupSlotId:" << groupSlotId << " cnt" << cnt;
    uint64_t find = groupSlotsMaxSize;
    for (uint64_t i = 0; i < groupSlots.size() ; i++) {
        if (groupSlots[i].groupslotsId == groupSlotId) {
            ASSERT(groupSlots[i].copyoutCount == 0);
            groupSlots[i].copyoutCount = cnt;
            find = i;
            break;
        }
    }
    assert(find != groupSlotsMaxSize);
}

void GroupScheduler::GetSrc(ROBID bid, ROBID gid, std::vector<TileOperandPtr>& srcs)
{
    bool find = false;
    for (uint32_t i = 0; i < groupSlots.size(); i++) {
        if (groupSlots[i].info.bid == bid && groupSlots[i].info.gid == gid) {
            find = true;
            srcs = groupSlots[i].info.srcs;
            return;
        }
    }
    ASSERT(find);
}

void GroupScheduler::GetDst(ROBID bid, ROBID gid, std::vector<TileOperandPtr>& dsts)
{
    bool find = false;
    for (uint32_t i = 0; i < groupSlots.size(); i++) {
        if (groupSlots[i].info.bid == bid && groupSlots[i].info.gid == gid) {
            find = true;
            dsts = groupSlots[i].info.dsts;
            return;
        }
    }
    ASSERT(find);
}

uint64_t GroupScheduler::TriggerCopyout(ROBID bid, ROBID gid, uint64_t tpc)
{
    uint32_t slotId = 0;
    uint64_t find = groupSlotsMaxSize;
    for (uint64_t i = 0; i < groupSlots.size() ; i++) {
        if (groupSlots[i].info.bid == bid && groupSlots[i].info.gid == gid) {
            ASSERT(groupSlots[i].state == GroupSlotState::SELECTED);
            groupSlots[i].info.tpc = tpc;
            groupSlots[i].state = GroupSlotState::STALLED;
            groupSlots[i].info.cmd->bTextPC = tpc;
            groupSlots[i].contextSwitchNum++;
            SetSwimlineEnd(bid, gid);
            slotId = groupSlots[i].groupslotsId;
            find = i;
            break;
        }
    }
    assert(find != groupSlotsMaxSize);
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "TriggerCopyout gid:" << std::dec << gid.val << " slotId:" << slotId;
    return slotId;
}

uint64_t GroupScheduler::GetSlotsID(ROBID bid, ROBID gid)
{
    uint32_t slotId = 0;
    uint64_t find = groupSlotsMaxSize;
    for (uint64_t i = 0; i < groupSlots.size() ; i++) {
        if (groupSlots[i].info.bid == bid && groupSlots[i].info.gid == gid) {
            ASSERT(groupSlots[i].state == GroupSlotState::SELECTED);
            slotId = groupSlots[i].groupslotsId;
            find = i;
            break;
        }
    }
    assert(find != groupSlotsMaxSize);
    return slotId;
}

void GroupScheduler::DecodeCopyoutInst(ROBID bid, ROBID gid)
{
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "DecodeCopyoutInst bid:" << std::dec << bid.val << " gid:" << gid.val;
    uint64_t find = groupSlotsMaxSize;
    for (uint64_t i = 0; i < groupSlots.size() ; i++) {
        if (groupSlots[i].info.bid == bid && groupSlots[i].info.gid == gid) {
            ASSERT(groupSlots[i].copyoutCount > 0);
            groupSlots[i].copyoutCount--;
            if (groupSlots[i].copyoutCount == 0) {
                groupSlots[i].info.copyoutDone = true;
            }
            find = i;
            break;
        }
    }
    assert(find != groupSlotsMaxSize);
}

uint32_t GroupScheduler::TriggerCopyin(ROBID bid, ROBID gid)
{
    bool find = false;
    for (uint32_t i = 0; i < groupSlots.size(); i++) {
        if (groupSlots[i].info.bid == bid && groupSlots[i].info.gid == gid) {
            ASSERT(groupSlots[i].state == GroupSlotState::SELECTED);
            find = true;
            groupSlots[i].contextSwitchNum++;
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << "TriggerCopyin gid" << std::dec << gid.val << " slotid:"
                << groupSlots[i].groupslotsId;
            return groupSlots[i].info.gprBaseAddr;
        }
    }
    ASSERT(find);
    return 0;
}

void GroupScheduler::StallStatusTransition()
{
    for (uint32_t i = 0; i < groupSlots.size(); i++) {
        if (groupSlots[i].state == GroupSlotState::STALLED && groupSlots[i].info.vGatherDone &&
            groupSlots[i].info.copyoutDone) {
            groupSlots[i].state = GroupSlotState::ELIGIBLE;
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << "StallStatusTransition gid:" << std::dec
                << groupSlots[i].info.gid.val << " slotid:" << groupSlots[i].groupslotsId;
        }
    }
}

void GroupScheduler::TriggerVgather(uint32_t groupSlotsID)
{
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "TriggerVgather groupSlotsID:" << std::dec << groupSlotsID;
    bool find = false;
    for (auto it = vGatherQ.begin(); it != vGatherQ.end(); ++it) {
        if (it->cmdType == ReqCmdType::VGATHER && it->groupSlotId == groupSlotsID) {
            find = true;
            vecBridgeReqQ->Write(*it);
            vGatherQ.erase(it);
            return;
        }
    }
    ASSERT(find);
    return;
}

void GroupScheduler::TriggerVscatter(uint32_t stqIndex)
{
    bool find = false;
    for (auto it = vGatherQ.begin(); it != vGatherQ.end(); ++it) {
        if (it->cmdType == ReqCmdType::VSCATTER && it->tid == stqIndex) {
            find = true;
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << "TriggerVscatter: Found VSCATTER req with stqIndex=" << stqIndex
                << ", sending to TileBridge";
            vecBridgeReqQ->Write(*it);
            vGatherQ.erase(it);
            return;
        }
    }
    if (!find) {
        LOG_DEBUG_M(Unit::VECTOR, Stage::NA) <<
            "TriggerVscatter ERROR: No VSCATTER req found with stqIndex=" << stqIndex;
    }
    ASSERT(find);
    return;
}

uint32_t GroupScheduler::GetTransactionId(uint32_t grouSlotId)
{
    return groupSlots[grouSlotId].contextSwitchNum;
}

bool GroupScheduler::CheckFirstDispatch(ROBID bid, ROBID gid)
{
    LOG_INFO_M(Unit::VECTOR, Stage::NA)  << "CheckFirstDispatch gid:" << gid.val;
    bool find = false;
    for (uint32_t i = 0; i < groupSlots.size(); i++) {
        if (groupSlots[i].info.bid == bid && groupSlots[i].info.gid == gid) {
            find = true;
            if (groupSlots[i].contextSwitchNum == 0) {
                return true;
            } else {
                return false;
            }
        }
    }
    ASSERT(find);
    return false;
}

void GroupScheduler::Resolve(ROBID bid, ROBID gid)
{
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "MCALL Group Resolve bid:" << bid.val << " gid:" << gid.val;
    bool find = false;
    for (uint32_t i = 0; i < groupSlots.size(); i++) {
        if (groupSlots[i].info.bid != bid || groupSlots[i].info.gid != gid) {
            continue;
        }
        ASSERT(groupSlots[i].state == GroupSlotState::SELECTED);
        groupSlots[i].state = GroupSlotState::RESOLVED;
        bool bidFind = false;
        uint32_t index = 0;
        auto it = bidBufferMcall.begin();
        while (it != bidBufferMcall.end()) {
            index++;
            if (it->bid != bid) {
                ++it;
                continue;
            } else {
                bidFind = true;
                it->resolvedGroupNum++;
                break;
            }
        }
        if (bidFind && it->resolvedGroupNum == it->totalGroupNum) {
            bidBufferMcall.erase(it);
        }
        assert(bidFind);
        freeSlotsIdList[groupSlots[i].groupslotsId] = true;
        LOG_INFO_M(Unit::VECTOR, Stage::NA) << "cmd dump" << groupSlots[i].info.cmd->Dump();
        PrintGroupPipeView(groupSlots[i].info);
        GetSim()->GetVerifyManager(groupSlots[i].info.cmd->stid)->RecordPARGroupCompleted(bid.val, gid.val);
        assert(groupSlots[i].info.cmd->bid == bid && groupSlots[i].info.cmd->gid == gid);
        SetSwimlineEnd(bid, gid);
        vecMCallWakeupQ->Write(groupSlots[i].info.cmd);
        groupSlots.erase(groupSlots.begin()+i);
        find = true;
        break;
    }
    ASSERT(find);
}

SimSys* GroupScheduler::GetSim()
{
    return m_sim;
}

void GroupScheduler::SetSim(SimSys *sim)
{
    m_sim = sim;
}

void GroupScheduler::SetFlush(FlushBus &bus)
{
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << top->m_coreId << "] GS:  Recv Flush " << bus;
    top->m_grob->SetFlush(bus);
    auto match = [&bus](LocalFreeInfo &info) {
        return info.stid == bus.req.stid && LessEqual(bus.req.bid, info.bid);
    };

    gsRreqVecSrfQ->FlushIf(match);
    gsRrspSrfVecQ->FlushIf(match);
    gsWreqVecSrfQ->FlushIf(match);
    gsWrspSrfVecQ->FlushIf(match);
    gsReqVecSiexQ->FlushIf(match);
    gsRspSiexVecQ->FlushIf(match);
    gsDataVecViexQ->FlushIf(match);
    gsDataViexVecQ->FlushIf(match);

    for (auto it = cmdMonitor.begin(); it != cmdMonitor.end();) {
        if (LessEqual(bus.req.bid, (*it)->bid) && (*it)->stid == bus.req.stid) {
            it = cmdMonitor.erase(it);
        } else {
            ++it;
        }
    }

    /* flush BlockCommandPtr Queue */
    auto flushBlockCommandPtrQ = [&bus](std::deque<BlockCommandPtr>& q) {
        for (auto it = q.cbegin(); it != q.cend();) {
            if (LessEqual(bus.req.bid, (*it)->bid) && (*it)->stid == bus.req.stid) {
                it = q.erase(it);
            } else {
                it++;
            }
        }
    };

    flushBlockCommandPtrQ(workingCMDQ);
    flushBlockCommandPtrQ(handleRegisterCMDQ);
}

std::string VecGroupInfo::DumpSwim() const
{
    std::stringstream out;
    out << "B" << std::dec << bid.val << ",G" << gid.val;
    return out.str();
}

void GroupScheduler::SetSwimlineStart(ROBID bid, ROBID gid, uint32_t tid)
{
    uint64_t find = groupSlotsMaxSize;
    for (uint64_t i = 0; i < groupSlots.size() ; i++) {
        if (groupSlots[i].info.bid == bid && groupSlots[i].info.gid == gid) {
            groupSlots[i].info.startCycle = GetSim()->getCycles();
            groupSlots[i].info.tid = tid;
            find = i;
            break;
        }
    }
    assert(find != groupSlotsMaxSize);
}

void GroupScheduler::SetSwimlineEnd(ROBID bid, ROBID gid)
{
    uint64_t find = groupSlotsMaxSize;
    for (uint64_t i = 0; i < groupSlots.size() ; i++) {
        if (groupSlots[i].info.bid == bid && groupSlots[i].info.gid == gid) {
            PrintGroupSwimLane(groupSlots[i].info);
            find = i;
            break;
        }
    }
    assert(find != groupSlotsMaxSize);
}

void GroupScheduler::PrintGroupSwimLane(VecGroupInfo& info)
{
    SwimLogData logData;
    logData.name = info.DumpSwim();
    logData.pid = CORE_TOP_MACHINE_ID;
    logData.tid = top->groupMachineId + info.gid.val;
    logData.sTime = info.startCycle;
    logData.eTime = GetSim()->getCycles();
    logData.eventId = GetSim()->GetEventId();
    GetSim()->AddDuration(logData);
    info.startCycle = 0;
    info.tid = 0;
}

} // namespace JCore
