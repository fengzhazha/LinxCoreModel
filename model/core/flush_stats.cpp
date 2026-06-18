#include "core/flush_stats.h"

namespace JCore {

void FlushStats::InitSmtPmu(uint32_t sthreadNum)
{
    smtIntraBlockMispredictionArray.resize(sthreadNum, 0);
    smtInterBlockMispredictionArray.resize(sthreadNum, 0);
    smtIntraBlockMemoryAaccelssConflictArray.resize(sthreadNum, 0);
    smtInterBlockMemoryAaccelssConflictArray.resize(sthreadNum, 0);
}

void FlushStats::ReportFlush()
{
    rpt->ReportTitle("BCC Detail Stats(Flush Statistics) Total");
    rpt->ReportVal("intra-block ld-st conflict", IntraBlockMemoryAaccelssConflict);
    rpt->ReportVal("inter-block ld-st conflict", InterBlockMemoryAaccelssConflict);
    rpt->ReportVal("intra-block misp", IntraBlockMisprediction);
    rpt->ReportVal("inter-block misp", InterBlockMisprediction);

    for (size_t stid = 0; stid < smtIntraBlockMemoryAaccelssConflictArray.size(); stid++) {
        rpt->ReportTitle("BCC Detail Stats(Flush Statistics) SThread " + std::to_string(stid));
        rpt->ReportVal("intra-block ld-st conflict", smtIntraBlockMemoryAaccelssConflictArray[stid]);
        rpt->ReportVal("inter-block ld-st conflict", smtInterBlockMemoryAaccelssConflictArray[stid]);
        rpt->ReportVal("intra-block misp", smtIntraBlockMispredictionArray[stid]);
        rpt->ReportVal("inter-block misp", smtInterBlockMispredictionArray[stid]);
    }
}

void FlushStats::report() {
    rpt->ReportTitle("Flush Statistics");
    rpt->ReportVal("intra-block ld-st conflict", IntraBlockMemoryAaccelssConflict);
    rpt->ReportVal("inter-block ld-st conflict", InterBlockMemoryAaccelssConflict);
    rpt->ReportVal("intra-block misp", IntraBlockMisprediction);
    rpt->ReportVal("inter-block misp", InterBlockMisprediction);
    rpt->ReportVal("Efficient inter ld-st conflict", efficientInterConflict);
    rpt->ReportVal("Efficient intra ld-st conflict", efficientIntraConflict);
    rpt->ReportVal("Efficient inter misprediction", efficientInerMisprediction);
    rpt->ReportVal("Efficient intra misprediction", efficientIntraMisprediction);
    rpt->ReportVal("Efficient block replay", efficientBlockReplay);
    rpt->ReportVal("ldq stall", LoadQueueStall);
    rpt->ReportVal("stq stall", StoreQueueStall);
    rpt->ReportVal("issq stall", IssueQueueStall);
    rpt->ReportVal("Stack get correct" ,stackGetCorrect);
    rpt->ReportVal("Stack get incorrect" ,stackGetIncorrect);
}

void FlushStats::Reset()
{
    IntraBlockMisprediction = 0;
    InterBlockMisprediction = 0;
    IntraBlockMemoryAaccelssConflict = 0;
    InterBlockMemoryAaccelssConflict = 0;
    efficientInterConflict = 0;
    efficientIntraConflict = 0;
    efficientInerMisprediction = 0;
    efficientIntraMisprediction = 0;
    efficientBlockReplay = 0;
    LoadQueueStall = 0;
    StoreQueueStall = 0;
    IssueQueueStall = 0;
    stackGetCorrect = 0;
    stackGetIncorrect = 0;
}

} // namespace JCore
