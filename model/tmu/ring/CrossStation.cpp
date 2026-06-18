#include "tmu/ring/CrossStation.h"

#include <algorithm>
#include <iostream>
#include "core/Core.h"
#include "../configs/core_config.h"
#include "interface/ConstConfig.h"
#include "tmu/ring/Ring.h"

namespace JCore {
using namespace std;
RingState::RingState(uint32_t id) : oriId(id)
{
    // Initialize for all ChannelType values
    channelRequests.resize(static_cast<int>(ChannelType::COUNT));
    nextChannelRequests.resize(static_cast<int>(ChannelType::COUNT));
    requestDownStash.resize(static_cast<int>(ChannelType::COUNT));
    channelLocks.resize(static_cast<int>(ChannelType::COUNT), 0);
}

SimSys* CrossStation::GetSim()
{
    return sim;
}

void CrossStation::Build()
{
    configs.overrideDefaultConfig(GetSim()->getCfgs());
    for (auto &spb : spbArray) {
        spb.InitMaxSize(configs.spb_depth);
    }
    for (auto &mgb : mgbArray) {
        mgb.InitMaxSize(configs.mgb_depth);
    }
    stat.rpt = GetSim()->getRpt();
    stat.id = id;
    constexpr size_t frqNum = 2;
    frq.resize(frqNum);
    toCoreBuff.resize(1);
    vecLoadBeginPortId = configs.vec_read_port[0];
    vecLoadEndPortId = configs.vec_read_port[configs.vec_read_port.size() - 1];
}

void CrossStation::InitCoreBuffer(uint32_t coreNum)
{
    toCoreBuff.resize(coreNum);
}

CrossStation::CrossStation(uint32_t cid)
    : id(cid)
{
    for (size_t i = 0; i < static_cast<size_t>(Direction::RING_NUM); i++) {
        ringState.push_back(RingState(id));
    }

    for (int i = 0; i < static_cast<int>(ChannelType::COUNT); ++i) {
        GetCCRingState().channelLocks[static_cast<int>(i)] = 0;
        GetCWRingState().channelLocks[static_cast<int>(i)] = 0;
    }
    spbArray.resize(static_cast<size_t>(BufferType::COUNT));
    mgbArray.resize(static_cast<size_t>(BufferType::COUNT));
}

void CrossStation::SetVerboseOn()
{
    verbose = true;
}

void CrossStation::ConnectRings(std::shared_ptr<Ring> cRing, std::shared_ptr<Ring> wRing)
{
    ccRing = cRing;
    cwRing = wRing;
}

void CrossStation::ReceiveRequest(const std::shared_ptr<Request>& req)
{
    LOG_INFO_M(Unit::TMU, Stage::MGB) << "Cross station " << std::dec << id << " off Ring ,"
                                      << *req << "allocate mgb ";
    ASSERT (req->GetFlit().tgtId == id);
    TryPutInMGB(req);
}

void CrossStation::Work()
{
    ProcessMGB();
    ProcessSPB();
    CheckStarvation();
    stat.frqReadOccupiedCycle += !frq[0].empty();
    stat.frqReadFullCycle += frq[0].size() >= configs.frq_entry_size;
    stat.frqReadOccupiedSize += frq[0].size();
    stat.frqWriteOccupiedCycle += !frq[1].empty();
    stat.frqWriteFullCycle += frq[1].size() >= configs.frq_entry_size;
    stat.frqWriteOccupiedSize += frq[1].size();
    stat.cycles++;
}

void CrossStation::Xfer()
{
    for (auto &spb : spbArray) {
        spb.Work();
    }
    for (auto &mgb : mgbArray) {
        mgb.Work();
    }
    TransferTilePipe();
    for (size_t i = 0; i < static_cast<size_t>(ChannelType::COUNT); i++) {
        stat.offRingCCBackPressure[i] = ccRing->stat.offRingBackPressure[id][i];
        stat.offRingCWBackPressure[i] = cwRing->stat.offRingBackPressure[id][i];
    }
}

void CrossStation::Reset() {}

bool CrossStation::SpbHasAvailEntry(BufferType type)
{
    return !spbArray[static_cast<size_t>(type)].Full();
}

uint32_t CrossStation::GetSpbAvailableCount(BufferType type)
{
    auto& spb = spbArray[static_cast<size_t>(type)];
    uint32_t currentSize = spb.Size() + spb.SizeW();
    uint32_t maxSize = spb.MaxSize();
    return (currentSize >= maxSize) ? 0 : (maxSize - currentSize);
}

uint32_t CrossStation::GetSpbSize(BufferType type)
{
    return spbArray[static_cast<size_t>(type)].Size() + spbArray[static_cast<size_t>(type)].SizeW();
}

bool CrossStation::SpbHasStashEntry(BufferType type)
{
    auto spbReadQ = spbArray[static_cast<size_t>(type)].GetRawReadData();
    auto spbWriteQ = spbArray[static_cast<size_t>(type)].GetRawWriteData();
    for (auto& req: spbReadQ) {
        if (req->IsStash()) {
            return true;
        }
    }
    for (auto& req: spbWriteQ) {
        if (req->IsStash()) {
            return true;
        }
    }

    return false;
}

bool CrossStation::WriteSpb(const std::shared_ptr<Request>& req)
{
    if (req->IsFromCore()) {
        uint32_t destNode = pTileUtils->GetDstNode(req->GetAddr());
        req->SetTgtNodeId(destNode);
    }
    req->inSpbTime = GetSim()->getCycles();
    spbArray[static_cast<size_t>(req->GetBufferType())].Write(req);
    if (req->IsFromCore()) {
        if (req->IsRead()) {
            stat.ldReqCntOfstart++;
            stat.ldReqCntOfstartSize += req->GetSize();
        } else if (req->IsWrite()) {
            stat.stReqCntOfstart++;
            stat.stReqCntOfstartSize += req->GetSize();
        }
    } else if (req->IsFromTileRegister()) {
        stat.ldRespCntOfStart++;
        stat.ldRespCntOfStartSize += req->GetSize();
    }
    LOG_INFO_M(Unit::TMU, Stage::SPB) << "Cross station " << std::dec << id<< ": write " << *req << " in SPB";
    if (req->GetPEType() == MachineType::CUBE) {
        if (req->GetBufferType() == BufferType::LOAD_REQ0) {
            cwRing->stat.numCubeLoadReqs[id]++;
        } else if (req->GetBufferType() == BufferType::STORE_DATA) {
            cwRing->stat.numCubeStoreReqs[id]++;
        }
    } else if (req->GetPEType() == MachineType::VECTOR) {
        if (req->GetBufferType() == BufferType::LOAD_REQ0) {
            cwRing->stat.numVectorLoadReqs[id]++;
        } else if (req->GetBufferType() == BufferType::STORE_DATA) {
            cwRing->stat.numVectorStoreReqs[id]++;
        }
    } else if (req->GetPEType() == MachineType::TMA) {
        if (req->GetBufferType() == BufferType::LOAD_REQ0) {
            cwRing->stat.numTMALoadReqs[id]++;
        } else if (req->GetBufferType() == BufferType::STORE_DATA) {
            cwRing->stat.numTMAStoreReqs[id]++;
        }
    }
    return true;
}

bool CrossStation::TryPutInMGB(const std::shared_ptr<Request> req)
{
    if (req == nullptr) {
        return false;
    }
    auto &mgb = mgbArray[static_cast<size_t>(req->GetBufferType())];
    if (mgb.Full()) {
        LOG_INFO_M(Unit::TMU, Stage::MGB) << "Cross station " << std::dec << id << " Fail to put " << *req << " in MGB";
        return false; // MGB full
    }

    req->inMgbTime = GetSim()->getCycles();
    mgb.Write(req);
    if (req->IsFromCore()) {
        if (req->IsRead()) {
            stat.ldReqCntOfEnd++;
            stat.ldReqCntOfEndSize += req->GetSize();
        } else if (req->IsWrite()) {
            stat.stReqCntOfEnd++;
            stat.stReqCntOfEndSize += req->GetSize();
        }
    } else if (req->IsFromTileRegister()) {
        stat.ldRespCntOfEnd++;
        stat.ldRespCntOfEndSize += req->GetSize();
    }
    LOG_INFO_M(Unit::TMU, Stage::MGB) << "Cross station " << std::dec << id << " " << *req << " In MGB";
    return true;
}

void CrossStation::ProcessSPB()
{
    std::vector<std::vector<bool>> onRing(static_cast<size_t>(Direction::RING_NUM),
        std::vector<bool>(static_cast<size_t>(ChannelType::COUNT), false));
    std::vector<std::vector<bool>> onRingBp(static_cast<size_t>(Direction::RING_NUM),
        std::vector<bool>(static_cast<size_t>(ChannelType::COUNT), false));
    // FIFO struture, 1 cycle only process 1 request
    for (size_t i = 0; i < spbArray.size() && pTileUtils->IsRingMode(false); i++) {
        // spbArray statistics
        stat.spbOccupiedCycle[i] += !spbArray[i].Empty();
        stat.spbFullCycle[i] += spbArray[i].Full();
        stat.spbOccupiedSize[i] += spbArray[i].Size() + spbArray[i].SizeW();

        // 打印每个SPB的容量信息
        uint32_t currentSize = spbArray[i].Size() + spbArray[i].SizeW();
        uint32_t maxSize = spbArray[i].MaxSize();
        if (currentSize > 0 || spbArray[i].Full()) {
            LOG_INFO_M(Unit::TMU, Stage::SPB) << "[Station " << id << "] SPB[" << i << "] "
                // << "(" << EnumToString(static_cast<BufferType>(i)) << "): "
                << currentSize << "/" << maxSize
                << (spbArray[i].Full() ? " [FULL]" : "");
        }
        uint64_t channelIdx = static_cast<uint64_t>(GetBufTypeCvtMap().at(static_cast<BufferType>(i)));
        if (ringState[static_cast<size_t>(Direction::CLOCKWISE)].channelRequests[channelIdx] != nullptr) {
            stat.ringBusyCW[i]++;
        }
        if (ringState[static_cast<size_t>(Direction::COUNTER_CLOCKWISE)].channelRequests[channelIdx] != nullptr) {
            stat.ringBusyCC[i]++;
        }

        if (spbArray[i].Empty()) {
            continue;
        }

        ProcessSPBArray(i, onRing, onRingBp);
    }
    for (uint32_t i = 0; i < static_cast<size_t>(ChannelType::COUNT); i++) {
        if (onRingBp[static_cast<size_t>(Direction::CLOCKWISE)][i]) {
            stat.onRingCWBackPressure[i] += 1;
        }
        if (onRingBp[static_cast<size_t>(Direction::COUNTER_CLOCKWISE)][i]) {
            stat.onRingCCBackPressure[i] += 1;
        }
    }
}

void CrossStation::ProcessSPBArray(size_t bufferId, std::vector<std::vector<bool>> &onRing,
    std::vector<std::vector<bool>> &onRingBp)
{
    auto &readQ = spbArray[bufferId].GetRawReadData();
    size_t spbDepth = configs.nonblocking_spb_processing ? spbArray[bufferId].Size() : 1;
    for (size_t j = 0; j < spbDepth; ++j) {
        std::shared_ptr<Request> req = readQ[j];
        uint32_t destNode = req->GetArcTgtNode();
        // 确定路由方向
        Direction route = cwRing->DetermineRoute(req->GetArcSrcNode(), destNode);
        req->SetIdealHops(cwRing->DetermineIdealHops(req->GetArcSrcNode(), destNode));
        size_t channel = static_cast<size_t>(req->GetChannel());
        if (onRing[static_cast<size_t>(route)][channel]) {
            continue;
        }
        if (configs.tmu_stash_rand && req->IsStashCompData()) {
            Direction newRoute = route;
            std::uint32_t randNum = std::rand();
            const int directionNum = 2;
            if (randNum % directionNum == 0) {
                newRoute = Direction::COUNTER_CLOCKWISE;
                req->SetArcTgtNode(vecLoadEndPortId);
            } else {
                newRoute = Direction::CLOCKWISE;
                req->SetArcTgtNode(vecLoadBeginPortId);
            }
            LOG_INFO_M(Unit::TMU, Stage::SPB) << "Stash compdata rand on cross station " << std::dec << id
                                        << " " << *req
                                        << " origin direction " << int(route) << " new direction " << int(newRoute);
            route = newRoute;
        } else if (configs.tmu_stash_backpressure_perception && req->IsStashCompData()) {
            Direction newRoute = route;
            if (stat.onRingCWBackPressure[bufferId] > stat.onRingCCBackPressure[bufferId]) {
                newRoute = Direction::COUNTER_CLOCKWISE;
                req->SetArcTgtNode(vecLoadEndPortId);
            } else {
                newRoute = Direction::CLOCKWISE;
                req->SetArcTgtNode(vecLoadBeginPortId);
            }
            LOG_INFO_M(Unit::TMU, Stage::SPB) << "Stash compdata backpressure on cross station " << std::dec
                                        << id << " " << *req
                                        << " origin direction " << int(route) << " new direction " << int(newRoute)
                                        << " onRingCWBackPressure " << stat.onRingCWBackPressure[bufferId]
                                        << " onRingCCBackPressure "  << stat.onRingCCBackPressure[bufferId];
            route = newRoute;
        }
        req->SetDirection(static_cast<int>(route));
        LOG_INFO_M(Unit::TMU, Stage::SPB) << "Cross station " << std::dec << id << " " << *req
                                        << " pre On Ring , Node=" << id << " direction " << int(route);
        if (CanPutOnRing(req->GetChannel(), route, req->GetId())) {
            req->onRingTime = GetSim()->getCycles();
            uint64_t onRingLatency = req->onRingTime - req->inSpbTime;
            size_t bufferIdx = static_cast<size_t>(ChannelType::COUNT) + static_cast<size_t>(req->GetBufferType());
            // 顺时针
            if (route == Direction::CLOCKWISE) {
                // Add to clockwise ring state
                cwRing->ForwardReq(id, req, channel);
                cwRing->stat.sumOnRingLatency[channel] += onRingLatency;
                cwRing->stat.sumOnRingLatency[bufferIdx] += onRingLatency;
                PrintStagePkt("SPB", "On CW Ring", req);
            } else {
                // 逆时针 Add to counter-clockwise ring state
                ccRing->ForwardReq(id, req, channel);
                ccRing->stat.sumOnRingLatency[channel] += (onRingLatency);
                ccRing->stat.sumOnRingLatency[bufferIdx] += (onRingLatency);
                PrintStagePkt("SPB", "On CC Ring", req);
            }
            readQ.erase(readQ.begin() + j);
            req->ResetCyclesWaiting();
            stat.onRingCycle++;
            if (req->IsFromCore() && (req->IsRead())) {
                stat.reqTotalCnt[channel]++;
            }
            if (req->IsFromCore() && (req->IsWrite())) {
                stat.dataTotalCnt[channel]++;
            }
            onRing[static_cast<size_t>(route)][channel] = true;
            // If this was a locked channel, unlock it
            if (ringState[static_cast<size_t>(Direction::COUNTER_CLOCKWISE)]
                    .channelLocks[channel] == req->GetId()) {
                UnlockChannel(req->GetChannel(), ringState[static_cast<size_t>(Direction::COUNTER_CLOCKWISE)]);
            }
            if (ringState[static_cast<size_t>(Direction::CLOCKWISE)]
                    .channelLocks[channel] == req->GetId()) {
                UnlockChannel(req->GetChannel(), ringState[static_cast<size_t>(Direction::CLOCKWISE)]);
            }
            break;
        } else {
            req->IncrementCyclesWaiting();
            if (!onRing[static_cast<size_t>(route)][channel]) {
                onRingBp[static_cast<size_t>(route)][channel] = true;
            }
        }
    }
}

// ===== VEC Ring stub functions (方案A) =====

bool CrossStation::IsVecReadPort()
{
    return GetSim()->core->configs.debug_stub_vec_ring_enable
           && id == pTileUtils->GetUBConfig()->vec_read_port[0];
}

bool CrossStation::IsDbidResponse(const std::shared_ptr<Request>& pkt) const
{
    return pkt->GetFlit().cmd == CHICommand::CompDBIDResp;
}

std::shared_ptr<Request> CrossStation::GenerateStubAddrTile(uint32_t groupSlotId, uint32_t txnId, int tmaNodeId)
{
    // 创建Addr Tile数据 (64个UINT32地址)
    vector<uint32_t> addrTileData(MAX_TILE_DATA_BYTE / sizeof(uint32_t));
    for (int lane = 0; lane < 64; lane++) {
        addrTileData[lane] = 0x1000 + lane;  // 简单递增地址
    }

    // 使用RxvscatterData创建CHIFlit (保留打包逻辑)
    shared_ptr<Request> addrTile = RxvscatterData(
        groupSlotId, // addr (groupSlotId)
        txnId, // txnId
        1, // did = 1 (Addr Tile)
        id, // srcNode: vec_read_port[0]
        tmaNodeId, // tgtNode: TMA
        0);

    // 设置数据
    addrTile->SetData(addrTileData);
    addrTile->SetSize(64 * sizeof(uint32_t));
    addrTile->SetPEType(MachineType::VECTOR);

    LOG_INFO_M(Unit::TMU, Stage::NA)
        << "[VEC Ring Stub] Generated Addr Tile: groupSlotId=" << groupSlotId
        << ", txnId=0x" << std::hex << txnId << std::dec
        << ", srcNode=" << id
        << ", tgtNode=" << tmaNodeId;

    return addrTile;
}

std::shared_ptr<Request> CrossStation::GenerateStubDataTile(uint32_t groupSlotId, uint32_t txnId, int tmaNodeId)
{
    // 创建Data Tile数据 (64个UINT16数据)
    vector<uint16_t> dataTileData(MAX_TILE_DATA_BYTE / sizeof(uint16_t));
    for (int lane = 0; lane < 64; lane++) {
        dataTileData[lane] = 0x1234 + lane;  // 简单递增数据
    }

    // 使用RxvscatterData创建CHIFlit (保留打包逻辑)
    shared_ptr<Request> dataTile = RxvscatterData(
        groupSlotId,
        txnId,
        0,           // did = 0 (Data Tile)
        id,          // srcNode: vec_read_port[0]
        tmaNodeId,   // tgtNode: TMA
        0);

    // 设置数据
    dataTile->SetData(dataTileData);
    dataTile->SetSize(64 * sizeof(uint16_t));
    dataTile->SetPEType(MachineType::VECTOR);

    LOG_INFO_M(Unit::TMU, Stage::NA)
        << "[VEC Ring Stub] Generated Data Tile: groupSlotId=" << groupSlotId
        << ", txnId=0x" << std::hex << txnId << std::dec
        << ", srcNode=" << id
        << ", tgtNode=" << tmaNodeId;

    return dataTile;
}

void CrossStation::ProcessVecRingStub(std::shared_ptr<Request> pkt)
{
    // 1. 提取关键信息
    uint32_t txnId = pkt->GetFlit().txnId;
    uint32_t groupSlotId = pkt->GetFlit().addr;
    uint32_t tmaNodeId = pkt->GetFlit().srcId;

    LOG_INFO_M(Unit::TMU, Stage::MGB)
        << "[VEC Ring Stub] DBID Response received: txnId=0x" << std::hex << txnId
        << ", groupSlotId=" << std::dec << groupSlotId
        << ", from TMA node=" << tmaNodeId;

    // 2. 生成并发送Addr Tile
    shared_ptr<Request> addrTile = GenerateStubAddrTile(groupSlotId, txnId, tmaNodeId);
    WriteSpb(addrTile);

    LOG_INFO_M(Unit::TMU, Stage::NA)
        << "[VEC Ring Stub] Addr Tile sent to Ring: txnId=0x" << std::hex << txnId
        << ", srcNode=" << std::dec << id
        << ", tgtNode=" << tmaNodeId;

    // 3. 生成并发送Data Tile
    shared_ptr<Request> dataTile = GenerateStubDataTile(groupSlotId, txnId, tmaNodeId);
    WriteSpb(dataTile);

    LOG_INFO_M(Unit::TMU, Stage::NA)
        << "[VEC Ring Stub] Data Tile sent to Ring: txnId=0x" << std::hex << txnId
        << ", srcNode=" << std::dec << id
        << ", tgtNode=" << tmaNodeId;
}

// ===== End VEC Ring stub functions =====

void CrossStation::ProcessMGB()
{
    for (auto &mgb : mgbArray) {
        if (mgb.Empty()) {
            continue;
        }
        std::shared_ptr<Request> pkt = mgb.Front();
        // ===== 方案A: VEC Ring stub处理 =====    ！！！！！！！！！！！！需要补充检查逻辑！！
        if (IsVecReadPort() && IsDbidResponse(pkt) && GetSim()->core->configs.debug_stub_vec_ring_enable) {
            ProcessVecRingStub(pkt);
            mgb.Read();  // 弹出DBID Response
            continue;    // 跳过toCoreBuff和FrqAddRequest
        }
        // ===== End 方案A =====
        if (pkt->IsFromTileRegister()) {
            // from tilereg to core
            // read mgb to core
            toCoreBuff[pkt->coreId].push(pkt);
            LOG_INFO_M(Unit::TMU, Stage::MGB) << "Cross station " << std::dec << id << " " << *(pkt) << " To Core";
            pkt->outMgbTime = GetSim()->getCycles();
            stat.totalReq2RespLatency += (pkt->outMgbTime - pkt->reqInSpbTime);
            mgb.Read();
        } else if (pkt->IsFromCore() && FrqCanAaccelptRequest(pkt->IsRead())) {
            // from core to tilereg
            // read mgbArray to frq
            FrqAddRequest(pkt);
            LOG_INFO_M(Unit::TMU, Stage::MGB) << "Cross station " << std::dec << id << " " << *(pkt) << " In Frq";
            pkt->outMgbTime = GetSim()->getCycles();
            pkt->frqTime = GetSim()->getCycles();
            mgb.Read();
        }
    }

    for (size_t i = 0; i < static_cast<size_t>(Direction::RING_NUM) && pTileUtils->IsRingOrXbarMode(false); i++) {
        // 从两个ring state收集请求
        for (int channel = 0; channel < static_cast<int>(ChannelType::COUNT); ++channel) {
            if (!TryPutInMGB(ringState[i].requestDownStash[channel])) {
                stat.offRingCCBackPressure[static_cast<size_t>(channel)]++;
                continue;
            }
            shared_ptr<Request> selectedReq = ringState[i].requestDownStash[channel];
            LOG_INFO_M(Unit::TMU, Stage::MGB) << "Cross station " << std::dec << id
                                              << " direction" << i << " selected " << *selectedReq
                                              << " from channel=" << selectedReq->GetChannelName();
            ringState[i].requestDownStash[channel] = nullptr;
            stat.offRingCycle++;
            size_t bufferIdx = static_cast<size_t>(ChannelType::COUNT) +
                               static_cast<size_t>(selectedReq->GetBufferType());
            if (i == 0) {
                cwRing->stat.sumOffRingLatency[channel] += (selectedReq->inMgbTime - selectedReq->offRingTime);
                cwRing->stat.sumOffRingLatency[bufferIdx] += (selectedReq->inMgbTime - selectedReq->offRingTime);
            } else {
                ccRing->stat.sumOffRingLatency[channel] += (selectedReq->inMgbTime - selectedReq->offRingTime);
                ccRing->stat.sumOffRingLatency[bufferIdx] += (selectedReq->inMgbTime - selectedReq->offRingTime);
            }
        }
    }
    // report mgbArray statistics
    for (size_t i = 0; i < mgbArray.size(); i++) {
        stat.mgbOccupiedCycle[i] += !mgbArray[i].Empty();
        stat.mgbFullCycle[i] += mgbArray[i].Full();
        stat.mgbOccupiedSize[i] += mgbArray[i].Size() + mgbArray[i].SizeW();
    }
}

bool CrossStation::HasNoResp(uint32_t coreId)
{
    return toCoreBuff[coreId].empty();
}

std::shared_ptr<Request> CrossStation::GetDataComReq(uint32_t coreId)
{
    if (toCoreBuff[coreId].empty()) {
        return nullptr;
    }

    std::shared_ptr<Request> req = toCoreBuff[coreId].front();
    LOG_INFO_M(Unit::TMU, Stage::MGB) << "Cross station " << std::dec << id << ", read out toCoreBuff, " << *(req);
    toCoreBuff[coreId].pop();
    return req;
}

std::shared_ptr<Request> CrossStation::GetDataComReqFront(uint32_t coreId)
{
    if (toCoreBuff[coreId].empty()) {
        return nullptr;
    }

    std::shared_ptr<Request> req = toCoreBuff[coreId].front();
    LOG_INFO_M(Unit::TMU, Stage::MGB) << "Cross station " << std::dec << id << ", read front from toCoreBuff, "
                                      << *(req);
    return req;
}

void CrossStation::PopDataComReq(uint32_t coreId)
{
    if (toCoreBuff[coreId].empty()) {
        ASSERT(false && "Can not pop empty toCoreBuffer!");
    }
    toCoreBuff[coreId].pop();
}

std::shared_ptr<Request> CrossStation::Rxreq(uint32_t addr, uint32_t stid, uint32_t coreId)
{
    CHIFlit flit(FlitType::HEADER, CHICommand::ReadUnique, id,
                     pTileUtils->GetDstNode(addr), addr, 0);
    auto req = std::make_shared<Request>(flit, ChannelType::REQ0);
    req->SetCoreId(coreId);
    req->SetStid(stid);
    LOG_INFO_M(Unit::TMU, Stage::NA) << "Rxreq " << *req;
    return req;
}

std::shared_ptr<Request> CrossStation::Rxdat(uint32_t addr, uint32_t stid, uint32_t coreId)
{
    CHIFlit flit(FlitType::HEADER, CHICommand::Write, id,
                     pTileUtils->GetDstNode(addr), addr, 0);
    auto req = std::make_shared<Request>(flit, ChannelType::DATA);
    req->SetCoreId(coreId);
    req->SetStid(stid);
    LOG_INFO_M(Unit::TMU, Stage::NA) << "Rxdat " << *req;
    return req;
}

std::shared_ptr<Request> CrossStation::Txdat(uint32_t addr, int srcNode, int tgtNode, uint32_t stid, uint32_t coreId)
{
    CHIFlit flit(FlitType::HEADER, CHICommand::Comp, srcNode, tgtNode, addr, 0);
    auto req = std::make_shared<Request>(flit, ChannelType::DATA);
    req->SetCoreId(coreId);
    req->SetStid(stid);
    LOG_INFO_M(Unit::TMU, Stage::NA) << "Txdat " << *req;
    return req;
}

std::shared_ptr<Request> CrossStation::Txrsp(uint32_t addr, int srcNode, int tgtNode, uint32_t stid, uint32_t coreId)
{
    CHIFlit flit(FlitType::HEADER, CHICommand::Comp, srcNode, tgtNode, addr, 0);
    auto req = std::make_shared<Request>(flit, ChannelType::RSP);
    req->SetCoreId(coreId);
    req->SetStid(stid);
    LOG_INFO_M(Unit::TMU, Stage::NA) << "Txrsp " << *req;
    return req;
}

std::shared_ptr<Request> CrossStation::RxvscatterReq(uint32_t addr, uint32_t txnId,
    int tgtNode, uint32_t coreId) const
{
    CHIFlit flit(FlitType::HEADER, CHICommand::CompDBIDResp, id, tgtNode, addr, txnId);
    auto req = std::make_shared<Request>(flit, ChannelType::RSP);
    req->SetCoreId(coreId);
    LOG_INFO_M(Unit::TMU, Stage::NA) << "Rxreq_vscatter " << *req;
    return req;
}

std::shared_ptr<Request> CrossStation::RxvscatterData(uint32_t addr, uint32_t txnId, uint8_t did,
    int srcNode, int tgtNode, uint32_t coreId) const
{
    CHIFlit flit(FlitType::HEADER, CHICommand::CompDBIDResp, srcNode, tgtNode, addr, txnId, did);
    auto req = std::make_shared<Request>(flit, ChannelType::DATA);
    req->SetCoreId(coreId);
    LOG_INFO_M(Unit::TMU, Stage::NA) << "Rxdat_vscatter " << *req;
    return req;
}

bool CrossStation::ShouldDropAtNode(const std::shared_ptr<Request>& req, uint32_t nodeId)
{
    return req != nullptr && req->GetArcTgtNode() == nodeId;
}

void CrossStation::CheckStarvation()
{
    // Check SPB for starvation
    for (auto &spb : spbArray) {
        if (spb.Empty()) {
            continue;
        }
        auto req = spb.Front();
        if (req->GetCyclesWaiting() >= configs.starvation_threshold
            && ringState[req->GetDirection()].channelLocks[static_cast<int>(req->GetChannel())] != 0) {
            if (!req->IsTagged()) {
                req->SetTagged(true);
                PrintStagePkt("SPB", "on ring starvation detected", req);
                // Lock the channel in both ring states
                LockChannel(req->GetChannel(), req->GetId(), ringState[req->GetDirection()]);
            }
        }
    }

    // Check MGB for starvation
    for (auto &mgb : mgbArray) {
        if (mgb.Empty()) {
            continue;
        }
        auto req = mgb.Front();
        if (req->GetCyclesWaiting() >= configs.starvation_threshold && !req->IsTagged()) {
            req->SetTagged(true);
            PrintStagePkt("MGB", "off ring starvation detected", req);
        }
    }
}

void CrossStation::PrintStagePkt(std::string stageName, std::string operatorName, std::shared_ptr<Request> pkt)
{
    LOG_INFO_M(Unit::TMU, Stage::NA) << "Cross station " << std::dec << id << " " << stageName
                                     << " " << *pkt << " " << operatorName;
}

bool CrossStation::CanPutOnRing(ChannelType channel, Direction direction, uint32_t rid)
{
    // （1）ring上对应请求的channel 是empty
    /**
    * for example ring has 5 channel
    * channel-1: DATA_REQ
    * channel-2: DATA_RSP
    * channel-3: SNOOP
    */
    // 上ring的防饿死机制
    // Check if channel is locked in either ring state
    uint32_t dir = static_cast<uint32_t>(direction);
    uint32_t chan = static_cast<uint32_t>(channel);
    const std::shared_ptr<Request> ringReq = ringState[dir].channelRequests[chan];
    if (ringState[dir].channelRequests[chan] != nullptr) {
        uint32_t addCnt = ringReq->IsFromCore() ? 1 : 0;
        if (dir == 0) {
            if (ringReq->GetPEType() == MachineType::VECTOR) {
                stat.cwNotOnRingByVec[chan]++;
                stat.cwNotOnRingByVecFromCore[chan] += addCnt;
            } else if (ringReq->GetPEType() == MachineType::CUBE) {
                stat.cwNotOnRingByCube[chan]++;
                stat.cwNotOnRingByCubeFromCore[chan] += addCnt;
            } else if (ringReq->GetPEType() == MachineType::TMA) {
                stat.cwNotOnRingByTmaFromCore[chan] += addCnt;
                stat.cwNotOnRingByTma[chan]++;
            }
            stat.cwNotOnRingTotal[chan]++;
        } else {
            if (ringReq->GetPEType() == MachineType::VECTOR) {
                stat.ccNotOnRingByVec[chan]++;
                stat.ccNotOnRingByVecFromCore[chan] += addCnt;
            } else if (ringReq->GetPEType() == MachineType::CUBE) {
                stat.ccNotOnRingByCube[chan]++;
                stat.ccNotOnRingByCubeFromCore[chan] += addCnt;
            } else if (ringReq->GetPEType() == MachineType::TMA) {
                stat.ccNotOnRingByTma[chan]++;
                stat.ccNotOnRingByTmaFromCore[chan] += addCnt;
            }
            stat.ccNotOnRingTotal[chan]++;
        }
        std::string ringName = (direction == Direction::CLOCKWISE) ? "CW" : "CC";
        LOG_INFO_M(Unit::TMU, Stage::SPB) << "Cross station " << std::dec << id
                                          << " On " << ringName << " ring, Node=" << id
                                          << " channel=" << static_cast<int>(channel)
                                          << " " << ringName << " ring channel is not empty";
        return false;
    }
    if (ringState[dir].channelLocks[chan] != 0 && ringState[dir].channelLocks[chan] != rid) {
        std::string ringName = (direction == Direction::CLOCKWISE) ? "CW" : "CC";
        LOG_INFO_M(Unit::TMU, Stage::SPB) << "Cross station " << std::dec << id
                                          << " On " << ringName << " ring, Node=" << id
                                          << " channel=" << static_cast<int>(channel)
                                          << " " << ringName << " ring channel is locked";
        return false;
    }
    return true;
}

bool CrossStation::FrqCanAaccelptRequest(bool isRead) const
{
    return isRead ? frq[0].size() < configs.frq_entry_size : frq[1].size() < configs.frq_entry_size;
}

void CrossStation::FrqAddRequest(const std::shared_ptr<Request>& request)
{
    TilePipeState newTileReq = TilePipeState();
    newTileReq.tStage = TileStage::PIPE_STAGE;
    newTileReq.waitCycle = 1;
    newTileReq.req = request;
    if (request->IsRead()) {
        frq[0].push_back(newTileReq);
    } else {
        frq[1].push_back(newTileReq);
    }
}

std::shared_ptr<Request> CrossStation::GetFrqReq(bool isRead)
{
    auto &frqWR = isRead ? frq[0] : frq[1];
    for (auto it = frqWR.begin(); it != frqWR.end(); ++it) {
        if (it->tStage == TileStage::TILE_REG_STAGE) {
            std::shared_ptr<Request> req = it->req;
            LOG_INFO_M(Unit::TMU, Stage::NA) << "Cross station " << std::dec << id
                                              << " Read only " << *req << ", from stash:"<< req->IsStashReq();
            return req;
        }
    }

    return nullptr;
}

void CrossStation::PopReadyEntryFrq(bool isRead)
{
    auto &frqWR = isRead ? frq[0] : frq[1];
    for (auto it = frqWR.begin(); it != frqWR.end(); ++it) {
        if (it->tStage == TileStage::TILE_REG_STAGE) {
            std::shared_ptr<Request> req = it->req;
            it->tStage = TileStage::DATA_DONE;
            LOG_INFO_M(Unit::TMU, Stage::NA) << "Cross station " << std::dec << id
                                             << " FRQ pop " << *req;
            it = frqWR.erase(it);
            break;
        }
    }
}

void CrossStation::TransferTilePipe()
{
    for (auto &frqWR : frq) {
        for (TilePipeState& tileReq : frqWR) {
            tileReq.waitCycle++;
            if (tileReq.waitCycle >= configs.tile_pipe_line
                && tileReq.tStage == TileStage::PIPE_STAGE) {
                tileReq.tStage = TileStage::TILE_REG_STAGE;
            }
        }
    }
}

void CrossStation::LockChannel(ChannelType channel, uint32_t reqId, RingState& ringState)
{
    ringState.channelLocks[static_cast<int>(channel)] = reqId;
    LOG_INFO_M(Unit::TMU, Stage::NA) << "Cross station " << std::dec << id
                                     << " reqId=" << reqId << " Locked channel=" << static_cast<int>(channel);
}

void CrossStation::UnlockChannel(ChannelType channel, RingState& ringState)
{
    ringState.channelLocks[static_cast<int>(channel)] = 0;
    LOG_INFO_M(Unit::TMU, Stage::NA) << "Cross station " << std::dec << id
                                     << " Unlocked channel=" << static_cast<int>(channel);
}

void CrossStation::CheckStatus()
{
    std::cout << "Station " << id << std::endl;
    std::cout << "SPB:" << std::endl;
    for (uint32_t i = 0; i < spbArray.size(); i++) {
        if (spbArray[i].Empty()) {
            continue;
        }
        auto &q = spbArray[i].GetRawReadData();
        for (auto &req : q) {
            std::cout << "channel " << i << ": " << *req << std::endl;
        }
    }
    std::cout << "MGB:" << std::endl;
    for (uint32_t i = 0; i < mgbArray.size(); i++) {
        if (mgbArray[i].Empty()) {
            continue;
        }
        auto &q = mgbArray[i].GetRawReadData();
        for (auto &req : q) {
            std::cout << "channel " << i << ": " << *req << std::endl;
        }
    }
    std::cout << "FRQ:" << std::endl;
    for (uint32_t i = 0; i < frq.size(); i++) {
        if (frq[i].empty()) {
            continue;
        }
        for (auto &e : frq[i]) {
            std::cout << "channel " << i << ": " << *e.req << std::endl;
        }
    }
}

bool CrossStation::SPBFull(BufferType type)
{
    return spbArray[static_cast<size_t>(type)].Full();
}

bool CrossStation::MGBFull(BufferType type)
{
    return mgbArray[static_cast<size_t>(type)].Full();
}

bool CrossStation::SPBEmpty(BufferType type)
{
    return spbArray[static_cast<size_t>(type)].Empty();
}

bool CrossStation::MGBEmpty(BufferType type)
{
    return mgbArray[static_cast<size_t>(type)].Empty();
}

bool CrossStation::SPBEmpty(ChannelType channel)
{
    for (auto it = buffer2Channel.begin(); it != buffer2Channel.end(); ++it) {
        auto &spb = spbArray[static_cast<size_t>(it->first)];
        if (it->second == channel && !spb.Empty()) {
            return false;
        }
    }
    return true;
}

bool CrossStation::MGBEmpty(ChannelType channel)
{
    for (auto it = buffer2Channel.begin(); it != buffer2Channel.end(); ++it) {
        auto &mgb = mgbArray[static_cast<size_t>(it->first)];
        if (it->second == channel && !mgb.Empty()) {
            return false;
        }
    }
    return true;
}

void CrossStation::WriteMGB(const shared_ptr<Request>& pkt)
{
    mgbArray[static_cast<size_t>(pkt->GetChannel())].Write(pkt);
}

shared_ptr<Request> CrossStation::PopSPB(BufferType type)
{
    auto &spb = spbArray[static_cast<size_t>(type)];
    return spb.Empty() ? nullptr : spb.Read();
}

shared_ptr<Request> CrossStation::ReadOnlySPB(BufferType type)
{
    auto &spb = spbArray[static_cast<size_t>(type)];
    return spb.Empty() ? nullptr : spb.Front();
}

shared_ptr<Request> CrossStation::ReadOnlyMGB(BufferType type)
{
    auto &mgb = mgbArray[static_cast<size_t>(type)];
    return mgb.Empty() ? nullptr : mgb.Front();
}

shared_ptr<Request> CrossStation::PopSPB(ChannelType channel)
{
    for (auto it = buffer2Channel.begin(); it != buffer2Channel.end(); ++it) {
        auto &spb = spbArray[static_cast<size_t>(it->first)];
        if (it->second == channel && !spb.Empty()) {
            return spb.Read();
        }
    }
    return nullptr;
}


shared_ptr<Request> CrossStation::ReadOnlySPB(ChannelType channel)
{
    for (auto it = buffer2Channel.begin(); it != buffer2Channel.end(); ++it) {
        auto &spb = spbArray[static_cast<size_t>(it->first)];
        if (it->second == channel && !spb.Empty()) {
            return spb.Front();
        }
    }
    return nullptr;
}

shared_ptr<Request> CrossStation::ReadOnlyMGB(ChannelType channel)
{
    for (auto it = buffer2Channel.begin(); it != buffer2Channel.end(); ++it) {
        auto &mgb = mgbArray[static_cast<size_t>(it->first)];
        if (it->second == channel && !mgb.Empty()) {
            return mgb.Front();
        }
    }
    return nullptr;
}

} // namespace JCore
