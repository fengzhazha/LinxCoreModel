#include "veclite/VectorLiteExE.h"
#include "veclite/VectorLite.h"
#include "veclite/VectorLiteUopInfo.h"
#include "core/Bus.h"

namespace JCore {
using namespace std;

VectorLiteExE::VectorLiteExE()
{}

VectorLiteExE::~VectorLiteExE()
{}

void VectorLiteExE::Work()
{
    ReceiveUOP();
    Execute();
}

void VectorLiteExE::Xfer() const {}

void VectorLiteExE::ReceiveUOP()
{
    while (!m_uiqEXEInQ->Empty()) {
        VectorLiteUopT uop = m_uiqEXEInQ->Read();
        uop->latency = VectorLiteUopInfo::Inst().GetLatency(uop->tileOp);
        ASSERT(uop->latency <= m_exeMaxLatency);
        m_entries.emplace_back(make_pair(uop, make_shared<StageInfo>(uop->latency)));
        LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
            << "EXE, receive: " << uop->cmd->Dump() << ", uopIdx: " << dec << uop->idx;
    }
}

bool VectorLiteExE::Execute()
{
    // TODO: exe by tileop
    vector<function<void(const ExeEntry&)>> exeFunc;
    auto it = m_entries.cbegin();
    while (it != m_entries.cend()) {
        if (it->second->stage == ExeStage::RESOLVED) {
            // remove retired entries
            it = m_entries.erase(it);
            continue;
        }
        switch (it->second->stage) {
            case ExeStage::EX:
                exeFunc.emplace_back(bind(&VectorLiteExE::RunEX, this, placeholders::_1));
                break;
            case ExeStage::W1:
                exeFunc.emplace_back(bind(&VectorLiteExE::RunW1, this, placeholders::_1));
                break;
            case ExeStage::W2:
                exeFunc.emplace_back(bind(&VectorLiteExE::RunW2, this, placeholders::_1));
                break;
            default:
                ASSERT(false);
        }
        ++it;
    }
    ASSERT(exeFunc.size() == m_entries.size());
    for (size_t idx = 0; idx < m_entries.size(); idx++) {
        exeFunc[idx](m_entries[idx]);
    }
    return true;
}

void VectorLiteExE::Resolve(const VectorLiteUopT &uop) const
{
    LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
        << "EXE, resolve: " << uop->cmd << ", uopIdx: " << dec << uop->idx;
    top->m_stats.m_executeUopNum++;
    if (top->config.tileOpFusion) {
        if (!(uop->cmd->tileOp == TileOp::TCOLEXPANDMUL) && !(uop->cmd->tileOp == TileOp::TCOLEXPANDSUB)) {
            top->ResolveUop(uop->cmd->bid);
        }
    } else {
        top->ResolveUop(uop->cmd->bid);
    }
    m_exeRobRslvQ->Write(uop);
}

void VectorLiteExE::Build()
{
    ASSERT(top != nullptr);
    SetSim(top->GetSim());
    m_exeMaxLatency = top->config.exeMaxLatency;
}

bool VectorLiteExE::CheckStall(TileOp tileOp) const
{
    (void)tileOp;
    return false;
}

SimSys* VectorLiteExE::GetSim() const
{
    return m_sim;
}

void VectorLiteExE::SetSim(SimSys* sim)
{
    ASSERT(sim != nullptr);
    m_sim = sim;
}

void VectorLiteExE::SetFlush(const FlushBus& bus)
{
    LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
        << "EXE, flush: " << bus;
    auto matchCmd = [&bus](const VectorLiteUopT& uop) -> bool {
        return LessEqual(bus.req.bid, uop->cmd->bid);
    };
    m_uiqEXEInQ->FlushIf(matchCmd);
    auto it = m_entries.cbegin();
    while (it != m_entries.cend()) {
        if (LessEqual(bus.req.bid, it->first->cmd->bid)) {
            it = m_entries.erase(it);
            continue;
        }
        ++it;
    }
}

bool VectorLiteExE::IsIdle()
{
    return (m_entries.size() == 0);
}

void VectorLiteExE::Stats() const {}

void VectorLiteExE::RunEX(const ExeEntry& entry) const
{
    ASSERT(entry.second->latency > 0);
    entry.second->exStage += 1;
    entry.second->latency -= 1;

    LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
        << "EXE, E" << dec << entry.second->exStage << ": " << entry.first->cmd->Dump()
        << " latency " << entry.second->latency;

    const map<uint32_t, uint64_t&> stageCycleMap = {
        { 1U, entry.first->cycleInfo->e1Cycle },
        { 2U, entry.first->cycleInfo->e2Cycle },
        { 3U, entry.first->cycleInfo->e3Cycle },
        { 4U, entry.first->cycleInfo->e4Cycle },
        { 5U, entry.first->cycleInfo->e5Cycle },
    };
    if (stageCycleMap.find(entry.second->exStage) != stageCycleMap.cend()) {
        stageCycleMap.at(entry.second->exStage) = GetSim()->getCycles();
    }

    if (entry.second->latency == 1U) {
        entry.second->stage = ExeStage::W1;
    }
}

void VectorLiteExE::RunW1(const ExeEntry& entry) const
{
    LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
        << "EXE, W1: " << entry.first->cmd->Dump();
    entry.first->cycleInfo->w1Cycle = GetSim()->getCycles();
    entry.second->stage = ExeStage::W2;
}

void VectorLiteExE::RunW2(const ExeEntry& entry) const
{
    LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
        << "EXE, W2: " << entry.first->cmd->Dump();
    ASSERT(m_exeRobRslvQ != nullptr);
    Resolve(entry.first);
    entry.first->cycleInfo->w2Cycle = GetSim()->getCycles();
    entry.second->stage = ExeStage::RESOLVED;
}

} // namespace JCore
