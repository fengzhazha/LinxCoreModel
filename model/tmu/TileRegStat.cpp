#include "tmu/TileRegStat.h"

namespace JCore {

void TileRegStat::Reset() {}

void TileRegStat::Report(std::string &name)
{
    rpt->ReportTitle(name + " Statistics");
    rpt->ReportVal("Load request count", ldReqCnt);
    rpt->ReportVal("Store request count", stReqCnt);
    rpt->ReportVal("Load request count of bank conflict", ldConfCnt);
    rpt->ReportVal("Store request count of bank conflict", stConfCnt);
    rpt->ReportVal("Total load request size", totalLdSize);
    rpt->ReportVal("Total store request size", totalStSize);
    rpt->ReportVal("Tile Tag aaccelss way cnt", tileTagAaccelss);
    rpt->ReportVal("TA use way cnt", allocTaWayCnt);
    rpt->ReportVal("TO use way cnt", allocToWayCnt);
    rpt->ReportVal("Vec Load hit TA way", hitTaWayCnt);
    rpt->ReportVal("Vec Load hit TO way", hitToWayCnt);
    rpt->ReportVal("Stash free way cnt", freeWayCnt);
    rpt->ReportVal("  |--stash flush way cnt", flushWayCnt);
}

void TileRegStat::SamplingStash()
{
    samplingLdReqCnt = ldReqCnt;
    samplingLdConfCnt = ldConfCnt;
    samplingTotalLdSize = totalLdSize;
    samplingStReqCnt = stReqCnt;
    samplingStConfCnt = stConfCnt;
    samplingTotalStSize = totalStSize;
}

void TileRegStat::ReportSampling(std::string name)
{
    rpt->ReportTitle(name + " Statistics");
    rpt->ReportVal("Load request count", ldReqCnt - samplingLdReqCnt);
    rpt->ReportVal("Store request count", stReqCnt - samplingStReqCnt);
    rpt->ReportVal("Load request count of bank conflict", ldConfCnt - samplingLdConfCnt);
    rpt->ReportVal("Store request count of bank conflict", stConfCnt - samplingStConfCnt);
    rpt->ReportVal("Total load request size", totalLdSize - samplingTotalLdSize);
    rpt->ReportVal("Total store request size", totalStSize - samplingTotalStSize);
}

} // namespace JCore
