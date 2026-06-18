#include "veclite/VectorLiteScheduleIQ.h"
#include "veclite/VectorLite.h"
#include "core/Bus.h"
#include "plat/DebugLog.h"

namespace JCore {

void SIQInfo::SetReady(bool rdy)
{
    ready = rdy;
}

bool SIQInfo::CheckReady() const
{
    return ready;
}

std::string SIQInfo::Dump() const
{
    std::stringstream oss;
    oss << "SIQInfo: uopCount=" << uopCount << " (512B each)";
    return oss.str();
}

VectorLiteScheduleIQ::VectorLiteScheduleIQ() {}
VectorLiteScheduleIQ::~VectorLiteScheduleIQ() {}

void VectorLiteScheduleIQ::Xfer()
{
    Issue();
    Wakeup();
    dispatchBlock = ROBID();
    dispatchBlockValid = false;
}

void VectorLiteScheduleIQ::Work()
{
    ReceivedCmd();
}

void VectorLiteScheduleIQ::ReceivedCmd()
{
    while (!m_fetchSiqInQ->Empty()) {
        BlockCommandPtr cmd = m_fetchSiqInQ->Front();
        LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
            << "SIQ, receive: " << cmd->Dump();
        if (!AllocEntry(cmd)) {
            break;
        }
        m_fetchSiqInQ->Read();
    }
}

bool VectorLiteScheduleIQ::AllocEntry(const BlockCommandPtr &cmd)
{
    // Check for stall - if entries queue is full, cannot allocate
    if (entries.size() >= m_siqDepth) {
        LOG_DEBUG_M(Unit::VECLITE, Stage::NA)
            << "SIQ stalled: entries full (" << entries.size() << "/" << m_siqDepth << ")";
        return false;
    }

    // Create entry but don't split UOPs yet
    // Will split when wakeup notification arrives from VectorLiteStash
    auto entry = std::make_shared<SIQInfo>(cmd);
    entry->uopCount = CalUOPCount(cmd);
    entry->siqCycle = GetSim()->getCycles();
    entries.push_back(entry);

    totalTileOPsReceived++;

    LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
        << "SIQ, allocated entry for TileOP: " << GetTileOpName(cmd->tileOp)
        << " expecting " << entry->uopCount << " UOPs (512B each)"
        << " entries: " << entries.size() << "/" << m_siqDepth
        << " " << cmd->Dump();

    return true;
}

uint32_t VectorLiteScheduleIQ::CalUOPCount(const BlockCommandPtr &cmd) const
{
    if (!cmd || cmd->dsts.empty()) {
        return 1;
    }

    uint64_t lb0 = cmd->lb0;
    uint64_t lb1 = cmd->lb1;
    uint64_t lb2 = cmd->lb2;
    uint64_t threadCount = lb0 * lb1;
    uint64_t grpCount;
    if (!top->config.perfectReduceOp && cmd->tileOp == TileOp::TCOLSUM) {
        grpCount = (threadCount + 127) / lb2 - 1;
    } else if (!top->config.perfectReduceOp && cmd->tileOp == TileOp::TCOLMAX) {
        grpCount = (threadCount + 127) / 128 - (lb2 / 128);
    } else if (!top->config.expFlopsDouble && cmd->tileOp == TileOp::TEXP) {
        grpCount = (threadCount + 63) / 64;
    } else {
        grpCount = (threadCount + 127) / 128;
    }
    return grpCount;
}

void VectorLiteScheduleIQ::Wakeup()
{
    while (!m_stashSiqWakeuQ->Empty()) {
        ROBID bid = m_stashSiqWakeuQ->Read();
        WakeupEntry(bid);
    }
}

void VectorLiteScheduleIQ::WakeupEntry(ROBID &bid)
{
    for (auto e : entries) {
        if (e->cmd->bid == bid) {
            ASSERT(!e->CheckReady());
            e->SetReady(true);
            LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
                << "SIQ, entry woken up, Block " << e->cmd->Dump();
        }
    }
}

void VectorLiteScheduleIQ::Issue()
{
    m_issueLimit = top->config.siqToUiqIssueLimit;
    ASSERT(m_issueLimit > 0);
    for (auto it = entries.begin(); it != entries.end();) {
        auto& entry = *it;
        if (m_issueLimit == 0) {
            break;
        }
        IssueEntry(entry);
        if (entry->state == SIQEntryState::DONE) {
            it = entries.erase(it);
            continue;
        }
        ++it;
    }
}

void VectorLiteScheduleIQ::IssueEntryStateIdle(const SIQInfoPtr &entry) const
{
    if (entry->uopCount == 0) {
        LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
            << "SIQ, done, uopCount is zero, Block " << entry->cmd->Dump();
        entry->state = SIQEntryState::DONE;
        ASSERT(false); // TBC: need to send to ROB or not
        return;
    }
    auto cmd = entry->cmd;
    // Generate UOPs and fill into the existing entry
    // Reserve space for UOPs
    LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
        << "SIQ, splitting TileOP " << GetTileOpName(cmd->tileOp)
        << " into " << entry->uopCount << " UOPs (512B each), Block " << entry->cmd->Dump();
    // update read reg latency
    entry->readRegLatency = count_if(cmd->srcs.cbegin(), cmd->srcs.cend(), [](const TileOperandPtr& src) {
        return src->vld;
    });
    if (entry->readRegLatency == 0) {
        // TEXPANDS without src needs read
        entry->readRegLatency = 1;
    }
    if (top->config.tileOpFusion && (cmd->tileOp == TileOp::TCOLEXPANDMUL || cmd->tileOp == TileOp::TCOLEXPANDSUB)) {
        entry->readRegLatency += 1;
    }
    // Generate UOPs for each 512B slice
    entry->uops.reserve(entry->uopCount);
    for (uint32_t i = 0; i < entry->uopCount; i++) {
        uint64_t offset = i * UOP_GRANULARITY;
        auto uop = GenerateUOP(entry, i, offset);
        if (uop) {
            entry->uops.push_back(uop);
        }
    }
    LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
        << "SIQ, dispatch, Block " << entry->cmd->Dump();

    entry->state = SIQEntryState::DISPATCH;
}

void VectorLiteScheduleIQ::IssueEntryStateDispatch(const SIQInfoPtr &entry)
{
    auto cmd = entry->cmd;
    uint32_t readPortNum = entry->uops.size();
    if (readPortNum > top->config.siqToUiqIssueLimit) {
        readPortNum = top->config.siqToUiqIssueLimit;
    }
    if (dispatchBlockValid && (dispatchBlock.val != cmd->bid.val)) {
        return;
    }
    bool tileOpFusion = top->config.tileOpFusion &&
        (cmd->tileOp == TileOp::TCOLEXPANDMUL || cmd->tileOp == TileOp::TCOLEXPANDSUB);
    if (!top->HasReadPort(tileOpFusion, cmd->bid, readPortNum, entry->readRegLatency)) {
        return;
    }
    dispatchBlockValid = true;
    if (entry->uopCount == entry->uops.size()) {
        m_siqRobDispQ->Write(entry);
        dispatchBlock = entry->cmd->bid;
    }
    LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
        << "SIQ, wake up, acquired tile reg read port: " << entry->readRegLatency << ", Block "
        << entry->cmd->Dump();

    uint32_t issueCnt = 0;
    ASSERT(m_uiqStallFn);
    auto it = entry->uops.cbegin();
    while (it != entry->uops.cend()) {
        if (m_issueLimit == 0) {
            break;
        }
        auto& uop = *it;
        if ((issueCnt == 0) && m_uiqStallFn(uop)) {
            // initial check: all uiq (4) entires must be available
            top->m_stats.m_uiqStallCycle += 1;
            break;
        }
        m_siqUiqInQ->Write(uop);
        issueCnt++;
        m_issueLimit--;
        it = entry->uops.erase(it);
        continue;
    }
    if (issueCnt > 0) {
        LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
            << "SIQ, issued TileOP: " << GetTileOpName(entry->cmd->tileOp)
            << " with " << issueCnt << " UOPs (512B each), Block " << entry->cmd->Dump();
    }
    if (entry->uops.empty()) {
        m_siqRobDispLastQ->Write(entry);
        LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << top->GetCoreId() << "] "
            << "SIQ, done, Block " << entry->cmd->Dump();
        entry->state = SIQEntryState::DONE;
    }
}

void VectorLiteScheduleIQ::IssueEntryStateDone(const SIQInfoPtr &entry) const
{
    (void)entry;
    ASSERT(false);
}

void VectorLiteScheduleIQ::IssueEntry(const SIQInfoPtr &entry)
{
    switch (entry->state) {
        case SIQEntryState::IDLE:
            IssueEntryStateIdle(entry);
            break;
        case SIQEntryState::DISPATCH:
            IssueEntryStateDispatch(entry);
            break;
        case SIQEntryState::DONE:
            IssueEntryStateDone(entry);
            break;
        default:
            ASSERT(false);
            break;
    }
}

uint32_t VectorLiteScheduleIQ::CalculateSliceCount(uint64_t totalSize) const
{
    if (totalSize == 0) {
        return 0;
    }

    // Ceiling division to handle partial slices
    // All UOPs are 512B granularity
    uint32_t numSlices = (totalSize + UOP_GRANULARITY - 1) / UOP_GRANULARITY;

    // Ensure at least 1 slice
    return std::max(1u, numSlices);
}

VectorLiteUopT VectorLiteScheduleIQ::GenerateUOP(
    const SIQInfoPtr &entry,
    uint32_t sliceIdx,
    uint64_t offset) const
{
    auto cmd = entry->cmd;
    if (!cmd) {
        return nullptr;
    }
    uint32_t totalSlices = entry->uopCount;

    // Create UOP with fixed 512B size
    auto uop = std::make_shared<VectorLiteUop>(sliceIdx, cmd, cmd->tileOp);
    uop->offset = offset;
    uop->sliceIdx = sliceIdx;
    uop->totalSlices = totalSlices;
    uop->readRegLatency = entry->readRegLatency;
    ASSERT(uop->readRegLatency > 0);
    // size is already set to UOP_GRANULARITY (512B) in constructor
    uop->cycleInfo->f0Cycle = entry->cmd->reqCycle;
    uop->cycleInfo->p1Cycle = entry->siqCycle;

    LOG_DEBUG_M(Unit::VECLITE, Stage::NA)
        << "Generated UOP: " << uop->Dump();

    return uop;
}

void VectorLiteScheduleIQ::Build()
{
    SetSim(top->GetSim());
    m_siqDepth = top->config.siqDepth;
}

bool VectorLiteScheduleIQ::CheckStall() const
{
    return (entries.size() + m_fetchSiqInQ->GetRawWriteData().size()) >= m_siqDepth;
}

SimSys* VectorLiteScheduleIQ::GetSim() const
{
    return sim;
}

void VectorLiteScheduleIQ::SetSim(SimSys *s)
{
    sim = s;
}

void VectorLiteScheduleIQ::SetFlush(const FlushBus &bus)
{
    for (auto it = entries.begin(); it != entries.end();) {
        if (LessEqual(bus.req.bid, (*it)->cmd->bid)) {
            it = entries.erase(it);
            continue;
        }
        ++it;
    }
}

bool VectorLiteScheduleIQ::IsIdle() const
{
    return m_fetchSiqInQ->Empty();
}

void VectorLiteScheduleIQ::Stats() const
{
    // TODO: Add statistics reporting if needed
}

} // namespace JCore
