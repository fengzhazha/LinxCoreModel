#include "vectorcore/VectorTop.h"
#include "core/Core.h"

namespace JCore {
VectorTop::VectorTop() {}
VectorTop::~VectorTop() {}

void VectorTop::Work()
{
    for (auto &vec : m_vecCores) {
        vec->Work();
    }
    ChangedSelectCore();
}

void VectorTop::Xfer()
{
    for (auto &vec : m_vecCores) {
        vec->Xfer();
    }
}

void VectorTop::Reset()
{
    for (auto &vec : m_vecCores) {
        vec->Reset();
    }
}

void VectorTop::SetSim(SimSys *sm)
{
    this->sim = sm;
}

SimSys *VectorTop::GetSim()
{
    return this->sim;
}

Core *VectorTop::GetCore()
{
    return this->core;
}

void VectorTop::SetCore(Core *c)
{
    this->core = c;
}

void VectorTop::Build()
{
    BuildVecCore();
}

void VectorTop::BuildVecCore()
{
    m_vecCores.resize(this->core->configs.simtPeCount);
    for (uint32_t i = 0; i < m_vecCores.size(); i++) {
        auto &vecCore = m_vecCores[i];
        vecCore = std::make_shared<VectorCore>(i);
        vecCore->top = this;
        vecCore->core = this->core;
        vecCore->sim = this->sim;
        vecCore->pTileUtils = &this->core->tileUtils;
        vecCore->bccVecBlockCmdQ = &this->core->coreInterface.bccVecBlockCmdQ;
        vecCore->vecBCCWakeupQ = &this->core->coreInterface.vecBCCWakeupQ;
        vecCore->bccMCallBlockCmdQ = &this->core->coreInterface.bccMcallBlockCmdQ;
        vecCore->vecBridgeRspQ = &this->core->vecBridgeRspQ;
        vecCore->vecBridgeReqQ = &this->core->vecBridgeReqQ;
        vecCore->vecMCallWakeupQ = &this->core->coreInterface.schedulerBCCRslvQ;
        vecCore->m_vecMemLdReqQ = &this->core->coreInterface.vecCoreMemLdReqArray[i];
        vecCore->m_vecMemLdResQ = &this->core->coreInterface.vecCoreMemLdResArray[i];
        vecCore->m_vecMemStReqQ = &this->core->coreInterface.vecCoreMemStReqArray[i];
        vecCore->m_vecMemStResQ = &this->core->coreInterface.vecCoreMemStResArray[i];
        vecCore->m_tileRegVecLdResQ = &this->core->coreInterface.tileRegVecLdResArray[i];
        vecCore->m_vecTileRegLdReqQ = &this->core->coreInterface.vecTileRegLdReqArray[i];
        vecCore->m_tileRegVecLdRetryQ = &this->core->coreInterface.tileRegVecLdRetryArray[i];
        vecCore->m_tileRegVecWrResQ = &this->core->coreInterface.tileRegVecWrResArray[i];
        vecCore->m_vecTileRegStReqQ = &this->core->coreInterface.vecTileRegStReqArray[i];
        vecCore->m_tileRegVecStRetryQ = &this->core->coreInterface.tileRegVecStRetryArray[i];
        vecCore->iexVcoreReqQ = &this->core->coreInterface.iexVcoreReqQ[i];
        vecCore->vcoreIexResQ = &this->core->coreInterface.vcoreIexResQ[i];
        vecCore->vcoreIexLdResQ = &this->core->coreInterface.vcoreLdIexResQ[i];
        vecCore->vcoreIexStResQ = &this->core->coreInterface.vcoreStIexResQ[i];
        vecCore->resolveBlock = &this->core->coreInterface.resolveBlock;
        vecCore->m_vecBCCCreditQ = &this->core->coreInterface.vecBCCCreditArray[i];
        vecCore->m_bccVectorFlushQ = &this->core->coreInterface.bCCVecFlushArray[i];
        vecCore->loadNonFlushQ = &this->core->coreInterface.load_non_flush_q[i];
        vecCore->storeNonFlushQ = &this->core->coreInterface.store_non_flush_q[i];
        vecCore->m_vecBridgeMemReqQ = this->core->coreInterface.vecBridgeMemReqQ;
        vecCore->m_bridgeVecLdRetQ = this->core->coreInterface.bridgeVecLdRetQ;
        vecCore->m_bridgeVecStRslvQ = this->core->coreInterface.bridgeVecStRslvQ;
        vecCore->toClearQ = &this->core->coreInterface.toClearArray[i];
        vecCore->machineType = MachineType::VECTOR;
        vecCore->machineId = GetProcessID(MachineType::VECTOR, i, vecCore->config.blockCmdBufferSize);
        vecCore->Build();
        this->core->peArray.push_back(vecCore->GetPE());
        vecCore->m_vectorLM->req_vec_siex_q = &this->core->coreInterface.req_vec_siex_q;
        vecCore->m_vectorLM->rsp_siex_vec_q = &this->core->coreInterface.rsp_siex_vec_q;
        vecCore->m_vectorLM->rreq_vec_srf_q = &this->core->coreInterface.rreq_vec_srf_q[i];
        vecCore->m_vectorLM->rrsp_srf_vec_q = &this->core->coreInterface.rrsp_srf_vec_q[i];
        vecCore->m_vectorLM->wreq_vec_srf_q = &this->core->coreInterface.wreq_vec_srf_q[i];
        vecCore->m_vectorLM->wrsp_srf_vec_q = &this->core->coreInterface.wrsp_srf_vec_q[i];
        vecCore->m_vectorLM->data_vec_viex_q = &this->core->coreInterface.data_vec_viex_q[i];
        vecCore->m_vectorLM->data_viex_vec_q = &this->core->coreInterface.data_viex_vec_q[i];
        vecCore->m_vectorLM->stashCmdQ = &this->core->coreInterface.stashCmdQ;
        vecCore->m_vectorLM->stashAllocDoneQ = &this->core->coreInterface.stashAllocDoneArray[i];
        vecCore->m_vectorLM->stashRslvQ = &this->core->coreInterface.stashRslvArray[i];
        
        vecCore->m_vectorGS->gsReqVecSiexQ = &this->core->coreInterface.gsReqVecSiexQ;
        vecCore->m_vectorGS->gsRspSiexVecQ = &this->core->coreInterface.gsRspSiexVecQ;
        vecCore->m_vectorGS->gsRreqVecSrfQ = &this->core->coreInterface.gsRreqVecSrfQ[i];
        vecCore->m_vectorGS->gsRrspSrfVecQ = &this->core->coreInterface.gsRrspSrfVecQ[i];
        vecCore->m_vectorGS->gsWreqVecSrfQ = &this->core->coreInterface.gsWreqVecSrfQ[i];
        vecCore->m_vectorGS->gsWrspSrfVecQ = &this->core->coreInterface.gsWrspSrfVecQ[i];
        vecCore->m_vectorGS->gsDataVecViexQ = &this->core->coreInterface.gsDataVecViexQ[i];
        vecCore->m_vectorGS->gsDataViexVecQ = &this->core->coreInterface.gsDataViexVecQ[i];

        vecCore->m_grob->stashCompQ = &this->core->coreInterface.stashCompQ;
        sim->ObjRegisterLogInfo(vecCore, i, this->core->globalSeqToSwimPtr++);
        sim->RegisterMultiThread(vecCore, vecCore->config.blockCmdBufferSize, i);
        vecCore->groupMachineId = GetProcessID(MachineType::VECTOR_WL, i, vecCore->config.vector_core_thread_num);
        std::string name = MachineName(MachineType::VECTOR_WL) + std::to_string(i);
        std::string machineViewName = MachineName(MachineType::VECTOR_WL) + "_Machine_View";
        auto logger = GetSim()->GetSwimLogger();
        if (logger != nullptr) {
            logger->SetProcessName(name, vecCore->groupMachineId, this->core->globalSeqToSwimPtr++);
            logger->SetThreadName(machineViewName, vecCore->groupMachineId, CORE_INTER_VIEW_TID);
            for (uint64_t grp = 0; grp < vecCore->config.vector_core_thread_num; grp++) {
                logger->SetThreadName(name + "_GROUP" + std::to_string(grp), CORE_TOP_MACHINE_ID,
                                      vecCore->groupMachineId + grp);
            }
        }
    }
}

uint32_t VectorTop::GetVecNum()
{
    return m_vecCores.size();
}

void VectorTop::InitRing(std::shared_ptr<CrossStation> &station)
{
    for (auto &vecCore : m_vecCores) {
        station->InitCoreBuffer(m_vecCores.size());
        vecCore->AddNode(station);
        vecCore->ldRingport0Id = (this->core->tileUtils.GetUBConfig()->vec_read_port[0]);
        vecCore->stationReadRange = this->core->tileUtils.GetUBConfig()->vec_read_port.size();
    }
}

void VectorTop::InitDualPERing(unsigned coreIdx, std::shared_ptr<CrossStation> &station)
{
    auto &vecCore = m_vecCores[coreIdx];
    station->InitCoreBuffer(m_vecCores.size());
    vecCore->AddNode(station);
    vecCore->ldRingport0Id = vecCore->pTileUtils->GetUBConfig()->vec_read_port[0 + coreIdx * 2];
    vecCore->stationReadRange = 2;
}

void VectorTop::PrintConfigs()
{
    for (auto &vecCore : m_vecCores) {
        vecCore->PrintCfg();
    }
}

void VectorTop::ReportIEXStats()
{
    for (auto &vecCore : m_vecCores) {
        vecCore->ReportIEXStats();
    }
}

void VectorTop::ReportTileLoadLatDis()
{
    for (auto &vecCore : m_vecCores) {
        vecCore->Report_load_latency();
    }
}

void VectorTop::ReportVectorKeyStat()
{
    for (auto &vecCore : m_vecCores) {
        vecCore->ReportVectorKeyStat();
    }
}

void VectorTop::ReportVectorTopDown()
{
    for (auto &vecCore : m_vecCores) {
        vecCore->ReportVectorTopDown();
    }
}

void VectorTop::ReportStat()
{
    for (auto &vecCore : m_vecCores) {
        vecCore->ReportStat();
    }
}

uint32_t VectorTop::BussyCount()
{
    uint32_t count = 0;
    for (auto &vecCore : m_vecCores) {
        if (vecCore->IsVecBusy()) {
            ++count;
        }
    }

    return count;
}

ROBID VectorTop::GetOldestGid(uint32_t stid)
{
    bool foundId = false;
    ROBID bid;
    ROBID gid;
    for (auto &vecCore : m_vecCores) {
        ROBID bidTmp = vecCore->GetOldestBid(stid);
        ROBID gidTmp = vecCore->GetOldestGid(stid);
        if (!foundId || LessEqual(bidTmp, bid)) {
            bid = bidTmp;
            gid = gidTmp;
            foundId = true;
        }
    }

    return gid;
}

void VectorTop::Dump()
{
    for (auto &vecCore : m_vecCores) {
        vecCore->Dump();
    }
}

void VectorTop::Replay(FlushBus &bus)
{
    for (auto &vecCore : m_vecCores) {
        vecCore->Replay(bus);
    }
}

void VectorTop::SetFlush(FlushBus &bus)
{
    for (auto &vecCore : m_vecCores) {
        vecCore->SetFlush(bus);
    }
}

uint32_t VectorTop::GetLdqCredit(uint32_t coreId)
{
    return m_vecCores[coreId]->m_vecta->ldQ_credit;
}

void VectorTop::SubLdqCredit(uint32_t coreId, uint32_t num)
{
    m_vecCores[coreId]->m_vecta->ldQ_credit -= num;
}

uint32_t VectorTop::GetStqCredit(uint32_t coreId, uint32_t tid)
{
    return m_vecCores[coreId]->m_vecta->stQ_credit[tid];
}

void VectorTop::SubStqCredit(uint32_t coreId, uint32_t tid, uint32_t num)
{
    m_vecCores[coreId]->m_vecta->stQ_credit[tid] -= num;
}

uint64_t VectorTop::GetTileLdqSize(uint32_t coreId)
{
    return m_vecCores[coreId]->GetConfig().ta_ldq_size;
}

uint64_t VectorTop::GetTileStqSize(uint32_t coreId)
{
    return m_vecCores[coreId]->GetConfig().ta_stq_size;
}

void VectorTop::ResetStats()
{
    for (auto &vecCore : m_vecCores) {
        vecCore->ResetStats();
    }
}

uint64_t VectorTop::IexLdPipeCount(uint32_t coreId)
{
    return m_vecCores[coreId]->IexLdPipeCount();
}

std::shared_ptr<IEX> VectorTop::GetIex(uint32_t coreId)
{
    ASSERT(coreId < m_vecCores.size());
    return m_vecCores[coreId]->GetIex();
}

std::shared_ptr<VecPE> VectorTop::GetPE(uint32_t coreId)
{
    ASSERT(coreId < m_vecCores.size());
    return m_vecCores[coreId]->GetPE();
}

void VectorTop::SetPE(std::shared_ptr<VecPE> pe, uint32_t coreId)
{
    ASSERT(coreId < m_vecCores.size());
    return m_vecCores[coreId]->SetPE(pe);
}

void VectorTop::ChangedSelectCore()
{
    selectCore = 0;
    uint32_t idleSpace = 0;
    for (uint32_t coreId = 0; coreId < m_vecCores.size(); ++coreId) {
        if (m_vecCores[coreId]->GetIdleSpace() > idleSpace) {
            selectCore = coreId;
            idleSpace = m_vecCores[coreId]->GetIdleSpace();
        }
    }
}

uint32_t VectorTop::GetSelectCore() const
{
    return selectCore;
}

const VectorCoreConfig& VectorTop::GetConfig() const
{
    return m_vecCores[0]->GetConfig();
}

void VectorTop::SetPeBussy(uint32_t coreId, uint64_t cycle)
{
    m_vecCores[coreId]->m_stats.SetPeBussy(cycle);
}

void VectorTop::SetPeRegTagStall(uint32_t coreId, uint64_t cycle)
{
    m_vecCores[coreId]->m_stats.SetPeBussy(cycle);
}

void VectorTop::SetPeRobStall(uint32_t coreId, uint64_t cycle)
{
    m_vecCores[coreId]->m_stats.SetPeRobStall(cycle);
}

void VectorTop::SetPeMemoryBound(uint32_t coreId, uint64_t cycle)
{
    m_vecCores[coreId]->m_stats.SetPeMemoryBound(cycle);
}

void VectorTop::SetDecIns(uint32_t coreId, uint64_t insts)
{
    m_vecCores[coreId]->m_stats.SetDecIns(insts);
}

void VectorTop::ReduceDecIns(uint32_t coreId, uint64_t insts)
{
    m_vecCores[coreId]->m_stats.ReduceDecIns(insts);
}

void VectorTop::SetPeIqStall(uint32_t coreId, uint64_t cycle)
{
    m_vecCores[coreId]->m_stats.SetPeIqStall(cycle);
}

void VectorTop::SetPeRetireIns(uint32_t coreId, uint64_t insts)
{
    m_vecCores[coreId]->m_stats.SetPeRetireIns(insts);
}

bool VectorTop::CheckLoadQEmpty(uint32_t coreId)
{
    return m_vecCores[coreId]->m_vecta->CheckLoadQEmpty();
}

} // namespace JCore
