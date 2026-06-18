#include "veclite/VectorLiteTop.h"
#include "core/Core.h"

namespace JCore {
VectorLiteTop::VectorLiteTop() {}
VectorLiteTop::~VectorLiteTop() {}

void VectorLiteTop::Work()
{
    for (auto &vec : m_vecLiteCores) {
        vec->Work();
    }
    ChangedSelectCore();
}

void VectorLiteTop::Xfer()
{
    for (auto &vec : m_vecLiteCores) {
        vec->Xfer();
    }
}

void VectorLiteTop::Reset()
{
    for (auto &vec : m_vecLiteCores) {
        vec->Reset();
    }
}

void VectorLiteTop::SetSim(SimSys *sm)
{
    this->sim = sm;
}

SimSys *VectorLiteTop::GetSim()
{
    return this->sim;
}

Core *VectorLiteTop::GetCore()
{
    return this->core;
}

void VectorLiteTop::SetCore(Core *c)
{
    this->core = c;
}

void VectorLiteTop::Build()
{
    BuildVecCore();
}

void VectorLiteTop::BuildVecCore()
{
    m_vecLiteCores.resize(this->core->configs.simtPeCount);
    for (uint32_t i = 0; i < m_vecLiteCores.size(); i++) {
        auto &vecCore = m_vecLiteCores[i];
        vecCore = std::make_shared<VectorLite>(i);
        vecCore->top = this;
        vecCore->pTileUtils = &this->core->tileUtils;
        vecCore->m_tileRegVecLdResQ = &this->core->coreInterface.tileRegVecLdResArray[i];
        vecCore->m_vecTileRegLdReqQ = &this->core->coreInterface.vecTileRegLdReqArray[i];
        vecCore->m_tileRegVecWrResQ = &this->core->coreInterface.tileRegVecWrResArray[i];
        vecCore->m_vecTileRegStReqQ = &this->core->coreInterface.vecTileRegStReqArray[i];
        vecCore->machineType = MachineType::VECLITE;
        vecCore->machineId = GetProcessID(MachineType::VECLITE, i);
        sim->ObjRegisterLogInfo(vecCore, i, sim->core->globalSeqToSwimPtr++);
        vecCore->Build();
    }
}

uint32_t VectorLiteTop::GetVecNum()
{
    return m_vecLiteCores.size();
}

void VectorLiteTop::InitRing(std::shared_ptr<CrossStation> &station)
{
    for (auto &vecCore : m_vecLiteCores) {
        station->InitCoreBuffer(m_vecLiteCores.size());
        vecCore->AddNode(station);
        vecCore->stationReadRange = this->core->tileUtils.GetUBConfig()->vec_read_port.size();
    }
}

void VectorLiteTop::InitDualPERing(unsigned coreIdx, std::shared_ptr<CrossStation> &station)
{
    auto &vecCore = m_vecLiteCores[coreIdx];
    station->InitCoreBuffer(m_vecLiteCores.size());
    vecCore->AddNode(station);
    vecCore->stationReadRange = 2;
}

void VectorLiteTop::PrintConfigs()
{
    for (auto &vecCore : m_vecLiteCores) {
        vecCore->PrintCfg();
    }
}

void VectorLiteTop::ReportStat()
{
    for (auto &vecCore : m_vecLiteCores) {
        vecCore->ReportStat();
    }
}

uint32_t VectorLiteTop::BussyCount()
{
    uint32_t count = 0;
    for (auto &vecCore : m_vecLiteCores) {
        if (vecCore->IsVecBusy()) {
            ++count;
        }
    }

    return count;
}

void VectorLiteTop::Dump()
{
    for (auto &vecCore : m_vecLiteCores) {
        vecCore->Dump();
    }
}

void VectorLiteTop::Replay(const FlushBus &bus) const
{
    (void)bus;
}

void VectorLiteTop::SetFlush(const FlushBus &bus)
{
    for (auto &vecCore : m_vecLiteCores) {
        vecCore->SetFlush(bus);
    }
}


void VectorLiteTop::ResetStats()
{
    for (auto &vecCore : m_vecLiteCores) {
        vecCore->ResetStats();
    }
}


void VectorLiteTop::ChangedSelectCore()
{
    selectCore = 0;
    uint32_t idleSpace = 0;
    for (uint32_t coreId = 0; coreId < m_vecLiteCores.size(); ++coreId) {
        if (m_vecLiteCores[coreId]->GetIdleSpace() > idleSpace) {
            selectCore = coreId;
            idleSpace = m_vecLiteCores[coreId]->GetIdleSpace();
        }
    }
}

uint32_t VectorLiteTop::GetSelectCore() const
{
    return selectCore;
}

const VectorLiteCoreConfig& VectorLiteTop::GetConfig() const
{
    return m_vecLiteCores[0]->GetConfig();
}

bool VectorLiteTop::CheckLoadQEmpty(uint32_t coreId) const
{
    (void)coreId;
    return false;
}

uint32_t VectorLiteTop::GetVecPEDecWidth() const
{
    ASSERT(core != nullptr);
    return core->GetVecPEDecWidth();
}

} // namespace JCore
