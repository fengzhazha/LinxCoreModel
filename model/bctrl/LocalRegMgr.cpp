#include "bctrl/LocalRegMgr.h"

#include "bctrl/spe/DCTop.h"
#include "core/Core.h"

namespace JCore {


static const char* GetRegType(OperandType typ)
{
    switch (typ) {
        case OperandType::OPD_TLINK:
            return "TR";
        case OperandType::OPD_ULINK:
            return "UR";
        case OperandType::OPD_VTLINK:
            return "VTR";
        case OperandType::OPD_VULINK:
            return "VUR";
        case OperandType::OPD_VMLINK:
            return "VMR";
        case OperandType::OPD_VNLINK:
            return "VNR";
        case OperandType::OPD_PREDMASK:
            return "PRED";
        case OperandType::OPD_TILE_DLINK:
        case OperandType::OPD_TILE_TLINK:
        case OperandType::OPD_TILE_ULINK:
        case OperandType::OPD_TILE_MLINK:
        case OperandType::OPD_TILE_NLINK:
            return "TileR";
        default:
            return "UNKNOWN";
    }
}

void LocalRegMgr::InitFreeList(uint64_t gprCnt, uint32_t regWidth)
{
    // 64bit for uint64_t (see iex_rf.cpp)
    constexpr static uint32_t gprWidth = 64;
    regStatus.resize(gprCnt * gprWidth / regWidth);
    for (size_t i = 0; i < regStatus.size(); ++i) {
        regStatus[i].free = true;
        regStatus[i].startTag = i * regWidth / gprWidth;
        RegSeg freeSeg;
        freeSeg.segTag = i * regWidth / gprWidth,
        freeSeg.segSize = regWidth / gprWidth;
        regStatus[i].freeSeg.push_back(freeSeg);
    }
}

void LocalRegMgr::InitPred(uint32_t gprCnt, uint32_t robSize, OperandType typ, uint32_t pe, uint32_t regWidth)
{
    ASSERT(gprCnt > zeroPredSize);
    gprCnt -= zeroPredSize;
    mapSize = gprCnt > robSize ? robSize : gprCnt;
    pSize = gprCnt;
    mapQ.resize(mapSize);
    for (uint32_t i = 0; i < mapQ.size(); ++i) {
        mapQ[i].tag = i + zeroPredSize;
    }
    type = typ;
    peid = pe;
}

void LocalRegMgr::Init(uint32_t gprCnt, uint32_t robSize, OperandType typ, uint32_t pe, uint32_t regWidth)
{
    if (typ == OperandType::OPD_PREDMASK) {
        InitPred(gprCnt, robSize, typ, pe, regWidth);
        return;
    }
    mapSize = gprCnt > robSize ? robSize : gprCnt;
    pSize = gprCnt;
    mapQ.resize(mapSize);
    for (uint32_t i = 0; i < mapQ.size(); ++i) {
        mapQ[i].tag = i;
    }
    type = typ;
    peid = pe;
    if (IsVecR()) {
        InitFreeList(gprCnt, regWidth);
    }
}

void LocalRegMgr::Init(uint32_t gprCnt, uint32_t pCnt, OperandType typ, uint32_t regWidth)
{
    // ifu use, mapQAllocPtr 需要适配，alloc只使用了entry0
    ASSERT(false);
    pSize = gprCnt;
    type = typ;
    mapSize = pCnt;
    mapQ.resize(mapSize);
    for (uint32_t i = 0 ; i < mapQ.size(); ++i) {
        mapQ[i].tag = i;
    }
    if (IsVecR()) {
        InitFreeList(gprCnt, regWidth);
    } else if (IsTileR()) {
        mapSize = (mapQ.size() / TILE_TYPE_COUNT);
        usedPSize = vector<uint32_t>(TILE_TYPE_COUNT, 0);
        usedEntrySize = vector<uint32_t>(TILE_TYPE_COUNT, 0);
        mapQAllocPtr = vector<ROBID>(TILE_TYPE_COUNT, ROBID());
        mapQDeallocPtr  = vector<ROBID>(TILE_TYPE_COUNT, ROBID());
        allocPtr = vector<uint32_t>(TILE_TYPE_COUNT, 0);
        pSize = pSize / TILE_TYPE_COUNT;
        tailPtr = vector<uint32_t>(TILE_TYPE_COUNT, pSize);
        deallocPtr = vector<uint32_t>(TILE_TYPE_COUNT, 0);
    }
}

bool LocalRegMgr::IsTileR()
{
    return OperandTypeIsTile(type);
}

bool LocalRegMgr::IsVecR()
{
    return type == OperandType::OPD_VTLINK || type == OperandType::OPD_VULINK
           || type == OperandType::OPD_VMLINK || type == OperandType::OPD_VNLINK;
}

// Tile
RecordInfo LocalRegMgr::Alloc(ROBID bid, uint32_t size, OperandType tType)
{
    ASSERT(size > 0);
    RecordInfo info;
    info.vld = true;
    info.bid = bid;
    if (allocPtr[static_cast<size_t>(tType)] + size > pSize) {
        ASSERT(static_cast<size_t>(tType) < tailPtr.size());
        ASSERT(static_cast<size_t>(tType) < allocPtr.size());
        info.addr = pSize * static_cast<uint32_t>(tType);
        tailPtr[static_cast<size_t>(tType)] = allocPtr[static_cast<size_t>(tType)];
        allocPtr[static_cast<size_t>(tType)] = size;
    } else {
        info.addr = allocPtr[static_cast<size_t>(tType)] + pSize * static_cast<uint32_t>(tType);
        ASSERT(static_cast<size_t>(tType) < allocPtr.size());
        allocPtr[static_cast<size_t>(tType)] += size;
    }

    info.size = size;
    info.kill = false;
    info.markKill = false;
    info.seq = GetAllocEntryID(tType);
    ASSERT(info.seq.val < mapQ.size());
    info.tag = mapQ[info.seq.val].tag;
    mapQ[info.seq.val] = info;

    IncAllocPtr(tType);
    usedPSize[static_cast<uint32_t>(tType)] += size;
    ++usedEntrySize[static_cast<size_t>(tType)];
    return info;
}

ROBID LocalRegMgr::GetAllocEntryID(OperandType tType)
{
    ROBID eid = mapQAllocPtr[static_cast<uint32_t>(tType)];
    AddROBID(eid, mapSize * static_cast<uint32_t>(tType), mapQ.size());
    return eid;
}

void LocalRegMgr::IncAllocPtr(OperandType tType)
{
    IncROBID(mapQAllocPtr[static_cast<uint32_t>(tType)], mapSize);
}

// Scalar
uint32_t LocalRegMgr::Alloc(ROBID bid, ROBID rid, uint32_t size)
{
    ASSERT(size > 0);
    RecordInfo info;
    info.vld = true;
    info.bid = bid;
    info.rid = rid;
    info.tag = allocPtr[0];
    info.size = size;
    info.kill = false;
    info.markKill = false;
    info.seq = mapQAllocPtr[0];
    ASSERT(mapQAllocPtr[0].val < mapQ.size());
    mapQ[mapQAllocPtr[0].val] = info;
    LOG_INFO_M(Unit::BCC, Stage::NA) << "[LOCAL GPR PE" << dec << peid << "] " << GetRegType(type) << " alloc B" << bid
                                     << "-I" << rid << "-tag" << info.tag << "-seq" << info.seq;

    IncROBID(mapQAllocPtr[0], mapQ.size());
    allocPtr[0] = (allocPtr[0] + size) % pSize;
    usedPSize[0] += size;
    ++usedEntrySize[0];
    return info.tag;
}

// vector
uint32_t LocalRegMgr::Alloc(ROBID bid, ROBID rid, ROBID gid, uint32_t size)
{
    ASSERT(size > 0);
    if (IsVecR()) {
        return AllocVec(bid, rid, gid, size);
    }
    RecordInfo info;
    info.vld = true;
    info.bid = bid;
    info.rid = rid;
    info.gid = gid;
    info.tag = allocPtr[0];
    if (type == OperandType::OPD_PREDMASK) {
        info.tag += zeroPredSize;
    }
    info.size = size;
    info.kill = false;
    info.markKill = false;
    info.seq = mapQAllocPtr[0];
    ASSERT(mapQAllocPtr[0].val < mapQ.size());
    mapQ[mapQAllocPtr[0].val] = info;
    LOG_INFO_M(Unit::BCC, Stage::NA) << "[LOCAL GPR PE" << dec << peid << "] " << GetRegType(type) << " alloc B" << bid
                                     << "-I" << rid << "-G" << gid << "-tag" << info.tag << "-seq" << info.seq;

    IncROBID(mapQAllocPtr[0], mapQ.size());
    allocPtr[0] = (allocPtr[0] + size) % pSize;
    usedPSize[0] += size;
    ++usedEntrySize[0];
    return info.tag;
}

uint32_t LocalRegMgr::FindValidTwoTag(uint32_t size, uint32_t &freeIdx)
{
    for (uint32_t i = 0; i < regStatus.size() - 1; ++i) {
        uint32_t j = i + 1;
        if (regStatus[i].free && regStatus[j].free) {
            RegSeg usedi;
            usedi.segTag = regStatus[i].startTag;
            usedi.segSize = size;
            regStatus[i].useSeg.push_back(usedi);
            RegSeg usedj;
            usedj.segTag = regStatus[j].startTag;
            usedj.segSize = size;
            regStatus[j].useSeg.push_back(usedj);
            regStatus[i].freeSeg.clear();
            regStatus[j].freeSeg.clear();
            regStatus[i].free = false;
            regStatus[j].free = false;
            freeIdx = i;
            return regStatus[i].startTag;
        }
    }
    ASSERT(0 && "local gpr is full, use CheckStall() before alloc");
    return 0;
}

uint32_t LocalRegMgr::FindValidTag(uint32_t size, uint32_t &freeIdx)
{
    auto checkErase = [](vector<RegSeg> &seg, uint32_t j) {
        if (seg[j].segSize == 0) {
            seg.erase(seg.begin() + j);
        }
    };
    if (size == NEED_TWO_REG) {
        return FindValidTwoTag(size, freeIdx);
    }
    for (uint32_t i = 0; i < regStatus.size(); ++i) {
        auto &freeSeg = regStatus[i].freeSeg;
        for (uint32_t j = 0; j < freeSeg.size(); ++j) {
            if (freeSeg[j].segSize >= size) {
                // 满足当前需求
                freeSeg[j].segSize -= size;
                uint32_t tag = freeSeg[j].segTag;
                freeSeg[j].segTag += size;
                regStatus[i].free = false;
                RegSeg used;
                used.segTag = tag;
                used.segSize = size;
                regStatus[i].useSeg.push_back(used);
                checkErase(freeSeg, j);
                freeIdx = i;
                regStatus[i].free = false;
                return tag;
            }
        }
    }
    ASSERT(0 && "local gpr is full, use CheckStall() before alloc");
    return 0;
}

uint32_t LocalRegMgr::AllocVec(ROBID bid, ROBID rid, ROBID gid, uint32_t size)
{
    uint32_t freePtr = 0;
    uint32_t vecAllocPtr = FindValidTag(size, freePtr);
    RecordInfo info;
    info.vld = true;
    info.bid = bid;
    info.rid = rid;
    info.gid = gid;
    info.tag = vecAllocPtr;
    info.size = size;
    info.kill = false;
    info.markKill = false;
    info.freePtr = freePtr;
    if (usedPSize[0] == 0) {
        info.seq = mapQAllocPtr[0];
        ASSERT(mapQAllocPtr[0].val < mapQ.size());
        mapQ[mapQAllocPtr[0].val] = info;
        mapQDeallocPtr[0] = mapQAllocPtr[0];
        tail = mapQAllocPtr[0];
    } else {
        info.prev = tail;
        info.seq = mapQAllocPtr[0];
        ASSERT(tail.val < mapQ.size());
        ASSERT(mapQAllocPtr[0].val < mapQ.size());
        mapQ[tail.val].next = mapQAllocPtr[0];
        mapQ[mapQAllocPtr[0].val] = info;
        tail = mapQAllocPtr[0];
    }
    mapQAllocPtr[0] = GetNextFreeEntry();
    mapQ[mapQAllocPtr[0].val].prev = tail;
    mapQ[tail.val].next = mapQAllocPtr[0];
    LOG_INFO_M(Unit::BCC, Stage::NA) << "[LOCAL GPR PE" << dec << peid << "] " << GetRegType(type) << " alloc B" << bid
                                     << "-I" << rid << "-G" << gid << "-tag" << info.tag << "-seq" << info.seq;

    usedPSize[0] += size;
    ++usedEntrySize[0];
    // Should happen in vnr, if all reg are allocated in vn
    ASSERT((usedEntrySize[0] != MAX_RELEASE_DISTENCE || usedPSize[0] != pSize) &&
            "4 FP64 instruction make REG full, should not happen");
    return vecAllocPtr;
}

void LocalRegMgr::UnsetRdy(uint32_t tag, ExecEngineTyp iexTyp)
{
    PLpvInfo lpvInfo;
    std::shared_ptr<IEX> iex;
    if (sim->core->IsVectorIex(iexTyp)) {
        ASSERT(0);
        iex = sim->core->vectorTop->GetIex(0);
    } else {
        iex = sim->core->iex[iexTyp];
    }
    switch (type) {
        case OperandType::OPD_TLINK:
            iex->SetRegReadyTable(OperandType::OPD_TLINK, tag, false, lpvInfo, peid, tid);
            break;
        case OperandType::OPD_ULINK:
            iex->SetRegReadyTable(OperandType::OPD_ULINK, tag, false, lpvInfo, peid, tid);
            break;
        case OperandType::OPD_PREDMASK:
            iex->SetRegReadyTable(OperandType::OPD_PREDMASK, tag, false, lpvInfo, peid, tid);
            break;
        default:
            break;
    }
}

uint32_t LocalRegMgr::GetMapGroupID(uint32_t eid)
{
    return IsTileR() ? eid / mapSize : 0;
}

void LocalRegMgr::Free(uint32_t eid)
{
    // return;

    if (!mapQ[eid].retired) {
        ASSERT(false);
    }

    if (!mapQ[eid].kill) {
        // kill will be release before here
        uint32_t mgid = GetMapGroupID(eid);
        auto bid = mapQ[eid].bid;
        auto rid = mapQ[eid].rid;
        auto tag = mapQ[eid].tag;
        auto gid = mapQ[eid].gid;
        LOG_INFO_M(Unit::BCC, Stage::NA) << dec << "[LOCAL GPR PE" << dec << peid << "] " << GetRegType(type)
                                         << " free B" << bid << "-I" << rid << "-G" << gid << "-tag" << tag
                                         << "-seq" << eid;
        ASSERT(usedPSize[0] >= mapQ[eid].size);
        ASSERT(usedEntrySize[mgid] >= 1);
        InsertFree(mapQ[eid].tag, mapQ[eid].size, mapQ[eid].freePtr);
    }

    mapQ[eid].vld = false;
    mapQ[eid].retired = false;
    mapQ[eid].idxVld = true;
    mapQ[eid].kill = false;
    mapQ[eid].markKill = false;
    mapQ[eid].size = 0;
}

bool checkDistance(OperandType type, SimInst &inst, ROBID mapQDeallocPtr, uint32_t mapSize)
{
    ROBID seq;
    switch (type) {
        case OperandType::OPD_TLINK:
            seq = inst->tSeq;
            break;
        case OperandType::OPD_ULINK:
            seq = inst->uSeq;
            break;
        case OperandType::OPD_PREDMASK:
            seq = inst->predSeq;
            break;
        default:
            break;
    }

    int distance = DeltaBID(seq, mapQDeallocPtr, mapSize);
    return distance > MAX_RELEASE_DISTENCE;
}

uint32_t LocalRegMgr::GetReleaseDistence()
{
    switch (type) {
        case OperandType::OPD_PREDMASK:
            return 1;
        default:
            return MAX_RELEASE_DISTENCE;
    }
    return 1;
}

void LocalRegMgr::ReportRetired(uint32_t seq, bool isDealloc)
{
    LOG_INFO_M(Unit::BCC, Stage::NA) << "[LOCAL GPR PE" << peid << "] " << GetRegType(type)
                                     << " report local reg retired, B" << mapQ[seq].bid << "-I" << mapQ[seq].rid
                                     << "-tag" << mapQ[seq].tag << "-seq" << seq;

    mapQ[seq].retired = true;
    if (isDealloc) {
        uint32_t mgid = GetMapGroupID(seq);
        if (deallocPtr[mgid] + mapQ[seq].size >= tailPtr[mgid]) {
            ASSERT(mgid < tailPtr.size());
            ASSERT(mgid < deallocPtr.size());
            tailPtr[mgid] = pSize;
            deallocPtr[mgid] = 0;
        } else {
            ASSERT(mgid < deallocPtr.size());
            ASSERT(seq < mapQ.size());
            deallocPtr[mgid] += mapQ[seq].size;
        }
        // Tile R may not be inorder release, so may not need this assertion
        Free(seq);
        ASSERT(mgid < mapQDeallocPtr.size());
        IncROBID(mapQDeallocPtr[mgid], mapSize);
    }
}

void LocalRegMgr::ReportBlockCommit(ROBID bid)
{
    if (IsVecR()) {
        ReportBlockCommitVec(bid);
        return;
    }
    LOG_INFO_M(Unit::BCC, Stage::NA) << "[LOCAL GPR PE" << peid << "] B" << bid << " commit.";
    if (usedPSize[0] == 0 || usedEntrySize[0] == 0) {
        return;
    }
    while (mapQDeallocPtr[0] != mapQAllocPtr[0]) {
        auto &info = mapQ[mapQDeallocPtr[0].val];
        // bid 提交，对应寄存器释放
        if (!info.vld) {
            ASSERT(usedEntrySize[0] == 0);
            break;
        }
        if (info.bid != bid) {
            break;
        }
        ASSERT(mapQ[mapQDeallocPtr[0].val].retired);
        LOG_INFO_M(Unit::BCC, Stage::NA) << "[LOCAL GPR PE" << peid << "] B" << bid << "-I" << info.rid
                                         << "-tag" << info.tag << "-seq" << info.seq << " released.";
        Free(mapQDeallocPtr[0].val);
        IncROBID(mapQDeallocPtr[0], mapSize);
    }
    LOG_INFO_M(Unit::BCC, Stage::NA) << "mapQDeallocPtr[0]" << std::dec << mapQDeallocPtr[0] << " mapQAllocPtr[0]" << mapQAllocPtr[0]
        << " usedPSize[0]" << usedPSize[0] << " usedEntrySize[0]" << usedEntrySize[0];
}

void LocalRegMgr::ReportBlockCommitVec(ROBID bid)
{
    LOG_INFO_M(Unit::BCC, Stage::NA) << "[LOCAL GPR PE" << peid << "] B" << bid << " commit.";
    if (usedPSize[0] == 0 || usedEntrySize[0] == 0) {
        return;
    }
    while (mapQDeallocPtr[0] != mapQAllocPtr[0]) {
        auto &info = mapQ[mapQDeallocPtr[0].val];
        // bid 提交，对应寄存器释放
        if (info.bid != bid) {
            break;
        }
        ASSERT(mapQ[mapQDeallocPtr[0].val].retired);
        LOG_INFO_M(Unit::BCC, Stage::NA) << "[LOCAL GPR PE" << peid << "] B" << bid << "-I" << info.rid
                                         << "-tag" << info.tag << "-seq" << info.seq << " released.";
        ROBID next = mapQ[mapQDeallocPtr[0].val].next;
        if (!info.kill) {
            Free(mapQDeallocPtr[0].val);
        }
        mapQDeallocPtr[0] = next;
    }
}

// only in vector
void LocalRegMgr::ReportGroupCommit(ROBID bid, ROBID gid)
{
    if (!IsVecR()) {
        LOG_INFO_M(Unit::BCC, Stage::NA) << "[LOCAL GPR PE" << peid << "]" << GetRegType(type)
                                         << " B" << bid << "-G" << gid << " commit.";
        if (usedPSize[0] == 0 || usedEntrySize[0] == 0) {
            return;
        }
        while (mapQDeallocPtr[0] != mapQAllocPtr[0]) {
            auto &info = mapQ[mapQDeallocPtr[0].val];
            // bid 提交，对应寄存器释放
            if (info.bid != bid || info.gid != gid) {
                break;
            }
            ASSERT(mapQ[mapQDeallocPtr[0].val].retired);
            LOG_INFO_M(Unit::BCC, Stage::NA) << "[LOCAL GPR PE" << peid << "] B" << bid << "-I" << info.rid
                                             << "-tag" << info.tag << "-seq" << info.seq << " released.";
            Free(mapQDeallocPtr[0].val);
            IncROBID(mapQDeallocPtr[0], mapQ.size());
        }
        return;
    }
    LOG_INFO_M(Unit::BCC, Stage::NA) << "[LOCAL GPR PE" << peid << "] B" << bid << "-G" << gid <<  " commit.";
    ROBID ptr = mapQDeallocPtr[0];
    while (ptr != mapQAllocPtr[0] && usedPSize[0] != 0 && usedEntrySize[0] != 0) {
        auto &info = mapQ[ptr.val];
        // gid, bid 提交，对应寄存器释放
        if (info.bid != bid || info.gid != gid) {
            ptr = mapQ[ptr.val].next;
            continue;
        }

        LOG_DEBUG_M(Unit::BCC, Stage::NA) << "[LOCAL GPR PE" << peid << "] B" << bid << "-I" << info.rid << "-G" << gid
             << "-tag" << info.tag << " released. [type" << static_cast<int>(type) << "]";

        info.vld = false;
        ASSERT(mapQ[ptr.val].kill || mapQ[ptr.val].retired);
        ROBID prev = info.prev;
        ROBID next = info.next;
        if (ptr != mapQDeallocPtr[0]) {
            mapQ[prev.val].next = next;
            mapQ[next.val].prev = prev;
        } else {
            mapQDeallocPtr[0] = next;
        }

        LOG_INFO_M(Unit::BCC, Stage::NA) << "[LOCAL GPR PE" << peid << "] B" << bid << "-I" << info.rid << "-G" << gid
                                         << "-tag" << info.tag << " released. [type" << static_cast<int>(type) << "]";
        if (!info.kill) {
            InsertFree(info.tag, info.size, info.freePtr);
        }
        info.kill = true;
        ptr = next;
    }
}

void LocalRegMgr::ReportKill(uint32_t seq)
{
    if (mapQ[seq].kill) {
        return;
    }
    mapQ[seq].kill = true;
    mapQ[seq].vld = false;

    uint32_t tag = mapQ[seq].tag;
    uint32_t size = mapQ[seq].size;
    uint32_t freeIdx = mapQ[seq].freePtr;
    auto bid = mapQ[seq].bid;
    auto rid = mapQ[seq].rid;
    LOG_INFO_M(Unit::BCC, Stage::NA) << "[LOCAL GPR PE" << dec << peid << "] " << GetRegType(type) << " kill B" << bid
                                     << "-I" << rid << "-tag" << tag;
    // FIME：Kill free phsical size, but not free entry size
    InsertFree(tag, size, freeIdx);
}

void LocalRegMgr::MergeFreeList(std::vector<RegSeg>& seg)
{
    std::sort(seg.begin(), seg.end(), [](const RegSeg& a, const RegSeg& b) {
        return a.segTag < b.segTag;
    });

    for (auto it = seg.begin(); it != seg.end();) {
        auto nextIt = it + 1;
        if (nextIt == seg.end()) {
            break;
        }
        if (it->segTag + it->segSize == nextIt->segTag) {
            nextIt->segTag = it->segTag;
            nextIt->segSize += it->segSize;
            it = seg.erase(it);
        } else {
            ++it;
        }
    }
}

void LocalRegMgr::InsertFreeVecTwoReg(uint32_t tag, uint32_t freeIdx)
{
    // 2048bit/64bit
    const uint32_t regSize = 32;
    regStatus[freeIdx].free = true;
    regStatus[freeIdx + 1].free = true;
    regStatus[freeIdx].useSeg.clear();
    regStatus[freeIdx + 1].useSeg.clear();
    RegSeg segi;
    segi.segTag = tag;
    segi.segSize = regSize;
    RegSeg segj;
    segj.segTag = tag + regSize;
    segj.segSize = regSize;
    regStatus[freeIdx].freeSeg.push_back(segi);
    regStatus[freeIdx + 1].freeSeg.push_back(segj);
}

void LocalRegMgr::InsertFreeVec(uint32_t tag, uint32_t size, uint32_t freeIdx)
{
    ASSERT(IsVecR());
    if (size == NEED_TWO_REG) {
        InsertFreeVecTwoReg(tag, freeIdx);
        return;
    }
    RegSeg freed;
    freed.segTag = tag;
    freed.segSize = size;
    auto &seg = regStatus[freeIdx].useSeg;
    bool found = false;
    for (size_t i = 0; i < seg.size(); ++i) {
        if (seg[i].segTag == tag && seg[i].segSize == size) {
            seg.erase(seg.begin() + i);
            found = true;
            break;
        }
    }
    if (!found) {
        cerr << "[LOCAL GPR PE" << peid << "], freeIdx=" << freeIdx << endl;
    }
    if (seg.empty()) {
        regStatus[freeIdx].free = true;
    }
    ASSERT(found && "Not found in freelist");
    regStatus[freeIdx].freeSeg.push_back(freed);
    MergeFreeList(regStatus[freeIdx].freeSeg);
}

void LocalRegMgr::InsertFree(uint32_t tag, uint32_t size, uint32_t freeIdx)
{
    if (IsVecR()) {
        InsertFreeVec(tag, size, freeIdx);
    }
    --usedEntrySize[GetMapGroupID(tag)];
    usedPSize[GetMapGroupID(tag)] -= size;
}

RecordInfo LocalRegMgr::Dispatch(int32_t offset, OperandType tType)
{
    if (IsVecR()) {
        return DispatchVec(offset);
    }
    ROBID dispSeq = mapQAllocPtr[static_cast<size_t>(tType)];
    SubROBID(dispSeq, offset + 1, mapSize);
    if (IsTileR()) {
        AddROBID(dispSeq, mapSize * static_cast<uint32_t>(tType), mapQ.size());
    }
    auto bid = mapQ[dispSeq.val].bid;
    auto rid = mapQ[dispSeq.val].rid;
    auto tag = mapQ[dispSeq.val].tag;
    LOG_INFO_M(Unit::BCC, Stage::NA) << "[LOCAL GPR PE" << dec << peid << "] " << GetRegType(type) << " dispatch B"
                                     << bid << "-I" << rid << "-tag" << tag << "-seq" << dispSeq;
    return mapQ[dispSeq.val];
}

void LocalRegMgr::MarkNeedKill(int32_t offset)
{
    ROBID dispSeq = mapQAllocPtr[0];
    SubROBID(dispSeq, offset + 1, mapSize);
    mapQ[dispSeq.val].markKill = true;
}

static bool GetSeq(const std::vector<RecordInfo> &mapQ, const ROBID &bid, const ROBID &gid,
    int32_t offset, ROBID &dispSeq)
{
    int32_t off = 0;
    uint32_t mapSize = mapQ.size();
    for (uint32_t i = 0; i < mapQ.size(); ++i) {
        SubROBID(dispSeq, 1, mapSize);
        ROBID oldBid = mapQ[dispSeq.val].bid;
        ROBID oldGid = mapQ[dispSeq.val].gid;
        if (oldBid == bid && oldGid == gid && mapQ[dispSeq.val].vld) {
            ++off;
            if (off == offset + 1) {
                return true;
            }
        }
    }

    return false;
}

// 预留 mcall 上寄存器分配
RecordInfo LocalRegMgr::DispatchInVec(ROBID bid, ROBID gid, int32_t offset)
{
    ROBID dispSeq = mapQAllocPtr[0];
    ASSERT(GetSeq(mapQ, bid, gid, offset, dispSeq));
    auto rid = mapQ[dispSeq.val].rid;
    auto tag = mapQ[dispSeq.val].tag;
    LOG_INFO_M(Unit::BCC, Stage::NA) << "[LOCAL GPR PE" << dec << peid << "] " << GetRegType(type) << " dispatch B"
                                     << bid << "-I" << rid << "-tag" << tag << "-seq" << dispSeq;
    return mapQ[dispSeq.val];
}

RecordInfo LocalRegMgr::DispatchPred(ROBID bid, ROBID gid, int32_t offset, OperandType tTypeS)
{
    ROBID dispSeq = mapQAllocPtr[0];
    SubROBID(dispSeq, offset + 1, mapSize);
    auto oldBid = mapQ[dispSeq.val].bid;
    auto oldRid = mapQ[dispSeq.val].rid;
    auto oldGid = mapQ[dispSeq.val].gid;
    if (oldBid != bid || oldGid != gid || !mapQ[dispSeq.val].vld) {
        // Rename to zero-pred reg
        RecordInfo zero;
        zero.vld = true;
        zero.tag = 0;
        zero.bid = bid;
        zero.gid = gid;
        zero.ready = true;
        return zero;
    }
    auto tag = mapQ[dispSeq.val].tag;
    LOG_INFO_M(Unit::BCC, Stage::NA) << "[LOCAL GPR PE" << dec << peid << "] " << GetRegType(type) << ", pred type:" <<
            GetTileTypeStr(tTypeS) << " dispatch B" << oldBid << "-I" << oldRid << "-tag" << tag << "-seq" << dispSeq;
    return mapQ[dispSeq.val];
}

RecordInfo LocalRegMgr::DispatchVec(int32_t offset)
{
    ROBID dispSeq = tail;
    for (int32_t i = 0; i < offset; ++i) {
        dispSeq = mapQ[dispSeq.val].prev;
    }
    auto bid = mapQ[dispSeq.val].bid;
    auto rid = mapQ[dispSeq.val].rid;
    auto tag = mapQ[dispSeq.val].tag;
    auto gid = mapQ[dispSeq.val].gid;
    LOG_INFO_M(Unit::BCC, Stage::NA) << "[LOCAL GPR PE" << dec << peid << "] " << GetRegType(type) << " dispatch B"
                                     << bid << "-I" << rid << "-G" << gid << "-tag" << tag << "-seq" << dispSeq;

    if (!mapQ[dispSeq.val].vld) {
        cout << "[LOCAL GPR PE" << dec << peid << "] " << GetRegType(type) << " dispatch exception B" << bid
             << "-I" << rid << "-G" << gid << "-tag" << tag << "-seq" << dispSeq << endl;
    }
    return mapQ[dispSeq.val];
}

ROBID LocalRegMgr::GetNextSeq()
{
    return mapQAllocPtr[0];
}

ROBID GetSeq(FlushBus &flushBus, OperandType type)
{
    switch (type) {
        case OperandType::OPD_TLINK:
            return flushBus.req.tSeq;
        case OperandType::OPD_ULINK:
            return flushBus.req.uSeq;
        case OperandType::OPD_PREDMASK:
            return flushBus.req.predSeq;
        default:
            break;
    }
    ASSERT(0 && "no support");
    ROBID id;
    return id;
}

void LocalRegMgr::FlushTile(FlushBus &flushBus)
{
    for (size_t mgid = 0; mgid < TILE_TYPE_COUNT; ++mgid) {
        bool flushed = false;
        for (int32_t i = usedEntrySize[mgid] - 1; i >= 0; --i) {
            size_t eid = (mapQDeallocPtr[mgid].val + i) % mapSize + mgid * mapSize;
            RecordInfo &info = mapQ[eid];
            if (!info.vld || info.retired) {
                break;
            }
            if ((flushBus.baseOnBid && !LessEqual(flushBus.req.bid, info.bid)) ||
                (!flushBus.baseOnBid && !LessROBID(flushBus.req.bid, info.bid))) {
                break;
            }

            LOG_INFO_M(Unit::BCC, Stage::NA) << "[LOCAL GPR PE" << peid << "] type " << GetRegType(type) << " B"
                                             << info.bid << "-I" << info.rid << "-tag" << info.tag << "-seq"
                                             << info.seq << " flush." << " use " << usedPSize[mgid] << " use group "
                                             << usedEntrySize[mgid];

            ASSERT(usedPSize[mgid] >= info.size);
            info.vld = false;
            info.retired = false;
            usedPSize[mgid] -= info.size;
            --usedEntrySize[mgid];
            flushed = true;
        }

        if (flushed) {
            ASSERT(mgid < mapQDeallocPtr.size());
            ASSERT(mgid < mapQAllocPtr.size());
            mapQAllocPtr[mgid] = mapQDeallocPtr[mgid];
            AddROBID(mapQAllocPtr[mgid], usedEntrySize[mgid], mapSize);
            allocPtr[mgid] = (mapQ[mapQAllocPtr[mgid].val].addr) % pSize;
            if (allocPtr[mgid] > deallocPtr[mgid] && tailPtr[mgid] >= allocPtr[mgid]) {
                tailPtr[mgid] = pSize;
            }
        }
    }
}

bool LocalRegMgr::CheckLegal()
{
    ROBID res = mapQDeallocPtr[0];
    AddROBID(res, usedEntrySize[0], mapSize);
    if (res.val != mapQAllocPtr[0].val) {
        return false;
    }
    return true;
}

void LocalRegMgr::flush(FlushBus &flushBus)
{
    if (IsVecR()) {
        FlushVec(flushBus);
        return;
    }

    if (IsTileR()) {
        FlushTile(flushBus);
        return;
    }

    bool flushed = false;
    uint32_t mapSize = mapQ.size();
    // 'seq' is the next allocated register index
    ROBID seq = GetSeq(flushBus, type);
    // 'preSeq' is the current allocated register index
    ROBID preSeq = seq;
    for (int32_t i = usedEntrySize[0] - 1; i >= 0; --i) {
        uint32_t idx = (mapQDeallocPtr[0].val + i) % mapSize;
        ROBID itSeq = mapQDeallocPtr[0];
        AddROBID(itSeq, i, mapSize);
        RecordInfo &info = mapQ[idx];
        if (!info.vld) {
            break;
        }
        if (flushBus.baseOnBid && !LessEqual(flushBus.req.bid, info.bid)) {
            break;
        }
        if (!flushBus.baseOnBid && (!LessEqual(flushBus.req.bid, preSeq, info.bid, itSeq) ||
            !LessEqual(flushBus.req.bid, flushBus.req.rid, info.bid, info.rid))) {
                break;
        }

        LOG_INFO_M(Unit::BCC, Stage::NA) << "[LOCAL GPR PE" << peid << "] type " << GetRegType(type) << " B" << info.bid
                                         << "-I" << info.rid << "-tag" << info.tag << "-seq" << info.seq << " flush."
                                         << " use " << usedPSize[0] << " use group " << usedEntrySize[0];
        ASSERT(usedPSize[0] >= info.size);
        info.vld = false;
        info.retired = false;
        usedPSize[0] -= info.size;
        --usedEntrySize[0];
        flushed = true;
    }

    if (flushed) {
        mapQAllocPtr[0] = mapQDeallocPtr[0];
        AddROBID(mapQAllocPtr[0], usedEntrySize[0], mapSize);
        allocPtr[0] = mapQ[mapQAllocPtr[0].val].tag;
    }
}

void LocalRegMgr::FlushVec(FlushBus &flushBus)
{
    // 'seq' is the next allocated register index
    ROBID seq = GetSeq(flushBus, type);
    // 'preSeq' is the current allocated register index
    ROBID preSeq = seq;

    ROBID idx = tail;
    // cout << GetRegType(type) << " recv flush: " << flushBus << endl;
    for (int32_t i = usedEntrySize[0] - 1; i >= 0; --i) {
        RecordInfo &info = mapQ[idx.val];
        ROBID itSeq = info.seq;
        if (!info.vld) {
            break;
        }
        if (flushBus.baseOnBid && !LessEqual(flushBus.req.bid, info.bid)) {
            break;
        }
        if (!flushBus.baseOnBid && (!LessEqual(flushBus.req.bid, flushBus.req.gid, preSeq, info.bid, info.gid, itSeq) ||
            !LessEqual(flushBus.req.bid, flushBus.req.gid, flushBus.req.rid, info.bid, info.gid, info.rid))) {
                break;
        }

        ASSERT(usedPSize[0] >= info.size);
        info.vld = false;
        info.retired = false;
        info.kill = false;
        info.markKill = false;
        itSeq = mapQ[itSeq.val].prev;
        idx = mapQ[idx.val].prev;
        ROBID prev = mapQ[tail.val].prev;

        InsertFree(info.tag, info.size, info.freePtr);
        auto bid = info.bid;
        auto rid = info.rid;
        LOG_INFO_M(Unit::BCC, Stage::NA) << "[LOCAL GPR PE" << dec << peid << "] " << GetRegType(type) << " flush B"
                                         << bid << "-I" << rid << "-G" << info.gid << "-tag" << info.tag << "-seq"
                                         << tail;
        tail = prev;
    }

    mapQAllocPtr[0] = GetNextFreeEntry();
    mapQ[tail.val].next = mapQAllocPtr[0];
    mapQ[mapQAllocPtr[0].val].prev = tail;
}

ROBID LocalRegMgr::GetCurSeq()
{
    return mapQAllocPtr[0];
}

ROBID LocalRegMgr::GetPrevROBID(ROBID& seq)
{
    ROBID prev = seq;
    if (IsVecR()) {
        prev = mapQ[seq.val].prev;
    } else {
        SubROBID(prev, 1, mapQ.size());
    }
    return prev;
}

void LocalRegMgr::SetCurSeq(ROBID &seq)
{
    if (!IsVecR()) {
        mapQAllocPtr[0] = seq;
    }
}

bool LocalRegMgr::VecRegEnoughFor(uint32_t size)
{
    if (size == NEED_TWO_REG) {
        for (uint32_t i = 0; i < regStatus.size() - 1; ++i) {
            uint32_t j = i + 1;
            if (regStatus[i].free && regStatus[j].free) {
                return true;
            }
        }
        return false;
    }
    for (uint32_t i = 0; i < regStatus.size(); ++i) {
        auto &freeSeg = regStatus[i].freeSeg;
        for (uint32_t j = 0; j < freeSeg.size(); ++j) {
            if (freeSeg[j].segSize >= size) {
                return true;
            }
        }
    }
    return false;
}

void LocalRegMgr::PrintInfo()
{
    ROBID head = mapQDeallocPtr[0];
    while (head != mapQAllocPtr[0]) {
        cout << "B" << mapQ[head.val].bid << "-I" << mapQ[head.val].rid << "-G"
             << mapQ[head.val].gid << ", vld=" << mapQ[head.val].vld << "size=" << mapQ[head.val].size << endl;
        head = mapQ[head.val].next;
    }
}

bool LocalRegMgr::CheckStall(uint32_t size, OperandType tType)
{
    if (IsTileR()) {
        size_t idx = static_cast<size_t>(tType);
        return (usedEntrySize[idx] + 1) > mapSize ||
               (usedPSize[idx] + size > tailPtr[idx]) ||
               (allocPtr[idx] > deallocPtr[idx] && allocPtr[idx] + size > pSize && deallocPtr[idx] < size);
    }
    if (!IsVecR()) {
        return (usedEntrySize[0] + 1) > mapSize ||
               (usedPSize[0] + size > pSize);
    }

    // vector
    return (usedEntrySize[0] + 1) > mapQ.size() || (usedPSize[0] + size > pSize) || !VecRegEnoughFor(size);
}

void LocalRegMgr::Check(uint32_t size, OperandType tType)
{
    cout << "check Tile type:" << dec << static_cast<int>(tType) << " usedESize"
         << usedEntrySize[static_cast<size_t>(tType)] << " mapSize:" << mapSize
         << " usedPSize:" << usedPSize[static_cast<size_t>(tType)] << " PSize:"
         << pSize << " needSize:" << size << endl;
    cout << " allocPtrA " << hex << allocPtr[static_cast<int>(tType)]
         << dec << " deallocPtr " << mapQDeallocPtr[static_cast<int>(tType)]
         << " allocPtr " << mapQAllocPtr[static_cast<int>(tType)] << endl;
    for (uint32_t i = 0; i < mapSize; ++i) {
        ROBID idx = mapQDeallocPtr[static_cast<size_t>(tType)];
        AddROBID(idx, i, mapSize);
        uint32_t eid = idx.val + mapSize * static_cast<size_t>(tType);
        if (!mapQ[eid].vld) {
            break;
        }
        cout << "entry: " << mapQ[eid] << endl;
    }
}

void LocalRegMgr::ReportMaskVLd(ROBID &seq, ROBID bid, ROBID rid)
{
    uint32_t rsize = mapQ.size();
    for (uint32_t i = 0; i < rsize; i++) {
        RecordInfo &it = mapQ[(i + seq.val) % rsize];

        if (it.bid != bid) {
            break;
        }

        if (LessEqual(it.rid, rid)) {
            it.idxVld = false;
        }
    }
}

ROBID LocalRegMgr::GetNextFreeEntry()
{
    uint32_t cnt = 0;
    uint32_t size = mapQ.size();
    ROBID next = tail;
    IncROBID(next, size);

    while (cnt != size) {
        if (!mapQ[next.val].vld) {
            return next;
        }
        IncROBID(next, size);
        ++cnt;
    }
    return mapQDeallocPtr[0];
}

void LocalRegMgr::PrintFreeList()
{
    cout << "----------" << GetRegType(type) << " PE" << peid << "---------" << endl;
    for (size_t i = 0; i < regStatus.size(); ++i) {
        cout << "regStatus[" << i << "]\n";
        for (size_t j = 0; j < regStatus[i].useSeg.size(); ++j) {
            cout << "use[" << j << "] = { tag: " << regStatus[i].useSeg[j].segTag << ", size: "
                 << regStatus[i].useSeg[j].segSize << " }" <<endl;
        }
        for (size_t j = 0; j < regStatus[i].freeSeg.size(); ++j) {
            cout << "free[" << j << "] = { tag: " << regStatus[i].freeSeg[j].segTag << ", size: "
                 << regStatus[i].freeSeg[j].segSize << " }" <<endl;
        }
    }
    cout << "-----END " << GetRegType(type) << " PE" << peid << "---------" << endl;
}

uint64_t LocalRegMgr::GetTileRenSize(OperandType tType)
{
    return usedPSize[static_cast<size_t>(tType)];
}

uint64_t LocalRegMgr::GetTileMapQSize(OperandType tType)
{
    return usedEntrySize[static_cast<size_t>(tType)];
}

std::ostream &operator<<(std::ostream &os, const RecordInfo &info)
{
    if (info.vld) {
        os << dec << "B" << info.bid  << " kill:" << info.kill << " " << static_cast<int>(info.type);
        os << " size:" << info.size;
        os << " tag:" << info.tag;
        os << hex << " addr:0x" << info.addr;
        os << " retire:" << info.retired;
    } else {
        os << "invalid";
    }
    return os;
}

} // namespace JCore
