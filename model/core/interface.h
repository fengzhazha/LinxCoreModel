#pragma once

#include "core/Bus.h"
#include "interface/InterfaceCommon.h"
#include "interface/CommitInfo.h"
#include "ModelCommon/SimInstInfo.h"

namespace JCore {

struct CoreInterface {
    // PE-IEX (fengshulin)
    /* \brief Input cmd instructions from each PE rename stage to issue queue */
    std::vector<std::vector<SimQueue<SimInst>*>>       pe_iex_cmd_array;
    /* \brief Input alu instructions from each PE rename stage to issue queue */
    std::vector<std::vector<SimQueue<SimInst>*>>       pe_iex_alu_array;   // 4 PE
    /* \brief Input agu instructions from each PE rename stage to issue queue */
    std::vector<std::vector<SimQueue<SimInst>*>>       pe_iex_lda_array;
    std::vector<std::vector<SimQueue<SimInst>*>>       pe_iex_sc_lda_array;
    /* \brief Input sta instructions from each PE rename stage to issue queue */
    std::vector<std::vector<SimQueue<SimInst>*>>       pe_iex_sta_array;
    /* \brief Input std instructions from each PE rename stage to issue queue */
    std::vector<std::vector<SimQueue<SimInst>*>>       pe_iex_std_array;
    /* \brief Input bru instructions from each PE rename stage to issue queue */
    std::vector<std::vector<SimQueue<SimInst>*>>       pe_iex_bru_array;
    std::vector<std::vector<SimQueue<SimInst>*>>       pe_iex_vec_agu_array;
    std::vector<std::vector<SimQueue<SimInst>*>>       pe_iex_vec_scalar_array;
    std::vector<std::vector<SimQueue<SimInst>*>>       pe_iex_rd_array;

    /* \brief Input agu instructions from each PE rename stage to issue queue */
    std::vector<std::vector<SimQueue<SimInst>*>>       rename_sc_array;
    std::vector<std::vector<SimQueue<SimInst>*>>       sc_iq_array;

    std::vector<SimQueue<RFReqBus>*>                   pe_rf_wr_q;

    // IEX-PE (wuzhiwei)
    /* \brief Resolved instructions array from IEX to each PE */
    std::vector<std::vector<std::vector<SimQueue<PEResolveBus>*>>>  iex_pe_rslv_array;

    // IEX-BlockROB (liulusen)
    std::vector<SimQueue<ResolveBlockBus>>           iex_brob_rslvblk;

    // IEX-LSU (linrengkang)
    /* \brief Load address array from IEX to each LSU */
    std::vector<std::vector<std::vector<SimQueue<MemReqBus>*>>>   iex_lsu_lda_array;
    /* \brief Store address array from IEX to each LSU */
    std::vector<std::vector<std::vector<SimQueue<MemReqBus>*>>>   iex_lsu_sta_array;
    /* \brief Store data array from IEX to each LSU */
    std::vector<std::vector<std::vector<SimQueue<MemReqBus>*>>>   iex_lsu_std_array;

    std::deque<CommitInfo>                           pe_scalper_commit_q;
    std::deque<MemReqBus>                            peScbWrQ;
    std::deque<SimInst>                              iexScalperInstQ;
    // LSU-IEX (linrengkang)
    /* \brief Load return data array from LSU to iex */
    std::vector<std::vector<std::vector<SimQueue<MemReqBus>*>>>   lsu_iex_lret_array;

    // LSU-PE 
    /* \brief Resolved instructions array from LSU to each PE */
    std::vector<SimQueue<MemReqBus>*>           lsu_pe_rslv_array;
    std::vector<SimQueue<LdNonFlushBus>>        load_non_flush_q;
    std::vector<SimQueue<StNonFlushBus>>        store_non_flush_q;

    SimQueue<LdNonFlushBus>                     mtc_load_non_flush_q;
    SimQueue<StNonFlushBus>                     mtc_store_non_flush_q;
    // Related to flush
    /* \brief BCC report request to Flush Control */
    SimQueue<FlushReq>*                         bcc_flush_rpt_q;
    /* \brief IEX report request to Flush Control(IEX-ISQ is filling up with younger blocks) */
    std::vector<SimQueue<FlushReq>*>            iex_flush_rpt_q;
    /* \brief PE report request to Flush Control */
    std::vector<SimQueue<FlushReq>*>            pe_flush_rpt_array;
    /* \brief LSU report request to Flush Control */
    std::vector<SimQueue<FlushReq>*>            lsu_flush_rpt_q;

    // system register-pipe
    /* \brief Data return for sysget */
    std::vector<RFReqBus*>                      sys_data_ret;
    /* \brief Read request for sysget */
    std::vector<RFReqBus*>                      sys_rd_req;
    /* \brief Write request for sysset */
    std::vector<RFReqBus*>                      sys_wr_req;

    /* \brief Ptag release request from block rename and PE to iex-ptag-ready */
    SimQueue<uint32_t>                          *rtable_release_q;
    /* brief Ptag retire request from BROB to iex-ptag-ready */
    SimQueue<ROBID>                             *block_rtable_retire_q;

    std::vector<SimQueue<BlockCommandPtr>>       bccCubeBlockCmdQ;
    SimQueue<BlockCommandPtr>               bccVecBlockCmdQ;
    SimQueue<BlockCommandPtr>               bccMTCBlockCmdQ;
    SimQueue<BlockCommandPtr>               bccTMABlockCmdQ;
    SimQueue<BlockCommandPtr>               bccTAUBlockCmdQ;
    SimQueue<BlockCommandPtr>               cubeBCCWakeupQ;
    SimQueue<BlockCommandPtr>               vecBCCWakeupQ;
    SimQueue<BlockCommandPtr>               mtcBCCWakeupQ;
    SimQueue<BlockCommandPtr>               tmaBCCWakeupQ;
    SimQueue<BlockCommandPtr>               tauBCCWakeupQ;
    SimQueue<BlockCommandPtr>               bccMcallBlockCmdQ;
    SimQueue<BlockCommandPtr>               mcallBCCWakeupQ;
    SimQueue<BlockCommandPtr>               schedulerBCCRslvQ;
    // x-core connections
    std::vector<SimQueue<BCCCubeFlush>>     bCCCubeFlushArray;
    SimQueue<BCCMemLdReq>                   bCCMemLdReqQ;
    SimQueue<MemBCCLdRes>                   bCCMemLdResQ;
    SimQueue<BCCMemSnpRes>                  bCCMemSnpResQ;
    SimQueue<BCCMemStReq>                   bCCMemStReqQ;
    SimQueue<MemBCCStRes>                   bCCMemStResQ;
    std::vector<SimQueue<BCCTTransFlush>>   bCCTTransFlushArray;
    std::vector<SimQueue<BCCVecFlush>>      bCCVecFlushArray;

    std::vector<SimQueue<BCCVecFlush>>      bCCMtcFlushArray;

    std::vector<SimQueue<Credit>>           cubeBCCCreditArray;
    std::vector<SimQueue<CubeTileRegLdReq>> cubeTileRegLdReqArray;
    std::vector<SimQueue<CubeTileRegStReq>> cubeTileRegStReqArray;

    std::vector<SimQueue<TileRegCubeLdRes>> tileRegCubeLdResArray;
    std::vector<SimQueue<TileRegCubeLdRes>>  tileRegCubeWrResArray;
    std::vector<SimQueue<TileRegCubeLdRetry>> tileRegCubeLdRetryArray;
    std::vector<SimQueue<TileRegCubeStRetry>> tileRegCubeStRetryArray;
    std::vector<SimQueue<TileRegTTransLdRes>> tileRegTTransLdResArray;
    std::vector<SimQueue<TileRegTTransLdRes>> tileRegTTransWrResArray;
    std::vector<SimQueue<TileRegTTransLdRetry>> tileRegTTransLdRetryArray;
    std::vector<SimQueue<TileRegTTransStRetry>> tileRegTTransStRetryArray;

    std::vector<SimQueue<TileRegVecLdRes>>  tileRegVecLdResArray;
    std::vector<SimQueue<TileRegVecLdRes>>  tileRegVecWrResArray;
    std::vector<SimQueue<TileRegVecLdRetry>> tileRegVecLdRetryArray;
    std::vector<SimQueue<TileRegVecStRetry>> tileRegVecStRetryArray;

    std::vector<SimQueue<TileRegVecLdRes>>  tileRegMtcLdResArray;
    std::vector<SimQueue<TileRegVecLdRes>>  tileRegMtcWrResArray;
    std::vector<SimQueue<TileRegVecLdRetry>> tileRegMtcLdRetryArray;
    std::vector<SimQueue<TileRegVecStRetry>> tileRegMtcStRetryArray;

    std::vector<SimQueue<TileRegTTransLdRes>>   tileRegBridgeLdResArray;
    std::vector<SimQueue<TileRegTTransLdRes>>   tileRegBridgeWrResArray;
    std::vector<SimQueue<TileRegTTransLdRetry>> tileRegBridgeLdRetryArray;
    std::vector<SimQueue<TileRegTTransStRetry>> tileRegBridgeStRetryArray;

    std::vector<SimQueue<TTransTileRegLdReq>>   bridgeTileRegLdReqArray;
    std::vector<SimQueue<TTransTileRegStReq>>   bridgeTileRegStReqArray;

    std::vector<SimQueue<TTransMemLdReq>>   tTransMemLdReqArray;
    std::vector<SimQueue<MemTTransLdRes>>   tTransMemLdResArray;
    std::vector<SimQueue<TTransMemStReq>>   tTransMemStReqArray;
    std::vector<SimQueue<MemTTransStRes>>   tTransMemStResArray;
    std::vector<SimQueue<TTransTileRegLdReq>> tTransTileRegLdReqArray;
    std::vector<SimQueue<TTransTileRegStReq>> tTransTileRegStReqArray;

    std::vector<SimQueue<Credit>>               vecBCCCreditArray;
    std::vector<SimQueue<VecTileRegLdReq>>      vecTileRegLdReqArray;
    std::vector<SimQueue<VecTileRegStReq>>      vecTileRegStReqArray;
    std::vector<SimQueue<TTransMemLdReq>>       vecCoreMemLdReqArray;
    std::vector<SimQueue<MemTTransLdRes>>       vecCoreMemLdResArray;
    std::vector<SimQueue<TTransMemStReq>>       vecCoreMemStReqArray;
    std::vector<SimQueue<MemTTransStRes>>       vecCoreMemStResArray;

    std::vector<SimQueue<VecTileRegLdReq>>      mtcTileRegLdReqArray;
    std::vector<SimQueue<VecTileRegStReq>>      mtcTileRegStReqArray;
    std::vector<SimQueue<TTransMemLdReq>>       mtcCoreMemLdReqArray;
    std::vector<SimQueue<MemTTransLdRes>>       mtcCoreMemLdResArray;
    std::vector<SimQueue<TTransMemStReq>>       mtcCoreMemStReqArray;
    std::vector<SimQueue<MemTTransStRes>>       mtcCoreMemStResArray;

    /* vectorcore <---> iex */
    std::vector<SimQueue<MemRequest>>           iexVcoreReqQ;
    bool                                        resolveBlock;
    std::vector<SimQueue<MemRequest>>           vcoreIexResQ;
    std::vector<SimQueue<MemRequest>>           vcoreLdIexResQ;
    std::vector<SimQueue<MemRequest>>           vcoreStIexResQ;
    SimQueue<VecTileRegStReq>                   iexScbReqQ;
    SimQueue<TileRegVecLdRes>                   scbIexRspQ;
    SimQueue<MemRequest>                        iexMcoreReqQ;
    bool                                        mtcResolveBlock;
    SimQueue<MemRequest>                        mcoreIexResQ;
    SimQueue<VecTileRegStReq>                   mtciexScbReqQ;
    SimQueue<TileRegVecLdRes>                   mtcscbIexRspQ;
    SimQueue<Credit>                            m_biqBCCCreditQ;
    SimQueue<SimInst>                           cmdIQBISQ;

    SimQueue<TTransMemLdReq>                    lsuMemLdReqQ;
    SimQueue<TTransMemStReq>                    lsuMemStReqQ;
    SimQueue<MemTTransLdRes>                    lsuMemLdResQ;
    SimQueue<MemTTransStRes>                    lsuMemStResQ;
    SimQueue<PtrPacket>                         lsuL2ReqQ;
    SimQueue<PtrPacket>                         l2LsuRspQ;
    SimQueue<GFUMemReq>                         l2SocReqQ;
    SimQueue<GFUMemReq>                         socL2RspQ;
    SimQueue<PtrPacket>                         l1_l2_q;
    SimQueue<PtrPacket>                         l2_l1_q;
    SimQueue<PtrPacket>                         inst_l2_q;
    SimQueue<PtrPacket>                         l2_inst_q;
    SimQueue<GFUMemReq>                         l2_mem_q;
    SimQueue<GFUMemReq>                         mem_l2_q;
    SimQueue<PtrPacket>                         pref_l2_q;
    SimQueue<PtrPacket>                         hpref_l2_q;
    SimQueue<PtrPacket>                         snp_l2_q;
    SimQueue<PtrPrefFB>                         l2_pref_q;

    std::vector<SimQueue<MemReqBus>>             pref_lu_array;
    std::vector<SimQueue<MemReqBus>>             lu_pref_array;
    std::vector<SimQueue<PtrPrefFB>>             feedback_lu_pref_array;
    std::vector<SimQueue<PtrPacket>>             pref_l2_array;
    std::vector<SimQueue<PtrPrefFB>>             l2_pref_array;
    std::vector<SimQueue<PtrPacket>>             snoop_lu_l2_array;
    std::vector<SimQueue<PtrPacket>>             lookup_lu_l2_array;
    std::vector<SimQueue<PtrPacket>>             lookup_l2_l1_array;

    // BCC->LSU
    /* \brief From block-ROB to LSU tile load resolve queue */
    std::vector<SimQueue<IDBus>>                  bccLsuTloadCommitArray;
    /* \brief From block-ROB to LSU tile store resolve queue */
    std::vector<SimQueue<IDBus>>                  bccLsuTstoreCommitArray;

    // LSU->bridge
    /* \brief From lsu to bridge tile load queue */
    std::vector<SimQueue<MemReqBus>>            lsuBridgeTloadArray;
    /* \brief From lsu to bridge tile store queue */
    std::vector<SimQueue<MemReqBus>>            lsuBridgeTstoreArray;

    std::shared_ptr<SimQueue<MemRequest>>       vecBridgeMemReqQ;
    std::shared_ptr<SimQueue<MemRequest>>       bridgeVecLdRetQ;
    std::shared_ptr<SimQueue<MemRequest>>       bridgeVecStRslvQ;

    SimQueue<LocalFreeInfo>                     req_vec_siex_q;
    SimQueue<LocalFreeInfo>                     rsp_siex_vec_q;

    std::vector<SimQueue<LocalFreeInfo>>        rreq_vec_srf_q;
    std::vector<SimQueue<LocalFreeInfo>>        rrsp_srf_vec_q;

    std::vector<SimQueue<LocalFreeInfo>>        wreq_vec_srf_q;
    std::vector<SimQueue<LocalFreeInfo>>        wrsp_srf_vec_q;

    std::vector<SimQueue<LocalFreeInfo>>        data_vec_viex_q;
    std::vector<SimQueue<LocalFreeInfo>>        data_viex_vec_q;

    SimQueue<LocalFreeInfo>                     gsReqVecSiexQ;
    SimQueue<LocalFreeInfo>                     gsRspSiexVecQ;

    std::vector<SimQueue<LocalFreeInfo>>        gsRreqVecSrfQ;
    std::vector<SimQueue<LocalFreeInfo>>        gsRrspSrfVecQ;
    std::vector<SimQueue<LocalFreeInfo>>        gsWreqVecSrfQ;
    std::vector<SimQueue<LocalFreeInfo>>        gsWrspSrfVecQ;
    std::vector<SimQueue<LocalFreeInfo>>        gsDataVecViexQ;
    std::vector<SimQueue<LocalFreeInfo>>        gsDataViexVecQ;

    SimQueue<LocalFreeInfo>                     mtc_req_vec_siex_q;
    SimQueue<LocalFreeInfo>                     mtc_rsp_siex_vec_q;

    SimQueue<LocalFreeInfo>                     mtc_rreq_vec_srf_q;
    SimQueue<LocalFreeInfo>                     mtc_rrsp_srf_vec_q;

    SimQueue<LocalFreeInfo>                     mtc_wreq_vec_srf_q;
    SimQueue<LocalFreeInfo>                     mtc_wrsp_srf_vec_q;

    SimQueue<LocalFreeInfo>                     mtc_data_vec_viex_q;
    SimQueue<LocalFreeInfo>                     mtc_data_viex_vec_q;

    // stash
    SimQueue<BlockCommandPtr>                   stashCmdQ;
    SimQueue<BlockCommandPtr>                   stashCompQ;
    SimQueue<TileOperandPtr>                    stashFreeQ;
    SimQueue<TileOperandPtr>                    stashRetireQ;
    std::vector<SimQueue<BlockCommandPtr>>      stashAllocDoneArray;
    std::vector<SimQueue<BlockCommandPtr>>      stashRslvArray;
    std::vector<SimQueue<TileOperandPtr>>       toClearArray;

    SimQueue<BlockCommandPtr>                   cubeStashCmdQ;
    SimQueue<BlockCommandPtr>                   cubeStashCompQ;
    SimQueue<BlockCommandPtr>                   stashCubeAllocDoneQ;

    SimQueue<BlockCommandPtr>                   bccVecliteBlockCmdQ;
    std::vector<SimQueue<BCCVecFlush>>          bCCVecliteFlushArray;
    SimQueue<BlockCommandPtr>                   vecliteBccWakeupQ;

    SimQueue<BlockCommandPtr>                   vecliteStashCmdQ;
    SimQueue<BlockCommandPtr>                   vecliteStashCompQ;
    std::vector<SimQueue<BlockCommandPtr>>      stashVecliteAllocDoneArray;
    std::vector<SimQueue<BlockCommandPtr>>      stashVecliteRslvArray;
};

} // namespace JCore
