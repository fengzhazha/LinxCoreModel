#include "bctrl/bctrl_stats.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <utility>
#include <vector>

#include "../isa/ISACommon/InstGroup.h"

namespace JCore {

void BCtrlStats::topdown() {
    rpt->ReportTitle("BCC Top-Down Metrics");
    uint64_t slots_retired = binsts + slot_minst_retired;
    rpt->ReportPct("Retiring", slots_retired, slots_total);
    uint64_t slotsRobAllocated = slotsBrobAllocated + slotsPerobAllocated;
    uint64_t slots_bad_spec = (slotsRobAllocated >= slots_retired) ? slotsRobAllocated - slots_retired : 0;
    rpt->ReportPct("Bad speculation", slots_bad_spec, slots_total);
    float frontend_bound = float(slots_fe_bubble) / float(slots_total);
    rpt->ReportPct("Frontend bound", frontend_bound);
    float fetch_latency = float(cycles_fe_bubble) / float(cycles);
    const uint64_t feCycleCnt = 4;
    rpt->ReportPct("  |--Fetch latency", fetch_latency);
    rpt->ReportPct("    |--redirect bubble", float((bru_flush + ooo_flush) * feCycleCnt) / float(cycles));
    rpt->ReportPct("    |--other bubble", fetch_latency - float((bru_flush + ooo_flush) * feCycleCnt) / float(cycles));
    rpt->ReportPct("  |--Fetch bandwidth", frontend_bound - fetch_latency);
    uint64_t slots_be_bound = slots_total - slots_fe_bubble - slots_bad_spec - slots_retired;
    rpt->ReportPct("Backend bound", slots_be_bound, slots_total);
    float backend_pct = (float)slots_be_bound / (float)slots_total;
    uint64_t be_total_stall = slots_decode_stall + slots_brename_stall + slots_brob_stall + slots_sysblock_stall;
    rpt->ReportPct("  |--Decode bound", static_cast<float>(slots_decode_stall)*backend_pct, static_cast<float>(be_total_stall));
    rpt->ReportPct("  |--Rename bound", static_cast<float>(slots_brename_stall)*backend_pct, static_cast<float>(be_total_stall));
    rpt->ReportPct("  |--BROB bound",   static_cast<float>(slots_brob_stall)  *backend_pct, static_cast<float>(be_total_stall));
    rpt->ReportPct("  |--SYS block bound", static_cast<float>(slots_sysblock_stall)*backend_pct, static_cast<float>(be_total_stall));
}

void BCtrlStats::ReportTopDownInst()
{
    rpt->ReportTitle("Retired block Type");
    rpt->ReportVal("Retired Block Num", binsts);
    rpt->ReportVal("  |--Retired STD Block Num", rtiredHdrStd);
    rpt->ReportVal("  |--Retired PARALLEL Block Num", rtiredHdrSimt);
    rpt->ReportValAndPct("    |--Retired CUBE Block Num", rtiredCubeBlockCnt, rtiredHdrSimt);
    rpt->ReportValAndPct("    |--Retired VCALL Block Num", rtiredVcallBlockCnt, rtiredHdrSimt);
    rpt->ReportValAndPct("    |--Retired MCALL Block Num", rtiredMcallBlockCnt, rtiredHdrSimt);
    rpt->ReportValAndPct("    |--Retired TMA Block Num", rtiredTMABlockCnt, rtiredHdrSimt);
    rpt->ReportVal("  |--Retired SYS Block Num", rtiredHdrSys);
    rpt->ReportVal("  |--Retired FP Block Num", rtiredHdrFp);
    rpt->ReportVal("  |--Retired TEMPLATE Block Num", rtiredHdrCt);
    rpt->ReportVal("    |--MEMSET TEMPLATE Block Num", rtiredHdrCtMset);
    rpt->ReportVal("    |--MEMCOPY TEMPLATE Block Num", rtiredHdrCtMcopy);
    rpt->ReportVal("    |--FENTRY TEMPLATE Block Num", rtiredHdrCtFentry);
    rpt->ReportVal("    |--FEXIT TEMPLATE Block Num", rtiredHdrCtFexit);
    rpt->ReportVal("    |--FRET TEMPLATE Block Num", rtiredHdrCtFret);
    rpt->ReportVal("  |--Retired OTHER Block Num", rtiredHdrOther);
}

void BCtrlStats::ReportBCCInst()
{
    rpt->ReportVal("Retired instructions", minsts);
    for (uint64_t i = 0; i < static_cast<uint64_t>(InstGroup::UNDEF); i++) {
        rpt->ReportValAndPct("  |--" + GetInstGroupName(static_cast<InstGroup>(i)), groupInstMap[i], minsts);
    }
}

void BCtrlStats::ReportBccStats()
{
    rpt->ReportTitle("BCC Stall Stats Total");
    rpt->ReportVal("BRob Full Stall", slots_brob_stall);
    rpt->ReportVal("RENAME Stall", slots_brename_stall);
    rpt->ReportVal("Block Rename Get Stall", brename_get_stall);
    rpt->ReportVal("Block Rename Set Stall", brename_set_stall);
    rpt->ReportVal("BIsq Full Stall ", bisqFullStall);
    rpt->ReportVal("TileReg Full Stall ", tileregSizeFullStall);

    for (size_t stid = 0; stid < smtSlotsBrobStallArray.size(); stid++) {
        rpt->ReportTitle("BCC Stall Stats SThread " + std::to_string(stid));
        rpt->ReportVal("BRob Full Stall", smtSlotsBrobStallArray[stid]);
        rpt->ReportVal("RENAME Stall", smtSlotsBrenameStallArray[stid]);
        rpt->ReportVal("Block Rename Get Stall", smtBrenameGetStallArray[stid]);
        rpt->ReportVal("Block Rename Set Stall", smtBrenameSetStallArray[stid]);
        rpt->ReportVal("BIsq Full Stall ", smtBisqFullStallArray[stid]);
        rpt->ReportVal("TileReg Full Stall ", smtTileregSizeFullStallArray[stid]);
    }
}

std::string toHex(uint64_t num) {
    std::string strHex = "";
    while(num) {
        uint64_t numTmp = num % 16;
        char c = numTmp >= 10 ? 'a' + (numTmp - 10): '0' + numTmp;
        strHex = c + strHex;
        num >>= 4;
    }

    return strHex;
}

void BCtrlStats::report() {
    rpt->ReportTitle("BCC Branch Prediction");
    rpt->ReportAvg("Retired MPKB", float(retired_mispred) * 1000, float(retired_header));
    rpt->ReportVal("Retired header", retired_header);
    rpt->ReportVal("Retired header type", retired_header);
    rpt->ReportValAndPct("  |--FALL", rtiredHdrFall, retired_header);
    rpt->ReportValAndPct("  |--COND", rtiredHdrCond, retired_header);
    rpt->ReportValAndPct("  |--DIRECT", rtiredHdrDir, retired_header);
    rpt->ReportValAndPct("  |--INDIRECT", rtiredHdrInDir, retired_header);
    rpt->ReportValAndPct("  |--CALL", rtiredHdrCall, retired_header);
    rpt->ReportValAndPct("  |--INDCALL", rtiredHdrInCall, retired_header);
    rpt->ReportValAndPct("  |--RET", rtiredHdrRet, retired_header);
    rpt->ReportValAndPct("  |--ECALL", rtiredHdrEcall, retired_header);
    rpt->ReportValAndPct("  |--OTHER", rtiredHdrOthBr, retired_header);
    rpt->ReportAvg("Average header retire latency", float(retired_header_total_lat), float(retired_header));
    rpt->ReportVal("Retired mispredicted header", retired_mispred);
    rpt->ReportValAndPct("  |--Misp FALL", rtiredHdrFallMiss, retired_mispred);
    rpt->ReportValAndPct("  |--Misp COND", rtiredHdrCondMiss, retired_mispred);
    rpt->ReportValAndPct("  |--Misp DIRECT", rtiredHdrDirMiss, retired_mispred);
    rpt->ReportValAndPct("  |--Misp INDIRECT", rtiredHdrInDirMiss, retired_mispred);
    rpt->ReportValAndPct("  |--Misp CALL", rtiredHdrCallMiss, retired_mispred);
    rpt->ReportValAndPct("  |--Misp INDCALL", rtiredHdrInCallMiss, retired_mispred);
    rpt->ReportValAndPct("  |--Misp RET", rtiredHdrRetMiss, retired_mispred);
    rpt->ReportValAndPct("  |--Misp ECALL", rtiredHdrEcallMiss, retired_mispred);
    rpt->ReportValAndPct("  |--Misp OTHER", rtiredHdrOthBrMiss, retired_mispred);
    rpt->ReportVal("First after nuke", retired_misp_first_after_nuke);

    rpt->ReportTitle("Percentage of block Type");
    rpt->ReportVal("Retired header", binsts);
    rpt->ReportValAndPct("  |--STD", rtiredHdrStd, binsts);
    rpt->ReportValAndPct("  |--SYS", rtiredHdrSys, binsts);
    rpt->ReportValAndPct("  |--FP", rtiredHdrFp, binsts);
    rpt->ReportValAndPct("  |--SIMT", rtiredHdrSimt, binsts);
    rpt->ReportValAndPct("  |--TEMP", rtiredHdrCt, binsts);
    rpt->ReportValAndPct("    |--MEMSET", rtiredHdrCtMset, rtiredHdrCt);
    rpt->ReportValAndPct("    |--MEMCOPY", rtiredHdrCtMcopy, rtiredHdrCt);
    rpt->ReportValAndPct("    |--FENTRY", rtiredHdrCtFentry, rtiredHdrCt);
    rpt->ReportValAndPct("    |--FEXIT", rtiredHdrCtFexit, rtiredHdrCt);
    rpt->ReportValAndPct("    |--FRET", rtiredHdrCtFret, rtiredHdrCt);
    rpt->ReportValAndPct("  |--OTHER", rtiredHdrOther, binsts);

    rpt->ReportTitle("Percentage of template block Minsts");
    rpt->ReportVal("Retired temelate header minsts", rtiredHdrCtMinsts);
    rpt->ReportValAndPct("  |--MEMSET", rtiredHdrCtMsetMinsts, rtiredHdrCtMinsts);
    rpt->ReportValAndPct("  |--MEMCOPY", rtiredHdrCtMcopyMinsts, rtiredHdrCtMinsts);
    rpt->ReportValAndPct("  |--FENTRY", rtiredHdrCtFentryMinsts, rtiredHdrCtMinsts);
    rpt->ReportValAndPct("  |--FEXIT", rtiredHdrCtFexitMinsts, rtiredHdrCtMinsts);
    rpt->ReportValAndPct("  |--FRET", rtiredHdrCtFretMinsts, rtiredHdrCtMinsts);

    rpt->ReportTitle("Average MInsts in Template Header");
    rpt->ReportAvg("MEMSET", rtiredHdrCtMsetMinsts, rtiredHdrCtMset);
    rpt->ReportAvg("MEMCOPY", rtiredHdrCtMcopyMinsts, rtiredHdrCtMcopy);
    rpt->ReportAvg("FENTRY", rtiredHdrCtFentryMinsts, rtiredHdrCtFentry);
    rpt->ReportAvg("FEXIT", rtiredHdrCtFexitMinsts, rtiredHdrCtFexit);
    rpt->ReportAvg("FRET", rtiredHdrCtFretMinsts, rtiredHdrCtFret);

    rpt->ReportTitle("Average MInsts in Header(Type)");
    rpt->ReportAvg("STD", rtiredHdrStdMinsts, rtiredHdrStd);
    rpt->ReportAvg("SYS", rtiredHdrSysMinsts, rtiredHdrSys);
    rpt->ReportAvg("FP", rtiredHdrFpMinsts, rtiredHdrFp);
    rpt->ReportAvg("SIMT", rtiredHdrSimtMinsts, rtiredHdrSimt);
    rpt->ReportAvg("TEMP", rtiredHdrCtMinsts, rtiredHdrCt);
    rpt->ReportAvg("OTHER", rtiredHdrOtherMinsts, rtiredHdrOther);
    rpt->ReportTitle("Average MInsts in Header(Branch)");
    rpt->ReportAvg("FALL", rtiredHdrFallMinst, rtiredHdrFall);
    rpt->ReportAvg("COND", rtiredHdrCondMinst, rtiredHdrCond);
    rpt->ReportAvg("DIRECT", rtiredHdrDirMinst, rtiredHdrDir);
    rpt->ReportAvg("INDIRECT", rtiredHdrInDirMinst, rtiredHdrInDir);
    rpt->ReportAvg("CALL", rtiredHdrCallMinst, rtiredHdrCall);
    rpt->ReportAvg("INDCALL", rtiredHdrInCallMinst, rtiredHdrInCall);
    rpt->ReportAvg("RET", rtiredHdrRetMinst, rtiredHdrRet);
    rpt->ReportAvg("ECALL", rtiredHdrEcallMinst, rtiredHdrEcall);
    rpt->ReportAvg("OTHER", rtiredHdrOthBrMinst, rtiredHdrOthBr);

    const uint64_t linkMaxSize = 8;
    uint64_t tRegSrcCntSum = 0;
    uint64_t uRegSrcCntSum = 0;
    uint64_t vtRegSrcCntSum = 0;
    uint64_t vuRegSrcCntSum = 0;
    uint64_t vmRegSrcCntSum = 0;
    uint64_t vnRegSrcCntSum = 0;
    uint64_t tRegSrcRobDistSum = 0;
    uint64_t uRegSrcRobDistSum = 0;
    uint64_t vtRegSrcRobDistSum = 0;
    uint64_t vuRegSrcRobDistSum = 0;
    uint64_t vmRegSrcRobDistSum = 0;
    uint64_t vnRegSrcRobDistSum = 0;
    for (uint64_t i = 0; i < linkMaxSize; ++i) {
        tRegSrcCntSum += tRegSrcCnt[i];
        uRegSrcCntSum += uRegSrcCnt[i];
        vtRegSrcCntSum += vtRegSrcCnt[i];
        vuRegSrcCntSum += vuRegSrcCnt[i];
        vmRegSrcCntSum += vmRegSrcCnt[i];
        vnRegSrcCntSum += vnRegSrcCnt[i];
        tRegSrcRobDistSum += tRegSrcRobDist[i];
        uRegSrcRobDistSum += uRegSrcRobDist[i];
        vtRegSrcRobDistSum += vtRegSrcRobDist[i];
        vuRegSrcRobDistSum += vuRegSrcRobDist[i];
        vmRegSrcRobDistSum += vmRegSrcRobDist[i];
        vnRegSrcRobDistSum += vnRegSrcRobDist[i];
    }
    rpt->ReportTitle("Average Rob Distance for Aaccelss T_reg");
    rpt->ReportAvg("T_reg avg", tRegSrcRobDistSum, tRegSrcCntSum);
    for  (uint64_t i = 0; i < linkMaxSize; ++i) {
        rpt->ReportAvg("T#" + std::to_string(i + 1), tRegSrcRobDist[i], tRegSrcCnt[i]);
    }
    rpt->ReportTitle("Average Rob Distance for Aaccelss U_reg");
    rpt->ReportAvg("U_reg avg", uRegSrcRobDistSum, uRegSrcCntSum);
    for  (uint64_t i = 0; i < linkMaxSize; ++i) {
        rpt->ReportAvg("U#" + std::to_string(i + 1), uRegSrcRobDist[i], uRegSrcCnt[i]);
    }
    rpt->ReportTitle("Average Rob Distance for Aaccelss VT_reg");
    rpt->ReportAvg("VT_reg avg", vtRegSrcRobDistSum, vtRegSrcCntSum);
    for  (uint64_t i = 0; i < linkMaxSize; ++i) {
        rpt->ReportAvg("VT#" + std::to_string(i + 1), vtRegSrcRobDist[i], vtRegSrcCnt[i]);
    }
    rpt->ReportTitle("Average Rob Distance for Aaccelss VU_reg");
    rpt->ReportAvg("VU_reg avg", vuRegSrcRobDistSum, vuRegSrcCntSum);
    for  (uint64_t i = 0; i < linkMaxSize; ++i) {
        rpt->ReportAvg("VU#" + std::to_string(i + 1), vuRegSrcRobDist[i], vuRegSrcCnt[i]);
    }
    rpt->ReportTitle("Average Rob Distance for Aaccelss VM_reg");
    rpt->ReportAvg("VM_reg avg", vmRegSrcRobDistSum, vmRegSrcCntSum);
    for  (uint64_t i = 0; i < linkMaxSize; ++i) {
        rpt->ReportAvg("VM#" + std::to_string(i + 1), vmRegSrcRobDist[i], vmRegSrcCnt[i]);
    }
    rpt->ReportTitle("Average Rob Distance for Aaccelss VN_reg");
    rpt->ReportAvg("VN_reg avg", vnRegSrcRobDistSum, vnRegSrcCntSum);
    for  (uint64_t i = 0; i < linkMaxSize; ++i) {
        rpt->ReportAvg("VN#" + std::to_string(i + 1), vnRegSrcRobDist[i], vnRegSrcCnt[i]);
    }

    rpt->ReportTitle("BCC Pipe");
    rpt->ReportVal("BHC aaccelss", bhc_aaccelss);
    rpt->ReportValAndPct("  |--BHC miss", bhc_miss, bhc_aaccelss);
    rpt->ReportAvg("Average header fetch latency", bhc_total_lat, bhc_aaccelss);
    rpt->ReportVal("BTLB aaccelss", btlb_aaccelss);
    rpt->ReportValAndPct("  |--BTLB miss", btlb_miss, btlb_aaccelss);
    rpt->ReportVal("UBTB aaccelss", ubtb_aaccelss);
    rpt->ReportValAndPct("  |--UBTB miss", ubtb_miss, ubtb_aaccelss);
    rpt->ReportVal("PBTB aaccelss", pbtb_aaccelss);
    rpt->ReportValAndPct("  |--PBTB miss", pbtb_miss, pbtb_aaccelss);
    rpt->ReportVal("TAGE allocate", tageAllocSuaccelss + tageAllocFail);
    rpt->ReportValAndPct("  |--TAGE allocate suaccelss", tageAllocSuaccelss, tageAllocSuaccelss + tageAllocFail);
    rpt->ReportValAndPct("  |--TAGE allocate fail", tageAllocFail, tageAllocSuaccelss + tageAllocFail);
    rpt->ReportVal("History conflict", histConfNum);

    if (histHashMap.size() > 0) {
        std::vector<std::pair<uint64_t, HashPCInfo>> histHashVec(histHashMap.begin(), histHashMap.end());
        sort(histHashVec.begin(), histHashVec.end(),
             [](std::pair<uint64_t, HashPCInfo>& a, std::pair<uint64_t, HashPCInfo>& b) {
            const uint32_t conflictNum = 2;
            if (a.second.confilictNum < conflictNum) {
                return false;
            }
            if (b.second.confilictNum < conflictNum) {
                return true;
            }
            return (a.second.allocNum > b.second.allocNum);
        });
        for (auto it = histHashVec.begin(); it != histHashVec.end(); it++) {
            const uint32_t conflictNum = 2;
            if (it->second.confilictNum < conflictNum) {
                break;
            }
            std::string rptStrTop = "  |--(NODB)History tag 0x" + toHex(it->first);
            rpt->ReportVal(rptStrTop, it->second.allocNum);
            const size_t maxStatsCnt = 3;
            std::vector<std::pair<uint64_t, DstInfo>> pcHashVec(it->second.pcMap.begin(), it->second.pcMap.end());
            sort(pcHashVec.begin(), pcHashVec.end(),
                 [](std::pair<uint64_t, DstInfo>& a, std::pair<uint64_t, DstInfo>& b) {
                return (a.second.num > b.second.num);
            });
            size_t i = 0;
            for (auto sucIt = pcHashVec.begin(); sucIt != pcHashVec.end() && i < maxStatsCnt; sucIt++, i++) {
                std::string rptStr = "    |--(NODB)Hash pc 0x" + toHex(sucIt->second.pc) +
                                     " & tgt 0x" + toHex(sucIt->second.tgt);
                rpt->ReportValAndPct(rptStr, sucIt->second.num, it->second.allocNum);
            }
        }
    }
    rpt->ReportVal("TAGE bank conflict", tageConfNum);
    for (size_t i = 0; i < TAG_NTABLE; i++) {
        ReportTageBankConflict(i);
    }
    rpt->ReportAvg("Local pipe occupied", local_occupied_num, local_occupied_cyc);
    rpt->ReportAvg("Average fall through size", fetch_size_after_taken, fetch_fall_throgh);
    uint64_t total_deliver = f4_bhq_non_fb + f4_bhq_one_fb + f4_bhq_two_fb + f4_bhq_multi_fb;

    rpt->ReportVal("F4 pipe total deliver", total_deliver);
    rpt->ReportValAndPct("  |--F4 pipe deliver 0 FB", f4_bhq_non_fb, total_deliver);
    rpt->ReportValAndPct("  |--F4 pipe deliver 1 FB", f4_bhq_one_fb, total_deliver);
    rpt->ReportValAndPct("  |--F4 pipe deliver 2 FB", f4_bhq_two_fb, total_deliver);
    rpt->ReportValAndPct("  |--F4 pipe deliver multiple FB", f4_bhq_multi_fb, total_deliver);
    rpt->ReportVal("Backend stall", backend_stall);
    rpt->ReportVal("BRQ stall", brq_stall);
    rpt->ReportVal("BHQ clear", bhq_clear);
    rpt->ReportVal("BHQ empty", bhq_empty);
    rpt->ReportVal("BHQ get header", bhq_get_header);
    rpt->ReportVal("Slots FE bubble", slots_fe_bubble);
    rpt->ReportVal("Cycles FE bubble", cycles_fe_bubble);
    rpt->ReportVal("BHC stall global", bhc_stall_global);
    rpt->ReportVal("BHC stall local", bhc_stall_local);
    rpt->ReportVal("Local stall global", local_stall_global);
    rpt->ReportVal("RAS stall", ras_stall);
    rpt->ReportValAndPct("Intra flush global occupy F4", intra_flush_global, f4_working);
    rpt->ReportValAndPct("Intra flush caused by FB1", fb1_intra_flush, intra_flush_global);
    rpt->ReportValAndPct("Intra flush caused by FB2", fb2_intra_flush, intra_flush_global);
    rpt->ReportVal("Intra flush short-forward", forward_flush);
    rpt->ReportValAndPct("Short-forward prediction", forward_pred, f1_two_pred);
    rpt->ReportVal("F1 stage two taken prediction", f1_two_pred);
    rpt->ReportVal("F1 stage predict taken", f1_pred_taken);
    rpt->ReportVal("F1 stage use local pipe", f1_use_local);
    rpt->ReportVal("Intra flush global", intra_flush_global);
    rpt->ReportVal("Intra flush local end", intra_flush_local_end);
    rpt->ReportVal("Intra flush local inconditional taken", intra_flush_local_incond);
    rpt->ReportVal("Intra flush local addpc in call", intra_flush_local_call);
    rpt->ReportVal("Intra flush ras", intra_flush_ras);
    rpt->ReportVal("Intra flush ibtb", intra_flush_ibtb);
    rpt->ReportVal("Intra flush ubtb miss", intra_flush_ubtb_miss);
    rpt->ReportVal("Intra flush ubtb miss uncod", intra_flush_ubtb_miss_uncond);
    rpt->ReportVal("Intra flush ubtb nt", intra_flush_ubtb_nt);
    rpt->ReportVal("Intra flush ubtb tk", intra_flush_ubtb_tk);
    rpt->ReportVal("Intra flush ubtb tk pos", intra_flush_ubtb_tk_pos);
    rpt->ReportVal("Intra flush ubtb tk dir", intra_flush_ubtb_tk_dir);
    rpt->ReportVal("Intra flush ubtb tk tgt", intra_flush_ubtb_tk_tgt);
    rpt->ReportVal("Intra flush ubtb hit bim only", intra_flush_ubtb_hit_bim_only);
    rpt->ReportAvg("Average issued headers per cycle", total_p1_count, cycles - pe_stall_cycle);
    rpt->ReportVal("Intra flush of fetch PC", intra_flush);

    if (intraFlushCntMap.size() > 0) {
        const int maxStastCnt = 20;
        uint32_t i = 0;
        std::vector<std::pair<uint64_t, uint64_t>> intraCntVec(intraFlushCntMap.begin(), intraFlushCntMap.end());
        sort(intraCntVec.begin(), intraCntVec.end(), [](std::pair<uint64_t, uint64_t> &a, std::pair<uint64_t, uint64_t> &b) {
            return a.second > b.second;
        });
        for (auto it = intraCntVec.begin(); it != intraCntVec.end() && i < maxStastCnt; ++it, ++i) {
            // (NODB):this flag is used to prevent the statistics from being displayed by the CI
            std::string rptStr = "  |--(NODB)Top" + std::to_string(i) + " 0x" + toHex(it->first);
            rpt->ReportValAndPct(rptStr, it->second, intra_flush);
        }
    }

    rpt->ReportVal("Loop trained", loop_trained);
    rpt->ReportVal("Loop predicted", loop_predicted);
    rpt->ReportVal("Loop mispred", loop_mispred);

    rpt->ReportVal("BROB stall", slots_brob_stall);
    rpt->ReportVal("RENAME stall", slots_brename_stall);
    rpt->ReportVal("brename get stall", brename_get_stall);
    rpt->ReportVal("brename set stall", brename_set_stall);

    rpt->ReportTitle("BFU Loop Buffer");
    rpt->ReportVal("Loop Buffer insert", lb_insert);
    rpt->ReportVal("Loop Buffer hit", lb_hit);
    rpt->ReportVal("Loop Buffer redirect", lb_redirect);
    rpt->ReportVal("Loop Buffer Send Headers num", lb_sendHdr);
    rpt->ReportValAndPct("Loop Buffer retired header", lb_commitHdrCnt, retired_header);
    rpt->ReportAvg("Average headers per hit", lb_commitHdrCnt, lb_hit);

    rpt->ReportTitle("Mispredicted BPC of being predicted");
    rpt->ReportVal("Total Predicted BPC", bpc_mispredict_count);
    if (misBpcCntMap.size() > 0) {
        const int maxStastCnt = 20;
        uint32_t i = 0;
        std::vector<std::pair<uint64_t, uint64_t>> misBpcCntVec(misBpcCntMap.begin(), misBpcCntMap.end());
        sort(misBpcCntVec.begin(), misBpcCntVec.end(), [](std::pair<uint64_t, uint64_t> &a, std::pair<uint64_t, uint64_t> &b) {
            return a.second > b.second;
        });
        for (auto it = misBpcCntVec.begin(); it != misBpcCntVec.end() && i < maxStastCnt; ++it, ++i) {
            std::string rptStr = "  |--(NODB)Top" + std::to_string(i) + " 0x" + toHex(it->first);
            rpt->ReportValAndPct(rptStr, it->second, bpc_mispredict_count);
        }
    }

    rpt->ReportTitle("Mispredicted BPC of doing prediction");
    rpt->ReportVal("Total Predicted BPC", bpc_mispredict_count);
    if (misBrBpcCntMap.size() > 0) {
        const int maxStastCnt = 20;
        uint32_t i = 0;
        std::vector<std::pair<uint64_t, uint64_t>> misBpcCntVec(misBrBpcCntMap.begin(), misBrBpcCntMap.end());
        sort(misBpcCntVec.begin(), misBpcCntVec.end(), [](std::pair<uint64_t, uint64_t> &a, std::pair<uint64_t, uint64_t> &b) {
            return a.second > b.second;
        });
        for (auto it = misBpcCntVec.begin(); it != misBpcCntVec.end() && i < maxStastCnt; ++it, ++i) {
            std::string rptStr = "  |--(NODB)Top" + std::to_string(i) + " 0x" + toHex(it->first);
            rpt->ReportValAndPct(rptStr, it->second, bpc_mispredict_count);
        }
    }

    rpt->ReportValAndPct("Mispredicted of FB1", first_taken_mis_pred, bpc_mispredict_count);
    rpt->ReportValAndPct("Mispredicted of FB2", second_taken_mis_pred, bpc_mispredict_count);
    rpt->ReportValAndPct("Mispredicted of short forward", forward_mis_pred, bpc_mispredict_count);
    rpt->ReportTitle("Block Dispatch Statistics");
    rpt->ReportVal("Block Dispatch count", blk_dispatch_cnt);
    rpt->ReportVal("Round Robin count", rr_dispatch_cnt);
    rpt->ReportVal("Workload count", wl_dispatch_cnt);
    rpt->ReportVal("TPC Continue count", tc_dispatch_cnt);
    rpt->ReportVal("BMDB count", bmdb_dispatch_cnt);
}


void BCtrlStats::ReportTageBankConflict(size_t t)
{
    auto &hashMap = tageHashMap[t];
    if (hashMap.size() < 0) {
        return;
    }
    std::vector<std::pair<uint64_t, HashPCInfo>> tageHashVec(hashMap.begin(), hashMap.end());
    sort(tageHashVec.begin(), tageHashVec.end(),
         [](std::pair<uint64_t, HashPCInfo>& a, std::pair<uint64_t, HashPCInfo>& b) {
        const uint32_t conflictNum = 2;
        if (a.second.confilictNum < conflictNum) {
            return false;
        }
        if (b.second.confilictNum < conflictNum) {
            return true;
        }
        return (a.second.allocNum > b.second.allocNum);
    });
    const size_t maxTagStats = 3;
    size_t i = 0;
    for (auto it = tageHashVec.begin(); it != tageHashVec.end() && i < maxTagStats; it++, i++) {
        const uint32_t conflictNum = 2;
        if (it->second.confilictNum < conflictNum) {
            break;
        }
        std::string rptStrTop = "  |--(NODB)TAGE table " + std::to_string(t) +  " tag 0x" + toHex(it->first);
        rpt->ReportVal(rptStrTop, it->second.allocNum);
        const size_t maxStatsCnt = 3;
        std::vector<std::pair<uint64_t, DstInfo>> pcHashVec(it->second.pcMap.begin(), it->second.pcMap.end());
        sort(pcHashVec.begin(), pcHashVec.end(), [](std::pair<uint64_t, DstInfo>& a, std::pair<uint64_t, DstInfo>& b) {
            return (a.second.num > b.second.num);
        });
        size_t j = 0;
        for (auto sucIt = pcHashVec.begin(); sucIt != pcHashVec.end() && j < maxStatsCnt; sucIt++, j++) {
            std::string rptStr = "    |--(NODB)Hash pc tag 0x" + toHex(sucIt->second.pc);
            rpt->ReportValAndPct(rptStr, sucIt->second.num, it->second.allocNum);
        }
    }
}

void BCtrlStats::Reset()
{
    cycles = 0;
    binsts = 0;
    minsts = 0;
    slot_minst_retired = 0;
    moveInsts = 0;
    total_brob_size = 0;
    total_ostd_blocks = 0;
    total_ostd_cube_blocks = 0;
    total_ostd_vector_blocks = 0;
    total_ostd_tload = 0;
    total_ostd_tstore = 0;
    fetchMinstCnt = 0;
    fetchHdrCnt = 0;
    exist_minst_cycle = 0;

    slots_total = 0;
    slotsBrobAllocated = 0;
    slotsPerobAllocated = 0;
    slots_fe_bubble = 0;
    slots_flush_recover_bubble = 0;
    slots_decode_stall = 0;
    slots_brename_stall = 0;
    slots_brob_stall = 0;
    slots_sysblock_stall = 0;
    cycles_fe_bubble = 0;
    slots_clear_bubble = 0;
    clear_retired = 0;
    brename_set_stall = 0;
    brename_get_stall = 0;
    bisqFullStall = 0;
    tileregSizeFullStall = 0;

    bhc_aaccelss = 0;
    bhc_miss = 0;
    bhc_total_lat = 0;
    btlb_aaccelss = 0;
    btlb_miss = 0;
    ubtb_aaccelss = 0;
    ubtb_miss = 0;
    pbtb_aaccelss = 0;
    pbtb_miss = 0;

    forward_pred = 0;
    f1_two_pred = 0;
    f1_pred_taken = 0;
    f1_use_local = 0;
    f4_bhq_non_fb = 0;
    f4_bhq_one_fb = 0;
    f4_bhq_two_fb = 0;
    f4_bhq_multi_fb = 0;
    f4_working = 0;
    f4_pred_taken = 0;
    f4_use_local = 0;
    local_occupied_cyc = 0;
    local_occupied_num = 0;
    fetch_size_after_taken = 0;
    fetch_fall_throgh = 0;

    loop_trained = 0;
    loop_predicted = 0;
    loop_mispred = 0;

    lb_hit = 0;
    lb_insert = 0;
    lb_redirect = 0;
    lb_sendHdr = 0;
    lb_commitHdrCnt = 0;

    tageAllocSuaccelss = 0;
    tageAllocFail = 0;
    histConfNum = 0;
    tageConfNum = 0;

    rtiredHdrStd = 0;
    rtiredHdrSys = 0;
    rtiredHdrFp = 0;
    rtiredHdrSimt = 0;
    rtiredCubeBlockCnt = 0;
    rtiredVcallBlockCnt = 0;
    rtiredMcallBlockCnt = 0;
    rtiredHdrCt = 0;
    rtiredHdrOther = 0;
    rtiredTMABlockCnt = 0;

    rtiredHdrCtMset = 0;
    rtiredHdrCtMcopy = 0;
    rtiredHdrCtFentry = 0;
    rtiredHdrCtFexit = 0;
    rtiredHdrCtFret = 0;

    rtiredHdrStdMinsts = 0;
    rtiredHdrSysMinsts = 0;
    rtiredHdrFpMinsts = 0;
    rtiredHdrSimtMinsts = 0;
    rtiredHdrCtMinsts = 0;
    rtiredHdrOtherMinsts = 0;

    rtiredHdrCtMsetMinsts = 0;
    rtiredHdrCtMcopyMinsts = 0;
    rtiredHdrCtFentryMinsts = 0;
    rtiredHdrCtFexitMinsts = 0;
    rtiredHdrCtFretMinsts = 0;

    const uint64_t linkMaxSize = 8;
    tRegSrcCnt.resize(linkMaxSize, 0);
    uRegSrcCnt.resize(linkMaxSize, 0);
    vtRegSrcCnt.resize(linkMaxSize, 0);
    vuRegSrcCnt.resize(linkMaxSize, 0);
    vmRegSrcCnt.resize(linkMaxSize, 0);
    vnRegSrcCnt.resize(linkMaxSize, 0);
    tRegSrcRobDist.resize(linkMaxSize, 0);
    uRegSrcRobDist.resize(linkMaxSize, 0);
    vtRegSrcRobDist.resize(linkMaxSize, 0);
    vuRegSrcRobDist.resize(linkMaxSize, 0);
    vmRegSrcRobDist.resize(linkMaxSize, 0);
    vnRegSrcRobDist.resize(linkMaxSize, 0);

    retired_header = 0;
    retired_fb = 0;
    retired_mispred = 0;
    retired_mispred_cond = 0;
    retired_mispred_target = 0;
    retired_mispred_direction = 0;
    retired_header_total_lat = 0;
    retired_misp_first_after_nuke = 0;

    rtiredHdrFall = 0;
    rtiredHdrCond = 0;
    rtiredHdrDir = 0;
    rtiredHdrInDir = 0;
    rtiredHdrCall = 0;
    rtiredHdrInCall = 0;
    rtiredHdrRet = 0;
    rtiredHdrEcall = 0;
    rtiredHdrOthBr = 0;

    rtiredHdrFallMiss = 0;
    rtiredHdrCondMiss = 0;
    rtiredHdrDirMiss = 0;
    rtiredHdrInDirMiss = 0;
    rtiredHdrCallMiss = 0;
    rtiredHdrInCallMiss = 0;
    rtiredHdrRetMiss = 0;
    rtiredHdrEcallMiss = 0;
    rtiredHdrOthBrMiss = 0;

    rtiredHdrFallMinst = 0;
    rtiredHdrCondMinst = 0;
    rtiredHdrDirMinst = 0;
    rtiredHdrInDirMinst = 0;
    rtiredHdrCallMinst = 0;
    rtiredHdrInCallMinst = 0;
    rtiredHdrRetMinst = 0;
    rtiredHdrEcallMinst = 0;
    rtiredHdrOthBrMinst = 0;

    intra_flush = 0;
    intra_flush_global = 0;
    intra_flush_local_end = 0;
    intra_flush_local_incond = 0;
    intra_flush_local_call = 0;
    fb1_intra_flush = 0;
    fb2_intra_flush = 0;
    forward_flush = 0;
    intra_flush_ibtb = 0;
    intra_flush_ras = 0;
    intra_flush_ubtb_miss = 0;
    intra_flush_ubtb_miss_uncond = 0;
    intra_flush_ubtb_nt = 0;
    intra_flush_ubtb_tk = 0;
    intra_flush_ubtb_tk_pos = 0;
    intra_flush_ubtb_tk_dir = 0;
    intra_flush_ubtb_tk_tgt = 0;
    intra_flush_ubtb_hit_bim_only = 0;
    bru_flush = 0;
    ooo_flush = 0;

    backend_stall = 0;
    brq_stall = 0;
    bhc_stall_global = 0;
    bhc_stall_local = 0;
    local_stall_global = 0;
    btlb_stall = 0;
    ras_stall = 0;

    bhq_clear = 0;
    bhq_empty = 0;
    bhq_get_header = 0;

    bpc_discontinue_count = 0;
    bpc_mispredict_count = 0;
    first_taken_mis_pred = 0;
    second_taken_mis_pred = 0;
    forward_mis_pred = 0;
    total_p1_count = 0;
    pe_stall_cycle = 0;

    blk_dispatch_cnt = 0;
    rr_dispatch_cnt = 0;
    wl_dispatch_cnt = 0;
    tc_dispatch_cnt = 0;
    bmdb_dispatch_cnt = 0;

    misBpcCntMap.clear();
    misBrBpcCntMap.clear();
    intraFlushCntMap.clear();
    histHashMap.clear();
    groupInstMap.clear();
    for (size_t i = 0; i < TAG_NTABLE; i++) {
        tageHashMap[i].clear();
    }
}

void BCtrlStats::SetEfficient(uint64_t cycle)
{
    efficient += cycle;
}

uint64_t BCtrlStats::GetEfficient() const
{
    return efficient;
}

void BCtrlStats::SetBccBound(uint64_t cycle)
{
    bccBound += cycle;
}

uint64_t BCtrlStats::GetBccBound() const
{
    return bccBound;
}

void BCtrlStats::SetBccBoundEfficient(uint64_t cycle)
{
    bccBoundEfficient += cycle;
}

uint64_t BCtrlStats::GetBccBoundEfficient() const
{
    return bccBoundEfficient;
}

void BCtrlStats::SetBccBoundFrontend(uint64_t cycle)
{
    bccBoundFrontend += cycle;
}

uint64_t BCtrlStats::GetBccBoundFrontend() const
{
    return bccBoundFrontend;
}

void BCtrlStats::SetBccBoundBackend(uint64_t cycle)
{
    bccBoundBackend += cycle;
}

uint64_t BCtrlStats::GetBccBoundBackend() const
{
    return bccBoundBackend;
}

void BCtrlStats::SetBccBoundBackendRobStall(uint64_t cycle)
{
    bccBoundBackendRobStall += cycle;
}

uint64_t BCtrlStats::GetBccBoundBackendRobStall() const
{
    return bccBoundBackendRobStall;
}

void BCtrlStats::SetBccBoundBackendRegStall(uint64_t cycle)
{
    bccBoundBackendRegStall += cycle;
}

void BCtrlStats::ReduceBccBoundBackendRegStall(uint64_t cycle)
{
    bccBoundBackendRegStall -= cycle;
}

uint64_t BCtrlStats::GetBccBoundBackendRegStall() const
{
    return bccBoundBackendRegStall;
}

void BCtrlStats::SetBccBoundBackendIqStall(uint64_t cycle)
{
    bccBoundBackendIqStall += cycle;
}

void BCtrlStats::ReduceBccBoundBackendIqStall(uint64_t cycle)
{
    bccBoundBackendIqStall -= cycle;
}

uint64_t BCtrlStats::GetBccBoundBackendIqStall() const
{
    return bccBoundBackend;
}

void BCtrlStats::SetBccBoundBackendMemBound(uint64_t cycle)
{
    bccBoundBackendMemBound += cycle;
}

void BCtrlStats::ReduceBccBoundBackendMemBound(uint64_t cycle)
{
    bccBoundBackendMemBound -= cycle;
}

uint64_t BCtrlStats::GetBccBoundBackendMemBound() const
{
    return bccBoundBackendMemBound;
}

void BCtrlStats::SetPeBound(uint64_t cycle)
{
    peBound += cycle;
}

uint64_t BCtrlStats::GetPeBound() const
{
    return peBound;
}

void BCtrlStats::SetPeBoundCube(uint64_t cycle)
{
    peBoundCube += cycle;
}

uint64_t BCtrlStats::GetPeBoundCube() const
{
    return peBoundCube;
}

void BCtrlStats::SetPeBoundCubeBrobStall(uint64_t cycle)
{
    peBoundCubeBrobStall += cycle;
}

uint64_t BCtrlStats::GetPeBoundCubeBrobStall() const
{
    return peBoundCubeBrobStall;
}

uint64_t BCtrlStats::GetPeBoundCubeBiqStall() const
{
    return peBoundCubeBiqStall;
}

void BCtrlStats::SetPeBoundCubeBiqStall(uint64_t cycle)
{
    peBoundCubeBiqStall += cycle;
}

void BCtrlStats::SetPeBoundCubeTileTagStall(uint64_t cycle)
{
    peBoundCubeTileTagStall += cycle;
}

uint64_t BCtrlStats::GetPeBoundCubeTileTagStall() const
{
    return peBoundCubeTileTagStall;
}

void BCtrlStats::SetPeBoundVec(uint64_t cycle)
{
    peBoundVec += cycle;
}

uint64_t BCtrlStats::GetPeBoundVec() const
{
    return peBoundVec;
}

void BCtrlStats::SetPeBoundVecBrobStall(uint64_t cycle)
{
    peBoundVecBrobStall += cycle;
}

uint64_t BCtrlStats::GetPeBoundVecBrobStall() const
{
    return peBoundVecBrobStall;
}

uint64_t BCtrlStats::GetPeBoundVecBiqStall() const
{
    return peBoundVecBiqStall;
}

void BCtrlStats::SetPeBoundVecBiqStall(uint64_t cycle)
{
    peBoundVecBiqStall += cycle;
}

void BCtrlStats::SetPeBoundVecTileTagStall(uint64_t cycle)
{
    peBoundVecTileTagStall += cycle;
}

uint64_t BCtrlStats::GetPeBoundVecTileTagStall() const
{
    return peBoundVecTileTagStall;
}

void BCtrlStats::SetPeBoundTma(uint64_t cycle)
{
    peBoundTma += cycle;
}

uint64_t BCtrlStats::GetPeBoundTma() const
{
    return peBoundTma;
}

void BCtrlStats::SetPeBoundTmaBrobStall(uint64_t cycle)
{
    peBoundTmaBrobStall += cycle;
}

uint64_t BCtrlStats::GetPeBoundTmaBrobStall() const
{
    return peBoundTmaBrobStall;
}

uint64_t BCtrlStats::GetPeBoundTmaBiqStall() const
{
    return peBoundTmaBiqStall;
}

void BCtrlStats::SetPeBoundTmaBiqStall(uint64_t cycle)
{
    peBoundTmaBiqStall += cycle;
}

void BCtrlStats::SetPeBoundTmaTileTagStall(uint64_t cycle)
{
    peBoundTmaTileTagStall += cycle;
}

uint64_t BCtrlStats::GetPeBoundTmaTileTagStall() const
{
    return peBoundTmaTileTagStall;
}

void BCtrlStats::SetPeBoundTau(uint64_t cycle)
{
    peBoundTau += cycle;
}

uint64_t BCtrlStats::GetPeBoundTau() const
{
    return peBoundTau;
}

void BCtrlStats::SetPeBoundTauBrobStall(uint64_t cycle)
{
    peBoundTauBrobStall += cycle;
}

uint64_t BCtrlStats::GetPeBoundTauBrobStall() const
{
    return peBoundTauBrobStall;
}

uint64_t BCtrlStats::GetPeBoundTauBiqStall() const
{
    return peBoundTauBiqStall;
}

void BCtrlStats::SetPeBoundTauBiqStall(uint64_t cycle)
{
    peBoundTauBiqStall += cycle;
}

void BCtrlStats::SetPeBoundTauTileTagStall(uint64_t cycle)
{
    peBoundTauTileTagStall += cycle;
}

uint64_t BCtrlStats::GetPeBoundTauTileTagStall() const
{
    return peBoundTauTileTagStall;
}

} // namespace JCore
