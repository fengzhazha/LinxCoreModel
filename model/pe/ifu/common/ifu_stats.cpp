#include "pe/ifu/common/ifu_stats.h"

namespace JCore {

void IFUStats::report(const std::string& name)
{
    rpt->ReportVal("FetchReqs Merge", ifu_merge);
    rpt->ReportTitle(name + " Inner BP Stats");
    rpt->ReportVal("Predict Number", pred_count);
    uint64_t vld_count = mis_count + corr_count;
    rpt->ReportValAndPct("Valid Predict (corr + mis)", vld_count, pred_count);
    rpt->ReportVal("Not Cover", nc_count);
    rpt->ReportVal("Predict Correct", corr_count);
    rpt->ReportValAndPct("InstBP miss", mis_count, vld_count);

    rpt->ReportTitle(name + " ICache Stats");
    rpt->ReportVal("ICache Aaccelss", l1_aaccelss);
    rpt->ReportValAndPct("ICache Miss", l1_miss, l1_aaccelss);
    rpt->ReportAvg("Average Fetch Latency", l1_total_lat, l1_aaccelss);
    rpt->ReportVal("L2 Aaccelss", l2_aaccelss);
    rpt->ReportVal("L2 Invalid L1 ICacheLine", l2_invalid);
}

void IFUStats::ReportVec(const std::string& name)
{
    rpt->ReportTitle(name);
    rpt->ReportVal("ICache Miss", l1_miss);
    rpt->ReportVal("ICache Aaccelss", l1_aaccelss);
    rpt->ReportVal("ICache Refill", l1Refill);
    rpt->ReportVal("Miss Buffer Stall", missBufferStall);
    rpt->ReportVal("All Branch", branch);
    rpt->ReportVal("Condition Branch", cond);
    rpt->ReportVal("Direct Branch", direct);
}

} // namespace JCore
