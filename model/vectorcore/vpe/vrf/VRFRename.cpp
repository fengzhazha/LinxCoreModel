#include "VRFRename.h"
#include <set>
#include "core/Core.h"
#include "vectorcore/vpe/VecPEDecode.h"
#include "pe/PEBase.h"
#include "vectorcore/VectorCore.h"

namespace JCore {

VRFRename::VRFRename() {}

VRFRename::~VRFRename()
{
    if (freeSize != freeList.size()) {
        LOG_ERROR << "Error: Not all VRF has been freed. VRF Number: " << freeList.size() << ", freeSize: " << freeSize;
        for (uint32_t tag = 0; tag < freeList.size(); tag++) {
            LOG_ERROR << "tag=" << tag << ", free=" << static_cast<uint32_t>(freeList[tag]);
        }
    }
}

void VRFRename::Build(uint32_t threadNum, uint32_t mapSize, uint32_t pSize)
{
    uint32_t scalarCnt = sim->core->configs.scalar_smt_thread;
    mapQ = vector<vector<vector<VRFMapEntry>>>(threadNum,
           vector<vector<VRFMapEntry>>(scalarCnt,
           vector<VRFMapEntry>(mapSize, VRFMapEntry())));
    mapDeallocPtr = vector<vector<uint32_t>>(threadNum, vector<uint32_t>(scalarCnt, 0));
    mapAllocPtr = vector<vector<uint32_t>>(threadNum, vector<uint32_t>(scalarCnt, 0));
    mapFreeSize = vector<vector<uint32_t>>(threadNum, vector<uint32_t>(scalarCnt, mapSize));
    freeList.assign(pSize, true);
    recycleList.assign(pSize, false);
    regUsingCnt.assign(pSize, 0);
    regReadCnt.assign(pSize, 0);
    pmu = vector<VRFPmuEntry>(pSize, VRFPmuEntry());
    freeSize = pSize;

    smap = vector<vector<vector<vector<VRFMapEntry>>>>(threadNum,
           vector<vector<vector<VRFMapEntry>>>(scalarCnt,
           vector<vector<VRFMapEntry>>(HAND_NUM,
           vector<VRFMapEntry>(VRF_RELATIVE_DISTACE, VRFMapEntry()))));
    smapPtr = vector<vector<vector<uint32_t>>>(threadNum,
              vector<vector<uint32_t>>(scalarCnt,
              vector<uint32_t>(HAND_NUM, 0)));
    cmap = vector<vector<vector<vector<VRFMapEntry>>>>(threadNum,
           vector<vector<vector<VRFMapEntry>>>(scalarCnt,
           vector<vector<VRFMapEntry>>(HAND_NUM,
           vector<VRFMapEntry>(VRF_RELATIVE_DISTACE, VRFMapEntry()))));
    cmapPtr = vector<vector<vector<uint32_t>>>(threadNum,
              vector<vector<uint32_t>>(scalarCnt,
              vector<uint32_t>(HAND_NUM, 0)));
}

bool VRFRename::Stall(const SimInst& inst, uint32_t tid, uint32_t stid, uint32_t size)
{
    bool pair = size > MAX_REG_SIZE;
    if (!pair) {
        if (mapFreeSize[tid][stid] == 0) {
            return true;
        }
        if (freeSize > 0) {
            return false;
        }
        if (GetSim()->core->configs.vrf_reuse_src_kill_reg > 0) {
            for (auto& psrc : inst->psrcs_) {
                if (SrcPtagReusable(tid, stid, psrc, false)) {
                    // can reuse src.kill
                    return false;
                }
            }
        }
        return true;
    }
    // pair
    if (mapFreeSize[tid][stid] == 0) {
        return true;
    }
    if (freeSize > 1) {
        for (uint32_t i = 0; i < freeList.size() - 1; ++i) {
            if (freeList[i] && freeList[i + 1]) {
                return false;
            }
        }
    }
    if (GetSim()->core->configs.vrf_reuse_src_kill_reg > 0) {
        for (auto& psrc : inst->psrcs_) {
            if (SrcPtagReusable(tid, stid, psrc, true)) {
                // can reuse src.kill
                return false;
            }
        }
    }
    return true;
}

bool VRFRename::MultiStall(const SimInst& inst, const std::vector<uint8_t>& vrfReqNum)
{
    vector<bool> freeListCopy = freeList;
    uint32_t freeSizeCopy = freeSize;
    uint32_t mapFreeSizeCopy = mapFreeSize[inst->tid][inst->stid];
    auto allocateSingle = [&freeListCopy, &freeSizeCopy]() {
        if (freeSizeCopy == 0) {
            return false;
        }
        for (uint32_t i = 0; i < freeListCopy.size(); ++i) {
            if (freeListCopy[i]) {
                freeListCopy[i] = false;
                --freeSizeCopy;
                return true;
            }
        }
        ASSERT(false);
        return false;
    };
    auto allocatePair = [&freeListCopy, &freeSizeCopy]() {
        const uint32_t regNum = 2;
        if (freeSizeCopy < regNum) {
            return false;
        }
        for (uint32_t i = 0; i < freeListCopy.size() - 1; ++i) {
            if (freeListCopy[i] && freeListCopy[i + 1]) {
                freeListCopy[i] = false;
                freeListCopy[i + 1] = false;
                freeSizeCopy -= regNum;
                return true;
            }
        }
        return false;
    };
    set<POperandPtr> recycledSrc;
    auto reuseSrc = [this, &inst, &recycledSrc](bool pair) {
        for (auto& psrc : inst->psrcs_) {
            if (recycledSrc.find(psrc) == recycledSrc.cend() && SrcPtagReusable(inst->tid, inst->stid, psrc, pair)) {
                recycledSrc.insert(psrc);
                return true;
            }
        }
        return false;
    };
    for (auto& vrfNum : vrfReqNum) {
        if (mapFreeSizeCopy == 0) {
            return true;
        }
        bool pair = (vrfNum > 1U);
        bool canReuse = false;
        if (GetSim()->core->configs.vrf_reuse_src_kill_reg > 0) {
            canReuse = reuseSrc(pair);
        }
        if (!canReuse) {
            bool canAllocate = pair ? allocatePair() : allocateSingle();
            if (!canAllocate) {
                return true;
            }
        }
        --mapFreeSizeCopy;
    }
    return false;
}

bool VRFRename::MapQStall(uint32_t tid, uint32_t stid, uint32_t cnt)
{
    return mapFreeSize[tid][stid] < cnt;
}

VRFMapEntry VRFRename::Dispatch(uint32_t tid, uint32_t stid, const SimInst& inst, OperandType rType, uint32_t distance)
{
    ASSERT(distance <= VRF_RELATIVE_DISTACE);
    ASSERT(stid != -1U);
    uint32_t mapIdx = (smapPtr[tid][stid][REG_TYPE_MAP.at(rType)] + VRF_RELATIVE_DISTACE - distance) %
        VRF_RELATIVE_DISTACE;
    auto& entry = smap[tid][stid][REG_TYPE_MAP.at(rType)][mapIdx];
    uint32_t ptag = entry.tag;
    auto oldUsingCnt = regUsingCnt[ptag];
    regUsingCnt[ptag] += 1;
    regReadCnt[ptag] += 1;
    LOG_INFO_M(Unit::VRF_REN, Stage::NA) << dec << "[VECTOR " << top->m_coreId << "] tid="
        << tid << " Dispatch uid=" << inst->uid
        << " B" << entry.bid << "-I" << entry.rid << "-G" << entry.gid << "-ptag" << entry.tag << " pair="
        << entry.pair << " regUsingCnt=" << oldUsingCnt << "->" << regUsingCnt[ptag] << " freeSize=" << freeSize
        << " reuse=" << entry.recycled;
    if (entry.pair) {
        regUsingCnt[ptag + 1] += 1;
        regReadCnt[ptag + 1] += 1;
    }
    pmu[ptag].useTime++;
    return entry;
}

void VRFRename::MarkNeedKill(uint32_t tid, uint32_t stid, OperandType rType, uint32_t distance)
{
    ASSERT(distance <= VRF_RELATIVE_DISTACE);
    uint32_t mapIdx = (smapPtr[tid][stid][REG_TYPE_MAP.at(rType)] + VRF_RELATIVE_DISTACE - distance) %
        VRF_RELATIVE_DISTACE;
    auto& entry = smap[tid][stid][REG_TYPE_MAP.at(rType)][mapIdx];
    entry.markNeedKill = true;
}

bool VRFRename::IsVecCore(uint32_t coreThread)
{
    return coreThread < GetSim()->core->configs.threadCount * GetSim()->core->configs.simtPeCount;
}

bool VRFRename::FindReusablePtag(const POperandPtr& src, bool pair, uint32_t& tag)
{
    if (src->IsVRFReg() && !src->reuse && (src->pair == pair)) {
        if (recycleList.at(src->ptag)) {
            // reuse once only
            return false;
        }
        if (GetSim()->core->configs.vrf_reuse_src_kill_reg == 1U) {
            // regUsingCnt
            if (regUsingCnt.at(src->ptag) > 1) {
                // may be using by previous inst
                return false;
            }
        } else {
            ASSERT(GetSim()->core->configs.vrf_reuse_src_kill_reg == 2U);
            // regReadCnt
            if (regReadCnt.at(src->ptag) > 1) {
                // may be read by previous inst
                return false;
            }
        }
        tag = src->ptag;
        return true;
    }
    return false;
}

bool VRFRename::SrcPtagReusable(uint32_t tid, uint32_t stid, const POperandPtr& src, bool pair)
{
    if (src->IsVRFReg() && !src->reuse && (src->pair == pair)) {
        OperandType rType = src->type;
        uint32_t mapIdx = (smapPtr[tid][stid][REG_TYPE_MAP.at(rType)] + VRF_RELATIVE_DISTACE - (src->value + 1)) %
            VRF_RELATIVE_DISTACE;
        auto& entry = smap[tid][stid][REG_TYPE_MAP.at(rType)][mapIdx];
        if (recycleList.at(entry.tag)) {
            // reuse once only
            return false;
        }
        if (GetSim()->core->configs.vrf_reuse_src_kill_reg == 1U) {
            // regUsingCnt
            if (regUsingCnt.at(entry.tag) > 0) {
                // may be using by previous inst
                return false;
            }
        } else {
            ASSERT(GetSim()->core->configs.vrf_reuse_src_kill_reg == 2U);
            // regReadCnt
            if (regReadCnt.at(entry.tag) > 0) {
                // may be read by previous inst
                return false;
            }
        }
        return true;
    }
    return false;
}

void VRFRename::Alloc(VRFMapEntry& info, const SimInst& inst)
{
    if (GetSim()->core->configs.vrf_reuse_src_kill_reg > 0) {
        // try to reuse src.kill
        for (auto& psrc : inst->psrcs_) {
            info.recycled = FindReusablePtag(psrc, info.pair, info.tag);
            if (info.recycled) {
                break;
            }
        }
        if (info.recycled) {
            recycleList.at(info.tag) = true;
        }
    }
    if (!info.recycled) {
        info.tag = info.pair ? GetFreePairTag() : GetFreeTag();
    }
    // write smap
    uint32_t& ptr = smapPtr[info.tid][info.stid][REG_TYPE_MAP.at(info.rType)];
    smap[info.tid][info.stid][REG_TYPE_MAP.at(info.rType)][ptr] = info;
    // allocated entry
    ASSERT(!mapQ[info.tid][info.stid][mapAllocPtr[info.tid][info.stid]].vld);
    mapQ[info.tid][info.stid][mapAllocPtr[info.tid][info.stid]] = info;
    ASSERT(mapFreeSize[info.tid][info.stid] > 0);
    mapFreeSize[info.tid][info.stid]--;
    // increase pointer
    ptr = (ptr + 1) % VRF_RELATIVE_DISTACE;
    mapAllocPtr[info.tid][info.stid] = (mapAllocPtr[info.tid][info.stid] + 1) % mapQ[info.tid][info.stid].size();
    // update pmu
    if (info.recycled) {
        pmu[info.tag].reuseTime += 1;
        pmu[info.tag].deallocCycle = GetSim()->getCycles();
        UpdatePEPmu(pmu[info.tag]);
        if (info.pair) {
            pmu[info.tag + 1].reuseTime += 1;
            pmu[info.tag + 1].deallocCycle = GetSim()->getCycles();
            UpdatePEPmu(pmu[info.tag + 1]);
        }
    }
    pmu[info.tag].allocCycle = GetSim()->getCycles();
    pmu[info.tag].useTime = 1;
    pmu[info.tag].type = info.rType;
    if (info.pair) {
        pmu[info.tag + 1].allocCycle = GetSim()->getCycles();
        pmu[info.tag + 1].useTime = 1;
        pmu[info.tag + 1].type = info.rType;
    }
    if (!IsVecCore(info.tid)) {
        pmu[info.tag].vec = false;
        pmu[info.tag].mtc = true;
    } else {
        pmu[info.tag].vec = true;
        pmu[info.tag].mtc = false;
    }
    LOG_INFO_M(Unit::VRF_REN, Stage::NA) << dec << "[VECTOR " << top->m_coreId << "] tid="
        << info.tid << " Alloc uid=" << inst->uid << " B" << info.bid << "-I" << info.rid << "-G" << info.gid
        << "-ptag" << info.tag << " pair=" << info.pair << " regUsingCnt=" << regUsingCnt[info.tag] << " freeSize="
        << freeSize << " reuse=" << info.recycled;
}

uint32_t VRFRename::GetFreeTag()
{
    for (uint32_t i = 0; i < freeList.size(); ++i) {
        if (freeList[i]) {
            freeList[i] = false;
            regUsingCnt[i] = 0;
            regReadCnt[i] = 0;
            ASSERT(freeSize > 0);
            --freeSize;
            return i;
        }
    }
    ASSERT(0 && "No free VRF tag!");
    return 0;
}

uint32_t VRFRename::GetFreePairTag()
{
    for (uint32_t i = 0; i < freeList.size() - 1; ++i) {
        if (freeList[i] && freeList[i + 1]) {
            // tag
            freeList[i] = false;
            regUsingCnt[i] = 0;
            regReadCnt[i] = 0;
            ASSERT(freeSize > 0);
            --freeSize;
            // tag + 1
            freeList[i + 1] = false;
            regUsingCnt[i + 1] = 0;
            regReadCnt[i + 1] = 0;
            ASSERT(freeSize > 0);
            --freeSize;
            return i;
        }
    }
    ASSERT(0 && "No free VRF tag!");
    return 0;
}

void VRFRename::ReleaseWholeThread(uint32_t tid, uint32_t stid, const SimInst& inst)
{
    for (uint32_t i = 0; i < cmap[tid][stid].size(); ++i) {
        auto &map = cmap[tid][stid][i];
        auto &ptr = cmapPtr[tid][stid][i];
        while (map[ptr].vld) {
            UpdatePmuIf(map[ptr]);
            FreeCMapEntry(map[ptr], inst, false);
            ptr = (ptr + 1) % VRF_RELATIVE_DISTACE;
        }
    }
}

void VRFRename::UpdatePmuIf(const VRFMapEntry& e)
{
    if (!e.kill && !e.done && (e.recycled || !recycleList[e.tag])) {
        pmu[e.tag].deallocCycle = GetSim()->getCycles();
        UpdatePEPmu(pmu[e.tag]);
        if (e.pair) {
            pmu[e.tag + 1].deallocCycle = GetSim()->getCycles();
            UpdatePEPmu(pmu[e.tag + 1]);
        }
    }
}

void VRFRename::Retire(uint32_t tid, uint32_t stid, const SimInst& inst, uint32_t tag, bool recycled, bool end)
{
    uint32_t eid = GetEntryID(tid, stid, tag);
    ASSERT(eid == mapDeallocPtr[tid][stid]);
    auto &e = mapQ[tid][stid][eid];
    e.retired = true;
    auto &map = cmap[e.tid][stid][REG_TYPE_MAP.at(e.rType)];
    auto &ptr = cmapPtr[e.tid][stid][REG_TYPE_MAP.at(e.rType)];
    LOG_INFO_M(Unit::VRF_REN, Stage::NA) << dec << "[VECTOR " << top->m_coreId << "] tid="
        << tid << " Retire uid=" << inst->uid << " B" << map[ptr].bid << "-I" << map[ptr].rid << "-G"
        << map[ptr].gid << "-ptag" << map[ptr].tag << " pair=" << map[ptr].pair << " regUsingCnt="
        << regUsingCnt[map[ptr].tag] << " kill=" << map[ptr].kill << " done=" << map[ptr].done << " cmapVld="
        << map[ptr].vld << " freeSize=" << freeSize;
    if (map[ptr].vld) {
        ASSERT(map[ptr].retired);
        if (!map[ptr].kill && !map[ptr].done && (e.recycled || !recycleList[e.tag])) {
            pmu[map[ptr].tag].deallocCycle = GetSim()->getCycles();
            UpdatePEPmu(pmu[map[ptr].tag]);
            if (map[ptr].pair) {
                pmu[map[ptr].tag + 1].deallocCycle = GetSim()->getCycles();
                UpdatePEPmu(pmu[map[ptr].tag + 1]);
            }
        }
        LOG_DETAIL("@%lu uid:%lu tpc:0x%lx Retire Map vld, tag=%u, recycled=%u, using=%u", GetSim()->getCycles(),
            inst->uid, inst->pc, map[ptr].tag, static_cast<uint32_t>(map[ptr].recycled), regUsingCnt[map[ptr].tag]);
        FreeCMapEntry(map[ptr], inst, false);
    } else {
        LOG_DETAIL("@%lu uid:%lu tpc:0x%lx Retire Map not vld, tag=%u, recycled=%u, using=%u", GetSim()->getCycles(),
            inst->uid, inst->pc, map[ptr].tag, static_cast<uint32_t>(map[ptr].recycled), regUsingCnt[map[ptr].tag]);
    }
    map[ptr] = e;
    RetiredEntry(tid, stid);
    ptr = (ptr + 1) % VRF_RELATIVE_DISTACE;
    if (end) {
        ReleaseWholeThread(tid, stid, inst);
    }
    if (!GetSim()->core->configs.simtEnable) {
        return;
    }

    if (IsVecCore(tid)) {
        vrfRtableTagRetireQ.Write(tag);
    } else {
        vrfMtcRtableTagRetireQ.Write(tag);
    }
}

void VRFRename::FreeCMapEntry(VRFMapEntry& e, const SimInst& inst, bool flush)
{
    LOG_DETAIL("@%lu uid:%lu tpc:0x%lx FreeCMapEntry, tag=%u, recycled=%u, recycleList=%u, using=%u, kill=%u, done=%u, "
        "flush=%u",
        GetSim()->getCycles(), inst->uid, inst->pc, e.tag, static_cast<uint32_t>(e.recycled),
        static_cast<uint32_t>(recycleList[e.tag]), regUsingCnt[e.tag], static_cast<uint32_t>(e.kill),
        static_cast<uint32_t>(e.done), static_cast<uint32_t>(flush));
    if (!e.kill && !e.done && (e.recycled || !recycleList[e.tag])) {
        LOG_DETAIL("@%lu uid:%lu tpc:0x%lx FreeCMapEntry free tag, tag=%u, recycled=%u, recycleList=%u, using=%u",
            GetSim()->getCycles(), inst->uid, inst->pc, e.tag, static_cast<uint32_t>(e.recycled),
            static_cast<uint32_t>(recycleList[e.tag]), regUsingCnt[e.tag]);
        FreeTag(e.tid, e.stid, e.tag, inst);
        if (e.pair) {
            FreeTag(e.tid, e.stid, e.tag + 1, inst);
        }
    }
    e.vld = false;
    e.retired = false;
    e.kill = false;
    e.markNeedKill = false;
    e.done = false;
    e.pair = false;
}

void VRFRename::RetiredEntry(uint32_t tid, uint32_t stid)
{
    auto &e = mapQ[tid][stid][mapDeallocPtr[tid][stid]];
    e.vld = false;
    e.retired = false;
    e.kill = false;
    e.markNeedKill = false;
    e.done = false;
    e.pair = false;
    ++mapFreeSize[tid][stid];
    ASSERT(0 <= mapFreeSize[tid][stid] && mapFreeSize[tid][stid] <= mapQ[tid][stid].size());
    mapDeallocPtr[tid][stid] = (mapDeallocPtr[tid][stid] + 1) % mapQ[tid][stid].size();
}

uint32_t VRFRename::GetEntryID(uint32_t tid, uint32_t stid, uint32_t tag)
{
    for (uint32_t i = 0; i < mapQ[tid][stid].size(); ++i) {
        uint32_t eid = (mapDeallocPtr[tid][stid] + i) % mapQ[tid][stid].size();
        if (mapQ[tid][stid][eid].vld && mapQ[tid][stid][eid].tag == tag) {
            return eid;
        }
    }
    ASSERT(0 && "Can't find match entry by tag!");
    return 0;
}

void VRFRename::FreeEntry(uint32_t tid, uint32_t stid, uint32_t eid, const SimInst& inst)
{
    auto &e = mapQ[tid][stid][eid];
    ASSERT(e.vld);
    if (!e.kill && !e.done && e.vld) {
        FreeTag(tid, stid, e.tag, inst);
        if (e.pair) {
            FreeTag(tid, stid, e.tag + 1, inst);
        }
    }
    e.vld = false;
    e.retired = false;
    e.kill = false;
    e.markNeedKill = false;
    e.done = false;
    e.pair = false;
    ++mapFreeSize[tid][stid];
    ASSERT(0 <= mapFreeSize[tid][stid] && mapFreeSize[tid][stid] <= mapQ[tid][stid].size());
}

void VRFRename::FreeTag(uint32_t tid, uint32_t stid, uint32_t tag, const SimInst& inst)
{
    (void)stid;
    if (freeList[tag]) {
        LOG_ERROR << "Error: tag=" << tag << " is already free.";
        ASSERT(false);
    }
    freeList[tag] = true;
    LOG_DETAIL("@%lu uid:%lu tpc:0x%lx FreeTag reset recycleList, tag=%u, using=%u", GetSim()->getCycles(), inst->uid,
        inst->pc, tag, regUsingCnt[tag]);
    recycleList[tag] = false;
    regUsingCnt[tag] = 0;
    ++freeSize;
    regReadCnt[tag] = 0;
    ASSERT(0 <= freeSize && freeSize <= freeList.size());
    if (!GetSim()->core->configs.simtEnable) {
        return;
    }

    if (IsVecCore(tid)) {
        vrfRtableTagFreeQ.Write(tag);
    } else {
        vrfMtcRtableTagFreeQ.Write(tag);
    }
}

bool VRFRename::Kill(uint32_t tid, uint32_t stid, const SimInst& inst, OperandType rType, uint32_t tag, bool recycled)
{
    LOG_DETAIL("@%lu uid:%lu tpc:0x%lx Kill, tag=%u, src recycled=%u, recycleList=%u, using=%u", GetSim()->getCycles(),
        inst->uid, inst->pc, tag, static_cast<uint32_t>(recycled), static_cast<uint32_t>(recycleList[tag]),
        regUsingCnt[tag]);
    for (auto& e : cmap[tid][stid][REG_TYPE_MAP.at(rType)]) {
        if (!e.vld) {
            continue;
        }
        if (e.recycled != recycled) {
            continue;
        }
        if (e.tag != tag) {
            continue;
        }
        if (e.kill || e.done) {
            continue;
        }
        auto oldUsingCnt = regUsingCnt[tag];
        if (regUsingCnt[tag] > 0) {
            regUsingCnt[tag] -= 1;
            LOG_DETAIL("@%lu uid:%lu tpc:0x%lx Kill decrement, tag=%u, src recycled=%u, recycleList=%u, using=%u",
                GetSim()->getCycles(), inst->uid, inst->pc, tag, static_cast<uint32_t>(recycled),
                static_cast<uint32_t>(recycleList[tag]), regUsingCnt[tag]);
        }
        if (recycleList[e.tag] && !recycled) {
            e.kill = true;
            return true;
        }
        if ((regUsingCnt[tag] == 0) || (recycled == recycleList[e.tag])) {
            e.kill = true;
            pmu[tag].deallocCycle = GetSim()->getCycles();
            UpdatePEPmu(pmu[tag]);
            FreeTag(tid, stid, tag, inst);
            LOG_INFO_M(Unit::VRF_REN, Stage::NA) << dec << "[VECTOR " << top->m_coreId << "] tid=" << tid <<
                " Kill uid=" << inst->uid << " B" << e.bid << "-I" << e.rid << "-G" << e.gid << "-ptag" << e.tag
                << " pair=" << e.pair << " regUsingCnt=" << oldUsingCnt << "->" << regUsingCnt[tag] << " freeSize="
                << freeSize;
            if (e.pair) {
                pmu[tag + 1].deallocCycle = GetSim()->getCycles();
                UpdatePEPmu(pmu[tag + 1]);
                auto oldUsingCnt1 = regUsingCnt[tag + 1];
                FreeTag(tid, stid, tag + 1, inst);
                LOG_INFO_M(Unit::VRF_REN, Stage::NA) << dec << "[VECTOR " << top->m_coreId
                    << "] tid=" << tid << " Kill uid=" << inst->uid << " B" << e.bid << "-I" << e.rid << "-G" << e.gid
                    << "-ptag" << (e.tag + 1) << " pair=" << e.pair << " regUsingCnt=" << oldUsingCnt1 << "->"
                    << regUsingCnt[tag + 1] << " recycled=" << recycled << " freeSize=" << freeSize;
            }
        }
        break;
    }
    return true;
}

bool VRFRename::PreRelease(uint32_t tid, uint32_t stid, const SimInst& inst,
                           OperandType rType, uint32_t tag, bool recycled)
{
    LOG_DETAIL("@%lu uid:%lu tpc:0x%lx PreRelease, tag=%u, src recycled=%u, recycleList=%u, using=%u",
        GetSim()->getCycles(), inst->uid, inst->pc, tag, static_cast<uint32_t>(recycled),
        static_cast<uint32_t>(recycleList[tag]), regUsingCnt[tag]);
    auto& map = cmap[tid][stid][REG_TYPE_MAP.at(rType)];
    for (auto& e : map) {
        if (!e.vld) {
            continue;
        }
        if (e.recycled != recycled) {
            continue;
        }
        if (e.tag != tag) {
            continue;
        }
        if (e.kill || e.done) {
            continue;
        }
        auto oldUsingCnt = regUsingCnt[tag];
        if (regUsingCnt[tag] > 0) {
            regUsingCnt[tag] -= 1;
            LOG_DETAIL("@%lu uid:%lu tpc:0x%lx PreRelease decrement, tag=%u, src recycled=%u, recycleList=%u, using=%u",
                GetSim()->getCycles(), inst->uid, inst->pc, tag, static_cast<uint32_t>(recycled),
                static_cast<uint32_t>(recycleList[tag]), regUsingCnt[tag]);
        }
        if (recycleList[e.tag] && !recycled) {
            e.done = true;
            return true;
        }
        if ((regUsingCnt[tag] == 0) || (recycled == recycleList[e.tag])) {
            e.done = true;
            pmu[tag].deallocCycle = GetSim()->getCycles();
            UpdatePEPmu(pmu[tag]);
            FreeTag(tid, stid, tag, inst);
            vecPE->vpeTop->stats->vrfPreReleaseTimes[REG_TYPE_MAP.at(rType)] += 1;
            LOG_INFO_M(Unit::VRF_REN, Stage::NA) << dec << "[VECTOR " << top->m_coreId
                << "] tid=" << tid << " PreRelease uid=" << inst->uid << " B" << e.bid << "-I" << e.rid << "-G" << e.gid
                << "-ptag" << e.tag << " pair=" << e.pair << " regUsingCnt=" << oldUsingCnt << "->" << regUsingCnt[tag]
                << " recycled=" << recycled << " freeSize=" << freeSize;
            if (e.pair) {
                pmu[tag + 1].deallocCycle = GetSim()->getCycles();
                UpdatePEPmu(pmu[tag + 1]);
                auto oldUsingCnt1 = regUsingCnt[tag + 1];
                FreeTag(tid, stid, tag + 1, inst);
                vecPE->vpeTop->stats->vrfPreReleaseTimes[REG_TYPE_MAP.at(rType)] += 1;
                LOG_INFO_M(Unit::VRF_REN, Stage::NA) << dec << "[VECTOR " << top->m_coreId
                    << "] tid=" << tid << " PreRelease uid=" << inst->uid << " B" << e.bid << "-I" << e.rid
                    << "-G" << e.gid << "-ptag" << (e.tag + 1) << " pair=" << e.pair << " regUsingCnt="
                    << oldUsingCnt1 << "->" << regUsingCnt[tag + 1] << " freeSize=" << freeSize;
            }
        }
        break;
    }
    return true;
}

void VRFRename::Flush(FlushBus flushReq)
{
    for (uint32_t tid = 0; tid < mapQ.size(); tid++) {
        if (flushReq.baseOnThread && flushReq.req.tid != tid) {
            continue;
        }
        FlushSingleMap(tid, flushReq.req.stid, flushReq);
    }
}

void VRFRename::FlushSingleMap(uint32_t tid, uint32_t stid, FlushBus flushReq)
{
    // flush mapQ
    bool flushed = false;
    for (uint32_t i = 0; i < mapQ[tid][stid].size(); ++i) {
        uint32_t eid = (mapDeallocPtr[tid][stid] + i) % mapQ[tid][stid].size();
        auto &e = mapQ[tid][stid][eid];
        if (!e.vld) {
            break;
        }
        if (flushReq.baseOnBid && !LessEqual(flushReq.req.bid, e.bid)) {
            continue;
        }
        if (!flushReq.baseOnBid && (!LessEqual(flushReq.req.bid, flushReq.req.gid, e.bid, e.gid) ||
            !LessEqual(flushReq.req.bid, flushReq.req.rid, e.bid, e.rid))) {
            continue;
        }
        LOG_INFO_M(Unit::VRF_REN, Stage::NA) << dec << " FlushSingleMap B" << e.bid << "-I" << e.rid << "-G" << e.gid
            << "-ptag" << e.tag;
        SimInst inst = make_shared<SimInstInfo>();
        FreeEntry(tid, stid, eid, inst);
        mapAllocPtr[tid][stid] = (mapAllocPtr[tid][stid] + mapQ[tid][stid].size() - 1) % mapQ[tid][stid].size();
        flushed = true;
    }
    // flush cmap
    for (uint32_t i = 0; i < cmap[tid][stid].size(); ++i) {
        for (uint32_t j = 0; j < cmap[tid][stid][i].size(); ++j) {
            uint32_t ptr = (cmapPtr[tid][stid][i] + cmap[tid][stid][i].size() - 1) % cmap[tid][stid][i].size();
            if (!cmap[tid][stid][i][ptr].vld) {
                break;
            }
            if (!((flushReq.baseOnBid && LessEqual(flushReq.req.bid, cmap[tid][stid][i][ptr].bid)) ||
                  (!flushReq.baseOnBid && LessROBID(flushReq.req.bid, cmap[tid][stid][i][ptr].bid)))) {
                break;
            }
            SimInst inst = make_shared<SimInstInfo>();
            LOG_DETAIL("@%lu uid:%lu tpc:0x%lx Flush, tag=%u, recycled=%u, using=%u", GetSim()->getCycles(),
                inst->uid, inst->pc, cmap[tid][stid][i][ptr].tag,
                static_cast<uint32_t>(cmap[tid][stid][i][ptr].recycled),
                regUsingCnt[cmap[tid][stid][i][ptr].tag]);
            FreeCMapEntry(cmap[tid][stid][i][ptr], inst, true);
            cmapPtr[tid][stid][i] = ptr;
        }
    }
    if (!flushed) {
        return;
    }
    ASSERT(!mapQ[tid][stid][mapAllocPtr[tid][stid]].vld && "Illegal ooo-flush!");
    // reset smap and smap pointer
    smap[tid][stid] = cmap[tid][stid];
    smapPtr[tid][stid] = cmapPtr[tid][stid];
    for (uint32_t i = 0; i < mapQ[tid].size(); ++i) {
        uint32_t eid = (mapDeallocPtr[tid][stid] + i) % mapQ[tid][stid].size();
        auto &e = mapQ[tid][stid][eid];
        if (!e.vld) {
            break;
        }
        auto &smapEntry = smap[tid][stid][REG_TYPE_MAP.at(e.rType)][smapPtr[tid][stid][REG_TYPE_MAP.at(e.rType)]];
        smapEntry = e;
        smapPtr[tid][stid][REG_TYPE_MAP.at(e.rType)] =
                                            (smapPtr[tid][stid][REG_TYPE_MAP.at(e.rType)] + 1) % VRF_RELATIVE_DISTACE;
    }
}

void VRFRename::UpdatePEPmu(VRFPmuEntry &pmu)
{
    uint32_t idx = REG_TYPE_MAP.at(pmu.type);
    ASSERT(pmu.deallocCycle >= pmu.allocCycle);
    vecPE->vpeTop->stats->vrfUsedCycles[idx] += (pmu.deallocCycle - pmu.allocCycle);
    vecPE->vpeTop->stats->vrfUsedTimes[idx] += (pmu.useTime);
    pmu.useTime = 0;
    vecPE->vpeTop->stats->vrfRecycledTimes[idx] += (pmu.reuseTime);
    pmu.reuseTime = 0;
}

void VRFRename::InitVecPMU()
{
    ASSERT(vecPE != nullptr);
    ASSERT(vecPE->vpeTop != nullptr);
    ASSERT(vecPE->vpeTop->stats != nullptr);
    vecPE->vpeTop->stats->vrfUsedCycles.resize(HAND_NUM, 0);
    vecPE->vpeTop->stats->vrfUsedTimes.resize(HAND_NUM, 0);
    vecPE->vpeTop->stats->vrfRecycledTimes.resize(HAND_NUM, 0);
    vecPE->vpeTop->stats->vrfPreReleaseTimes.resize(HAND_NUM, 0);
}

void VRFRename::InitMtcPMU()
{
    // ASSERT(mtcPE != nullptr);
    // ASSERT(mtcPE->top != nullptr);
    // ASSERT(mtcPE->top->PE::stats != nullptr);
    // mtcPE->top->PE::stats->vrfUsedCycles.resize(HAND_NUM, 0);
    // mtcPE->top->PE::stats->vrfUsedTimes.resize(HAND_NUM, 0);
    // mtcPE->top->PE::stats->vrfRecycledTimes.resize(HAND_NUM, 0);
    // mtcPE->top->PE::stats->vrfPreReleaseTimes.resize(HAND_NUM, 0);
}

std::ostream& operator<<(std::ostream& os, VRFMapEntry const &e)
{
    if (e.vld) {
        os << "B" << e.bid << "-I" << e.rid << "-G" << e.gid
            << "-tid:" << e.tid << "-ptag:" << e.tag << " "
            << " type:" << REG_TYPE_MAP.at(e.rType) << " pair:" << e.pair
            << " retire:" << e.retired << " kill:" << e.kill << " done:" << e.done;
    } else {
        os << "invalid ";
    }
    return os;
}

}
