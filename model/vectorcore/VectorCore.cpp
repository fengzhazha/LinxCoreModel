#include "vectorcore/VectorTop.h"
#include "vectorcore/VectorCore.h"

#include "core/Core.h"

namespace JCore {

/* The data need from large to samll which using for load/store request. */
std::array<int, VectorCore::M_DATA_UNIT_SIZE_NUM> VectorCore::gDataUnitSizeArray = {
    M_BASEDATASIZE * 4, M_BASEDATASIZE * 3, M_BASEDATASIZE * 2, M_BASEDATASIZE};

VectorCore::VectorCore(uint32_t coreId) : m_coreId(coreId) {}

VectorCore::~VectorCore() {}

void VectorCore::SetVerboseOn()
{
    verbose = true;
}

void VectorCore::Work()
{
    m_lgprRF->Work();
    m_pe->Work();
    m_iex->Work();

    Flush();

    HandleMemRequest();
    m_grob->Work();
    m_gBuffer->Work();
    m_vectorLM->Work();
    m_vectorGS->Work();
    m_vecta->Work();
    HandleMemLdResp();
    HandleMemStResp();

    m_stats.m_totalCycle++;
    if (!m_vecOpQ.empty()) {
        m_stats.m_workingCycle++;
    }
}

void VectorCore::Xfer()
{
    m_lgprRF->Xfer();
    m_pe->Xfer();
    m_iex->Xfer();

    iexVcoreReqQ->Work();
    vcoreIexResQ->Work();
    vcoreIexLdResQ->Work();
    vcoreIexStResQ->Work();
    m_lm2GBufferQ.Work();
    m_lm2GROB.Work();
    m_GBuffer2GROB.Work();
    m_GROB2MaskFile.Work();
    for (uint32_t stid = 0; stid < m_scbDrainBusQ.size(); ++stid) {
        m_scbDrainBusQ[stid].Work();
        m_scbDrainDoneBusQ[stid].Work();
    }
    m_storeDoneBusQ.Work();
    m_vectorGS->Xfer();

    if (m_vecta) {
        m_vecta->Xfer();
    }
    CheckBusy();
}

void VectorCore::InitGroupScheduler()
{
    m_vectorGS = make_shared<GroupScheduler>(logicalGroups, GetSim()->core->configs.simtLane,
                                                config.vector_core_lm_work_req_num);
    m_vectorGS->Build();
    m_vectorGS->SetSim(GetSim());
    m_vectorGS->bccMCallBlockCmdQ = bccMCallBlockCmdQ;
    m_vectorGS->vecMCallWakeupQ = vecMCallWakeupQ;
    m_vectorGS->vecBridgeRspQ = vecBridgeRspQ;
    m_vectorGS->vecBridgeReqQ = vecBridgeReqQ;
    m_vectorGS->m_vecBCCCreditQ = m_vecBCCCreditQ;
    m_vectorGS->m_lm2GBufferQ = &m_lm2GBufferQ;
    m_vectorGS->m_lm2GROB = &m_lm2GROB;
    m_vectorGS->top = this;
}

void VectorCore::InitVectorLoopManager()
{
    m_vectorLM = make_shared<VectorLoopManager>(logicalGroups, GetSim()->core->configs.simtLane,
                                                config.vector_core_lm_work_req_num);
    m_vectorLM->Build();
    m_vectorLM->SetSim(GetSim());
    m_vectorLM->bccVecBlockCmdQ = bccVecBlockCmdQ;
    m_vectorLM->vecBCCWakeupQ = vecBCCWakeupQ;
    m_vectorLM->m_vecBCCCreditQ = m_vecBCCCreditQ;
    m_vectorLM->m_lm2GBufferQ = &m_lm2GBufferQ;
    m_vectorLM->m_lm2GROB = &m_lm2GROB;
    m_vectorLM->top = this;
}

void VectorCore::InitVectorGBuffer()
{
    m_gBuffer = make_shared<GroupBuffer>();
    m_gBuffer->SetSim(GetSim());
    m_gBuffer->Build(logicalGroups, config.vector_core_grob_depth, config.vector_core_grob_distribute_num);
    m_gBuffer->m_peIfuQ = m_peIfuQ;
    m_gBuffer->m_workLoadQ.resize(m_workLoadQ.size());
    for (uint32_t i = 0; i < m_workLoadQ.size(); i++) {
        m_gBuffer->m_workLoadQ[i] = m_workLoadQ[i];
    }
    m_gBuffer->m_lm2GBufferQ = &m_lm2GBufferQ;
    m_gBuffer->m_GBuffer2GROB = &m_GBuffer2GROB;
    m_gBuffer->m_vectorLM = m_vectorLM;

    m_gBuffer->m_maskFile = make_shared<LastMaskFile>();
    m_gBuffer->m_maskFile->SetSim(GetSim());
    m_gBuffer->m_maskFile->Build(config.vector_core_grob_depth, config.fetch_blocks_width);
    m_gBuffer->m_maskFile->m_GROB2MaskFile = &m_GROB2MaskFile;
    m_gBuffer->top = this;
}

void VectorCore::InitVectorGROB()
{
    m_grob = make_shared<GROB>();
    m_grob->SetSim(GetSim());
    m_grob->top = this;
    m_grob->machineId = machineId;
    m_grob->Build(config.vector_core_grob_depth, config.fetch_blocks_width);
    m_grob->m_lm2GROB = &m_lm2GROB;
    m_grob->m_GBuffer2GROB = &m_GBuffer2GROB;
    m_grob->m_GROB2MaskFile = &m_GROB2MaskFile;

    m_grob->m_drainBusQ.resize(GetSim()->core->configs.scalar_smt_thread);
    m_grob->m_drainRespBusQ.resize(GetSim()->core->configs.scalar_smt_thread);
    for (uint32_t stid = 0; stid < GetSim()->core->configs.scalar_smt_thread; ++stid) {
        m_grob->m_drainBusQ[stid] = &m_scbDrainBusQ[stid];
        m_grob->m_drainRespBusQ[stid] = &m_scbDrainDoneBusQ[stid];
    }
}

void VectorCore::InitVectorTA()
{
    m_vecta = std::make_shared<VectorCoreTA>();
    m_vecta->Build(this, config.ta_set, config.ta_way, config.ta_waysize, config.ta_ldq_size,
        config.ta_stq_size, config.enable_tile_cache);
    m_vecta->ldOutputQ = vcoreIexLdResQ;
    m_vecta->stOutputQ = vcoreIexStResQ;
    m_vecta->tileRegLdReqQ = this->m_vecTileRegLdReqQ;
    m_vecta->tileRegStReqQ = this->m_vecTileRegStReqQ;
    m_vecta->tileRegLdResQ = this->m_tileRegVecLdResQ;
    m_vecta->tileRegWrRspQ = this->m_tileRegVecWrResQ;
    m_vecta->drainScbDataQ.resize(GetSim()->core->configs.scalar_smt_thread);
    m_vecta->drainScbRespQ.resize(GetSim()->core->configs.scalar_smt_thread);
    for (uint32_t stid = 0; stid < GetSim()->core->configs.scalar_smt_thread; ++stid) {
        m_vecta->drainScbDataQ[stid] = &m_scbDrainBusQ[stid];
        m_vecta->drainScbRespQ[stid] = &m_scbDrainDoneBusQ[stid];
    }
    m_vecta->storeDataCompRespQ = &m_storeDoneBusQ;

    m_vecta->resolveSignal = this->resolveBlock;
    m_vecta->m_vecBridgeMemReqQ = this->m_vecBridgeMemReqQ;
    m_vecta->m_bridgeVecLdRetQ = this->m_bridgeVecLdRetQ;
    m_vecta->m_bridgeVecStRslvQ = this->m_bridgeVecStRslvQ;
    this->taLoadQ = &m_vecta->loadQ;
    this->taStoreQ = &m_vecta->storeQ;
}

void VectorCore::BuildPE()
{
    uint32_t peID = this->core->configs.stdPeCount + m_coreId;
    m_pe = std::make_shared<VecPE>("Vector Core " + to_string(m_coreId), m_coreId);
    m_pe->core = this->core;
    m_pe->sim = this->sim;
    m_pe->machineType = MachineType::VECTOR;
    m_pe->ifu_l2_q = &this->core->inst_l2_q[static_cast<int>(LSUType::VECTOR_LSU)];
    m_pe->snp_l2_q = &this->core->snp_l2_q[static_cast<int>(LSUType::VECTOR_LSU)];
    m_pe->l2_ifu_q = &this->core->l2_ifu_array[static_cast<int>(LSUType::VECTOR_LSU)][peID];
    m_pe->pe_load_nf_q  = &this->core->coreInterface.load_non_flush_q[m_coreId];
    m_pe->pe_store_nf_q  = &this->core->coreInterface.store_non_flush_q[m_coreId];
    m_pe->pe_flush_rpt_q = this->core->coreInterface.pe_flush_rpt_array[peID];
    m_pe->lsu_pe_rslv_array = this->core->coreInterface.lsu_pe_rslv_array[peID];
    m_pe->peID = peID;
    m_pe->vectorCore = this;
    m_pe->stack_error_pc_q = this->core->sRenameUnit->stack_error_pc_q;
    // m_pe->iexType = static_cast<ExecEngineTyp>(static_cast<uint32_t>(ExecEngineTyp::IEX_NUM) + m_coreId);
    m_pe->lgprRF = m_lgprRF;
    m_pe->vrfRename = m_vrfRename;
    m_pe->Build();

    for (uint32_t tid = 0; tid < this->core->configs.threadCount; ++tid) {
        ASSERT(m_grob);
        m_pe->prob[tid]->m_grob = m_grob;
    }

    m_peIfuQ = &m_pe->pe_ifu_q;
    m_workLoadQ.resize(this->core->configs.threadCount);
    for (uint32_t tid = 0; tid < m_workLoadQ.size(); ++tid) {
        m_workLoadQ[tid] = &(m_pe->workThdQ[tid]);
    }

    m_vrfRename->vecPE = &m_pe->decoder; // 仅做数据统计，可以传输其他数据结构指针。
    m_vrfRename->InitVecPMU();
}

void VectorCore::BuildSIMTIEX()
{
    // Build simt iex
    uint32_t iexIdx = static_cast<uint32_t>(ExecEngineTyp::IEX_NUM) + m_coreId;
    m_iex = std::make_shared<IEX>();
    m_iex->core = this->core;
    m_iex->debugLogger = this->core->debug_log;
    m_iex->id = static_cast<ExecEngineTyp>(iexIdx);
    m_iex->coreId = m_coreId;
    m_iex->machineType = MachineType::VIEX;
    m_iex->Build();
    m_iex->iex_brob_rslvblk = &this->core->coreInterface.iex_brob_rslvblk[iexIdx];
    m_iex->iex_flush_rpt_q = this->core->coreInterface.iex_flush_rpt_q[iexIdx];
    m_iex->rtable.block_rtable_retire_q = this->core->coreInterface.block_rtable_retire_q;
    m_iex->rtable.vrfRtableTagFreeQ = &m_vrfRename->vrfRtableTagFreeQ;
    m_iex->rtable.vrfRtableTagRetireQ = &m_vrfRename->vrfRtableTagRetireQ;
    m_iex->rtable.rtable_release_q = this->core->coreInterface.rtable_release_q;
    m_iex->name = " SIMT IEX ";
    m_iex->iexVcoreReqQ = &this->core->coreInterface.iexVcoreReqQ[m_coreId];
    m_iex->vcoreIexResQ = &this->core->coreInterface.vcoreIexResQ[m_coreId];
    m_iex->vcoreIexLdResQ = vcoreIexLdResQ;
    m_iex->vcoreIexStResQ = vcoreIexStResQ;
    m_iex->resolveBlock = &this->core->coreInterface.resolveBlock;
    m_iex->BuildSupplement();
}

void VectorCore::ConnectSIMTIEXIntf()
{
    // Initial For IEX-LSU
    this->core->coreInterface.iex_lsu_lda_array[SIMT_IEX] = m_iex->iex_lsu_lda_array;
    this->core->coreInterface.lsu_iex_lret_array[SIMT_IEX] = m_iex->lsu_iex_lret_array;
    this->core->coreInterface.iex_lsu_sta_array[SIMT_IEX] = m_iex->iex_lsu_sta_array;
    this->core->coreInterface.iex_lsu_std_array[SIMT_IEX] = m_iex->iex_lsu_std_array;

    uint32_t iexIdx = static_cast<uint32_t>(ExecEngineTyp::IEX_NUM) + m_coreId;
    uint32_t peID = this->core->configs.stdPeCount + m_coreId;
    // m_pe->pe_iex_cmd_q = this->core->coreInterface.pe_iex_cmd_array[iexIdx][peID]; // cmd_pipe 不接vecpe
    m_pe->pe_iex_alu_q = this->core->coreInterface.pe_iex_alu_array[iexIdx][peID];
    // m_pe->pe_iex_lda_q = this->core->coreInterface.pe_iex_lda_array[iexIdx][peID];
    // m_pe->pe_iex_sta_q = this->core->coreInterface.pe_iex_sta_array[iexIdx][peID];
    m_pe->pe_iex_std_q = this->core->coreInterface.pe_iex_std_array[iexIdx][peID];
    // m_pe->pe_iex_bru_q = this->core->coreInterface.pe_iex_bru_array[iexIdx][peID];
    m_pe->pe_iex_vec_agu_q = this->core->coreInterface.pe_iex_vec_agu_array[iexIdx][peID];
    m_pe->pe_iex_vec_scalar_q = this->core->coreInterface.pe_iex_vec_scalar_array[iexIdx][peID];
    m_iex->dispatchUnit.pe_iex_cmd_array[peID] = this->core->coreInterface.pe_iex_cmd_array[iexIdx][peID];
    m_iex->dispatchUnit.pe_iex_alu_array[peID] = this->core->coreInterface.pe_iex_alu_array[iexIdx][peID];
    m_iex->dispatchUnit.pe_iex_lda_array[peID] = this->core->coreInterface.pe_iex_lda_array[iexIdx][peID];
    m_iex->dispatchUnit.pe_iex_sta_array[peID] = this->core->coreInterface.pe_iex_sta_array[iexIdx][peID];
    m_iex->dispatchUnit.pe_iex_std_array[peID] = this->core->coreInterface.pe_iex_std_array[iexIdx][peID];
    m_iex->dispatchUnit.pe_iex_bru_array[peID] = this->core->coreInterface.pe_iex_bru_array[iexIdx][peID];
    m_iex->dispatchUnit.pe_iex_vec_agu_array[peID] = this->core->coreInterface.pe_iex_vec_agu_array[iexIdx][peID];
    m_iex->dispatchUnit.pe_iex_vec_scalar_array[peID] = this->core->coreInterface.pe_iex_vec_scalar_array[iexIdx][peID];
    // m_pe->rf_ct_q = m_iex->rf_ct_q[peID]; // vec 目前不需要支持传统ct 块的接口
    // m_pe->brRlsvQ = &m_iex->brRlsvQ;
    m_pe->iex_pe_rslv_array = this->core->coreInterface.iex_pe_rslv_array[iexIdx][peID];

    // todo: rob
    for (uint32_t tid = 0; tid < m_pe->prob.size(); tid++) {
        auto rob = m_pe->prob[tid];
        rob->iex_pe_rslv_q = this->core->coreInterface.iex_pe_rslv_array[iexIdx][peID][tid];
        rob->pe_iex_rd_q = this->core->coreInterface.pe_iex_rd_array[iexIdx][peID];
        rob->pe_rf_wr_req = this->core->coreInterface.pe_rf_wr_q[peID];
    }

    m_iex->iex_pe_rslv_array[peID] = this->core->coreInterface.iex_pe_rslv_array[iexIdx][peID];
    m_iex->rd.pe_iex_rd_array[peID] = this->core->coreInterface.pe_iex_rd_array[iexIdx][peID];

    m_iex->rf.pe_rf_wr_req = this->core->coreInterface.pe_rf_wr_q;
    m_iex->rf.data_vec_viex_q = &this->core->coreInterface.data_vec_viex_q[m_coreId];
    m_iex->rf.data_viex_vec_q = &this->core->coreInterface.data_viex_vec_q[m_coreId];
    m_iex->rf.gsDataVecViexQ = &this->core->coreInterface.gsDataVecViexQ[m_coreId];
    m_iex->rf.gsDataViexVecQ = &this->core->coreInterface.gsDataViexVecQ[m_coreId];
}

void VectorCore::Build()
{
    config.overrideDefaultConfig(GetSim()->getCfgs());
    ubConfig.overrideDefaultConfig(GetSim()->getCfgs());
    srcAddrMap.resize(GetSim()->core->configs.scalar_smt_thread);
    logicalGroups = config.vector_core_thread_num;
    m_stats.rpt = GetSim()->getRpt();
    m_stats.m_threadNum = logicalGroups;
    m_stats.Reset();
    m_ldReqArray.resize(config.vector_core_load_request_queue_size);
    ldArraySize = 0;
    m_stReqArray.resize(config.vector_core_store_request_queue_size);
    stArraySize = 0;
    m_laBuffer = std::vector<uint8_t>(ubConfig.tile_reg_size * KILO);
    m_taBuffer = std::vector<uint8_t>(ubConfig.tile_reg_size * KILO);
    m_taVld = std::vector<bool>(ubConfig.tile_reg_size * KILO);
    for (uint32_t i = 0; i < ubConfig.tile_reg_size * KILO; ++i) {
        m_taVld[i] = false;
    }

    for (uint32_t i = 0; i < m_stReqArray.size(); ++i) {
        m_stReqArray[i].status = VecRequestStatus::INVALID;
        m_stReqArray[i].valid = false;
    }
    m_memReqArray.resize(m_memReqArraySize);
    for (uint64_t i = 0; i < config.blockCmdBufferSize; i++) {
        m_blockCmdBufferIDPool.push_back(i);
    }
    m_scbDrainBusQ.resize(GetSim()->core->configs.scalar_smt_thread);
    m_scbDrainDoneBusQ.resize(GetSim()->core->configs.scalar_smt_thread);
    BuildVrf();
    BuildSrf();
    ConnectSRFIntf();
    InitGroupScheduler();
    InitVectorLoopManager();
    InitVectorGROB();
    InitVectorTA();
    BuildPE();
    InitVectorGBuffer();
    BuildSIMTIEX();
    ConnectSIMTIEXIntf();
}

void VectorCore::Report_load_latency()
{
    m_stats.Report_load_latency();
}

bool VectorCore::IsVecBusy()
{
    for (uint32_t stid = 0; stid < GetSim()->core->configs.scalar_smt_thread; ++stid) {
        if (!m_grob->IsIdle(stid)) {
            return true;
        }
    }

    return false;
}

void VectorCore::CheckBusy()
{
    bool currentBusy = IsVecBusy();
    if (currentBusy && !lastCycleVectorBusy) {
        // free to busy
        vectorBusyStartCycle = GetSim()->getCycles();
    }
    if (!currentBusy && lastCycleVectorBusy) {
        // busy to free
        SwimLogData logData;
        logData.name = "Vector busy";
        logData.pid = machineId;
        logData.tid = CORE_INTER_VIEW_TID;
        logData.sTime = vectorBusyStartCycle;
        logData.eTime = GetSim()->getCycles();
        logData.eventId = GetSim()->GetEventId();
        logData.hint = "Vector busy";
        GetSim()->AddDuration(logData);
    }
    lastCycleVectorBusy = currentBusy;
}

void VectorCore::LoadWakeUp(MemRequest& req)
{
    m_iex->VecLoadWakeUp(req);
}

void VectorCore::InsertBlockCmdBuffer(BlockCommandPtr cmd)
{
    uint64_t ptr = m_blockCmdBufferIDPool[0];
    m_blockCmdBufferIDPool.erase(m_blockCmdBufferIDPool.begin());
    cmd->execMachineId = machineId + ptr;
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << m_coreId << "] vec block cmd alloc " << ptr << " to "
        << cmd->Dump();
    ASSERT(m_blockCmdBuffer[ptr] == nullptr);
    m_blockCmdBuffer[ptr] = cmd;
}

void VectorCore::DeallocBlockCmdBuffer(BlockCommandPtr grobCmd)
{
    DeleteBidEntry(grobCmd->bid, grobCmd->stid);
    uint64_t index = grobCmd->execMachineId - machineId;
    ASSERT(m_blockCmdBuffer[index] != nullptr);
    ASSERT(m_blockCmdBuffer[index]->bid == grobCmd->bid);
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << m_coreId << "] vec block cmd dealloc " << index << " free "
        << grobCmd->Dump();
    m_blockCmdBuffer[index] = nullptr;
    m_blockCmdBufferIDPool.push_back(index);
}

void VectorCore::FlushCmdBuffer(FlushBus& bus)
{
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << m_coreId << "] flush block cmd, " << bus;
    for (auto it = m_blockCmdBuffer.begin(); it != m_blockCmdBuffer.end(); it++) {
        if (it->second == nullptr) {
            continue;
        }
        if (bus.req.stid == it->second->stid && LessEqual(bus.req.bid, it->second->bid)) {
            cout << " [VECTOR " << m_coreId << "] block cmd dealloc by flush, bid:" << it->second->bid << endl;
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << m_coreId << "] block cmd dealloc by flush, bid:" <<
                it->second;
            m_blockCmdBuffer[it->first] = nullptr;
            m_blockCmdBufferIDPool.push_back(it->first);
        }
    }
}

void VectorCore::ReportStat()
{
    m_pe->ReportStat();
    std::string preName = "Tile LSU";
    m_stats.Report(preName, m_coreId);
    m_iex->ReportStat();
    RptVecRenameStats();
}

void VectorCore::Reset()
{
    m_pe->Reset();
    m_iex->Reset();

    m_laBuffer.clear();
    for (auto &req : m_ldReqArray) {
        req.status = VecRequestStatus::INVALID;
    }
    ldArraySize = 0;
    for (auto &req : m_stReqArray) {
        req.status = VecRequestStatus::INVALID;
        req.valid = false;
    }
    stArraySize = 0;
    return;
}

SimSys* VectorCore::GetSim()
{
    return sim;
}

const VectorCoreConfig& VectorCore::GetConfig() const
{
    return config;
}

uint64_t VectorCore::GetRequestCount(uint32_t shapeSize, uint32_t elementSize)
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

uint64_t GetReqQueueFreeCount(std::vector<VecRequest> &reqArray)
{
    uint64_t count = 0;
    for (VecRequest &r : reqArray) {
        if (r.status == VecRequestStatus::INVALID) {
            count++;
        }
    }

    return count;
}

void VectorCore::Flush()
{
    if (m_bccVectorFlushQ->Empty()) {
        return;
    }

    m_bccVectorFlushQ->Read();
    Reset();
}

void VectorCore::DoCommonFlush(FlushBus& bus)
{
    // TODO add tid on each part for correct flush
    m_vecta->Flush(bus);
    FlushCmdBuffer(bus);
    // Load/store input
    auto matchReq = [&bus](MemRequest &req) {
        if (bus.req.stid == req.stid) {
            return false;
        }
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
    vcoreIexLdResQ->FlushIf(matchReq);
    vcoreIexStResQ->FlushIf(matchReq);

    auto matchDrain = [&bus](ScbDrainBus &req) {
        if (bus.req.stid == req.stid) {
            return false;
        }
        return LessEqual(bus.req.bid, req.bid);
    };
    m_scbDrainBusQ[bus.req.stid].FlushIf(matchDrain);
    m_scbDrainDoneBusQ[bus.req.stid].FlushIf(matchDrain);

    auto matchMskFile = [&bus](MaskReleaseInfo &info) {
        if (info.stid != bus.req.stid) {
            return false;
        }
        return LessEqual(bus.req.bid, info.bid);
    };
    m_GROB2MaskFile.FlushIf(matchMskFile);

    m_vectorLM->SetFlush(bus);

    auto matchAllocReq = [&bus](VCore::GBufferAllocReq &info) {
        if (info.blockCmd->stid != bus.req.stid) {
            return false;
        }
        return LessEqual(bus.req.bid, info.blockCmd->bid);
    };
    if (bus.baseOnBid) {
        m_lm2GBufferQ.FlushIf(matchAllocReq);
    }

    auto matchGrobAllocInfo = [&bus](GroupAllocInfo &info) {
        if (info.stid != bus.req.stid) {
            return false;
        }
        return LessEqual(bus.req.bid, info.bid);
    };
    if (bus.baseOnBid) {
        m_lm2GROB.FlushIf(matchGrobAllocInfo);
    }

    auto matchIssueInfo = [&bus](GroupIssueInfo &info) {
        if (info.stid != bus.req.stid) {
            return false;
        }
        return LessEqual(bus.req.bid, info.bid);
    };
    if (bus.baseOnBid) {
        m_GBuffer2GROB.FlushIf(matchIssueInfo);
    }

    // Ifu info flush
    auto matchFetchState = [&bus](BlkBodyFetchState &req) {
        if (req.stid != bus.req.stid) {
            return false;
        }
        return LessEqual(bus.req.bid, req.bid);
    };
    auto matchBlkInfo = [&bus](BlockRunInfo &req) {
        if (req.stid != bus.req.stid) {
            return false;
        }
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
    // TODO: unused code
    auto matchMemLdReq = [&bus](TTransMemLdReq &req) {
        return LessEqual(bus.req.bid, req.GetBid());
    };
    m_vecMemLdReqQ->FlushIf(matchMemLdReq);

    auto matchMemStReq = [&bus](TTransMemStReq &req) {
        return LessEqual(bus.req.bid, req.GetBid());
    };
    m_vecMemStReqQ->FlushIf(matchMemStReq);

    auto matchIexReq = [&bus](MemRequest& req) {
        if (req.stid != bus.req.stid) {
            return false;
        }
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
    m_pe->Flush(bus);
    m_iex->setFlush(bus);
}

void VectorCore::Replay(FlushBus& bus)
{
    m_gBuffer->SetFlush(bus);
    DoCommonFlush(bus);
}

void VectorCore::SetFlush(FlushBus &bus)
{
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "Vec Top Recv Flush " << bus;
    m_gBuffer->SetFlush(bus);
    DoCommonFlush(bus);
}

void VectorCore::HandleDataRet(MissMemReq& missReq)
{
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
    ASSERT(GetSim()->core->IsVecPe(req.thread));
    m_memReqQ.Write(req);
}

void VectorCore::HandleDataRet(MemRequest& memReq)
{
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
    ASSERT(GetSim()->core->IsVecPe(memReq.thread));
    m_memReqQ.Write(memReq);
}

void VectorCore::HandleMissReturn()
{
    vector<uint32_t> addrs;
    for (auto it = m_vecRet.begin(); it != m_vecRet.end(); ++it) {
        if (it->second) {
            addrs.push_back(it->first);

            for (auto& req : m_vecLoad[it->first]) {
                HandleDataRet(req);
                LOG_INFO_M(Unit::VECTOR, Stage::NA) << "Response Load request " << req.req << ". And addr: 0x"
                    << std::hex << it->first << " is returned";
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

void VectorCore::HandleMemRequest()
{
    // 将 req 发送到 TA 中
    auto &readQ = iexVcoreReqQ->GetRawReadData();
    for (auto it = readQ.begin(); it != readQ.end();) {
        MemRequest req = *it;
        it = readQ.erase(it);
        if (req.isLoad) {
            ASSERT(GetSim()->core->IsVecPe(req.thread));
            taLoadQ->Write(req);
        } else {
            ASSERT(GetSim()->core->IsVecPe(req.thread));
            taStoreQ->Write(req);
        }
    }
}

bool VectorCore::GenVecMemLdReq(MemRequest req)
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
            ldReq.SetCoreId(m_coreId);
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << "Load request split " << req << ". And addr is 0x" << hex << addr;
            m_vecMemLdReqQ->Write(ldReq);
        }
        req.val = true;
        m_memReqArray[i] = req;
        return true;
    }
    return false;
}

bool VectorCore::GenVecMemStReq(MemRequest req)
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
            stReq.SetCoreId(m_coreId);
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << "Store request split " << req << ". And addr is 0x" << hex << addr;
            m_vecMemStReqQ->Write(stReq);
        }
        req.val = true;
        m_memReqArray[i] = req;
        return true;
    }
    return false;
}

void VectorCore::HandleMemLdResp()
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
        LOG_INFO_M(Unit::VECTOR, Stage::NA) << "Store request response " << *req;
        m_memReqQ.Write(*req);
    }
}

void VectorCore::HandleMemStResp()
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

ROBID VectorCore::GetOldestGid(uint32_t stid)
{
    return m_grob->GetOldestGid(stid);
}

ROBID VectorCore::GetOldestBid(uint32_t stid)
{
    return m_grob->GetOldestBid(stid);
}

void VectorCore::Dump()
{
    m_vecta->Dump();
    m_vectorLM->Dump();
    for (uint32_t i = 0; i < m_pe->prob.size(); ++i) {
         m_pe->prob[i]->PrintStatus();
    }
    m_vectorGS->Dump();
}

void VectorCore::PrintCfg()
{
    GetSim()->core->SetConfigs("vector_lane_number", GetSim()->core->configs.GetCondfigValue("simtLane"));
    GetSim()->core->SetConfigs("logic_group_parallel_cnt", config.GetCondfigValue("vector_core_thread_num"));
    GetSim()->core->SetConfigs("group_rob_entry_number", config.GetCondfigValue("vector_core_grob_depth"));
    GetSim()->core->SetConfigs("tile_lsu_cache_sets", config.GetCondfigValue("ta_set"));
    GetSim()->core->SetConfigs("tile_lsu_cache_ways", config.GetCondfigValue("ta_way"));
    GetSim()->core->SetConfigs("tile_lsu_ldq_entry", config.GetCondfigValue("ta_ldq_size"));
    GetSim()->core->SetConfigs("tile_lsu_stq_entry", config.GetCondfigValue("ta_stq_size"));
    GetSim()->core->SetConfigs("tile_lsu_scb_entry", config.GetCondfigValue("ta_scb_entry_size"));
    GetSim()->core->SetTmuConfigs("tile_register_size", ubConfig.GetCondfigValue("tile_reg_size"));
    GetSim()->core->SetTmuConfigs("spb_size", ubConfig.GetCondfigValue("spb_depth"));
    GetSim()->core->SetTmuConfigs("mgb_size", ubConfig.GetCondfigValue("mgb_depth"));
}

static std::string GetReasonString(IssueBlockReason reason)
{
    string reasonStr;
    switch (reason) {
        case IssueBlockReason::IQ_EMPTY:
            reasonStr = "IQ_EMPTY";
            break;
        case IssueBlockReason::CANT_ISSUE_NOT_READY:
            reasonStr = "CANT_ISSUE_NOT_READY";
            break;
        case IssueBlockReason::CANT_ISSUE_BUT_READY:
            reasonStr = "CANT_ISSUE_BUT_READY";
            break;
        case IssueBlockReason::CMD_BIOT_NOT_SELECTED:
            reasonStr = "CMD_BIOT_NOT_SELECTED";
            break;
        case IssueBlockReason::SSRSET_NOT_OLDEST:
            reasonStr = "SSRSET_NOT_OLDEST";
            break;
        case IssueBlockReason::SYS_STATE_STALL:
            reasonStr = "SYS_STATE_STALL";
            break;
        case IssueBlockReason::LD_NOT_IN_WINDOW:
            reasonStr = "LD_NOT_IN_WINDOW";
            break;
        case IssueBlockReason::ST_NOT_IN_WINDOW:
            reasonStr = "ST_NOT_IN_WINDOW";
            break;
        case IssueBlockReason::AGE_QUEUE_LDQ_LIMIT:
            reasonStr = "AGE_QUEUE_LDQ_LIMIT";
            break;
        case IssueBlockReason::NOT_PREV_ISSUED:
            reasonStr = "NOT_PREV_ISSUED";
            break;
        case IssueBlockReason::PIPE_STALL:
            reasonStr = "PIPE_STALL";
            break;
        case IssueBlockReason::CANT_ISSUE_EXEC_UNIT:
            reasonStr = "CANT_ISSUE_EXEC_UNIT";
            break;
        case IssueBlockReason::CANCEL_ISSUE_EXEC_UNIT:
            reasonStr = "CANCEL_ISSUE_EXEC_UNIT";
            break;
        case IssueBlockReason::CANCEL_ISSUE_ALU_RD_PORT:
            reasonStr = "CANCEL_ISSUE_ALU_RD_PORT";
            break;
        case IssueBlockReason::CANCEL_ISSUE_AGU_RD_PORT:
            reasonStr = "CANCEL_ISSUE_AGU_RD_PORT";
            break;
        default:
            break;
    }
    reasonStr = string("IssueBlockReason (") + reasonStr + ")";
    return reasonStr;
}

void VectorCore::ReportIEXStats()
{
    auto rpt = this->sim->getRpt();
    auto& s = this->core->bctrl->stats;
    rpt->ReportAvg("Vector SALU Utilization(Total Cycles)", m_iex->stats->sALUCycles,
        s->cycles);
    rpt->ReportAvg("Vector SALU Utilization(Vector Busy Cycles)", m_iex->stats->sALUCycles,
        this->core->vecBusyCycle);
    if (m_iex->configs.simt_iex_max_ex_no_rslv_cflt > 0) {
        for (auto& it : m_iex->stats->sAluExtendCycles) {
            std::string name = "Vector SALU Extended Cycles (" + std::to_string(it.first) + " Cycle)";
            rpt->ReportVal(name, static_cast<uint64_t>(it.second));
        }
    }
    rpt->ReportAvg("Vector VALU Utilization(Total Cycles)", m_iex->stats->vALUCycles,
        s->cycles);
    rpt->ReportAvg("Vector VALU Utilization(Vector Busy Cycles)", m_iex->stats->vALUCycles,
        this->core->vecBusyCycle);
    if (m_iex->configs.simt_iex_max_ex_no_rslv_cflt > 0) {
        for (auto& it : m_iex->stats->vAluExtendCycles) {
            std::string name = "Vector VALU Extended Cycles (" + std::to_string(it.first) + " Cycle)";
            rpt->ReportVal(name, static_cast<uint64_t>(it.second));
        }
    }
    for (uint32_t pickId = 0; pickId < m_iex->stats->vectorALUPickerIssueCnt.size(); pickId++) {
        auto& entry = m_iex->stats->vectorALUPickerIssueCnt.at(pickId);
        for (uint32_t cIdx = 0; cIdx < entry.size(); cIdx++) {
            string unitStr = IexLatency::GetExecUnitString(static_cast<IexExecUnit>(cIdx));
            string name = "Vector VALU Picker" + to_string(pickId) + " Issue (" + unitStr + ")";
            rpt->ReportVal(name, static_cast<uint64_t>(entry.at(cIdx)));
        }
    }
    rpt->ReportVal("Vector Regfile read port conflict cycle(src1)", m_iex->stats->src1Conflict);
    rpt->ReportVal("Vector Regfile read port conflict cycle(src2)", m_iex->stats->src2Conflict);
    for (uint32_t idx = 0; idx < static_cast<uint32_t>(IssueBlockReason::NUM); idx++) {
        auto reason = static_cast<IssueBlockReason>(idx);
        string reasonStr = GetReasonString(reason);
        uint64_t cnt = 0;
        if (m_iex->stats->vectorIssueBlock.find(reason) != m_iex->stats->vectorIssueBlock.cend()) {
            cnt = m_iex->stats->vectorIssueBlock.at(reason);
        }
        rpt->ReportVal(reasonStr, cnt);
    }
    for (uint32_t port = 0; port < m_iex->stats->vectorRdPortUsedCnt.size(); port++) {
        std::string name = "VRF read port " + std::to_string(port) + " used cnt";
        rpt->ReportVal(name, m_iex->stats->vectorRdPortUsedCnt.at(port));
    }
    for (uint32_t pipeSrcId = 0; pipeSrcId < m_iex->stats->vectorPipeSrcRdPortUsedCnt.size(); pipeSrcId++) {
        for (uint32_t port = 0; port < m_iex->stats->vectorPipeSrcRdPortUsedCnt.at(pipeSrcId).size(); port++) {
            std::string name = "VRF pipe " + RdPortControl::GetPipeSrcIdStr(
                static_cast<RdPortControl::PipeSrcId>(pipeSrcId)) + " read port " + std::to_string(port) + " used cnt";
            rpt->ReportVal(name, m_iex->stats->vectorPipeSrcRdPortUsedCnt.at(pipeSrcId).at(port));
        }
    }
    for (auto& itBank : m_iex->stats->vectorVrfBankRdPortUsedCnt) {
        for (auto& itCnt : itBank.second) {
            string statStr = "VRF Bank " + to_string(itBank.first) + " Rd " + to_string(itCnt.first);
            rpt->ReportVal(statStr, itCnt.second);
        }
    }
}

void VectorCore::ResetStats()
{
    m_iex->resetStats();

    for (uint32_t i = 0; i < m_pe->prob.size(); ++i) {
        m_iex->stats->slots += m_pe->prob[i]->getROBSize();
    }
}

uint64_t VectorCore::IexLdPipeCount()
{
    if (m_iex->configs.simt_separate_ld_st_pipe) {
        return m_iex->configs.simt_iex_agu_iq_count * (m_iex->configs.simt_iex_agu_iq_picker - 1);
    }
    return m_iex->configs.simt_iex_agu_iq_count * m_iex->configs.simt_iex_agu_iq_picker;
}

std::shared_ptr<IEX> VectorCore::GetIex()
{
    return m_iex;
}

std::shared_ptr<VecPE> VectorCore::GetPE()
{
    return m_pe;
}

void VectorCore::SetPE(std::shared_ptr<VecPE> pe)
{
    this->m_pe = pe;
}

void VectorCore::ReportPEStat()
{
    auto rpt = sim->getRpt();
    rpt->ReportVal("Vector Busy Cycle", m_stats.grobBusyCyc);
    rpt->ReportVal("Vector Retired Load Cnt", m_pe->stats->retiredLoadInst);
    rpt->ReportVal("Vector Retired Store Cnt", m_pe->stats->retiredStoreInst);
    rpt->ReportVal("Vector Retired ALU Cnt", m_pe->stats->retiredAluInst);
    rpt->ReportVal("  |--Vector Retired SALU Cnt", m_pe->stats->retiredSAluInst);
    rpt->ReportVal("  |--Vector Retired VALU Cnt", m_pe->stats->retiredVAluInst);
    if (m_stats.grobBusyCyc == 0) {
        rpt->ReportVal("Vector ALU IPC", 0.);
        rpt->ReportVal("Vector IPC", 0.);
    } else {
        rpt->ReportVal("Vector ALU IPC",
            static_cast<float>(m_pe->stats->retiredAluInst) / static_cast<float>(m_stats.grobBusyCyc));
        uint64_t totalInst = m_pe->stats->retiredLoadInst + m_pe->stats->retiredStoreInst + m_pe->stats->retiredAluInst;
        rpt->ReportVal("Vector IPC",
            static_cast<float>(totalInst) / static_cast<float>(m_stats.grobBusyCyc));
    }
}

void VectorCore::ReportVectorKeyStat()
{
    auto rpt = sim->getRpt();
    rpt->ReportTitle("Vector " + to_string(m_coreId) + " Key Stats");
    // inst cnt and ipc, busy cycle
    ReportPEStat();
    ReportIEXStats();

    rpt->ReportValAndPct("Vector VRF Ptag Usage(Ptag Bussy Cycle)",
        static_cast<float>(m_stats.vrfTagUtilization) / m_stats.vrfTagBussy, this->core->configs.vrf_preg_count);
    rpt->ReportValAndPct("Vector VRF MAPQ Usage(MAPQ Bussy Cycle)",
        static_cast<float>(m_stats.vrfMapQOccupiedSize) / m_stats.vrfMapQBussy,
        this->core->configs.vrf_mapq_depth * this->core->configs.threadCount);
    rpt->ReportValAndPct("Vector " + to_string(m_coreId) + " ROB Usage(Vector ROB Busy Cycles)",
        static_cast<float>(m_pe->stats->vecTotalRobBussyInstNum) / m_pe->stats->vecTotalRobBussyCyc,
        m_pe->configs.vpeROBDepth * core->configs.threadCount);
    for (uint32_t tid = 0; tid < this->core->configs.threadCount; ++tid) {
        string str = "  |--ROB[" + std::to_string(tid) + "]";
        rpt->ReportValAndPct(str,
            static_cast<float>(m_pe->stats->vecThreadRobBussyInstNum[tid]) / m_pe->stats->vecThreadRobBussyCyc[tid],
            m_pe->configs.vpeROBDepth);
    }
    ReportLoadPortUse(this->core->vecBusyCycle);
}

void VectorCore::ReportVectorTopDown()
{
    auto rpt = sim->getRpt();
    rpt->ReportTitle("Vector " + to_string(m_coreId) + " TopDown");

    uint32_t width = m_pe->configs.decodeWidth;
    uint64_t slots = width * m_stats.GetPeBussy();
    uint32_t decInst = m_stats.GetDecIns();
    uint64_t robStall = m_stats.GetPeRobStall();
    uint64_t regStall = m_stats.GetPeRegTagStall();
    uint64_t iqStall = m_stats.GetPeIqStall();
    uint64_t memBound = m_stats.GetPeMemoryBound();
    uint64_t peBound = robStall + regStall + iqStall + memBound;
    uint64_t frontend = slots - decInst - peBound;

    rpt->ReportPct("Frontend", frontend, slots);
    rpt->ReportPct("PE bound", peBound, slots);
    rpt->ReportPct("  |-- rob stall", robStall, slots);
    rpt->ReportPct("  |-- reg stall", regStall, slots);
    rpt->ReportPct("  |-- iq stall", iqStall, slots);
    rpt->ReportPct("  |-- mem bound", memBound, slots);
    rpt->ReportPct("Retiring", m_stats.GetPeRetireIns(), slots);
    rpt->ReportPct("Bad Speculation", decInst - m_stats.GetPeRetireIns(), slots);
}

void VectorCore::BuildSrf()
{
    m_lgprRF = std::make_shared<LGPRRF>();
    m_lgprRF->SetSim(this->sim);
    m_lgprRF->debugLogger = this->core->debug_log;
    m_lgprRF->Build();
}

void VectorCore::ConnectSRFIntf()
{
    m_lgprRF->lFreeListInput.mtc_rreq_vec_srf_q = &this->core->coreInterface.mtc_rreq_vec_srf_q;
    m_lgprRF->lFreeListInput.mtc_rrsp_srf_vec_q = &this->core->coreInterface.mtc_rrsp_srf_vec_q;
    m_lgprRF->lFreeListOutput.mtc_wreq_vec_srf_q = &this->core->coreInterface.mtc_wreq_vec_srf_q;
    m_lgprRF->lFreeListOutput.mtc_wrsp_srf_vec_q = &this->core->coreInterface.mtc_wrsp_srf_vec_q;

    m_lgprRF->lFreeListInput.rreq_vec_srf_q = &this->core->coreInterface.rreq_vec_srf_q[m_coreId];
    m_lgprRF->lFreeListInput.rrsp_srf_vec_q = &this->core->coreInterface.rrsp_srf_vec_q[m_coreId];
    m_lgprRF->lFreeListOutput.wreq_vec_srf_q = &this->core->coreInterface.wreq_vec_srf_q[m_coreId];
    m_lgprRF->lFreeListOutput.wrsp_srf_vec_q = &this->core->coreInterface.wrsp_srf_vec_q[m_coreId];

    m_lgprRF->lFreeListInput.gsRreqVecSrfQ = &this->core->coreInterface.gsRreqVecSrfQ[m_coreId];
    m_lgprRF->lFreeListInput.gsRrspSrfVecQ = &this->core->coreInterface.gsRrspSrfVecQ[m_coreId];
    m_lgprRF->lFreeListOutput.gsWreqVecSrfQ = &this->core->coreInterface.gsWreqVecSrfQ[m_coreId];
    m_lgprRF->lFreeListOutput.gsWrspSrfVecQ = &this->core->coreInterface.gsWrspSrfVecQ[m_coreId];
}

void VectorCore::BuildVrf()
{
    m_vrfRename = std::make_shared<VRFRename>();
    m_vrfRename->top = this;
    m_vrfRename->sim = this->sim;
    uint32_t threadCount = this->core->configs.threadCount;
    m_vrfRename->Build(threadCount, this->core->configs.vrf_mapq_depth, this->core->configs.vrf_preg_count);
}

void VectorCore::RptVecRenameStats()
{
    m_stats.vrfTagUtilization += m_vrfRename->GetTagUsedSize();
    m_stats.vrfMapQOccupiedSize += m_vrfRename->GetMapQSize();
    if (m_vrfRename->GetTagUsedSize() != 0) {
        ++m_stats.vrfTagBussy;
    }

    if (m_vrfRename->GetMapQSize() != 0) {
        ++m_stats.vrfMapQBussy;
    }
}

uint32_t VectorCore::GetSelectCore() const
{
    return top->GetSelectCore();
}

void VectorCore::ReportLoadPortUse(uint64_t vecBusyCycle)
{
    auto rpt = this->sim->getRpt();
    rpt->ReportAvg("Vector Load Port Utilization(Vector Busy Cycles)",
        m_stats.tile_total_load_request,
        vecBusyCycle);
    rpt->ReportAvg("Vector Load Request Average Used Cycles", m_stats.tile_total_load_lat,
        m_stats.tile_total_load_inst);
    rpt->ReportVal("  |--Vec load_lat_le_10_cycle", m_stats.tile_load_to_use[0]);
    rpt->ReportVal("  |--Vec load_lat_le_20_cycle", m_stats.tile_load_to_use[1]);
    rpt->ReportVal("  |--Vec load_lat_le_30_cycle", m_stats.tile_load_to_use[2]);
    rpt->ReportVal("  |--Vec load_lat_le_50_cycle", m_stats.tile_load_to_use[3]);
    rpt->ReportVal("  |--Vec load_lat_ge_50_cycle", m_stats.tile_load_to_use[4]);

    rpt->ReportAvg("Vector Group Parallel(GBuffer Busy Cycles)",
        m_stats.gBufferBussyGroupNum, m_stats.gBufferBussyCyc);
}

uint32_t VectorCore::GetIdleSpace() const
{
    return m_blockCmdBufferIDPool.size();
}

void VectorCore::RegisterTileAddr(ROBID bid, uint32_t stid, uint32_t addr, uint32_t size)
{
    auto& vec = srcAddrMap[stid][bid];
    vec.emplace_back(bid, addr, size);
}

bool VectorCore::IsInAddrRange(ROBID bid, uint32_t stid, uint32_t addr)
{
    for (auto it = srcAddrMap[stid].cbegin(); it != srcAddrMap[stid].cend(); ++it) {
        if (it->first == bid) {
            const std::vector<TileSrcAddr>& vec = it->second;
            for (const auto& tile : vec) {
                if (addr >= tile.startAddr && addr < tile.startAddr + tile.size) {
                    return true;
                }
            }
            break;
        }
    }
    return false;
}

bool VectorCore::DeleteBidEntry(ROBID bid, uint32_t stid)
{
    for (auto it = srcAddrMap[stid].cbegin(); it != srcAddrMap[stid].cend();) {
        if (it->first == bid) {
            it = srcAddrMap[stid].erase(it);
            return true;
        }
        it++;
    }
    return false;
}

} // namespace JCore
