#include <sstream>
#include <map>

#include "SPEStats.h"

namespace JCore {

void SPEStats::report(std::string& pe_name) {
    std::string str = pe_name + " Statistics";
    rpt->ReportTitle(str);
    rpt->ReportVal("Retired blocks", binsts);
    rpt->ReportVal("Retired insts", minsts);
    rpt->ReportVal("Retired load", retired_load);
    rpt->ReportVal("Retired store", retired_store);
    rpt->ReportVal("Retired inner jump", retired_innerJump);
    rpt->ReportVal("Stall caused by local gpr", localRegStall);
    rpt->ReportVal("No issue get stall", stall_get);
    rpt->ReportVal("No issue load stall", memstall_anyload);
    rpt->ReportVal("No issue get or load stall", stall_get_or_load);
    rpt->ReportVal("Average fetch insts per cycle", float(minsts_per_cycle) / float(exist_minst_cycle));
}

void SPEStats::ReportVec(const std::string& name)
{
    std::stringstream sTitle;
    sTitle << name << " OOO&IEX stats";
    rpt->ReportTitle(sTitle.str());
    rpt->ReportVal("decode inst bubble", slots_fe_bubble);
    rpt->ReportAvg("average decode bundle", decode_cnt, decodeCycle);
    rpt->ReportVal("Thread0 Pick Cnt", threadPickCnt[0]);
    rpt->ReportVal("Thread1 Pick Cnt", threadPickCnt[1]);
    rpt->ReportVal("Thread2 Pick Cnt", threadPickCnt[2]);
    rpt->ReportVal("Thread3 Pick Cnt", threadPickCnt[3]);
    rpt->ReportVal("Thread0 rob stall cnt", threadRobStall[0]);
    rpt->ReportVal("Thread1 rob stall cnt", threadRobStall[1]);
    rpt->ReportVal("Thread2 rob stall cnt", threadRobStall[2]);
    rpt->ReportVal("Thread3 rob stall cnt", threadRobStall[3]);
    rpt->ReportVal("decode pe rob stall", robStall);
    rpt->ReportVal("decode local gpr stall", localRegStall);
    rpt->ReportVal("inst buffer stall", ibStall);
    rpt->ReportVal("predicate reg stall", pptagStall);
    rpt->ReportVal("vector reg ptag stall", vptagStall);
    rpt->ReportVal("vector local scalar reg stall", lptagStall);

    rpt->ReportVal("retired ld inst", retired_load);
    rpt->ReportVal("retired st inst", retired_store);
    rpt->ReportVal("retired scalar inst", retiredScalar);
    rpt->ReportVal("retired vector inst", retiredVector);
    rpt->ReportVal("retired div/sqrt inst", retiredDivSqrt);

    for (size_t i = 0; i < mapQStall.size(); i++) {
        std::stringstream ss;
        ss << "MAPQ lg" << i << " stall";
        rpt->ReportVal(ss.str(), mapQStall[i]);
    }

    for (size_t i = 0; i < logicROBStall.size(); i++) {
        std::stringstream ss;
        ss << "ROB lg" << i << " stall";
        rpt->ReportVal(ss.str(), logicROBStall[i]);
    }

    const std::map<size_t, std::string> vrfRegName = {
        {0, "VT"},
        {1, "VU"},
        {2, "VM"},
        {3, "VN"},
    };
    for (size_t i = 0; i < vrfUsedCycles.size(); i++) {
        std::stringstream ss;
        ss << vrfRegName.at(i) << " average lifecycle/used times";
        if (vrfUsedTimes[i] == 0) {
            rpt->ReportVal(ss.str(), 0.);
        } else {
            float avgUsed = static_cast<float>(vrfUsedCycles[i]) / static_cast<float>(vrfUsedTimes[i]);
            rpt->ReportVal(ss.str(), avgUsed);
        }
    }
    for (size_t i = 0; i < vrfRecycledTimes.size(); i++) {
        std::stringstream ss;
        ss << vrfRegName.at(i) << " recycled times";
        rpt->ReportVal(ss.str(), vrfRecycledTimes[i]);
    }
    for (size_t i = 0; i < vrfPreReleaseTimes.size(); i++) {
        std::stringstream ss;
        ss << vrfRegName.at(i) << " pre-release times";
        rpt->ReportVal(ss.str(), vrfPreReleaseTimes[i]);
    }
}

void SPEStats::topdown(std::string& pe_name) {
    rpt->ReportTitle(pe_name + " Top-Down Metric");
    uint64_t slots_retired = minsts;
    rpt->ReportPct("Retiring", slots_retired, slots_total);
    uint64_t slots_other_idle = slots_idle - slots_flush_recover_idle;
    rpt->ReportPct("Idle", slots_idle, slots_total);
    rpt->ReportPct("  |--Flush recover idle", slots_flush_recover_idle, slots_total);
    rpt->ReportPct("  |--Other idle", slots_other_idle, slots_total);
    uint64_t slots_badspec = ((slots_rob_allocated >= slots_retired)? slots_rob_allocated-slots_retired : 0);
    rpt->ReportPct("Bad speculation", slots_badspec, slots_total);
    uint64_t slots_bcc_supply_bubble = slots_supply_bubble - slots_decode_stall;
    uint64_t slots_bcc_send_bubble = slots_bcc_supply_bubble - slots_flush_recover_bubble;
    rpt->ReportPct("Frontend bound", slots_supply_bubble, slots_total);
    rpt->ReportPct("  |--Inadequate supply", slots_bcc_supply_bubble, slots_total);
    rpt->ReportPct("     |--Flush recover bubble", slots_flush_recover_bubble, slots_total);
    rpt->ReportPct("     |--BCC send bubble", slots_bcc_send_bubble, slots_total);
    rpt->ReportPct("  |--Decode bound", slots_decode_stall, slots_total);
    rpt->ReportPct("     |--Decode stall by local reg", 0, slots_decode_stall);
    uint64_t slots_backend_bound = slots_total - slots_retired - slots_badspec - slots_idle - slots_supply_bubble;
    float backendPct = (float)slots_backend_bound / (float)slots_total;
    uint64_t beTotal = slotsRenameStall + slotsRobStall + slotsIqStall;
    rpt->ReportPct("Backend bound", slots_backend_bound, slots_total);
    rpt->ReportPct("  |--Rename bound", static_cast<float>(slotsRenameStall)*backendPct, static_cast<float>(beTotal));
    rpt->ReportPct("  |--ROB bound", static_cast<float>(slotsRobStall)*backendPct, static_cast<float>(beTotal));
    rpt->ReportPct("  |--iex bound", static_cast<float>(slotsIqStall)*backendPct, static_cast<float>(beTotal));
}

void SPEStats::Reset()
{
    cycles = 0;
    decodeCycle = 0;
    alloc_cycle = 0;
    renameCycle = 0;
    d2_s1_cycle = 0;
    binsts = 0;
    binsts_commit = 0;
    minsts = 0;
    robDepth = 0;
    max_rob_size = 0;
    minsts_per_cycle = 0;
    exist_minst_cycle = 0;

    slots_total = 0;
    slots_rob_allocated = 0;
    slots_fe_bubble = 0;
    slots_idle = 0;
    slots_flush_recover_idle = 0;
    slotsReceived = 0;

    slotsCtGen = 0;

    slotsDecoderSend = 0;
    slots_supply_bubble = 0;
    slots_decode_stall = 0;
    slots_flush_recover_bubble = 0;
    slotsRenameStall = 0;
    slotsRobStall = 0;
    slotsIqStall = 0;

    exec_stall = 0;
    memstall_anyload = 0;
    memstall_store = 0;
    memstall_l1miss = 0;
    memstall_l2miss = 0;
    memstall_mem = 0;

    retired_load = 0;
    retired_store = 0;
    retired_innerJump = 0;

    curr_cycle_issued_cnt = 0;
    stall_get = 0;
    stall_get_or_load = 0;

    s1_inst_cnt = 0;
    d2_optimized_inst = 0;
    d2_s1_inst = 0;
    d2_reg_sink_inst = 0;
    decode_cnt = 0;

    localRegStall = 0;
    robStall = 0;
    for (size_t i = 0; i < 4; i++) {
        threadRobStall[i] = 0;
        threadPickCnt[i] = 0;
    }

    ibStall = 0;
    pptagStall = 0;
    vptagStall = 0;
    lptagStall = 0;

    retiredScalar = 0;
    retiredVector = 0;
    retiredGather = 0;
    retiredScatter = 0;
    retiredDivSqrt = 0;

    retiredAluInst = 0;
}
} // namespace JCore
