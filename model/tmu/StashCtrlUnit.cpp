#include "tmu/StashCtrlUnit.h"

namespace JCore {
using namespace std;
inline std::string StashFsmToString(StashFSM fsm)
{
    switch (fsm) {
        case StashFSM::FREE: return "FREE";
        case StashFSM::EXCLUSIVE: return "EXCLUSIVE";
        case StashFSM::SHARED: return "SHARED";
        default: return "UNKNOWN";
    }
}

void StashCtrlUnit::Work()
{
    StashRetireFree();
    SendStashReq();
    RecvStashReq();

    BlockStashCompelte();
    RecvStashResp();
}

void StashCtrlUnit::StashRetireFree()
{
    while (!stashRetireQ->Empty()) {
        TileOperandPtr dst = stashRetireQ->Read();
        if (dst->vld) {
            RetireTileTag(dst->handType, dst->tileTag);
        }
    }

    while (!stashFreeQ->Empty()) {
        TileOperandPtr dst = stashFreeQ->Read();
        if (dst->vld) {
            FreeWayWithTag(dst->handType, dst->tileTag);
        }
    }
}

void StashCtrlUnit::OperandComplete(BlockCommandPtr cmd, TileOperandPtr operand, bool isSrc)
{
    if (!operand->vld) {
        return;
    }
    FreelistLookupInfo info = Lookup(cmd->coreId, operand->handType, operand->tileTag);
    if (info.hit) {
        auto &e = wayFreeLists[cmd->coreId][info.wayId];
        if (e.relateCntMap.count(cmd->bid.val) != 0) {
            if (!isSrc) {
                // block resolve, free 占用的way
                ASSERT(e.fsm == StashFSM::EXCLUSIVE);
                e.fsm = StashFSM::SHARED;
                BlockCommandPtr wakeupCmd = make_shared<BlockCommand>();
                wakeupCmd->tileOp = cmd->tileOp;
                wakeupCmd->bid = cmd->bid;
                wakeupCmd->coreId = cmd->coreId;
                wakeupCmd->dsts.emplace_back(operand);
                stashRslvArray[cmd->coreId]->Write(wakeupCmd);
                AddSwimLane(e.relateCntMap[cmd->bid.val]);
                LOG_INFO_M(Unit::TMU, Stage::STASH) << cmd->Dump()
                                                    << " Tile dest resolve, wakeup way:"
                                                    << operand->wayId;
            }
            e.relateCntMap.erase(cmd->bid.val);
            ASSERT(e.totalCnt > 0);
            --e.totalCnt;
            if (e.totalCnt == 0) {
                ++freeSizes[cmd->coreId];
            }
        }
    }
}

void StashCtrlUnit::BlockStashCompelte()
{
    while (!stashCompQ->Empty()) {
        auto cmd = stashCompQ->Read();
        LOG_INFO_M(Unit::TMU, Stage::STASH) << "Block stash resolve" << cmd->Dump();
        for (auto &src : cmd->srcs) {
            OperandComplete(cmd, src, true);
        }
        for (auto &dst : cmd->dsts) {
            OperandComplete(cmd, dst, false);
        }
    }

    while (!cubeStashCompQ->Empty()) {
        auto cmd = cubeStashCompQ->Read();
        LOG_INFO_M(Unit::TMU, Stage::STASH) << "Block stash resolve" << cmd->Dump();
        for (auto &dst : cmd->dsts) {
            OperandComplete(cmd, dst, false);
        }
    }

    while (!vecliteStashCompQ->Empty()) {
        auto cmd = vecliteStashCompQ->Read();
        LOG_INFO_M(Unit::TMU, Stage::STASH) << "Block stash resolve" << cmd->Dump();
        for (auto &src : cmd->srcs) {
            OperandComplete(cmd, src, true);
        }
        for (auto &dst : cmd->dsts) {
            OperandComplete(cmd, dst, false);
        }
    }
}

void StashCtrlUnit::RecvStashReq()
{
    if (!stashCmdQ->Empty()) {
        auto cmd = stashCmdQ->Front();
        if (!cmd->srcs.empty()) {
            if (LookupAndGenReq(cmd, cmd->srcs[0], true, *stashAllocDoneArray[cmd->coreId])) {
                stashCmdQ->Read();
            }
        } else if (!cmd->dsts.empty()) {
            if (LookupAndGenReq(cmd, cmd->dsts[0], false, *stashAllocDoneArray[cmd->coreId])) {
                stashCmdQ->Read();
            }
        }
    }

    if (!cubeStashCmdQ->Empty()) {
        auto cmd = cubeStashCmdQ->Front();
        if (!cmd->dsts.empty()) {
            if (LookupAndGenReq(cmd, cmd->dsts[0], false, *stashCubeAllocDoneQ)) {
                cubeStashCmdQ->Read();
            }
        }
    }

    if (!vecliteStashCmdQ->Empty()) {
        auto cmd = vecliteStashCmdQ->Front();
        if (!cmd->srcs.empty()) {
            if (LookupAndGenReq(cmd, cmd->srcs[0], true, *stashVecliteAllocDoneArray[cmd->coreId])) {
                vecliteStashCmdQ->Read();
            }
        } else if (!cmd->dsts.empty()) {
            if (LookupAndGenReq(cmd, cmd->dsts[0], false, *stashVecliteAllocDoneArray[cmd->coreId])) {
                vecliteStashCmdQ->Read();
            }
        }
    }
}

const uint32_t StashCtrlUnit::GetMinHops(uint32_t srcNode, uint32_t destNode) const
{
    uint32_t nodesNum = pTileUtils->GetNodeNum();
    uint32_t clockwiseHops = (destNode - srcNode + nodesNum) % nodesNum;
    uint32_t counterClockwiseHops = (srcNode - destNode + nodesNum) % nodesNum;
    uint32_t directionNum = 2;

    if (clockwiseHops < counterClockwiseHops) {
        return clockwiseHops;
    } else if (clockwiseHops == counterClockwiseHops) {
        std::uint32_t randNum = std::rand();
        if (randNum % directionNum == 0) {
            return counterClockwiseHops;
        } else {
            return clockwiseHops;
        }
    } else {
        return counterClockwiseHops;
    }
}

void StashCtrlUnit::GetStashDst(shared_ptr<Request> pkt, uint32_t& dstNodeId) const // 计算当前bank与vector那个load port更近
{
    uint32_t vecReadPortNum = pTileUtils->GetUBConfig()->vec_read_port.size();
    for (uint32_t portNum = 0; portNum < vecReadPortNum; portNum++) {
        uint32_t vecLoadPort = pTileUtils->GetUBConfig()->vec_read_port[portNum];
        if (vecLoadPort == dstNodeId) {
            continue;
        }
        uint32_t tmuNode = pkt->GetArcTgtNode();
        const uint32_t curMinHops = GetMinHops(tmuNode, vecLoadPort);
        const uint32_t globalMinHops = GetMinHops(tmuNode, dstNodeId);
        dstNodeId = curMinHops < globalMinHops ? vecLoadPort : dstNodeId;
        LOG_INFO_M(Unit::TMU, Stage::STASH) << "Calculate dstNodeId, tmuNode: " << tmuNode
                                        << " vecLoadPort: " << vecLoadPort
                                        << " curMinHops: " << curMinHops
                                        << " globalMinHops: " << globalMinHops
                                        << " dstNodeId: " << dstNodeId;
    }
}


bool StashCtrlUnit::LookupAndGenReq(BlockCommandPtr &cmd, TileOperandPtr &operand, bool isSrc,
    SimQueue<BlockCommandPtr> &allocDoneQ)
{
    ctrlStats->tileTagAaccelss++;
    FreelistLookupInfo info = Lookup(cmd->coreId, operand->handType, operand->tileTag);
    // hit older way
    if (info.hit) {
        if (!isSrc) {
            LOG_INFO_M(Unit::TMU, Stage::STASH) << cmd->Dump();
            DumpWayFreeLists(cmd->coreId);
        }
        ASSERT(isSrc);
        cmd->startCalCycle = GetSim()->getCycles();
        cmd->execMachineId = machineId + (cmd->coreId + 1) * info.wayId;
        operand->wayId = info.wayId;
        // record in entry
        WayFreeListInfo &e = wayFreeLists[cmd->coreId][info.wayId];
        if (e.totalCnt == 0) {
            --freeSizes[cmd->coreId];
        }
        if (e.relateCntMap.count(cmd->bid.val) != 0) {
            e.relateCntMap[cmd->bid.val] = cmd;
        } else {
            e.relateCntMap.emplace(cmd->bid.val, cmd);
            ++e.totalCnt;
        }
        if (info.dataFromTA) {
            ctrlStats->hitTaWayCnt++;
        } else {
            ctrlStats->hitToWayCnt++;
        }
        // return way id
        allocDoneQ.Write(cmd);
        // wakeup
        if (info.fsm == StashFSM::SHARED) {
            BlockCommandPtr wakeupCmd = make_shared<BlockCommand>();
            wakeupCmd->tileOp = cmd->tileOp;
            wakeupCmd->bid = cmd->bid;
            wakeupCmd->dsts.emplace_back(operand);
            stashRslvArray[cmd->coreId]->Write(wakeupCmd);
        }
        LOG_INFO_M(Unit::TMU, Stage::STASH) << "Hit stash core:" << cmd->coreId << " way:" << info.wayId << " "
                                            << cmd->Dump() << " origin addr:0x" << hex << operand->baseAddr
                                            << " real addr:0x" << hex << (operand->baseAddr);
        return true;
    }
    // alloacted new way
    auto &freeSize = freeSizes[cmd->coreId];
    if (freeSize > 0) {
        if (cmd->IsVecStash()) {
            operand->wayId = AllocWayId(cmd, operand, isSrc);
        } else {
            operand->wayId = AllocWayId(cmd, operand, true);
        }

        operand->bid = cmd->bid;
        cmd->startCalCycle = GetSim()->getCycles();
        cmd->execMachineId = machineId + (cmd->coreId + 1) * operand->wayId;
        LOG_INFO_M(Unit::TMU, Stage::STASH) << "Allocate core:" << cmd->coreId << " way:" << operand->wayId << " "
                                            << cmd->Dump() << " origin addr:0x" << hex << operand->baseAddr
                                            << " real addr:0x" << hex << (operand->baseAddr);
        if (isSrc) {
            uint32_t nodesNum = pTileUtils->GetNodeNum();
            uint32_t sumStashTimes = 0;
            LOG_INFO_M(Unit::TMU, Stage::STASH) << "Generate stash request for " << cmd->Dump();
            uint64_t base = operand->baseAddr;
            uint32_t totalCnt = operand->size / MAX_TILE_DATA_BYTE;
            uint32_t stashReqNum = totalCnt;
            if (tmuConfigs.tmu_stash_outstanding_opt) {
                stashReqNum = totalCnt / nodesNum >= 1 ? nodesNum : totalCnt % nodesNum;
            }
            cmd->stashCnt = totalCnt;
            for (uint32_t i = 0; i < stashReqNum; ++i) {
                uint64_t addr = base + i * pTileUtils->Bytes2Addr(MAX_TILE_DATA_BYTE);
                auto pkt = stations[bccReadSrcId]->Rxreq(addr, cmd->stid);
                uint32_t reqStashTimes = 1;
                reqStashTimes = ((totalCnt % nodesNum) != 0 && i < (totalCnt % nodesNum))
                                ? totalCnt / nodesNum + 1
                                : totalCnt / nodesNum;
                reqStashTimes = tmuConfigs.tmu_stash_outstanding_opt ? reqStashTimes : 1;
                sumStashTimes += reqStashTimes;
                uint32_t dstNodeId = operand->dstNodeId;
                GetStashDst(pkt, dstNodeId);
                pkt->SetStashDstNode(dstNodeId);
                pkt->SetStashReq(true);
                pkt->SetSize(MAX_TILE_DATA_BYTE);
                pkt->SetBufId(++reqId);
                pkt->SetWayId(operand->wayId);
                pkt->SetCoreId(cmd->coreId);
                pkt->SetStashTimes(reqStashTimes);
                pkt->cmd = cmd;
                stashReqQ.Write(pkt);
                LOG_INFO_M(Unit::TMU, Stage::STASH) << "Generate stash request " << *pkt
                                    << " dst node: " << pkt->GetStashDstNode()
                                    << " count: " << i
                                    << " stash times: " << reqStashTimes
                                    << " totalCnt: " << totalCnt;
            }
            if (totalCnt != sumStashTimes) {
                LOG_INFO_M(Unit::TMU, Stage::STASH) << "stashtimes not correct " << cmd->Dump()
                                                    << " origin addr" << hex << cmd->dsts[0]->baseAddr;
                ASSERT(totalCnt == sumStashTimes);
            }
            ctrlStats->allocTaWayCnt++;
        } else {
            if (cmd->IsVecStash()) {
                toClearArray[cmd->coreId]->Write(operand);
                ctrlStats->allocToWayCnt++;
            }
        }
        allocDoneQ.Write(cmd);
        return true;
    }
    return false;
}

void StashCtrlUnit::SendStashReq()
{
    if (!stashReqQ.Empty() && stations[bccReadSrcId]->SpbHasAvailEntry(JCore::BufferType::LOAD_REQ0)) {
        auto pkt = stashReqQ.Read();
        stations[bccReadSrcId]->WriteSpb(pkt);
        idMap[pkt->GetBufId()] = pkt;
        LOG_INFO_M(Unit::TMU, Stage::STASH) << "Send stash request to ring: " << *pkt << " " << pkt->cmd;
    }
}

void StashCtrlUnit::DumpWayFreeLists(uint32_t coreId)
{
    std::cout << "┌─────┬───────┬──────────┬──────────┬───────────────┬────────────┬───────────┬───────────┐" << endl;
    std::cout << "│Core │ Way   │  FSM     │ isTA     │ Type          │ Tag        │ totalCnt  │ cmdInfo   |" << endl;
    std::cout << "├─────┼───────┼──────────┼──────────┼───────────────┼────────────┼───────────┤───────────|" << endl;

    const auto& wayFreeList = wayFreeLists[coreId];
    for (size_t way = 0; way < wayFreeList.size(); ++way) {
        const auto& info = wayFreeList[way];
        const auto cmdInfo = (info.cmd) ? info.cmd->DumpFormalName() : "null";
        std::cout << "│" << std::setw(5) << coreId
                    << "│" << std::setw(7) << way
                    << "│" << std::setw(10) << StashFsmToString(info.fsm)
                    << "│" << std::setw(10) << (info.isTA ? "true" : "false")
                    << "│" << std::setw(15) << GetOperandType(info.type)
                    << "│ " << std::setw(9) << std::dec << info.tag << std::dec
                    << "│" << std::setw(11) << info.totalCnt
                    << "│" << std::setw(11) << cmdInfo << "│"<< endl;
    }
    std::cout << "└─────┴───────┴──────────┴──────────┴───────────────┴────────────┴───────────┘───────────┘"<< endl;
}

uint32_t StashCtrlUnit::GetAvailWayNum(uint32_t coreId)
{
    uint32_t availNum = 0;
    auto &wayFreeList = wayFreeLists[coreId];
    for (uint32_t i = 0; i < wayFreeList.size(); ++i) {
        auto &e = wayFreeList[i];
        if (e.fsm == StashFSM::FREE) {
            availNum++;
        } else if (e.fsm == StashFSM::SHARED && e.totalCnt == 0) {
            availNum++;
        }
    }
    return availNum;
}

bool StashCtrlUnit::TrackFreeSize(uint32_t freeSize, uint32_t coreId)
{
    uint32_t mapFreeSize = GetAvailWayNum(coreId);
    if (freeSize != mapFreeSize) {
        cout << "freeSize:" << freeSize << ", mapFreeSize:" << mapFreeSize << endl;
        return false;
    }
    return true;
}

uint32_t StashCtrlUnit::AllocWayId(BlockCommandPtr cmd, TileOperandPtr operand, bool isTA)
{
    uint32_t selectedWayId = -1U;
    auto &wayFreeList = wayFreeLists[cmd->coreId];
    for (uint32_t i = 0; i < wayFreeList.size(); ++i) {
        auto &e = wayFreeList[i];
        if (e.fsm == StashFSM::FREE) {
            selectedWayId = i;
            break;
        }
        if (e.fsm == StashFSM::SHARED && e.totalCnt == 0 && selectedWayId == -1U) {
            selectedWayId = i;
        }
    }
    if (selectedWayId != -1U) {
        auto &e = wayFreeList[selectedWayId];
        e.fsm = StashFSM::EXCLUSIVE;
        e.type = operand->handType;
        e.tag = operand->tileTag;
        e.retired = false;
        e.cmd = cmd;
        e.totalCnt = 1;
        e.isTA = isTA;
        e.relateCntMap.emplace(cmd->bid.val, cmd);
        ASSERT(freeSizes[cmd->coreId] > 0);
        --freeSizes[cmd->coreId];
    }
    if (selectedWayId == -1U) {
        DumpWayFreeLists(cmd->coreId);
    }
    ASSERT(selectedWayId != -1U);
    return selectedWayId;
}

void StashCtrlUnit::FreeWayId(uint32_t coreId, uint32_t eid)
{
    auto &e = wayFreeLists[coreId][eid];
    if (e.fsm == StashFSM::FREE) {
        return;
    }
    ASSERT(e.fsm != StashFSM::FREE);
    if (e.totalCnt > 0) {
        ++freeSizes[coreId];
    }
    LOG_INFO_M(Unit::TMU, Stage::STASH) << "free way:" << eid << " cmd: " << e.cmd->Dump();
    e.fsm = StashFSM::FREE;
    e.cmd = nullptr;
    e.isTA = false;
    e.retired = false;
    e.relateCntMap.clear();
    ctrlStats->freeWayCnt++;
    ASSERT(freeSizes[coreId] <= wayFreeLists[coreId].size());
    if (!TrackFreeSize(freeSizes[coreId], coreId)) {
        cout << "free way, wayId:" << eid << endl;
        ASSERT(false);
    }
}

void StashCtrlUnit::RecvStashResp()
{
    if (!stashRespQ.Empty()) {
        auto pkt = stashRespQ.Read();
        if (idMap.count(pkt->GetBufId()) != 0) {
            auto idMapPkt = idMap.at(pkt->GetBufId());

            LOG_INFO_M(Unit::TMU, Stage::STASH) << "Recive stash response: " << *pkt
                                                << " rest count:" << pkt->cmd->stashCnt - 1
                                                << " cmd: " << pkt->cmd->Dump()
                                                << " stashTimes: " << idMapPkt->GetStashTimes()
                                                << " cmd: " << pkt->cmd->Dump();
            idMapPkt->SetStashTimes(idMapPkt->GetStashTimes() - 1);
            pkt->cmd->stashCnt--;
            if (pkt->cmd->stashCnt == 0) {
                BlockCommandPtr wakeupCmd = make_shared<BlockCommand>();
                wakeupCmd->tileOp = pkt->cmd->tileOp;
                wakeupCmd->bid = pkt->cmd->bid;
                wakeupCmd->coreId = pkt->cmd->coreId;
                wakeupCmd->dsts.emplace_back(pkt->cmd->srcs[0]);
                stashRslvArray[pkt->cmd->coreId]->Write(wakeupCmd);
                auto &wayFreeList = wayFreeLists[pkt->cmd->coreId];
                ASSERT(wayFreeList[pkt->cmd->srcs[0]->wayId].fsm == StashFSM::EXCLUSIVE);
                wayFreeList[pkt->cmd->srcs[0]->wayId].fsm = StashFSM::SHARED;
                LOG_INFO_M(Unit::TMU, Stage::STASH) << "Stash resolve and wakeup way:" << pkt->cmd->srcs[0]->wayId
                                                    << ", " << pkt->cmd->Dump();
                AddSwimLane(pkt->cmd);
            }
            if (idMapPkt->GetStashTimes() == 0) {
                LOG_INFO_M(Unit::TMU, Stage::STASH) << "stash finish: " << *pkt
                                                << " stashTimes: " << idMapPkt->GetStashTimes();
                idMap.erase(pkt->GetBufId());
            }
        } else {
            LOG_INFO_M(Unit::TMU, Stage::STASH) << " Error stash response: this stash req has been erase" << *pkt;
            ASSERT(idMap.count(pkt->GetBufId()) == 0);
        }
    }
}

void StashCtrlUnit::Xfer()
{
    stashReqQ.Work();
    stashRespQ.Work();
}

void StashCtrlUnit::Build(TileRegStat* stats)
{
    ctrlStats = stats;
    vecConfigs.overrideDefaultConfig(GetSim()->getCfgs());
    tmuConfigs.overrideDefaultConfig(GetSim()->getCfgs());
    wayFreeLists = vector<vector<WayFreeListInfo>>(coreNum,
                   vector<WayFreeListInfo>(vecConfigs.ta_cache_way, WayFreeListInfo()));
    freeSizes = vector<uint32_t>(coreNum, vecConfigs.ta_cache_way);
    Reset();
}

void StashCtrlUnit::Reset()
{
    for (uint32_t i = 0; i < wayFreeLists.size(); ++i) {
        for (auto &e : wayFreeLists[i]) {
            e.fsm = StashFSM::FREE;
            e.cmd = nullptr;
            e.totalCnt = 0;
            e.relateCntMap.clear();
        }
        freeSizes[i] = vecConfigs.ta_cache_way;
    }
}

void StashCtrlUnit::AddSwimLane(BlockCommandPtr cmd)
{
    SwimLogData logData;
    logData.name = std::to_string(cmd->bid.val) + " " + GetTileOpName(cmd->tileOp);
    logData.pid = CORE_TOP_MACHINE_ID;
    logData.tid = cmd->execMachineId;
    logData.sTime = cmd->startCalCycle;
    logData.eTime = GetSim()->getCycles();
    logData.hint = cmd->DumpSwimInfo(swimLaneOffset);
    cmd->eventId = (cmd->eventId >= 0) ? cmd->eventId : GetSim()->GetEventId();
    logData.eventId = cmd->eventId;
    GetSim()->AddDuration(logData);
}

void StashCtrlUnit::Flush(FlushBus &flushReq)
{
    LOG_DEBUG_M(Unit::TMU, Stage::STASH) << "Recv flush bus " << flushReq;
    auto matchBcmd = [&flushReq] (BlockCommandPtr cmd) -> bool {
        return cmd->stid == flushReq.req.stid && LessEqual(flushReq.req.bid, cmd->bid);
    };
    stashCmdQ->FlushIf(matchBcmd);
    stashCompQ->FlushIf(matchBcmd);
    cubeStashCmdQ->FlushIf(matchBcmd);
    stashCubeAllocDoneQ->FlushIf(matchBcmd);
    cubeStashCompQ->FlushIf(matchBcmd);
    vecliteStashCmdQ->FlushIf(matchBcmd);
    vecliteStashCompQ->FlushIf(matchBcmd);
    for (auto &q : stashAllocDoneArray) {
        q->FlushIf(matchBcmd);
    }
    for (auto &q : stashRslvArray) {
        q->FlushIf(matchBcmd);
    }
    for (uint32_t i = 0; i < wayFreeLists.size(); ++i) {
        for (uint32_t j = 0; j < wayFreeLists[i].size(); ++j) {
            FlushSingleListEntry(flushReq, i, j);
        }
    }

    auto matchPkt = [&flushReq] (shared_ptr<Request> pkt) -> bool {
        return LessEqual(flushReq.req.bid, pkt->cmd->bid);
    };
    stashReqQ.FlushIf(matchPkt);
    stashRespQ.FlushIf(matchPkt);
    for (auto it = idMap.begin(); it != idMap.end();) {
        if (matchPkt(it->second)) {
            it = idMap.erase(it);
        } else {
            ++it;
        }
    }
}

void StashCtrlUnit::FlushSingleListEntry(FlushBus &flushReq, uint32_t coreId, uint32_t eid)
{
    auto matchBcmd = [&flushReq] (BlockCommandPtr cmd) -> bool {
        return LessEqual(flushReq.req.bid, cmd->bid);
    };
    auto &e = wayFreeLists[coreId][eid];
    if (e.fsm == StashFSM::FREE || e.totalCnt == 0) {
        return;
    }
    if (matchBcmd(e.cmd) && !e.cmd->dsts.empty() && !e.retired) {
        LOG_INFO_M(Unit::TMU, Stage::STASH) << "Stash flush free eid:" << eid << " " << e.cmd;
        FreeWayId(coreId, eid);
        ctrlStats->flushWayCnt++;
    }
    for (auto it = e.relateCntMap.cbegin(); it != e.relateCntMap.cend();) {
        if (matchBcmd(it->second)) {
            it = e.relateCntMap.erase(it);
            ASSERT(e.totalCnt > 0);
            --e.totalCnt;
            if (e.totalCnt == 0) {
                ++freeSizes[coreId];
            }
        } else {
            ++it;
        }
    }
}

FreelistLookupInfo StashCtrlUnit::Lookup(uint32_t coreId, OperandType type, uint64_t tag)
{
    FreelistLookupInfo info = FreelistLookupInfo();
    auto &wayFreeList = wayFreeLists[coreId];
    for (uint32_t i = 0; i < wayFreeList.size(); ++i) {
        auto &e = wayFreeList[i];
        if (e.fsm != StashFSM::FREE && e.type == type && e.tag == tag) {
            info.hit = true;
            info.wayId = i;
            info.fsm = e.fsm;
            info.dataFromTA = e.isTA;
            break;
        }
    }
    return info;
}

void StashCtrlUnit::FreeWayWithTag(OperandType type, uint64_t tag)
{
    for (uint32_t i = 0; i < wayFreeLists.size(); ++i) {
        for (uint32_t j = 0; j < wayFreeLists[i].size(); ++j) {
            auto &e = wayFreeLists[i][j];
            if (e.fsm != StashFSM::FREE && e.type == type && e.tag == tag) {
                FreeWayId(i, j);
                break;
            }
        }
    }
}

void StashCtrlUnit::RetireTileTag(OperandType type, uint64_t tag)
{
    for (uint32_t i = 0; i < wayFreeLists.size(); ++i) {
        for (uint32_t j = 0; j < wayFreeLists[i].size(); ++j) {
            auto &e = wayFreeLists[i][j];
            if (e.fsm != StashFSM::FREE && e.type == type && e.tag == tag) {
                e.retired = true;
                break;
            }
        }
    }
}

}
