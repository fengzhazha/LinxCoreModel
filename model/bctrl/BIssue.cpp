#include "bctrl/BIssue.h"
#include "ModelCommon/ModelEnumDefines.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <cmath>

#include "bctrl/BCtrl.h"
#include "core/Core.h"

namespace JCore {

using namespace std;

void BlockIssueQueueUnit::Work()
{
    InsertBlockCmd();
    DispatchBlockCmd();
    WakeupBlockCmd();
    for (uint32_t i = 0; i < GetSim()->core->configs.scalar_smt_thread; ++i) {
        PrevTileRelease(i);
    }
    RetireMCallGroup();
    for (uint32_t i = 0; i < GetSim()->core->configs.scalar_smt_thread; ++i) {
        GatherStats(i);
    }
}

void BlockIssueQueueUnit::Xfer()
{
}

void BlockIssueQueueUnit::Reset()
{
}

void BlockIssueQueueUnit::Build()
{
    bIssueQ.push_back(BIssueQ(BIQType::CUBE_IQ, top->configs.cubeIssueQDepth,
                              GetSim()->core->configs.scalar_smt_thread));
    bIssueQ.push_back(BIssueQ(BIQType::VEC_IQ, top->configs.vecIssueQDepth, GetSim()->core->configs.scalar_smt_thread));
    bIssueQ.push_back(BIssueQ(BIQType::VET_IQ, top->configs.vecIssueQDepth, GetSim()->core->configs.scalar_smt_thread));
    bIssueQ.push_back(BIssueQ(BIQType::MTC_IQ, top->configs.mtcIssueQDepth, GetSim()->core->configs.scalar_smt_thread));
    bIssueQ.push_back(BIssueQ(BIQType::TMA_IQ, top->configs.tmaIssueQDepth, GetSim()->core->configs.scalar_smt_thread));
    bIssueQ.push_back(BIssueQ(BIQType::TAU_IQ, top->configs.tauIssueQDepth, GetSim()->core->configs.scalar_smt_thread));
    bIssueQ.push_back(BIssueQ(BIQType::MCALL_IQ, top->configs.mcallIssueQDepth,
                              GetSim()->core->configs.scalar_smt_thread));
    for (auto &q : bIssueQ) {
        q.top = top;
        if (q.type == BIQType::CUBE_IQ) {
            q.cHandIDAlloc = std::make_shared<CHandIDAllocator>();
            q.cHandIDAlloc->cubeCreditVec.resize(top->sim->core->configs.cube_core_num);
            q.cHandIDAlloc->Build(GetSim()->core->configs.scalar_smt_thread);
            for (size_t i = 0 ; i < q.cHandIDAlloc->cubeCreditVec.size(); i++) {
                q.cHandIDAlloc->cubeCreditVec[i].peId = i;
                for (uint32_t stid = 0; stid < GetSim()->core->configs.scalar_smt_thread; ++stid) {
                    q.cHandIDAlloc->totalSize[i][stid] = top->cubeConfig.l0c_buffer_entry;
                    q.cHandIDAlloc->usingSize[i][stid] = 0;
                    q.cHandIDAlloc->allocPtr[i][stid] = 0;
                    q.cHandIDAlloc->deallocPtr[i][stid] = 0;
                }
            }
        }
    }

    blockDispatchQ[BIQType::CUBE_IQ].resize(top->sim->core->configs.cube_core_num);
    blockDispatchQ[BIQType::VEC_IQ].resize(1);
    blockDispatchQ[BIQType::VET_IQ].resize(1);
    blockDispatchQ[BIQType::MTC_IQ].resize(1);
    blockDispatchQ[BIQType::TMA_IQ].resize(1);
    blockDispatchQ[BIQType::TAU_IQ].resize(1);
    blockDispatchQ[BIQType::MCALL_IQ].resize(1);
    blockDispatchQ[BIQType::CUBE_IQ][0] = nullptr;
    blockDispatchQ[BIQType::VEC_IQ][0] = nullptr;
    blockDispatchQ[BIQType::MTC_IQ][0] = nullptr;
    blockDispatchQ[BIQType::TMA_IQ][0] = nullptr;
    blockDispatchQ[BIQType::TAU_IQ][0] = nullptr;
    blockDispatchQ[BIQType::VET_IQ][0] = nullptr;
    blockDispatchQ[BIQType::MCALL_IQ][0] = nullptr;
    blockWakeupQ[BIQType::CUBE_IQ] = nullptr;
    blockWakeupQ[BIQType::VEC_IQ] = nullptr;
    blockWakeupQ[BIQType::VET_IQ] = nullptr;
    blockWakeupQ[BIQType::MTC_IQ] = nullptr;
    blockWakeupQ[BIQType::TMA_IQ] = nullptr;
    blockWakeupQ[BIQType::TAU_IQ] = nullptr;
    blockWakeupQ[BIQType::MCALL_IQ] = nullptr;

    auto &tmuConfig = GetSim()->core->tileUtils.configs;
    uint64_t totalTileSize = tmuConfig.tile_reg_size * NUM_1024;
    if (top->configs.shared_tile_enable) {
        sharedTileStatus.resize(GetSim()->core->configs.scalar_smt_thread);
        for (uint32_t i = 0; i < sharedTileStatus.size(); ++i) {
            sharedTileStatus[i].BuildCapacity(totalTileSize, top->configs.tile_element_size);
            map<OperandType, uint32_t> tileHandlePerMap = {
                {OperandType::OPD_TILE_TLINK, top->configs.tile_t_hand_size},
                {OperandType::OPD_TILE_ULINK, top->configs.tile_u_hand_size},
                {OperandType::OPD_TILE_MLINK, top->configs.tile_m_hand_size},
                {OperandType::OPD_TILE_NLINK, top->configs.tile_n_hand_size},
                {OperandType::OPD_TILE_STACK, top->configs.tile_stk_hand_size},
                {OperandType::OPD_TILE_MCALL_GPR, 0},
                {OperandType::OPD_TILE_MCALL_STK, 0},
            };
            sharedTileStatus[i].BuildTilePartion(tileHandlePerMap, top->configs.tile_tag_num);
            sharedTileStatus[i].top = this;
        }
    }

    top->stats->tileregTagUsage.resize(TILE_TYPE_COUNT, 0);
    top->stats->tileregTagUsageHistogram.resize(TILE_TYPE_COUNT);
    uint64_t segAddr = 0;

    uint64_t handTileSize = (totalTileSize * top->configs.tile_t_hand_size) / NUM_100;
    tileStatus[OperandType::OPD_TILE_TLINK] = TileStatusGroup(OperandType::OPD_TILE_TLINK,
        top->configs.tile_tag_num, segAddr, handTileSize, RELATIVE_DISTANCE);
    tileStatus[OperandType::OPD_TILE_TLINK].top = this;
    top->stats->tileregTagUsageHistogram[TileType2Idx(OperandType::OPD_TILE_TLINK)].resize(
        top->configs.tile_tag_num + 1, 0);
    segAddr += handTileSize;
    top->stats->tileMaxHandSize[TileType2Idx(OperandType::OPD_TILE_TLINK)] = handTileSize;
    handTileSize = (totalTileSize * top->configs.tile_u_hand_size) / NUM_100;
    tileStatus[OperandType::OPD_TILE_ULINK] = TileStatusGroup(OperandType::OPD_TILE_ULINK,
        top->configs.tile_tag_num, segAddr, handTileSize, RELATIVE_DISTANCE);
    tileStatus[OperandType::OPD_TILE_ULINK].top = this;
    top->stats->tileregTagUsageHistogram[TileType2Idx(OperandType::OPD_TILE_ULINK)].resize(
        top->configs.tile_tag_num + 1, 0);
    segAddr += handTileSize;
    top->stats->tileMaxHandSize[TileType2Idx(OperandType::OPD_TILE_ULINK)] = handTileSize;
    handTileSize = (totalTileSize * top->configs.tile_m_hand_size) / NUM_100;
    tileStatus[OperandType::OPD_TILE_MLINK] = TileStatusGroup(OperandType::OPD_TILE_MLINK,
        top->configs.tile_tag_num, segAddr, handTileSize, RELATIVE_DISTANCE);
    tileStatus[OperandType::OPD_TILE_MLINK].top = this;
    top->stats->tileregTagUsageHistogram[TileType2Idx(OperandType::OPD_TILE_MLINK)].resize(
        top->configs.tile_tag_num + 1, 0);
    segAddr += handTileSize;
    top->stats->tileMaxHandSize[TileType2Idx(OperandType::OPD_TILE_MLINK)] = handTileSize;
    handTileSize = (totalTileSize * top->configs.tile_n_hand_size) / NUM_100;
    tileStatus[OperandType::OPD_TILE_NLINK] = TileStatusGroup(OperandType::OPD_TILE_NLINK,
        top->configs.tile_tag_num, segAddr, handTileSize, RELATIVE_DISTANCE);
    tileStatus[OperandType::OPD_TILE_NLINK].top = this;
    top->stats->tileregTagUsageHistogram[TileType2Idx(OperandType::OPD_TILE_NLINK)].resize(
        top->configs.tile_tag_num + 1, 0);
    segAddr += handTileSize;
    top->stats->tileMaxHandSize[TileType2Idx(OperandType::OPD_TILE_NLINK)] = handTileSize;

    handTileSize = (totalTileSize * top->configs.tile_stk_hand_size) / NUM_100;
    tileStatus.emplace(OperandType::OPD_TILE_STACK, TileStatusGroup(OperandType::OPD_TILE_STACK,
        top->configs.tile_tag_num, segAddr, handTileSize, 1U));
    tileStatus.at(OperandType::OPD_TILE_STACK).top = this;
    top->stats->tileregTagUsageHistogram[TileType2Idx(OperandType::OPD_TILE_STACK)].resize(
        top->configs.tile_tag_num + 1, 0);
    segAddr += handTileSize;
    top->stats->tileMaxHandSize[TileType2Idx(OperandType::OPD_TILE_STACK)] = handTileSize;

    tileStatus[OperandType::OPD_TILE_MCALL_GPR] = TileStatusGroup(OperandType::OPD_TILE_MCALL_GPR,
        top->configs.tile_tag_num, 0, 0, 0);
    tileStatus[OperandType::OPD_TILE_MCALL_GPR].top = this;
    top->stats->tileregTagUsageHistogram[TileType2Idx(OperandType::OPD_TILE_MCALL_GPR)].resize(
        top->configs.tile_tag_num + 1, 0);
    top->stats->tileMaxHandSize[TileType2Idx(OperandType::OPD_TILE_MCALL_GPR)] = 0;

    tileStatus[OperandType::OPD_TILE_MCALL_STK] = TileStatusGroup(OperandType::OPD_TILE_MCALL_STK,
        top->configs.tile_tag_num, 0, 0, 0);
    tileStatus[OperandType::OPD_TILE_MCALL_STK].top = this;
    top->stats->tileregTagUsageHistogram[TileType2Idx(OperandType::OPD_TILE_MCALL_STK)].resize(
        top->configs.tile_tag_num + 1, 0);
    top->stats->tileMaxHandSize[TileType2Idx(OperandType::OPD_TILE_MCALL_STK)] = 0;

    tileStatus[OperandType::OPD_TILE_ACC] = TileStatusGroup(OperandType::OPD_TILE_ACC, top->configs.tile_acc_tag_num,
                                                            segAddr, handTileSize, RELATIVE_DISTANCE);
    tileStatus[OperandType::OPD_TILE_ACC].top = this;
    top->stats->tileregTagUsageHistogram[TileType2Idx(OperandType::OPD_TILE_ACC)].resize(
        top->configs.tile_acc_tag_num + 1, 0);

    accPtr.resize(GetSim()->core->configs.scalar_smt_thread);
    accChainId.resize(GetSim()->core->configs.scalar_smt_thread);
    for (uint32_t stid = 0; stid < GetSim()->core->configs.scalar_smt_thread; ++stid) {
        accPtr[stid] = top->configs.tile_acc_tag_num - 1;
        accChainId[stid] = 0;
    }
    bIssueQ[static_cast<uint64_t>(BIQType::CUBE_IQ)].SetAccChainFilter(top->configs.parallel_acc_chain_threshold);

    if (GetSim()->GetSwimLogger() != nullptr) {
        auto swimLog = GetSim()->GetSwimLogger();
        for (auto it = tileSwimIdMap.begin(); it != tileSwimIdMap.end(); ++it) {
            swimLog->SetThreadName("Alive " + GetTileTypeStr(it->first), top->machineId, top->machineId + it->second);
        }
        swimLog->SetThreadName("Total Tile Alive ", top->machineId, top->machineId + totalTileSwimId);
    }
    mcallStatus.maxGroupSchedulerCreit = top->core->configs.mcallGroupSchedulerDepth;
    mcallStatus.Reset();
}

SimSys *BlockIssueQueueUnit::GetSim()
{
    return top->sim;
}

void BlockIssueQueueUnit::WindowSlides(uint64_t distance, BIQType iq, bool isLoad)
{
    (void)distance;
    (void)isLoad;
    (void)iq;
}

void BlockIssueQueueUnit::Flush(FlushBus &flushReq)
{
    for (auto &biq : bIssueQ) {
        biq.Flush(flushReq);
    }
    uint32_t stid = flushReq.req.stid;
    auto matchLessEqualBid = [&flushReq] (BlockCommandPtr cmd) -> bool {
        return cmd->stid == flushReq.req.stid && LessEqual(flushReq.req.bid, cmd->bid);
    };
    for (auto &dispQ : blockDispatchQ) {
        for (auto &dispQue : dispQ.second) {
            if (!dispQue) {
                continue;
            }
            auto& readQ = dispQue->GetRawReadData();
            auto& writeQ = dispQue->GetRawWriteData();
            auto& delayQ = dispQue->GetRawDelayCycle();
            for (auto it = writeQ.cbegin(); it != writeQ.cend();) {
                if (matchLessEqualBid(*it)) {
                    if (!delayQ.empty()) {
                        delayQ.erase(delayQ.begin() + (it - writeQ.begin()));
                    }
                    RollBackLCubeCredit(*it);
                    it = writeQ.erase(it);
                } else {
                    ++it;
                }
            }
            for (auto it = readQ.cbegin(); it != readQ.cend();) {
                if (matchLessEqualBid(*it)) {
                    RollBackLCubeCredit(*it);
                    it = readQ.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
    for (auto &wakeupQ : blockWakeupQ) {
        if (!wakeupQ.second) {
            continue;
        }
        wakeupQ.second->FlushIf(matchLessEqualBid);
    }
    auto matchCMDUInst = [&flushReq] (SimInst inst) -> bool {
        return inst->stid == flushReq.req.stid && LessEqual(flushReq.req.bid, inst->bid);
    };
    cmdIQBISQ->FlushIf(matchCMDUInst);
    if (top->configs.shared_tile_enable) {
        sharedTileStatus[stid].Flush(flushReq);
    } else {
        tileStatus.at(OperandType::OPD_TILE_TLINK).FlushTileAddr(flushReq);
        tileStatus.at(OperandType::OPD_TILE_ULINK).FlushTileAddr(flushReq);
        tileStatus.at(OperandType::OPD_TILE_MLINK).FlushTileAddr(flushReq);
        tileStatus.at(OperandType::OPD_TILE_NLINK).FlushTileAddr(flushReq);
        tileStatus.at(OperandType::OPD_TILE_STACK).FlushTileAddr(flushReq);
    }

    accChainId[flushReq.req.stid] = top->blockROB.GetACCChainID(flushReq.req.bid, flushReq.req.stid);
    accPtr[flushReq.req.stid] = top->blockROB.GetACCTagPtr(flushReq.req.bid, flushReq.req.stid);

    if (mcallStatus.curMCall.cmd != nullptr) {
        if (matchLessEqualBid(mcallStatus.curMCall.cmd)) {
            mcallStatus.Reset();
        }
    }
}

void BlockIssueQueueUnit::RollBackLCubeCredit(BlockCommandPtr cmd)
{
    auto& cubeCreditVector = bIssueQ[0].cHandIDAlloc->cubeCreditVec;
    auto sortFun = [this](const CubeCredit a, const CubeCredit b) {
        return a.peId < b.peId;
    };
    std::sort(cubeCreditVector.begin(), cubeCreditVector.end(), sortFun);
    if (!CubeTileOp(cmd->tileOp)) {
        return;
    }
    cubeCreditVector[cmd->peid].issuedCubeCmd--;
}

void TileStatus::Reset()
{
    ready = false;
    canDealloc = false;
    allocated = false;
    size = -1U;
    addr = -1U;
    producerCmd = nullptr;
    producer = nullptr;
    isZero = false;
    tileInfo = nullptr;
    LOG_INFO_M(Unit::BCC, Stage::NA) << "Reset tile " << Dump();
}

void SharedMapQStatus::Reset()
{
    vld = false;
    kill = false;
    isZero = false;
    ready = false;
    retired = false;
    occupiedTagNum = 0;
    handType = OperandType::OPD_INVALID;
    producerCmd = nullptr;
    moveInfoQ.clear();
    copyInfo = TcopyTagInfo();
    lastReuse = true;
    usingConsumCnt = 0;
    consumList.clear();
    tileInfo = nullptr;
}

bool TileStatusGroup::TryAllocateTileAddr(uint64_t size)
{
    if (usingTagNum >= entryNum) {
        return false;
    }
    // zero tile reg
    if (size == 0) {
        return true;
    }
    /* case1:
     * |tile| <= segStartAddr
     * |tile| <= deallocPtr
     * |....|
     * |tile| <= currAllocAddr
     * |tile|
     * |tile| <= segStartAddr + threshold
     * case2:
     * |tile| <= segStartAddr
     * |tile| <= currAllocAddr
     * |....|
     * |tile| <= deallocPtr
     * |tile|
     * |tile| <= segStartAddr + threshold
    */
    uint64_t maxAllocAddr = segEndAddr;
    if (entry[deallocPtr].allocated && entry[deallocPtr].addr >= currAllocAddr) { // 区分case 1 和case2
        maxAllocAddr = entry[deallocPtr].addr; // case2
    } else {
        if (currAllocAddr + size > maxAllocAddr) { // case1
            currAllocAddr = segStartAddr;
            maxAllocAddr = entry[deallocPtr].addr;
        }
    }
    if (currAllocAddr + size > maxAllocAddr) {
        return false;
    }
    return true;
}

void TileStatusGroup::FlushTileAddr(FlushBus &flushReq)
{
    bool flushed = false;
    uint32_t flushCheckNum = usingTagNum - (retiredPtr >= deallocPtr
                             ? retiredPtr - deallocPtr
                             : entryNum - deallocPtr + retiredPtr);
    for (uint32_t i = 0; i < flushCheckNum; ++i) {
        uint32_t ptr = (retiredPtr + i) % entryNum;
        if (!entry[ptr].allocated) {
            break;
        }
        if (LessEqual(flushReq.req.bid, entry[ptr].producerCmd->bid)) {
            if (!flushed) {
                currAllocAddr = entry[ptr].addr;
                lastAllocPtr = ptr;
                allocPtr = ptr;
            }
            flushed = true;
            --usingTagNum;
            currSize += entry[ptr].size;
            auto swimLog = top->GetSim()->GetSwimLogger();
            if (swimLog != nullptr) {
                uint64_t tid = top->top->machineId + top->tileSwimIdMap.at(handType);
                swimLog->AddCounterEvent(top->top->machineId, tid,
                                         TraceLog::CounterType::QUEUE_POP,
                                         entry[ptr].size);
                swimLog->AddCounterEvent(top->top->machineId, top->machineId + top->totalTileSwimId,
                                         TraceLog::CounterType::QUEUE_POP,
                                         entry[ptr].size);
            }
            entry[ptr].Reset();
        }
        if (flushed && entry[ptr].allocated) {
            ASSERT(0 && "OOO tile rename is not supported");
        }
    }
}

bool TileStatusGroup::AllocateTileAddr(uint64_t &tag, uint64_t size, bool isZero)
{
    if (!TryAllocateTileAddr(size)) {
        return false;
    }
    if (isZero) {
        return AllocateZeroTileAddr(tag);
    }
    tag = allocPtr;
    allocPtr = (allocPtr + 1) % entry.size();
    entry[tag].Reset();
    entry[tag].allocated = true;
    entry[tag].addr = currAllocAddr;
    uint64_t tmuBankAddr = (entry[tag].addr / top->GetSim()->core->tileUtils.configs.bank_element_size);
    entry[tag].size = size;
    top->GetSim()->core->tileUtils.CheckAddrOverflow(tmuBankAddr, size);
    usingTagNum++;
    currSize += size;
    currAllocAddr = currAllocAddr + size;
    return true;
}

bool TileStatusGroup::AllocateZeroTileAddr(uint64_t &tag)
{
    tag = allocPtr;
    allocPtr = (allocPtr + 1) % entry.size();
    entry[tag].Reset();
    entry[tag].allocated = true;
    entry[tag].addr = -1ULL;
    entry[tag].size = 0;
    usingTagNum++;
    return true;
}

uint32_t SharedTileStatus::GetFreeTileVtag(OperandType handType)
{
    uint32_t vtag = -1U;
    if (OperandTypeIsMcallTile(handType)) {
        for (uint32_t i = 0; i < mapQEntry[handType].size(); ++i) {
            if (!mapQEntry[handType][i].vld) {
                vtag = i;
                break;
            }
        }
    } else {
        vtag = allocPtr[handType];
    }
    ASSERT(vtag != -1U);
    return vtag;
}

bool SharedTileStatus::Allocate(OperandType handType, uint64_t &vtag, uint32_t size, bool isZero)
{
    if (freeMapQEntryNum[handType] == 0) {
        return false;
    }
    if (isZero) {
        return AllocateZero(handType, vtag, size);
    }
    uint32_t ptag = AllocateFreeEntry(handType, size);
    if (ptag == -1U) {
        return false;
    }
    vtag = GetFreeTileVtag(handType);
    ASSERT(!mapQEntry[handType][vtag].vld);
    auto &e = mapQEntry[handType][vtag];
    e.Reset();
    e.vld = true;
    e.vtag = vtag;
    e.ptag = ptag;
    e.addr = ptag * elementSize;
    e.size = size;
    e.occupiedTagNum = freeListEntry[ptag].occupiedSize;
    occupiedSize[handType] += e.occupiedTagNum * elementSize;
    e.handType = handType;
    if (!OperandTypeIsMcallTile(handType)) {
        allocPtr[handType] = (allocPtr[handType] + 1) % mapQEntry[handType].size();
    }
    ASSERT(freeMapQEntryNum[handType] > 0);
    --freeMapQEntryNum[handType];
    LOG_INFO_M(Unit::BCC, Stage::NA) << dec << "Tile allocate type:" << GetTileTypeStr(handType)
                                     << " vtag:" << vtag << " ptag:" << ptag;
    return true;
}

bool SharedTileStatus::AllocateZero(OperandType handType, uint64_t &vtag, uint32_t size)
{
    uint32_t ptag = -1U;
    vtag = GetFreeTileVtag(handType);
    ASSERT(!mapQEntry[handType][vtag].vld);
    auto &e = mapQEntry[handType][vtag];
    e.Reset();
    e.vld = true;
    e.vtag = vtag;
    e.ptag = ptag;
    e.addr = -1ULL;
    e.size = size;
    e.ready = true;
    e.occupiedTagNum = 0;
    occupiedSize[handType] += e.occupiedTagNum * elementSize;
    e.handType = handType;
    if (!OperandTypeIsMcallTile(handType)) {
        allocPtr[handType] = (allocPtr[handType] + 1) % mapQEntry[handType].size();
    }
    ASSERT(freeMapQEntryNum[handType] > 0);
    --freeMapQEntryNum[handType];
    LOG_INFO_M(Unit::BCC, Stage::NA) << dec << "Tile allocate type:" << GetTileTypeStr(handType)
                                     << " vtag:" << vtag << " ptag:" << ptag;
    return true;
}

TileStatus& TileStatusGroup::GetTileStatus(uint64_t tag)
{
    return entry[tag];
}

void TileStatusGroup::Wakeup(TileOperandPtr tileOpd)
{
    entry[tileOpd->tileTag].ready = true;
}

void TileStatusGroup::TryDealloc()
{
    while (entry[deallocPtr].allocated && entry[deallocPtr].canDealloc) {
        currSize = currSize - entry[deallocPtr].size;
        auto swimLog = top->GetSim()->GetSwimLogger();
        if (swimLog != nullptr) {
            uint64_t tid = top->top->machineId + top->tileSwimIdMap.at(handType);
            swimLog->AddCounterEvent(top->top->machineId, tid,
                                     TraceLog::CounterType::QUEUE_POP,
                                     entry[deallocPtr].size);
            swimLog->AddCounterEvent(top->top->machineId, top->top->machineId + top->totalTileSwimId,
                                     TraceLog::CounterType::QUEUE_POP,
                                     entry[deallocPtr].size);
        }
        usingTagNum--;
        entry[deallocPtr].Reset();
        retiredTagQ.push_back(deallocPtr);
        deallocPtr = (deallocPtr + 1) % entryNum;
    }
}

void TileStatusGroup::CheckRelativeIndex()
{
    size_t delta = 0;
    if (retiredPtr >= deallocPtr) {
        delta = retiredPtr - deallocPtr;
    } else {
        delta = retiredPtr + entryNum - deallocPtr;
    }
    // TileTag out of relative index range, set to be released.
    uint64_t index = deallocPtr;
    while (delta > relativeDistance) {
        entry[index].canDealloc = true;
        index = (index + 1) % entryNum;
        delta--;
    }
}

void TileStatusGroup::ReportTileNotReuse(TileOperandPtr opd)
{
    entry[opd->tileTag].canDealloc = true;
    TryDealloc();
}

void SharedTileStatus::ReportKill(TileOperandPtr src)
{
    auto &e = mapQEntry[src->handType][src->tileTag];
    if (!e.vld || e.kill || !e.moveInfoQ.empty()) {
        return;
    }
    e.kill = true;
    FreeMultiFreeEntry(e.ptag, e.occupiedTagNum);
    if (freeListEntry[e.ptag].free) {
        occupiedSize[src->handType] -= e.occupiedTagNum * elementSize;
        auto swimLog = top->GetSim()->GetSwimLogger();
        if (swimLog != nullptr) {
            uint64_t tid = top->top->machineId + top->tileSwimIdMap.at(src->handType);
            swimLog->AddCounterEvent(top->top->machineId, tid,
                                     TraceLog::CounterType::QUEUE_POP,
                                     e.occupiedTagNum * elementSize);
            swimLog->AddCounterEvent(top->top->machineId, top->top->machineId + top->totalTileSwimId,
                                     TraceLog::CounterType::QUEUE_POP,
                                     e.occupiedTagNum * elementSize);
        }
    }
}

void TileStatusGroup::ReportDstRetired(TileOperandPtr opd)
{
    ASSERT(retiredPtr == opd->tileTag);
    retiredPtr = (retiredPtr + 1) % entryNum;
    CheckRelativeIndex();
    TryDealloc();
}

string SharedMapQStatus::Dump() const
{
    std::stringstream os;
    os << "Entry ";
    if (vld) {
        os << dec << "vtag:" << vtag << " ptag:" << ptag
           << dec << " occuiped size:" << occupiedTagNum
           << hex << " addr:" << addr
           << dec << " size:" << size
           << " hand:" << GetTileTypeStr(handType)
           << " retired:" << retired
           << " ready:" << ready
           << " kill:" << kill;
    } else {
        os << "Invalid";
    }
    return os.str();
}

uint64_t SharedTileStatus::GetTotal() const
{
    return freeListEntry.size() * elementSize;
}

void SharedTileStatus::Dump()
{
    for (auto &mapQ : mapQEntry) {
        cout << "hand " << GetTileTypeStr(mapQ.first)
             << dec << " deallocPtr:" << deallocPtr[mapQ.first]
             << dec << " retirePtr:" << retirePtr[mapQ.first]
             << dec << " lastAllocPtr:" << lastAllocPtr[mapQ.first]
             << dec << " allocPtr:" << allocPtr[mapQ.first]
             << " size " << freeMapQEntryNum[mapQ.first] << endl;
        for (uint32_t i = 0; i < mapQ.second.size(); ++i) {
            uint32_t tag = (deallocPtr[mapQ.first] + i) % mapQ.second.size();
            if (mapQ.second[tag].vld) {
                cout << dec << " Index " << tag << " " << mapQ.second[tag].Dump() << endl;
            }
        }
    }
    uint64_t tileRegSize = top->GetSim()->core->tileUtils.configs.tile_reg_size;

    cout << "tileRegSize: " << tileRegSize << endl;
    cout << "totalTileSize: " << GetTotal() << endl;
    for (auto &mem : occupiedSize) {
        cout << GetTileTypeStr(mem.first) << " used size: " << dec << mem.second << endl;
    }
}

void SharedTileStatus::ReportRetire(TileOperandPtr dst)
{
    if (OperandTypeIsMcallTile(dst->handType)) {
        return;
    }
    auto &rPtr = retirePtr[dst->handType];
    ASSERT(dst->tileTag == rPtr);
    ASSERT(mapQEntry[dst->handType][rPtr].vld);
    mapQEntry[dst->handType][rPtr].retired = true;
    LOG_INFO_M(Unit::BCC, Stage::NA) << "retire " << mapQEntry[dst->handType][rPtr].producerCmd->Dump();
    rPtr = (rPtr + 1) % mapQEntry[dst->handType].size();
    auto &dPtr = deallocPtr[dst->handType];
    uint32_t retiredNum = dPtr <= rPtr ? rPtr - dPtr : mapQEntry[dst->handType].size() - dPtr + rPtr;
    uint32_t relativeDistance = (dst->handType != OperandType::OPD_TILE_STACK) ? RELATIVE_DISTANCE : 1U;
    if (retiredNum > relativeDistance) {
        auto &e = mapQEntry[dst->handType][dPtr];
        ASSERT(e.vld);
        FreeMapQEntry(dst->handType, dPtr);
        retiredTagQMap[dst->handType].push_back(dPtr);
        dPtr = (dPtr + 1) % mapQEntry[dst->handType].size();
    }
}

void SharedTileStatus::FreeMapQEntry(OperandType handType, uint32_t vtag)
{
    auto &e = mapQEntry[handType][vtag];
    ASSERT(e.vld);
    LOG_INFO_M(Unit::BCC, Stage::NA) << "Tile mapQ free " << e.producerCmd->Dump()
                                     << " vtag:" << vtag << " ptag:" << e.ptag;
    if (e.copyInfo.vld && mapQEntry[e.copyInfo.handType][e.copyInfo.tag].vld
        && mapQEntry[e.copyInfo.handType][e.copyInfo.tag].ptag == e.ptag) {
        auto &srcEntry = mapQEntry[e.copyInfo.handType][e.copyInfo.tag];
        for (auto it = srcEntry.moveInfoQ.cbegin(); it != srcEntry.moveInfoQ.cend();) {
            if (it->tag == vtag && handType == it->handType) {
                it = srcEntry.moveInfoQ.erase(it);
                break;
            }
            ++it;
        }
    }
    if (!e.kill && !e.isZero) {
        FreeMultiFreeEntry(e.ptag, e.occupiedTagNum);
        if (freeListEntry[e.ptag].free) {
            occupiedSize[handType] -= e.occupiedTagNum * elementSize;
            auto swimLog = top->GetSim()->GetSwimLogger();
            if (swimLog != nullptr) {
                uint64_t tid = top->top->machineId + top->tileSwimIdMap.at(handType);
                swimLog->AddCounterEvent(top->top->machineId, tid,
                                         TraceLog::CounterType::QUEUE_POP,
                                         e.occupiedTagNum * elementSize);
                swimLog->AddCounterEvent(top->top->machineId, top->top->machineId + top->totalTileSwimId,
                                         TraceLog::CounterType::QUEUE_POP,
                                         e.occupiedTagNum * elementSize);
            }
        }
    }
    e.Reset();
    freeMapQEntryNum[handType]++;
    ASSERT(freeMapQEntryNum[handType] <= mapQEntry[handType].size());
}

void TileStatusGroup::GatherStats()
{
    top->top->stats->tileregTagUsage[TileType2Idx(handType)] += usingTagNum;
    top->top->stats->tileRenAllocSize[TileType2Idx(handType)] += currSize;
    if (currSize > top->top->stats->tileRenMaxAllocSize[TileType2Idx(handType)]) {
        top->top->stats->tileRenMaxAllocSize[TileType2Idx(handType)] = currSize;
    }
    ++top->top->stats->tileregTagUsageHistogram[TileType2Idx(handType)][usingTagNum];
}

void SharedTileStatus::GatherStats()
{
    auto &stats = top->top->stats;
    for (auto &mem : mapQEntry) {
        size_t idx = TileType2Idx(mem.first);
        stats->tileregTagUsage[idx] += mem.second.size() - freeMapQEntryNum[mem.first];
        stats->tileRenAllocSize[idx] += occupiedSize[mem.first];
        if (occupiedSize[mem.first] > stats->tileRenMaxAllocSize[idx]) {
            stats->tileRenMaxAllocSize[idx] = occupiedSize[mem.first];
        }
        ++stats->tileregTagUsageHistogram[idx][mem.second.size() - freeMapQEntryNum[mem.first]];
    }
}

void PartitionMapQ::Build(uint32_t size)
{
    entry = vector<SharedMapQStatus>(size, SharedMapQStatus());
}

void SharedTileStatus::Reset()
{
    for (auto &e : freeListEntry) {
        e.Reset();
    }
    freeTagNum = maxFreeTagNum;
    for (auto &mem : mapQEntry) {
        for (auto &e : mem.second) {
            e.Reset();
        }
        freeMapQEntryNum[mem.first] = mem.second.size();
        deallocPtr[mem.first] = 0;
        retirePtr[mem.first] = 0;
        allocPtr[mem.first] = 0;
    }
}

void SharedTileStatus::BuildCapacity(uint64_t totalSize, uint64_t elem)
{
    ASSERT(totalSize % elem == 0);
    elementSize = elem;
    freeListEntry.resize(totalSize / elem);
}

void SharedTileStatus::BuildTilePartion(map<OperandType, uint32_t> tileHandlePerMap, uint32_t mapQSize)
{
    uint32_t perPtr = 0;
    for (auto &e : tileHandlePerMap) {
        // build partition free entry border
        freeTagNum[e.first] = freeListEntry.size() * e.second / NUM_100;
        maxFreeTagNum[e.first] = freeListEntry.size() * e.second / NUM_100;
        partitionSPtr[e.first] = freeListEntry.size() * perPtr / NUM_100;
        partitionEPtr[e.first] = partitionSPtr[e.first] + freeTagNum[e.first];
        perPtr += e.second;
        ASSERT(perPtr <= NUM_100);
        // build partition mapQ
        mapQEntry[e.first] = vector<SharedMapQStatus>(mapQSize, SharedMapQStatus());
        freeMapQEntryNum[e.first] = mapQSize;
        deallocPtr[e.first] = 0;
        retirePtr[e.first] = 0;
        allocPtr[e.first] = 0;
    }
}

void SharedTileStatus::Wakeup(OperandType handType, uint32_t tag)
{
    mapQEntry[handType][tag].ready = true;
}

bool SharedTileStatus::CheckStall(OperandType handType, uint32_t size)
{
    if (freeMapQEntryNum[handType] == 0) {
        return true;
    }
    ASSERT(size != 0);
    uint32_t needTagNum = size / elementSize + (size % elementSize == 0 ? 0 : 1);
    uint32_t totalFreeNum = 0;
    for (auto &mem : freeTagNum) {
        totalFreeNum += mem.second;
    }
    if (needTagNum > totalFreeNum) {
        return true;
    }
    if (needTagNum <= freeTagNum.at(handType)) {
        if (CheckPartitionFree(handType, needTagNum)) {
            return false;
        }
    }
    return !CheckSharedFree(needTagNum);
}

bool SharedTileStatus::CheckPartitionFree(OperandType handType, uint32_t needTagNum)
{
    uint32_t idx = partitionSPtr[handType];
    while (idx + needTagNum < partitionEPtr[handType]) {
        uint32_t nextIdx = idx;
        bool found = true;
        for (uint32_t i = 0; i < needTagNum; ++i) {
            uint32_t tag = idx + needTagNum - i - 1;
            if (!freeListEntry[tag].free) {
                nextIdx = tag;
                found = false;
                break;
            }
        }
        if (found) {
            return true;
        }
        idx = nextIdx;
        ++idx;
    }
    return false;
}

bool SharedTileStatus::CheckSharedFree(uint32_t needTagNum)
{
    uint32_t idx = freeListEntry.size();
    while (idx >= needTagNum) {
        uint32_t nextIdx = idx;
        bool found = true;
        for (uint32_t tag = idx - needTagNum; tag < idx; ++tag) {
            if (!freeListEntry[tag].free) {
                nextIdx = tag;
                found = false;
                break;
            }
        }
        if (found) {
            return true;
        }
        if (nextIdx == 0) {
            break;
        }
        idx = nextIdx;
    }
    return false;
}

uint32_t SharedTileStatus::AllocateFreeEntry(OperandType handType, uint32_t size)
{
    ASSERT(size != 0);
    uint32_t needTagNum = size / elementSize + (size % elementSize == 0 ? 0 : 1);
    if (needTagNum <= freeTagNum.at(handType)) {
        uint32_t ptag = AllocatePartionTag(handType, needTagNum);
        if (ptag != -1U) {
            return ptag;
        }
    }
    return AllocateSharedTag(needTagNum);
}

uint32_t SharedTileStatus::AllocatePartionTag(OperandType handType, uint32_t needTagNum)
{
    uint32_t idx = partitionSPtr[handType];
    while (idx + needTagNum < partitionEPtr[handType]) {
        uint32_t nextIdx = idx;
        bool found = true;
        for (uint32_t i = 0; i < needTagNum; ++i) {
            uint32_t tag = idx + needTagNum - i - 1;
            if (!freeListEntry[tag].free) {
                nextIdx = tag;
                found = false;
                break;
            }
        }
        if (found) {
            OccupyMultiFreeEntry(idx, needTagNum);
            return idx;
        }
        idx = nextIdx;
        ++idx;
    }
    return -1U;
}

uint32_t SharedTileStatus::AllocateSharedTag(uint32_t needTagNum)
{
    uint32_t idx = freeListEntry.size();
    while (idx >= needTagNum) {
        uint32_t nextIdx = idx;
        bool found = true;
        for (uint32_t tag = idx - needTagNum; tag < idx; ++tag) {
            if (!freeListEntry[tag].free) {
                nextIdx = tag;
                found = false;
                break;
            }
        }
        if (found) {
            OccupyMultiFreeEntry(idx - needTagNum, needTagNum);
            return idx - needTagNum;
        }
        if (nextIdx == 0) {
            break;
        }
        idx = nextIdx;
    }
    return -1U;
}

void SharedTileStatus::OccupyMultiFreeEntry(uint32_t tag, uint32_t size)
{
    for (uint32_t idx = tag; idx < tag + size; ++idx) {
        OccupySingleFreeEntry(idx);
        freeListEntry[idx].occupiedSize = size;
    }
}

void SharedTileStatus::OccupySingleFreeEntry(uint32_t tag)
{
    freeListEntry[tag].free = false;
    freeListEntry[tag].ready = false;
    freeListEntry[tag].producerCnt = 1;
    for (auto &mem : partitionSPtr) {
        if (tag >= mem.second && tag < partitionEPtr.at(mem.first)) {
            --freeTagNum[mem.first];
            break;
        }
    }
}

void SharedTileStatus::IncFreeEntryDstUseCnt(uint32_t tag, uint32_t size)
{
    for (uint32_t idx = tag; idx < tag + size; ++idx) {
        freeListEntry[idx].producerCnt++;
    }
}

void SharedTileStatus::FreeMultiFreeEntry(uint32_t tag, uint32_t size)
{
    for (uint32_t idx = tag; idx < tag + size; ++idx) {
        FreeSingleFreeEntry(idx);
    }
}

void SharedTileStatus::FreeSingleFreeEntry(uint32_t tag)
{
    ASSERT(freeListEntry[tag].producerCnt > 0);
    --freeListEntry[tag].producerCnt;
    if (!freeListEntry[tag].free && freeListEntry[tag].producerCnt == 0) {
        freeListEntry[tag].free = true;
        freeListEntry[tag].ready = false;
        freeListEntry[tag].occupiedSize = 1;
        for (auto &mem : partitionSPtr) {
            if (tag >= mem.second && tag < partitionEPtr.at(mem.first)) {
                ++freeTagNum[mem.first];
                break;
            }
        }
    }
}

BIssueQ::BIssueQ(BIQType t, uint64_t depth, uint32_t scalarThreads)
{
    type = t;
    entry.resize(depth, nullptr);
    blockMap.resize(scalarThreads);
}

void BIssueQ::Wakeup(BlockCommandPtr cmd, TileOperandPtr tileOpd)
{
    if (cmd) {
        LOG_INFO_M(Unit::BCC, Stage::NA) << "Wakeup by cmd. " << cmd->Dump();
    }
    if (!tileOpd->vld || !tileOpd->renamed) {
        return;
    }

    for (uint64_t i = 0; i < size; i++) {
        entry[i]->Wakeup(cmd, tileOpd);
        if (entry[i]->Ready()) {
            LOG_INFO_M(Unit::BCC, Stage::NA) << "Cmd is ready. " << entry[i]->Dump();
        }
    }
}

void BIssueQ::FlagDoubleOutput(TileOperandPtr tileOpd)
{
    for (uint64_t i = 0; i < size; i++) {
        entry[i]->FlagDoubleOutput(tileOpd);
    }
}

bool BIssueQ::GetCHandId(BlockCommandPtr cmd)
{
    auto& cubeCreditVector = cHandIDAlloc->cubeCreditVec;
    auto sortFun = [this](const CubeCredit a, const CubeCredit b) {
        return a.issuedCubeCmd < b.issuedCubeCmd;
    };
    std::sort(cubeCreditVector.begin(), cubeCreditVector.end(), sortFun);
    if (TileOpAllocACC(cmd->tileOp)) {
        return GetCHandIdAlloc(cmd);
    } else {
        return GetCHandIdDep(cmd);
    }
    return false;
}

bool BIssueQ::GetCHandIdAlloc(BlockCommandPtr cmd)
{
    CHandAllocInfoPtr res = nullptr;
    uint64_t dimM = static_cast<uint64_t>(ceil(static_cast<float>(cmd->lb0) / top->cubeConfig.m_width));
    uint64_t dimN = static_cast<uint64_t>(ceil(static_cast<float>(cmd->lb1) / top->cubeConfig.n_width));
    if (top->cubeConfig.l0b_buffer_entry_width == 2048) {
        dimN *= 2;
    }
    auto& blockDispatchQue = top->blockIssueQueueUnit.blockDispatchQ;
    auto& cubeCreditVector = cHandIDAlloc->cubeCreditVec;
    bool find = false;
    for (size_t i = 0; i < cubeCreditVector.size(); i++) {
        uint64_t peId = cubeCreditVector[i].peId;
        res = cHandIDAlloc->LookUp(cmd->accChainId, peId, cmd->stid);
        find = (res != nullptr);
        uint64_t dispQSize = blockDispatchQue[BIQType::CUBE_IQ][peId]->Size() +
                             blockDispatchQue[BIQType::CUBE_IQ][peId]->SizeW();
        bool canDisp = (dispQSize <= top->configs.dispatch_queue_depth);
        if (res != nullptr) {
            if (cHandIDAlloc->Alloc(dimM * dimN, cmd->accChainId, res, peId, cmd->stid)) {
                cubeCreditVector[i].issuedCubeCmd++;
                cmd->peid = peId;
                return true && canDisp;
            } else {
                return false && canDisp;
            }
        }
    }
    if (!find) {
        for (size_t i = 0; i < cubeCreditVector.size(); i++) {
            uint64_t peId = cubeCreditVector[i].peId;
            uint64_t dispQSize = blockDispatchQue[BIQType::CUBE_IQ][peId]->Size() +
                                 blockDispatchQue[BIQType::CUBE_IQ][peId]->SizeW();
            if (dispQSize >= top->configs.dispatch_queue_depth) {
                continue;
            }
            if (cHandIDAlloc->Alloc(dimM * dimN, cmd->accChainId, res, peId, cmd->stid)) {
                cubeCreditVector[i].issuedCubeCmd++;
                cmd->peid = peId;
                return true;
            }
        }
    }
    return false;
}

bool BIssueQ::GetCHandIdDep(BlockCommandPtr cmd)
{
    CHandAllocInfoPtr res = nullptr;
    auto& blockDispatchQue = top->blockIssueQueueUnit.blockDispatchQ;
    auto& cubeCreditVector = cHandIDAlloc->cubeCreditVec;
    for (size_t i = 0; i < cubeCreditVector.size(); i++) {
        uint64_t peId = cubeCreditVector[i].peId;
        uint64_t dispQSize = blockDispatchQue[BIQType::CUBE_IQ][peId]->Size() +
                             blockDispatchQue[BIQType::CUBE_IQ][peId]->SizeW();
        if (dispQSize >= top->configs.dispatch_queue_depth) {
            LOG_INFO_M(Unit::BCC, Stage::NA) << "GetCHandIdDep dispQSize: " << dec
                                             << dispQSize << " top->configs.dispatch_queue_depth: "
                                             << top->configs.dispatch_queue_depth;
            continue;
        }
        ASSERT(cmd->stid != -1U);
        res = cHandIDAlloc->LookUp(cmd->accChainId, peId, cmd->stid);
        if (res != nullptr) {
            if (ReleaseACCTag(cmd->tileOp)) {
                cHandIDAlloc->Free(res, peId, cmd->stid);
                cHandIDAlloc->chainAllocEntryNum[peId][cmd->stid].erase(cmd->accChainId);
            }
            cubeCreditVector[i].issuedCubeCmd++;
            cmd->peid = peId;
            return true;
        }
        LOG_INFO_M(Unit::BCC, Stage::NA) << "GetCHandIdDep cHandIDAlloc->LookUp error cmd: " << cmd->Dump();
    }
    return false;
}

BlockCommandPtr BIssueQ::CubePick()
{
    uint64_t index = 0;
    BlockCommandPtr picked = nullptr;
    for (uint64_t i = 0; i < size; i++) {
        if (entry[i]->Ready()) {
            if (!GetCHandId(entry[i])) {
                continue;
            }
            index = i;
            picked = entry[i];
            break;
        }
    }
    if (picked != nullptr) {
        for (uint64_t i = index; i < size - 1; i++) {
            entry[i] = entry[i + 1];
        }
        size--;
        entry[size] = nullptr;
        blockMap[picked->stid].erase(picked->bid.val);
    }
    return picked;
}

BlockCommandPtr BIssueQ::Pick()
{
    if (type == BIQType::CUBE_IQ) {
        return CubePick();
    }
    if (type == BIQType::MCALL_IQ) {
        return MCallPick();
    }
    uint64_t index = 0;
    BlockCommandPtr picked = nullptr;
    for (uint64_t i = 0; i < size; i++) {
        if (entry[i]->Ready()) {
            ROBID nonFlusPtr = top->blockROB.GetNonFlushOldestBid(entry[i]->stid);
            if (entry[i]->tileOp == TileOp::MSCATTER && !LessEqual(entry[i]->bid, nonFlusPtr)) {
                continue;
            }
            index = i;
            picked = entry[i];
            break;
        }
    }
    if (picked != nullptr) {
        for (uint64_t i = index; i < size - 1; i++) {
            entry[i] = entry[i + 1];
        }
        size--;
        entry[size] = nullptr;
        blockMap[picked->stid].erase(picked->bid.val);
    }
    return picked;
}

BlockCommandPtr BIssueQ::MCallPick()
{
    for (uint64_t i = 0; i < size; i++) {
        if (entry[i]->Ready()) {
            return entry[i];
        }
    }
    return nullptr;
}

void BIssueQ::Release(ROBID bid, uint32_t stid)
{
    for (uint64_t i = 0; i < size; i++) {
        if (entry[i]->bid == bid) {
            for (uint64_t j = i; j < size - 1; ++j) {
                entry[j] = entry[j + 1];
            }
            --size;
            entry[size] = nullptr;
            blockMap[stid].erase(bid.val);
        }
    }
}

uint64_t BIssueQ::Size()
{
    return size;
}

bool BIssueQ::Full()
{
    return size >= entry.size();
}

void BIssueQ::Insert(BlockCommandPtr blockCmd)
{
    entry[size] = blockCmd;
    blockMap[blockCmd->stid][blockCmd->bid.val] = blockCmd;
    size++;
}

BlockCommandPtr BIssueQ::Get(ROBID bid, uint32_t stid)
{
    if (blockMap[stid].find(bid.val) == blockMap[stid].end()) {
        return nullptr;
    }
    return blockMap[stid][bid.val];
}

void BIssueQ::Flush(FlushBus &flushReq)
{
    for (BlockCommandPtr &it : entry) {
        if (it == nullptr) {
            continue;
        }

        if (flushReq.req.stid == it->stid && LessEqual(flushReq.req.bid, it->bid)) {
            it = nullptr;
            --size;
        }
    }
}

void BIssueQ::SetAccChainFilter(uint64_t accChainThres)
{
    for (uint64_t i = 0; i < accChainThres; i++) {
        accChainIdFilter[i] = false;
    }
}

bool BlockIssueQueueUnit::CheckBlockCmdStall(BIQType biqType, SimInst inst)
{
    // Block Issue Queue full
    if (inst->opcode == Opcode::OP_BSTART) {
        bool biqFull = false;
        for (auto &biq : bIssueQ) {
            if (biq.type == biqType) {
                biqFull |= biq.Full();
            }
        }
        if (biqFull) {
            top->stats->bisqFullStall++;
            top->stats->smtBisqFullStallArray[inst->stid]++;
            return true;
        }
    }

    // TileReg limit
    OperandType handType = inst->GetDstTileType();
    uint64_t size = inst->GetDstTileSize();
    if (size == 0) {
        return false;
    }
    bool tileRegFull = top->configs.shared_tile_enable && handType != OperandType::OPD_TILE_ACC
                       ? sharedTileStatus[inst->stid].CheckStall(handType, size)
                       : !tileStatus.at(handType).TryAllocateTileAddr(size);
    if (tileRegFull) {
        top->stats->tileregSizeFullStall++;
        top->stats->smtTileregSizeFullStallArray[inst->stid]++;
        lastTileRegFull = true;
    } else {
        lastTileRegFull = false;
    }
    return tileRegFull;
}

bool BlockIssueQueueUnit::CheckBIQStall(BIQType biqType, SimInst inst)
{
    // Block Issue Queue full
    if (inst->opcode == Opcode::OP_BSTART) {
        for (auto &biq : bIssueQ) {
            if (biq.type == biqType) {
                return true;
            }
        }
    }

    return false;
}

bool BlockIssueQueueUnit::CheckTileRegStall(SimInst inst)
{
    // TileReg limit
    OperandType handType = inst->GetDstTileType();
    uint64_t size = inst->GetDstTileSize();
    if (size == 0) {
        return false;
    }
    BlockCommandPtr cmd = top->blockROB.GetLastBlockCMDPtr(inst->bid, inst->stid);
    if (cmd != nullptr) {
        handType = IsBlockTypeMCall(cmd->blockType) && handType == OperandType::OPD_TILE_STACK
                   ? OperandType::OPD_TILE_MCALL_STK : handType;
    } else {
        return false;
    }

    bool tileRegFull = top->configs.shared_tile_enable && handType != OperandType::OPD_TILE_ACC
                       ? sharedTileStatus[inst->stid].CheckStall(handType, size)
                       : !tileStatus.at(handType).TryAllocateTileAddr(size);
    if (tileRegFull && handType != OperandType::OPD_TILE_ACC) {
        if (inst->bid == top->blockROB.getOldestBlockID(inst->stid)) {
            if (top->configs.shared_tile_enable) {
                sharedTileStatus[inst->stid].Dump();
            }
            ASSERT(0 && "Tile register is not enough for the simulator!");
        }
    }
    return tileRegFull;
}

bool BlockIssueQueueUnit::GetTileRegInfo(BlockCommandPtr cmd, SimInst inst)
{
    for (auto &tileSrc : inst->psrcs_) {
        if (!OperandTypeIsTile(tileSrc->type)) {
            continue;
        }
        if (top->configs.shared_tile_enable && tileSrc->type != OperandType::OPD_TILE_ACC) {
            tileSrc->ptag = sharedTileStatus[cmd->stid].GetProducerTag(tileSrc->type, tileSrc->value + 1);
            auto &mapQ = sharedTileStatus[cmd->stid].mapQEntry[tileSrc->type];
            tileSrc->renamed = true;
            tileSrc->ready = mapQ[tileSrc->ptag].ready;
            tileSrc->baseAddr = mapQ[tileSrc->ptag].addr;
            tileSrc->size = mapQ[tileSrc->ptag].size;
            tileSrc->tileInfo = mapQ[tileSrc->ptag].tileInfo;
            // tileSrc->isZero = mapQ[tileSrc->ptag].isZero;
            BlockCommandPtr srcCmd = mapQ[tileSrc->ptag].producerCmd;
            GetSim()->GetViewManager(cmd->stid)->AddBlockSrc(cmd->bid.val, srcCmd->eventId);
            if (top->configs.tile_pre_release) {
                SetConsumerInfo(tileSrc, cmd->bid, cmd->stid);
            }
        } else {
            tileSrc->renamed = true;
            tileSrc->ptag = tileStatus.at(tileSrc->type).GetProducerTag(tileSrc->value + 1);
            tileSrc->ready = tileStatus.at(tileSrc->type).entry[tileSrc->ptag].ready;
            tileSrc->baseAddr = tileStatus.at(tileSrc->type).entry[tileSrc->ptag].addr;
            tileSrc->size = tileStatus.at(tileSrc->type).entry[tileSrc->ptag].size;
            tileSrc->tileInfo = tileStatus.at(tileSrc->type).entry[tileSrc->ptag].tileInfo;
            BlockCommandPtr srcCmd = tileStatus.at(tileSrc->type).entry[tileSrc->ptag].producerCmd;
            GetSim()->GetViewManager(cmd->stid)->AddBlockSrc(cmd->bid.val, srcCmd->eventId);
        }
    }
    uint64_t existDsts = cmd->dsts.size();
    for (uint32_t i = 0; i < inst->pdsts_.size(); i++) {
        auto &tileDst = inst->pdsts_[i];
        if (tileDst->type == OperandType::OPD_TILE_ACC) {
            continue;
        }
        if (tileDst->type == OperandType::OPD_TILE_STACK && IsBlockTypeMCall(cmd->blockType)) {
            continue;
        }
        if (!RenameSingleTileDst(cmd, inst, tileDst, existDsts + i)) {
            return false;
        }
    }
    return true;
}

void BlockIssueQueueUnit::SetConsumerInfo(POperandPtr const& src, ROBID bid, uint32_t stid)
{
    if (top->configs.shared_tile_enable && top->configs.tile_pre_release) {
        auto &e = sharedTileStatus[stid].mapQEntry[src->type][src->ptag];
        if (!src->reuse) {
            e.lastReuse = false;
        }
        e.younerestConsumID = bid;
        if (e.consumList.count(bid) == 0) {
            ++e.usingConsumCnt;
            e.consumList.emplace(bid, false);
        }
    }
}

void BlockIssueQueueUnit::SetTileSrcRslv(BlockCommandPtr const& cmd)
{
    if (!top->configs.shared_tile_enable || !top->configs.tile_pre_release || cmd == nullptr) {
        return;
    }
    for (auto &src : cmd->srcs) {
        if (src->handType != OperandType::OPD_TILE_ACC) {
            auto &e = sharedTileStatus[cmd->stid].mapQEntry[src->handType][src->tileTag];
            if (e.vld && e.consumList.count(cmd->bid) != 0) {
                if (!src->reuse) {
                    e.lastReuse = false;
                }
                if (!e.consumList[cmd->bid]) {
                    ASSERT(e.usingConsumCnt > 0);
                    --e.usingConsumCnt;
                }
                e.consumList[cmd->bid] = true;
            }
        }
    }
}

bool SharedMapQStatus::PreRelease(ROBID nonFlushPtr)
{
    return !lastReuse && LessROBID(younerestConsumID, nonFlushPtr) && usingConsumCnt == 0 && !consumList.empty();
}

uint64_t TileStatusGroup::GetProducerTag(uint32_t link)
{
    ASSERT(entry[(lastAllocPtr - link + entry.size()) % entry.size()].allocated);
    return (lastAllocPtr - link + entry.size()) % entry.size();
}

uint32_t SharedTileStatus::GetProducerTag(OperandType handType, uint32_t link)
{
    ASSERT(handType != OperandType::OPD_INVALID);
    uint32_t tag = (lastAllocPtr[handType] + mapQEntry[handType].size() - link) % mapQEntry[handType].size();
    ASSERT(mapQEntry[handType][tag].vld);
    return tag;
}

bool BlockIssueQueueUnit::RenameSingleTileDst(BlockCommandPtr cmd, SimInst inst, POperandPtr dst,
                                              uint32_t dstLocInBlock)
{
    dst->tileInfo = cmd->GetDstTileInfo(dstLocInBlock);
    if (top->configs.shared_tile_enable && dst->type != OperandType::OPD_TILE_ACC) {
        if (top->configs.tile_move_enable && cmd->IsMoveOp()) {
            if (sharedTileStatus[cmd->stid].MoveTileTag(inst->GetFirstValidTileSrc(), dst)) {
                auto &mapQ = sharedTileStatus[cmd->stid].mapQEntry[dst->type];
                dst->renamed = true;
                dst->baseAddr = mapQ[dst->ptag].addr;
                mapQ[dst->ptag].producer = ConvertOperandToTileOperand(dst, true); // 可能存在其他作用，后续需要确认
                mapQ[dst->ptag].producerCmd = cmd;
                mapQ[dst->ptag].tileInfo = dst->tileInfo;
                LOG_INFO_M(Unit::BCC, Stage::NA) << "Tile rename move " << cmd->Dump();
                return true;
            }
        } else if (sharedTileStatus[cmd->stid].Allocate(dst->type, dst->ptag, dst->size, dst->size == 0)) {
            auto &mapQ = sharedTileStatus[cmd->stid].mapQEntry[dst->type];
            dst->baseAddr = mapQ[dst->ptag].addr;
            dst->renamed = true;
            mapQ[dst->ptag].producer = ConvertOperandToTileOperand(dst, true);
            mapQ[dst->ptag].producerCmd = cmd;
            mapQ[dst->ptag].isZero = dst->size == 0;
            mapQ[dst->ptag].tileInfo = dst->tileInfo;
            auto swimLog = GetSim()->GetSwimLogger();
            if (swimLog != nullptr) {
                uint64_t tid = top->machineId + tileSwimIdMap.at(dst->type);
                uint64_t incSize = mapQ[dst->ptag].occupiedTagNum * sharedTileStatus[cmd->stid].elementSize;
                swimLog->AddCounterEvent(top->machineId, tid, TraceLog::CounterType::QUEUE_PUSH, incSize);
                swimLog->AddCounterEvent(top->machineId, top->machineId + totalTileSwimId,
                                         TraceLog::CounterType::QUEUE_PUSH, incSize);
            }
            LOG_INFO_M(Unit::BCC, Stage::NA) << "Tile rename Allocate " << cmd->Dump();
            return true;
        }
        if (cmd->bid == top->blockROB.getOldestBlockID(cmd->stid)) {
            sharedTileStatus[cmd->stid].Dump();
            ASSERT(0 && "Tile register is not enough for the simulator!");
        }
        return false;
    }
    if (!top->configs.shared_tile_enable
        && tileStatus.at(dst->type).AllocateTileAddr(dst->ptag, dst->size, dst->size == 0)) {
        dst->baseAddr = tileStatus.at(dst->type).GetTileStatus(dst->ptag).addr;
        dst->renamed = true;
        tileStatus.at(dst->type).GetTileStatus(dst->ptag).producer = ConvertOperandToTileOperand(dst, true);
        tileStatus.at(dst->type).GetTileStatus(dst->ptag).producerCmd = cmd;
        tileStatus.at(dst->type).GetTileStatus(dst->ptag).tileInfo = dst->tileInfo;
        auto swimLog = GetSim()->GetSwimLogger();
        if (swimLog != nullptr) {
            uint64_t tid = top->machineId + tileSwimIdMap.at(dst->type);
            swimLog->AddCounterEvent(top->machineId, tid, TraceLog::CounterType::QUEUE_PUSH, dst->size);
            swimLog->AddCounterEvent(top->machineId, top->machineId + totalTileSwimId,
                                     TraceLog::CounterType::QUEUE_PUSH, dst->size);
        }
        return true;
    }
    if (cmd->bid == top->blockROB.getOldestBlockID(cmd->stid)) {
        ASSERT(0 && "Tile register is not enough for the simulator!");
    }
    return false;
}

void BlockIssueQueueUnit::AllocACCDep(BlockCommandPtr cmd)
{
    if (!CubeTileOp(cmd->tileOp)) {
        return;
    }
    uint64_t prevAccChainId = accChainId[cmd->stid];
    uint64_t prevAccPtr = accPtr[cmd->stid];
    cmd->accChainId = accChainId[cmd->stid];
    if (ReleaseACCTag(cmd->tileOp)) {
        accChainId[cmd->stid] = (accChainId[cmd->stid] + 1) % top->configs.parallel_acc_chain_threshold;
    }
    // || cmd->tileOp == TileOp::ACCSCAL
    if (cmd->tileOp == TileOp::ACCCVT || cmd->tileOp == TileOp::TMATMUL_ACC || cmd->tileOp == TileOp::TMATMULMX_ACC) {
        TileOperandPtr tileOpd = std::make_shared<TileOperand>();
        tileOpd->vld = true;
        tileOpd->renamed = true;
        tileOpd->handType = OperandType::OPD_TILE_ACC;
        tileOpd->tileTag = accPtr[cmd->stid];
        tileOpd->ready = tileStatus.at(OperandType::OPD_TILE_ACC).entry[accPtr[cmd->stid]].ready;
        BlockCommandPtr srcCmd = tileStatus.at(OperandType::OPD_TILE_ACC).entry[accPtr[cmd->stid]].producerCmd;
        GetSim()->GetViewManager(cmd->stid)->AddBlockSrc(cmd->bid.val, srcCmd->eventId);
        cmd->srcs.push_back(tileOpd);
    }
    if (ForwardACCFlag(cmd->tileOp)) {
        TileOperandPtr tileOpd = std::make_shared<TileOperand>();
        tileOpd->vld = true;
        tileOpd->isDst = true;
        tileOpd->renamed = true;
        tileOpd->handType = OperandType::OPD_TILE_ACC;
        accPtr[cmd->stid] = (accPtr[cmd->stid] + 1) % top->configs.tile_acc_tag_num;
        tileOpd->tileTag = accPtr[cmd->stid];
        tileStatus.at(OperandType::OPD_TILE_ACC).entry[accPtr[cmd->stid]].Reset();
        tileStatus.at(OperandType::OPD_TILE_ACC).entry[accPtr[cmd->stid]].producerCmd = cmd;
        cmd->dsts.push_back(tileOpd);
    }
    top->blockROB.SetACC(cmd->bid, prevAccChainId, prevAccPtr, accChainId[cmd->stid], accPtr[cmd->stid], cmd->stid);
}

bool BlockIssueQueueUnit::BIQFull(SimInst inst)
{
    if (inst->opcode != Opcode::OP_BSTART) {
        return false;
    }

    for (auto &biq : bIssueQ) {
        if (biq.type == inst->biqType) {
            return biq.Full();
        }
    }
    return false;
}

void BlockIssueQueueUnit::FlagDoubleOutput(std::vector<TileOperandPtr> &srcs)
{
    for (auto &q : bIssueQ) {
        if (q.type != BIQType::CUBE_IQ) {
            continue;
        }

        for (TileOperandPtr &src : srcs) {
            q.FlagDoubleOutput(src);
        }
    }
}

void BlockIssueQueueUnit::InsertBlockCmd()
{
    if (cmdIQBISQ->Empty()) {
        return;
    }
    cmdIQBISQ->unsetStall();
    if (mcallStatus.curMCall.stall) {
        cmdIQBISQ->setStall();
        return;
    }
    SimInst inst = cmdIQBISQ->Front();
    if (inst->biqType == BIQType::BCC_IQ) {
        cmdIQBISQ->Read();
        return;
    }
    bool canPop = true;
    ROBID bid = inst->bid;
    if (inst->opcode == Opcode::OP_BSTART) {
        if (BIQFull(inst)) {
            return;
        }
        BlockCommandPtr biqEntry = top->blockROB.GetBlockCMDPtr(bid, inst->stid);
        biqEntry->eventId = GetSim()->GetEventId();
        if (IsBlockTypeMCall(biqEntry->blockType)) {
            mcallStatus.curMCall.cmd = biqEntry;
        }
        for (auto &biq : bIssueQ) {
            if (biq.type == inst->biqType && biq.Full()) {
                canPop = false;
                break;
            }
            if (biq.type == inst->biqType && !biq.Full()) {
                biq.Insert(biqEntry);
                break;
            }
        }
        if (canPop) {
            cmdIQBISQ->Read();
        }
        return;
    }
    BlockCommandPtr biqEntry = nullptr;
    for (auto &iq : bIssueQ) {
        biqEntry = iq.Get(bid, inst->stid);
        if (biqEntry != nullptr && iq.type == inst->biqType) {
            break;
        }
    }
    if (biqEntry == nullptr) {
        top->blockROB.reportException(bid, inst->stid, "Can not found Block Command in block issue queue");
        return;
    }
    switch (inst->opcode) {
        case Opcode::OP_BSTOP:
            AllocACCDep(biqEntry);
            biqEntry->HandleUInstBSTOP(inst);
            SetMCallStatus(inst);
            IncLastAllocPtr(inst->stid);
            break;
        case Opcode::OP_B_IOR:
            biqEntry->HandleUInstBIOR(inst);
            break;
        case Opcode::OP_B_IOT:
            ASSERT(biqEntry);
            if (!GetTileRegInfo(biqEntry, inst)) {
                canPop = false;
                break;
            }

            biqEntry->HandleUInstBIOT(inst);
            for (auto &tileDst : inst->pdsts_) {
                if (tileDst->renamed) {
                    top->core->tileReg->SetZeroData(tileDst->baseAddr, tileDst->size, inst->stid);
                }
            }

            if (inst->codeLen == EncodeLen::ENL_V) { // 原有添加判断isVec，从指令集定义B.IOT 不可能是64bit 向量指令
                FlagDoubleOutput(biqEntry->srcs);
            }

            LOG_INFO_M(Unit::BCC, Stage::NA) << "handle BIOT " << biqEntry->Dump();
            break;
        case Opcode::OP_B_DIM:
        case Opcode::OP_B_DIMI:
            biqEntry->HandleBDIM(*inst);
            break;
        case Opcode::OP_B_CATR:
            biqEntry->HandleBCATR(*inst);
            break;
        case Opcode::OP_B_DATR:
            biqEntry->HandleBDATR(*inst);
            break;
        case Opcode::OP_B_TEXT:
            biqEntry->HandleBTEXT(*inst);
            break;
        case Opcode::OP_B_HINT_PREFETCH:
            biqEntry->HandleBHINT(*inst);
            break;
        case Opcode::OP_B_IOD:
        default:
            ASSERT(false && "Unsupport B.PARAMETER");
    }
    if (canPop) {
        cmdIQBISQ->Read();
    }
}

void BlockIssueQueueUnit::SetMCallStatus(SimInst const& inst)
{
    if (mcallStatus.curMCall.cmd != nullptr && mcallStatus.curMCall.cmd->bid == inst->bid) {
        mcallStatus.curMCall.vld = true;
        mcallStatus.curMCall.stall = true;
        mcallStatus.curMCall.totalThread = mcallStatus.curMCall.cmd->GetGroupNum(top->core->configs.simtLane);

        GetSim()->GetVerifyManager(inst->stid)->InitPARGroup(mcallStatus.curMCall.cmd->bid.val,
                                                             mcallStatus.curMCall.totalThread);
        GetSim()->GetViewManager(inst->stid)->InitBIQType(mcallStatus.curMCall.cmd->bid.val, BIQType::MCALL_IQ);
        GetSim()->GetViewManager(inst->stid)->InitPARGroup(mcallStatus.curMCall.cmd->bid.val,
                                                           mcallStatus.curMCall.totalThread);
    }
}

void BlockIssueQueueUnit::IncLastAllocPtr(uint32_t stid)
{
    if (top->configs.shared_tile_enable) {
        sharedTileStatus[stid].lastAllocPtr = sharedTileStatus[stid].allocPtr;
    } else {
        tileStatus.at(OperandType::OPD_TILE_TLINK).lastAllocPtr = tileStatus.at(OperandType::OPD_TILE_TLINK).allocPtr;
        tileStatus.at(OperandType::OPD_TILE_ULINK).lastAllocPtr = tileStatus.at(OperandType::OPD_TILE_ULINK).allocPtr;
        tileStatus.at(OperandType::OPD_TILE_MLINK).lastAllocPtr = tileStatus.at(OperandType::OPD_TILE_MLINK).allocPtr;
        tileStatus.at(OperandType::OPD_TILE_NLINK).lastAllocPtr = tileStatus.at(OperandType::OPD_TILE_NLINK).allocPtr;
        tileStatus.at(OperandType::OPD_TILE_STACK).lastAllocPtr = tileStatus.at(OperandType::OPD_TILE_STACK).allocPtr;
    }
}

uint32_t BlockIssueQueueUnit::GetCoreNum() const
{
    uint32_t coreNum = 1;
    return coreNum;
}

void BlockIssueQueueUnit::DispatchBlockCmd()
{
    for (auto& biq : bIssueQ) {
        if (!blockDispatchQ[biq.type][0]) {
            continue;
        }

        uint64_t dispQSize = blockDispatchQ[biq.type][0]->Size() + blockDispatchQ[biq.type][0]->SizeW();
        if (biq.type != BIQType::CUBE_IQ && dispQSize >= top->configs.dispatch_queue_depth) {
            continue;
        }

        uint32_t coreNum = GetCoreNum();
        for (uint32_t i = 0; i < coreNum; ++i) {
            BlockCommandPtr cmd = biq.Pick();
            if (biq.type == BIQType::MCALL_IQ && cmd != nullptr) {
                cmd = TryMCallAlloc(cmd);
            }
            if (cmd == nullptr) {
                break;
            }
            uint32_t peid = 0; // 当前版本下，BISSUE 并不决定发到某个pe，故全部置0
            LOG_INFO_M(Unit::BCC, Stage::NA) << "send blockCMD " << cmd->Dump() << " biq.type: "
                << static_cast<uint32_t>(biq.type);
            if (top->configs.shared_tile_enable && top->configs.tile_move_enable && cmd->IsMoveOp()) {
                WakeupBIssueQEntry(cmd);
                GetSim()->core->bctrl->blockROB.IssueBlock(cmd->bid, 0, cmd->stid);
                GetSim()->core->bctrl->blockROB.completeBlock(cmd->bid, true, cmd->stid);
                GetSim()->GetVerifyManager(cmd->stid)->InitPARGroup(cmd->bid.val, 0);
                GetSim()->GetViewManager(cmd->stid)->InitBIQType(cmd->bid.val, BIQType::TMA_IQ);
                GetSim()->GetViewManager(cmd->stid)->InitPARGroup(cmd->bid.val, 0);
            } else if (top->configs.skip_tma && biq.type == BIQType::TMA_IQ) {
                blockWakeupQ[biq.type]->Write(cmd);
                GetSim()->core->bctrl->blockROB.IssueBlock(cmd->bid, peid, cmd->stid);
                GetSim()->core->bctrl->blockROB.completeBlock(cmd->bid, true, cmd->stid);
            } else if (top->configs.skip_tau && biq.type == BIQType::TAU_IQ) {
                blockWakeupQ[biq.type]->Write(cmd);
                GetSim()->core->bctrl->blockROB.IssueBlock(cmd->bid, peid, cmd->stid);
                GetSim()->core->bctrl->blockROB.completeBlock(cmd->bid, true, cmd->stid);
            } else if (top->configs.skip_vec && biq.type == BIQType::VEC_IQ) {
                blockWakeupQ[biq.type]->Write(cmd);
                GetSim()->core->bctrl->blockROB.IssueBlock(cmd->bid, peid, cmd->stid);
                GetSim()->core->bctrl->blockROB.completeBlock(cmd->bid, true, cmd->stid);
            } else if (top->configs.skip_vec && biq.type == BIQType::VET_IQ) {
                blockWakeupQ[biq.type]->Write(cmd);
                GetSim()->core->bctrl->blockROB.IssueBlock(cmd->bid, peid, cmd->stid);
                GetSim()->core->bctrl->blockROB.completeBlock(cmd->bid, true, cmd->stid);
            } else if (top->configs.skip_cube && biq.type == BIQType::CUBE_IQ) {
                blockWakeupQ[biq.type]->Write(cmd);
                GetSim()->core->bctrl->blockROB.IssueBlock(cmd->bid, peid, cmd->stid);
                GetSim()->core->bctrl->blockROB.completeBlock(cmd->bid, true, cmd->stid);
            } else  {
                GetSim()->core->bctrl->blockROB.IssueBlock(cmd->bid, peid, cmd->stid);
                if (biq.type == BIQType::VEC_IQ && cmd->bTextPC == 0) {
                    blockWakeupQ[biq.type]->Write(cmd);
                    GetSim()->core->bctrl->blockROB.completeBlock(cmd->bid, true, cmd->stid);
                } else {
                    blockDispatchQ[biq.type][cmd->peid]->Write(cmd);
                }
            }
            if (biq.type == BIQType::MCALL_IQ
                && mcallStatus.curMCall.totalThread == mcallStatus.curMCall.renameGidPtr) {
                biq.Release(cmd->bid, cmd->stid);
            }
            if (ForwardACCFlag(cmd->tileOp)) {
                LOG_INFO_M(Unit::BCC, Stage::NA) << "Wakeup by acc cmd. " << cmd->Dump();
                for (TileOperandPtr &dst : cmd->dsts) {
                    biq.Wakeup(cmd, dst);
                    WakeupTile(dst, cmd->stid);
                }
            }
        }
    }
}

BlockCommandPtr BlockIssueQueueUnit::TryMCallAlloc(BlockCommandPtr const& cmd)
{
    if (mcallStatus.groupSchedulerCreit == 0) {
        return nullptr;
    }
    uint32_t stid = cmd->stid;

    // allocate for stack tile
    TileOperandPtr stackDst = cmd->GetTileDstByOperandType(OperandType::OPD_TILE_STACK);
    if (stackDst != nullptr) {
        stackDst = std::make_shared<TileOperand>(*stackDst);
        stackDst->handType = OperandType::OPD_TILE_MCALL_STK;
        stackDst->tileInfo = std::make_shared<TileInfo>();
        stackDst->tileInfo->dataType = DataType::UINT32;
        if (top->configs.shared_tile_enable &&
            sharedTileStatus[stid].Allocate(stackDst->handType, stackDst->tileTag,
                                            stackDst->size, stackDst->size == 0)) {
            auto &mapQ = sharedTileStatus[stid].mapQEntry[stackDst->handType];
            stackDst->baseAddr = mapQ[stackDst->tileTag].addr;
            stackDst->renamed = true;
            mapQ[stackDst->tileTag].producer = stackDst;
            mapQ[stackDst->tileTag].producerCmd = cmd;
            mapQ[stackDst->tileTag].isZero = stackDst->size == 0;
        } else {
            return nullptr;
        }
    }

    // allocate for GPR tile
    TileOperandPtr gprDst = std::make_shared<TileOperand>();
    gprDst->vld = true;
    gprDst->handType = OperandType::OPD_TILE_MCALL_GPR;
    gprDst->bid = cmd->bid;
    gprDst->tileInfo = std::make_shared<TileInfo>();
    gprDst->tileInfo->dataType = DataType::UINT32;
    constexpr uint32_t sgprHandCount = 2;
    constexpr uint32_t distance = 4;
    constexpr uint32_t dSize = 8;
    gprDst->size = GetVREGNum(cmd->vregMode) * top->core->configs.simtLane * dSize;
    gprDst->size += sgprHandCount * distance * dSize;
    if (top->configs.shared_tile_enable
        && sharedTileStatus[stid].Allocate(gprDst->handType, gprDst->tileTag, gprDst->size, gprDst->size == 0)) {
        auto &mapQ = sharedTileStatus[stid].mapQEntry[gprDst->handType];
        gprDst->baseAddr = mapQ[gprDst->tileTag].addr;
        gprDst->renamed = true;
        mapQ[gprDst->tileTag].producer = gprDst;
        mapQ[gprDst->tileTag].producerCmd = cmd;
        mapQ[gprDst->tileTag].isZero = gprDst->size == 0;
    } else {
        if (stackDst != nullptr && top->configs.shared_tile_enable) {
            sharedTileStatus[stid].FreeMapQEntry(stackDst->handType, stackDst->tileTag);
        }
        return nullptr;
    }

    // add swim logger
    if (stackDst != nullptr) {
        auto swimLog = GetSim()->GetSwimLogger();
        if (swimLog != nullptr) {
            auto &mapQ = sharedTileStatus[stid].mapQEntry[stackDst->handType];
            uint64_t tid = top->machineId + tileSwimIdMap.at(stackDst->handType);
            uint64_t incSize = mapQ[stackDst->tileTag].occupiedTagNum * sharedTileStatus[stid].elementSize;
            swimLog->AddCounterEvent(top->machineId, tid, TraceLog::CounterType::QUEUE_PUSH, incSize);
            swimLog->AddCounterEvent(top->machineId, top->machineId + totalTileSwimId,
                                     TraceLog::CounterType::QUEUE_PUSH, incSize);
        }
    }
    if (gprDst != nullptr) {
        auto swimLog = GetSim()->GetSwimLogger();
        if (swimLog != nullptr) {
            auto &mapQ = sharedTileStatus[stid].mapQEntry[gprDst->handType];
            uint64_t tid = top->machineId + tileSwimIdMap.at(gprDst->handType);
            uint64_t incSize = mapQ[gprDst->tileTag].occupiedTagNum * sharedTileStatus[stid].elementSize;
            swimLog->AddCounterEvent(top->machineId, tid, TraceLog::CounterType::QUEUE_PUSH, incSize);
            swimLog->AddCounterEvent(top->machineId, top->machineId + totalTileSwimId,
                                     TraceLog::CounterType::QUEUE_PUSH, incSize);
        }
    }

    // generate new group command
    BlockCommandPtr groupCmd = std::make_shared<BlockCommand>(*cmd);
    groupCmd->dsts.emplace_back(gprDst);
    if (stackDst) {
        groupCmd->SetNewTileDst(stackDst, OperandType::OPD_TILE_STACK);
        top->core->tileReg->SetZeroData(stackDst->baseAddr, stackDst->size, stid);
    }
    top->core->tileReg->SetZeroData(gprDst->baseAddr, gprDst->size, stid);
    groupCmd->gid.val = mcallStatus.curMCall.renameGidPtr;
    ++mcallStatus.curMCall.renameGidPtr;
    --mcallStatus.groupSchedulerCreit;
    LOG_INFO_M(Unit::BCC, Stage::NA) << "TryMCallAlloc " << groupCmd->Dump();
    return groupCmd;
}

void BlockIssueQueueUnit::WakeupTile(TileOperandPtr dst, uint32_t stid)
{
    if (dst->vld && dst->renamed) {
        if (top->configs.shared_tile_enable && dst->handType != OperandType::OPD_TILE_ACC) {
            sharedTileStatus[stid].Wakeup(dst->handType, dst->tileTag);
            WakeupMoveDependency(sharedTileStatus[stid].mapQEntry[dst->handType][dst->tileTag], stid);
        } else {
            tileStatus.at(dst->handType).Wakeup(dst);
        }
    }
}

void BlockIssueQueueUnit::WakeupMoveDependency(SharedMapQStatus e, uint32_t stid)
{
    if (e.vld && !e.moveInfoQ.empty()) {
        for (auto it = e.moveInfoQ.cbegin(); it != e.moveInfoQ.cend(); ++it) {
            auto &dstE = sharedTileStatus[stid].mapQEntry[it->handType][it->tag];
            if (it->vld && dstE.vld && dstE.copyInfo.vld) {
                WakeupBIssueQEntry(dstE.producer, stid);
            }
        }
    }
}

void BlockIssueQueueUnit::WakeupBIssueQEntry(BlockCommandPtr cmd)
{
    for (TileOperandPtr &dst : cmd->dsts) {
        WakeupBIssueQEntry(dst, cmd->stid, cmd);
    }
    if (CubeTileOp(cmd->tileOp)) {
        auto& cubeCreditVec = bIssueQ[0].cHandIDAlloc->cubeCreditVec;
        for (size_t i = 0; i < cubeCreditVec.size(); i++) {
            if (cubeCreditVec[i].peId == cmd->peid) {
                cubeCreditVec[i].issuedCubeCmd--;
            }
        }
    }
}

void BlockIssueQueueUnit::WakeupBIssueQEntry(TileOperandPtr dst, uint32_t stid, BlockCommandPtr cmd)
{
    if (dst->vld && dst->handType != OperandType::OPD_TILE_ACC && dst->renamed) {
        for (auto &biq : bIssueQ) {
            biq.Wakeup(cmd, dst);
        }
        WakeupTile(dst, stid);
    }
}

void BlockIssueQueueUnit::WakeupBlockCmd()
{
    for (auto &wakeupQ : blockWakeupQ) {
        while (wakeupQ.second && !wakeupQ.second->Empty()) {
            BlockCommandPtr cmd = wakeupQ.second->Read();
            LOG_INFO_M(Unit::BCC, Stage::NA) << "Wakeup by cmd: " << cmd->Dump();
            WakeupBIssueQEntry(cmd);
        }
    }
}

void BlockIssueQueueUnit::PrevTileRelease(uint32_t stid)
{
    if (top->configs.shared_tile_enable && top->configs.tile_pre_release) {
        ROBID nonFlusPtr = top->blockROB.GetNonFlushOldestBid(stid);
        sharedTileStatus[stid].PrevTileRetire(nonFlusPtr);
    }
}

void SharedTileStatus::PrevTileRetire(ROBID nonFlushPtr)
{
    for (auto &mem : mapQEntry) {
        auto &mapQ = mem.second;
        uint32_t size = mapQ.size() - freeMapQEntryNum[mem.first];
        for (uint32_t i = 0; i < size; ++i) {
            uint32_t ptr = (deallocPtr[mem.first] + i) % mapQ.size();
            if (!mapQ[ptr].vld) {
                break;
            }
            auto &e = mapQ[ptr];
            if (e.kill || !e.moveInfoQ.empty() || !mapQ[ptr].PreRelease(nonFlushPtr)) {
                continue;
            }
            e.kill = true;
            FreeMultiFreeEntry(e.ptag, e.occupiedTagNum);
            if (!freeListEntry[e.ptag].free) {
                continue;
            }
            occupiedSize[mem.first] -= e.occupiedTagNum * elementSize;
            auto swimLog = top->GetSim()->GetSwimLogger();
            if (swimLog != nullptr) {
                uint64_t tid = top->top->machineId + top->tileSwimIdMap.at(mem.first);
                swimLog->AddCounterEvent(top->top->machineId, tid,
                                         TraceLog::CounterType::QUEUE_POP,
                                         e.occupiedTagNum * elementSize);
                swimLog->AddCounterEvent(top->top->machineId, top->top->machineId + top->totalTileSwimId,
                                         TraceLog::CounterType::QUEUE_POP,
                                         e.occupiedTagNum * elementSize);
            }
        }
    }
}

void BlockIssueQueueUnit::GatherStats(uint32_t stid)
{
    if (top->configs.shared_tile_enable) {
        sharedTileStatus[stid].GatherStats();
    } else {
        tileStatus.at(OperandType::OPD_TILE_TLINK).GatherStats();
        tileStatus.at(OperandType::OPD_TILE_ULINK).GatherStats();
        tileStatus.at(OperandType::OPD_TILE_MLINK).GatherStats();
        tileStatus.at(OperandType::OPD_TILE_NLINK).GatherStats();
        tileStatus.at(OperandType::OPD_TILE_STACK).GatherStats();
    }
    tileStatus.at(OperandType::OPD_TILE_ACC).GatherStats();
}

void BlockIssueQueueUnit::ReportBlockRetired(BlockCommandPtr cmd)
{
    if (cmd == nullptr) {
        return;
    }
    // reuse is false, free
    for (TileOperandPtr &src : cmd->srcs) {
        if (src->handType == OperandType::OPD_TILE_ACC) {
            continue;
        }
        if (!src->reuse) {
            if (top->configs.shared_tile_enable) {
                sharedTileStatus[cmd->stid].ReportKill(src);
            } else {
                tileStatus.at(src->handType).ReportTileNotReuse(src);
            }
        }
    }
    // Out of relative index range, free
    for (TileOperandPtr &dst : cmd->dsts) {
        if (dst->handType == OperandType::OPD_TILE_ACC) {
            continue;
        }
        if (top->configs.shared_tile_enable) {
            if (!(dst->handType == OperandType::OPD_TILE_STACK && cmd->biqType == BIQType::MCALL_IQ)) {
                sharedTileStatus[cmd->stid].ReportRetire(dst);
            }
        } else {
            tileStatus.at(dst->handType).ReportDstRetired(dst);
        }
        stashRetireQ->Write(dst);
    }
    if (top->configs.shared_tile_enable) {
        for (auto &mem : sharedTileStatus[cmd->stid].retiredTagQMap) {
            while (!mem.second.empty()) {
                TileOperandPtr operand = make_shared<TileOperand>();
                operand->vld = true;
                operand->handType = mem.first;
                operand->tileTag = mem.second.front();
                mem.second.pop_front();
                stashFreeQ->Write(operand);
            }
        }
    } else {
        for (auto &mem : tileStatus) {
            while (!mem.second.retiredTagQ.empty()) {
                TileOperandPtr operand = make_shared<TileOperand>();
                operand->vld = true;
                operand->handType = mem.second.handType;
                operand->tileTag = mem.second.retiredTagQ.front();
                mem.second.retiredTagQ.pop_front();
                stashFreeQ->Write(operand);
            }
        }
    }
}

void BlockIssueQueueUnit::RetireMCallGroup()
{
    while (!schedulerBCCRslvQ->Empty()) {
        LOG_INFO_M(Unit::BCC, Stage::NA) << "RetireMCallGroup";
        BlockCommandPtr cmd = schedulerBCCRslvQ->Read();
        LOG_INFO_M(Unit::BCC, Stage::NA) << "cmd dump" << cmd->Dump();
        for (auto dst : cmd->dsts) {
            if (OperandTypeIsMcallTile(dst->handType)) {
                MCallTileRelese(dst, cmd->stid);
            }
        }
        ReleaseMCallStatus(cmd->bid);
    }
}

void BlockIssueQueueUnit::MCallTileRelese(TileOperandPtr const& dst, uint32_t stid)
{
    if (top->configs.shared_tile_enable) {
        sharedTileStatus[stid].FreeMapQEntry(dst->handType, dst->tileTag);
    }
}

void BlockIssueQueueUnit::ReleaseMCallStatus(ROBID bid)
{
    if (mcallStatus.curMCall.vld && mcallStatus.curMCall.cmd->bid == bid) {
        ++mcallStatus.groupSchedulerCreit;
        ASSERT(mcallStatus.groupSchedulerCreit <= mcallStatus.maxGroupSchedulerCreit);
        ++mcallStatus.curMCall.retiredGroupCnt;
        if (mcallStatus.curMCall.retiredGroupCnt == mcallStatus.curMCall.totalThread) {
            blockWakeupQ[BIQType::MCALL_IQ]->Write(mcallStatus.curMCall.cmd);
            top->blockROB.completeBlock(mcallStatus.curMCall.cmd->bid, false, mcallStatus.curMCall.cmd->stid);
            LOG_INFO_M(Unit::BCC, Stage::NA) << "[Group Scheduler] : Completed Block " << mcallStatus.curMCall.cmd->bid;
            mcallStatus.Reset();
        }
    }
}

bool CHandIDAllocator::Alloc(uint64_t num, uint64_t accChainId, CHandAllocInfoPtr& info, uint64_t peId, uint32_t stid)
{
    if (chainAllocEntryNum[peId][stid].find(accChainId) != chainAllocEntryNum[peId][stid].end()) {
        usingSize[peId][stid] -= chainAllocEntryNum[peId][stid][accChainId];
        chainAllocEntryNum[peId][stid][accChainId] = 0;
    }
    if (usingSize[peId][stid] + num > totalSize[peId][stid]) {
        return false;
    }
    uint64_t s = allocPtr[peId][stid];
    uint64_t e = (allocPtr[peId][stid] + num - 1) % totalSize[peId][stid];
    allocPtr[peId][stid] = (allocPtr[peId][stid] + num) % totalSize[peId][stid];
    CHandAllocInfoPtr res = std::make_shared<CHandAllocInfo>();
    res->accChainId = accChainId;
    res->startHandId = s;
    res->endHandId = e;
    res->num = num;
    allocated[peId][stid][accChainId] = res;
    chainAllocEntryNum[peId][stid][accChainId] = num;
    usingSize[peId][stid] += num;
    info = res;
    return true;
}

bool CHandIDAllocator::TryDealloc(CHandAllocInfoPtr info, uint64_t peId, uint32_t stid)
{
    if (deallocPtr[peId][stid] == info->startHandId) {
        usingSize[peId][stid] -= info->num;
        allocated[peId][stid].erase(info->accChainId);
        deallocPtr[peId][stid] = (info->startHandId + info->num) % totalSize[peId][stid];
        return true;
    } else {
        toFree[peId][stid][info->accChainId] = info;
    }
    return false;
}

void CHandIDAllocator::Free(CHandAllocInfoPtr info, uint64_t peId, uint32_t stid)
{
    if (TryDealloc(info, peId, stid)) {
        bool suaccelss = true;
        while (suaccelss) {
            suaccelss = false;
            uint64_t freeChainId = 0;
            for (auto &entry : toFree[peId][stid]) {
                if (TryDealloc(entry.second, peId, stid)) {
                    freeChainId = entry.first;
                    suaccelss = true;
                    break;
                }
            }
            if (suaccelss) {
                toFree[peId][stid].erase(freeChainId);
            }
        }
    }
}

CHandAllocInfoPtr CHandIDAllocator::LookUp(uint64_t accChainId, uint64_t peId, uint32_t stid)
{
    if (allocated[peId][stid].find(accChainId) == allocated[peId][stid].end()) {
        return nullptr;
    }
    return allocated[peId][stid][accChainId];
}

void SharedFreeListStatus::Reset()
{
    free = true;
    ready = false;
    occupiedSize = 1;
}

void SharedTileStatus::Flush(FlushBus &flushReq)
{
    for (auto &mapQ : mapQEntry) {
        uint32_t flushSize = 0;
        for (uint32_t i = 0; i < mapQ.second.size(); ++i) {
            uint32_t ptr = (retirePtr[mapQ.first] + i) % mapQ.second.size();
            if (!OperandTypeIsMcallTile(mapQ.first) && (!mapQ.second[ptr].vld || mapQ.second[ptr].retired)) {
                break;
            }
            if (!mapQ.second[ptr].vld) {
                continue;
            }
            if (LessEqual(flushReq.req.bid, mapQ.second[ptr].producerCmd->bid)) {
                ++flushSize;
                LOG_INFO_M(Unit::BCC, Stage::NA) << "flush ";
                FreeMapQEntry(mapQ.first, ptr);
            } else {
                if (flushSize > 0 && !OperandTypeIsMcallTile(mapQ.first)) {
                    ASSERT(0 && "OOO tile rename is not supported!");
                }
                FlushConsumerInfo(flushReq, mapQ.first, ptr);
            }
        }
        if (!OperandTypeIsMcallTile(mapQ.first)) {
            auto &aPtr = allocPtr[mapQ.first];
            aPtr = (aPtr + mapQ.second.size() - flushSize) % mapQ.second.size();
            if (flushSize > 0) {
                lastAllocPtr[mapQ.first] = aPtr;
            }
        }
    }
}

void SharedTileStatus::FlushConsumerInfo(FlushBus &flushReq, OperandType handType, uint32_t ptr)
{
    auto &e = mapQEntry[handType][ptr];
    if (!e.lastReuse && LessEqual(flushReq.req.bid, e.younerestConsumID)) {
        e.lastReuse = true;
    }
    bool rest = false;
    for (auto it = e.consumList.begin(); it != e.consumList.end();) {
        if (LessEqual(flushReq.req.bid, it->first)) {
            if (!it->second) {
                ASSERT(e.usingConsumCnt > 0);
                --e.usingConsumCnt;
            }
            it = e.consumList.erase(it);
        } else {
            e.younerestConsumID = rest && LessEqual(it->first, e.younerestConsumID) ? e.younerestConsumID : it->first;
            rest = true;
            ++it;
        }
    }
}

bool SharedTileStatus::MoveTileTag(POperandPtr const& src, POperandPtr const& dst)
{
    ASSERT(src != nullptr);
    if (freeMapQEntryNum[dst->type] == 0) {
        return false;
    }
    dst->ptag = allocPtr[dst->type];
    auto &srcEntry = mapQEntry.at(src->type)[src->ptag];
    TcopyTagInfo srcMoveInfo = TcopyTagInfo();
    srcMoveInfo.vld = true;
    srcMoveInfo.handType = dst->type;
    srcMoveInfo.tag = dst->ptag;
    srcEntry.moveInfoQ.push_back(srcMoveInfo);
    auto &dstEntry = mapQEntry.at(dst->type)[dst->ptag];
    dstEntry.Reset();
    dstEntry.vld = true;
    dstEntry.ready = srcEntry.ready;
    dstEntry.addr = srcEntry.addr;
    dstEntry.handType = dst->type;
    dstEntry.vtag = dst->ptag;
    dstEntry.size = dst->size;
    dstEntry.ptag = srcEntry.ptag;
    dstEntry.occupiedTagNum = srcEntry.occupiedTagNum;
    dstEntry.copyInfo.vld = true;
    dstEntry.copyInfo.handType = src->type;
    dstEntry.copyInfo.tag = src->ptag;
    ASSERT(freeMapQEntryNum[dst->type] > 0);
    --freeMapQEntryNum[dst->type];
    allocPtr[dst->type] = (allocPtr[dst->type] + 1) % mapQEntry[dst->type].size();
    IncFreeEntryDstUseCnt(srcEntry.ptag, srcEntry.occupiedTagNum);
    return true;
}

} // namespace JCore
