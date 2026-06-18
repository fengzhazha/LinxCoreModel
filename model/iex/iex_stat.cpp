#include "iex/iex_stat.h"
#include "iex/iex_latency.h"
#include "iex/iex_iq.h"

namespace JCore {

void IEXStats::reportVecMtc(const std::string& name)
{
    if (alu_iq.valuStats != nullptr) {
        auto alu = alu_iq.valuStats.get();
        stringstream s;
        s << name << "(VALU ISQ)";
        rpt->ReportTitle(s.str());

        rpt->ReportVal("VALU issue0", alu->issue0);
        rpt->ReportVal("VALU issue1", alu->issue1);
        rpt->ReportVal("VALU issue2", alu->issue2);
        rpt->ReportVal("VALU issue3", alu->issue3);
        rpt->ReportVal("VALU issue4", alu->issue4);

        rpt->ReportVal("issue 1 VINT", alu->issue1VInt);
        rpt->ReportVal("issue 2 VINT", alu->issue2VInt);
        rpt->ReportVal("issue 3 VINT", alu->issue3VInt);
        rpt->ReportVal("issue 4 VINT", alu->issue4VInt);

        rpt->ReportVal("issue 1 VCVT", alu->issue1VCvt);
        rpt->ReportVal("issue 2 VCVT", alu->issue2VCvt);
        rpt->ReportVal("issue 3 VCVT", alu->issue3VCvt);
        rpt->ReportVal("issue 4 VCVT", alu->issue4VCvt);

        rpt->ReportVal("issue 1 VFP", alu->issue1VFp);
        rpt->ReportVal("issue 2 VFP", alu->issue2VFp);
        rpt->ReportVal("issue 3 VFP", alu->issue3VFp);
        rpt->ReportVal("issue 4 VFP", alu->issue4VFp);

        rpt->ReportVal("issue 1 SHF", alu->issue1Shf);
        rpt->ReportVal("issue 2 SHF", alu->issue2Shf);
        rpt->ReportVal("issue 3 SHF", alu->issue3Shf);
        rpt->ReportVal("issue 4 SHF", alu->issue4Shf);

        rpt->ReportVal("issue 1 DIVSQRT", alu->issue1DivSqrt);
        rpt->ReportVal("issue 2 DIVSQRT", alu->issue2DivSqrt);
        rpt->ReportVal("issue 3 DIVSQRT", alu->issue3DivSqrt);
        rpt->ReportVal("issue 4 DIVSQRT", alu->issue4DivSqrt);

        uint64_t totalPick = alu->readyNotPickLat0 + alu->readyNotPickLat1 + alu->readyNotPickLat2 +
            alu->readyNotPickLat3 + alu->readyNotPickLat4;
        rpt->ReportValAndPct("valu pick lat le 5cyc", alu->readyNotPickLat0, totalPick);
        rpt->ReportValAndPct("valu pick lat le 10cyc", alu->readyNotPickLat1, totalPick);
        rpt->ReportValAndPct("valu pick lat le 20cyc", alu->readyNotPickLat2, totalPick);
        rpt->ReportValAndPct("valu pick lat le 30cyc", alu->readyNotPickLat3, totalPick);
        rpt->ReportValAndPct("valu pick lat ge 30cyc", alu->readyNotPickLat4, totalPick);
    }
    rpt->ReportTitle(name + "(IEX)");

    rpt->ReportVal("dispatch stall", dispatchStall);
    rpt->ReportVal("Total Tile Load Execute Insts", tile_load_inst_num);
    rpt->ReportVal("|--Tile gather Load Execute insts", tile_gather_load_inst_num);
    rpt->ReportVal("|--Tile gather Load issue total cycle", tile_gather_issue_cycle);
    rpt->ReportVal("|--Tile gather Load stall total cycle", tile_gather_stall_cycle);
    rpt->ReportVal("|--Tile gather Load complete total cycle", tile_gather_complete_cycle);
    rpt->ReportVal("|--Tile continious Load Execute insts", tile_continious_load_inst_num);
    rpt->ReportVal("Total Tile Store Execute Insts", tile_store_inst_num);
    rpt->ReportVal("|--Tile scatter Store Execute insts", tile_scatter_store_inst_num);
    rpt->ReportVal("|--Tile continious Store Execute insts", tile_continious_store_inst_num);
    rpt->ReportVal("Total Memory Load Execute Insts", memory_load_inst_num);
    rpt->ReportVal("|--Memory gather Load Execute insts", memory_gather_load_inst_num);
    rpt->ReportVal("|--Memory continious Load Execute Insts", memory_continious_load_inst_num);
    rpt->ReportVal("Total Memory Store Execute Insts", memory_store_inst_num);
    rpt->ReportVal("|--Memory scatter Store Execute insts", memory_scatter_store_inst_num);
    rpt->ReportVal("|--Memory continious Store Execute insts", memory_continious_store_inst_num);

    for (uint32_t i = 0; i < sca_iq.depth.size(); i++) {
        stringstream s;
        s << name << " SCALAR " << i;
        rpt->ReportValAndPct(s.str() + " pick stall", sca_iq.pick_stall_count[i], cycles);
        rpt->ReportValAndPct(s.str() + " insert stall", sca_iq.insert_stall_count[i], cycles);
    }

    for (uint32_t i = 0; i < agu_iq.depth.size(); i++) {
        stringstream s;
        s << name << " AGU " << i;
        rpt->ReportValAndPct(s.str() + " pick stall", agu_iq.pick_stall_count[i], cycles);
        rpt->ReportValAndPct(s.str() + " insert stall", agu_iq.insert_stall_count[i], cycles);
    }

    for (uint32_t i = 0; i < alu_iq.depth.size(); i++) {
        stringstream s;
        s << name << " VALU " << i;
        rpt->ReportValAndPct(s.str() + " pick stall", alu_iq.pick_stall_count[i], cycles);
        rpt->ReportValAndPct(s.str() + " insert stall", alu_iq.insert_stall_count[i], cycles);
    }

    for (uint32_t i = 0; i < std_iq.depth.size(); i++) {
        stringstream s;
        s << name << " VSTD " << i;
        rpt->ReportValAndPct(s.str() + " pick stall", std_iq.pick_stall_count[i], cycles);
        rpt->ReportValAndPct(s.str() + " insert stall", std_iq.insert_stall_count[i], cycles);
    }
}

void IEXStats::report(const std::string& name) {
    rpt->ReportTitle(name + " Statistics");

    rpt->ReportAvg(name + "CMD average wait time in issueQ", cmd_iq.total_wait, cmd_iq.total_inst_num);
    rpt->ReportAvg(name + "ALU average wait time in issueQ", alu_iq.total_wait, alu_iq.total_inst_num);
    rpt->ReportAvg(name + "LDA average wait time in issueQ", lda_iq.total_wait, lda_iq.total_inst_num);
    rpt->ReportAvg(name + "STA average wait time in issueQ", sta_iq.total_wait, sta_iq.total_inst_num);
    rpt->ReportAvg(name + "STD average wait time in issueQ", std_iq.total_wait, std_iq.total_inst_num);
    rpt->ReportAvg(name + "BRU average wait time in issueQ", bru_iq.total_wait, bru_iq.total_inst_num);
    rpt->ReportAvg(name + "AGU average wait time in issueQ", agu_iq.total_wait, agu_iq.total_inst_num);
    rpt->ReportAvg(name + "SCA average wait time in issueQ", sca_iq.total_wait, sca_iq.total_inst_num);
    rpt->ReportValAndPct(name + "CMD Pick Stall", cmd_iq.all_pick_stall, cycles);
    rpt->ReportValAndPct(name + "ALU Pick Stall", alu_iq.all_pick_stall, cycles);
    rpt->ReportValAndPct(name + "LDA Pick Stall", lda_iq.all_pick_stall, cycles);
    rpt->ReportValAndPct(name + "STA Pick Stall", sta_iq.all_pick_stall, cycles);
    rpt->ReportValAndPct(name + "STD Pick Stall", std_iq.all_pick_stall, cycles);
    rpt->ReportValAndPct(name + "BRU Pick Stall", bru_iq.all_pick_stall, cycles);
    rpt->ReportValAndPct(name + "AGU Pick Stall", agu_iq.all_pick_stall, cycles);
    rpt->ReportValAndPct(name + "SCA Pick Stall", sca_iq.all_pick_stall, cycles);
    rpt->ReportValAndPct(name + "CMD insert Stall", cmd_iq.all_insert_stall, cycles);
    rpt->ReportValAndPct(name + "ALU insert Stall", alu_iq.all_insert_stall, cycles);
    rpt->ReportValAndPct(name + "LDA insert Stall", lda_iq.all_insert_stall, cycles);
    rpt->ReportValAndPct(name + "STA insert Stall", sta_iq.all_insert_stall, cycles);
    rpt->ReportValAndPct(name + "STD insert Stall", std_iq.all_insert_stall, cycles);
    rpt->ReportValAndPct(name + "BRU insert Stall", bru_iq.all_insert_stall, cycles);
    rpt->ReportValAndPct(name + "AGU insert Stall", agu_iq.all_insert_stall, cycles);
    rpt->ReportValAndPct(name + "SCA insert Stall", sca_iq.all_insert_stall, cycles);
    rpt->ReportVal(name + "ope s1 insts", ope_s1_inst_cnt);
    rpt->ReportVal(name + "stack get insts", s1_stack_get_inst);
    rpt->ReportVal(name + "stack load check insts", s1_stack_load_inst);
    uint64_t s1_inst = ope_s1_inst_cnt;
    rpt->ReportVal(name + "total s1 insts", s1_inst);
    rpt->ReportAvg(name + "average ope s1 insts", ope_s1_inst_cnt, cycles - stall_cycle);
    rpt->ReportAvg(name + "average total s1 insts", s1_inst, cycles - stall_cycle);
    rpt->ReportValAndPct(name + "Dynamic RENO optimized insts S1", s1_optimized_inst, s1_inst);
    rpt->ReportVal(name + "PE d2 inst total", d2_inst_total);
    rpt->ReportValAndPct(name + "Register sink replace D2", d2_reg_sink_inst, d2_inst_total);
    rpt->ReportValAndPct(name + "Static RENO optimized inst D2", d2_optimized_inst_total, d2_inst_total);
    rpt->ReportValAndPct(name + "RENO partial src get data D2", d2_partial_src_inst_total, d2_inst_total);
    rpt->ReportValAndPct(name + "RENO could be optimized", can_be_optimized_inst, d2_inst_total);
    rpt->ReportValAndPct(name + "RENO CISC register sink inst D2", d2_cisc_sink_inst, d2_inst_total);
    rpt->ReportVal(name + "Load + setc.qe/setc.ne resolve", loadResolveCnt);

    rpt->ReportVal(name + "total ld-st conflict", memoryAaccelssConflict);
    rpt->ReportVal(name + "total mdb intercept", total_intercept);
    rpt->ReportValAndPct(name + "efficient mdb intercept radio", efficient_intercept, total_intercept);

    for (uint32_t i = 0; i < cmd_iq.depth.size(); i++) {
        stringstream s;
        s << name << "CMD IQ " << i << " Statistics";
        rpt->ReportTitle(s.str());
        float cmd_q_avg_occ = (float)(cmd_iq.depth[i]) / cycles;
        rpt->ReportVal("cmd average occupied entries", cmd_q_avg_occ);
        rpt->ReportValAndPct("  |-- occ <= 25%",  cmd_iq.depth_25[i], cycles);
        rpt->ReportValAndPct("  |-- occ <= 50%",  cmd_iq.depth_50[i], cycles);
        rpt->ReportValAndPct("  |-- occ <= 75%",  cmd_iq.depth_75[i], cycles);
        rpt->ReportValAndPct("  |-- occ > 75%",  cmd_iq.depth_100[i], cycles);
        rpt->ReportValAndPct("  |-- Pick Stall ",  cmd_iq.pick_stall_count[i], cycles);
        rpt->ReportValAndPct("  |-- insert Stall ",  cmd_iq.insert_stall_count[i], cycles);
    }
    for (uint32_t i = 0; i < alu_iq.depth.size(); i++) {
        stringstream s;
        s << name << "ALU IQ " << i << " Statistics";
        rpt->ReportTitle(s.str());
        float alu_q_avg_occ = (float)(alu_iq.depth[i]) / cycles;
        rpt->ReportVal("alu average occupied entries", alu_q_avg_occ);
        rpt->ReportValAndPct("  |-- occ <= 25%",  alu_iq.depth_25[i], cycles);
        rpt->ReportValAndPct("  |-- occ <= 50%",  alu_iq.depth_50[i], cycles);
        rpt->ReportValAndPct("  |-- occ <= 75%",  alu_iq.depth_75[i], cycles);
        rpt->ReportValAndPct("  |-- occ > 75%",  alu_iq.depth_100[i], cycles);
        rpt->ReportValAndPct("  |-- Pick Stall ",  alu_iq.pick_stall_count[i], cycles);
        rpt->ReportValAndPct("  |-- insert Stall ",  alu_iq.insert_stall_count[i], cycles);
    }
    for (uint32_t i = 0; i < lda_iq.depth.size(); i++) {
        stringstream s;
        s << name << "LDA IQ " << i << " Statistics";
        rpt->ReportTitle(s.str());
        float lda_q_avg_occ = (float)(lda_iq.depth[i]) / cycles;
        rpt->ReportVal("lda average occupied entries", lda_q_avg_occ);
        rpt->ReportValAndPct("  |-- occ <= 25%",  lda_iq.depth_25[i], cycles);
        rpt->ReportValAndPct("  |-- occ <= 50%",  lda_iq.depth_50[i], cycles);
        rpt->ReportValAndPct("  |-- occ <= 75%",  lda_iq.depth_75[i], cycles);
        rpt->ReportValAndPct("  |-- occ > 75%",  lda_iq.depth_100[i], cycles);
        rpt->ReportValAndPct("  |-- Pick Stall ",  lda_iq.pick_stall_count[i], cycles);
        rpt->ReportValAndPct("  |-- insert Stall ",  lda_iq.insert_stall_count[i], cycles);
    }
    for (uint32_t i = 0; i < sta_iq.depth.size(); i++) {
        stringstream s;
        s << name << "STA IQ " << i << " Statistics";
        rpt->ReportTitle(s.str());
        float sta_q_avg_occ = (float)(sta_iq.depth[i]) / cycles;
        rpt->ReportVal("sta average occupied entries", sta_q_avg_occ);
        rpt->ReportValAndPct("  |-- occ <= 25%",  sta_iq.depth_25[i], cycles);
        rpt->ReportValAndPct("  |-- occ <= 50%",  sta_iq.depth_50[i], cycles);
        rpt->ReportValAndPct("  |-- occ <= 75%",  sta_iq.depth_75[i], cycles);
        rpt->ReportValAndPct("  |-- occ > 75%",  sta_iq.depth_100[i], cycles);
        rpt->ReportValAndPct("  |-- Pick Stall ",  sta_iq.pick_stall_count[i], cycles);
        rpt->ReportValAndPct("  |-- insert Stall ",  sta_iq.insert_stall_count[i], cycles);
    }
    for (uint32_t i = 0; i < std_iq.depth.size(); i++) {
        stringstream s;
        s << name << "STD IQ " << i << " Statistics";
        rpt->ReportTitle(s.str());
        float std_q_avg_occ = (float)(std_iq.depth[i]) / cycles;
        rpt->ReportVal("std average occupied entries", std_q_avg_occ);
        rpt->ReportValAndPct("  |-- occ <= 25%",  std_iq.depth_25[i], cycles);
        rpt->ReportValAndPct("  |-- occ <= 50%",  std_iq.depth_50[i], cycles);
        rpt->ReportValAndPct("  |-- occ <= 75%",  std_iq.depth_75[i], cycles);
        rpt->ReportValAndPct("  |-- occ > 75%",  std_iq.depth_100[i], cycles);
        rpt->ReportValAndPct("  |-- Pick Stall ",  std_iq.pick_stall_count[i], cycles);
        rpt->ReportValAndPct("  |-- insert Stall ",  std_iq.insert_stall_count[i], cycles);
    }
    for (uint32_t i = 0; i < bru_iq.depth.size(); i++) {
        stringstream s;
        s << name << "BRU IQ " << i << " Statistics";
        rpt->ReportTitle(s.str());
        float bru_q_avg_occ = (float)(bru_iq.depth[i]) / cycles;
        rpt->ReportVal("bru average occupied entries", bru_q_avg_occ);
        rpt->ReportValAndPct("  |-- occ <= 25%",  bru_iq.depth_25[i], cycles);
        rpt->ReportValAndPct("  |-- occ <= 50%",  bru_iq.depth_50[i], cycles);
        rpt->ReportValAndPct("  |-- occ <= 75%",  bru_iq.depth_75[i], cycles);
        rpt->ReportValAndPct("  |-- occ > 75%",  bru_iq.depth_100[i], cycles);
        rpt->ReportValAndPct("  |-- Pick Stall ",  bru_iq.pick_stall_count[i], cycles);
        rpt->ReportValAndPct("  |-- insert Stall ",  bru_iq.insert_stall_count[i], cycles);
    }
    for (uint32_t i = 0; i < agu_iq.depth.size(); i++) {
        stringstream s;
        s << name << "Vector AGU IQ " << i << " Statistics";
        rpt->ReportTitle(s.str());
        float agu_q_avg_occ = (float)(agu_iq.depth[i]) / cycles;
        rpt->ReportVal("sca average occupied entries", agu_q_avg_occ);
        rpt->ReportValAndPct("  |-- occ <= 25%",  agu_iq.depth_25[i], cycles);
        rpt->ReportValAndPct("  |-- occ <= 50%",  agu_iq.depth_50[i], cycles);
        rpt->ReportValAndPct("  |-- occ <= 75%",  agu_iq.depth_75[i], cycles);
        rpt->ReportValAndPct("  |-- occ > 75%",  agu_iq.depth_100[i], cycles);
        rpt->ReportValAndPct("  |-- Pick Stall ",  agu_iq.pick_stall_count[i], cycles);
        rpt->ReportValAndPct("  |-- insert Stall ",  agu_iq.insert_stall_count[i], cycles);
    }
    for (uint32_t i = 0; i < sca_iq.depth.size(); i++) {
        stringstream s;
        s << name << "Vector Scalar IQ " << i << " Statistics";
        rpt->ReportTitle(s.str());
        float sca_q_avg_occ = (float)(sca_iq.depth[i]) / cycles;
        rpt->ReportVal("sca average occupied entries", sca_q_avg_occ);
        rpt->ReportValAndPct("  |-- occ <= 25%",  sca_iq.depth_25[i], cycles);
        rpt->ReportValAndPct("  |-- occ <= 50%",  sca_iq.depth_50[i], cycles);
        rpt->ReportValAndPct("  |-- occ <= 75%",  sca_iq.depth_75[i], cycles);
        rpt->ReportValAndPct("  |-- occ > 75%",  sca_iq.depth_100[i], cycles);
        rpt->ReportValAndPct("  |-- Pick Stall ",  sca_iq.pick_stall_count[i], cycles);
        rpt->ReportValAndPct("  |-- insert Stall ",  sca_iq.insert_stall_count[i], cycles);
    }
}

void IEXStats::topdown(const std::string& name) {
    rpt->ReportTitle(name + " IEX Top-Down Metric");
    uint64_t mem_bound = memstall_anyload + memstall_store;
    uint64_t slots_iq_stall = cmd_iq.slots_stall + alu_iq.slots_stall + lda_iq.slots_stall +
        std_iq.slots_stall + bru_iq.slots_stall;
    float backend_pct = 0;
    if (slots != 0) {
        backend_pct = (float)(slots - slots_retired - slots_empty - slots_bandwidth) / (float)slots;
    }
    rpt->ReportPct("Retiring", slots_retired, slots);
    uint64_t dsipBandwidth = slots_bandwidth - slots_iq_stall;
    rpt->ReportPct("Dsipatch bound", slots_empty + dsipBandwidth, slots);
    rpt->ReportPct("  |--Dsipatch Latency", slots_empty, slots);
    rpt->ReportPct("  |--Dsipatch Bandwidth", dsipBandwidth, slots);
    rpt->ReportPct("ISQ Source", slots_iq_stall, slots);
    rpt->ReportPct("  |--CMD",  cmd_iq.slots_stall, slots);
    rpt->ReportPct("  |--ALU",  alu_iq.slots_stall, slots);
    rpt->ReportPct("  |--AGU",  agu_iq.slots_stall, slots);
    rpt->ReportPct("  |--LDA",  lda_iq.slots_stall, slots);
    rpt->ReportPct("  |--STD",  std_iq.slots_stall, slots);
    rpt->ReportPct("  |--BRU",  bru_iq.slots_stall, slots);
    rpt->ReportPct("  |--SCA",  sca_iq.slots_stall, slots);
    rpt->ReportPct("Core bound", (exec_stall - mem_bound) * backend_pct, static_cast<float>(exec_stall));
    rpt->ReportPct("Memory bound", mem_bound * backend_pct, static_cast<float>(exec_stall));
    rpt->ReportPct("  |--L1 bound",  (memstall_anyload - memstall_l1miss) * backend_pct, static_cast<float>(exec_stall));
    rpt->ReportPct("  |--L2 bound",  (memstall_l1miss  - memstall_l2miss) * backend_pct, static_cast<float>(exec_stall));
    rpt->ReportPct("  |--Mem bound", memstall_l2miss * backend_pct, static_cast<float>(exec_stall));
}

void IEXStats::Build(const IEXConfig& configs) {
    uint32_t iexCmdIqCount = (id == SCALAR_IEX) ? configs.iexCmdIqCount : 0;
    uint32_t iexAluIqCount = (id == SCALAR_IEX) ? configs.iexAluIqCount : configs.simt_iex_alu_iq_count;
    uint32_t iexAguIqCount = (id == SCALAR_IEX) ? 0 : configs.simt_iex_agu_iq_count;
    uint32_t iexLdaIqCount = (id == SCALAR_IEX) ? configs.iexLdaIqCount : 0;
    uint32_t iexStaIqCount = (id == SCALAR_IEX) ? configs.iexStaIqCount : 0;
    uint32_t iexStdIqCount = (id == SCALAR_IEX) ? configs.iexStdIqCount : configs.simt_iex_std_iq_count;
    uint32_t iexBruIqCount = (id == SCALAR_IEX) ? configs.iexBruIqCount : 0;
    uint32_t iexScaIqCount = (id == SCALAR_IEX) ? 0 : configs.simt_iex_sca_iq_count;

    if (id == MEM_IEX) {
        iexCmdIqCount = (id == SCALAR_IEX) ? configs.iexCmdIqCount : 0;
        iexAluIqCount = (id == SCALAR_IEX) ? configs.iexAluIqCount : configs.mtc_iex_alu_iq_count;
        iexLdaIqCount = (id == SCALAR_IEX) ? configs.iexLdaIqCount : configs.mtc_iex_lda_iq_count;
        iexStaIqCount = (id == SCALAR_IEX) ? configs.iexStaIqCount : configs.mtc_iex_sta_iq_count;
        iexStdIqCount = (id == SCALAR_IEX) ? configs.iexStdIqCount : configs.mtc_iex_std_iq_count;
        iexBruIqCount = (id == SCALAR_IEX) ? configs.iexBruIqCount : configs.mtc_iex_bru_iq_count;
    }

    // cmd statistics
    cmd_iq.depth.resize(iexCmdIqCount, 0);
    cmd_iq.depth_25.resize(iexCmdIqCount, 0);
    cmd_iq.depth_50.resize(iexCmdIqCount, 0);
    cmd_iq.depth_75.resize(iexCmdIqCount, 0);
    cmd_iq.depth_100.resize(iexCmdIqCount, 0);
    cmd_iq.pick_stall_count.resize(iexCmdIqCount, 0);
    cmd_iq.insert_stall_count.resize(iexCmdIqCount, 0);
    // alu statistics
    alu_iq.depth.resize(iexAluIqCount, 0);
    alu_iq.depth_25.resize(iexAluIqCount, 0);
    alu_iq.depth_50.resize(iexAluIqCount, 0);
    alu_iq.depth_75.resize(iexAluIqCount, 0);
    alu_iq.depth_100.resize(iexAluIqCount, 0);
    alu_iq.pick_stall_count.resize(iexAluIqCount, 0);
    alu_iq.insert_stall_count.resize(iexAluIqCount, 0);
    if (id != SCALAR_IEX) {
        alu_iq.valuStats = std::make_shared<VAluStats>();
    }
    // agu statistics
    agu_iq.depth.resize(iexAguIqCount, 0);
    agu_iq.depth_25.resize(iexAguIqCount, 0);
    agu_iq.depth_50.resize(iexAguIqCount, 0);
    agu_iq.depth_75.resize(iexAguIqCount, 0);
    agu_iq.depth_100.resize(iexAguIqCount, 0);
    agu_iq.pick_stall_count.resize(iexAguIqCount, 0);
    agu_iq.insert_stall_count.resize(iexAguIqCount, 0);
    // lda statistics
    lda_iq.depth.resize(iexLdaIqCount, 0);
    lda_iq.depth_25.resize(iexLdaIqCount, 0);
    lda_iq.depth_50.resize(iexLdaIqCount, 0);
    lda_iq.depth_75.resize(iexLdaIqCount, 0);
    lda_iq.depth_100.resize(iexLdaIqCount, 0);
    lda_iq.pick_stall_count.resize(iexLdaIqCount, 0);
    lda_iq.insert_stall_count.resize(iexLdaIqCount, 0);
    // sta statistics
    sta_iq.depth.resize(iexStaIqCount, 0);
    sta_iq.depth_25.resize(iexStaIqCount, 0);
    sta_iq.depth_50.resize(iexStaIqCount, 0);
    sta_iq.depth_75.resize(iexStaIqCount, 0);
    sta_iq.depth_100.resize(iexStaIqCount, 0);
    sta_iq.pick_stall_count.resize(iexStaIqCount, 0);
    sta_iq.insert_stall_count.resize(iexStaIqCount, 0);
    // std statistics
    std_iq.depth.resize(iexStdIqCount, 0);
    std_iq.depth_25.resize(iexStdIqCount, 0);
    std_iq.depth_50.resize(iexStdIqCount, 0);
    std_iq.depth_75.resize(iexStdIqCount, 0);
    std_iq.depth_100.resize(iexStdIqCount, 0);
    std_iq.pick_stall_count.resize(iexStdIqCount, 0);
    std_iq.insert_stall_count.resize(iexStdIqCount, 0);
    // bru statistics
    bru_iq.depth.resize(iexBruIqCount, 0);
    bru_iq.depth_25.resize(iexBruIqCount, 0);
    bru_iq.depth_50.resize(iexBruIqCount, 0);
    bru_iq.depth_75.resize(iexBruIqCount, 0);
    bru_iq.depth_100.resize(iexBruIqCount, 0);
    bru_iq.pick_stall_count.resize(iexBruIqCount, 0);
    bru_iq.insert_stall_count.resize(iexBruIqCount, 0);
    // Vector Scalar
    sca_iq.depth.resize(iexScaIqCount, 0);
    sca_iq.depth_25.resize(iexScaIqCount, 0);
    sca_iq.depth_50.resize(iexScaIqCount, 0);
    sca_iq.depth_75.resize(iexScaIqCount, 0);
    sca_iq.depth_100.resize(iexScaIqCount, 0);
    sca_iq.pick_stall_count.resize(iexScaIqCount, 0);
    sca_iq.insert_stall_count.resize(iexScaIqCount, 0);

    if (id != SCALAR_IEX) {
        for (uint32_t offset = 0; offset < configs.simt_iex_max_ex_no_rslv_cflt; offset++) {
            sAluExtendCycles.emplace(offset, 0);
            vAluExtendCycles.emplace(offset, 0);
        }
        vectorALUPickerIssueCnt.assign(configs.simt_iex_alu_iq_picker + configs.simt_iex_alu_iq_heter_picker,
            vector<uint32_t>(static_cast<uint32_t>(IexExecUnit::UNIT_NUM), 0));
        uint32_t maxRdPort = 10; // 3-pipe x 3-src + 1-st = 10
        for (uint32_t bankId = 0; bankId < configs.simt_vrf_bank_num; bankId++) {
            for (uint32_t portId = 0; portId <= maxRdPort; portId++) {
                vectorVrfBankRdPortUsedCnt[bankId][portId] = 0;
            }
        }
        vectorRdPortUsedCnt.assign(configs.simt_iex_read_port_num, 0);
        vectorPipeSrcRdPortUsedCnt.assign(static_cast<uint32_t>(RdPortControl::PipeSrcId::NUM),
            vector<uint64_t>(configs.simt_iex_read_port_num, 0));
    }
}

void IEXStats::Reset()
{
    cycles = 0;
    stall_cycle = 0;

    cmd_iq = IQStats();
    alu_iq = IQStats();
    lda_iq = IQStats();
    sta_iq = IQStats();
    std_iq = IQStats();
    bru_iq = IQStats();
    agu_iq = IQStats();

    slots = 0;
    slots_idle = 0;
    slots_retired = 0;
    slots_bandwidth = 0;
    slots_empty = 0;
    issued_cnt = 0;
    exec_stall = 0;
    memstall_anyload = 0;
    memstall_store = 0;
    memstall_l1miss = 0;
    memstall_l2miss = 0;

    ope_s1_inst_cnt = 0;
    s1_stack_get_inst = 0;
    s1_stack_load_inst = 0;
    s1_optimized_inst = 0;
    d2_inst_total = 0;
    d2_optimized_inst_total = 0;
    d2_partial_src_inst_total = 0;
    can_be_optimized_inst = 0;
    d2_reg_sink_inst = 0;
    d2_cisc_sink_inst = 0;
    loadResolveCnt = 0;
    total_wakeup = 0;
    no_wakeup = 0;
    total_intercept = 0;
    efficient_intercept = 0;
    memoryAaccelssConflict = 0;
    no_wakeup_ptag_map.clear();

    tile_load_inst_num = 0;
    tile_gather_load_inst_num = 0;
    tile_gather_stall_cycle = 0;
    tile_gather_issue_cycle = 0;
    tile_gather_complete_cycle = 0;
    tile_continious_load_inst_num = 0;
    tile_store_inst_num = 0;
    tile_scatter_store_inst_num = 0;
    tile_continious_store_inst_num = 0;
    memory_load_inst_num = 0;
    memory_gather_load_inst_num = 0;
    memory_continious_load_inst_num = 0;
    memory_store_inst_num = 0;
    memory_scatter_store_inst_num = 0;
    memory_continious_store_inst_num = 0;
    total_mem_load_req_cnt = 0;
    total_mem_load_latency = 0;
    dispatchStall = 0;
    src1Conflict = 0;
    src2Conflict = 0;
}

IEXStats::~IEXStats()
{
    no_wakeup_ptag_map.clear();
}

} // namespace JCore
