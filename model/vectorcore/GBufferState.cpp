#include "vectorcore/GBufferState.h"

namespace JCore {

GBufferState::GBufferState() {}

GBufferState::~GBufferState() {}

void GBufferState::Build()
{
    // initialize the depth of each threads
    m_entries.resize(m_depth);

    m_readPtr.val = 0;
    m_writePtr.val = 0;
    m_count = 0;
}

bool GBufferState::IsEmpty() const
{
    return m_count == 0;
}

bool GBufferState::IsFull() const
{
    return m_count == m_depth;
}

bool GBufferState::CanDispatch()
{
    if (m_writePtr != m_readPtr) {
        ASSERT(m_count > 0);
    }
    return m_writePtr != m_readPtr;
}

GBufferEntry& GBufferState::Front()
{
    if (IsEmpty()) {
        ASSERT(0);
    }

    return m_entries[m_readPtr.val];
}

GBufferEntry& GBufferState::ReadyToDisp(uint32_t m_thdN)
{
    if (IsEmpty()) {
        ASSERT(0);
    }
    GBufferEntry &entry = m_entries[m_readPtr.val];
    entry.gid.val = entry.dispCnt * m_thdN + entry.offset;
    entry.pmuInfo.issueCycle = GetSim()->getCycles();
    ++m_entries[m_readPtr.val].dispCnt;
    return entry;
}

void GBufferState::Read()
{
    if (IsEmpty()) {
        ASSERT(0);
    }
    IncROBID(m_readPtr, m_depth);
    --m_count;
}

bool GBufferState::Write(const GBufferEntry &entry)
{
    if (IsFull()) {
        return false;
    }
    const uint32_t writeIdx = m_writePtr.val;
    m_entries[writeIdx] = entry;
    m_entries[writeIdx].tid = m_tid;
    m_entries[writeIdx].status = BlockStatus::ALLOCATED;
    m_entries[writeIdx].pmuInfo.createCycle = GetSim()->getCycles();

    IncROBID(m_writePtr, m_depth);
    m_count++;
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "alloc GBuffer entry, B" << entry.bid << "-T" << m_tid
        << ", writePtr:" << m_writePtr << ", size:" << m_count;
    return true;
}

void GBufferState::SetStatus(const ROBID &gid, BlockStatus status)
{
    uint32_t entryIdx = gid.val - m_baseGidVal;
    if (status == BlockStatus::COMPLETED) {
        m_entries[entryIdx].pmuInfo.completeCycle = GetSim()->getCycles() + 1;
    }
    m_entries[entryIdx].status = status;
}

void GBufferState::Reset()
{
    m_readPtr = ROBID();
    m_writePtr = ROBID();
    m_count = 0;
}

SimSys* GBufferState::GetSim()
{
    return m_sim;
}

void GBufferState::SetSim(SimSys *sim)
{
    m_sim = sim;
}

void GBufferState::UpdateReadPtr(ROBID &gid)
{
    ROBID id = gid;
    while (id != m_readPtr) {
        m_entries[id.val].status = BlockStatus::ALLOCATED;
        IncROBID(id, m_depth);
    }
    m_readPtr = gid;
}

GBufferEntry& GBufferState::GetEntry(ROBID &gid)
{
    uint32_t id = gid.val - m_baseGidVal;
    if (id < 0 || id >= m_depth) {
        LOG_ERROR << "[Vector GBuffer Manager]: Invalid gid " << gid << "in thread " << m_tid;
        abort();
    }
    return m_entries[gid.val];
}

void GBufferState::SetFlush(FlushBus &bus, uint32_t m_thdN)
{
    if (m_count == 0) {
        return;
    }

    GBufferState gstate(m_tid, m_depth, m_baseGidVal);
    gstate.Build();
    gstate.SetSim(m_sim);
    ROBID idx = m_readPtr;
    for (uint32_t i = 0; i < m_count; ++i) {
        if (m_entries[idx.val].vld &&
            (!LessEqual(bus.req.bid, m_entries[idx.val].bid) || bus.req.stid != m_entries[idx.val].blockCmd->stid)) {
            gstate.Write(m_entries[idx.val]);
        }
        IncROBID(idx, m_depth);
    }

    m_entries = std::move(gstate.m_entries);
    m_writePtr = gstate.GetWritePtr();
    m_readPtr = gstate.GetReadPtr();
}

} // namespace JCore
