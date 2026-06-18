#pragma once

#include <map>
#include <memory>
#include <vector>

#include "../configs/iex_config.h"
#include "DataStruct/base_stats.h"
#include "ModelCommon/ModelEnumDefines.h"
#include "utils/util.h"

namespace JCore {

using namespace std;

struct VAluStats {
    uint64_t issue0 = 0;
    uint64_t issue1 = 0;
    uint64_t issue2 = 0;
    uint64_t issue3 = 0;
    uint64_t issue4 = 0;

    uint64_t issue1VInt = 0;
    uint64_t issue2VInt = 0;
    uint64_t issue3VInt = 0;
    uint64_t issue4VInt = 0;

    uint64_t issue1VCvt = 0;
    uint64_t issue2VCvt = 0;
    uint64_t issue3VCvt = 0;
    uint64_t issue4VCvt = 0;

    uint64_t issue1VFp = 0;
    uint64_t issue2VFp = 0;
    uint64_t issue3VFp = 0;
    uint64_t issue4VFp = 0;

    uint64_t issue1Shf = 0;
    uint64_t issue2Shf = 0;
    uint64_t issue3Shf = 0;
    uint64_t issue4Shf = 0;

    uint64_t issue1DivSqrt = 0;
    uint64_t issue2DivSqrt = 0;
    uint64_t issue3DivSqrt = 0;
    uint64_t issue4DivSqrt = 0;

    uint64_t readyNotPickLat0 = 0;
    uint64_t readyNotPickLat1 = 0;
    uint64_t readyNotPickLat2 = 0;
    uint64_t readyNotPickLat3 = 0;
    uint64_t readyNotPickLat4 = 0;
};

enum class IssueBlockReason {
    IQ_EMPTY = 0,
    CANT_ISSUE_NOT_READY,
    CANT_ISSUE_BUT_READY,
    CMD_BIOT_NOT_SELECTED,
    SSRSET_NOT_OLDEST,
    SYS_STATE_STALL,
    LD_NOT_IN_WINDOW,
    ST_NOT_IN_WINDOW,
    AGE_QUEUE_LDQ_LIMIT,
    NOT_PREV_ISSUED,
    PIPE_STALL,
    CANT_ISSUE_EXEC_UNIT,
    CANCEL_ISSUE_EXEC_UNIT,
    CANCEL_ISSUE_ALU_RD_PORT,
    CANCEL_ISSUE_AGU_RD_PORT,
    NUM,
    UNDEF = 0xff
};

struct IQStats {
    // Statistics for each queue
    vector<uint64_t> depth;
    vector<uint64_t> depth_25;
    vector<uint64_t> depth_50;
    vector<uint64_t> depth_75;
    vector<uint64_t> depth_100;
    vector<uint64_t> pick_stall_count;
    vector<uint64_t> insert_stall_count;
    // Statistics total
    uint64_t total_inst_num = 0;
    uint64_t total_wait = 0;
    uint64_t all_pick_stall = 0;
    uint64_t all_insert_stall = 0;
    uint64_t slots_stall = 0;
    std::shared_ptr<VAluStats> valuStats = nullptr;
    IQStats() {}
    ~IQStats()
    {
        ReleaseVecEle(depth);
        ReleaseVecEle(depth_25);
        ReleaseVecEle(depth_50);
        ReleaseVecEle(depth_75);
        ReleaseVecEle(depth_100);
        ReleaseVecEle(pick_stall_count);
        ReleaseVecEle(insert_stall_count);
    }
};

class IEXStats : public NS_PLAT::BaseStats {
public:
    IEXStats() {};
    virtual ~IEXStats();
    ExecEngineTyp id = SCALAR_IEX;
    uint64_t cycles = 0;
    uint64_t stall_cycle = 0;
    // cmd pipe
    IQStats cmd_iq;
    // alu pipe
    IQStats alu_iq;
    // vector agu pipe
    IQStats agu_iq;
    // scalar lda pipe
    IQStats lda_iq;
    // scalper lda pipe
    IQStats lda_sc_iq;
    // sta pipe
    IQStats sta_iq;
    // std pipe
    IQStats std_iq;
    // bru pipe
    IQStats bru_iq;
    // Vector scalar pipe
    IQStats sca_iq;

    // Topdown
    uint64_t slots = 0;
    uint64_t slots_idle = 0;
    uint64_t slots_retired = 0;
    uint64_t slots_bandwidth = 0;
    uint64_t slots_empty = 0;
    uint64_t issued_cnt = 0;
    uint64_t exec_stall = 0; // num of cycles when few insts have executed
    uint64_t memstall_anyload = 0; // cycles when exec is stalled and anyload is not completed
    uint64_t memstall_store = 0;
    uint64_t memstall_l1miss = 0;
    uint64_t memstall_l2miss = 0;

    uint64_t ope_s1_inst_cnt = 0;
    uint64_t s1_stack_get_inst = 0;
    uint64_t s1_stack_load_inst = 0;
    uint64_t s1_optimized_inst = 0;
    uint64_t d2_inst_total = 0;
    uint64_t d2_optimized_inst_total = 0;
    uint64_t d2_partial_src_inst_total = 0;
    uint64_t can_be_optimized_inst = 0;
    uint64_t d2_reg_sink_inst = 0;
    uint64_t d2_cisc_sink_inst = 0;
    uint64_t loadResolveCnt = 0;
    uint64_t total_wakeup = 0;
    uint64_t no_wakeup = 0;
    uint64_t total_intercept = 0;
    uint64_t efficient_intercept = 0;
    uint64_t memoryAaccelssConflict = 0;

    uint64_t tile_load_inst_num = 0;
    uint64_t tile_gather_load_inst_num = 0;
    uint64_t tile_gather_stall_cycle = 0;
    uint64_t tile_gather_issue_cycle = 0;
    uint64_t tile_gather_complete_cycle = 0;
    uint64_t tile_continious_load_inst_num = 0;
    uint64_t tile_store_inst_num = 0;
    uint64_t tile_scatter_store_inst_num = 0;
    uint64_t tile_continious_store_inst_num = 0;
    uint64_t memory_load_inst_num = 0;
    uint64_t memory_gather_load_inst_num = 0;
    uint64_t memory_continious_load_inst_num = 0;
    uint64_t memory_store_inst_num = 0;
    uint64_t memory_scatter_store_inst_num = 0;
    uint64_t memory_continious_store_inst_num = 0;
    uint64_t total_mem_load_req_cnt = 0;
    uint64_t total_mem_load_latency = 0;

    uint64_t dispatchStall = 0;

    uint64_t sALUCycles = 0;
    uint64_t vALUCycles = 0;
    uint64_t src1Conflict = 0;
    uint64_t src2Conflict = 0;
    uint64_t src3Conflict = 0;
    // Counter for extended cycles (for rslv bus cflt)
    std::map<uint32_t, uint32_t> sAluExtendCycles;
    std::map<uint32_t, uint32_t> vAluExtendCycles;

    std::map<IssueBlockReason, uint32_t> vectorIssueBlock;
    std::vector<std::vector<uint32_t>> vectorALUPickerIssueCnt;
    std::map<uint32_t, map<uint32_t, uint64_t>> vectorVrfBankRdPortUsedCnt;
    std::vector<uint64_t> vectorRdPortUsedCnt;
    std::vector<std::vector<uint64_t>> vectorPipeSrcRdPortUsedCnt;

    std::map<uint64_t, uint64_t> no_wakeup_ptag_map;
    using BaseStats::BaseStats;
    using BaseStats::Reset;
    using BaseStats::report;
    void ReleaseVec();
    void Reset();
    void report(const std::string& name);
    void reportVecMtc(const std::string& name);
    void Build(const IEXConfig& configs);
    void resetStats();
    void topdown(const std::string& name);
};


} // namespace JCore
