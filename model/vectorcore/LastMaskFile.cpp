#include "vectorcore/LastMaskFile.h"
#include "core/Core.h"


namespace JCore {

LastMaskFile::LastMaskFile() {}

LastMaskFile::~LastMaskFile() {}

void LastMaskFile::Work()
{
    Release();
    Alloc();
}

void LastMaskFile::Build(uint32_t d, uint32_t fetchWidth)
{
    this->depth = d;
    this->fetchWidth = fetchWidth;
    m_entries.resize(GetSim()->core->configs.scalar_smt_thread);
    size.resize(GetSim()->core->configs.scalar_smt_thread);
}

void LastMaskFile::Alloc()
{
}

void LastMaskFile::Alloc(const ROBID &bid, uint32_t lastGroupNum, uint32_t stid)
{
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[Vector MaskFile Manager]: Alloc entry. Block " << bid
                                        << " last group " << lastGroupNum;
    ASSERT(m_entries[stid].count(bid) == 0);
    ASSERT(lastGroupNum > 0);
    LastMaskFileState info;
    info.vld = true;
    info.bid = bid;
    info.lastGroupNum = lastGroupNum;
    m_entries[stid][bid] = info;
    ++size[stid];
}

void LastMaskFile::Release()
{
    for (uint32_t i = 0; i < fetchWidth; ++i) {
        if (m_GROB2MaskFile->Empty()) {
            break;
        }

        MaskReleaseInfo info = m_GROB2MaskFile->Read();
        Release(info);
    }
}

void LastMaskFile::Release(MaskReleaseInfo &info)
{
    LOG_DEBUG_M(Unit::VECTOR, Stage::NA) << "[Vector MakeFile Manager]: Release bid " << info.bid;
    m_entries[info.stid].erase(info.bid);
    --size[info.stid];
}

bool LastMaskFile::CheckStall(uint32_t reserved, uint32_t stid)
{
    return size[stid] + reserved > depth;
}

uint32_t LastMaskFile::GetLastGroupNum(const ROBID &bid, uint32_t stid)
{
    ASSERT(m_entries[stid].count(bid) != 0);
    ASSERT(m_entries[stid][bid].lastGroupNum != 0);
    return m_entries[stid][bid].lastGroupNum;
}

SimSys* LastMaskFile::GetSim()
{
    return sim;
}

void LastMaskFile::SetSim(SimSys *sm)
{
    sim = sm;
}

void LastMaskFile::SetFlush(FlushBus &bus)
{
    if (!bus.baseOnBid) {
        return;
    }

    for (auto it = m_entries[bus.req.stid].cbegin(); it != m_entries[bus.req.stid].cend();) {
        if (LessEqual(bus.req.bid, it->first)) {
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[Vector MakeFile Manager]:  flush bid " << it->first;
            it = m_entries[bus.req.stid].erase(it);
            --size[bus.req.stid];
        } else {
            ++it;
        }
    }
}

} // namespace JCore
