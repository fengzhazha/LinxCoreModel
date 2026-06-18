#include "mtccore/memcore/MemoryCore.h"

#include "core/Core.h"
#include "DFX/InstTracer.h"

namespace JCore {

/* The data need from large to samll which using for load/store request. */
std::array<int, MemoryCore::M_DATA_UNIT_SIZE_NUM> MemoryCore::gDataUnitSizeArray = {
    M_BASEDATASIZE * 4, M_BASEDATASIZE * 3, M_BASEDATASIZE * 2, M_BASEDATASIZE};

MemoryCore::MemoryCore() : m_peNum(MTC_PE_NUM)
{
    m_iex = std::make_shared<VecIEX>();
    for (uint32_t i = 0; i < m_peNum; i++) {
        std::shared_ptr<VecPE> pe = std::make_shared<VecPE>("Memory Core " + to_string(i), i);
        m_peArray.push_back(pe);
    }
}

MemoryCore::~MemoryCore()
{
}

void MemoryCore::SetVerboseOn()
{
    verbose = true;
}

void MemoryCore::Work()
{
    m_iex->Work();
    for (uint32_t i = 0; i < m_peNum; i++) {
        m_peArray[i]->Work();
    }

    Flush();

    HandleMemRequest();
    m_grob->Work();
    m_gBuffer->Work();
    m_vectorLM->Work();
    m_vecta->Work();
    HandleMemLdResp();
    HandleMemStResp();

    m_stats.m_totalCycle++;
    if (!m_vecOpQ.empty()) {
        m_stats.m_workingCycle++;
    }
    for (uint32_t tid = 0; tid < logicalGroups; ++tid) {
        if (m_freeTidQ.find(tid) == m_freeTidQ.end()) {
            m_stats.m_perThreadWorkingCycle[tid]++;
        }
    }
}

void MemoryCore::Xfer()
{
    m_iex->Xfer();
    for (uint32_t i = 0; i < m_peNum; i++) {
        m_peArray[i]->Xfer();
    }
    iexVcoreReqQ->Work();
    vcoreIexResQ->Work();
    m_memReqQ.Work();
    m_lm2GBufferQ.Work();
    m_lm2GROB.Work();
    m_GBuffer2GROB.Work();
    m_GROB2MaskFile.Work();
    m_scbDrainBusQ.Work();
    m_scbDrainDoneBusQ.Work();

    if (m_vecta) {
        m_vecta->Xfer();
    }
    return;
}

void MemoryCore::InitMemoryLoopManager()
{
    m_vectorLM = make_shared<MemoryLoopManager>(logicalGroups, GetSim()->core->configs.simtLane,
                                                config.mtc_core_lm_work_req_num);
    m_vectorLM->top = this;
    m_vectorLM->Build();
    m_vectorLM->SetSim(GetSim());
    m_vectorLM->bccMtcBlockCmdQ = bccMtcBlockCmdQ;
    m_vectorLM->mtcBCCWakeupQ = mtcBCCWakeupQ;
    m_vectorLM->m_scalperInstQ = m_scaplerInstQ;
    m_vectorLM->m_lm2GBufferQ = &m_lm2GBufferQ;
    m_vectorLM->m_lm2GROB = &m_lm2GROB;
    m_vectorLM->top = this;
}

void MemoryCore::InitMemoryGBuffer()
{
    m_gBuffer = make_shared<GroupBuffer>();
    m_gBuffer->SetSim(GetSim());
    m_gBuffer->Build(logicalGroups, config.mtc_core_grob_depth, config.mtc_core_grob_distribute_num);
    m_gBuffer->m_peIfuQ = m_peIfuQ;
    m_gBuffer->m_workLoadQ.resize(m_workLoadQ.size());
    for (uint32_t i = 0; i < m_workLoadQ.size(); i++) {
        m_gBuffer->m_workLoadQ[i] = m_workLoadQ[i];
    }
    m_gBuffer->m_lm2GBufferQ = &m_lm2GBufferQ;
    m_gBuffer->m_GBuffer2GROB = &m_GBuffer2GROB;
    m_gBuffer->m_memoryLM = m_vectorLM;

    m_gBuffer->m_maskFile = make_shared<LastMaskFile>();
    m_gBuffer->m_maskFile->SetSim(GetSim());
    m_gBuffer->m_maskFile->Build(config.mtc_core_grob_depth, config.fetch_blocks_width);
    // m_gBuffer->m_maskFile->m_GROB2MaskFile = &m_GROB2MaskFile;
    m_gBuffer->mtop = this;
}

void MemoryCore::InitMemoryGROB()
{
    m_grob = make_shared<GROB>();
    m_grob->SetSim(GetSim());
    m_grob->mtop = this;
    m_grob->machineId = machineId;
    m_grob->Build(config.mtc_core_grob_depth, config.fetch_blocks_width);
    m_grob->m_lm2GROB = &m_lm2GROB;
    m_grob->m_GBuffer2GROB = &m_GBuffer2GROB;
    // m_grob->m_GROB2MaskFile = &m_GROB2MaskFile;
    // m_grob->m_drainBusQ = &m_scbDrainBusQ;
    // m_grob->m_drainRespBusQ = &m_scbDrainDoneBusQ;
    m_grob->mtop = this;
}

void MemoryCore::InitMemoryTA()
{
    m_vecta = std::make_shared<MemoryCoreTA>();
    m_vecta->Build(this, config.ta_set, config.ta_way, config.ta_entries, config.ta_waysize, config.ta_ldq_size,
        config.ta_stq_size, config.enable_tile_cache);
    m_vecta->outputQ = &m_memReqQ;
    m_vecta->tileRegLdReqQ = this->m_vecTileRegLdReqQ;
    m_vecta->tileRegStReqQ = this->m_vecTileRegStReqQ;
    m_vecta->tileRegLdResQ = this->m_tileRegVecLdResQ;
    m_vecta->tileRegWrRspQ = this->m_tileRegVecWrResQ;
    m_vecta->drainScbDataQ = &m_scbDrainBusQ;
    m_vecta->drainScbRespQ = &m_scbDrainDoneBusQ;

    m_vecta->resolveSignal = this->resolveBlock;
    this->taLoadQ = &m_vecta->loadQ;
    this->taStoreQ = &m_vecta->storeQ;

    m_vecta->scb->scbStReqQ = this->scbStReqQ;
    m_vecta->scb->scbStResQ = this->scbStResQ;
    m_scbCommitQ = &m_vecta->scb->scbCommitQ;
}

void MemoryCore::Build()
{
    m_iex->Build();
    config.overrideDefaultConfig(GetSim()->getCfgs());
    ubConfig.overrideDefaultConfig(GetSim()->getCfgs());
    for (uint32_t i = 0; i < m_peNum; i++) {
        m_peArray[i]->sim = sim;
        m_peArray[i]->Build();
    }

    logicalGroups = config.mtc_core_thread_num;
    m_stats.rpt = GetSim()->getRpt();
    m_stats.m_threadNum = logicalGroups;
    m_stats.Reset();
    m_ldReqArray.resize(config.mtc_core_load_request_queue_size);
    ldArraySize = 0;
    m_stReqArray.resize(config.mtc_core_store_request_queue_size);
    stArraySize = 0;
    m_laBuffer = std::vector<uint8_t>(ubConfig.tile_reg_size * KILO);
    for (uint32_t i = 0; i < logicalGroups; ++i) {
        m_freeTidQ.emplace(i);
    }
    m_taBuffer = std::vector<uint8_t>(ubConfig.tile_reg_size * KILO);
    m_taVld = std::vector<bool>(ubConfig.tile_reg_size * KILO);
    for (uint32_t i = 0; i < ubConfig.tile_reg_size * KILO; ++i) {
        m_taVld[i] = false;
    }

    for (uint32_t i = 0; i < m_stReqArray.size(); ++i) {
        m_stReqArray[i].status = MtcRequestStatus::INVALID;
        m_stReqArray[i].valid = false;
    }
    m_memReqArray.resize(m_memReqArraySize);
    InitMemoryLoopManager();
    InitMemoryGBuffer();
    InitMemoryGROB();
    InitMemoryTA();
    return;
}

bool MemoryCore::IsMtcBusy()
{
    bool groupBuffer = m_gBuffer->IsEmpty();
    return !groupBuffer;
}

void MemoryCore::Report_load_latency()
{
    m_stats.Report_load_latency();
}

void MemoryCore::ReportStat()
{
    std::string preName = "Memory Core Tile LSU";
    m_stats.Report(preName);
}

void MemoryCore::Reset()
{
    m_iex->Reset();
    for (uint32_t i = 0; i < m_peNum; i++) {
        m_peArray[i]->Reset();
    }

    m_laBuffer.clear();
    for (auto &req : m_ldReqArray) {
        req.status = MtcRequestStatus::INVALID;
    }
    ldArraySize = 0;
    for (auto &req : m_stReqArray) {
        req.status = MtcRequestStatus::INVALID;
        req.valid = false;
    }
    stArraySize = 0;
    m_freeTidQ.clear();
    for (uint32_t i = 0; i < logicalGroups; ++i) {
        m_freeTidQ.emplace(i);
    }
    return;
}

SimSys* MemoryCore::GetSim()
{
    return sim;
}

const MtcCoreConfig& MemoryCore::GetConfig() const
{
    return config;
}

uint64_t MemoryCore::GetRequestCount(uint32_t shapeSize, uint32_t elementSize)
{
    uint64_t count = 0;
    uint64_t totalSize = shapeSize * elementSize;

    for (int unitSize : gDataUnitSizeArray) {
        count += totalSize / unitSize;
    }

    if (totalSize > 0) {
        /* When there is still a remaining size, initiate a load request using the smallest data unit. */
        count++;
    }

    return count;
}

uint64_t GetReqQueueFreeCount(std::vector<MtcRequest> &reqArray)
{
    uint64_t count = 0;
    for (MtcRequest &r : reqArray) {
        if (r.status == MtcRequestStatus::INVALID) {
            count++;
        }
    }

    return count;
}

void MemoryCore::Flush()
{
    if (m_bccVectorFlushQ->Empty()) {
        return;
    }

    m_bccVectorFlushQ->Read();
    Reset();
}

void MemoryCore::DoCommonFlush(FlushBus& bus)
{
    // TODO add tid on each part for correct flush
    m_vecta->Flush(bus);

    // Load/store input
    auto matchReq = [&bus](MemRequest &req) {
        if (bus.baseOnThread && req.tid != bus.req.tid) {
            return false;
        }
        return LessEqual(bus.req.bid, req.bid);
    };

    // To tile register info flush
    taLoadQ->FlushIf(matchReq);
    taStoreQ->FlushIf(matchReq);
    m_memReqQ.FlushIf(matchReq);
    vcoreIexResQ->FlushIf(matchReq);

    auto matchDrain = [&bus](ScbDrainBus &req) {
        return LessEqual(bus.req.bid, req.bid);
    };
    m_scbDrainBusQ.FlushIf(matchDrain);
    m_scbDrainDoneBusQ.FlushIf(matchDrain);

    auto matchMskFile = [&bus](ROBID &bid) {
        return LessEqual(bus.req.bid, bid);
    };
    m_GROB2MaskFile.FlushIf(matchMskFile);

    m_vectorLM->SetFlush(bus);

    auto matchAllocReq = [&bus](VCore::GBufferAllocReq &info) {
        return LessEqual(bus.req.bid, info.blockCmd->bid);
    };
    if (bus.baseOnBid) {
        m_lm2GBufferQ.FlushIf(matchAllocReq);
    }

    auto matchGrobAllocInfo = [&bus](GroupAllocInfo &info) {
        return LessEqual(bus.req.bid, info.bid);
    };
    if (bus.baseOnBid) {
        m_lm2GROB.FlushIf(matchGrobAllocInfo);
    }

    auto matchIssueInfo = [&bus](GroupIssueInfo &info) {
        return LessEqual(bus.req.bid, info.bid);
    };
    if (bus.baseOnBid) {
        m_GBuffer2GROB.FlushIf(matchIssueInfo);
    }

    // Ifu info flush
    auto matchFetchState = [&bus](BlkBodyFetchState &req) {
        return LessEqual(bus.req.bid, req.bid);
    };
    auto matchBlkInfo = [&bus](BlockRunInfo &req) {
        if (bus.baseOnThread && req.tid != bus.req.tid) {
            return false;
        }
        return LessEqual(bus.req.bid, req.bid);
    };
    for (uint32_t i = 0; i < logicalGroups; ++i) {
        if (bus.baseOnThread && i == bus.req.tid) {
            m_peIfuQ->FlushIf(matchFetchState, i);
            m_workLoadQ[i]->FlushIf(matchBlkInfo);
        } else if (!bus.baseOnThread) {
            m_peIfuQ->FlushIf(matchFetchState, i);
            m_workLoadQ[i]->FlushIf(matchBlkInfo);
        }
    }

    // To memory info flush
    auto matchMemLdReq = [&bus](TTransMemLdReq &req) {
        return LessEqual(bus.req.bid, req.GetBid());
    };
    m_vecMemLdReqQ->FlushIf(matchMemLdReq);

    auto matchMemStReq = [&bus](TTransMemStReq &req) {
        return LessEqual(bus.req.bid, req.GetBid());
    };
    m_vecMemStReqQ->FlushIf(matchMemStReq);

    auto matchIexReq = [&bus](MemRequest& req) {
        if (bus.baseOnThread && req.tid != bus.req.tid) {
            return false;
        }
        if (bus.baseOnBid) {
            return LessEqual(bus.req.bid, req.bid);
        }
        return LessEqual(bus.req.bid, bus.req.rid, req.bid, req.rid);
    };
    iexVcoreReqQ->FlushIf(matchIexReq);
    m_memReqQ.FlushIf(matchIexReq);
}


void MemoryCore::Replay(FlushBus& bus)
{
    m_gBuffer->SetFlush(bus);
    DoCommonFlush(bus);
}

void MemoryCore::SetFlush(FlushBus &bus)
{
    m_gBuffer->SetFlush(bus);
    DoCommonFlush(bus);
}

void MemoryCore::HandleDataRet(MtcMissMemReq& missReq)
{
    ASSERT(0);
    union {
        uint64_t data;
        uint8_t src[8];
    } unionData;
    MemRequest req = missReq.req;
    for (uint64_t lane : missReq.lanes) {
        uint64_t addr = req.addrs.Get(lane, static_cast<uint32_t>(OperandWidth::OPDW_D));
        for (uint32_t i = 0; i < req.width; ++i) {
            unionData.src[i] = m_taBuffer[addr + i];
        }
        req.data.Set(unionData.data, lane, req.width);
    }
    m_memReqQ.Write(req);
}

void MemoryCore::HandleDataRet(MemRequest& memReq)
{
    ASSERT(0);
    if (memReq.isLoad) {
        ASSERT(0 && "Should not be here");
    }
    union {
        uint64_t data;
        uint8_t src[8];
    } unionData;
    constexpr static uint64_t bytes = 8;
    for (uint32_t lane = 0; lane < memReq.lanes; ++lane) {
        uint64_t addr = memReq.addrs.Get(lane, static_cast<uint32_t>(OperandWidth::OPDW_D));
        unionData.data = memReq.data.Get(lane, memReq.width * bytes);
        for (uint32_t i = 0; i < memReq.width; ++i) {
            m_taBuffer[addr + i] = unionData.src[i];
        }
    }
    m_memReqQ.Write(memReq);
}

void MemoryCore::HandleMissReturn()
{
    vector<uint32_t> addrs;
    for (auto it = m_vecRet.begin(); it != m_vecRet.end(); ++it) {
        if (it->second) {
            addrs.push_back(it->first);

            for (auto& req : m_vecLoad[it->first]) {
                HandleDataRet(req);
                if (VERBOSE_ON) {
                    std::cout << "Response Load request " << req.req << ". And addr: 0x" << std::hex
                        << it->first << " is returned" << endl;
                }
            }
            m_vecLoad.erase(it->first);
            if (m_storeMiss.find(it->first) != m_storeMiss.end()) {
                HandleDataRet(m_storeMiss[it->first]);
            }
        }
    }

    for (uint32_t addr : addrs) {
        m_vecRet.erase(addr);
    }
}

static bool CompareAddr(VecData instData, VecData reqData, MemRequest req)
{
    for (uint32_t lane = 0; lane < req.lanes; ++lane) {
        if ((req.mask & (1ULL << lane)) == 0) {
            continue;
        }

        if (instData.Get(lane, static_cast<uint32_t>(OperandWidth::OPDW_D)) != reqData.Get(lane, static_cast<uint32_t>(OperandWidth::OPDW_D))) {
            return false;
        }
    }
    return true;
}

void MemoryCore::HandleMemRequest()
{
    // 将 req 发送到 TA 中
    auto &readQ = iexVcoreReqQ->GetRawReadData();
    for (auto it = readQ.begin(); it != readQ.end();) {
        MemRequest req = *it;
        it = readQ.erase(it);
        if (req.isLoad) {
            ASSERT(req.thread == GetSim()->core->GetMtcPEIndex());
            taLoadQ->Write(req);
        } else {
            ASSERT(req.thread == GetSim()->core->GetMtcPEIndex());
            taStoreQ->Write(req);
        }
        if (core->pTracer->IsEnabled() && req.uinst) {
            PMemDumpInfo info = std::make_shared<MemDumpInfo>(req.uinst);
            info->recvCycle = GetSim()->getCycles();
            info->isLoad = req.isLoad;
            core->pTracer->Push(req.uinst->uid, info);
        }
    }
    // FIXME: temporarily limit the response to the iex, to avoid deadlock
    uint64_t sentreq = 0;
    VecData  retData;
    /*
     * FIXME: Temporary modification, used to delay by one cycle.
     * E2 is when the TA BUFFER receives the request.
     * E3 is when the TA BUFFER obtains the data.
     * E4 is when the TA BUFFER organizes the data.
     * W1 is when the data is returned to IEX.
     */
    while (!m_memReqQ.Empty() && sentreq < GetSim()->core->iex[MEM_IEX]->configs.mtc_iex_lda_iq_count
                              && !vcoreIexResQ->Full()) {
        MemRequest req = m_memReqQ.Read();
        if (VERBOSE_ON) {
            std::cout << "Aaccelpt req " << req << endl;
        }

        ASSERT(req.thread == GetSim()->core->GetMtcPEIndex());
        ++sentreq;
        if (!(req.opcode == Opcode::OP_TSD)) {
            vcoreIexResQ->Write(req);
            continue;
        }
        for (auto it = m_vectorLM->tstoreInstQ.begin(); it != m_vectorLM->tstoreInstQ.end();) {
            if (!CompareAddr((*it)->pdsts_[DST0_IDX]->vecData, req.addrs, req)) {
                ++it;
                continue;
            }
            // (*it)->isScalar = true;
            retData.Init(req.width * 8, req.lanes);
            for (uint32_t lane = 0; lane < req.lanes; ++lane) {
                uint64_t data = req.data.Get(lane);
                retData.Set(data, lane);
            }
            (*it)->pdsts_[DST0_IDX]->vecData.data = retData.data;
            (*it)->pdsts_[DST0_IDX]->vecData.width = retData.width;
            (*it)->pdsts_[DST0_IDX]->vecData.size = retData.size;
            (*it)->pipeCycle->tileDataRetCycle = GetSim()->cycles;
            (*it)->subissued = true;
        }
    }
}

bool MemoryCore::GenVecMemLdReq(MemRequest req)
{
    if (!req.isLoad) {
        return false;
    }
    for (uint32_t i = 0; i < m_memReqArraySize; i++) {
        if (m_memReqArray[i].val) {
            continue;
        }
        for (uint32_t j = 0; j < req.lanes; j++) {
            req.data.Set(0, j, req.width);
            TTransMemLdReq ldReq;
            uint32_t reqId = i * m_memReqId + j;
            uint64_t addr = req.addrs.Get(j, static_cast<uint32_t>(OperandWidth::OPDW_D));
            ldReq.SetAddr(addr);
            ldReq.SetSize(req.width);
            ldReq.SetReqId(reqId);
            ldReq.SetCoreId(coreId);
            if (VERBOSE_ON) {
                std::cout << "Load request split " << req << ". And addr is 0x" << hex << addr << endl;
            }
            m_vecMemLdReqQ->Write(ldReq);
        }
        req.val = true;
        m_memReqArray[i] = req;
        return true;
    }
    return false;
}

bool MemoryCore::GenVecMemStReq(MemRequest req)
{
    if (req.isLoad) {
        return false;
    }
    for (uint32_t i = 0; i < m_memReqArraySize; i++) {
        if (m_memReqArray[i].val) {
            continue;
        }
        for (uint32_t j = 0; j < req.lanes; j++) {
            TTransMemStReq stReq;
            uint32_t reqId = i * m_memReqId + j;
            uint64_t addr = req.addrs.Get(j, static_cast<uint32_t>(OperandWidth::OPDW_D));
            stReq.SetAddr(addr);
            stReq.SetSize(req.width);
            stReq.SetReqId(reqId);
            stReq.SetCoreId(coreId);
            if (VERBOSE_ON) {
                std::cout << "Store request split " << req << ". And addr is 0x" << hex << addr << endl;
            }
            m_vecMemStReqQ->Write(stReq);
        }
        req.val = true;
        m_memReqArray[i] = req;
        return true;
    }
    return false;
}

void MemoryCore::HandleMemLdResp()
{
    if (m_vecMemLdResQ->Empty()) {
        return;
    }

    MemRequest *req = nullptr;
    for (uint64_t j = 0; j < config.loadCountPerCycle; j++) {
        if (m_vecMemLdResQ->Empty()) {
            break;
        }
        union {
            uint64_t data;
            uint8_t src[8];
        } unionData;
        unionData.data = 0;
        MemTTransLdRes res = m_vecMemLdResQ->Read();
        uint32_t id = res.GetReqId();
        uint32_t i = id / m_memReqId;
        uint32_t lane = id % m_memReqId;
        req = &m_memReqArray[i];
        ASSERT(req->val);
        uint8_t *data = res.GetData();
        for (uint32_t i = 0; i < req->width; i++) {
            unionData.src[i] = data[i];
        }
        req->data.Set(unionData.data, lane, req->width * 8);
        req->start++;
        if (req->start < req->lanes) {
            continue;
        }
        req->val = false;
        if (VERBOSE_ON) {
            std::cout << "Store request response " << *req << endl;
        }
        m_memReqQ.Write(*req);
    }
}

void MemoryCore::HandleMemStResp()
{
    for (uint64_t j = 0; j < config.loadCountPerCycle; j++) {
        if (m_vecMemStResQ->Empty()) {
            break;
        }
        MemTTransStRes res = m_vecMemStResQ->Read();
        uint32_t id = res.GetReqId();
        uint32_t i = id / m_memReqId;
        MemRequest& req = m_memReqArray[i];
        ASSERT(req.val);
        req.start++;
        if (req.start < req.lanes) {
            continue;
        }
        req.val = false;
        m_memReqQ.Write(req);
    }
}

ROBID MemoryCore::GetOldestGid()
{
    return m_grob->GetOldestGid(0);
}

void MemoryCore::Dump()
{
    m_vecta->Dump();
}

void MemoryCore::PrintCfg()
{
    GetSim()->core->SetMtcConfigs("vector_lane_number", GetSim()->core->configs.GetCondfigValue("simtLane"));
    GetSim()->core->SetMtcConfigs("logic_group_parallel_cnt", config.GetCondfigValue("mtc_core_thread_num"));
    GetSim()->core->SetMtcConfigs("group_rob_entry_number", config.GetCondfigValue("mtc_core_grob_depth"));
    GetSim()->core->SetMtcConfigs("tile_lsu_cache_sets", config.GetCondfigValue("ta_set"));
    GetSim()->core->SetMtcConfigs("tile_lsu_cache_ways", config.GetCondfigValue("ta_way"));
    GetSim()->core->SetMtcConfigs("tile_lsu_ldq_entry", config.GetCondfigValue("ta_ldq_size"));
    GetSim()->core->SetMtcConfigs("tile_lsu_stq_entry", config.GetCondfigValue("ta_stq_size"));
    GetSim()->core->SetMtcConfigs("tile_lsu_scb_entry", config.GetCondfigValue("ta_scb_entry_size"));
}

} // namespace JCore
