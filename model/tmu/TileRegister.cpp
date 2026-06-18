#include "tmu/TileRegister.h"

#include "SimSys.h"
#include "core/Core.h"

namespace JCore {

SimSys* TileRegister::GetSim()
{
    return sim;
}

void TileRegister::Build()
{
    stat.rpt = GetSim()->getRpt();
    priorityState = TilePriorityState::VEC_CUBE_TTRANS;
    configs.overrideDefaultConfig(GetSim()->getCfgs());
    rowNum = pow(LOG_2, configs.bank_select_bits.size());
    colNum = configs.bank_ncols;
    uint32_t bankNum = rowNum * colNum;
    uint32_t bankSize = configs.tile_reg_size * KILO * BITS_IN_BYTE / bankNum;
    banks.resize(GetSim()->core->configs.scalar_smt_thread);
    for (uint32_t stid = 0; stid < banks.size(); ++stid) {
        for (uint32_t i = 0; i < bankNum; i++) {
            // bank row size : bankSize/configs.bank_intervel = 256
            // bank colnumn size : configs.bank_intervel = 512bits
            banks[stid].emplace_back(TileRegBank(bankSize, configs.bank_intervel));
        }
    }
    maxAddr = pTileUtils->Bytes2Addr(configs.tile_reg_size * KILO) - 1;
    uint32_t logThreadID = 1;
    loadThreadID = logThreadID;
    logThreadID++;
    storeThreadID = logThreadID;
    preLoadByte = 0;
    preStoreByte = 0;
    loadByte = 0;
    storeByte = 0;
    stashCtrlUnit->sim = sim;
    stashCtrlUnit->machineType = MachineType::STASH;
    stashCtrlUnit->machineId = GetProcessID(MachineType::STASH, 0, 1);
    sim->ObjRegisterLogInfo(stashCtrlUnit, 0, sim->core->globalSeqToSwimPtr++);
    stashCtrlUnit->pTileUtils = pTileUtils;
    stashQueues.resize(pTileUtils->GetNodeNum());
    dataPipePriority = DataPipeState::OTHERS_STASH;
    tmuOtherPriorityNum = 0;
    for (uint32_t i = 0; i < configs.tmu_stash_outstanding; i++) {
        stashRR.push_back(i);
    }
    stashCtrlUnit->Build(&stat);
    sim->RegisterMultiThread(stashCtrlUnit, stashCtrlUnit->coreNum * stashCtrlUnit->vecConfigs.ta_cache_way, 0);
}

void TileRegister::AddNodeForStashCtrl(shared_ptr<CrossStation> station)
{
    stashCtrlUnit->AddNode(station);
}

void TileRegister::Reset()
{
    for (uint32_t stid = 0; stid < banks.size(); ++stid) {
        for (auto& bank : banks[stid]) {
            bank = TileRegBank();
        }
    }
}

void TileRegister::Work()
{
    preLoadByte = loadByte;
    preStoreByte = storeByte;
    loadByte = 0;
    storeByte = 0;
    stashCtrlUnit->Work();
    // Ring/Xbar mode
    if (pTileUtils->IsRingOrXbarMode(false)) {
        HandleRingOrXbarRequest();
    }
    switch (priorityState) {
        case TilePriorityState::VEC_CUBE_TTRANS:
            HandleVecRequest();
            HandleMtcRequest();
            HandleCubeRequest();
            HandleBridgeRequest();
            HandleTTransRequest();
            break;
        case TilePriorityState::TTRANS_VEC_CUBE:
            HandleBridgeRequest();
            HandleTTransRequest();
            HandleVecRequest();
            HandleMtcRequest();
            HandleCubeRequest();
            break;
        case TilePriorityState::CUBE_TTANS_VEC:
            HandleCubeRequest();
            HandleBridgeRequest();
            HandleTTransRequest();
            HandleVecRequest();
            HandleMtcRequest();
            break;
        default:
            break;
    }
}

void TileRegister::AddCounter(uint32_t bytes, uint64_t id)
{
    if (id == loadThreadID) {
        stat.totalLdSize += bytes;
    } else if (id == storeThreadID) {
        stat.totalStSize += bytes;
    }
}

void TileRegister::HandleVecRequest()
{
    HandleVecWriteRequest();
    HandleVecReadRequest();
}

void TileRegister::HandleMtcRequest()
{
    HandleMtcWriteRequest();
    HandleMtcReadRequest();
}


void TileRegister::HandleCubeRequest()
{
    HandleCubeWriteRequest();
    HandleCubeReadRequest();
}

void TileRegister::HandleTTransRequest()
{
    HandleTTransWriteRequest();
    HandleTTransReadRequest();
}

void TileRegister::HandleBridgeRequest()
{
    HandleBridgeWriteRequest();
    HandleBridgeReadRequest();
}

void TileRegister::HandleVecWriteRequest()
{
    uint32_t writeNum = 0;
    while (writeNum < configs.vec_nwrite) {
        bool working = false;
        for (size_t i = 0; i < vecTileRegStReqArray.size(); i++) {
            auto &vecTileRegStReqQ = vecTileRegStReqArray[i];
            if (vecTileRegStReqQ->Empty()) {
                continue;
            }

            VecTileRegStReq req = vecTileRegStReqQ->Read();
            pTileUtils->CheckAddrOverflow(req.GetDest(), req.GetSize());
            AddCounter(req.GetSize(), storeThreadID);
            writeNum++;
            stat.stReqCnt++;
            working = true;
            LOG_INFO_M(Unit::TMA, Stage::NA) << " Recv write , reqId=" << dec << req.GetReqId() << std::dec;
            if (BankConflict(req.GetDest(), req.GetSize(), req.GetStid())) {
                TileRegVecStRetry retry = TileRegVecStRetry();
                retry.SetRespId(req.GetReqId());
                retry.SetBid(req.GetBid());
                retry.SetGid(req.GetGid());
                retry.SetRid(req.GetRid());
                retry.SetTid(req.GetTid());
                tileRegVecStRetryArray[i]->Write(retry);
                stat.stConfCnt++;
            } else {
                StoreData(req.GetDest(), req.GetSize(),
                    req.GetData(), req.GetDataMask(), req.GetStid());
                TileRegVecLdRes resp = TileRegVecLdRes();
                resp.SetStid(req.GetStid());
                resp.SetRespId(req.GetReqId());
                tileRegVecWrResArray[i]->Write(resp);
            }
        }

        if (!working) {
            break;
        }
    }
}

void TileRegister::HandleVecReadRequest()
{
    uint32_t readNum = 0;
    while (readNum < configs.vec_nread) {
        bool working = false;
        for (size_t i = 0; i < vecTileRegLdReqArray.size(); i++) {
            auto &vecTileRegLdReqQ = vecTileRegLdReqArray[i];
            if (vecTileRegLdReqQ->Empty()) {
                continue;
            }

            VecTileRegLdReq req = vecTileRegLdReqQ->Read();
            pTileUtils->CheckAddrOverflow(req.GetSrc(), req.GetSize());
            AddCounter(req.GetSize(), loadThreadID);
            readNum++;
            stat.ldReqCnt++;
            working = true;
            LOG_INFO_M(Unit::TMA, Stage::NA) << "Recv read , reqId=" << dec << req.GetReqId() << std::dec;
            if (BankConflict(req.GetSrc(), req.GetSize(), req.GetStid())) {
                TileRegVecLdRetry retry = TileRegVecLdRetry();
                retry.SetRespId(req.GetReqId());
                retry.SetBid(req.GetBid());
                retry.SetGid(req.GetGid());
                retry.SetRid(req.GetRid());
                retry.SetTid(req.GetTid());
                tileRegVecLdRetryArray[i]->Write(retry);
                stat.ldConfCnt++;
            } else {
                TileRegVecLdRes resp = TileRegVecLdRes();
                resp.SetRespId(req.GetReqId());
                resp.SetSrc(req.GetAddr());
                resp.SetBid(req.GetBid());
                resp.SetGid(req.GetGid());
                resp.SetRid(req.GetRid());
                resp.SetTid(req.GetTid());
                resp.SetTagId(req.GetTagId(), req.GetHandType());
                resp.SetStid(req.GetStid());
                uint8_t *data = LoadData(req.GetSrc(), req.GetSize(), req.GetStid());
                resp.SetData(data);
                resp.SetPrefetch(req.GetIsStash());
                tileRegVecLdResArray[i]->Write(resp);
                free(data);
            }
        }

        if (!working) {
            break;
        }
    }
}


void TileRegister::HandleMtcWriteRequest()
{
    uint32_t writeNum = 0;
    for (size_t i = 0; i < mtcTileRegStReqArray.size() && writeNum != configs.vec_nwrite; i++) {
        auto &vecTileRegStReqQ = mtcTileRegStReqArray[i];
        while (writeNum != configs.vec_nwrite && !vecTileRegStReqQ->Empty()) {
            VecTileRegStReq req = vecTileRegStReqQ->Read();
            pTileUtils->CheckAddrOverflow(req.GetDest(), req.GetSize());
            AddCounter(req.GetSize(), storeThreadID);
            writeNum++;
            stat.stReqCnt++;
            LOG_INFO_M(Unit::TMA, Stage::NA) << "Recv write , reqId=" << req.GetReqId() << std::dec;
            if (BankConflict(req.GetDest(), req.GetSize(), req.GetStid())) {
                TileRegVecStRetry retry = TileRegVecStRetry();
                retry.SetRespId(req.GetReqId());
                retry.SetBid(req.GetBid());
                retry.SetGid(req.GetGid());
                retry.SetRid(req.GetRid());
                retry.SetTid(req.GetTid());
                tileRegMtcStRetryArray[i]->Write(retry);
                stat.stConfCnt++;
            } else {
                StoreData(req.GetDest(), req.GetSize(), req.GetData(), DataMask(1), req.GetStid());
                TileRegVecLdRes resp = TileRegVecLdRes();
                resp.SetStid(req.GetStid());
                resp.SetRespId(req.GetReqId());
                tileRegMtcWrResArray[i]->Write(resp);
            }
        }
    }
}

void TileRegister::HandleMtcReadRequest()
{
    uint32_t readNum = 0;
    for (size_t i = 0; i < mtcTileRegLdReqArray.size() && readNum != configs.vec_nread; i++) {
        auto &vecTileRegLdReqQ = mtcTileRegLdReqArray[i];
        while (readNum != configs.vec_nread && !vecTileRegLdReqQ->Empty()) {
            VecTileRegLdReq req = vecTileRegLdReqQ->Read();
            pTileUtils->CheckAddrOverflow(req.GetSrc(), req.GetSize());
            AddCounter(req.GetSize(), loadThreadID);
            readNum++;
            stat.ldReqCnt++;
            if (BankConflict(req.GetSrc(), req.GetSize(), req.GetStid())) {
                TileRegVecLdRetry retry = TileRegVecLdRetry();
                retry.SetRespId(req.GetReqId());
                retry.SetBid(req.GetBid());
                retry.SetGid(req.GetGid());
                retry.SetRid(req.GetRid());
                retry.SetTid(req.GetTid());
                tileRegMtcLdRetryArray[i]->Write(retry);
                stat.ldConfCnt++;
            } else {
                TileRegVecLdRes resp = TileRegVecLdRes();
                resp.SetRespId(req.GetReqId());
                resp.SetBid(req.GetBid());
                resp.SetGid(req.GetGid());
                resp.SetRid(req.GetRid());
                resp.SetTid(req.GetTid());
                resp.SetStid(req.GetStid());
                uint8_t *data = LoadData(req.GetSrc(), req.GetSize(), req.GetStid());
                resp.SetData(data);
                tileRegMtcLdResArray[i]->Write(resp);
                free(data);
            }
        }
    }
}

void TileRegister::HandleBridgeWriteRequest()
{
    uint32_t writeNum = 0;
    for (size_t i = 0; i < bridgeTileRegStReqArray.size() && writeNum != configs.ttrans_nwrite; i++) {
        auto &bridgeTileRegStReqQ = bridgeTileRegStReqArray[i];
        while (writeNum != configs.ttrans_nwrite && !bridgeTileRegStReqQ->Empty()) {
            TTransTileRegStReq req = bridgeTileRegStReqQ->Read();
            pTileUtils->CheckAddrOverflow(req.GetDest(), req.GetSize());
            AddCounter(req.GetSize(), storeThreadID);
            writeNum++;
            stat.stReqCnt++;
            LOG_INFO_M(Unit::TMU, Stage::NA) << "[Tile Register]: Recv bridge write, coreId=" << std::dec << i
                                             << ", reqId=" << req.GetReqId();
            if (BankConflict(req.GetDest(), req.GetSize(), req.GetStid())) {
                TileRegTTransStRetry retry = TileRegTTransStRetry();
                retry.SetRespId(req.GetReqId());
                tileRegBridgeStRetryArray[i]->Write(retry);
                stat.stConfCnt++;
            } else {
                StoreData(req.GetDest(), req.GetSize(), req.GetData(), DataMask(1), req.GetStid());
                TileRegTTransLdRes resp = TileRegTTransLdRes();
                resp.SetRespId(req.GetReqId());
                tileRegBridgeWrResArray[i]->Write(resp);
            }
        }
    }
}

void TileRegister::HandleBridgeReadRequest()
{
    uint32_t readNum = 0;
    for (size_t i = 0; i < bridgeTileRegLdReqArray.size() && readNum != configs.ttrans_nread; i++) {
        auto &bridgeTileRegLdReqQ = bridgeTileRegLdReqArray[i];
        while (readNum != configs.ttrans_nread && !bridgeTileRegLdReqQ->Empty()) {
            TTransTileRegLdReq req = bridgeTileRegLdReqQ->Read();
            pTileUtils->CheckAddrOverflow(req.GetSrc(), req.GetSize());
            AddCounter(req.GetSize(), loadThreadID);
            readNum++;
            stat.ldReqCnt++;
            LOG_INFO_M(Unit::TMU, Stage::NA) << "[Tile Register]: Recv bridge read, coreId=" << std::dec << i
                                             << ", reqId=" << req.GetReqId();
            if (BankConflict(req.GetSrc(), req.GetSize(), req.GetStid())) {
                TileRegTTransLdRetry retry = TileRegTTransLdRetry();
                retry.SetRespId(req.GetReqId());
                tileRegBridgeLdRetryArray[i]->Write(retry);
                stat.ldConfCnt++;
            } else {
                TileRegTTransLdRes resp = TileRegTTransLdRes();
                resp.SetSrc(req.GetSrc());
                resp.SetRespId(req.GetReqId());
                uint8_t *data = LoadData(req.GetSrc(), req.GetSize(), req.GetStid());
                resp.SetData(data);
                tileRegBridgeLdResArray[i]->Write(resp);
                free(data);
            }
        }
    }
}

void TileRegister::ReceiveStashRequest(std::shared_ptr<CrossStation> station, std::deque<StashLdReq>& stashQueue)
{
    // BCC->RING->drop
    std::shared_ptr<Request> req = station->GetFrqReq(true);
    if (req != nullptr && req->IsStashReq()) {
        for (uint32_t i = 0; i < req->GetStashTimes(); i++) {
            uint32_t addr = req->GetAddr() + (stations.size() * i) * pTileUtils->Bytes2Addr(MAX_TILE_DATA_BYTE);
            // 根据stash请求创建一个新的带有新地址的请求
            auto sepStashReq = station->Rxreq(addr, req->GetCoreId());
            sepStashReq->SetStashDstNode(req->GetStashDstNode());
            sepStashReq->SetStashReq(true);
            sepStashReq->SetSize(MAX_TILE_DATA_BYTE);
            sepStashReq->SetBufId(req->GetBufId());
            sepStashReq->SetWayId(req->GetWayId());
            sepStashReq->cmd = req->cmd;
            sepStashReq->SetCoreId(req->coreId);
            sepStashReq->SetStashTimes(1);

            StashLdReq stashReq(sepStashReq);
            stashReq.SetStation(station->GetId());
            LOG_INFO_M(Unit::TMU, Stage::NA) << "receive bcc stash " << *stashReq.req
            << " in station: " << station->GetId()
            << " stashTimes: " << i;
            stashQueue.emplace_back(stashReq);
        }
        station->PopReadyEntryFrq(true);
    }
    // pe->ring->drop
    if (req != nullptr && req->IsStashResp()) {
        stashCtrlUnit->stashRespQ.Write(req);
        LOG_INFO_M(Unit::TMU, Stage::NA) << "send bcc stash resp to stash control" << *req << ", stashCnt:" << req->cmd->stashCnt - 1;
        station->PopReadyEntryFrq(true);
    }
}

void TileRegister::HandleStashRequest(std::deque<StashLdReq>& stashQueue)
{
    if (!stashQueue.empty()) {
        auto stash = stashQueue.front();
        uint32_t addr = stash.GetAddr();
        if (stations[stash.GetStation()]->SpbHasStashEntry(BufferType::LOAD_DATA_RES)) {
            LOG_INFO_M(Unit::TMU, Stage::NA) << "spb has stash req, don't need to send ";
            return;
        }
        LOG_INFO_M(Unit::TMU, Stage::NA) << "check td stash " << *stash.req;
        if (stations[stash.GetStation()]->SpbHasAvailEntry(BufferType::LOAD_DATA_RES) && spbWrieNum == 0) {
            stat.ldReqCnt++;
            std::shared_ptr<Request> data_comp = stations[stash.GetStation()]->Txdat(addr,
                                                                                     stash.req->GetArcTgtNode(),
                                                                                     stash.req->GetStashDstNode(),
                                                                                     stash.req->stid,
                                                                                     stash.req->coreId);
            LOG_INFO_M(Unit::TMU, Stage::NA) << "TD recv stash " << *stash.req;
            uint32_t size = stash.req->GetSize();
            pTileUtils->CheckAddrOverflow(addr, size);
            StashPipeInResp(stash.req, data_comp);
            data_comp->tileTime = GetSim()->getCycles();
            data_comp->SetBufId(stash.req->GetBufId());
            data_comp->SetAddr(addr);
            data_comp->SetSize(stash.req->GetSize());
            data_comp->SetWayId(stash.req->GetWayId());
            data_comp->uopId = stash.req->uopId;
            data_comp->cmd = stash.req->cmd;
            data_comp->SetStashResp(true);
            AddCounter(size, loadThreadID);
            if (BankConflict(addr, stash.req->GetSize(), stash.req->cmd ? stash.req->cmd->stid : 0)) {
                // retry TODO
                ASSERT(0);
            } else {
                uint8_t *data = LoadData(addr, size, stash.req->cmd ? stash.req->cmd->stid : 0);
                data_comp->SetData(data);
                spbWrieNum = 1;
                stations[stash.GetStation()]->WriteSpb(data_comp);
                free(data);
            }
            LOG_INFO_M(Unit::TMU, Stage::NA) << "TD send stash request "<< *data_comp << "to PE" << ", count:" <<
                data_comp->cmd->stashCnt;
            stashQueue.pop_front();
        }
    }
}

void TileRegister::HandleRingOrXbarRequest()
{
    switch (dataPipePriority) {
        case DataPipeState::OTHERS_STASH:
            // LOG_INFO_M(Unit::TMU, Stage::NA) << "HandleStash req priority, priority is OTHERS_STASH " ;
            for (size_t i = 0; i < stations.size(); i++) {
                spbWrieNum = 0;
                HandStationRequest(stations[i]);
                ReceiveStashRequest(stations[i], stashQueues[i]);
                bool exStash = std::count(stashRR.begin(), stashRR.end(), i);
                if (exStash) {
                    HandleStashRequest(stashQueues[i]);
                }
            }
            tmuOtherPriorityNum += 1 ;

            if (tmuOtherPriorityNum == configs.tmu_other_req_priority_times) {
                dataPipePriority = DataPipeState::STASH_OTHERS;
                tmuOtherPriorityNum = 0;
                // LOG_INFO_M(Unit::TMU, Stage::NA) << "HandleStash req priority cnt: "
                //                                  << configs.tmu_other_req_priority_times;
            }
            break;
        case DataPipeState::STASH_OTHERS:
            // LOG_INFO_M(Unit::TMU, Stage::NA) << "HandleStash req priority, priority is STASH_OTHERS " ;
            for (size_t i = 0; i < stations.size(); i++) {
                spbWrieNum = 0;
                ReceiveStashRequest(stations[i], stashQueues[i]);
                bool exStash = std::count(stashRR.begin(), stashRR.end(), i);
                if (exStash) {
                    HandleStashRequest(stashQueues[i]);
                }
                HandStationRequest(stations[i]);
            }
            dataPipePriority = DataPipeState::OTHERS_STASH;
            break;
        default:
            break;
    }
    for (uint32_t i = 0; i < configs.tmu_stash_outstanding; i++) {
        stashRR[i] = (stashRR[i] + configs.tmu_stash_outstanding) % stations.size();
    }
}

void TileRegister::StashPipeInResp(std::shared_ptr<Request> req, std::shared_ptr<Request> resp)
{
    resp->reqInSpbTime = req->inSpbTime;
    resp->reqOnRingTime = req->onRingTime;
    resp->reqOffRingTime = req->offRingTime;
    resp->reqInMgbTime = req->inMgbTime;
    resp->reqOutMgbTime = req->outMgbTime;
    resp->frqTime = req->frqTime;
}

void TileRegister::HandStationRequest(std::shared_ptr<CrossStation> station)
{
    std::shared_ptr<Request> req = station->GetFrqReq(true);
    if (req != nullptr && station->SpbHasAvailEntry(BufferType::LOAD_DATA_RES) && !req->IsStash() && spbWrieNum == 0) {
        stat.ldReqCnt++;
        std::shared_ptr<Request> data_comp = station->Txdat(req->GetAddr(),
                                                            req->GetArcTgtNode(),
                                                            req->GetArcSrcNode(), req->stid, req->coreId);
        pTileUtils->CheckAddrOverflow(req->GetAddr(), req->GetSize());
        StashPipeInResp(req, data_comp);
        data_comp->tileTime = GetSim()->getCycles();
        data_comp->SetBufId(req->GetBufId());
        data_comp->SetAddr(req->GetFlit().addr);
        data_comp->SetSize(req->GetSize());
        data_comp->uopId = req->uopId;
        data_comp->cmd = req->cmd;
        data_comp->SetPrefetchResp(req->IsPrefetchReq());
        data_comp->SetPEType(req->GetPEType());
        AddCounter(req->GetSize(), loadThreadID);
        if (BankConflict(req->GetAddr(), req->GetSize(), req->cmd ? req->cmd->stid : 0)) {
            // retry TODO
            TileRegVecLdRetry retry = TileRegVecLdRetry();
            retry.SetRespId(req->GetBufId());
            stat.ldConfCnt++;
        } else {
            uint8_t *data = LoadData(req->GetAddr(), req->GetSize(), req->cmd ? req->cmd->stid : 0);
            data_comp->SetData(data);
            spbWrieNum = 1;
            station->WriteSpb(data_comp);
            free(data);
        }
        station->PopReadyEntryFrq(true);
    }

    req = station->GetFrqReq(false);
    if (req == nullptr || !station->SpbHasAvailEntry(BufferType::STORE_DATA_RES) || req->IsStash()) {
        return;
    }
    LOG_INFO_M(Unit::TMU, Stage::NA) << " mymark " << *req;
    std::shared_ptr<Request> data_comp;
    if (!req->broadcast) {
        data_comp = station->Txrsp(req->GetAddr(), req->GetArcTgtNode(), req->GetArcSrcNode(), req->stid, req->coreId);
    } else {
        if (req->broadcastList.empty()) {
            data_comp = station->Txrsp(req->GetAddr(), req->GetArcTgtNode(), req->broadcastTgt, req->stid, req->coreId);
        } else {
            BroadcastInfo info = req->broadcastList[0];
            req->broadcastList.erase(req->broadcastList.begin());
            data_comp = station->Txrsp(req->GetAddr(), req->GetArcTgtNode(), info.station, req->stid, req->coreId);

            if (info.stash) {
                data_comp->SetWayId(info.stashWayId);
                data_comp->SetStashReq(true);
            }
        }
        data_comp->broadcastList = req->broadcastList;
        data_comp->broadcastTgt = req->broadcastTgt;
    }
    data_comp->broadcast = req->broadcast;
    stat.stReqCnt++;
    StashPipeInResp(req, data_comp);
    data_comp->tileTime = GetSim()->getCycles();
    data_comp->SetBufId(req->GetBufId());
    data_comp->SetAddr(req->GetFlit().addr);
    data_comp->SetSize(req->GetSize());
    data_comp->uopId = req->uopId;
    data_comp->cmd = req->cmd;
    data_comp->SetPEType(req->GetPEType());
    pTileUtils->CheckAddrOverflow(req->GetAddr(), req->GetSize());
    AddCounter(req->GetSize(), storeThreadID);
    if (BankConflict(req->GetAddr(), req->GetSize(), req->cmd ? req->cmd->stid : 0)) {
        // retry TODO
        TileRegVecStRetry retry = TileRegVecStRetry();
        retry.SetRespId(req->GetBufId());
        stat.stConfCnt++;
    } else {
        station->WriteSpb(data_comp);
        StoreData(req->GetAddr(), req->GetSize(), req->GetData(), req->dmask, req->cmd ? req->cmd->stid : 0);
    }
    station->PopReadyEntryFrq(false);
    // LOG_INFO_M(Unit::TMU, Stage::NA) << "HandleStash store req: " << *data_comp
    //     << " broadcast " << req->broadcast << " broadcastTgt " << req->broadcastTgt;
}

void TileRegister::HandleCubeWriteRequest()
{
    uint32_t writeNum = 0;
    while (writeNum < configs.cube_nwrite) {
        bool working = false;
        for (size_t i = 0; i < cubeTileRegStReqArray.size(); i++) {
            auto &cubeTileRegStReqQ = cubeTileRegStReqArray[i];
            if (cubeTileRegStReqQ->Empty()) {
                continue;
            }
            CubeTileRegStReq req = cubeTileRegStReqQ->Read();
            pTileUtils->CheckAddrOverflow(req.GetDest(), req.GetSize());
            AddCounter(req.GetSize(), storeThreadID);
            writeNum++;
            stat.stReqCnt++;
            working = true;
            LOG_INFO_M(Unit::TMU, Stage::NA) << "[Tile Register]: Recv cube write, coreId=" << std::dec << i
                                            << ", reqId=" << req.GetReqId();
            if (BankConflict(req.GetDest(), req.GetSize(), req.GetStid())) {
                TileRegCubeStRetry retry = TileRegCubeStRetry();
                retry.SetRespId(req.GetReqId());
                tileRegCubeStRetryArray[i]->Write(retry);
                stat.stConfCnt++;
            } else {
                StoreData(req.GetDest(), req.GetSize(), req.GetData(), DataMask(1), req.GetStid());
                TileRegCubeLdRes resp = TileRegCubeLdRes();
                resp.SetRespId(req.GetReqId());
                resp.SetBid(req.GetBid());
                resp.SetStid(req.GetStid());
                tileRegCubeWrResArray[i]->Write(resp);
            }
        }

        if (!working) {
            break;
        }
    }
}

void TileRegister::HandleCubeReadRequest()
{
    uint32_t readNum = 0;
    while (readNum < configs.cube_nread) {
        bool working = false;
        for (size_t i = 0; i < cubeTileRegLdReqArray.size(); i++) {
            auto &cubeTileRegLdReqQ = cubeTileRegLdReqArray[i];
            if (cubeTileRegLdReqQ->Empty()) {
                continue;
            }
            CubeTileRegLdReq req = cubeTileRegLdReqQ->Read();
            pTileUtils->CheckAddrOverflow(req.GetSrc(), req.GetSize());
            AddCounter(req.GetSize(), loadThreadID);
            readNum++;
            stat.ldReqCnt++;
            working = true;
            LOG_INFO_M(Unit::TMU, Stage::NA) << "[Tile Register]: Recv cube read, coreId=" << std::dec << i
                                            << ", reqId=" << req.GetReqId();
            if (BankConflict(req.GetSrc(), req.GetSize(), req.GetStid())) {
                TileRegCubeLdRetry retry = TileRegCubeLdRetry();
                retry.SetRespId(req.GetReqId());
                tileRegCubeLdRetryArray[i]->Write(retry);
                stat.ldConfCnt++;
            } else {
                TileRegCubeLdRes resp = TileRegVecLdRes();
                resp.SetStid(req.GetStid());
                resp.SetRespId(req.GetReqId());
                uint8_t *data = LoadData(req.GetSrc(), req.GetSize(), req.GetStid());
                resp.SetData(data);
                tileRegCubeLdResArray[i]->Write(resp);
                free(data);
            }
        }

        if (!working) {
            break;
        }
    }
}

void TileRegister::HandleTTransWriteRequest()
{
    uint32_t writeNum = 0;
    for (size_t i = 0; i < tTransTileRegStReqArray.size() && writeNum != configs.ttrans_nwrite; i++) {
        auto &tTransTileRegStReqQ = tTransTileRegStReqArray[i];
        while (writeNum != configs.ttrans_nwrite && !tTransTileRegStReqQ->Empty()) {
            TTransTileRegStReq req = tTransTileRegStReqQ->Read();
            pTileUtils->CheckAddrOverflow(req.GetDest(), req.GetSize());
            AddCounter(req.GetSize(), storeThreadID);
            writeNum++;
            stat.stReqCnt++;
            if (BankConflict(req.GetDest(), req.GetSize(), req.GetStid())) {
                TileRegTTransStRetry retry = TileRegTTransStRetry();
                retry.SetRespId(req.GetReqId());
                tileRegTTransStRetryArray[i]->Write(retry);
                stat.stConfCnt++;
            } else {
                StoreData(req.GetDest(), req.GetSize(), req.GetData(), DataMask(1), req.GetStid());
            }
        }
    }
}

void TileRegister::HandleTTransReadRequest()
{
    uint32_t readNum = 0;
    for (size_t i = 0; i < tTransTileRegLdReqArray.size() && readNum != configs.ttrans_nread; i++) {
        auto &tTransTileRegLdReqQ = tTransTileRegLdReqArray[i];
        while (readNum != configs.ttrans_nread && !tTransTileRegLdReqQ->Empty()) {
            TTransTileRegLdReq req = tTransTileRegLdReqQ->Read();
            pTileUtils->CheckAddrOverflow(req.GetSrc(), req.GetSize());
            AddCounter(req.GetSize(), loadThreadID);
            readNum++;
            stat.ldReqCnt++;
            if (BankConflict(req.GetSrc(), req.GetSize(), req.GetStid())) {
                TileRegTTransLdRetry retry = TileRegTTransLdRetry();
                retry.SetRespId(req.GetReqId());
                tileRegTTransLdRetryArray[i]->Write(retry);
                stat.ldConfCnt++;
            } else {
                TileRegTTransLdRes resp = TileRegTTransLdRes();
                resp.SetRespId(req.GetReqId());
                uint8_t *data = LoadData(req.GetSrc(), req.GetSize(), req.GetStid());
                resp.SetData(data);
                tileRegTTransLdResArray[i]->Write(resp);
                free(data);
            }
        }
    }
}

bool TileRegister::BankConflict(uint32_t addr, uint32_t size, uint32_t stid)
{
    if (configs.perfect_mode) {
        return false;
    }
    uint32_t bytes = size;
    while (bytes !=  0) {
        if (banks[stid][GetBankIdx(addr, stid)].IsAaccelssed()) {
            return true;
        }
        uint32_t wrSize = addr == AlignToBankAddr(addr) ? configs.bank_intervel / BITS_IN_BYTE
                                                        : pTileUtils->Addr2Bytes(AlignToNextBankAddr(addr) - addr);
        if (bytes <= wrSize) {
            break;
        }
        addr += pTileUtils->Bytes2Addr(wrSize);
        bytes -= wrSize;
    }
    return false;
}

void TileRegister::CheckAddr(uint32_t addr, uint32_t bankIdx, uint32_t rowInBank, uint32_t colInBank)
{
    uint64_t value = addr;
    uint64_t key = (static_cast<uint64_t>(colInBank) << 34) | (static_cast<uint64_t>(rowInBank) << 21) | (bankIdx);

    auto result = addrCheckerHash.insert({key, value});
    if (!result.second) {
        uint64_t existing_value = result.first->second;
        if (existing_value != value) {
            cout << " tow addrs has same bankIdx/rowInBank/colInBank , addr: 0x" << hex <<
            existing_value <<  " and 0x" << addr << endl;
            cout << dec << " bankIdx " << bankIdx << "  rowInBank " << rowInBank  << " colInBank " <<
            colInBank << " " << endl;
            assert(0);
        }
    }
}

JCore::TileRegister::IdxStruct TileRegister::Addr2Idx(uint32_t addr, uint32_t stid)
{
    IdxStruct idx;
    idx.bankIdx = GetBankIdx(addr, stid);
    idx.rowInBank = GetSegmentIdxInBank(addr);
    idx.colInBank = banks[stid][idx.bankIdx].GetColIdx(pTileUtils->Addr2Bytes(addr) * BITS_IN_BYTE);
    return idx;
}

uint8_t* TileRegister::LoadData(uint32_t addr, uint32_t size, uint32_t stid)
{
    uint8_t *data = (uint8_t *)malloc(MAX_TILE_DATA_BYTE * sizeof(uint8_t));
    uint32_t loadBytes = 0;
    IdxStruct idx = Addr2Idx(addr, stid);
    LOG_DEBUG_M(Unit::TMA, Stage::NA) << " addr: 0x" << hex << addr << dec << " bankIdx " << idx.bankIdx
        << "  rowInBank " << idx.rowInBank  << " colInBank " << idx.colInBank << " ";
    CheckAddr(addr, idx.bankIdx, idx.rowInBank, idx.colInBank);
    while (size != loadBytes) {
        auto& bank = banks[stid][idx.bankIdx];
        ASSERT(loadBytes < MAX_TILE_DATA_BYTE);
        data[loadBytes] = bank.Load(idx.rowInBank, idx.colInBank);
        LOG_DEBUG_M(Unit::TMU, Stage::NA) << "{addr:0x" << hex << addr << " data:0x"
            << static_cast<int>(data[loadBytes]) << "} ";
        loadBytes++;
        idx.colInBank = (idx.colInBank + 1) % configs.bank_element_size;
        if (idx.colInBank == 0) {
            addr += 1;
            idx = Addr2Idx(addr, stid);
        }
    }
    return data;
}

void TileRegister::StoreData(uint32_t addr, uint32_t size, uint8_t* data, DataMask mask, uint32_t stid)
{
    uint32_t storeBytes = 0;
    IdxStruct idx = Addr2Idx(addr, stid);
    LOG_DEBUG_M(Unit::TMA, Stage::NA) << " addr: 0x" << hex << addr << dec << " bankIdx " << idx.bankIdx
        << "  rowInBank " << idx.rowInBank  << " colInBank " << idx.colInBank << " ";
    CheckAddr(addr, idx.bankIdx, idx.rowInBank, idx.colInBank);
    while (size != storeBytes) {
        auto& bank = banks[stid][idx.bankIdx];
        if (mask.IsValid(storeBytes)) {
            LOG_DEBUG_M(Unit::TMU, Stage::NA) << "{addr:0x" << hex << addr << " data:0x"
                << static_cast<int>(data[storeBytes]) << "} ";
            bank.Store(idx.rowInBank, idx.colInBank, data[storeBytes]);
        } else {
            LOG_DEBUG_M(Unit::TMU, Stage::NA) << "{InvalidMask addr:0x" << hex << addr << " data:0x"
                << static_cast<int>(data[storeBytes]) << "} ";
        }
        ASSERT(storeBytes < MAX_TILE_DATA_BYTE);
        storeBytes++;
        idx.colInBank = (idx.colInBank + 1) % configs.bank_element_size;
        if (idx.colInBank == 0) {
            addr += 1;
            idx = Addr2Idx(addr, stid);
        }
    }
}

void TileRegister::SetZeroData(uint32_t addr, uint32_t size, uint32_t stid)
{
    uint32_t storeBytes = 0;
    IdxStruct idx = Addr2Idx(addr, stid);
    CheckAddr(addr, idx.bankIdx, idx.rowInBank, idx.colInBank);
    while (size != storeBytes) {
        auto& bank = banks[stid][idx.bankIdx];
        bank.Store(idx.rowInBank, idx.colInBank, 0);
        storeBytes++;
        idx.colInBank = (idx.colInBank + 1) % configs.bank_element_size;
        if (idx.colInBank == 0) {
            addr += 1;
            idx = Addr2Idx(addr, stid);
        }
    }
}

uint32_t TileRegister::GetSegmentIdxInBank(uint32_t addr)
{
    uint32_t lowMaskbit = log2(pTileUtils->Bytes2Addr(configs.bank_intervel / BITS_IN_BYTE * colNum));
    uint32_t maskAddr = addr >> lowMaskbit;
    uint32_t highShift = configs.bank_select_bits[1] + 1 - lowMaskbit;
    uint32_t highBits = (maskAddr >> highShift) << (configs.bank_select_bits[0] - lowMaskbit);
    uint32_t lowBits = maskAddr & ((0x1 << (configs.bank_select_bits[0] - lowMaskbit)) - 1);
    return  highBits | lowBits;
}

void TileRegister::Xfer()
{
    for (uint32_t stid = 0; stid < banks.size(); ++stid) {
        for (auto& bank : banks[stid]) {
            bank.SetAaccelssState(false);
        }
    }
    switch (priorityState) {
        case TilePriorityState::VEC_CUBE_TTRANS:
            priorityState = TilePriorityState::TTRANS_VEC_CUBE;
            break;
        case TilePriorityState::TTRANS_VEC_CUBE:
            priorityState = TilePriorityState::CUBE_TTANS_VEC;
            break;
        case TilePriorityState::CUBE_TTANS_VEC:
            priorityState = TilePriorityState::VEC_CUBE_TTRANS;
            break;
        default:
            break;
    }
    stashCtrlUnit->Xfer();
}

uint32_t TileRegister::GetBankIdx(uint32_t addr, uint32_t stid)
{
    uint32_t bankId = GetBankRowIdx(addr) * colNum + GetBankColIdx(addr);
    ASSERT(banks[stid].size() > bankId && "Bank index out of range!");
    return bankId;
}

uint32_t TileRegister::GetBankRowIdx(uint32_t addr)
{
    uint32_t bankMask = (0x1 << configs.bank_select_bits.size()) - 1;
    return ((addr >> configs.bank_select_bits[0]) & bankMask);
}

uint32_t TileRegister::GetBankColIdx(uint32_t addr)
{
    uint32_t offset = pTileUtils->Addr2Bytes(addr) * BITS_IN_BYTE % (configs.bank_intervel * colNum);
    return offset / configs.bank_intervel;
}

uint32_t TileRegister::AlignToBankAddr(uint32_t addr)
{
    return addr - addr % pTileUtils->Bytes2Addr(configs.bank_intervel / BITS_IN_BYTE);
}

uint32_t TileRegister::AlignToNextBankAddr(uint32_t addr)
{
    return AlignToBankAddr(addr) + pTileUtils->Bytes2Addr(configs.bank_intervel / BITS_IN_BYTE);
}

uint8_t TileRegBank::Load(uint32_t rowIdx, uint32_t colIdx)
{
    aaccelssed = true;
    ASSERT(mem.size() > rowIdx && "Bank load row index out of range!");
    ASSERT(mem[rowIdx].size() > colIdx && "Bank load col index out of range!");
    return mem[rowIdx][colIdx];
}

void TileRegBank::Store(uint32_t rowIdx, uint32_t colIdx, uint8_t data)
{
    aaccelssed = true;
    ASSERT(mem.size() > rowIdx && "Bank store row index out of range!");
    ASSERT(mem[rowIdx].size() > colIdx && "Bank store col index out of range!");
    mem[rowIdx][colIdx] = data;
}

bool TileRegBank::IsAaccelssed()
{
    return aaccelssed;
}

void TileRegBank::SetAaccelssState(bool aaccelss)
{
    aaccelssed = aaccelss;
}

uint32_t TileRegBank::GetColIdx(uint32_t addrBits)
{
    return addrBits % intervel / BITS_IN_BYTE;
}

void TileRegister::GetTileRegData(TileOperandPtr &tileInfo, std::vector<uint64_t> &data, uint32_t stid)
{
    if (tileInfo->isZero) {
        return;
    }
    uint32_t addr = tileInfo->baseAddr;
    uint32_t size = tileInfo->size;
    uint32_t loadBytes = 0;
    IdxStruct idx = Addr2Idx(addr, stid);
    CheckAddr(addr, idx.bankIdx, idx.rowInBank, idx.colInBank);
    while (size != loadBytes) {
        auto& bank = banks[stid][idx.bankIdx];
        data[loadBytes] = (uint64_t)bank.Load(idx.rowInBank, idx.colInBank);
        loadBytes++;
        idx.colInBank = (idx.colInBank + 1) % configs.bank_element_size;
        if (idx.colInBank == 0) {
            addr += 1;
            idx = Addr2Idx(addr, stid);
        }
    }
}

} // namespace JCore
