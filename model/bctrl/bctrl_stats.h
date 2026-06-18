#pragma once

#include <map>
#include <vector>
#include <cstdint>
#include <unordered_map>

#include "../isa/ISACommon/TileOpManager.h"
#include "DataStruct/base_stats.h"

namespace JCore {

const size_t TAG_NTABLE = 5;

struct DstInfo {
    uint64_t pc = -1U;
    uint64_t tgt = -1U;
    uint64_t ghr = -1U;
    uint32_t num = 0;
};

struct HashPCInfo {
    std::map<uint64_t, DstInfo> pcMap;
    uint32_t confilictNum = 0;
    uint64_t allocNum = 0;
};

class BCtrlStats : public NS_PLAT::BaseStats {
public:
    virtual ~BCtrlStats() {}
    /** common top-level counters */
    uint64_t cycles; // @bctrl.Work
    uint64_t binsts; // @brob.commitBlock, equals to slots_retired
    uint64_t minsts; // @brob.commitBlock, equals to minst_retired
    uint64_t slot_minst_retired; // @brob.commitBlock, equals to minst_retired, except CT insts and bstart
    uint64_t moveInsts;
    uint64_t total_brob_size; // @brob.Work
    uint64_t total_ostd_blocks; // @brob.GatherStats
    uint64_t total_ostd_cube_blocks; // @brob.GatherStats
    uint64_t total_ostd_vector_blocks; // @brob.GatherStats
    uint64_t total_ostd_tload; // @brob.GatherStats
    uint64_t total_ostd_tstore; // @brob.GatherStats

    std::vector<uint64_t> smtBrobSizeArray; // @smt.brob.Work
    std::vector<uint64_t> smtOstdBlocksArray; // @smt.brob.GatherStats
    std::vector<uint64_t> smtOstdCubeBlocksArray; // @smt.brob.GatherStats
    std::vector<uint64_t> smtOstdVectorBlocksArray; // @smt.brob.GatherStats
    std::vector<uint64_t> smtOstdTloadArray; // @smt.brob.GatherStats
    std::vector<uint64_t> smtOstdTstoreArray; // @smt.brob.GatherStats

    std::vector<uint64_t> issued_cube_blocks_histogram; // @brob.GatherStats
    std::vector<uint64_t> issued_vector_blocks_histogram; // @brob.GatherStats
    std::vector<uint64_t> issued_tload_histogram; // @brob.GatherStats
    std::vector<uint64_t> issued_tstore_histogram; // @brob.GatherStats
    uint64_t fetchMinstCnt; // @BRQ::push, equals to the count of minsts fetched by BFU
    uint64_t fetchHdrCnt;
    uint64_t exist_minst_cycle;

    // the number of minsts of different type
    std::unordered_map<uint64_t, uint64_t> groupInstMap;

    /* top-down
     * slots_rob_allocated = slotsBrobAllocated + slotsPerobAllocated
     * slots_total = slots_retired + slots_bad_spec + frontend_bound + slots_be_bound
     */
    uint64_t slots_total; // @bctrl.Work, increment by bctrl_bandwidth every cycle
    uint64_t slotsBrobAllocated; // the number of header allocated to brob
    uint64_t slotsPerobAllocated; // the number of minst allocated to perob,except CT insts and auto-gen insts
    uint64_t slots_fe_bubble; // increment slots_fe_bubble at the first empty header
    uint64_t slots_flush_recover_bubble; // fe_bubble due to flush

    uint64_t slots_decode_stall; // the bandwidth loss due to DCtop stall
    uint64_t slots_brename_stall; // the bandwidth loss due to brename stall
    uint64_t slots_brob_stall; // the bandwidth loss due to brob stall
    uint64_t slots_sysblock_stall; // the bandwidth loss due to SYS block stall
    uint64_t brename_set_stall; // get reach max lookup num
    uint64_t brename_get_stall; // set reach max allocate num
    uint64_t bisqFullStall;
    uint64_t tileregSizeFullStall;

    std::vector<uint64_t> smtSlotsDecodeStallArray;
    std::vector<uint64_t> smtSlotsSysblockStallArray;
    std::vector<uint64_t> smtSlotsBrobStallArray;
    std::vector<uint64_t> smtSlotsBrenameStallArray;
    std::vector<uint64_t> smtBrenameSetStallArray;
    std::vector<uint64_t> smtBrenameGetStallArray;
    std::vector<uint64_t> smtBisqFullStallArray;
    std::vector<uint64_t> smtTileregSizeFullStallArray;

    uint64_t cycles_fe_bubble;  // @decodeBlockHeader
    uint64_t slots_clear_bubble; // not yet supported
    uint64_t clear_retired; // not yet supported

    /** BFU */
    // BHC and BTLB aaccelss
    uint64_t bhc_aaccelss;
    uint64_t bhc_miss;
    uint64_t bhc_total_lat;
    uint64_t btlb_aaccelss;
    uint64_t btlb_miss;
    uint64_t ubtb_aaccelss;
    uint64_t ubtb_miss;
    uint64_t pbtb_aaccelss;
    uint64_t pbtb_miss;
    // pred statistics
    uint64_t forward_pred;
    uint64_t f1_two_pred;
    uint64_t f1_pred_taken;
    uint64_t f1_use_local;
    uint64_t f4_bhq_non_fb;
    uint64_t f4_bhq_one_fb;
    uint64_t f4_bhq_two_fb;
    uint64_t f4_bhq_multi_fb;
    uint64_t f4_working;
    uint64_t f4_pred_taken;
    uint64_t f4_use_local;
    uint64_t local_occupied_cyc;
    uint64_t local_occupied_num;
    uint64_t fetch_size_after_taken;
    uint64_t fetch_fall_throgh;
    // Loop predictor
    uint64_t loop_trained;
    uint64_t loop_predicted;
    uint64_t loop_mispred;
    // Loop Buffer
    uint64_t lb_hit;
    uint64_t lb_insert;
    uint64_t lb_redirect;
    uint64_t lb_sendHdr;
    uint64_t lb_commitHdrCnt;
    // tage
    uint64_t tageAllocSuaccelss;
    uint64_t tageAllocFail;
    uint64_t histConfNum;
    std::map<uint64_t, HashPCInfo> histHashMap;
    uint64_t tageConfNum;
    std::map<uint64_t, HashPCInfo> tageHashMap[TAG_NTABLE];

    // the number of retired header/minst based on Block-Type
    uint64_t rtiredHdrStd;
    uint64_t rtiredHdrSys;
    uint64_t rtiredHdrFp;
    uint64_t rtiredHdrSimt;
    uint64_t rtiredCubeBlockCnt;
    uint64_t rtiredVcallBlockCnt;
    uint64_t rtiredTmplBlockCnt;
    uint64_t rtiredMcallBlockCnt;
    uint64_t rtiredTMABlockCnt;
    uint64_t rtiredHdrCt;
    uint64_t rtiredHdrOther;

    uint64_t rtiredHdrCtMset;
    uint64_t rtiredHdrCtMcopy;
    uint64_t rtiredHdrCtFentry;
    uint64_t rtiredHdrCtFexit;
    uint64_t rtiredHdrCtFret;

    uint64_t rtiredHdrStdMinsts;
    uint64_t rtiredHdrSysMinsts;
    uint64_t rtiredHdrFpMinsts;
    uint64_t rtiredHdrSimtMinsts;
    uint64_t rtiredHdrCtMinsts;
    uint64_t rtiredHdrOtherMinsts;

    uint64_t rtiredHdrCtMsetMinsts;
    uint64_t rtiredHdrCtMcopyMinsts;
    uint64_t rtiredHdrCtFentryMinsts;
    uint64_t rtiredHdrCtFexitMinsts;
    uint64_t rtiredHdrCtFretMinsts;

    // the number of retired header/minst based on Branch-Type
    uint64_t retired_header;
    uint64_t retired_tileop = 0;
    uint64_t retired_tileop_vec = 0;
    uint64_t retiredTileopVet = 0;
    uint64_t retired_tileop_cube = 0;
    uint64_t retired_tileop_mtc = 0;
    uint64_t retired_tileop_tma = 0;

    uint64_t retired_tileop_cycles = 0;
    uint64_t retired_tileop_vec_cycles = 0;
    uint64_t retiredTileopVetCycles = 0;
    uint64_t retired_tileop_cube_cycles = 0;
    uint64_t retired_tileop_mtc_cycles = 0;
    uint64_t retired_tileop_tma_cycles = 0;

    uint64_t retired_fb;
    uint64_t retired_mispred;
    uint64_t retired_mispred_cond;
    uint64_t retired_mispred_target;
    uint64_t retired_mispred_direction;
    uint64_t retired_header_total_lat;
    uint64_t retired_misp_first_after_nuke;

    uint64_t rtiredHdrFall;     // FALL
    uint64_t rtiredHdrCond;     // CONDITIONAL
    uint64_t rtiredHdrDir;      // DIRECT
    uint64_t rtiredHdrInDir;    // INDIRECT
    uint64_t rtiredHdrCall;     // CALL
    uint64_t rtiredHdrInCall;   // INDCALL
    uint64_t rtiredHdrRet;      // RETURN
    uint64_t rtiredHdrEcall;    // ECALL
    uint64_t rtiredHdrOthBr;

    uint64_t rtiredHdrFallMiss;  // resolve mispredict count
    uint64_t rtiredHdrCondMiss;
    uint64_t rtiredHdrDirMiss;
    uint64_t rtiredHdrInDirMiss;
    uint64_t rtiredHdrCallMiss;
    uint64_t rtiredHdrInCallMiss;
    uint64_t rtiredHdrRetMiss;
    uint64_t rtiredHdrEcallMiss;
    uint64_t rtiredHdrOthBrMiss;

    uint64_t rtiredHdrFallMinst;  // minst count
    uint64_t rtiredHdrCondMinst;
    uint64_t rtiredHdrDirMinst;
    uint64_t rtiredHdrInDirMinst;
    uint64_t rtiredHdrCallMinst;
    uint64_t rtiredHdrInCallMinst;
    uint64_t rtiredHdrRetMinst;
    uint64_t rtiredHdrEcallMinst;
    uint64_t rtiredHdrOthBrMinst;

    std::vector<uint64_t> tRegSrcCnt;
    std::vector<uint64_t> uRegSrcCnt;
    std::vector<uint64_t> vtRegSrcCnt;
    std::vector<uint64_t> vuRegSrcCnt;
    std::vector<uint64_t> vmRegSrcCnt;
    std::vector<uint64_t> vnRegSrcCnt;
    std::vector<uint64_t> tRegSrcRobDist;
    std::vector<uint64_t> uRegSrcRobDist;
    std::vector<uint64_t> vtRegSrcRobDist;
    std::vector<uint64_t> vuRegSrcRobDist;
    std::vector<uint64_t> vmRegSrcRobDist;
    std::vector<uint64_t> vnRegSrcRobDist;

    // flush & BFU stall
    uint64_t intra_flush;
    uint64_t intra_flush_global;
    uint64_t intra_flush_local_end;
    uint64_t intra_flush_local_incond;
    uint64_t intra_flush_local_call;
    uint64_t fb1_intra_flush;
    uint64_t fb2_intra_flush;
    uint64_t forward_flush;
    uint64_t intra_flush_ibtb;
    uint64_t intra_flush_ras;
    uint64_t intra_flush_ubtb_miss;
    uint64_t intra_flush_ubtb_miss_uncond;
    uint64_t intra_flush_ubtb_nt;
    uint64_t intra_flush_ubtb_tk;
    uint64_t intra_flush_ubtb_tk_pos;
    uint64_t intra_flush_ubtb_tk_dir;
    uint64_t intra_flush_ubtb_tk_tgt;
    uint64_t intra_flush_ubtb_hit_bim_only;
    uint64_t bru_flush;
    uint64_t ooo_flush;
    std::map<uint64_t, uint64_t> intraFlushCntMap;

    uint64_t backend_stall;
    uint64_t brq_stall;
    uint64_t bhc_stall_global;
    uint64_t bhc_stall_local;
    uint64_t local_stall_global;
    uint64_t btlb_stall;
    uint64_t ras_stall;

    uint64_t bhq_clear;
    uint64_t bhq_empty;
    uint64_t bhq_get_header;

    uint64_t bpc_discontinue_count;
    uint64_t bpc_mispredict_count = 0;
    uint64_t first_taken_mis_pred = 0;
    uint64_t second_taken_mis_pred = 0;
    uint64_t forward_mis_pred = 0;
    uint64_t total_p1_count = 0;
    uint64_t pe_stall_cycle = 0;
    std::map<uint64_t, uint64_t> misBpcCntMap;
    std::map<uint64_t, uint64_t> misBrBpcCntMap;

    /** decode, rename and dispatch stage */
    uint64_t blk_dispatch_cnt;
    uint64_t rr_dispatch_cnt;
    uint64_t wl_dispatch_cnt;
    uint64_t tc_dispatch_cnt;
    uint64_t bmdb_dispatch_cnt;
    /* \brief pysical register */
    std::vector<uint64_t> tileregTagUsage;
    std::vector<std::vector<uint64_t>> tileregTagUsageHistogram;

    uint64_t tileRenAllocSize[TILE_TYPE_COUNT] = {0};
    uint64_t tileRenMaxAllocSize[TILE_TYPE_COUNT] = {0};
    uint64_t tileMapQOcupiedSize[TILE_TYPE_COUNT] = {0};
    uint64_t tileMaxHandSize[TILE_TYPE_COUNT] = {0};
    uint64_t tileRenStallCyc = 0;
    uint64_t vrfRenStallCyc = 0;

    uint64_t efficient = 0;
    uint64_t bccBound = 0;
    uint64_t bccBoundEfficient = 0;
    uint64_t bccBoundFrontend = 0;
    uint64_t bccBoundBackend = 0;
    uint64_t bccBoundBackendRobStall = 0;
    uint64_t bccBoundBackendRegStall = 0;
    uint64_t bccBoundBackendIqStall = 0;
    uint64_t bccBoundBackendMemBound = 0;
    uint64_t peBound = 0;
    uint64_t peBoundCube = 0;
    uint64_t peBoundCubeBiqStall = 0;
    uint64_t peBoundCubeBrobStall = 0;
    uint64_t peBoundCubeTileTagStall = 0;
    uint64_t peBoundVec = 0;
    uint64_t peBoundVecBiqStall = 0;
    uint64_t peBoundVecBrobStall = 0;
    uint64_t peBoundVecTileTagStall = 0;
    uint64_t peBoundTma = 0;
    uint64_t peBoundTmaBiqStall = 0;
    uint64_t peBoundTmaBrobStall = 0;
    uint64_t peBoundTmaTileTagStall = 0;
    uint64_t peBoundTau = 0;
    uint64_t peBoundTauBiqStall = 0;
    uint64_t peBoundTauBrobStall = 0;
    uint64_t peBoundTauTileTagStall = 0;

    using BaseStats::BaseStats;
    using BaseStats::Reset;
    using BaseStats::report;
    void Reset();
    void report();
    void ReportTopDownInst();
    void ReportBCCInst();
    void ReportBccStats();
    void topdown();
    void ReportTageBankConflict(size_t t);
    void SetEfficient(uint64_t cycle = 1);
    uint64_t GetEfficient() const;
    void SetBccBound(uint64_t cycle = 1);
    uint64_t GetBccBound() const;
    void SetBccBoundEfficient(uint64_t cycle = 1);
    uint64_t GetBccBoundEfficient() const;
    void SetBccBoundFrontend(uint64_t cycle = 1);
    uint64_t GetBccBoundFrontend() const;
    void SetBccBoundBackend(uint64_t cycle = 1);
    uint64_t GetBccBoundBackend() const;
    void SetBccBoundBackendRobStall(uint64_t cycle = 1);
    void ReduceBccBoundBackendRobStall(uint64_t cycle = 1);
    uint64_t GetBccBoundBackendRobStall() const;
    void SetBccBoundBackendRegStall(uint64_t cycle = 1);
    void ReduceBccBoundBackendRegStall(uint64_t cycle = 1);
    uint64_t GetBccBoundBackendRegStall() const;
    void SetBccBoundBackendIqStall(uint64_t cycle = 1);
    void ReduceBccBoundBackendIqStall(uint64_t cycle = 1);
    uint64_t GetBccBoundBackendIqStall() const;
    void SetBccBoundBackendMemBound(uint64_t cycle = 1);
    void ReduceBccBoundBackendMemBound(uint64_t cycle = 1);
    uint64_t GetBccBoundBackendMemBound() const;
    void SetPeBound(uint64_t cycle = 1);
    uint64_t GetPeBound() const;
    void SetPeBoundCube(uint64_t cycle = 1);
    uint64_t GetPeBoundCube() const;
    void SetPeBoundCubeBrobStall(uint64_t cycle = 1);
    uint64_t GetPeBoundCubeBrobStall() const;
    uint64_t GetPeBoundCubeBiqStall() const;
    void SetPeBoundCubeBiqStall(uint64_t cycle = 1);
    void SetPeBoundCubeTileTagStall(uint64_t cycle = 1);
    uint64_t GetPeBoundCubeTileTagStall() const;
    void SetPeBoundVec(uint64_t cycle = 1);
    uint64_t GetPeBoundVec() const;
    void SetPeBoundVecBrobStall(uint64_t cycle = 1);
    uint64_t GetPeBoundVecBrobStall() const;
    uint64_t GetPeBoundVecBiqStall() const;
    void SetPeBoundVecBiqStall(uint64_t cycle = 1);
    void SetPeBoundVecTileTagStall(uint64_t cycle = 1);
    uint64_t GetPeBoundVecTileTagStall() const;
    void SetPeBoundTma(uint64_t cycle = 1);
    uint64_t GetPeBoundTma() const;
    void SetPeBoundTmaBrobStall(uint64_t cycle = 1);
    uint64_t GetPeBoundTmaBrobStall() const;
    uint64_t GetPeBoundTmaBiqStall() const;
    void SetPeBoundTmaBiqStall(uint64_t cycle = 1);
    void SetPeBoundTmaTileTagStall(uint64_t cycle = 1);
    uint64_t GetPeBoundTmaTileTagStall() const;
    void SetPeBoundTau(uint64_t cycle = 1);
    uint64_t GetPeBoundTau() const;
    void SetPeBoundTauBrobStall(uint64_t cycle = 1);
    uint64_t GetPeBoundTauBrobStall() const;
    uint64_t GetPeBoundTauBiqStall() const;
    void SetPeBoundTauBiqStall(uint64_t cycle = 1);
    void SetPeBoundTauTileTagStall(uint64_t cycle = 1);
    uint64_t GetPeBoundTauTileTagStall() const;
};

} // namespace JCore
