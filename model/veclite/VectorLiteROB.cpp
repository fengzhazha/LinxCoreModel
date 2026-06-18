#include "veclite/VectorLiteROB.h"
#include "veclite/VectorLite.h"
#include "core/Bus.h"
#include "core/Core.h"

namespace JCore {

VectorLiteROB::VectorLiteROB() {}
VectorLiteROB::~VectorLiteROB() {}

void VectorLiteROB::Xfer() const {}

void VectorLiteROB::Work()
{
    CheckRetired();
    CheckCommit();
    CheckResolved();
    CheckDisp();
    ReceivedCmd();
}

void VectorLiteROB::ReceivedCmd()
{
    while (!m_fetchRobInQ->Empty()) {
        BlockCommandPtr cmd = m_fetchRobInQ->Front();
        LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
            << "ROB, receive: " << cmd->Dump();
        if (!AllocEntry(cmd)) {
            break;
        }
        m_fetchRobInQ->Read();
    }
}

bool VectorLiteROB::AllocEntry(const BlockCommandPtr &cmd)
{
    if (entries.count(cmd->bid) > 0) {
        cout << "Error: bid " << dec << cmd->bid << " already exists." << endl;
        Dump();
        fflush(stdout);
        ASSERT(false);
    }
    // TODO: check stall and get the counters
    entries[cmd->bid] = VROBInfo(cmd);
    return true;
}

void VectorLiteROB::CheckDisp()
{
    while (!m_siqRobDispQ->Empty()) {
        SIQInfoPtr sInfo = m_siqRobDispQ->Read();
        LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
            << "ROB, dispatch: " << sInfo->cmd->Dump();
        UpdateDispStats(sInfo);
    }
    while (!m_siqRobDispLastQ->Empty()) {
        SIQInfoPtr sInfo = m_siqRobDispLastQ->Read();
        LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
            << "ROB, dispatch last uop: " << sInfo->cmd->Dump();
        UpdateDispLastStats(sInfo);
    }
}

void VectorLiteROB::UpdateDispStats(const SIQInfoPtr &sInfo)
{
    ROBID &bid = sInfo->cmd->bid;
    ASSERT(entries.count(bid) != 0);
    entries[bid].status = VROBStatus::DISPATCHED;
    entries[bid].uopCount = sInfo->uopCount;
    if (entries[bid].dispatchCycle == 0) {
        entries[bid].dispatchCycle = GetSim()->getCycles();
        swimCount[bid] = 1;
    }
}

void VectorLiteROB::UpdateDispLastStats(const SIQInfoPtr &sInfo)
{
    ROBID &bid = sInfo->cmd->bid;
    ASSERT(swimCount.count(bid) != 0);
    
    entries[bid].dispatchLastCycle = (entries[bid].dispatchCycle == GetSim()->getCycles()) ?
        (GetSim()->getCycles() + 1) : GetSim()->getCycles();
}

void VectorLiteROB::CheckResolved()
{
    while (!m_exeRobRslvQ->Empty()) {
        auto uop = m_exeRobRslvQ->Read();
        ROBID bid = uop->cmd->bid;
        LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
            << "ROB, uop resolve: B" << bid << " BPC:0x" << hex << uop->cmd->bpc;
        DoResolved(uop);
    }
}

void VectorLiteROB::DoResolved(const VectorLiteUopT& uop)
{
    auto bid = uop->cmd->bid;
    ASSERT(entries.count(bid) != 0);
    ASSERT(entries[bid].status != VROBStatus::COMPLETED);
    ASSERT(entries[bid].resolvedUops.size() < entries[bid].uopCount);
    uop->cycleInfo->completeCycle = GetSim()->getCycles();

    entries[bid].HandleResolve(uop);
    if (entries[bid].CheckComplete()) {
        top->ResolveTile(bid);
        entries[bid].status = VROBStatus::COMPLETED;
        for (auto& rslvUop : entries[bid].resolvedUops) {
            rslvUop->cycleInfo->retireCycle = GetSim()->getCycles();
            GetSim()->GetViewManager(uop->cmd->stid)->RecordMinst(rslvUop->GetPipeViewInfo());
        }
        SwimLogData logData;
        std::stringstream ss;
        ss << entries[bid].cmd->DumpFormalName() << " BPC:0x" << hex << entries[bid].cmd->bpc;
        logData.name = ss.str();
        logData.pid = CORE_TOP_MACHINE_ID;
        logData.tid = entries[bid].cmd->execMachineId;
        logData.sTime = entries[bid].dispatchCycle;
        logData.eventId = (entries[bid].cmd->eventId >= 0) ? entries[bid].cmd->eventId : GetSim()->GetEventId();
        logData.eTime = entries[bid].dispatchLastCycle;
        logData.hint = entries[bid].cmd->DumpSwimInfo();
        GetSim()->AddDuration(logData);
        GetSim()->AddDependency(bid.val, logData.eventId, entries[bid].cmd->stid);
    }
}

void VectorLiteROB::CheckCommit()
{
    for (auto &e : entries) {
        if (e.second.status == VROBStatus::COMPLETED) {
            if (!SendDrainData(e.second.cmd)) {
                break;
            }
            e.second.status = VROBStatus::RETIRED;
            LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
                << "ROB, block had executed: " << e.second.cmd->Dump();
        }
    }
}

bool VectorLiteROB::SendDrainData(const BlockCommandPtr &cmd) const
{
    // TODO: Check stall
    ScbDrainBus bus;
    bus.bid = cmd->bid;
    bus.tid = top->GetCoreId();
    drainScbDataQ->Write(bus);
    return true;
}

void VectorLiteROB::CheckRetired()
{
    while (!drainScbRespQ->Empty()) {
        ROBID bid = drainScbRespQ->Read().bid;
        BlockCommandPtr cmd = entries[bid].cmd;
        entries.erase(bid);
        LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
                << "ROB, vector temp block is resolved: " << cmd->Dump();
        if (top->config.stash_mode) {
            vecliteStashCompQ->Write(cmd);
        }
        top->m_stats.m_executeBlockNum++;
        cmd->cmdExecCompleted = true;
        vecliteBccWakeupQ->Write(cmd);
        GetSim()->core->bctrl->blockROB.completeBlock(bid, true, cmd->stid);
    }
}

void VectorLiteROB::Build()
{
    maxDepth = top->GetConfig().vrobDepth;
    SetSim(top->GetSim());
}

bool VectorLiteROB::CheckStall() const
{
    return (entries.size() + m_fetchRobInQ->GetRawWriteData().size()) >= maxDepth;
}

SimSys* VectorLiteROB::GetSim()
{
    return sim;
}

void VectorLiteROB::SetSim(SimSys *s)
{
    sim = s;
}

void VectorLiteROB::SetFlush(const FlushBus &bus)
{
    auto matchCmd = [&bus](BlockCommandPtr& cmd) -> bool {
        return LessEqual(bus.req.bid, cmd->bid);
    };
    vecliteStashCompQ->FlushIf(matchCmd);
    vecliteBccWakeupQ->FlushIf(matchCmd);

    for (auto it = entries.begin(); it != entries.end();) {
        if (LessEqual(bus.req.bid, it->first)) {
            it = entries.erase(it);
        } else {
            ++it;
        }
    }
}

bool VectorLiteROB::IsIdle()
{
    return entries.empty();
}

void VectorLiteROB::Stats() const {}

void VectorLiteROB::Dump()
{
    for (auto& entry : entries) {
        cout << "VROB Entry " << entry.first << endl;
        cout << "  Command: " << entry.second.cmd << endl;
        cout << "  Status: " << static_cast<uint32_t>(entry.second.status) << endl;
        cout << "  uopCount: " << entry.second.uopCount << endl;
        cout << "  resolvedUopsCnt: " << entry.second.resolvedUops.size() << endl;
    }
    fflush(stdout);
}

}
