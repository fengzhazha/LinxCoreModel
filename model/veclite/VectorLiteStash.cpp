#include "veclite/VectorLiteStash.h"
#include "veclite/VectorLite.h"
#include "core/Bus.h"

namespace JCore {

VectorLiteStash::VectorLiteStash() {}
VectorLiteStash::~VectorLiteStash() {}

void VectorLiteStash::Build()
{
    SetSim(top->GetSim());
    m_stashReqQueDepth = top->config.stashReqQueDepth;
}

bool VectorLiteStash::CheckStall() const
{
    return (m_fetchStashInQ->GetRawWriteData().size()) > m_stashReqQueDepth;
}

SimSys* VectorLiteStash::GetSim()
{
    return sim;
}

void VectorLiteStash::SetSim(SimSys *s)
{
    sim = s;
}

void VectorLiteStash::Xfer() const {}

void VectorLiteStash::Work()
{
    Issue();
    CheckStashDone();
    CheckStashAllocDone();
    ReceivedStash();
    SendReadReq();
}

void VectorLiteStash::GenReadReq(BlockCommandPtr& cmd)
{
    if (!top->GetConfig().tileBufferEnable) {
        return;
    }
    if (cmd->srcs.empty()) {
        return;
    }

    for (auto& src : cmd->srcs) {
        uint64_t base = src->baseAddr;
        uint32_t totalCnt = src->size / MAX_TILE_DATA_BYTE;

        for (uint32_t i = 0; i < totalCnt; ++i) {
            uint64_t addr = base + i * MAX_TILE_DATA_BYTE;

            pendingReadRegQueue.emplace_back(
                cmd->bid,           // bid
                MAX_TILE_DATA_BYTE, // size
                i,                  // srcIndex
                src->tileTag,       // tag_id
                src->handType,
                cmd->stid,
                addr);
        }

        bool ret = top->AllocSrcCache(src->tileTag, src->handType, src->bid, src->size, src->baseAddr);
        ASSERT(ret); // TODO
    }
}

void VectorLiteStash::SendReadReq()
{
    if (!top->GetConfig().tileBufferEnable) {
        return;
    }
    // 非ring mode, 不发送读数据请求
    if (top->pTileUtils->IsRingOrXbarMode(false)) {
        return;
    }

    uint32_t maxReqPerCycle = top->GetConfig().vec_nread;
    if (maxReqPerCycle == 0) {
        maxReqPerCycle = 4;  // 默认值
    }

    uint32_t sentCount = 0;
    uint32_t readPos = 0;
    while (readPos < pendingReadRegQueue.size() && sentCount < maxReqPerCycle) {
        const TileSrcRequest& req = pendingReadRegQueue[readPos];

        // 创建并发送读请求
        auto ldReq = VecTileRegLdReq();
        ldReq.SetStid(req.stid);
        ldReq.SetReqId(top->tileReqId++);
        ldReq.SetSize(req.size);
        ldReq.SetSrc(req.addr);
        ldReq.SetTagId(req.tagId, req.handType);

        // 写入请求队列
        top->m_vecTileRegLdReqQ->Write(ldReq);

        sentCount++;
    }
    if (readPos > 0) {
        pendingReadRegQueue.erase(pendingReadRegQueue.begin(),
                                  pendingReadRegQueue.begin() + readPos);
    }
}

void VectorLiteStash::ReceivedStash()
{
    while (!m_fetchStashInQ->Empty()) {
        BlockCommandPtr cmd = m_fetchStashInQ->Front();
        LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
            << "Stash, receive: " << cmd->Dump();
        if (top->config.stash_mode) {
            if (!SendStashReq(cmd)) {
                break;
            }
        } else {
            GenReadReq(cmd);
            StashWakeup(cmd->bid);
        }
        m_fetchStashInQ->Read();
    }
}

bool VectorLiteStash::SendStashReq(const BlockCommandPtr &bcmd)
{
    cmdMonitor.push_back(bcmd);
    // TODO: Check for stall
    vecliteStashCmdQ->Write(bcmd);
    return true;
}

void VectorLiteStash::CheckStashAllocDone()
{
    while (!stashAllocDoneQ->Empty()) {
        BlockCommandPtr cmd = stashAllocDoneQ->Read();
        LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "]"
            << " Stash, way id return and set " << cmd->Dump();
        HandleWayId(cmd);
    }
}

static void SetWayId(TileOperandPtr &operand, BlockCommandPtr &cmd)
{
    for (auto &src : cmd->srcs) {
        if (src->vld && src->handType == operand->handType && src->tileTag == operand->tileTag) {
            src->wayId = operand->wayId;
            src->wayAllocated = true;
        }
    }
    for (auto &dst : cmd->dsts) {
        if (dst->vld && dst->handType == operand->handType && dst->tileTag == operand->tileTag) {
            dst->wayId = operand->wayId;
            dst->stashReady = true;
            dst->wayAllocated = true;
        }
    }
}

void VectorLiteStash::HandleWayId(const BlockCommandPtr &bcmd)
{
    for (auto &cmd : cmdMonitor) {
        if (bcmd->bid != cmd->bid) {
            continue;
        }
        if (!bcmd->srcs.empty()) {
            SetWayId(bcmd->srcs[0], cmd);
        }
        if (!bcmd->dsts.empty()) {
            SetWayId(bcmd->dsts[0], cmd);
        }
    }
}

void VectorLiteStash::CheckStashDone()
{
    while (!stashRslvQ->Empty()) {
        BlockCommandPtr cmd = stashRslvQ->Read();
        LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
            << "Stash, stash resolve return " << cmd->Dump() << " wakeup way: " << cmd->dsts[0]->wayId;
        HandleStashResolve(cmd);
    }
}

static bool CheckStashMatch(TileOperandPtr &src, TileOperandPtr &operand)
{
    return src->vld && src->handType == operand->handType && src->wayId == operand->wayId
            && src->tileTag == operand->tileTag && src->wayAllocated;
}

void VectorLiteStash::HandleStashResolve(const BlockCommandPtr &bcmd)
{
    for (auto &cmd : cmdMonitor) {
        for (auto &src : cmd->srcs) {
            if (CheckStashMatch(src, bcmd->dsts[0])) {
                src->stashReady = true;
            }
        }
    }
}

void VectorLiteStash::Issue()
{
    for (auto it = cmdMonitor.cbegin(); it != cmdMonitor.cend();) {
        BlockCommandPtr cmd = *it;
        if (cmd->StashReady(!cmd->dsts.empty())) {
            StashWakeup((*it)->bid);
            LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
                << "Stash, issue " << cmd->Dump();
            it = cmdMonitor.erase(it);
        } else {
            ++it;
        }
    }
}

void VectorLiteStash::StashWakeup(const ROBID &bid) const
{
    m_stashSiqWakeuQ->Write(bid);
}

void VectorLiteStash::SetFlush(const FlushBus &bus) const
{
    auto matchCmd = [&bus](BlockCommandPtr& cmd) -> bool {
        return LessEqual(bus.req.bid, cmd->bid);
    };
    vecliteStashCmdQ->FlushIf(matchCmd);
    stashAllocDoneQ->FlushIf(matchCmd);
    stashRslvQ->FlushIf(matchCmd);
    m_fetchStashInQ->FlushIf(matchCmd);
}

bool VectorLiteStash::IsIdle() const
{
    return false;
}

void VectorLiteStash::Stats() const {}

}
