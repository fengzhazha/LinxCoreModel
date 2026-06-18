#include "veclite/VectorLiteUOPIQ.h"
#include "veclite/VectorLite.h"
#include "core/Bus.h"
#include "core/Core.h"

namespace JCore {
using namespace std;

VectorLiteUOPIQ::IqPipe::IqPipe(VectorLiteUOPIQ* topVal,
    uint32_t pipeIdVal, uint32_t depthVal, uint32_t stallMinEntryNumVal)
    : top(topVal), pipeId(pipeIdVal), depth(depthVal), stallMinEntryNum(stallMinEntryNumVal)
{}

VectorLiteUOPIQ::IqPipe::~IqPipe()
{}

void VectorLiteUOPIQ::IqPipe::Work()
{
    for (auto& entry : entries) {
        if (entry.second->stage != IqStage::I) {
            continue;
        }
        if (entry.second->iCnt < entry.second->readRegLatency) {
            entry.second->iCnt += 1;
        }
        if (entry.second->iCnt >= entry.second->readRegLatency) {
            entry.second->stage = IqStage::S;
            entry.second->rdyCycle = top->GetSim()->getCycles();
        }
    }
}

bool VectorLiteUOPIQ::IqPipe::CheckStall(uint32_t pendingCnt) const
{
    // Stall when remaining capacity < N (i.e., entries > depth - N)
    // For depth=32 and N=4, stall when entries > 28
    ASSERT(depth > stallMinEntryNum);
    uint32_t stallThreshold = depth - stallMinEntryNum;
    return ((entries.size() + pendingCnt) > stallThreshold);
}

bool VectorLiteUOPIQ::IqPipe::Alloc(const VectorLiteUopT &uop)
{
    ASSERT(entries.size() < depth);
    entries.push_back(make_pair(uop, make_shared<StageInfo>(uop->readRegLatency)));
    return true;
}

deque<VectorLiteUOPIQ::IqEntry>& VectorLiteUOPIQ::IqPipe::Entries()
{
    return entries;
}

void VectorLiteUOPIQ::IqPipe::Flush(const FlushBus &bus)
{
    auto it = entries.cbegin();
    while (it != entries.cend()) {
        if (LessEqual(bus.req.bid, it->first->cmd->bid)) {
            it = entries.erase(it);
            continue;
        }
        ++it;
    }
}

VectorLiteUOPIQ::VectorLiteUOPIQ()
{}

VectorLiteUOPIQ::~VectorLiteUOPIQ() {}

void VectorLiteUOPIQ::Xfer() const {}

void VectorLiteUOPIQ::Work()
{
    ReceivedUInfo();
    for (auto& pipe : m_pipes) {
        pipe->Work();
    }
    Issue();
}

void VectorLiteUOPIQ::ReceivedUInfo()
{
    while (!m_siqUiqInQ->Empty()) {
        VectorLiteUopT uop = m_siqUiqInQ->Front();
        LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
            << "UIQ, receive: " << uop->cmd->Dump() << ", sliceIdx: " << dec << uop->idx;
        if (!AllocEntry(uop)) {
            break;
        }
        m_siqUiqInQ->Read();
    }
}

bool VectorLiteUOPIQ::AllocEntry(const VectorLiteUopT &uop)
{
    auto pipeType = VectorLiteUopInfo::Inst().GetPipe(uop->cmd->tileOp);
    ASSERT(m_pipeId.find(pipeType) != m_pipeId.cend());
    auto pipeId = m_pipeId.at(pipeType);
    ASSERT(pipeId < m_pipes.size());
    bool ret = m_pipes.at(pipeId)->Alloc(uop);
    LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
        << "UIQ, alloc: " << uop->cmd->Dump() << ", sliceIdx: " << dec << uop->idx;
    uop->cycleInfo->i1Cycle = GetSim()->getCycles();
    ASSERT(ret);
    return ret;
}

void VectorLiteUOPIQ::Issue()
{
    uint64_t currentCycle = GetSim()->getCycles();
    for (auto& pipe : m_pipes) {
        auto& entries = pipe->Entries();
        auto it = entries.cbegin();
        while (it != entries.cend()) {
            if (it->second->stage != VectorLiteUOPIQ::IqStage::S) {
                ++it;
                continue;
            }
            auto uop = it->first;
            auto tileOp = uop->cmd->tileOp;
            bool canIssue = true;
            stringstream blockStr;
            // TCOLSUM指令需要Scoreboard检查
            if (!top->config.perfectColSum && tileOp == TileOp::TCOLSUM) {
                // 检查是否满足2周期间隔
                uint32_t interval = currentCycle - m_lastTColSumIssueCycle;
                if ((m_lastTColSumIssueCycle == 0) || (interval >= m_minTColSumIssueInterval)) {
                    // 可以发射
                    m_lastTColSumIssueCycle = currentCycle;
                    m_tcolsumPending = false;
                } else {
                    // 不能发射，保持指令在队列中
                    canIssue = false;
                    // 标记有待发射的TCOLSUM
                    m_tcolsumPending = true;
                    blockStr << ", need " << dec << (m_minTColSumIssueInterval - interval) << " more cycles"
                        << ", last issued: " << m_lastTColSumIssueCycle;
                }
            }
            if (canIssue) {
                if (it->second->rdyCycle < currentCycle) {
                    uop->cycleInfo->s1Cycle = it->second->rdyCycle + 1;
                }
                top->core->vectorRealReadRFReqCnt += uop->cmd->srcs.size();
                m_uiqEXEInQ->Write(uop);
                (void)entries.erase(it);
                LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
                    << "UIQ, issue: " << uop->cmd->Dump() << ", tileOp: " << GetTileOpName(tileOp)
                    << ", sliceIdx: " << dec << uop->idx;
            } else {
                LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
                    << "UIQ, blocked: " << uop->cmd->Dump() << ", tileOp: " << GetTileOpName(tileOp)
                    << ", sliceIdx: " << dec << uop->idx
                    << blockStr.str();
            }
            break;
        }
    }
}

void VectorLiteUOPIQ::Build()
{
    SetSim(top->GetSim());

    // update configs
    m_iqDepth = top->config.uiqDepth;
    m_pipeNum = top->config.uiqPipeNum;
    ASSERT(m_iqDepth > 0);
    ASSERT(m_pipeNum > 0);
    m_pipeId.emplace(VectorLiteUopInfo::ExePipe::FMLA, 0);
    m_pipeId.emplace(VectorLiteUopInfo::ExePipe::IALU, 0);
    m_pipeId.emplace(VectorLiteUopInfo::ExePipe::FDIV, 0);
    m_pipeId.emplace(VectorLiteUopInfo::ExePipe::PERMUTE, 0);
    m_pipeId.emplace(VectorLiteUopInfo::ExePipe::IMAC, 0);
    m_pipeId.emplace(VectorLiteUopInfo::ExePipe::FCVT, 0);
    m_pipes.reserve(m_pipeNum);
    for (uint32_t pipeId = 0; pipeId < m_pipeNum; pipeId++) {
        m_pipes.push_back(make_shared<IqPipe>(this, pipeId, m_iqDepth, top->config.uiqAllocMinEntryNum));
    }
}

bool VectorLiteUOPIQ::CheckStall(const VectorLiteUopT &uop) const
{
    auto pipeType = VectorLiteUopInfo::Inst().GetPipe(uop->tileOp);
    ASSERT(m_pipeId.find(pipeType) != m_pipeId.cend());
    auto pipeId = m_pipeId.at(pipeType);
    ASSERT(pipeId < m_pipes.size());

    auto getQueuePendingCnt = [pipeId, this](const deque<VectorLiteUopT>& que) {
        return count_if(que.cbegin(), que.cend(), [pipeId, this](const VectorLiteUopT& qUop) {
            auto qPipeType = VectorLiteUopInfo::Inst().GetPipe(qUop->tileOp);
            ASSERT(m_pipeId.find(qPipeType) != m_pipeId.cend());
            auto qPipeId = m_pipeId.at(qPipeType);
            ASSERT(qPipeId < m_pipes.size());
            return qPipeId == pipeId;
        });
    };

    uint32_t pendingCnt = getQueuePendingCnt(m_siqUiqInQ->GetRawWriteData());
    return m_pipes.at(pipeId)->CheckStall(pendingCnt);
}

SimSys* VectorLiteUOPIQ::GetSim()
{
    return m_sim;
}

void VectorLiteUOPIQ::SetSim(SimSys *sim)
{
    m_sim = sim;
}

void VectorLiteUOPIQ::SetFlush(const FlushBus &bus)
{
    LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
        << "UIQ, flush: " << bus;
    auto matchCmd = [&bus](const VectorLiteUopT& uop) -> bool {
        return LessEqual(bus.req.bid, uop->cmd->bid);
    };
    m_siqUiqInQ->FlushIf(matchCmd);
    for (auto& pipe : m_pipes) {
        pipe->Flush(bus);
    }
}

bool VectorLiteUOPIQ::IsIdle()
{
    for (auto& pipe : m_pipes) {
        if (!pipe->Entries().empty()) {
            return false;
        }
    }
    return true;
}

void VectorLiteUOPIQ::Stats() const {}

}
