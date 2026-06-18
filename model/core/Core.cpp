#include "core/Core.h"

#include <iomanip>
#include <new>
#include <string>
#include <tuple>

#include "core/Bus.h"
#include "core/SRename.h"
#include "lsu/lsu.h"
#include "ModelSpec.h"
#include "SimSys.h"
#include "plat/DebugLog.h"
#include "utils/util.h"
#include "l2/L2Cache.h"
#if defined GENERIC_SOC || defined GENERIC_SOC_NEW
    #include "generic_soc/soc_wrapper.h"
#else
    #include "soc/soc_wrapper.h"
#endif
#include "mtccore/lsu/MtcLoadStoreUnit.h"
#include "vectorcore/VectorTop.h"
#include "DFX/InstTracer.h"

namespace JCore {

namespace {

template <class T>
void SimArrayConnect(std::vector<SimQueue<T>*> &dstArray, std::vector<SimQueue<T>> &srcArray)
{
    dstArray.resize(srcArray.size());
    for (size_t i = 0; i < srcArray.size(); i++) {
        dstArray[i] = &srcArray[i];
    }
}

template <class T>
void SimArrayWork(std::vector<SimQueue<T>> &simArray)
{
    for (auto &simQ : simArray) {
        simQ.Work();
    }
}

}

void Core::BuildFlushUnit()
{
    // Build Flush Control
    flushUnit = std::make_shared<FlushControl>();
    flushUnit->core = this;
    flushUnit->sim = sim;
    flushUnit->machineType = MachineType::FLUSH_UNIT;
    flushUnit->machineId = GetProcessID(MachineType::FLUSH_UNIT, 0);
    flushUnit->debugLogger = debug_log;
    flushUnit->Build();
    sim->addModule(flushUnit);
}

void Core::BuildBCC()
{
    // Build Block Control
    bctrl = std::make_shared<BCtrlUnit>();
    bctrl->core = this;
    bctrl->sim = sim;
    bctrl->machineType = MachineType::BCC;
    bctrl->machineId = GetProcessID(MachineType::BCC, 0);
    sim->ObjRegisterLogInfo(bctrl, 0, globalSeqToSwimPtr++);
    bctrl->debugLogger = debug_log;

    uint32_t iexNum = static_cast<uint32_t>(ExecEngineTyp::IEX_NUM) + configs.simtPeCount;
    bctrl->blockROB.iex_brob_rslvblk.resize(iexNum);
    for (uint32_t i = 0; i < iexNum; ++i) {
        bctrl->blockROB.iex_brob_rslvblk[i] = &coreInterface.iex_brob_rslvblk[i];
    }
    bctrl->blockROB.bcc_flush_rpt_q = coreInterface.bcc_flush_rpt_q;
    bctrl->blockROB.block_rtable_retire_q = coreInterface.block_rtable_retire_q;
    bctrl->Build();
    bctrl->scalarPE->d2Stage.ggprRenameUnit.rtable_release_q = coreInterface.rtable_release_q;
    sim->addModule(bctrl);
}

void Core::BuildStackRename()
{
    // Build stack rename unit
    sRenameUnit = std::make_shared<StackRenameUnit>();
    sRenameUnit->core = this;
    sRenameUnit->sim = sim;
    sRenameUnit->debugLogger = debug_log;
    sRenameUnit->Build();
    sim->addModule(sRenameUnit);
}

uint32_t Core::GetPECount()
{
    uint32_t peCount = configs.stdPeCount + configs.simtPeCount;
    if (configs.mtc_separate_enable) {
        peCount += configs.mtc_core_num;
    }
    return peCount;
}

uint32_t Core::GetRelativePEID(uint32_t peID, ExecEngineTyp peType)
{
    switch (peType) {
        case ExecEngineTyp::SCALAR_IEX:
            return peID;
            break;
        case ExecEngineTyp::SIMT_IEX:
            if (peID >= configs.stdPeCount) {
                return peID - configs.stdPeCount;
            }
            break;
        case ExecEngineTyp::MEM_IEX:
            if (peID >= configs.stdPeCount + configs.simtPeCount) {
                return peID - configs.stdPeCount - configs.simtPeCount;
            }
            break;
        default:
            break;
    }
    return peID;
}

uint32_t Core::GetVecPENum() const
{
    return configs.stdPeCount;
}

uint32_t Core::GetMtcPEIndex() const
{
    return (configs.stdPeCount + configs.simtPeCount);
}

void Core::BuildSTDPE()
{
    // Build standard PE
    // uint32_t peID = 0;
    uint32_t peCount = GetPECount();
    uint32_t scCnt = sim->core->configs.scalar_lsu_num;
    uint32_t vcCnt = sim->core->configs.vector_lsu_num;
    uint32_t totalLsuCnt = scCnt + vcCnt;
    if (configs.mtc_separate_enable) {
        uint32_t mtcCnt = sim->core->configs.mtccore_lsu_num;
        totalLsuCnt += mtcCnt;
    }
    l2_ifu_array.resize(totalLsuCnt);
    for (auto &array : l2_ifu_array) {
        array.resize(peCount);
    }
    bctrl->scalarPE->pe_flush_rpt_q = coreInterface.pe_flush_rpt_array[bctrl->scalarPE->peID];
    bctrl->scalarPE->rtable_release_q = coreInterface.rtable_release_q;
    bctrl->scalarPE->lsu_pe_rslv_array = coreInterface.lsu_pe_rslv_array[bctrl->scalarPE->peID];
    bctrl->scalarPE->stack_error_pc_q = sRenameUnit->stack_error_pc_q;
}

// merge with build stdpe
void Core::SetPEAttr()
{
    uint32_t peCount = GetPECount();
    for (uint64_t i = (configs.stdPeCount +  configs.simtPeCount); i < peCount; i++) {
        peArray[i]->iexType = MEM_IEX;
    }
}

void Core::BuildIEX()
{
    for (uint32_t i = 0; i < static_cast<uint32_t>(IEX_NUM); ++i) {
        iex[i] = nullptr;
    }

    // Build iex
    iex[SCALAR_IEX] = std::make_shared<IEX>();
    iex[SCALAR_IEX]->core = this;
    iex[SCALAR_IEX]->debugLogger = debug_log;
    iex[SCALAR_IEX]->id = SCALAR_IEX;
    iex[SCALAR_IEX]->machineType = MachineType::SIEX;
    iex[SCALAR_IEX]->Build();
    iex[SCALAR_IEX]->iex_brob_rslvblk = &coreInterface.iex_brob_rslvblk[SCALAR_IEX];
    iex[SCALAR_IEX]->iex_flush_rpt_q = coreInterface.iex_flush_rpt_q[SCALAR_IEX];
    iex[SCALAR_IEX]->rtable.block_rtable_retire_q = coreInterface.block_rtable_retire_q;
    iex[SCALAR_IEX]->rtable.rtable_release_q = coreInterface.rtable_release_q;
    SimArrayConnect(iex[SCALAR_IEX]->tTransTileRegLdReqArray, coreInterface.tTransTileRegLdReqArray);
    SimArrayConnect(iex[SCALAR_IEX]->tTransTileRegStReqArray, coreInterface.tTransTileRegStReqArray);
    iex[SCALAR_IEX]->name = " Scalar IEX ";
    sim->addModule(iex[SCALAR_IEX]);
}

void Core::ConnectIEXIntf()
{
    // Initial For IEX-LSU
    coreInterface.iex_lsu_lda_array[SCALAR_IEX] = iex[SCALAR_IEX]->iex_lsu_lda_array;
    coreInterface.lsu_iex_lret_array[SCALAR_IEX] = iex[SCALAR_IEX]->lsu_iex_lret_array;
    coreInterface.iex_lsu_sta_array[SCALAR_IEX] = iex[SCALAR_IEX]->iex_lsu_sta_array;
    coreInterface.iex_lsu_std_array[SCALAR_IEX] = iex[SCALAR_IEX]->iex_lsu_std_array;

    // Build PE interface
    for (uint64_t i = 0; i < configs.stdPeCount; i++) {
        bctrl->scalarPE->pe_iex_cmd_array.emplace_back(coreInterface.pe_iex_cmd_array[SCALAR_IEX][i]);
        bctrl->scalarPE->pe_iex_alu_array.emplace_back(coreInterface.pe_iex_alu_array[SCALAR_IEX][i]);
        bctrl->scalarPE->pe_iex_lda_array.emplace_back(coreInterface.pe_iex_lda_array[SCALAR_IEX][i]);
        bctrl->scalarPE->pe_iex_sta_array.emplace_back(coreInterface.pe_iex_sta_array[SCALAR_IEX][i]);
        bctrl->scalarPE->pe_iex_std_array.emplace_back(coreInterface.pe_iex_std_array[SCALAR_IEX][i]);
        bctrl->scalarPE->pe_iex_bru_array.emplace_back(coreInterface.pe_iex_bru_array[SCALAR_IEX][i]);
        iex[SCALAR_IEX]->dispatchUnit.pe_iex_cmd_array[i] = coreInterface.pe_iex_cmd_array[SCALAR_IEX][i];
        iex[SCALAR_IEX]->dispatchUnit.pe_iex_alu_array[i] = coreInterface.pe_iex_alu_array[SCALAR_IEX][i];
        iex[SCALAR_IEX]->dispatchUnit.pe_iex_lda_array[i] = coreInterface.pe_iex_lda_array[SCALAR_IEX][i];
        iex[SCALAR_IEX]->dispatchUnit.pe_iex_sta_array[i] = coreInterface.pe_iex_sta_array[SCALAR_IEX][i];
        iex[SCALAR_IEX]->dispatchUnit.pe_iex_std_array[i] = coreInterface.pe_iex_std_array[SCALAR_IEX][i];
        iex[SCALAR_IEX]->dispatchUnit.pe_iex_bru_array[i] = coreInterface.pe_iex_bru_array[SCALAR_IEX][i];
        bctrl->scalarPE->dcTop.templateDev[i].rf_ct_q = iex[SCALAR_IEX]->rf_ct_q[i];
        // peArray[i]->rf_ct_q = iex[SCALAR_IEX]->rf_ct_q[i];
        // peArray[i]->brRlsvQ = &iex[SCALAR_IEX]->brRlsvQ;
    }

    // Build pe_iex interface
    for (uint64_t i = 0; i < configs.stdPeCount; i++) {
        std::shared_ptr<SPE> ope = dynamic_pointer_cast<SPE>(peArray[i]);
        ope->iex_pe_rslv_array = coreInterface.iex_pe_rslv_array[SCALAR_IEX][i];
        // todo: rob
        for (uint32_t tid = 0; tid < ope->prob.size(); ++tid) {
            auto rob = ope->prob[tid];
            rob->iex_pe_rslv_q = coreInterface.iex_pe_rslv_array[SCALAR_IEX][i][tid];
            rob->pe_iex_rd_q = coreInterface.pe_iex_rd_array[SCALAR_IEX][i];
        }
    }
    uint32_t peCount = GetPECount();
    for (uint32_t i = 0; i < peCount; i++) {
        iex[SCALAR_IEX]->iex_pe_rslv_array[i] = coreInterface.iex_pe_rslv_array[SCALAR_IEX][i];
        iex[SCALAR_IEX]->rd.pe_iex_rd_array[i] = coreInterface.pe_iex_rd_array[SCALAR_IEX][i];
    }

    iex[SCALAR_IEX]->rf.req_vec_siex_q = &coreInterface.req_vec_siex_q;
    iex[SCALAR_IEX]->rf.rsp_siex_vec_q = &coreInterface.rsp_siex_vec_q;

    iex[SCALAR_IEX]->rf.gsReqVecSiexQ = &coreInterface.gsReqVecSiexQ;
    iex[SCALAR_IEX]->rf.gsRspSiexVecQ = &coreInterface.gsRspSiexVecQ;

    iex[SCALAR_IEX]->rf.mtc_req_vec_siex_q = &coreInterface.mtc_req_vec_siex_q;
    iex[SCALAR_IEX]->rf.mtc_rsp_siex_vec_q = &coreInterface.mtc_rsp_siex_vec_q;
}

void Core::BuildCubeCore()
{
    auto cubeChainInfoMap = std::make_shared<std::map<uint64_t, map<uint32_t, vector<uint64_t>>>>();
    cubeCores.resize(configs.cube_core_num);
    for (uint32_t i = 0; i < cubeCores.size(); i++) {
        auto &cubeCore = cubeCores[i];
        cubeCore = std::make_shared<CubeCore>();
        cubeCore->core = this;
        cubeCore->coreIndex = i;
        cubeCore->mutiCoreChainInfo = cubeChainInfoMap;
        cubeCore->sim = sim;
        cubeCore->pTileUtils = &tileUtils;
        cubeCore->Build();
        cubeCore->machineType = MachineType::CUBE;
        cubeCore->machineId = GetProcessID(MachineType::CUBE, i, cubeCore->cfgs.blockCmdBufferSize);
        sim->ObjRegisterLogInfo(cubeCore, i, globalSeqToSwimPtr++);
        sim->RegisterMultiThread(cubeCore, cubeCore->cfgs.blockCmdBufferSize, i);
        cubeCore->bccCubeBlockCmdQ = &coreInterface.bccCubeBlockCmdQ[i];
        cubeCore->cubeBCCWakeupQ = &coreInterface.cubeBCCWakeupQ;
        for (size_t j = 0; j < configs.cube_thread_num; j++) {
            cubeCore->CubeBCCCreditQ[j] = &coreInterface.cubeBCCCreditArray[i];
            cubeCore->CubeTileLdReqQ[j] = &coreInterface.cubeTileRegLdReqArray[i];
            cubeCore->TileCubeLdResQ[j] = &coreInterface.tileRegCubeLdResArray[i];
            cubeCore->TileRegCubeLdRetryQ[j] = &coreInterface.tileRegCubeLdRetryArray[i];
            cubeCore->CubeTileStReqQ[j] = &coreInterface.cubeTileRegStReqArray[i];
            cubeCore->TileRegCubeStRetryQ[j] = &coreInterface.tileRegCubeStRetryArray[i];
            cubeCore->BCCCubeFlushQ[j] = &coreInterface.bCCCubeFlushArray[i];
            cubeCore->tileRegCubeWrResQ = &coreInterface.tileRegCubeWrResArray[i];
        }
        sim->addModule(cubeCore);
    }
    for (uint32_t i = 0; i < cubeCores.size(); i++) {
        auto &cubeCore = cubeCores[i];
        cubeCore->cubeCores = &cubeCores;
    }
}

void Core::BuildVeclite()
{
    vecliteTop = std::make_shared<VectorLiteTop>();
    vecliteTop->SetSim(this->sim);
    vecliteTop->SetCore(this);
    vecliteTop->Build();
    if (configs.simtEnable) {
        sim->addModule(vecliteTop);
    }
}

void Core::BuildVecCore()
{
    vectorTop = std::make_shared<VectorTop>();
    vectorTop->SetSim(this->sim);
    vectorTop->SetCore(this);
    vectorTop->Build();
    if (configs.simtEnable) {
        sim->addModule(vectorTop);
    }
}

void Core::BuildMTCCore()
{
    if (!configs.mtc_separate_enable) {
        return;
    }
    mtcCores.resize(configs.mtc_core_num);
    // const uint32_t secondSimtPENum = configs.stdPeCount + configs.simtPeCount;
    for (uint32_t i = 0; i < mtcCores.size(); i++) {
        auto &mtcCore = mtcCores[i];
        mtcCore = std::make_shared<MemoryCore>();
        mtcCore->machineType = MachineType::MTC;
        mtcCore->machineId = GetProcessID(MachineType::MTC, i);
        sim->ObjRegisterLogInfo(mtcCore, i, globalSeqToSwimPtr++);
        mtcCore->core = this;
        mtcCore->sim = sim;
        mtcCore->pTileUtils = &tileUtils;
        // mtcCore->m_peIfuQ = &(dynamic_pointer_cast<OPE>(peArray[secondSimtPENum]))->pe_ifu_q;
        mtcCore->m_workLoadQ.resize(configs.threadCount);
        // for (uint32_t tid = 0; tid < mtcCore->m_workLoadQ.size(); ++tid) {
        //     mtcCore->m_workLoadQ[tid] = &((dynamic_pointer_cast<OPE>(peArray[secondSimtPENum]))->workThdQ[tid]);
        // }
        mtcCore->bccMtcBlockCmdQ = &coreInterface.bccMTCBlockCmdQ;
        mtcCore->mtcBCCWakeupQ = &coreInterface.mtcBCCWakeupQ;

        mtcCore->m_vecMemLdReqQ = &coreInterface.mtcCoreMemLdReqArray[i];
        mtcCore->m_vecMemLdResQ = &coreInterface.mtcCoreMemLdResArray[i];
        mtcCore->m_vecMemStReqQ = &coreInterface.mtcCoreMemStReqArray[i];
        mtcCore->m_vecMemStResQ = &coreInterface.mtcCoreMemStResArray[i];

        mtcCore->m_tileRegVecLdResQ = &coreInterface.tileRegMtcLdResArray[i];
        mtcCore->m_tileRegVecWrResQ = &coreInterface.tileRegMtcWrResArray[i];
        mtcCore->m_tileRegVecLdRetryQ = &coreInterface.tileRegMtcLdRetryArray[i];
        mtcCore->m_vecTileRegLdReqQ = &coreInterface.mtcTileRegLdReqArray[i];
        mtcCore->m_vecTileRegStReqQ = &coreInterface.mtcTileRegStReqArray[i];
        mtcCore->m_tileRegVecStRetryQ = &coreInterface.tileRegMtcStRetryArray[i];
        mtcCore->iexVcoreReqQ = &coreInterface.iexMcoreReqQ;
        mtcCore->vcoreIexResQ = &coreInterface.mcoreIexResQ;
        mtcCore->scbStReqQ = &coreInterface.mtciexScbReqQ;
        mtcCore->scbStResQ = &coreInterface.mtcscbIexRspQ;
        mtcCore->resolveBlock = &coreInterface.resolveBlock;
        mtcCore->m_scaplerInstQ = &coreInterface.iexScalperInstQ;
        mtcCore->m_bccVectorFlushQ = &coreInterface.bCCMtcFlushArray[i];
        mtcCore->loadNonFlushQ = &coreInterface.mtc_load_non_flush_q;
        mtcCore->storeNonFlushQ = &coreInterface.mtc_store_non_flush_q;
        mtcCore->Build();
        mtcCore->coreId = i;

        mtcCore->m_vectorLM->req_vec_siex_q = &coreInterface.mtc_req_vec_siex_q;
        mtcCore->m_vectorLM->rsp_siex_vec_q = &coreInterface.mtc_rsp_siex_vec_q;
        mtcCore->m_vectorLM->rreq_vec_srf_q = &coreInterface.mtc_rreq_vec_srf_q;
        mtcCore->m_vectorLM->rrsp_srf_vec_q = &coreInterface.mtc_rrsp_srf_vec_q;
        mtcCore->m_vectorLM->wreq_vec_srf_q = &coreInterface.mtc_wreq_vec_srf_q;
        mtcCore->m_vectorLM->wrsp_srf_vec_q = &coreInterface.mtc_wrsp_srf_vec_q;
        mtcCore->m_vectorLM->data_vec_viex_q = &coreInterface.mtc_data_vec_viex_q;
        mtcCore->m_vectorLM->data_viex_vec_q = &coreInterface.mtc_data_viex_vec_q;

        // std::shared_ptr<OPE> ope = dynamic_pointer_cast<OPE>(peArray[secondSimtPENum]);
        // for (uint32_t tid = 0; tid < configs.threadCount; ++tid) {
        //     ASSERT(mtcCore->m_gBuffer);
        //     ope->prob[tid]->m_mtcgrob = mtcCore->m_grob;
        // }
        if (configs.simtEnable && configs.mtc_separate_enable) {
            sim->addModule(mtcCore);
        }
    }
}

void Core::BuildMEMIEX()
{
    if (!configs.mtc_separate_enable) {
        return;
    }

    // Build memory core iex
    iex[MEM_IEX] = std::make_shared<IEX>();
    iex[MEM_IEX]->core = this;
    iex[MEM_IEX]->debugLogger = debug_log;
    iex[MEM_IEX]->id = MEM_IEX;
    iex[MEM_IEX]->machineType = MachineType::MIEX;
    iex[MEM_IEX]->Build();
    iex[MEM_IEX]->iex_brob_rslvblk = &coreInterface.iex_brob_rslvblk[MEM_IEX];
    iex[MEM_IEX]->iex_flush_rpt_q = coreInterface.iex_flush_rpt_q[MEM_IEX];
    iex[MEM_IEX]->rtable.vrfRtableTagFreeQ = &vrfRename->vrfMtcRtableTagFreeQ;
    iex[MEM_IEX]->rtable.vrfRtableTagRetireQ = &vrfRename->vrfMtcRtableTagRetireQ;
    iex[MEM_IEX]->rtable.block_rtable_retire_q = coreInterface.block_rtable_retire_q;
    iex[MEM_IEX]->rtable.rtable_release_q = coreInterface.rtable_release_q;
    iex[MEM_IEX]->name = " Memory IEX ";
    iex[MEM_IEX]->iexVcoreReqQ = &coreInterface.iexMcoreReqQ;
    iex[MEM_IEX]->vcoreIexResQ = &coreInterface.mcoreIexResQ;
    iex[MEM_IEX]->resolveBlock = &coreInterface.mtcResolveBlock;
    iex[MEM_IEX]->iexScbReqQ = &coreInterface.mtciexScbReqQ;
    iex[MEM_IEX]->scbIexRspQ = &coreInterface.mtcscbIexRspQ;
    if (configs.simtEnable && configs.mtc_separate_enable) {
        sim->addModule(iex[MEM_IEX]);
    }
}

void Core::ConnectMEMIEXIntf()
{
    if (!configs.mtc_separate_enable) {
        return;
    }

    // Initial For IEX-LSU
    coreInterface.iex_lsu_lda_array[MEM_IEX] = iex[MEM_IEX]->iex_lsu_lda_array;
    coreInterface.lsu_iex_lret_array[MEM_IEX] = iex[MEM_IEX]->lsu_iex_lret_array;
    coreInterface.iex_lsu_sta_array[MEM_IEX] = iex[MEM_IEX]->iex_lsu_sta_array;
    coreInterface.iex_lsu_std_array[MEM_IEX] = iex[MEM_IEX]->iex_lsu_std_array;

    iex[MEM_IEX]->iexScalperInstQ = &coreInterface.iexScalperInstQ;

    // Build PE interface
    uint32_t peCount = GetPECount();
    // for (uint64_t i = (configs.stdPeCount + configs.simtPeCount); i < peCount; i++) {
    //     peArray[i]->pe_iex_alu_q = coreInterface.pe_iex_alu_array[MEM_IEX][i];
    //     if (configs.scalper_enable) {
    //         // 开启scalper后，发向MTC的指令通向scalper
    //         peArray[i]->pe_iex_lda_q = coreInterface.pe_iex_sc_lda_array[MEM_IEX][i];
    //     } else {
    //         peArray[i]->pe_iex_lda_q = coreInterface.pe_iex_lda_array[MEM_IEX][i];
    //     }
    //     peArray[i]->pe_iex_lda_q = coreInterface.pe_iex_sc_lda_array[MEM_IEX][i];
    //     peArray[i]->pe_iex_sta_q = coreInterface.pe_iex_sta_array[MEM_IEX][i];
    //     peArray[i]->pe_iex_std_q = coreInterface.pe_iex_std_array[MEM_IEX][i];
    //     peArray[i]->pe_iex_bru_q = coreInterface.pe_iex_bru_array[MEM_IEX][i];
    //     iex[MEM_IEX]->dispatchUnit.pe_iex_alu_array[i] = coreInterface.pe_iex_alu_array[MEM_IEX][i];
    //     if (configs.scalper_enable) {
    //     //  开启scalper后，发向MTC的指令经Dispatch发向scalper
    //         iex[MEM_IEX]->dispatchUnit.pe_iex_sc_lda_array[i] = coreInterface.pe_iex_sc_lda_array[MEM_IEX][i];
    //     }
    //     iex[MEM_IEX]->dispatchUnit.pe_iex_lda_array[i] = coreInterface.pe_iex_lda_array[MEM_IEX][i];
    //     iex[MEM_IEX]->dispatchUnit.pe_iex_sta_array[i] = coreInterface.pe_iex_sta_array[MEM_IEX][i];
    //     iex[MEM_IEX]->dispatchUnit.pe_iex_std_array[i] = coreInterface.pe_iex_std_array[MEM_IEX][i];
    //     iex[MEM_IEX]->dispatchUnit.pe_iex_bru_array[i] = coreInterface.pe_iex_bru_array[MEM_IEX][i];
    //     peArray[i]->rf_ct_q = iex[MEM_IEX]->rf_ct_q[i];
    //     peArray[i]->brRlsvQ = &iex[MEM_IEX]->brRlsvQ;
    // }
    // peArray[configs.stdPeCount + configs.simtPeCount]->pe_scalper_commit_q =  &coreInterface.pe_scalper_commit_q;
    // peArray[configs.stdPeCount + configs.simtPeCount]->peScbWrQ =  &coreInterface.peScbWrQ;
        // Build pe_iex interface
    // uint32_t startIndex = GetMtcPeID();
    // for (uint64_t i = startIndex; i < peCount; i++) {
    //     std::shared_ptr<VecPE> ope = dynamic_pointer_cast<VecPE>(peArray[i]);
    //     ope->iex_pe_rslv_array = coreInterface.iex_pe_rslv_array[MEM_IEX][i];
    //     // todo: rob
    //     for (uint32_t tid = 0; tid < ope->prob.size(); tid++) {
    //         auto rob = ope->prob[tid];
    //         rob->iex_pe_rslv_q = coreInterface.iex_pe_rslv_array[MEM_IEX][i][tid];
    //         rob->pe_iex_rd_q = coreInterface.pe_iex_rd_array[MEM_IEX][i];
    //         rob->pe_rf_wr_req = coreInterface.pe_rf_wr_q[i];
    //     }
    // }

    for (uint32_t i = 0; i < peCount; i++) {
        iex[MEM_IEX]->iex_pe_rslv_array[i] = coreInterface.iex_pe_rslv_array[MEM_IEX][i];
        iex[MEM_IEX]->rd.pe_iex_rd_array[i] = coreInterface.pe_iex_rd_array[MEM_IEX][i];
    }

    iex[MEM_IEX]->rf.pe_rf_wr_req = coreInterface.pe_rf_wr_q;

    iex[MEM_IEX]->rf.data_vec_viex_q = &coreInterface.mtc_data_vec_viex_q;
    iex[MEM_IEX]->rf.data_viex_vec_q = &coreInterface.mtc_data_viex_vec_q;
}

void Core::BuildTileReg()
{
    tileReg = std::make_shared<TileRegister>();
    tileReg->sim = sim;
    tileReg->machineType = MachineType::TMU;
    tileReg->machineId = GetProcessID(MachineType::TMU, 0);
    sim->ObjRegisterLogInfo(tileReg, 0, globalSeqToSwimPtr++);
    tileReg->pTileUtils = &tileUtils;
    tileReg->stashCtrlUnit = make_shared<StashCtrlUnit>();
    tileReg->stashCtrlUnit->coreNum = configs.simtPeCount;
    tileReg->Build();
    ConnectTileBus();
    SetTRDirectAaccelssLatency();
    ConnetIexBus();
    sim->addModule(tileReg);

    readNextPriority_ = MachineType::CUBE;
    writeNextPriority_ = MachineType::CUBE;
}

// TileBankScoreBoard 实现
TileBankScoreBoard::TileBankScoreBoard()
    : occupiedPE_(MachineType::UNKNOW_CORE),
    remainingCycles_(0),
    currentCycle_(0)
{
}

void TileBankScoreBoard::SetOccupancy(MachineType pe, int cycles)
{
    if (cycles > 0) {
        occupiedPE_ = pe;
        remainingCycles_ = cycles;
        currentCycle_ = 0;
    }
}

bool TileBankScoreBoard::Update()
{
    if (remainingCycles_ > 0) {
        currentCycle_++;
        if (currentCycle_ >= remainingCycles_) {
            // 占用结束
            remainingCycles_ = 0;
            occupiedPE_ = MachineType::UNKNOW_CORE;
            return false;
        }
        return true;
    }
    return false;
}

bool TileBankScoreBoard::IsOccupied() const
{
    return remainingCycles_ > 0;
}

MachineType TileBankScoreBoard::GetOccupiedPE() const
{
    return occupiedPE_;
}

int TileBankScoreBoard::GetRemainingCycles() const
{
    return remainingCycles_ > 0 ? remainingCycles_ - currentCycle_ : 0;
}

void TileBankScoreBoard::Reset()
{
    occupiedPE_ = MachineType::UNKNOW_CORE;
    remainingCycles_ = 0;
    currentCycle_ = 0;
}

bool Core::CanReadTileReg(MachineType pe)
{
    if (configs.perfectAaccelssRF) {
        return true;
    }
    if (GetReadOccupiedPE() == pe) {
        return false;
    }
    if (pe == MachineType::CUBE) {
        cubeReadRFReqCnt++;
    } else if (pe == MachineType::TMA) {
        tmaReadRFReqCnt++;
    } else {
        vectorReadRFReqCnt++;
    }
    // 检查读score board是否被占用
    if (readScoreBoard_.IsOccupied()) {
        if (pe == MachineType::CUBE) {
            cubeReadRFReqArbFailCnt++;
            if (GetReadOccupiedPE() == MachineType::TMA) {
                cubeRdBlockByTmaCnt++;
            } else {
                cubeRdBlockByVecCnt++;
            }
        } else if (pe == MachineType::TMA) {
            tmaReadRFReqArbFailCnt++;
            if (GetReadOccupiedPE() == MachineType::CUBE) {
                tmaRdBlockByCubeCnt++;
            } else {
                tmaRdBlockByVecCnt++;
            }
        } else {
            vectorReadRFReqArbFailCnt++;
            if (GetReadOccupiedPE() == MachineType::CUBE) {
                vecRdBlockByCubeCnt++;
            } else {
                vecRdBlockByTmaCnt++;
            }
        }
        return false;
    }
    // 检查是否轮到该PE
    if (readNextPriority_ != pe) {
        if (pe == MachineType::CUBE) {
            cubeReadRFReqArbFailByPriCnt++;
        } else if (pe == MachineType::TMA) {
            tmaReadRFReqArbFailByPriCnt++;
        } else {
            vectorReadRFReqArbFailByPriCnt++;
        }
        return false;
    }
    return true;
}

bool Core::CanWriteTileReg(MachineType pe)
{
    if (configs.perfectAaccelssRF) {
        return true;
    }
    if (GetWriteOccupiedPE() == pe) {
        return false;
    }
    if (pe == MachineType::CUBE) {
        cubeWriteRFReqCnt++;
    } else if (pe == MachineType::TMA) {
        tmaWriteRFReqCnt++;
    } else {
        vectorWriteRFReqCnt++;
    }
    // 检查写score board是否被占用
    if (writeScoreBoard_.IsOccupied()) {
        if (pe == MachineType::CUBE) {
            cubeWriteRFReqArbFailCnt++;
            if (GetWriteOccupiedPE() == MachineType::TMA) {
                cubeWrBlockByTmaCnt++;
            } else {
                cubeWrBlockByVecCnt++;
            }
        } else if (pe == MachineType::TMA) {
            tmaWriteRFReqArbFailCnt++;
            if (GetWriteOccupiedPE() == MachineType::CUBE) {
                tmaWrBlockByCubeCnt++;
            } else {
                tmaWrBlockByVecCnt++;
            }
        } else {
            vectorWriteRFReqArbFailCnt++;
            if (GetWriteOccupiedPE() == MachineType::CUBE) {
                vecWrBlockByCubeCnt++;
            } else {
                vecWrBlockByTmaCnt++;
            }
        }
        return false;
    }
    // 检查是否轮到该PE
    if (writeNextPriority_ != pe) {
        if (pe == MachineType::CUBE) {
            cubeWriteRFReqArbFailByPriorityCnt++;
        } else if (pe == MachineType::TMA) {
            tmaWriteRFReqArbFailByPriorityCnt++;
        } else {
            vectorWriteRFReqArbFailByPriorityCnt++;
        }
        return false;
    }
    return true;
}

void Core::AcquireReadTileReg(MachineType pe, int latency)
{
    if (configs.perfectAaccelssRF) {
        return;
    }
    ASSERT(!readScoreBoard_.IsOccupied());
    if (readNextPriority_ != pe) {
        std::cout << "@" << sim->getCycles() << " wrong get write port - PE: " << MachineName(pe) << ", priority PE:"
            << MachineName(readNextPriority_) << std::endl;
        ASSERT(false);
    }
    if (latency > 0) {
        readScoreBoard_.SetOccupancy(pe, latency);
    }
}

void Core::AcquireWriteTileReg(MachineType pe, int latency)
{
    if (configs.perfectAaccelssRF) {
        return;
    }
    if (writeScoreBoard_.IsOccupied()) {
        std::cout << "@" << sim->getCycles() << " wrong get write port - PE: " << MachineName(pe) << ", occ PE:"
            << MachineName(writeScoreBoard_.GetOccupiedPE()) << std::endl;
        ASSERT(false);
    }
    if (writeNextPriority_ != pe) {
        std::cout << "@" << sim->getCycles() << " wrong get write port - PE: " << MachineName(pe) << ", priority:"
            << MachineName(writeNextPriority_) << std::endl;
        ASSERT(false);
    }
    if (latency > 0) {
        writeScoreBoard_.SetOccupancy(pe, latency);
    }
}

bool Core::IsReadOccupied() const
{
    return readScoreBoard_.IsOccupied();
}

bool Core::IsWriteOccupied() const
{
    return writeScoreBoard_.IsOccupied();
}

MachineType Core::GetReadPriority() const
{
    return readNextPriority_;
}

MachineType Core::GetWritePriority() const
{
    return writeNextPriority_;
}

MachineType Core::GetReadOccupiedPE() const
{
    return readScoreBoard_.GetOccupiedPE();
}

MachineType Core::GetWriteOccupiedPE() const
{
    return writeScoreBoard_.GetOccupiedPE();
}

void Core::ResetTileRegArb()
{
    readScoreBoard_.Reset();
    writeScoreBoard_.Reset();
    readNextPriority_ = MachineType::CUBE;
    writeNextPriority_ = MachineType::CUBE;
}

void Core::UpdateNextPriority(MachineType& currentPriority) const
{
    switch (currentPriority) {
        case MachineType::CUBE:
            currentPriority = MachineType::VECLITE;
            break;
        case MachineType::VECLITE:
            currentPriority = MachineType::TMA;
            break;
        case MachineType::TMA:
            currentPriority = MachineType::CUBE;
            break;
        default:
            currentPriority = MachineType::CUBE;
            break;
    }
}

void Core::BuildLSU()
{
    uint32_t scCnt = configs.scalar_lsu_num;
    uint32_t vcCnt = configs.vector_lsu_num;
    uint32_t mtcCnt = configs.mtccore_lsu_num;
    for (uint32_t i = 0; i < (scCnt + vcCnt); ++i) {
        sim->core->memIntf.emplace_back(std::make_shared<LoadStoreUnit>());
    }

    if (configs.mtc_separate_enable) {
        for (uint32_t i = 0; i < mtcCnt; ++i) {
            sim->core->MtcmemIntf.emplace_back(std::make_shared<MtcLoadStoreUnit>());
        }
        BuildMTCLSU();
    }
    BuildScalarLSU();
    BuildVectorLSU();
}

void Core::BuildScalarLSU()
{
    // Build Scalar LSU Top
    int scalarLSUIdx = static_cast<int>(LSUType::SCALAR_LSU);
    memIntf[scalarLSUIdx] = std::make_shared<LoadStoreUnit>();
    memIntf[scalarLSUIdx]->typeId = LSUType::SCALAR_LSU;
    memIntf[scalarLSUIdx]->memID_s = 4;
    memIntf[scalarLSUIdx]->sim = sim;
    memIntf[scalarLSUIdx]->core = this;
    memIntf[scalarLSUIdx]->debugLogger = debug_log;
    memIntf[scalarLSUIdx]->brob = &bctrl->blockROB;
    memIntf[scalarLSUIdx]->pkt_in_q  = &memRetQ[scalarLSUIdx];
    memIntf[scalarLSUIdx]->pkt_out_q = &memReqQ[scalarLSUIdx];
    memIntf[scalarLSUIdx]->inst_l2_q = &inst_l2_q[scalarLSUIdx];
    memIntf[scalarLSUIdx]->hpref_l2_q = &hpref_l2_q[scalarLSUIdx];
    memIntf[scalarLSUIdx]->l2_inst_q = &l2_inst_q[scalarLSUIdx];
    memIntf[scalarLSUIdx]->snp_l2_q  = &snp_l2_q[scalarLSUIdx];
    memIntf[scalarLSUIdx]->iex_lsu_lda_array = coreInterface.iex_lsu_lda_array[SCALAR_IEX];
    memIntf[scalarLSUIdx]->iex_lsu_sta_array = coreInterface.iex_lsu_sta_array[SCALAR_IEX];
    memIntf[scalarLSUIdx]->iex_lsu_std_array = coreInterface.iex_lsu_std_array[SCALAR_IEX];
    memIntf[scalarLSUIdx]->lsu_iex_lret_array = coreInterface.lsu_iex_lret_array[SCALAR_IEX];

    memIntf[scalarLSUIdx]->intf.pref_lu_q = &coreInterface.pref_lu_array[scalarLSUIdx];
    memIntf[scalarLSUIdx]->intf.lu_pref_q = &coreInterface.lu_pref_array[scalarLSUIdx];
    memIntf[scalarLSUIdx]->intf.feedback_lu_pref_q = &coreInterface.feedback_lu_pref_array[scalarLSUIdx];
    memIntf[scalarLSUIdx]->intf.pref_l2_q = &coreInterface.pref_l2_array[scalarLSUIdx];
    memIntf[scalarLSUIdx]->intf.l2_pref_q = &coreInterface.l2_pref_array[scalarLSUIdx];
    memIntf[scalarLSUIdx]->intf.snoop_lu_l2_q = &coreInterface.snoop_lu_l2_array[scalarLSUIdx];
    memIntf[scalarLSUIdx]->intf.lookup_lu_l2_q = &coreInterface.lookup_lu_l2_array[scalarLSUIdx];
    memIntf[scalarLSUIdx]->intf.lookup_l2_l1_q = &coreInterface.lookup_l2_l1_array[scalarLSUIdx];

    memIntf[scalarLSUIdx]->intf.bccTMABlockCmdQ = &coreInterface.bccTMABlockCmdQ;
    memIntf[scalarLSUIdx]->intf.tmaBCCWakeupQ = &coreInterface.tmaBCCWakeupQ;
    memIntf[scalarLSUIdx]->intf.bccLsuTloadCommitQ = &coreInterface.bccLsuTloadCommitArray[scalarLSUIdx];
    memIntf[scalarLSUIdx]->intf.bccLsuTstoreCommitQ = &coreInterface.bccLsuTstoreCommitArray[scalarLSUIdx];
    SimArrayConnect(memIntf[scalarLSUIdx]->intf.lsuBridgeTloadArray, coreInterface.lsuBridgeTloadArray);
    SimArrayConnect(memIntf[scalarLSUIdx]->intf.lsuBridgeTstoreArray, coreInterface.lsuBridgeTstoreArray);

    memIntf[scalarLSUIdx]->lsu_pe_rslv_array  = coreInterface.lsu_pe_rslv_array;
    memIntf[scalarLSUIdx]->Build();
    // Initialized by the number of LSUs
    coreInterface.lsu_flush_rpt_q[scalarLSUIdx] = memIntf[scalarLSUIdx]->lsu_flush_rpt_q;
    // for tile transfer
    SimArrayConnect(memIntf[scalarLSUIdx]->tTransMemLdReqArray, coreInterface.tTransMemLdReqArray);
    SimArrayConnect(memIntf[scalarLSUIdx]->tTransMemLdResArray, coreInterface.tTransMemLdResArray);
    SimArrayConnect(memIntf[scalarLSUIdx]->tTransMemStReqArray, coreInterface.tTransMemStReqArray);
    SimArrayConnect(memIntf[scalarLSUIdx]->tTransMemStResArray, coreInterface.tTransMemStResArray);
    sim->addModule(memIntf[scalarLSUIdx]);
}

void Core::BuildVectorLSU()
{
    int vectorLSUIdx = static_cast<int>(LSUType::VECTOR_LSU);
    memIntf[vectorLSUIdx] = std::make_shared<LoadStoreUnit>();
    memIntf[vectorLSUIdx]->typeId = LSUType::VECTOR_LSU;
    memIntf[vectorLSUIdx]->memID_s = 5;
    memIntf[vectorLSUIdx]->sim = sim;
    memIntf[vectorLSUIdx]->core = this;
    memIntf[vectorLSUIdx]->debugLogger = debug_log;
    memIntf[vectorLSUIdx]->brob = &bctrl->blockROB;
    memIntf[vectorLSUIdx]->pkt_in_q  = &memRetQ[vectorLSUIdx];
    memIntf[vectorLSUIdx]->pkt_out_q = &memReqQ[vectorLSUIdx];
    memIntf[vectorLSUIdx]->inst_l2_q = &inst_l2_q[vectorLSUIdx];
    memIntf[vectorLSUIdx]->hpref_l2_q = &hpref_l2_q[vectorLSUIdx];
    memIntf[vectorLSUIdx]->l2_inst_q = &l2_inst_q[vectorLSUIdx];
    memIntf[vectorLSUIdx]->snp_l2_q  = &snp_l2_q[vectorLSUIdx];

    memIntf[vectorLSUIdx]->iex_lsu_lda_array = coreInterface.iex_lsu_lda_array[SIMT_IEX];
    memIntf[vectorLSUIdx]->iex_lsu_sta_array = coreInterface.iex_lsu_sta_array[SIMT_IEX];
    memIntf[vectorLSUIdx]->iex_lsu_std_array = coreInterface.iex_lsu_std_array[SIMT_IEX];
    memIntf[vectorLSUIdx]->lsu_iex_lret_array = coreInterface.lsu_iex_lret_array[SIMT_IEX];

    memIntf[vectorLSUIdx]->intf.pref_lu_q = &coreInterface.pref_lu_array[vectorLSUIdx];
    memIntf[vectorLSUIdx]->intf.lu_pref_q = &coreInterface.lu_pref_array[vectorLSUIdx];
    memIntf[vectorLSUIdx]->intf.feedback_lu_pref_q = &coreInterface.feedback_lu_pref_array[vectorLSUIdx];
    memIntf[vectorLSUIdx]->intf.pref_l2_q = &coreInterface.pref_l2_array[vectorLSUIdx];
    memIntf[vectorLSUIdx]->intf.l2_pref_q = &coreInterface.l2_pref_array[vectorLSUIdx];
    memIntf[vectorLSUIdx]->intf.snoop_lu_l2_q = &coreInterface.snoop_lu_l2_array[vectorLSUIdx];
    memIntf[vectorLSUIdx]->intf.lookup_lu_l2_q = &coreInterface.lookup_lu_l2_array[vectorLSUIdx];
    memIntf[vectorLSUIdx]->intf.lookup_l2_l1_q = &coreInterface.lookup_l2_l1_array[vectorLSUIdx];

    memIntf[vectorLSUIdx]->intf.bccTMABlockCmdQ = &coreInterface.bccTMABlockCmdQ;
    memIntf[vectorLSUIdx]->intf.tmaBCCWakeupQ = &coreInterface.tmaBCCWakeupQ;
    memIntf[vectorLSUIdx]->intf.bccLsuTloadCommitQ = &coreInterface.bccLsuTloadCommitArray[vectorLSUIdx];
    memIntf[vectorLSUIdx]->intf.bccLsuTstoreCommitQ = &coreInterface.bccLsuTstoreCommitArray[vectorLSUIdx];
    SimArrayConnect(memIntf[vectorLSUIdx]->intf.lsuBridgeTloadArray, coreInterface.lsuBridgeTloadArray);
    SimArrayConnect(memIntf[vectorLSUIdx]->intf.lsuBridgeTstoreArray, coreInterface.lsuBridgeTstoreArray);

    memIntf[vectorLSUIdx]->lsu_pe_rslv_array  = coreInterface.lsu_pe_rslv_array;
    memIntf[vectorLSUIdx]->Build();
    // Initialized by the number of LSUs
    coreInterface.lsu_flush_rpt_q[vectorLSUIdx] = memIntf[vectorLSUIdx]->lsu_flush_rpt_q;
    // for tile transfer
    SimArrayConnect(memIntf[vectorLSUIdx]->tTransMemLdReqArray, coreInterface.tTransMemLdReqArray);
    SimArrayConnect(memIntf[vectorLSUIdx]->tTransMemLdResArray, coreInterface.tTransMemLdResArray);
    SimArrayConnect(memIntf[vectorLSUIdx]->tTransMemStReqArray, coreInterface.tTransMemStReqArray);
    SimArrayConnect(memIntf[vectorLSUIdx]->tTransMemStResArray, coreInterface.tTransMemStResArray);
    if (configs.simtEnable) {
        sim->addModule(memIntf[vectorLSUIdx]);
    }
}

uint32_t Core::GetMLSUScalpersize()
{
    return MtcmemIntf[0]->GetScalpersize();
}

void Core::BuildMTCLSU()
{
    if (!configs.mtc_separate_enable) {
        return;
    }

    int mtcLSUIdx = static_cast<int>(LSUType::MEMORY_LSU);
    MtcmemIntf[0]->typeId = LSUType::MEMORY_LSU;
    MtcmemIntf[0]->memIdS = 5;
    MtcmemIntf[0]->sim = sim;
    MtcmemIntf[0]->core = this;
    MtcmemIntf[0]->debugLogger = debug_log;
    MtcmemIntf[0]->brob = &bctrl->blockROB;
    MtcmemIntf[0]->pkt_in_q  = &memRetQ[mtcLSUIdx];
    MtcmemIntf[0]->pkt_out_q = &memReqQ[mtcLSUIdx];
    MtcmemIntf[0]->inst_l2_q = &inst_l2_q[mtcLSUIdx];
    MtcmemIntf[0]->hpref_l2_q = &hpref_l2_q[mtcLSUIdx];
    MtcmemIntf[0]->l2_inst_q = &l2_inst_q[mtcLSUIdx];
    MtcmemIntf[0]->snp_l2_q  = &snp_l2_q[mtcLSUIdx];
    MtcmemIntf[0]->mtciexScbReqQ = &coreInterface.mtciexScbReqQ;

    MtcmemIntf[0]->intf.pref_lu_q = &coreInterface.pref_lu_array[mtcLSUIdx];
    MtcmemIntf[0]->intf.lu_pref_q = &coreInterface.lu_pref_array[mtcLSUIdx];
    MtcmemIntf[0]->intf.feedback_lu_pref_q = &coreInterface.feedback_lu_pref_array[mtcLSUIdx];
    MtcmemIntf[0]->intf.pref_l2_q = &coreInterface.pref_l2_array[mtcLSUIdx];
    MtcmemIntf[0]->intf.l2_pref_q = &coreInterface.l2_pref_array[mtcLSUIdx];
    MtcmemIntf[0]->intf.snoop_lu_l2_q = &coreInterface.snoop_lu_l2_array[mtcLSUIdx];
    MtcmemIntf[0]->intf.lookup_lu_l2_q = &coreInterface.lookup_lu_l2_array[mtcLSUIdx];
    MtcmemIntf[0]->intf.lookup_l2_l1_q = &coreInterface.lookup_l2_l1_array[mtcLSUIdx];
    MtcmemIntf[0]->intf.iexScalperInstQ = &coreInterface.iexScalperInstQ;
    MtcmemIntf[0]->intf.pe_scalper_commit_q = &coreInterface.pe_scalper_commit_q;
    MtcmemIntf[0]->intf.peScbWrQ = &coreInterface.peScbWrQ;

    uint32_t peCount = GetPECount();
    uint32_t mtcPeIndex = GetMtcPeID();
    ASSERT(peCount == (mtcPeIndex + 1));
    if (configs.scalper_enable) {
        MtcmemIntf[0]->intf.pe_iex_lda_array = coreInterface.pe_iex_lda_array[MEM_IEX][mtcPeIndex];
    }
    memIntf[0]->intf.bccTMABlockCmdQ = &coreInterface.bccTMABlockCmdQ;
    memIntf[0]->intf.bccLsuTloadCommitQ = &coreInterface.bccLsuTloadCommitArray[mtcLSUIdx];
    memIntf[0]->intf.bccLsuTstoreCommitQ = &coreInterface.bccLsuTstoreCommitArray[mtcLSUIdx];

    MtcmemIntf[0]->Build();
    // Initialized by the number of LSUs
    coreInterface.lsu_flush_rpt_q[mtcLSUIdx] = MtcmemIntf[0]->lsu_flush_rpt_q;
    // for tile transfer
    SimArrayConnect(MtcmemIntf[0]->tTransMemLdReqArray, coreInterface.tTransMemLdReqArray);
    SimArrayConnect(MtcmemIntf[0]->tTransMemLdResArray, coreInterface.tTransMemLdResArray);
    SimArrayConnect(MtcmemIntf[0]->tTransMemStReqArray, coreInterface.tTransMemStReqArray);
    SimArrayConnect(MtcmemIntf[0]->tTransMemStResArray, coreInterface.tTransMemStResArray);
    if (configs.simtEnable) {
        sim->addModule(MtcmemIntf[0]);
    }
}

void Core::BuildSOC()
{
    if (configs.soc_enable) {
    #if defined GENERIC_SOC || defined GENERIC_SOC_NEW
        soc = std::make_shared<SocGeneric>();
    #else
        soc = std::make_shared<SOC>();
    #endif
        sim->addModule(soc);
    }
}

void Core::ConnectBFUTOL2()
{
    // connect BFU & L2
    int scalarLSUIdx = static_cast<int>(LSUType::SCALAR_LSU);
    bctrl->blockFetchUnit.bfuIntf.bfu_l2_q = &inst_l2_q[scalarLSUIdx];
    bctrl->blockFetchUnit.bfuIntf.bfu_hpre_l2_q = &hpref_l2_q[scalarLSUIdx];
    bctrl->blockFetchUnit.bfuIntf.l2_bfu_q = &l2_bfu_q[scalarLSUIdx];
    bctrl->blockFetchUnit.bfuIntf.snp_l2_q = &snp_l2_q[scalarLSUIdx];
}

void Core::BuildQueues()
{
    // Build queues
    bFetchReqQ.Build();
    iFetchReqQ.Build();
    bFetchRetQ.Build();
    vecBridgeReqQ.Build();
    vecBridgeRspQ.Build();

    for (uint32_t i = 0; i < memReqQ.size(); ++i) {
        memReqQ[i].Build();
        memRetQ[i].Build();
    }
    for (uint32_t i = 0; i < inst_l2_q.size(); ++i) {
        inst_l2_q[i].Build();
        l2_inst_q[i].Build();
        l2_bfu_q[i].Build();
        hpref_l2_q[i].Build();
        snp_l2_q[i].Build();
        for (uint64_t j = 0; j < l2_ifu_array[i].size(); ++j) {
            l2_ifu_array[i][j].Build();
        }
    }
}

void Core::ConnectFlushUnit()
{
    flushUnit->bcc_flush_rpt_q = coreInterface.bcc_flush_rpt_q;
    flushUnit->iex_flush_rpt_q = coreInterface.iex_flush_rpt_q;
    flushUnit->pe_flush_rpt_array = coreInterface.pe_flush_rpt_array;
    flushUnit->lsu_flush_rpt_q = coreInterface.lsu_flush_rpt_q;
}

void Core::BuildTileBridge()
{
    for (uint32_t i = 0; i < configs.tileBridgeNum; ++i) {
        auto tileBridge = std::make_shared<GFSIM::TileBridge::TileBridge>(sim, &tileUtils, i);
        sim->ObjRegisterLogInfo(tileBridge, i, globalSeqToSwimPtr++);
        sim->RegisterMultiThread(tileBridge, tileBridge->m_config.BPQ_SIZE, i);

        tileBridge->m_lsuBridgeTloadQ = &coreInterface.lsuBridgeTloadArray[i];
        tileBridge->m_lsuBridgeTstoreQ = &coreInterface.lsuBridgeTstoreArray[i];

        tileBridge->tmaBCCWakeupQ = &coreInterface.tmaBCCWakeupQ;
        tileBridge->tauBCCWakeupQ = &coreInterface.tauBCCWakeupQ;
        tileBridge->bccTAUBlockCmdQ = &coreInterface.bccTAUBlockCmdQ;
        // Vector <--> Bridge
        tileBridge->m_vectorBridgeReqQ = &vecBridgeReqQ;
        tileBridge->m_vectorBridgeRspQ = &vecBridgeRspQ;

        tileBridge->m_tileRegBridgeLdResQ = &coreInterface.tileRegBridgeLdResArray[i];
        tileBridge->m_tileRegBridgeWrResQ = &coreInterface.tileRegBridgeWrResArray[i];
        tileBridge->m_bridgeTileRegLdReqQ = &coreInterface.bridgeTileRegLdReqArray[i];
        tileBridge->m_bridgeTileRegStReqQ = &coreInterface.bridgeTileRegStReqArray[i];

        tileBridge->m_memBridgeRspQ = &memRetQ[static_cast<int>(LSUType::BRIDGE_TABLE)];
        tileBridge->m_bridgeMemReqQ = &memReqQ[static_cast<int>(LSUType::BRIDGE_TABLE)];

        auto tmuConfig = tileUtils.GetUBConfig();
        tileBridge->AddNode(stations[tmuConfig->tma_read_port[i]]);
        nodeToPEMap[tmuConfig->tma_read_port[i]] = "TMA" + to_string(i) + " Read";
        tileBridge->stationReadRange = tileBridge->stations.size();
        size_t j = 0;
        for (uint64_t writePort: tmuConfig->tma_write_port) {
            if (writePort < tileUtils.GetNodeNum()) {
                tileBridge->AddNode(stations[writePort]);
                nodeToPEMap[writePort] = "TMA" + to_string(i) + " Write" + to_string(j);
                j++;
            }
        }

        tileBridge->pTileUtils = &tileUtils;

        tileBridge->Build();
        if (configs.simtEnable) {
            sim->addModule(tileBridge);
        }
        tileBridges.emplace_back(tileBridge);
    }
}

void Core::buildCore(SimSys *sim) {
    // add itself
    this->sim = sim;

    configs.overrideDefaultConfig(sim->getCfgs());
    InitSwimLogger();
    sim->BuildViewManager(configs.block_rob_depth, configs.scalar_smt_thread);
    sim->BuildVerifyManager(configs.block_rob_depth, configs.scalar_smt_thread);
    for (uint64_t thread = 0; thread < configs.scalar_smt_thread; thread++) {
        sim->GetVerifyManager(thread)->config.deadLockThreshold = configs.deadlock_cycles - 1;
    }

    // Build uniform interface
    buildInterface();

    debug_log = std::make_shared<DebugLog>();
    debug_log->simPtr_ = sim;

    pTracer = make_shared<InstTracer>(sim, this, &std::cout);
    pTracer->SetFunctionalState(configs.dump_inst_trace);
    pTracer->Build();

    if (DEBUG_VERBOSE_ON) {
        if (!debug_log->Init("jcore_debug.log")) {
            std::cerr << "[FATAL] Failed to initialize debug log. Exiting." << std::endl;
            exit(1);
        }
    }
    tileUtils.Build(sim->getCfgs());
    BuildRenameUnit();

    BuildFlushUnit();

    BuildBCC();

    BuildStackRename();

    BuildSTDPE();
    SetPEAttr();

    BuildIEX();
    ConnectIEXIntf();

    BuildCubeCore();
    BuildVeclite();
    BuildVecCore();
    BuildMTCCore();

    BuildMEMIEX();
    ConnectMEMIEXIntf();

    BuildTileReg();

    BuildLSU();
    BuildSOC();

    ConnectBFUTOL2();

    InitRingAndXbar();

    BuildQueues();

    ConnectFlushUnit();
    ConnectBIQBus();

    BuildTileBridge();

    // Reset stats
    resetStats();

    illegalConfCheck();
}

void Core::BuildRenameUnit()
{
    vrfRename = std::make_shared<VRFRename>();
    vrfRename->sim = sim;
    uint32_t threadCount = configs.simtPeCount * configs.threadCount;
    if (configs.mtc_separate_enable) {
        threadCount += configs.mtcThreadCount;
    }
    vrfRename->Build(threadCount, configs.vrf_mapq_depth, configs.vrf_preg_count);
}

void Core::InitRingAndXbar()
{
    for (uint32_t i = 0; i < tileUtils.GetNodeNum(); ++i) {
        stations.emplace_back(std::make_shared<CrossStation>(i));
        stations[i]->sim = sim;
        stations[i]->pTileUtils = &tileUtils;
        stations[i]->Build();
        if (std::find(tileUtils.GetUBConfig()->cube_read_port.cbegin(),
                      tileUtils.GetUBConfig()->cube_read_port.cend(), i) !=
                      tileUtils.GetUBConfig()->cube_read_port.cend())  {
            stations[i]->InitCoreBuffer(configs.cube_core_num);
        }
        if (std::find(tileUtils.GetUBConfig()->cube_write_port.cbegin(),
                      tileUtils.GetUBConfig()->cube_write_port.cend(), i) !=
                      tileUtils.GetUBConfig()->cube_write_port.cend())  {
            stations[i]->InitCoreBuffer(configs.cube_core_num);
        }
        if (std::find(tileUtils.GetUBConfig()->tma_write_port.cbegin(),
                      tileUtils.GetUBConfig()->tma_write_port.cend(), i) !=
                      tileUtils.GetUBConfig()->tma_write_port.cend())  {
            stations[i]->InitCoreBuffer(tileUtils.GetUBConfig()->tma_write_port.size());
        }
    }

    // Ring
    ccRing = std::make_shared<Ring>(CrossStation::Direction::COUNTER_CLOCKWISE, stations);
    cwRing = std::make_shared<Ring>(CrossStation::Direction::CLOCKWISE, stations);
    ccRing->sim = sim;
    ccRing->pTileUtils = &tileUtils;
    cwRing->sim = sim;
    cwRing->pTileUtils = &tileUtils;
    ccRing->machineType = MachineType::CCRING;
    ccRing->machineId = GetProcessID(ccRing->machineType, 0, 1);
    sim->ObjRegisterLogInfo(ccRing, 0, globalSeqToSwimPtr++, tileUtils.GetThreadName(0));
    sim->RegisterMultiThread(ccRing, tileUtils.GetThreadNum(), 0, tileUtils.GetThreadNameMap());
    cwRing->machineType = MachineType::CWRING;
    cwRing->machineId = GetProcessID(cwRing->machineType, 0, 1);
    sim->ObjRegisterLogInfo(cwRing, 0, globalSeqToSwimPtr++, tileUtils.GetThreadName(0));
    sim->RegisterMultiThread(cwRing, tileUtils.GetThreadNum(), 0, tileUtils.GetThreadNameMap());
    ccRing->Build();
    cwRing->Build();

    // Xbar island
    island = make_shared<Island>();
    island->sim = sim;
    island->pTileUtils = &tileUtils;
    for (auto &station : stations) {
        island->nodes.emplace_back(station);
    }
    island->Build();

    if (tileUtils.IsRingMode(false)) {
        sim->addModule(ccRing);
        sim->addModule(cwRing);
    } else if (tileUtils.IsXbarMode()) {
        sim->addModule(island);
    }

    for (uint32_t i = 0; i < tileUtils.GetNodeNum(); ++i) {
        // tile register 默认连接对应的 CrossStation
        // port8 -> 0; port7 -> 1; port6 -> 2
        // Node1 -> 0; Node3 -> 1; Node5 -> 2;
        tileReg->AddNode(stations[i]);
        tileReg->AddNodeForStashCtrl(stations[i]);
        sim->addModule(stations[i]);
    }
    auto tmuConfig = tileUtils.GetUBConfig();
    if (tmuConfig->dual_pe_ring_mode) {
        for (unsigned cubeIdx = 0; cubeIdx < cubeCores.size(); cubeIdx++) {
            cubeCores[cubeIdx]->stations.push_back(stations[tmuConfig->cube_read_port[cubeIdx]]);
            cubeCores[cubeIdx]->stationReadRange = 1;
            cubeCores[cubeIdx]->stations.push_back(stations[tmuConfig->cube_write_port[cubeIdx]]);
            nodeToPEMap[tmuConfig->cube_read_port[cubeIdx]] = "Cube" + to_string(cubeIdx) + " Read";
            nodeToPEMap[tmuConfig->cube_write_port[cubeIdx]] = "Cube" + to_string(cubeIdx) + " Write";
        }
    } else {
        // cube read/write
        for (auto &cubeCore : cubeCores) {
            for (uint32_t i = 0; i < tmuConfig->cube_read_port.size(); ++i) {
                cubeCore->stations.push_back(stations[tmuConfig->cube_read_port[i]]);
                nodeToPEMap[tmuConfig->cube_read_port[i]] = "Cube" + to_string(i) + " Read";
            }
            cubeCore->stationReadRange = tmuConfig->cube_read_port.size();
            for (uint32_t i = 0; i < tmuConfig->cube_write_port.size(); ++i) {
                cubeCore->stations.push_back(stations[tmuConfig->cube_write_port[i]]);
                nodeToPEMap[tmuConfig->cube_write_port[i]] = "Cube" + to_string(i) + " Write";
            }
        }
    }
    if (configs.mtc_separate_enable) {
        // mtc read/write
        for (auto &mtc : mtcCores) {
            for (uint32_t i = 0; i < tmuConfig->mtc_read_port.size(); ++i) {
                // 2: MTC和bridge共一个station,各占用一个buffer
                stations[tmuConfig->mtc_read_port[i]]->InitCoreBuffer(2);
                mtc->AddNode(stations[tmuConfig->mtc_read_port[i]]);
                nodeToPEMap[tmuConfig->mtc_read_port[i]] = "MTC" + to_string(i) + " Read";
            }
            mtc->stationReadRange = tmuConfig->mtc_read_port.size();
            for (uint32_t i = 0; i < tmuConfig->mtc_write_port.size(); ++i) {
                // 2: MTC和bridge共一个station,各占用一个buffer
                stations[tmuConfig->mtc_write_port[i]]->InitCoreBuffer(2);
                mtc->AddNode(stations[tmuConfig->mtc_write_port[i]]);
                nodeToPEMap[tmuConfig->mtc_write_port[i]] = "MTC" + to_string(i) + " Write";
            }
        }
    }
    if (tmuConfig->dual_pe_ring_mode) {
        // vector read
        for (uint32_t i = 0; i < tmuConfig->vec_read_port.size(); ++i) {
            if (i <= 1) {
                vectorTop->InitDualPERing(0, stations[tmuConfig->vec_read_port[i]]);
                vecliteTop->InitDualPERing(0, stations[tmuConfig->vec_read_port[i]]);
                nodeToPEMap[tmuConfig->vec_read_port[i]] = "Vector0 Read" + to_string(i % 2);
            } else {
                vectorTop->InitDualPERing(1, stations[tmuConfig->vec_read_port[i]]);
                vecliteTop->InitDualPERing(1, stations[tmuConfig->vec_read_port[i]]);
                nodeToPEMap[tmuConfig->vec_read_port[i]] = "Vector1 Read" + to_string(i % 2);
            }
        }
        // vector write
        for (uint32_t i = 0; i < tmuConfig->vec_write_port.size(); ++i) {
            unsigned vecPeIdx = i % 2;
            vectorTop->InitDualPERing(vecPeIdx, stations[tmuConfig->vec_write_port[i]]);
            vecliteTop->InitDualPERing(vecPeIdx, stations[tmuConfig->vec_write_port[i]]);
            nodeToPEMap[tmuConfig->vec_write_port[i]] = "Vector" + to_string(i) + " Write";
        }
    } else {
        // vector read
        for (uint32_t i = 0; i < tmuConfig->vec_read_port.size(); ++i) {
            vectorTop->InitRing(stations[tmuConfig->vec_read_port[i]]);
            vecliteTop->InitRing(stations[tmuConfig->vec_read_port[i]]);
            nodeToPEMap[tmuConfig->vec_read_port[i]] = "Vector" + to_string(i) + " Read";
        }
        // vector write
        for (uint32_t i = 0; i < tmuConfig->vec_write_port.size(); ++i) {
            vectorTop->InitRing(stations[tmuConfig->vec_write_port[i]]);
            vecliteTop->InitRing(stations[tmuConfig->vec_write_port[i]]);
            nodeToPEMap[tmuConfig->vec_write_port[i]] = "Vector" + to_string(i) + " Write";
        }
    }
    for (uint32_t i = 0; i < tileUtils.GetNodeNum(); ++i) {
        stations[i]->ConnectRings(ccRing, cwRing);
    }
}

void Core::buildInterface() {
    uint32_t peCount = configs.stdPeCount + configs.simtPeCount + configs.memPeCount;
    uint32_t iexNum = static_cast<uint32_t>(ExecEngineTyp::IEX_NUM) + configs.simtPeCount;
    coreInterface.iex_pe_rslv_array.resize(iexNum);
    for (uint32_t iex = 0; iex < iexNum; ++iex) {
        coreInterface.iex_pe_rslv_array[iex].resize(peCount);
        for (uint64_t i = 0; i < peCount; i++) {
            uint32_t threadCount = i < configs.stdPeCount ? configs.scalar_smt_thread : configs.threadCount;
            for (uint32_t tid = 0; tid < threadCount; ++tid) {
                coreInterface.iex_pe_rslv_array[iex][i].push_back(new SimQueue<PEResolveBus>());
            }
        }
    }

    coreInterface.iex_brob_rslvblk.resize(iexNum);
    // Initial IEX-LSU interface
    coreInterface.iex_lsu_lda_array.resize(IEX_NUM);
    for (auto &arr : coreInterface.iex_lsu_lda_array) {
        arr.resize(configs.scalar_smt_thread);
    }
    coreInterface.lsu_iex_lret_array.resize(IEX_NUM);
    for (auto &arr : coreInterface.lsu_iex_lret_array) {
        arr.resize(configs.scalar_smt_thread);
    }
    coreInterface.iex_lsu_sta_array.resize(IEX_NUM);
    for (auto &arr : coreInterface.iex_lsu_sta_array) {
        arr.resize(configs.scalar_smt_thread);
    }
    coreInterface.iex_lsu_std_array.resize(IEX_NUM);
    for (auto &arr : coreInterface.iex_lsu_std_array) {
        arr.resize(configs.scalar_smt_thread);
    }
    coreInterface.iexScalperInstQ.clear();
    coreInterface.pe_scalper_commit_q.clear();
    coreInterface.peScbWrQ.clear();

    // Initial PE interface
    for (uint64_t iex = 0; iex < iexNum; ++iex) {
        coreInterface.pe_iex_cmd_array.push_back(std::vector<SimQueue<SimInst>*>());
        coreInterface.pe_iex_alu_array.push_back(std::vector<SimQueue<SimInst>*>());
        coreInterface.pe_iex_lda_array.push_back(std::vector<SimQueue<SimInst>*>());
        coreInterface.pe_iex_sc_lda_array.push_back(std::vector<SimQueue<SimInst>*>());
        coreInterface.pe_iex_sta_array.push_back(std::vector<SimQueue<SimInst>*>());
        coreInterface.pe_iex_std_array.push_back(std::vector<SimQueue<SimInst>*>());
        coreInterface.pe_iex_bru_array.push_back(std::vector<SimQueue<SimInst>*>());
        coreInterface.pe_iex_rd_array.push_back(std::vector<SimQueue<SimInst>*>());
        coreInterface.pe_iex_vec_agu_array.push_back(std::vector<SimQueue<SimInst>*>());
        coreInterface.pe_iex_vec_scalar_array.push_back(std::vector<SimQueue<SimInst>*>());
        for (uint64_t i = 0; i < peCount; i++) {
            coreInterface.pe_iex_cmd_array[iex].push_back(new SimQueue<SimInst>());
            coreInterface.pe_iex_alu_array[iex].push_back(new SimQueue<SimInst>());
            coreInterface.pe_iex_lda_array[iex].push_back(new SimQueue<SimInst>());
            coreInterface.pe_iex_sc_lda_array[iex].push_back(new SimQueue<SimInst>());
            coreInterface.pe_iex_sta_array[iex].push_back(new SimQueue<SimInst>());
            coreInterface.pe_iex_std_array[iex].push_back(new SimQueue<SimInst>());
            coreInterface.pe_iex_bru_array[iex].push_back(new SimQueue<SimInst>());
            coreInterface.pe_iex_rd_array[iex].push_back(new SimQueue<SimInst>());
            coreInterface.pe_iex_vec_agu_array[iex].push_back(new SimQueue<SimInst>());
            coreInterface.pe_iex_vec_scalar_array[iex].push_back(new SimQueue<SimInst>());
        }
    }

    for (size_t i = 0; i < peCount; ++i) {
        coreInterface.pe_rf_wr_q.push_back(new SimQueue<RFReqBus>());
    }

    // Initial For PE-LSU
    for (uint64_t i = 0; i < peCount; i++) {
        coreInterface.lsu_pe_rslv_array.push_back(new SimQueue<MemReqBus>());
    }

    // Initial For Flush Control
    coreInterface.bcc_flush_rpt_q = new SimQueue<FlushReq>();

    coreInterface.iex_flush_rpt_q.resize(iexNum);
    for (uint64_t i = 0; i < iexNum; ++i) {
        coreInterface.iex_flush_rpt_q[i] = new SimQueue<FlushReq>();
    }
    for (uint64_t i = 0; i < peCount; i++) {
        coreInterface.pe_flush_rpt_array.push_back(new SimQueue<FlushReq>());
    }

    // Build reg map queue
    coreInterface.rtable_release_q = new SimQueue<uint32_t>();
    coreInterface.block_rtable_retire_q = new SimQueue<ROBID>();

    coreInterface.bCCCubeFlushArray.resize(configs.cube_core_num);
    coreInterface.bCCTTransFlushArray.resize(configs.ttrans_core_num);
    coreInterface.bCCVecFlushArray.resize(configs.simtPeCount);

    coreInterface.cubeBCCCreditArray.resize(configs.cube_core_num);
    coreInterface.cubeTileRegLdReqArray.resize(configs.cube_core_num);
    coreInterface.cubeTileRegStReqArray.resize(configs.cube_core_num);

    coreInterface.tileRegCubeLdResArray.resize(configs.cube_core_num);
    coreInterface.tileRegCubeWrResArray.resize(configs.cube_core_num);
    coreInterface.tileRegCubeLdRetryArray.resize(configs.cube_core_num);
    coreInterface.tileRegCubeStRetryArray.resize(configs.cube_core_num);
    coreInterface.bccCubeBlockCmdQ.resize(configs.cube_core_num);
    coreInterface.tileRegTTransLdResArray.resize(configs.ttrans_core_num);
    coreInterface.tileRegTTransLdRetryArray.resize(configs.ttrans_core_num);
    coreInterface.tileRegTTransWrResArray.resize(configs.ttrans_core_num);
    coreInterface.tileRegTTransStRetryArray.resize(configs.ttrans_core_num);

    coreInterface.tileRegBridgeLdResArray.resize(configs.tileBridgeNum);
    coreInterface.tileRegBridgeWrResArray.resize(configs.tileBridgeNum);
    coreInterface.tileRegBridgeLdRetryArray.resize(configs.tileBridgeNum);
    coreInterface.tileRegBridgeStRetryArray.resize(configs.tileBridgeNum);
    coreInterface.bridgeTileRegLdReqArray.resize(configs.tileBridgeNum);
    coreInterface.bridgeTileRegStReqArray.resize(configs.tileBridgeNum);

    coreInterface.tileRegVecLdResArray.resize(configs.simtPeCount);
    coreInterface.tileRegVecWrResArray.resize(configs.simtPeCount);
    coreInterface.tileRegVecLdRetryArray.resize(configs.simtPeCount);
    coreInterface.tileRegVecStRetryArray.resize(configs.simtPeCount);

    coreInterface.tTransMemLdReqArray.resize(configs.ttrans_core_num);
    coreInterface.tTransMemLdResArray.resize(configs.ttrans_core_num);
    coreInterface.tTransMemStReqArray.resize(configs.ttrans_core_num);
    coreInterface.tTransMemStResArray.resize(configs.ttrans_core_num);
    coreInterface.tTransTileRegLdReqArray.resize(configs.ttrans_core_num);
    coreInterface.tTransTileRegStReqArray.resize(configs.ttrans_core_num);

    coreInterface.vecBCCCreditArray.resize(configs.simtPeCount);
    coreInterface.vecTileRegLdReqArray.resize(configs.simtPeCount);
    coreInterface.vecTileRegStReqArray.resize(configs.simtPeCount);

    coreInterface.vecCoreMemLdReqArray.resize(configs.threadCount);
    coreInterface.vecCoreMemLdResArray.resize(configs.threadCount);
    coreInterface.vecCoreMemStReqArray.resize(configs.threadCount);
    coreInterface.vecCoreMemStResArray.resize(configs.threadCount);

    uint32_t scCnt = sim->core->configs.scalar_lsu_num;
    uint32_t vcCnt = sim->core->configs.vector_lsu_num;
    uint32_t totalLsuCnt = scCnt + vcCnt;
    if (configs.mtc_separate_enable) {
        coreInterface.bCCMtcFlushArray.resize(configs.mtc_core_num);

        coreInterface.tileRegMtcLdResArray.resize(configs.mtc_core_num);
        coreInterface.tileRegMtcWrResArray.resize(configs.mtc_core_num);
        coreInterface.tileRegMtcLdRetryArray.resize(configs.mtc_core_num);
        coreInterface.tileRegMtcStRetryArray.resize(configs.mtc_core_num);

        coreInterface.mtcTileRegLdReqArray.resize(configs.mtc_core_num);
        coreInterface.mtcTileRegStReqArray.resize(configs.mtc_core_num);

        coreInterface.mtcCoreMemLdReqArray.resize(configs.threadCount);
        coreInterface.mtcCoreMemLdResArray.resize(configs.threadCount);
        coreInterface.mtcCoreMemStReqArray.resize(configs.threadCount);
        coreInterface.mtcCoreMemStResArray.resize(configs.threadCount);

        uint32_t mtcCnt  = sim->core->configs.mtccore_lsu_num;
        totalLsuCnt += mtcCnt;
    }

    coreInterface.pref_lu_array.resize(totalLsuCnt);
    coreInterface.lu_pref_array.resize(totalLsuCnt);
    coreInterface.feedback_lu_pref_array.resize(totalLsuCnt);
    coreInterface.pref_l2_array.resize(totalLsuCnt);
    coreInterface.l2_pref_array.resize(totalLsuCnt);
    coreInterface.snoop_lu_l2_array.resize(totalLsuCnt);
    coreInterface.lookup_lu_l2_array.resize(totalLsuCnt);
    coreInterface.lookup_l2_l1_array.resize(totalLsuCnt);

    constexpr uint32_t bridgeTableCnt = 1;
    memReqQ.resize(totalLsuCnt + bridgeTableCnt);
    memRetQ.resize(totalLsuCnt + bridgeTableCnt);
    for (uint32_t i = 0; i < totalLsuCnt + bridgeTableCnt; ++i) {
        mem_delay.emplace_back(DelayQueue<GFUMemReq>(configs.soc_default_latency)); // 30 cycle
    }

    vecBridgeReqQ.Setlatency(configs.vec_bridge_latency);
    vecBridgeRspQ.Setlatency(configs.vec_bridge_latency);
    inst_l2_q.resize(totalLsuCnt);
    hpref_l2_q.resize(totalLsuCnt);
    l2_inst_q.resize(totalLsuCnt);
    snp_l2_q.resize(totalLsuCnt);
    l2_bfu_q.resize(totalLsuCnt);
    coreInterface.lsu_flush_rpt_q.resize(totalLsuCnt);

    coreInterface.bccLsuTloadCommitArray.resize(static_cast<size_t>(LSUType::COUNT));
    coreInterface.bccLsuTstoreCommitArray.resize(static_cast<size_t>(LSUType::COUNT));

    coreInterface.lsuBridgeTloadArray.resize(configs.tileBridgeNum);
    coreInterface.lsuBridgeTstoreArray.resize(configs.tileBridgeNum);

    coreInterface.vecBridgeMemReqQ = std::make_shared<SimQueue<MemRequest>>();
    coreInterface.bridgeVecLdRetQ = std::make_shared<SimQueue<MemRequest>>();
    coreInterface.bridgeVecStRslvQ = std::make_shared<SimQueue<MemRequest>>();

    coreInterface.load_non_flush_q.resize(configs.simtPeCount);
    coreInterface.store_non_flush_q.resize(configs.simtPeCount);
    coreInterface.data_vec_viex_q.resize(configs.simtPeCount);
    coreInterface.data_viex_vec_q.resize(configs.simtPeCount);
    coreInterface.gsDataVecViexQ.resize(configs.simtPeCount);
    coreInterface.gsDataViexVecQ.resize(configs.simtPeCount);

    coreInterface.iexVcoreReqQ.resize(configs.simtPeCount);
    coreInterface.vcoreIexResQ.resize(configs.simtPeCount);
    coreInterface.vcoreLdIexResQ.resize(configs.simtPeCount);
    coreInterface.vcoreStIexResQ.resize(configs.simtPeCount);

    coreInterface.rreq_vec_srf_q.resize(configs.simtPeCount);
    coreInterface.rrsp_srf_vec_q.resize(configs.simtPeCount);
    coreInterface.wreq_vec_srf_q.resize(configs.simtPeCount);
    coreInterface.wrsp_srf_vec_q.resize(configs.simtPeCount);
    coreInterface.gsRreqVecSrfQ.resize(configs.simtPeCount);
    coreInterface.gsRrspSrfVecQ.resize(configs.simtPeCount);
    coreInterface.gsWreqVecSrfQ.resize(configs.simtPeCount);
    coreInterface.gsWrspSrfVecQ.resize(configs.simtPeCount);
    coreInterface.stashRslvArray.resize(configs.simtPeCount);
    coreInterface.stashAllocDoneArray.resize(configs.simtPeCount);
    coreInterface.toClearArray.resize(configs.simtPeCount);

    coreInterface.bCCVecliteFlushArray.resize(configs.simtPeCount);
    coreInterface.stashVecliteAllocDoneArray.resize(configs.simtPeCount);
    coreInterface.stashVecliteRslvArray.resize(configs.simtPeCount);
}

void Core::ConnectTileBus()
{
    // Input: form other module to tile register
    SimArrayConnect(tileReg->cubeTileRegLdReqArray, coreInterface.cubeTileRegLdReqArray);
    SimArrayConnect(tileReg->cubeTileRegStReqArray, coreInterface.cubeTileRegStReqArray);
    SimArrayConnect(tileReg->tTransTileRegLdReqArray, coreInterface.tTransTileRegLdReqArray);
    SimArrayConnect(tileReg->tTransTileRegStReqArray, coreInterface.tTransTileRegStReqArray);
    SimArrayConnect(tileReg->vecTileRegLdReqArray, coreInterface.vecTileRegLdReqArray);
    SimArrayConnect(tileReg->vecTileRegStReqArray, coreInterface.vecTileRegStReqArray);

    SimArrayConnect(tileReg->bridgeTileRegLdReqArray, coreInterface.bridgeTileRegLdReqArray);
    SimArrayConnect(tileReg->bridgeTileRegStReqArray, coreInterface.bridgeTileRegStReqArray);

    // Output: form tile register to other module
    SimArrayConnect(tileReg->tileRegCubeLdResArray, coreInterface.tileRegCubeLdResArray);
    SimArrayConnect(tileReg->tileRegCubeWrResArray, coreInterface.tileRegCubeWrResArray);
    SimArrayConnect(tileReg->tileRegCubeLdRetryArray, coreInterface.tileRegCubeLdRetryArray);
    SimArrayConnect(tileReg->tileRegCubeStRetryArray, coreInterface.tileRegCubeStRetryArray);
    SimArrayConnect(tileReg->tileRegTTransLdResArray, coreInterface.tileRegTTransLdResArray);
    SimArrayConnect(tileReg->tileRegTTransLdRetryArray, coreInterface.tileRegTTransLdRetryArray);
    SimArrayConnect(tileReg->tileRegTTransStRetryArray, coreInterface.tileRegTTransStRetryArray);
    SimArrayConnect(tileReg->tileRegVecLdResArray, coreInterface.tileRegVecLdResArray);
    SimArrayConnect(tileReg->tileRegVecWrResArray, coreInterface.tileRegVecWrResArray);
    SimArrayConnect(tileReg->tileRegVecLdRetryArray, coreInterface.tileRegVecLdRetryArray);
    SimArrayConnect(tileReg->tileRegVecStRetryArray, coreInterface.tileRegVecStRetryArray);

    SimArrayConnect(tileReg->tileRegBridgeLdResArray, coreInterface.tileRegBridgeLdResArray);
    SimArrayConnect(tileReg->tileRegBridgeWrResArray, coreInterface.tileRegBridgeWrResArray);
    SimArrayConnect(tileReg->tileRegBridgeLdRetryArray, coreInterface.tileRegBridgeLdRetryArray);
    SimArrayConnect(tileReg->tileRegBridgeStRetryArray, coreInterface.tileRegBridgeStRetryArray);
    tileReg->stashCtrlUnit->stashCmdQ = &coreInterface.stashCmdQ;
    tileReg->stashCtrlUnit->stashCompQ = &coreInterface.stashCompQ;
    tileReg->stashCtrlUnit->stashRetireQ = &coreInterface.stashRetireQ;
    tileReg->stashCtrlUnit->stashFreeQ = &coreInterface.stashFreeQ;

    tileReg->stashCtrlUnit->cubeStashCmdQ = &coreInterface.cubeStashCmdQ;
    tileReg->stashCtrlUnit->cubeStashCompQ = &coreInterface.cubeStashCompQ;
    tileReg->stashCtrlUnit->stashCubeAllocDoneQ = &coreInterface.stashCubeAllocDoneQ;
    SimArrayConnect(tileReg->stashCtrlUnit->stashAllocDoneArray, coreInterface.stashAllocDoneArray);
    SimArrayConnect(tileReg->stashCtrlUnit->stashRslvArray, coreInterface.stashRslvArray);
    SimArrayConnect(tileReg->stashCtrlUnit->toClearArray, coreInterface.toClearArray);

    tileReg->stashCtrlUnit->vecliteStashCmdQ = &coreInterface.vecliteStashCmdQ;
    tileReg->stashCtrlUnit->vecliteStashCompQ = &coreInterface.vecliteStashCompQ;
    SimArrayConnect(tileReg->stashCtrlUnit->stashVecliteAllocDoneArray, coreInterface.stashVecliteAllocDoneArray);
    SimArrayConnect(tileReg->stashCtrlUnit->stashVecliteRslvArray, coreInterface.stashVecliteRslvArray);

    if (configs.mtc_separate_enable) {
        SimArrayConnect(tileReg->mtcTileRegLdReqArray, coreInterface.mtcTileRegLdReqArray);
        SimArrayConnect(tileReg->mtcTileRegStReqArray, coreInterface.mtcTileRegStReqArray);

        SimArrayConnect(tileReg->tileRegMtcLdResArray, coreInterface.tileRegMtcLdResArray);
        SimArrayConnect(tileReg->tileRegMtcWrResArray, coreInterface.tileRegMtcWrResArray);
        SimArrayConnect(tileReg->tileRegMtcLdRetryArray, coreInterface.tileRegMtcLdRetryArray);
        SimArrayConnect(tileReg->tileRegMtcStRetryArray, coreInterface.tileRegMtcStRetryArray);
    }
}

template <class T>
void Core::SetSimArrayLatency(vector<SimQueue<T>*> const &simArray, const uint32_t latency) const
{
    for (auto &simQ : simArray) {
        simQ->Setlatency(latency);
    }
}

void Core::SetTRDirectAaccelssLatency()
{
    uint32_t latency = tileUtils.GetUBConfig()->direct_aaccelss_latency;
    SetSimArrayLatency(tileReg->cubeTileRegLdReqArray, latency);
    SetSimArrayLatency(tileReg->cubeTileRegStReqArray, latency);
    SetSimArrayLatency(tileReg->tTransTileRegLdReqArray, latency);
    SetSimArrayLatency(tileReg->tTransTileRegStReqArray, latency);
    SetSimArrayLatency(tileReg->vecTileRegLdReqArray, latency);
    SetSimArrayLatency(tileReg->vecTileRegStReqArray, latency);
    SetSimArrayLatency(tileReg->bridgeTileRegLdReqArray, latency);
    SetSimArrayLatency(tileReg->bridgeTileRegStReqArray, latency);
    SetSimArrayLatency(tileReg->tileRegCubeLdResArray, latency);
    SetSimArrayLatency(tileReg->tileRegCubeWrResArray, latency);
    SetSimArrayLatency(tileReg->tileRegCubeLdRetryArray, latency);
    SetSimArrayLatency(tileReg->tileRegCubeStRetryArray, latency);
    SetSimArrayLatency(tileReg->tileRegTTransLdResArray, latency);
    SetSimArrayLatency(tileReg->tileRegTTransLdRetryArray, latency);
    SetSimArrayLatency(tileReg->tileRegTTransStRetryArray, latency);
    SetSimArrayLatency(tileReg->tileRegVecLdResArray, latency);
    SetSimArrayLatency(tileReg->tileRegVecWrResArray, latency);
    SetSimArrayLatency(tileReg->tileRegVecLdRetryArray, latency);
    SetSimArrayLatency(tileReg->tileRegVecStRetryArray, latency);
    SetSimArrayLatency(tileReg->tileRegBridgeLdResArray, latency);
    SetSimArrayLatency(tileReg->tileRegBridgeWrResArray, latency);
    SetSimArrayLatency(tileReg->tileRegBridgeLdRetryArray, latency);
    SetSimArrayLatency(tileReg->tileRegBridgeStRetryArray, latency);

    if (configs.mtc_separate_enable) {
        SetSimArrayLatency(tileReg->mtcTileRegLdReqArray, latency);
        SetSimArrayLatency(tileReg->mtcTileRegStReqArray, latency);

        SetSimArrayLatency(tileReg->tileRegMtcLdResArray, latency);
        SetSimArrayLatency(tileReg->tileRegMtcWrResArray, latency);
        SetSimArrayLatency(tileReg->tileRegMtcLdRetryArray, latency);
        SetSimArrayLatency(tileReg->tileRegMtcStRetryArray, latency);
    }
}

void Core::ConnetIexBus()
{
    iex[SCALAR_IEX]->cmdIQBISQ = &coreInterface.cmdIQBISQ;
}

void Core::ConnectBIQBus()
{
    // cube
    coreInterface.bCCCubeFlushArray.resize(configs.cube_core_num);
    coreInterface.cubeBCCCreditArray.resize(configs.cube_core_num);
    bctrl->blockIssueQueueUnit.m_bccCubeFlushArr.resize(configs.cube_core_num);
    for (size_t i = 0; i < bctrl->blockIssueQueueUnit.blockDispatchQ[BIQType::CUBE_IQ].size(); i++) {
        bctrl->blockIssueQueueUnit.blockDispatchQ[BIQType::CUBE_IQ][i] = &coreInterface.bccCubeBlockCmdQ[i];
    }
    bctrl->blockIssueQueueUnit.blockDispatchQ[BIQType::VEC_IQ][0] = &coreInterface.bccVecBlockCmdQ;
    bctrl->blockIssueQueueUnit.blockDispatchQ[BIQType::TMA_IQ][0] = &coreInterface.bccTMABlockCmdQ;
    bctrl->blockIssueQueueUnit.blockDispatchQ[BIQType::VET_IQ][0] = &coreInterface.bccVecliteBlockCmdQ;
    bctrl->blockIssueQueueUnit.blockDispatchQ[BIQType::TAU_IQ][0] = &coreInterface.bccTAUBlockCmdQ;
    bctrl->blockIssueQueueUnit.blockDispatchQ[BIQType::MCALL_IQ][0] = &coreInterface.bccMcallBlockCmdQ;
    bctrl->blockIssueQueueUnit.blockWakeupQ[BIQType::CUBE_IQ] = &coreInterface.cubeBCCWakeupQ;
    bctrl->blockIssueQueueUnit.blockWakeupQ[BIQType::VEC_IQ] = &coreInterface.vecBCCWakeupQ;
    bctrl->blockIssueQueueUnit.blockWakeupQ[BIQType::VET_IQ] = &coreInterface.vecliteBccWakeupQ;
    bctrl->blockIssueQueueUnit.blockWakeupQ[BIQType::TMA_IQ] = &coreInterface.tmaBCCWakeupQ;
    bctrl->blockIssueQueueUnit.blockWakeupQ[BIQType::TAU_IQ] = &coreInterface.tauBCCWakeupQ;
    bctrl->blockIssueQueueUnit.blockWakeupQ[BIQType::MCALL_IQ] = &coreInterface.mcallBCCWakeupQ;
    bctrl->blockIssueQueueUnit.schedulerBCCRslvQ = &coreInterface.schedulerBCCRslvQ;
    bctrl->blockIssueQueueUnit.stashFreeQ = &coreInterface.stashFreeQ;
    bctrl->blockIssueQueueUnit.stashRetireQ = &coreInterface.stashRetireQ;
    for (uint32_t i = 0; i < configs.cube_core_num; i++) {
        bctrl->blockIssueQueueUnit.m_bccCubeFlushArr[i] = &coreInterface.bCCCubeFlushArray[i];
    }

    // tile transfer core
    coreInterface.bCCTTransFlushArray.resize(configs.ttrans_core_num);
    bctrl->blockIssueQueueUnit.m_bccTransFlushArr.resize(configs.ttrans_core_num);
    for (uint32_t i = 0; i < configs.ttrans_core_num; i++) {
        bctrl->blockIssueQueueUnit.m_bccTransFlushArr[i] = &coreInterface.bCCTTransFlushArray[i];
    }

    // vector core
    coreInterface.bCCVecFlushArray.resize(configs.simtPeCount);
    coreInterface.vecBCCCreditArray.resize(configs.simtPeCount);
    bctrl->blockIssueQueueUnit.m_bccVecFlushArr.resize(configs.simtPeCount);
    for (uint32_t i = 0; i < configs.simtPeCount; i++) {
        bctrl->blockIssueQueueUnit.m_bccVecFlushArr[i] = &coreInterface.bCCVecFlushArray[i];
    }

    coreInterface.bCCVecliteFlushArray.resize(configs.simtPeCount);
    bctrl->blockIssueQueueUnit.m_bCCVecliteFlushArray.resize(configs.simtPeCount);
    for (uint32_t i = 0; i < configs.simtPeCount; i++) {
        bctrl->blockIssueQueueUnit.m_bCCVecliteFlushArray[i] = &coreInterface.bCCVecliteFlushArray[i];
    }

    // lsu
    bctrl->blockROB.m_bccLsuTloadCommitQ = &coreInterface.bccLsuTloadCommitArray[SCALAR_IEX];
    bctrl->blockROB.m_bccLsuTstoreCommitQ = &coreInterface.bccLsuTstoreCommitArray[SCALAR_IEX];
    bctrl->m_biqBCCCreditQ = &coreInterface.m_biqBCCCreditQ;
    bctrl->blockIssueQueueUnit.cmdIQBISQ = &coreInterface.cmdIQBISQ;

    if (configs.mtc_separate_enable) {
        bctrl->blockIssueQueueUnit.blockDispatchQ[BIQType::MTC_IQ][0] = &coreInterface.bccMTCBlockCmdQ;
        bctrl->blockIssueQueueUnit.blockWakeupQ[BIQType::MTC_IQ] = &coreInterface.mtcBCCWakeupQ;

        // mtc core
        coreInterface.bCCMtcFlushArray.resize(configs.mtc_core_num);
        bctrl->blockIssueQueueUnit.m_bccMtcFlushArr.resize(configs.mtc_core_num);
        for (uint32_t i = 0; i < configs.mtc_core_num; i++) {
            bctrl->blockIssueQueueUnit.m_bccMtcFlushArr[i] = &coreInterface.bCCMtcFlushArray[i];
        }
    }
}

void Core::InitL2Cache()
{
    m_l2Cache = std::make_shared<L2Cache>();
    m_l2Cache->sim = this->sim;
    m_l2Cache->lsuL2ReqQ = &coreInterface.lsuL2ReqQ;
    m_l2Cache->l2LsuRspQ = &coreInterface.l2LsuRspQ;
    m_l2Cache->l2SocReqQ = &coreInterface.l2SocReqQ;
    m_l2Cache->socL2RspQ = &coreInterface.socL2RspQ;
    m_l2Cache->l1_l2_q = &coreInterface.l1_l2_q;
    m_l2Cache->l2_l1_q = &coreInterface.l2_l1_q;
    m_l2Cache->l2_mem_q = &coreInterface.l2_mem_q;
    m_l2Cache->mem_l2_q = &coreInterface.mem_l2_q;
    m_l2Cache->inst_l2_q = &coreInterface.inst_l2_q;
    m_l2Cache->pref_l2_q = &coreInterface.pref_l2_q;
    m_l2Cache->l2_inst_q = &coreInterface.l2_inst_q;
    m_l2Cache->l2_pref_q = &coreInterface.l2_pref_q;
    m_l2Cache->snp_l2_q = &coreInterface.snp_l2_q;
    m_l2Cache->hpref_l2_q = &coreInterface.hpref_l2_q;
    m_l2Cache->Build();
    m_l2Cache->Reset();
    m_l2Cache->stats->Reset();
    this->sim->addModule(m_l2Cache);
}

void Core::Work() {
    // drive queues
    bFetchReqQ.Work();
    iFetchReqQ.Work();
    // memReqQ.Work();
    bFetchRetQ.Work();
    iFetchRetQ.Work();
    vecBridgeReqQ.Work();
    vecBridgeRspQ.Work();
    for (auto &q : memRetQ) {
        q.Work();
    }
    for (uint32_t i = 0; i < memReqQ.size(); ++i) {
        memReqQ[i].Work();
        memRetQ[i].Work();
    }
    for (uint32_t i = 0; i < inst_l2_q.size(); ++i) {
        inst_l2_q[i].Work();
        l2_inst_q[i].Work();
        l2_bfu_q[i].Work();
        hpref_l2_q[i].Work();
        snp_l2_q[i].Work();
        for (uint64_t j = 0; j < l2_ifu_array[i].size(); ++j) {
            l2_ifu_array[i][j].Work();
        }
    }

    // drive routing
    route();

    watch_dog();

    handleSysReq();

    CountStats();
}

void Core::Xfer() {
    for (uint32_t i = 0; i < coreInterface.iex_lsu_lda_array.size(); ++i) {
        for (uint32_t stid = 0; stid < configs.scalar_smt_thread; ++stid) {
            for(auto simQ: coreInterface.iex_lsu_lda_array[i][stid])
                simQ->Work();
            for(auto simQ: coreInterface.lsu_iex_lret_array[i][stid])
                simQ->Work();
            for(auto simQ: coreInterface.iex_lsu_sta_array[i][stid])
                simQ->Work();
            for(auto simQ: coreInterface.iex_lsu_std_array[i][stid])
                simQ->Work();
        }
    }

    for (uint32_t i = 0; i < coreInterface.pe_iex_rd_array.size(); i++) {
        for (auto simQ : coreInterface.pe_iex_rd_array[i]) {
            if (simQ != nullptr) {
                simQ->Work();
            }
        }
    }

    for (auto simQ : coreInterface.pe_rf_wr_q) {
        if (simQ != nullptr) {
            simQ->Work();
        }
    }

    for (uint32_t i = 0; i < coreInterface.lsu_pe_rslv_array.size(); i++) {
        coreInterface.lsu_pe_rslv_array[i]->Work();
    }

    coreInterface.bcc_flush_rpt_q->Work();
    for (auto &rpt_q : coreInterface.iex_flush_rpt_q) {
        rpt_q->Work();
    }
    for (auto simQ : coreInterface.pe_flush_rpt_array)
        simQ->Work();
    for (auto &rptQ : coreInterface.lsu_flush_rpt_q) {
        rptQ->Work();
    }

    if (pTracer) {
        pTracer->Xfer();
    }

    XCoreSimQXfer();

    // 更新 tilereg 读score board
    readScoreBoard_.Update();
    // 更新 tilereg 写score board
    writeScoreBoard_.Update();
    if (!readScoreBoard_.IsOccupied()) {
        UpdateNextPriority(readNextPriority_);
    }

    if (!writeScoreBoard_.IsOccupied()) {
        UpdateNextPriority(writeNextPriority_);
    }
}

void Core::XCoreSimQXfer()
{
    SimArrayWork(coreInterface.bCCCubeFlushArray);
    coreInterface.bCCMemLdReqQ.Work();
    coreInterface.bCCMemLdResQ.Work();
    coreInterface.bCCMemSnpResQ.Work();
    coreInterface.bCCMemStReqQ.Work();
    SimArrayWork(coreInterface.bCCTTransFlushArray);
    SimArrayWork(coreInterface.bCCVecFlushArray);

    SimArrayWork(coreInterface.cubeBCCCreditArray);
    SimArrayWork(coreInterface.cubeTileRegLdReqArray);
    SimArrayWork(coreInterface.cubeTileRegStReqArray);
    SimArrayWork(coreInterface.tileRegCubeLdResArray);
    SimArrayWork(coreInterface.tileRegCubeWrResArray);
    SimArrayWork(coreInterface.tileRegCubeLdRetryArray);
    SimArrayWork(coreInterface.tileRegCubeStRetryArray);
    SimArrayWork(coreInterface.tileRegTTransLdResArray);
    SimArrayWork(coreInterface.tileRegTTransLdRetryArray);
    SimArrayWork(coreInterface.tileRegTTransWrResArray);
    SimArrayWork(coreInterface.tileRegTTransStRetryArray);
    SimArrayWork(coreInterface.tileRegVecLdResArray);
    SimArrayWork(coreInterface.tileRegVecWrResArray);
    SimArrayWork(coreInterface.tileRegVecLdRetryArray);
    SimArrayWork(coreInterface.tileRegVecStRetryArray);

    SimArrayWork(coreInterface.tileRegBridgeLdResArray);
    SimArrayWork(coreInterface.tileRegBridgeWrResArray);
    SimArrayWork(coreInterface.tileRegBridgeLdRetryArray);
    SimArrayWork(coreInterface.tileRegBridgeStRetryArray);
    SimArrayWork(coreInterface.bridgeTileRegLdReqArray);
    SimArrayWork(coreInterface.bridgeTileRegStReqArray);
    SimArrayWork(coreInterface.tTransMemLdReqArray);
    SimArrayWork(coreInterface.tTransMemLdResArray);
    SimArrayWork(coreInterface.tTransMemStReqArray);
    SimArrayWork(coreInterface.tTransMemStResArray);
    SimArrayWork(coreInterface.tTransTileRegLdReqArray);
    SimArrayWork(coreInterface.tTransTileRegStReqArray);
    SimArrayWork(coreInterface.vecBCCCreditArray);
    SimArrayWork(coreInterface.vecTileRegLdReqArray);
    SimArrayWork(coreInterface.vecTileRegStReqArray);

    SimArrayWork(coreInterface.pref_lu_array);
    SimArrayWork(coreInterface.lu_pref_array);
    SimArrayWork(coreInterface.feedback_lu_pref_array);
    SimArrayWork(coreInterface.pref_l2_array);
    SimArrayWork(coreInterface.l2_pref_array);
    SimArrayWork(coreInterface.snoop_lu_l2_array);
    SimArrayWork(coreInterface.lookup_lu_l2_array);
    SimArrayWork(coreInterface.lookup_l2_l1_array);
    SimArrayWork(coreInterface.iexVcoreReqQ);
    SimArrayWork(coreInterface.vcoreIexResQ);
    coreInterface.iexMcoreReqQ.Work();
    coreInterface.mcoreIexResQ.Work();

    coreInterface.cmdIQBISQ.Work();
    coreInterface.lsuMemLdReqQ.Work();
    coreInterface.lsuMemStReqQ.Work();
    coreInterface.lsuMemLdResQ.Work();
    coreInterface.lsuMemStResQ.Work();
    coreInterface.lsuL2ReqQ.Work();
    coreInterface.l2LsuRspQ.Work();
    coreInterface.socL2RspQ.Work();
    coreInterface.l2SocReqQ.Work();
    SimArrayWork(coreInterface.load_non_flush_q);
    SimArrayWork(coreInterface.store_non_flush_q);


    SimArrayWork(coreInterface.vecCoreMemLdReqArray);
    SimArrayWork(coreInterface.vecCoreMemLdResArray);
    SimArrayWork(coreInterface.vecCoreMemStReqArray);
    SimArrayWork(coreInterface.vecCoreMemStResArray);


    SimArrayWork(coreInterface.bccLsuTloadCommitArray);
    SimArrayWork(coreInterface.bccLsuTstoreCommitArray);
    SimArrayWork(coreInterface.lsuBridgeTloadArray);
    SimArrayWork(coreInterface.lsuBridgeTstoreArray);

    coreInterface.vecBridgeMemReqQ->Work();
    coreInterface.bridgeVecLdRetQ->Work();
    coreInterface.bridgeVecStRslvQ->Work();

    coreInterface.req_vec_siex_q.Work();
    coreInterface.rsp_siex_vec_q.Work();
    coreInterface.gsReqVecSiexQ.Work();
    coreInterface.gsRspSiexVecQ.Work();
    SimArrayWork(coreInterface.rreq_vec_srf_q);
    SimArrayWork(coreInterface.rrsp_srf_vec_q);
    SimArrayWork(coreInterface.wreq_vec_srf_q);
    SimArrayWork(coreInterface.wrsp_srf_vec_q);
    SimArrayWork(coreInterface.data_vec_viex_q);
    SimArrayWork(coreInterface.data_viex_vec_q);
    SimArrayWork(coreInterface.gsRreqVecSrfQ);
    SimArrayWork(coreInterface.gsRrspSrfVecQ);
    SimArrayWork(coreInterface.gsWreqVecSrfQ);
    SimArrayWork(coreInterface.gsWrspSrfVecQ);
    SimArrayWork(coreInterface.gsDataVecViexQ);
    SimArrayWork(coreInterface.gsDataViexVecQ);

    for (size_t i = 0; i < coreInterface.bccCubeBlockCmdQ.size(); i++) {
        coreInterface.bccCubeBlockCmdQ[i].Work();
    }
    coreInterface.bccVecBlockCmdQ.Work();
    coreInterface.bccMcallBlockCmdQ.Work();
    coreInterface.bccTMABlockCmdQ.Work();
    coreInterface.bccTAUBlockCmdQ.Work();

    coreInterface.cubeBCCWakeupQ.Work();
    coreInterface.vecBCCWakeupQ.Work();
    coreInterface.tmaBCCWakeupQ.Work();
    coreInterface.tauBCCWakeupQ.Work();
    coreInterface.mcallBCCWakeupQ.Work();
    coreInterface.schedulerBCCRslvQ.Work();

    coreInterface.stashCmdQ.Work();
    coreInterface.stashCompQ.Work();
    coreInterface.stashFreeQ.Work();
    coreInterface.stashRetireQ.Work();

    coreInterface.cubeStashCmdQ.Work();
    coreInterface.cubeStashCompQ.Work();
    coreInterface.stashCubeAllocDoneQ.Work();
    SimArrayWork(coreInterface.stashRslvArray);
    SimArrayWork(coreInterface.stashAllocDoneArray);
    SimArrayWork(coreInterface.toClearArray);

    coreInterface.bccVecliteBlockCmdQ.Work();
    SimArrayWork(coreInterface.bCCVecliteFlushArray);
    coreInterface.vecliteBccWakeupQ.Work();

    coreInterface.vecliteStashCmdQ.Work();
    coreInterface.vecliteStashCompQ.Work();
    SimArrayWork(coreInterface.stashVecliteAllocDoneArray);
    SimArrayWork(coreInterface.stashVecliteRslvArray);

    if (configs.mtc_separate_enable) {
        SimArrayWork(coreInterface.bCCMtcFlushArray);

        SimArrayWork(coreInterface.tileRegMtcLdResArray);
        SimArrayWork(coreInterface.tileRegMtcWrResArray);
        SimArrayWork(coreInterface.tileRegMtcLdRetryArray);
        SimArrayWork(coreInterface.tileRegMtcStRetryArray);

        SimArrayWork(coreInterface.mtcTileRegLdReqArray);
        SimArrayWork(coreInterface.mtcTileRegStReqArray);

        coreInterface.mtc_load_non_flush_q.Work();
        coreInterface.mtc_store_non_flush_q.Work();

        SimArrayWork(coreInterface.mtcCoreMemLdReqArray);
        SimArrayWork(coreInterface.mtcCoreMemLdResArray);
        SimArrayWork(coreInterface.mtcCoreMemStReqArray);
        SimArrayWork(coreInterface.mtcCoreMemStResArray);

        coreInterface.mtc_req_vec_siex_q.Work();
        coreInterface.mtc_rsp_siex_vec_q.Work();
        coreInterface.mtc_rreq_vec_srf_q.Work();
        coreInterface.mtc_rrsp_srf_vec_q.Work();
        coreInterface.mtc_wreq_vec_srf_q.Work();
        coreInterface.mtc_wrsp_srf_vec_q.Work();
        coreInterface.mtc_data_vec_viex_q.Work();
        coreInterface.mtc_data_viex_vec_q.Work();

        coreInterface.bccMTCBlockCmdQ.Work();

        coreInterface.mtcBCCWakeupQ.Work();
    }
}

void Core::Reset() {
    bctrl->Reset();
    for (auto &lsu : memIntf) {
        lsu->Reset();
    }
    if (configs.mtc_separate_enable) {
        for (auto &lsu : MtcmemIntf) {
            lsu->Reset();
        }
    }
    for (uint32_t i = 0; i <peArray.size(); i++) {
        if (i >= configs.stdPeCount && i < configs.stdPeCount + configs.simtPeCount) {
            continue;
        }
        peArray[i]->Reset();
    }
    vectorTop->Reset();
    vecliteTop->Reset();
    for (int i = 0; i < IEX_NUM; i++) {
        if (!IsVectorIex(static_cast<ExecEngineTyp>(i)) && iex[i]) {
            iex[i]->Reset();
        }
    }

    for (uint32_t i = 0; i < inst_l2_q.size(); ++i) {
        inst_l2_q[i].Reset();
        l2_inst_q[i].Reset();
        for (uint64_t j = 0; j < l2_ifu_array[i].size(); ++j) {
            l2_ifu_array[i][j].Reset();
        }
        l2_bfu_q[i].Reset();
        hpref_l2_q[i].Reset();
        snp_l2_q[i].Reset();
    }

    for (uint32_t i = 0; i < coreInterface.lsu_pe_rslv_array.size(); i++) {
        coreInterface.lsu_pe_rslv_array[i]->Reset();
    }

    for (uint32_t i = 0; i < coreInterface.iex_lsu_lda_array.size(); ++i) {
        for (uint32_t stid = 0; stid < configs.scalar_smt_thread; ++stid) {
            for(auto &simQ: coreInterface.iex_lsu_lda_array[i][stid]) {
                simQ->Reset();
            }
        }
    }

    for (uint32_t i = 0; i < coreInterface.lsu_iex_lret_array.size(); ++i) {
        for (uint32_t stid = 0; stid < configs.scalar_smt_thread; ++stid) {
            for(auto &simQ: coreInterface.lsu_iex_lret_array[i][stid]) {
                simQ->Reset();
            }
        }
    }

    for (uint32_t i = 0; i < coreInterface.iex_lsu_sta_array.size(); ++i) {
        for (uint32_t stid = 0; stid < configs.scalar_smt_thread; ++stid) {
            for(auto &simQ: coreInterface.iex_lsu_sta_array[i][stid]) {
                simQ->Reset();
            }
        }
    }

    for (uint32_t i = 0; i < coreInterface.iex_lsu_std_array.size(); ++i) {
        for (uint32_t stid = 0; stid < configs.scalar_smt_thread; ++stid) {
            for(auto &simQ: coreInterface.iex_lsu_std_array[i][stid]) {
                simQ->Reset();
            }
        }
    }

    coreInterface.bcc_flush_rpt_q->Reset();
    for (auto &rpt_q : coreInterface.iex_flush_rpt_q) {
        rpt_q->Reset();
    }
    for (auto simQ : coreInterface.pe_flush_rpt_array) {
        simQ->Reset();
    }
    for (auto &rptQ : coreInterface.lsu_flush_rpt_q) {
        rptQ->Reset();
    }

    if (pTracer) {
        pTracer->Reset();
    }
}

void Core::setBPC(uint64_t bpc, uint32_t stid) {
    CoreControlBus req;
    req.vld = true;
    req.initPC = bpc;

    // set block control core
    bctrl->interrupt(req, stid);
}

void Core::setGPR(uint32_t atag, uint64_t data, uint32_t stid) {
    uint32_t ptag = bctrl->scalarPE->d2Stage.ggprRenameUnit.GetCommitPTAG(atag, stid);
    iex[SCALAR_IEX]->setGPR(ptag, data);
}

uint64_t Core::getGPR(uint32_t atag, uint32_t stid) {
    uint32_t ptag = bctrl->scalarPE->d2Stage.ggprRenameUnit.GetCommitPTAG(atag, stid);
    return iex[SCALAR_IEX]->getGPR(ptag);
}

void Core::setSysreg(SystemReg sysID, uint64_t data){
    sysregFile[sysID] = data;
}

uint64_t Core::getSysreg(SystemReg sysID){
    return sysregFile[sysID];
}

void Core::setOffGPR(uint32_t offsetType, uint64_t data) {
    ASSERT(offsetType < OFFSET_COUNT && "Offset Type is not support");
    offsetGpr[offsetType] = data;
}

uint64_t Core::getOffGPR(uint32_t offsetType) {
    return offsetGpr[offsetType];
}

void Core::enableOffGPR() {
    offsetGprVld = true;
}

void Core::disableOffGPR() {
    offsetGprVld = false;
}

bool Core::getSwitchOffGPR() {
    return offsetGprVld;
}

uint32_t Core::getPECount() {
    return configs.stdPeCount + configs.simtPeCount + configs.memPeCount;
}

bool Core::IsScalarPe(uint32_t peId)
{
    return peId < configs.stdPeCount;
}

bool Core::IsScalarPe(MachineType machineType) const
{
    return machineType == MachineType::SPE;
}

bool Core::IsVecPe(uint32_t peId)
{
    return peId >= configs.stdPeCount && peId < configs.stdPeCount + configs.simtPeCount;
}

bool Core::IsVecPe(MachineType machineType) const
{
    return machineType == MachineType::VECTOR;
}

bool Core::IsMtcPe(uint32_t peId)
{
    return (peId >= (configs.stdPeCount + configs.simtPeCount));
}

uint32_t Core::GetMtcPeID()
{
    return (configs.stdPeCount + configs.simtPeCount);
}

void Core::route() {
    // Dispatch inst packet to IFU to BFU
    for (uint32_t i = 0; i < l2_inst_q.size(); ++i) {
        while (!l2_inst_q[i].Empty()) {
            PtrPacket pkt = l2_inst_q[i].Read();
            constexpr uint32_t IFU_PKT_ID_OFFSET = 16;
            if (pkt->id >= IFU_PKT_ID_OFFSET) {   // to IFU
                uint32_t ifuID = pkt->id - IFU_PKT_ID_OFFSET;
                ASSERT(ifuID < GetPECount());
                l2_ifu_array[i][ifuID].Write(pkt);
            } else {   // to BFU (id=1,2)
                l2_bfu_q[i].Write(pkt);
            }
        }
    }

    // LSU deque memRet in LSTop.cpp
}

void Core::PrintConfigs()
{
    bccPrintTable.configName = "BCC Config Name";
    vecPrintTable.configName = "VECTOR Config Name";
    cubePrintTable.configName = "CUBE Config Name";
    mtcPrintTable.configName = "Memory Core Config Name";
    tmuPrintTable.configName = "TMU Config Name";
    tmaPrintTable.configName = "TMA Config Name";

    vecPrintTable.setConfig("SRF: t_ptag_cnt", configs.GetCondfigValue("simt_local_reg_t"));
    vecPrintTable.setConfig("SRF: u_ptag_cnt", configs.GetCondfigValue("simt_local_reg_u"));
    vecPrintTable.setConfig("VRF: p_reg_size", configs.GetCondfigValue("simt_local_reg_p"));
    vectorTop->PrintConfigs();
    vecliteTop->PrintConfigs();

    mtcPrintTable.setConfig("SRF: t_ptag_cnt", configs.GetCondfigValue("mtc_local_reg_t"));
    mtcPrintTable.setConfig("SRF: u_ptag_cnt", configs.GetCondfigValue("mtc_local_reg_u"));
    mtcPrintTable.setConfig("VRF: p_reg_size", configs.GetCondfigValue("mtc_local_reg_p"));

    bccPrintTable.printTable();
    vecPrintTable.printTable();
    cubePrintTable.printTable();
    tmuPrintTable.printTable();
    tmaPrintTable.printTable();

    if (sim->core->configs.mtc_separate_enable) {
        mtcCores[0]->PrintCfg();
        mtcPrintTable.printTable();
    }
}

void Core::SetBccConfigs(const std::string& name, const std::string& value)
{
    bccPrintTable.setConfig(name, value);
}

void Core::SetConfigs(const std::string& name, const std::string& value)
{
    vecPrintTable.setConfig(name, value);
}

void Core::SetTmuConfigs(const std::string& name, const std::string& value)
{
    tmuPrintTable.setConfig(name, value);
}

void Core::SetCubeConfigs(const std::string& name, const std::string& value)
{
    cubePrintTable.setConfig(name, value);
}

void Core::SetMtcConfigs(const std::string& name, const std::string& value)
{
    mtcPrintTable.setConfig(name, value);
}

void Core::SetTmaConfigs(const std::string& name, const std::string& value)
{
    tmaPrintTable.setConfig(name, value);
}

void Core::ReportTileLoadLatDis()
{
    vectorTop->ReportTileLoadLatDis();
    if (sim->core->configs.mtc_separate_enable) {
        for (uint32_t i = 0; i < mtcCores.size(); i++) {
            mtcCores[i]->Report_load_latency();
        }
    }
}

void Core::ReportKeyStat()
{
    auto rpt = sim->getRpt();
    auto& s = bctrl->stats;
    rpt->ReportTitle("JCore Key Stats");
    rpt->ReportVal("JCore Tileop Total Cycles", s->retired_tileop_cycles);
    rpt->ReportVal("  |--Cube Tileop Total Cycles", s->retired_tileop_cube_cycles);
    rpt->ReportVal("  |--Vector Tileop Total Cycles", s->retired_tileop_vec_cycles);
    rpt->ReportVal("  |--TMA Tileop Total Cycles", s->retired_tileop_tma_cycles);
    rpt->ReportVal("  |--MTC Tileop Total Cycles", s->retired_tileop_mtc_cycles);

    rpt->ReportVal("JCore Total Tileop Counter", s->retired_tileop);
    rpt->ReportVal("  |--Cube Execute Tileop Counter", s->retired_tileop_cube);
    rpt->ReportVal("  |--Vector Execute Tileop Counter", s->retired_tileop_vec);
    rpt->ReportVal("  |--TMA Execute Tileop Counter", s->retired_tileop_tma);
    rpt->ReportVal("  |--MTC Execute Tileop Counter", s->retired_tileop_mtc);

    rpt->ReportVal("JCore Run Tileop Total Cycles", tileopTotalCycle);
    rpt->ReportVal("  |--Cube Busy Cycles", cubeBusyCycle);
    rpt->ReportVal("  |--Vector Busy Cycles", vecBusyCycle);
    rpt->ReportVal("  |--TMA Busy Cycles", tmaBusyCycle);
    rpt->ReportVal("    |--Tload Busy Cycles", tloadBusyCycle);
    rpt->ReportVal("      |--write Blocked By Ring Cycles", writeBlockedByRingCycle);
    rpt->ReportVal("    |--Tstore Busy Cycles", tstoreBusyCycle);
    rpt->ReportVal("    |--Tmov Busy Cycles", tmovBusyCycle);
    rpt->ReportVal("    |--Mgather Busy Cycles", mgatherBusyCycle);
    rpt->ReportVal("    |--Mscatter Busy Cycles", mscatterBusyCycle);
    rpt->ReportVal("    |--Vgather Busy Cycles", vgatherBusyCycle);
    rpt->ReportVal("    |--Vscatter Busy Cycles", vscatterBusyCycle);
    rpt->ReportVal("  |--MTC Busy Cycles", mtcBusyCycle);
    rpt->ReportVal("  |--All Cores Idle Cycles", tileIdleCycle);
    rpt->ReportVal("  |--Vector-Cube Cocurrent Cycles", vectorCubeConcurrentCycle);
    rpt->ReportVal("  |--Vector-Cube Active Cycles", vectorCubeActiveCycle);
    rpt->ReportVal("  |--Cube-TMA Cocurrent Cycles", cubeTMAConcurrentCycle);
    rpt->ReportVal("  |--Cube-TMA Active Cycles", cubeTMAActiveCycle);
    if (configs.singleTierMode) {
        rpt->ReportTitle("JCore Single Layer RF Stats");
        rpt->ReportVal("Cube Read TileReg Cnt", cubeReadRFReqCnt);
        rpt->ReportVal("Cube Real Read TileReg Cnt(512B)", cubeRealReadRFReqCnt);
        rpt->ReportVal("  |--Cube Read Arb Fail Cnt", cubeReadRFReqArbFailCnt);
        rpt->ReportVal("    |--Cube Blocked by TMA Cnt", cubeRdBlockByTmaCnt);
        rpt->ReportVal("    |--Cube Blocked by Vector Cnt", cubeRdBlockByVecCnt);
        rpt->ReportVal("  |--Cube Read Arb Fail By Not Own Pri Cnt", cubeReadRFReqArbFailByPriCnt);
        rpt->ReportVal("Tma Read TileReg Cnt", tmaReadRFReqCnt);
        rpt->ReportVal("Tma Real Read TileReg Cnt(256B)", tmaRealReadRFReqCnt);
        rpt->ReportVal("  |--Tma Read Arb Fail Cnt", tmaReadRFReqArbFailCnt);
        rpt->ReportVal("    |--Tma Blocked by Cube Cnt", tmaRdBlockByCubeCnt);
        rpt->ReportVal("    |--Tma Blocked by Vector Cnt", tmaRdBlockByVecCnt);
        rpt->ReportVal("  |--Tma Read Arb Fail By Not Own Pri Cnt", tmaReadRFReqArbFailByPriCnt);
        rpt->ReportVal("Vector Read TileReg Cnt", vectorReadRFReqCnt);
        rpt->ReportVal("Vector Real Read TileReg Cnt(512B)", vectorRealReadRFReqCnt);
        rpt->ReportVal("  |--Vector Read Arb Fail Cnt", vectorReadRFReqArbFailCnt);
        rpt->ReportVal("    |--Vector Blocked by TMA Cnt", vecRdBlockByTmaCnt);
        rpt->ReportVal("    |--Vector Blocked by Cube Cnt", vecRdBlockByCubeCnt);
        rpt->ReportVal("  |--Vector Read Arb Fail By Not Own Pri Cnt", vectorReadRFReqArbFailByPriCnt);

        rpt->ReportVal("Cube Write TileReg Cnt", cubeWriteRFReqCnt);
        rpt->ReportVal("Cube Real Write TileReg Cnt(256B)", cubeRealWriteRFReqCnt);
        rpt->ReportVal("  |--Cube Write Arb Fail Cnt", cubeWriteRFReqArbFailCnt);
        rpt->ReportVal("    |--Cube Write Blocked by TMA Cnt", cubeWrBlockByTmaCnt);
        rpt->ReportVal("    |--Cube Write Blocked by Vector Cnt", cubeWrBlockByVecCnt);
        rpt->ReportVal("  |--Cube Write Arb Fail By Not Own Pri Cnt", cubeWriteRFReqArbFailByPriorityCnt);
        rpt->ReportVal("Tma Write TileReg Cnt", tmaWriteRFReqCnt);
        rpt->ReportVal("Tma Real Write TileReg Cnt(256B)", tmaRealWriteRFReqCnt);
        rpt->ReportVal("  |--Tma Write Arb Fail Cnt", tmaWriteRFReqArbFailCnt);
        rpt->ReportVal("    |--Tma Write Blocked by Cube Cnt", tmaWrBlockByCubeCnt);
        rpt->ReportVal("    |--Tma Write Blocked by Vector Cnt", tmaWrBlockByVecCnt);
        rpt->ReportVal("  |--Tma Write Arb Fail By Not Own Pri Cnt", tmaWriteRFReqArbFailByPriorityCnt);
        rpt->ReportVal("Vector Write TileReg Cnt", vectorWriteRFReqCnt);
        rpt->ReportVal("Vector Real Write TileReg Cnt(512B)", vectorRealWriteRFReqCnt);
        rpt->ReportVal("  |--Vector Write Arb Fail Cnt", vectorWriteRFReqArbFailCnt);
        rpt->ReportVal("    |--Vector Write Blocked by TMA Cnt", vecWrBlockByTmaCnt);
        rpt->ReportVal("    |--Vector Write Blocked by Cube Cnt", vecWrBlockByCubeCnt);
        rpt->ReportVal("  |--Vector Write Arb Fail By Not Own Pri Cnt", vectorWriteRFReqArbFailByPriorityCnt);
    }
}

void Core::ReportBCCKeyStat()
{
    auto rpt = sim->getRpt();
    auto& s = bctrl->stats;
    rpt->ReportTitle("BCC Key Stats total");
    rpt->ReportVal("Average Outstanding Block Number", float(s->total_ostd_blocks) / s->cycles);
    rpt->ReportVal("  |--Average Outstanding Cube Block Number", float(s->total_ostd_cube_blocks) / s->cycles);
    rpt->ReportVal("  |--Average Outstanding Vector Block Number", float(s->total_ostd_vector_blocks) / s->cycles);
    rpt->ReportVal("  |--Average Outstanding TLoad Number", float(s->total_ostd_tload) / s->cycles);
    rpt->ReportVal("  |--Average Outstanding TStore Number", float(s->total_ostd_tstore) / s->cycles);

    for (size_t stid = 0; stid < configs.scalar_smt_thread; stid++) {
        rpt->ReportTitle("BCC Key Stats SThread " + std::to_string(stid));
        rpt->ReportVal("Average Outstanding Block Number", float(s->smtOstdBlocksArray[stid]) / s->cycles);
        rpt->ReportVal("  |--Average Outstanding Cube Block Number",
                       static_cast<float>(s->smtOstdCubeBlocksArray[stid]) / s->cycles);
        rpt->ReportVal("  |--Average Outstanding Vector Block Number",
                       static_cast<float>(s->smtOstdVectorBlocksArray[stid]) / s->cycles);
        rpt->ReportVal("  |--Average Outstanding TLoad Number",
                       static_cast<float>(s->smtOstdTloadArray[stid]) / s->cycles);
        rpt->ReportVal("  |--Average Outstanding TStore Number",
                       static_cast<float>(s->smtOstdTstoreArray[stid]) / s->cycles);
    }
}

void Core::ReportVectorKeyStat()
{
    vectorTop->ReportVectorKeyStat();
    vectorTop->ReportVectorTopDown();
}

void Core::ReportTMAKeyStat()
{
    auto rpt = sim->getRpt();
    for (auto& tileBridge : tileBridges) {
        JCore::GFSIM::TileBridge::PMUData& data = tileBridge->GetPMUData();

        rpt->ReportTitle("(NODB) TMA " + std::to_string(tileBridge->id) + " Key Stats");

        rpt->ReportVal("TMA Tload", data.tloadNum);
        uint64_t totalTloadUopNum = 0;
        uint64_t totalTloadReadDataSize = 0;
        for (const auto& pair : data.tloadReadStat) {
            std::string uopNumStr   = "  |--Read  uop num   (size " + std::to_string(pair.first) + ")";
            std::string dataSizeStr = "  |--Read  data size (size " + std::to_string(pair.first) + ")";
            auto accDataSize = pair.second * pair.first;
            rpt->ReportVal(uopNumStr, pair.second);
            rpt->ReportVal(dataSizeStr, accDataSize);
            totalTloadUopNum += pair.second;
            totalTloadReadDataSize += accDataSize;
        }
        rpt->ReportVal("  |--Write uop num", data.tloadWriteNum);
        rpt->ReportVal("  |--Write data size", data.tloadWriteDataSize);
        rpt->ReportVal("  |--Write valid data size", data.tloadWriteValidDataSize);
        if (totalTloadReadDataSize != 0) {
            rpt->ReportVal("  |--Data utilization", data.tloadWriteValidDataSize * 1.0 / totalTloadReadDataSize);
        }

        rpt->ReportVal("TMA Tstore", data.tstoreNum);
        uint64_t totalTstoreUopNum = 0;
        uint64_t totalTstoreWriteDataSize = 0;
        rpt->ReportVal("  |--Read  uop num", data.tstoreReadNum);
        rpt->ReportVal("  |--Read  data size", data.tstoreReadDataSize);
        for (const auto& pair : data.tstoreWriteStat) {
            std::string uopNumStr   = "  |--Write uop num   (size " + std::to_string(pair.first) + ")";
            std::string dataSizeStr = "  |--Write data size (size " + std::to_string(pair.first) + ")";
            auto accDataSize = pair.second * pair.first;
            rpt->ReportVal(uopNumStr, pair.second);
            rpt->ReportVal(dataSizeStr, accDataSize);
            totalTstoreUopNum += pair.second;
            totalTstoreWriteDataSize += accDataSize;
        }
        rpt->ReportVal("  |--Write valid data size", data.tstoreWriteValidDataSize);
        if (data.tstoreReadDataSize != 0) {
            rpt->ReportVal("  |--Data utilization", data.tstoreWriteValidDataSize * 1.0 / data.tstoreReadDataSize);
        }

        rpt->ReportVal("TMA Tmov", data.tcNum);
        rpt->ReportVal("  |--Read uop num", data.tcReadNum);
        rpt->ReportVal("  |--Write uop num", data.tcWriteNum);
        rpt->ReportVal("  |--Read Data size", data.tcReadDataSize);

        if (data.mgatherNum != 0) {
            rpt->ReportVal("TMA Mgather", data.mgatherNum);
            rpt->ReportVal("  |--Read Tile uop num", data.mgatherReadTileNum);
            rpt->ReportVal("  |--Read Tag req num", data.mgatherReadTagReqNum);
            rpt->ReportVal("  |--Read Mem req num", data.mgatherSendReadMemReqNum);
            rpt->ReportVal("  |--Write uop num", data.mgatherWriteNum);
            rpt->ReportVal("  |--Write data size", data.mgatherWriteSize);
            rpt->ReportVal("  |--Tag valid data size", data.mgatherReadTagValidDataSize);
            rpt->ReportVal("  |--Tag data utilization ratio",
                data.mgatherReadTagValidDataSize * 1.0 / (data.mgatherReadTagReqNum * MAX_TILE_DATA_BYTE));
        }

        if (data.vgatherNum != 0) {
            rpt->ReportVal("TMA Vgather", data.vgatherNum);
            rpt->ReportVal("  |--Read Tile uop num", data.vgatherReadTileNum);
            rpt->ReportVal("  |--Read Tag req num", data.vgatherReadTagReqNum);
            rpt->ReportVal("  |--Read Mem req num", data.vgatherSendReadMemReqNum);
            rpt->ReportVal("  |--Write uop num", data.vgatherWriteNum);
        }

        if (data.mscatterNum != 0) {
            rpt->ReportVal("TMA Mscatter", data.mscatterNum);
            rpt->ReportVal("  |--Read Tag req num", data.mscatterReadTagReqNum);
            rpt->ReportVal("  |--Read Mem req num", data.mscatterSendReadMemReqNum);
        }

        if (data.vscatterNum != 0) {
            rpt->ReportVal("TMA Vscatter", data.vscatterNum);
            rpt->ReportVal("  |--Read Tag req num", data.vscatterReadTagReqNum);
            rpt->ReportVal("  |--Read Mem req num", data.vscatterSendReadMemReqNum);
        }

        if (tileBridge->m_config.OCCUPANCY_VERBOSE) {
            const uint64_t PERCENT_SCALE = 100;
            uint64_t bpq75pRatio = 0;
            uint64_t bpq100Rratio = 0;
            if (data.bpqNonEmptryCycles != 0) {
                bpq75pRatio  = (data.bpq75pCycles * PERCENT_SCALE) / data.bpqNonEmptryCycles;
                bpq100Rratio = (data.bpqFullCycles * PERCENT_SCALE) / data.bpqNonEmptryCycles;
            }
            rpt->ReportVal("TMA BPQ Active Cycles", data.bpqNonEmptryCycles);
            rpt->ReportVal("  |-- 75% occupancy ratio", bpq75pRatio);
            rpt->ReportVal("  |--100% occupancy ratio", bpq100Rratio);

            uint64_t bdb_75p_ratio = 0;
            uint64_t bdb_100p_ratio = 0;
            if (data.tlbdbNonEmptyCycles != 0) {
                bdb_75p_ratio  = (data.tlbdb75pCycles * PERCENT_SCALE) / data.tlbdbNonEmptyCycles;
                bdb_100p_ratio = (data.tlbdbFullCycles * PERCENT_SCALE) / data.tlbdbNonEmptyCycles;
            }

            rpt->ReportVal("TMA TL BDB Active Cycles", data.tlbdbNonEmptyCycles);
            rpt->ReportVal("  |-- 75% occupancy ratio", bdb_75p_ratio);
            rpt->ReportVal("  |--100% occupancy ratio", bdb_100p_ratio);

            bdb_75p_ratio = 0;
            bdb_100p_ratio = 0;
            if (data.tsbdbNonEmptyCycles != 0) {
                bdb_75p_ratio  = (data.tsbdb75pCycles * PERCENT_SCALE) / data.tsbdbNonEmptyCycles;
                bdb_100p_ratio = (data.tsbdbFullCycles * PERCENT_SCALE) / data.tsbdbNonEmptyCycles;
            }
            rpt->ReportVal("TMA TS BDB Active Cycles", data.tsbdbNonEmptyCycles);
            rpt->ReportVal("  |-- 75% occupancy ratio", bdb_75p_ratio);
            rpt->ReportVal("  |--100% occupancy ratio", bdb_100p_ratio);

            uint64_t srfb_75p_ratio  = 0;
            uint64_t srfb_100p_ratio = 0;
            if (data.srfbNonEmptyCycles != 0) {
                srfb_75p_ratio  = (data.srfb75pCycles * PERCENT_SCALE) / data.srfbNonEmptyCycles;
                srfb_100p_ratio = (data.srfbFullCycles * PERCENT_SCALE) / data.srfbNonEmptyCycles;
            }

            rpt->ReportVal("TMA SRFB Active Cycles", data.srfbNonEmptyCycles);
            rpt->ReportVal("  |-- 75% occupancy ratio", srfb_75p_ratio);
            rpt->ReportVal("  |--100% occupancy ratio", srfb_100p_ratio);

            uint64_t nwcb_75p_ratio  = 0;
            uint64_t nwcb_100p_ratio = 0;
            if (data.nwcbNonEmptyCycles != 0) {
                nwcb_75p_ratio  = (data.nwcb75pCycles * PERCENT_SCALE) / data.nwcbNonEmptyCycles;
                nwcb_100p_ratio = (data.nwcbFullCycles * PERCENT_SCALE) / data.nwcbNonEmptyCycles;
            }

            rpt->ReportVal("TMA NWCB Active Cycles", data.nwcbNonEmptyCycles);
            rpt->ReportVal("  |-- 75% occupancy ratio", nwcb_75p_ratio);
            rpt->ReportVal("  |--100% occupancy ratio", nwcb_100p_ratio);

            uint64_t swcb_75p_ratio  = 0;
            uint64_t swcb_100p_ratio = 0;
            if (data.swcbNonEmptyCycles != 0) {
                swcb_75p_ratio  = (data.swcb75pCycles * PERCENT_SCALE) / data.swcbNonEmptyCycles;
                swcb_100p_ratio = (data.swcbFullCycles * PERCENT_SCALE) / data.swcbNonEmptyCycles;
            }

            rpt->ReportVal("TMA SWCB Active Cycles", data.swcbNonEmptyCycles);
            rpt->ReportVal("  |-- 75% occupancy ratio", swcb_75p_ratio);
            rpt->ReportVal("  |--100% occupancy ratio", swcb_100p_ratio);

            uint64_t srfbReadAvgCycle = 0;
            uint64_t swcbWriteAvgCycle = 0;
            if (data.srfbEntryNum != 0) {
                srfbReadAvgCycle = data.srfbEntryReadTotalCycles / data.srfbEntryNum;
            }
            if (data.swcbEntryNum != 0) {
                swcbWriteAvgCycle = data.swcbEntryWriteTotalCycles / data.swcbEntryNum;
            }
            rpt->ReportVal("Tag req duplicate send and hit cnt", data.tagReqDuplicateHitInflightCnt);
            rpt->ReportVal("Tag req duplicate sent and miss cnt", data.tagReqFirstHitThenMissCnt);
            rpt->ReportVal("SRFB read soc avg latency", srfbReadAvgCycle);
            rpt->ReportVal("SWCB write soc avg latency", swcbWriteAvgCycle);
        }

        if (tileBridge->m_config.TOP_DOWN_PMU_VERBOSE) {
            uint64_t fetchCycles = data.latBoundCycles +
                                   data.ostBoundCycles +
                                   data.varBoundCycles +
                                   data.varOstBoundCycles;
            uint64_t unavailCycles = fetchCycles + data.startingCycles + data.integCycles;
            uint64_t totalCycles =  data.idleCycles +
                                    data.efficientCycles +
                                    unavailCycles +
                                    data.egressBlockCycles;
            rpt->ReportTitle("TMA " + std::to_string(tileBridge->id) + " TopDown");
            rpt->ReportValAndPct("Idle", data.idleCycles, totalCycles);
            rpt->ReportValAndPct("Efficient", data.efficientCycles, totalCycles);
            rpt->ReportValAndPct("Unavailable", unavailCycles, totalCycles);
            rpt->ReportValAndPct("  |--Starting", data.startingCycles, totalCycles);
            rpt->ReportValAndPct("  |--Fetching", fetchCycles, totalCycles);
            rpt->ReportValAndPct("    |--Latency Bounded", data.latBoundCycles, totalCycles);
            rpt->ReportValAndPct("    |--Outstanding Bounded", data.ostBoundCycles, totalCycles);
            rpt->ReportValAndPct("    |--Variance Bounded", data.varBoundCycles, totalCycles);
            rpt->ReportValAndPct("    |--Var & Ost Bounded", data.varOstBoundCycles, totalCycles);
            rpt->ReportValAndPct("  |--Integrating", data.integCycles, totalCycles);
            rpt->ReportValAndPct("Egress Blocked", data.egressBlockCycles, totalCycles);
        }
    }
}

void Core::ReportTopdown()
{
    auto rpt = sim->getRpt();
    auto& stats = bctrl->stats;

    rpt->ReportTitle("BlockISA first Top-down");
    uint32_t width = bctrl->scalarPE->configs.decodeWidth;
    uint64_t slots = stats->cycles;
    slots = slots == 0 ? 1 : slots;
    uint64_t efficient = stats->GetEfficient();
    uint64_t bccBound = stats->GetBccBound();
    uint64_t bccSlots = width * bccBound;
    uint64_t peBound = stats->GetPeBound();

    uint64_t bccEfficient = stats->GetBccBoundEfficient();
    uint64_t bccRobStall = stats->GetBccBoundBackendRobStall();
    uint64_t bccRegStall = stats->GetBccBoundBackendRegStall();
    uint64_t bccIqStall = stats->GetBccBoundBackendIqStall();
    uint64_t bccMemBound = stats->GetBccBoundBackendMemBound();
    uint64_t bccFrontend = bccSlots > (bccEfficient + bccRobStall + bccRegStall + bccIqStall + bccMemBound) ?
        bccSlots - bccEfficient - bccRobStall - bccRegStall - bccIqStall - bccMemBound : 0;

    uint64_t cubeBound = stats->GetPeBoundCube();
    uint64_t vecBound = stats->GetPeBoundVec();
    uint64_t tmaBound = stats->GetPeBoundTma();
    uint64_t tauBound = stats->GetPeBoundTau();

    uint64_t cubeBoundBrob = stats->GetPeBoundCubeBrobStall();
    uint64_t cubeBoundBiq = stats->GetPeBoundCubeBiqStall();
    uint64_t cubeBoundTtag = stats->GetPeBoundCubeTileTagStall();

    uint64_t vecBoundBrob = stats->GetPeBoundVecBrobStall();
    uint64_t vecBoundBiq = stats->GetPeBoundVecBiqStall();
    uint64_t vecBoundTtag = stats->GetPeBoundVecTileTagStall();

    uint64_t tmaBoundBrob = stats->GetPeBoundTmaBrobStall();
    uint64_t tmaBoundBiq = stats->GetPeBoundTmaBiqStall();
    uint64_t tmaBoundTtag = stats->GetPeBoundTmaTileTagStall();

    uint64_t tauBoundBrob = stats->GetPeBoundTauBrobStall();
    uint64_t tauBoundBiq = stats->GetPeBoundTauBiqStall();
    uint64_t tauBoundTtag = stats->GetPeBoundTauTileTagStall();
    float rate = 1.0 / width;

    rpt->ReportPct("Retiring", efficient, slots);
    rpt->ReportPct("Frontend", bccBound, slots);
    rpt->ReportPct("  |-- frontend", static_cast<float>(bccFrontend) * rate / static_cast<float>(slots));
    rpt->ReportPct("  |-- retiring", static_cast<float>(bccEfficient) * rate / static_cast<float>(slots));
    rpt->ReportPct("  |-- rob stall", static_cast<float>(bccRobStall) * rate / static_cast<float>(slots));
    rpt->ReportPct("  |-- reg stall", static_cast<float>(bccRegStall) * rate / static_cast<float>(slots));
    rpt->ReportPct("  |-- iq stall", static_cast<float>(bccIqStall) * rate / static_cast<float>(slots));
    rpt->ReportPct("  |-- mem bound", static_cast<float>(bccMemBound) * rate / static_cast<float>(slots));
    rpt->ReportPct("Backend", peBound, slots);
    rpt->ReportPct("  |-- cube Bound", cubeBound, slots);
    rpt->ReportPct("    |-- brob stall", cubeBoundBrob, slots);
    rpt->ReportPct("    |-- biq stall", cubeBoundBiq, slots);
    rpt->ReportPct("    |-- tile reg stall", cubeBoundTtag, slots);
    rpt->ReportPct("  |-- vec Bound", vecBound, slots);
    rpt->ReportPct("    |-- brob stall", vecBoundBrob, slots);
    rpt->ReportPct("    |-- biq stall", vecBoundBiq, slots);
    rpt->ReportPct("    |-- tile reg stall", vecBoundTtag, slots);
    rpt->ReportPct("  |-- tma Bound", tmaBound, slots);
    rpt->ReportPct("    |-- brob stall", tmaBoundBrob, slots);
    rpt->ReportPct("    |-- biq stall", tmaBoundBiq, slots);
    rpt->ReportPct("    |-- tile reg stall", tmaBoundTtag, slots);
    rpt->ReportPct("  |-- tau Bound", tauBound, slots);
    rpt->ReportPct("    |-- brob stall", tauBoundBrob, slots);
    rpt->ReportPct("    |-- biq stall", tauBoundBiq, slots);
    rpt->ReportPct("    |-- tile reg stall", tauBoundTtag, slots);
}

void Core::ReportStat() {
    auto rpt = sim->getRpt();
    auto& s = bctrl->stats;
    if (!otherInstMap.empty()) {
        std::cout<<"(NODB)Other Inst: ";
        for (const auto& pair : otherInstMap) {
            std::cout<<OpcodeManager::Inst().GetOpcodeStr(pair.first)<<", "<<pair.second<<"; ";
        }
        std::cout<<std::endl;
    }
    PrintConfigs();
    // top-level summary
    rpt->ReportTitle("BlockISA CPU Stats");
    rpt->ReportVal("Total Cycles", s->cycles);
    rpt->ReportVal("Sim Total Cycles", sim->getCycles());
    ReportTopdown();
    ReportKeyStat();
    ReportBCCKeyStat();
    ReportCUBE();
    ReportVectorKeyStat();
    ReportTMAKeyStat();
    if (s->cycles == 0) return;
    uint64_t total_binsts = s->binsts;
    uint64_t total_retire_insts = 0;
    uint64_t cube_retired_insts = 0;
    uint64_t vec_retired_insts = 0;
    uint64_t mtc_retired_insts = 0;
    for (auto& cubeCore : cubeCores) {
        cube_retired_insts += cubeCore->cubeStats->exeTileOpCount;
    }
    for (uint32_t i = 0; i < peArray.size(); i++) {
        // 后续重新计算总指令数
        // shared_ptr<OPE> ope;
        // if (i >= configs.stdPeCount && i < configs.stdPeCount + configs.simtPeCount) {
        //     ope = vectorTop->GetPE(i - configs.stdPeCount);
        // } else {
        //     ope = (dynamic_pointer_cast<OPE>(peArray[i]));
        // }

        // uint32_t pe_rob_num = ope->prob.size();
        // if (i >= configs.stdPeCount) {
        //     pe_rob_num = 1;
        // }
        // for (uint32_t j = 0; j < pe_rob_num; ++j) {
        //     total_retire_insts += ope->stats->minsts;
        //     if (IsVecPe(i)) {
        //         vec_retired_insts = ope->stats->minsts;
        //     } else if (i > configs.stdPeCount) {
        //         mtc_retired_insts = ope->stats->minsts;
        //     }
        // }
    }
    total_retire_insts += cube_retired_insts;

    s->ReportTopDownInst();
    rpt->ReportVal("Retired Inst Number", total_retire_insts);
    rpt->ReportVal("  |--Retired Cube Inst Number", cube_retired_insts);
    rpt->ReportVal("  |--Retired Vec Inst Number", vec_retired_insts);
    rpt->ReportVal("  |--Retired Mtc Inst Number", mtc_retired_insts);
    rpt->ReportVal("BPC", float(total_binsts) / s->cycles);
    rpt->ReportVal("IPC", float(total_retire_insts) / s->cycles);
    rpt->ReportVal("Average BROB depth", float(s->total_brob_size) / s->cycles);
    rpt->ReportVal("Effective window size", (float(s->total_brob_size) / s->cycles) * (float(s->minsts) / total_binsts));
    rpt->ReportAvg("MPKB", float(s->retired_mispred) * 1000, float(s->retired_header));
    rpt->ReportAvg("MPKI", float(s->retired_mispred) * 1000, float(total_retire_insts));
    auto t = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - timePoint);
    rpt->ReportVal("Simulation speed (Kilo Insts Per Second)", total_retire_insts / double(t.count()));
    rpt->ReportVal("Discontinuous BPC Count", s->bpc_discontinue_count);
    rpt->ReportVal("Average Continuous BPC Length", float(s->binsts) / float(s->bpc_discontinue_count));
    uint64_t totalFetchCnt = s->fetchMinstCnt + s->fetchHdrCnt;
    rpt->ReportVal("Average fetch minsts/header per cycle", float(totalFetchCnt) / float(s->exist_minst_cycle));

    if (configs.print_histogram) {
        rpt->ReportTitle("BCC Detail Stats(Block issue)");
        rpt->ReportHistogram("Histogram of Outstanding Cube Blocks (Cycles)", s->issued_cube_blocks_histogram);
        rpt->ReportHistogram("Histogram of Outstanding Vector Blocks (Cycles)", s->issued_vector_blocks_histogram);
        rpt->ReportHistogram("Histogram of Outstanding TLoad (Cycles)", s->issued_tload_histogram);
        rpt->ReportHistogram("Histogram of Outstanding TStore (Cycles)", s->issued_tstore_histogram);
    }
    ReportBCC();
    ReportVectorCore();
    ReportMTCCore();
    ReportTileReg();
    rpt->ReportTitle("BlockISA CPU Report Stop");
}

void Core::ReportBCCTileRename()
{
    auto rpt = sim->getRpt();
    auto& s = bctrl->stats;
    rpt->ReportTitle("BCC Detail Stats(Tile rename)");
    uint64_t totalTagUtilization = std::accumulate(s->tileregTagUsage.begin(), s->tileregTagUsage.end(), 0);
    rpt->ReportAvg("Tile register average tag num", static_cast<uint64_t>(totalTagUtilization),
                   static_cast<uint64_t>(s->cycles));
    for (size_t i = 0; i < TILE_TYPE_COUNT; ++i) {
        rpt->ReportAvg("  |--" + GetTileTypeStr(TileIdx2TileType(i)),
            s->tileregTagUsage[i], static_cast<uint64_t>(s->cycles));
    }
    if (configs.print_histogram) {
        for (size_t i = 0; i < TILE_TYPE_COUNT; ++i) {
            rpt->ReportHistogram("Histogram of " + GetTileTypeStr(TileIdx2TileType(i)) +
                "-TileReg Tag Num (Cycles)", s->tileregTagUsageHistogram[i]);
        }
    }
    uint64_t totalUtilization = 0;
    for (size_t i = 0; i < TILE_TYPE_COUNT; ++i) {
        totalUtilization += s->tileRenAllocSize[i];
    }
    rpt->ReportAvg("Tile register average utilization(B)", static_cast<float>(totalUtilization),
                   static_cast<float>(s->cycles));
    for (size_t i = 0; i < TILE_TYPE_COUNT; ++i) {
        float averageUtilization = s->cycles == 0 ? 0
                                   : static_cast<float>(s->tileRenAllocSize[i]) / static_cast<float>(s->cycles);
        float totalAverageUtilization = s->cycles == 0 ? 0
                                        : static_cast<float>(totalUtilization) / static_cast<float>(s->cycles);
        rpt->ReportValAndPct("  |--" + GetTileTypeStr(TileIdx2TileType(i)), averageUtilization,
                             totalAverageUtilization);
    }
    for (size_t i = 0; i < TILE_TYPE_COUNT; ++i) {
        if (s->tileMaxHandSize[i] == 0) {
            continue;
        }
        rpt->ReportVal("Tile register max size of " + GetTileTypeStr(TileIdx2TileType(i)), s->tileMaxHandSize[i]);
        rpt->ReportValAndPct("  |-- utilization", s->tileRenMaxAllocSize[i], s->tileMaxHandSize[i]);
    }
    for (size_t i = 0; i < TILE_TYPE_COUNT; ++i) {
        rpt->ReportAvg(GetTileTypeStr(TileIdx2TileType(i)) + " MAPQ average occupied size",
                       static_cast<float>(s->tileMapQOcupiedSize[i]), static_cast<float>(s->cycles));
    }
}

void Core::ReportBCC()
{
    ReportBCCTileRename();
    bctrl->stats->ReportBccStats();
    flushUnit->flush_stats->ReportFlush();
}

void Core::ReportCUBE()
{
    for (auto& cubeCore : cubeCores) {
        cubeCore->ReportStat();
    }
}

void Core::ReportVectorCore()
{
    vectorTop->ReportStat();
    vecliteTop->ReportStat();
}

void Core::ReportMTCCore()
{
    if (!configs.mtc_separate_enable) {
        return;
    }
    peArray[configs.stdPeCount + configs.simtPeCount]->ReportStat();
    iex[MEM_IEX]->ReportStat();

    for (auto &lsu : MtcmemIntf) {
        lsu->Report();
    }

    for (uint32_t i = 0; i < mtcCores.size(); i++) {
        mtcCores[i]->ReportStat();
    }
}

inline std::string DualPENodeStr(unsigned index)
{
    switch (index) {
        case 0:   return "Cube0 Read";
        case 1:   return "Cube1 Read";
        case 2:   return "Cube2 Read";
        case 3:   return "Cube3 Read";
        case 4:   return "Cube0 Write";
        case 5:   return "Cube1 Write";
        case 6:   return "Cube2 Write";
        case 7:   return "Cube3 Write";
        case 8:   return "Vector0 Read0";
        case 9:   return "Vector0 Read1";
        case 10:   return "Vector1 Read0";
        case 11:   return "Vector1 Read1";
        case 12:   return "Vector0 Write";
        case 13:   return "Vector1 Write";
        case 14:   return "TMA Read";
        case 15:   return "TMA Write";
        default:   return "Node" + std::to_string(index);
    }
}

inline std::string SinglePENodeStr(unsigned index)
{
    switch (index) {
        case 0:   return "Cube Read";
        case 1:   return "Cube Write";
        case 2:   return "TMA Read";
        case 3:   return "TMA Write";
        case 4:   return "BCC";
        case 5:   return "Vector Read0";
        case 6:   return "Vector Read1";
        case 7:   return "Vector Write";
        default:  return "Node" + std::to_string(index);
    }
}

void Core::ReportTileReg()
{
    for (size_t i = 0; i < stations.size(); i++) {
        std::string name = nodeToPEMap[i];
        stations[i]->stat.Report(name);
    }
    auto rpt = sim->getRpt();
    rpt->ReportTitle("Total request and response counter");
    for (size_t i = 0; i < stations.size(); i++) {
        std::string name = "Node " + to_string(i);
        uint64_t totalCnt = stations[i]->stat.ldReqCntOfstart + stations[i]->stat.stReqCntOfstart
                            + stations[i]->stat.ldReqCntOfEnd + stations[i]->stat.stReqCntOfEnd
                            + stations[i]->stat.ldRespCntOfStart + stations[i]->stat.stRespCntOfStart
                            + stations[i]->stat.ldRespCntOfEnd + stations[i]->stat.stRespCntOfEnd;
        // counter of request or response start/end from/at this station
        rpt->ReportVal(name, totalCnt);
        std::string tileName = "  |--" + name + "-->TileReg,load request counter";
        rpt->ReportValAndPct(tileName, stations[i]->stat.ldReqCntOfstart, totalCnt);
        tileName = "  |--" + name + "-->TileReg,store request counter";
        rpt->ReportValAndPct(tileName, stations[i]->stat.stReqCntOfstart, totalCnt);
        tileName = "  |--" + stations[i]->stat.nodeName + "-->" + name + ",load request counter";
        rpt->ReportValAndPct(tileName, stations[i]->stat.ldReqCntOfEnd, totalCnt);
        tileName = "  |--" + stations[i]->stat.nodeName + "-->" + name + ",store request counter";
        rpt->ReportValAndPct(tileName, stations[i]->stat.stReqCntOfEnd, totalCnt);
        tileName = "  |-- TileReg-->" + name + ",load response counter";
        rpt->ReportValAndPct(tileName, stations[i]->stat.ldRespCntOfEnd, totalCnt);
        tileName = "  |-- TileReg-->" + name + ",store response counter";
        rpt->ReportValAndPct(tileName, stations[i]->stat.stRespCntOfEnd, totalCnt);
        tileName = "  |--" + name + "-->" + stations[i]->stat.nodeName + ",load response counter";
        rpt->ReportValAndPct(tileName, stations[i]->stat.ldRespCntOfStart, totalCnt);
        tileName = "  |--" + name + "-->" + stations[i]->stat.nodeName + ",store response counter";
        rpt->ReportValAndPct(tileName, stations[i]->stat.stRespCntOfStart, totalCnt);
    }
    rpt->ReportTitle("Max request 0 channel width");
    for (size_t i = 0; i < stations.size(); i++) {
        // size of request start/end from/at this station
        std::string name = "Node " + to_string(i);
        uint64_t channelWidth = sim->cycles * MAX_TILE_DATA_BYTE * LOG_2;
        rpt->ReportVal(name, channelWidth);
        std::string tileName = "  |--" + stations[i]->stat.nodeName + "-->" + name + ",load request size";
        rpt->ReportValAndPct(tileName, stations[i]->stat.ldReqCntOfstartSize, channelWidth);
        tileName = "  |--" + name + "-->TileReg,load request size";
        rpt->ReportValAndPct(tileName, stations[i]->stat.ldReqCntOfEndSize, channelWidth);
    }
    rpt->ReportTitle("Max data channel width");
    for (size_t i = 0; i < stations.size(); i++) {
        // size of response start/end from/at this station
        std::string name = "Node " + to_string(i);
        uint64_t channelWidth = sim->cycles * MAX_TILE_DATA_BYTE * LOG_2;
        rpt->ReportVal(name, channelWidth);
        std::string tileName = "  |--TileReg-->" + name + ",load response size";
        rpt->ReportValAndPct(tileName, stations[i]->stat.ldRespCntOfStartSize, channelWidth);
        tileName = "  |--" + name + "-->" + stations[i]->stat.nodeName + ",load response size";
        rpt->ReportValAndPct(tileName, stations[i]->stat.ldRespCntOfEndSize, channelWidth);
        tileName = "  |--" + stations[i]->stat.nodeName + "-->" + name + ",store request size";
        rpt->ReportValAndPct(tileName, stations[i]->stat.stReqCntOfstartSize, channelWidth);
        tileName = "  |--" + name + "-->TileReg,store request size";
        rpt->ReportValAndPct(tileName, stations[i]->stat.stReqCntOfEndSize, channelWidth);
    }
    // ring statistics
    cwRing->stat.nodeToPEMap.insert(nodeToPEMap.begin(), nodeToPEMap.end());
    cwRing->stat.ReportBothRing(ccRing->stat);
    std::string ringName = "Clockwise Ring";
    cwRing->stat.Report(ringName);
    ringName = "Counterclockwise Ring";
    ccRing->stat.Report(ringName);
    std::string tileName = "Tile register";
    tileReg->stat.Report(tileName);
}

void Core::resetStats() {
    timePoint = std::chrono::system_clock::now();
    bctrl->stats->Reset();
    for (uint32_t i = 0; i < configs.scalar_smt_thread; ++i) {
        bctrl->stats->slots_total += bctrl->blockROB.getBROBSize(i);
    }
    sRenameUnit->stats->Reset();
    flushUnit->flush_stats->Reset();
    flushUnit->flush_stats->InitSmtPmu(configs.scalar_smt_thread);
    vectorTop->ResetStats();
    vecliteTop->ResetStats();

    for (uint32_t i = 0; i < IEX_NUM; i++) {
        if (!IsVectorIex(static_cast<ExecEngineTyp>(i)) && iex[i]) {
            iex[i]->resetStats();
        }
    }

    for (auto info : bctrl->scalarPE->dcTop.statsInfo) {
        info = StatsInfo();
    }

    for (uint32_t i = 0 ;i < peArray.size(); ++i) {
        // shared_ptr<OPE> ope;
        // if (i >= configs.stdPeCount && i < configs.stdPeCount + configs.simtPeCount) {
        //     ope = vectorTop->GetPE(i - configs.stdPeCount);
        // } else {
        //     ope = (dynamic_pointer_cast<OPE>(peArray[i]));
        // }
        // ope->resetStats();

        // for (uint32_t tid = 0; tid < ope->prob.size(); ++tid) {
        //     ope->stats->slots_total += ope->prob[tid]->getROBSize();
        //     if (!IsVectorIex(ope->iexTyp)) {
        //         iex[ope->iexTyp]->stats->slots += ope->prob[tid]->getROBSize();
        //     }
        // }
    }

    for (auto &lsu : memIntf) {
        lsu->resetStats(); // LSUTop chooses fakeLSU or LSU array
    }
    for (auto &lsu : MtcmemIntf) {
        lsu->ResetStats(); // LSUTop chooses fakeLSU or LSU array
    }
    cubeBusyCycle = 0;
    vecBusyCycle = 0;
    mtcBusyCycle = 0;
    tileIdleCycle = 0;
}

void Core::setWarmup(uint64_t wB, uint32_t wI) {
    warmupBlockNum = wB;
    warmupMinstNum = wI;

    if (wB > 0 || wI > 0) {
        warmupFinished = false;
    } else {
        warmupFinished = true;
        OnWarmupFinish();
    }
}

void Core::OnWarmupFinish() {
    resetStats();
    if (configs.bp_mode == 0) {
        // enable bfu logger
        bctrl->blockFetchUnit.bfu->OnWarmupFinish();
    }
}

void Core::watch_dog(void) {
    uint64_t tick = GetSim()->getCycles();
    if ((tick % configs.watch_dog_timer) == 0) {
        uint64_t total_binst = bctrl->stats->binsts;
        for (uint32_t i = 0; i < configs.scalar_smt_thread; ++i) {
            BlockCommandPtr cmd = bctrl->blockROB.GetBlockCMDPtr(bctrl->blockROB.getOldestBlockID(i), i);
            if (cmd) {
                LOG_ERROR << "tid: " << dec << i << " Core@" << std::dec << tick << ": Retired blocks "
                          << total_binst << ". BROB head info: " << cmd->Dump();
            }
        }
    }
}

void Core::handleSysReq() {
    auto handleSysRegReq = [this](SystemReg& regType, POperandPtr& retOpd, uint32_t i) {
        if (regType == SystemReg::SYS_LXLCID || regType == SystemReg::SYS_TEMP_CORE_ID) {
            retOpd->data = coreInterface.sys_rd_req[i]->stid;
        } else {
            retOpd->data = getSysreg(regType);
        }
    };
    // Read system register
    for (uint32_t i = 0; i < coreInterface.sys_rd_req.size(); i++) {
        coreInterface.sys_data_ret[i]->Reset();
        if (!coreInterface.sys_rd_req[i]->vld) {
            continue;
        }
        coreInterface.sys_data_ret[i]->vld = true;
        coreInterface.sys_data_ret[i]->bid = coreInterface.sys_rd_req[i]->bid;
        coreInterface.sys_data_ret[i]->rid = coreInterface.sys_rd_req[i]->rid;
        coreInterface.sys_data_ret[i]->peid = coreInterface.sys_rd_req[i]->peid;
        size_t srcSize = coreInterface.sys_rd_req[i]->srcs.size();
        for (size_t j = 0; j < srcSize; j++) {
            auto &reqOpd = coreInterface.sys_rd_req[i]->srcs[j];
            if (reqOpd->type == OperandType::OPD_SYS) {
                POperandPtr retOpd = std::make_shared<PhysicalOperand>(OperandType::OPD_SYS, reqOpd->value);
                SystemReg regType = static_cast<SystemReg>(reqOpd->value);
                handleSysRegReq(regType, retOpd, i);
                coreInterface.sys_data_ret[i]->srcs.push_back(retOpd);
            }
        }
        coreInterface.sys_rd_req[i]->Reset();
    }
    // Write system register
    for (uint32_t i = 0; i < coreInterface.sys_wr_req.size(); i++) {
        if (!coreInterface.sys_wr_req[i]->vld) {
            continue;
        }
        setSysreg(static_cast<SystemReg>(coreInterface.sys_wr_req[i]->sysID),
            coreInterface.sys_wr_req[i]->dsts[DST0_IDX]->data);
        if (!iex[SCALAR_IEX]->ssrsetOrderQ.Empty()) {
            SimInst inst = iex[SCALAR_IEX]->ssrsetOrderQ.Front();
            if (coreInterface.sys_wr_req[i]->bid == inst->bid) {
                iex[SCALAR_IEX]->ssrsetOrderQ.Pop();
            }
        }
        coreInterface.sys_wr_req[i]->Reset();
    }
}

void Core::illegalConfCheck() {
    typedef  tuple<uint64_t, uint64_t, uint64_t> cmpRule;
    uint64_t max_int = std::numeric_limits<long long>::max();
    BCtrlConfig bctrlConf = bctrl->configs;
    BFUConfig bfuConf = bctrl->blockFetchUnit.bfu->cfg;
    L1Config l1Conf = memIntf[static_cast<int>(LSUType::SCALAR_LSU)]->l1Cache->configs;
    LSUConfig lsuConf = memIntf[static_cast<int>(LSUType::SCALAR_LSU)]->configs;
    map<string, cmpRule> cmpMap = {
        {"bhc_pf_nostd < bhc_nostd", make_tuple(bfuConf.bhc_pf_nostd + 1, bfuConf.bhc_nostd, max_int)},
        {"hwpf_max_pending_req >= bhc_pf_nostd", make_tuple(bfuConf.hwpf_max_pending_req, bfuConf.bhc_pf_nostd, max_int)},
        {"hwpf_max_pending_req <= bhc_pf_nostd ", make_tuple(bfuConf.bhc_pf_nostd, bfuConf.hwpf_max_pending_req, max_int)},
        {"ubtb size", make_tuple(bfuConf.ubtb_min_count, bfuConf.ubtb_nset * bfuConf.ubtb_nway, bfuConf.ubtb_max_count)},
        {"bhc size", make_tuple(bfuConf.bhc_min_size, bfuConf.bhc_nset * bfuConf.bhc_nway * bfuConf.bhc_way_size, bfuConf.bhc_max_size)},
        {"dcache size", make_tuple(l1Conf.dcache_min_size, l1Conf.dcache_nset * l1Conf.dcache_nway * l1Conf.dcache_way_size,
                                   l1Conf.dcache_max_size)},
    };

    for (const auto& it : cmpMap) {
        if (!((get<0>(it.second) <= get<1>(it.second)) && (get<1>(it.second) <= get<2>(it.second)))) {
            cout<<it.first<<"  dataMin: "<<get<0>(it.second)<<"  dataMid: "<<get<1>(it.second)<<"  dataMax: "<<get<2>(it.second)<<endl;
            ASSERT(0 && "Invalid parameter combination.");
        }
    }
}

void Core::PrintCoreStatus(uint32_t stid)
{
    (void)stid;
    LoggerManager::GetManager().ResetLevel(LoggerLevel::DETAIL);

    // Print BlockROB
    for (uint32_t i = 0; i < configs.scalar_smt_thread; ++i) {
        std::string oldestInfo = bctrl->blockROB.PrintLastStatusAndReturnOldest(i);
        cerr << oldestInfo << std::endl;
    }
    // Print pe
    for (uint32_t i = 0; i < peArray.size(); i++) {
        peArray[i]->PrintROBStatus();
    }

    // Print vector
    vectorTop->Dump();
    vecliteTop->Dump();

    // Print shareTile
    if (bctrl->configs.shared_tile_enable) {
        for (uint32_t i = 0; i < configs.scalar_smt_thread; ++i) {
            bctrl->blockIssueQueueUnit.sharedTileStatus[i].Dump();
        }
    }
    // Print BIFU
    bctrl->blockFetchUnit.bfu->Dump();
}

void Core::InitSwimLogger()
{
    auto logger = sim->GetSwimLogger();
    if (logger == nullptr) {
        return;
    }
    logger->config.overrideDefaultConfig(sim->getCfgs());
    logger->SetProcessName("CoreTop", CORE_TOP_MACHINE_ID, 0);
}

void Core::CountStats()
{
    bool allCoresIdle = false;
    bool vectorBusy = false;
    bool cubeBusy = false;
    bool tmaBusy = false;
    uint32_t vecBussyCount = vectorTop->BussyCount();
    if (vectorTop->BussyCount() != 0) {
        vecBusyCycle += vecBussyCount;
        allCoresIdle = true;
        vectorBusy = true;
    }
    vecBussyCount = vecliteTop->BussyCount();
    if (vecliteTop->BussyCount() != 0) {
        vecBusyCycle += vecBussyCount;
        allCoresIdle = true;
        vectorBusy = true;
    }

    ASSERT(mtcCores.size() == 0);
    for (auto &mtcCore : mtcCores) {
        mtcBusyCycle += mtcCore->IsMtcBusy() ? 1 : 0;
        allCoresIdle = allCoresIdle || mtcCore->IsMtcBusy();
    }
    for (auto& cubeCore : cubeCores) {
        cubeBusyCycle += cubeCore->IsCubeBusy() ? 1 : 0;
        allCoresIdle = allCoresIdle || cubeCore->IsCubeBusy();
        cubeBusy = cubeBusy || cubeCore->IsCubeBusy();
    }
    for (auto& tileBridge : tileBridges) {
        tmaBusyCycle += tileBridge->IsBusy() ? 1 : 0;
        tloadBusyCycle += tileBridge->IsTloadBusy() ? 1 : 0;
        writeBlockedByRingCycle += tileBridge->IsWriteRingBlocked() ? 1 : 0;
        tstoreBusyCycle += tileBridge->IsTstoreBusy() ? 1 : 0;
        tmovBusyCycle += tileBridge->IsTmovBusy() ? 1 : 0;
        mgatherBusyCycle += tileBridge->IsMgatherBusy() ? 1 : 0;
        mscatterBusyCycle += tileBridge->IsMscatterBusy() ? 1 : 0;
        vgatherBusyCycle += tileBridge->IsVgatherBusy() ? 1 : 0;
        vscatterBusyCycle += tileBridge->IsVscatterBusy() ? 1 : 0;
        if (!tileBridge->IsBusy()) {
            ASSERT(!tileBridge->IsTloadBusy() &&
                   !tileBridge->IsTstoreBusy() &&
                   !tileBridge->IsTmovBusy());
        }
        allCoresIdle = allCoresIdle || tileBridge->IsBusy();
        tmaBusy = tmaBusy || tileBridge->IsBusy();
    }
    if (!allCoresIdle) {
        tileIdleCycle++;
    } else {
        tileopTotalCycle++;
    }
    if (vectorBusy && cubeBusy) {
        vectorCubeConcurrentCycle++;
    }
    if (vectorBusy || cubeBusy) {
        vectorCubeActiveCycle++;
    }
    if (cubeBusy && tmaBusy) {
        cubeTMAConcurrentCycle++;
    }
    if (cubeBusy || tmaBusy) {
        cubeTMAActiveCycle++;
    }
}

bool Core::IsVectorIex(MachineType type) const
{
    return (type == MachineType::VIEX);
}

bool Core::IsMtcIex(MachineType type) const
{
    return (type == MachineType::MIEX);
}

bool Core::IsVectorIex(ExecEngineTyp type) const
{
    uint32_t iexNum = static_cast<uint32_t>(ExecEngineTyp::IEX_NUM) + configs.simtPeCount;
    return (type == ExecEngineTyp::SIMT_IEX) ||
        (static_cast<uint32_t>(type) >= static_cast<uint32_t>(ExecEngineTyp::IEX_NUM)
        && static_cast<uint32_t>(type) < iexNum);
}

Core::~Core()
{
    ReleaseVecPtr(coreInterface.lsu_pe_rslv_array);
    ReleaseVecPtr(coreInterface.pe_flush_rpt_array);
    ReleaseVecPtr(coreInterface.pe_rf_wr_q);
    DeletePtr(coreInterface.bcc_flush_rpt_q);
    ReleaseVecPtr(coreInterface.iex_flush_rpt_q);
    DeletePtr(coreInterface.rtable_release_q);
    DeletePtr(coreInterface.block_rtable_retire_q);

    for (auto& v_que : coreInterface.pe_iex_cmd_array) {
        ReleaseVecPtr(v_que);
    }
    for (auto& v_que : coreInterface.pe_iex_alu_array) {
        ReleaseVecPtr(v_que);
    }
    for (auto& v_que : coreInterface.pe_iex_lda_array) {
        ReleaseVecPtr(v_que);
    }
    for (auto& v_que : coreInterface.pe_iex_sta_array) {
        ReleaseVecPtr(v_que);
    }
    for (auto& v_que : coreInterface.pe_iex_std_array) {
        ReleaseVecPtr(v_que);
    }
    for (auto& v_que : coreInterface.pe_iex_bru_array) {
        ReleaseVecPtr(v_que);
    }
    for (auto& v_que : coreInterface.pe_iex_rd_array) {
        ReleaseVecPtr(v_que);
    }
    for (auto& v_que : coreInterface.pe_iex_vec_agu_array) {
        ReleaseVecPtr(v_que);
    }
    for (auto& v_que : coreInterface.pe_iex_vec_scalar_array) {
        ReleaseVecPtr(v_que);
    }

    uint32_t peCount = configs.stdPeCount + configs.simtPeCount + configs.memPeCount;
    uint32_t iexCount = static_cast<uint32_t>(ExecEngineTyp::IEX_NUM) + configs.simtPeCount;
    for (uint64_t iexNum = 0; iexNum < iexCount; ++iexNum) {
        ReleaseVecPtr(coreInterface.pe_iex_cmd_array[iexNum]);
        ReleaseVecPtr(coreInterface.pe_iex_alu_array[iexNum]);
        ReleaseVecPtr(coreInterface.pe_iex_lda_array[iexNum]);
        ReleaseVecPtr(coreInterface.pe_iex_sc_lda_array[iexNum]);
        ReleaseVecPtr(coreInterface.pe_iex_sta_array[iexNum]);
        ReleaseVecPtr(coreInterface.pe_iex_std_array[iexNum]);
        ReleaseVecPtr(coreInterface.pe_iex_bru_array[iexNum]);
        ReleaseVecPtr(coreInterface.pe_iex_rd_array[iexNum]);
        ReleaseVecPtr(coreInterface.pe_iex_vec_agu_array[iexNum]);
        ReleaseVecPtr(coreInterface.pe_iex_vec_scalar_array[iexNum]);
        for (uint64_t i = 0; i < peCount; i++) {
            ReleaseVecPtr(coreInterface.iex_pe_rslv_array[iexNum][i]);
        }
    }
}

bool Core::IsFullBP()
{
    return configs.bp_mode == static_cast<uint64_t>(BP_Mode::FULL_BP);
}

bool Core::IsPerfectBP()
{
    return configs.bp_mode == static_cast<uint64_t>(BP_Mode::PERFECT_BP);
}

IDBus Core::GetScalarOldestInfo(ROBID &oldestId)
{
    IDBus oldCmt;
    uint32_t start = 0;
    uint32_t size = configs.stdPeCount;
    uint32_t end = start + size;
    for (uint32_t pe = start; pe < end; ++pe) {
        IDBus cmtBus = peArray[pe]->GetRetireID(0);
        if (!cmtBus.vld || cmtBus.bid != oldestId) {
            continue;
        }

        oldCmt = (!oldCmt.vld || LessEqual(cmtBus.bid, cmtBus.lsID, oldCmt.bid, oldCmt.lsID)) ? cmtBus : oldCmt;
    }

    oldCmt.vld = true;
    oldCmt.bid = oldestId;
    return oldCmt;
}

IDBus Core::GetSimtOldestInfo()
{
    IDBus oldCmt;
    uint32_t start = configs.stdPeCount;
    uint32_t size = configs.simtPeCount;

    uint32_t end = start + size;
    for (uint32_t pe = start; pe < end; ++pe) {
        IDBus cmtBus = peArray[pe]->GetRetireID();
        if (!cmtBus.vld) {
            continue;
        }
        oldCmt = (!oldCmt.vld || LessEqual(cmtBus.bid, cmtBus.gid, cmtBus.lsID,
            oldCmt.bid, oldCmt.gid, oldCmt.lsID)) ? cmtBus : oldCmt;
    }

    return oldCmt;
}

uint32_t Core::GetScalarPEDecWidth() const
{
    return bctrl->scalarPE->configs.decodeWidth;
}

uint32_t Core::GetVecPEDecWidth() const
{
    return vectorTop->GetPE(0)->configs.decodeWidth;
}


} // namespace JCore
