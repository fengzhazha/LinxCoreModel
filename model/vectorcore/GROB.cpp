#include "vectorcore/GROB.h"
#include "core/Bus.h"
#include "core/Core.h"

namespace JCore {

GROB::GROB() {}

GROB::~GROB() {}

void GROB::Work()
{
    Retire();
    Commit();
    Dispatch();
    Alloc();
    Stats();
}

void GROB::Build(uint32_t d, uint32_t fetchWidth)
{
    this->depth = d;
    this->m_entries.clear();
    this->fetchWidth = fetchWidth;
    this->size.resize(GetSim()->core->configs.scalar_smt_thread);
    this->m_entries.resize(GetSim()->core->configs.scalar_smt_thread);
    auto swimLog = GetSim()->GetSwimLogger();
    if (swimLog != nullptr) {
        swimLog->SetThreadName("Alive Group count", top->machineId, top->machineId + groupThreadId);
    }
}

void GROB::Alloc()
{
    for (uint32_t i = 0; i < fetchWidth; ++i) {
        if (m_lm2GROB->Empty()) {
            break;
        }

        GroupAllocInfo info = m_lm2GROB->Read();
        Alloc(info);
    }
}

void GROB::Alloc(GroupAllocInfo &allocInfo)
{
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[Vector " << top->m_coreId << " GROB Manager]: Alloc GROB, block "
                                        << dec << allocInfo.bid << ", stid: " << allocInfo.stid;
    uint32_t stid = allocInfo.stid;
    if (m_entries[stid].count(allocInfo.bid) != 0) {
        return;
    }
    GROBState info;
    info.vld = true;
    info.bid = allocInfo.bid;
    info.stid = allocInfo.stid;
    info.bpc = allocInfo.bpc;
    info.tpc = allocInfo.tpc;
    info.tag = allocInfo.tag;
    info.cnt = allocInfo.groupNum;
    info.dest = allocInfo.dest;
    info.destVld = allocInfo.destVld;
    info.tileId = allocInfo.tileId;
    info.createCycle = allocInfo.createCycle;
    info.blockCmd = allocInfo.blockCmd;
    info.insertGroupBufferCycle = GetSim()->getCycles();
    m_entries[stid][allocInfo.bid] = info;
    ++size[allocInfo.stid];
}

static void SetCyclesInfo(GroupPipeViewPtr groupInfo, const GroupInfo &info)
{
    groupInfo->cycleInfo = std::make_shared<CycleInfo>();
    groupInfo->cycleInfo->createCycle = info.createCycle;
    groupInfo->cycleInfo->decodeCycle = info.decodeCycle;
    groupInfo->cycleInfo->p1Cycle = info.p1Cycle;
    groupInfo->cycleInfo->e1Cycle = info.exeCycle;
    groupInfo->cycleInfo->completeCycle = info.completeCycle;
}

std::string GroupInfo::DumpSwim() const
{
    std::stringstream out;
    out << "B" << std::dec << bid.val << ",G" << gid.val;
    return out.str();
}

std::string GroupInfo::Dump() const
{
    std::stringstream out;
    out << "Vec Group[B" << std::dec << bid.val << ",G" << gid.val << "]";
    return out.str();
}

void GROB::PrintGroupPipeView(const GroupInfo& info, uint32_t stid)
{
    GroupPipeViewPtr groupInfo = std::make_shared<GroupPipeViewInfo>();
    groupInfo->bid = info.bid.val;
    groupInfo->gid = info.gid.val;
    groupInfo->logicGid = info.logicalGID;
    SetCyclesInfo(groupInfo, info);
    groupInfo->cycleInfo->retireCycle = GetSim()->getCycles() + 1;
    groupInfo->label = info.Dump();
    GetSim()->GetViewManager(stid)->RecordPARGroupCompleted(groupInfo); // need stid
}

void GROB::PrintGroupSwimLane(const GroupInfo& info)
{
    SwimLogData logData;
    logData.name = info.DumpSwim();
    logData.pid = CORE_TOP_MACHINE_ID;
    logData.tid = top->groupMachineId + info.tid;
    logData.sTime = info.p1Cycle;
    logData.eTime = GetSim()->getCycles();
    logData.eventId = GetSim()->GetEventId();
    GetSim()->AddDuration(logData);
}

void GROB::CommitGroup(GROBState &robEntry, uint32_t &maxCmt)
{
    std::map<ROBID, GroupInfo> &groupInfo = robEntry.groupInfo;
    for (auto it = groupInfo.begin(); it != groupInfo.end();) {
        if (it->second.status == BlockStatus::COMPLETED) {
            PrintGroupPipeView(it->second, robEntry.blockCmd->stid);
            PrintGroupSwimLane(it->second);
            GetSim()->GetVerifyManager(robEntry.blockCmd->stid)->RecordPARGroupCompleted(robEntry.bid.val,
                                                                                         it->second.logicalGID);
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[Vector " << top->m_coreId << " GROB Manager]:  Commit Block "
                << robEntry.bid << " Group " << it->second.gid;
            it = groupInfo.erase(it);
            auto swimLog = GetSim()->GetSwimLogger();
            if (swimLog != nullptr) {
                swimLog->AddCounterEvent(machineId, machineId + groupThreadId,
                                         TraceLog::CounterType::QUEUE_POP, 1);
            }
            ++robEntry.cmt;
            --maxCmt;
        } else {
            ++it;
        }

        if (maxCmt == 0) {
            break;
        }
    }
}

void GROB::Resolve(GROBState &robEntry)
{
    // only valid output can wake up others
    if (robEntry.destVld) {
        robEntry.blockCmd->cmdExecCompleted = true;
        top->vecBCCWakeupQ->Write(robEntry.blockCmd);
    }
    GetSim()->core->bctrl->blockROB.completeBlock(robEntry.bid, false, robEntry.blockCmd->stid);
    stashCompQ->Write(robEntry.blockCmd);
    top->GetPE()->stats->binsts++;
    robEntry.vld = false;
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[Vector " << top->m_coreId << " GROB Manager]:  Resolve Block "
        << robEntry.bid;

    top->DeallocBlockCmdBuffer(robEntry.blockCmd);

    SwimLogData logData;
    logData.name = robEntry.blockCmd->DumpVecBriefName(GetSim()->core->configs.simtLane);
    logData.pid = CORE_TOP_MACHINE_ID;
    logData.tid = robEntry.blockCmd->execMachineId;
    logData.sTime = robEntry.blockCmd->startCalCycle;
    logData.eTime = GetSim()->getCycles();
    logData.hint = robEntry.blockCmd->DumpSwimInfo();
    robEntry.blockCmd->eventId = (robEntry.blockCmd->eventId >= 0) ? robEntry.blockCmd->eventId :
                                                                     GetSim()->GetEventId();
    logData.eventId = robEntry.blockCmd->eventId;
    GetSim()->AddDuration(logData);
    GetSim()->AddDependency(robEntry.bid.val, robEntry.blockCmd->eventId, robEntry.blockCmd->stid);
    --size[robEntry.blockCmd->stid];
}

void GROB::Commit()
{
    for (uint32_t stid = 0; stid < GetSim()->core->configs.scalar_smt_thread; ++stid) {
        if (size[stid] == 0) {
            continue;
        }
        uint32_t width = GetSim()->core->configs.threadCount;
        auto &stidEntry = m_entries[stid];
        for (auto entryPtr = stidEntry.begin(); entryPtr != stidEntry.end() && width > 0; ++entryPtr) {
            if (entryPtr->second.retired) {
                continue;
            }

            ASSERT(entryPtr->first == entryPtr->second.bid);
            CommitGroup(entryPtr->second, width);
            if (entryPtr->second.CheckOver()) {
                LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[Vector " << top->m_coreId
                    << " GROB Manager]:  Send drain sigal for scb. Block " << entryPtr->second.bid << " stid " << stid;
                ScbDrainBus drainInfo = {0, entryPtr->second.bid, stid};
                m_drainBusQ[stid]->Write(drainInfo);
                entryPtr->second.retired = true;
                top->m_lgprRF->lFreeListInput.ReleaseBlock(entryPtr->second.bid, stid);
                top->m_lgprRF->lFreeListOutput.ReleaseBlock(entryPtr->second.bid, stid);
            }
        }
    }
}

void GROB::Retire()
{
    uint32_t width = GetSim()->core->configs.threadCount;
    for (uint32_t stid = 0; stid < m_drainRespBusQ.size(); ++stid) {
        for (uint32_t i = 0; !m_drainRespBusQ[stid]->Empty() && i < width; ++i) {
            ScbDrainBus drainWakeInfo = m_drainRespBusQ[stid]->Read();
            ROBID bid = drainWakeInfo.bid;
            uint32_t stId = drainWakeInfo.stid;
            ASSERT(stId == stid);

            ASSERT(m_entries[stid].count(bid) != 0 && m_entries[stid][bid].CheckOver() && m_entries[stid][bid].retired);
            Resolve(m_entries[stid][bid]);
            // TODO: Alloc and Release sim queue need to change together.
            // m_GROB2MaskFile->Write(bid);
            MaskReleaseInfo maskInfo;
            maskInfo.bid = bid;
            maskInfo.stid = drainWakeInfo.stid;
            top->m_gBuffer->m_maskFile->Release(maskInfo);
            m_entries[stid].erase(bid);
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[Vector " << top->m_coreId << " GROB Manager]: Report retire Block "
                << drainWakeInfo.bid << " stid " << drainWakeInfo.stid;
        }
    }
}

void GROB::ReportCommitGroup(const ROBID &bid, const ROBID &gid, uint32_t stid)
{
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[Vector " << top->m_coreId
        << " GROB Manager]: Report Group complete. Block " << bid << " Group " << gid;
    if (!Find(bid, gid, stid)) {
        LOG_ERROR << "[Vector " << top->m_coreId << " GROB Manager]: Unkown bid in GROB!, bid " << bid << " gid "
            << gid;
        abort();
    }

    m_entries[stid][bid].groupInfo[gid].completeCycle = GetSim()->getCycles();
    m_entries[stid][bid].groupInfo[gid].exeCycle = GetSim()->getCycles() - 1;
    m_entries[stid][bid].groupInfo[gid].status = BlockStatus::COMPLETED;
    ASSERT(m_entries[stid][bid].cmt <= m_entries[stid][bid].cnt);
}

bool GROB::CheckStall(uint32_t reserved, uint32_t stid)
{
    if (m_entries[stid].size() + reserved > depth) {
        return true;
    }

    return false;
}

bool GROB::Find(const ROBID &bid, const ROBID &gid, uint32_t stid)
{
    return (m_entries[stid].count(bid) != 0) && (m_entries[stid][bid].groupInfo.count(gid) != 0);
}

SimSys* GROB::GetSim()
{
    return sim;
}

void GROB::SetSim(SimSys *sm)
{
    sim = sm;
}

void GROB::SetFlush(FlushBus &bus)
{
    uint32_t coreId = top != nullptr ? top->m_coreId : mtop->coreId;
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[Vector " << coreId << " GROB Manager]: Receive flush request " << bus;
    uint32_t stid = bus.req.stid;
    for (auto it = m_entries[stid].begin(); it != m_entries[stid].end();) {
        if (LessEqual(bus.req.bid, it->first)) {
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[Vector " << coreId << " GROB Manager]:  flush bid"
                << it->first;
            auto swimLog = GetSim()->GetSwimLogger();
            if (swimLog != nullptr) {
                swimLog->AddCounterEvent(machineId, machineId + groupThreadId,
                                         TraceLog::CounterType::QUEUE_POP, it->second.groupInfo.size());
            }
            it->second.groupInfo.clear();
            it = m_entries[stid].erase(it);
            --size[stid];
        } else {
            ++it;
        }
    }
}

void GROB::Dispatch()
{
    for (uint32_t i = 0; i < fetchWidth; ++i) {
        if (m_GBuffer2GROB->Empty()) {
            break;
        }

        GroupIssueInfo info = m_GBuffer2GROB->Read();
        CreateGroup(info);
    }
}

void GROB::CreateGroup(GroupIssueInfo &gInfo)
{
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[Vector " << top->m_coreId << " GROB Manager]: Create group. Block "
        << gInfo.bid << " Group " << gInfo.gid;
    ASSERT(CheckAlloc(gInfo.bid, gInfo.stid));
    GroupInfo info;
    info.tid = gInfo.tid;
    info.bid = gInfo.bid;
    info.gid = gInfo.gid;
    info.stid = gInfo.stid;
    info.logicalGID = gInfo.gid.val;
    info.createCycle = m_entries[gInfo.stid][gInfo.bid].createCycle;
    info.decodeCycle = m_entries[gInfo.stid][gInfo.bid].insertGroupBufferCycle;
    info.p1Cycle = gInfo.p1Cycle;
    info.status = BlockStatus::ALLOCATED;
    m_entries[gInfo.stid][gInfo.bid].groupInfo[gInfo.gid] = info;
    auto swimLog = GetSim()->GetSwimLogger();
    if (swimLog != nullptr) {
        swimLog->AddCounterEvent(machineId, machineId + groupThreadId,
                                 TraceLog::CounterType::QUEUE_PUSH, 1);
    }
}

void GROB::ReportAllocMinst(ROBID &bid, ROBID &gid, uint32_t stid)
{
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[Vector " << top->m_coreId << " GROB Manager]: Alloc inst in prob. Block "
        << bid << " Group " << gid;
    ASSERT(CheckAlloc(bid, stid));
    GroupInfo &info = m_entries[stid][bid].groupInfo[gid];
    if (info.status == BlockStatus::ALLOCATED) {
        info.status = BlockStatus::DISPATCHED;
    }
}

ROBID GROB::GetOldestGid(uint32_t stid)
{
    if (size[stid] == 0) {
        ROBID id;
        return id;
    }
    // TODO:: simt lsu 支持滑动窗口处理，没有此判断
    ASSERT(!m_entries[stid].empty());
    GROBState &robEntry = m_entries[stid].begin()->second;
    if (robEntry.groupInfo.size() == 0) {
        ROBID id;
        return id;
    }
    ASSERT(!robEntry.groupInfo.empty());
    return robEntry.groupInfo.begin()->second.gid;
}

ROBID GROB::GetOldestBid(uint32_t stid)
{
    if (size[stid] == 0) {
        ROBID id;
        return id;
    }
    // TODO:: simt lsu 支持滑动窗口处理，没有此判断
    GROBState &robEntry = m_entries[stid].begin()->second;
    if (robEntry.groupInfo.size() == 0) {
        ROBID id;
        return id;
    }

    return robEntry.groupInfo.begin()->second.bid;
}

bool GROB::CheckAlloc(const ROBID &bid, uint32_t stid)
{
    return m_entries[stid].count(bid) != 0;
}

bool GROB::IsIdle(uint32_t stid)
{
    return m_entries[stid].empty();
}

void GROB::Stats()
{
    for (uint32_t stid = 0; stid < m_entries.size(); ++stid) {
        if (!IsIdle(stid)) {
            if (top != nullptr) {
                ++top->m_stats.grobBusyCyc;
                ChecPEBussy(stid);
            }
        }
    }
}

void GROB::ChecPEBussy(uint32_t stid)
{
    if (m_entries[stid].empty()) {
        return;
    }

    for (auto &e : m_entries[stid]) {
        if (e.second.cmt < e.second.cnt) {
            top->m_stats.SetPeBussy(1);
            break;
        }
    }
}

void GROB::SetGroupStartCycle(ROBID bid, uint64_t cycle, uint32_t stid)
{
    if (m_entries[stid][bid].blockCmd->startCalCycle == 0) {
        m_entries[stid][bid].blockCmd->startCalCycle = cycle;
    }
}
} // namespace JCore
