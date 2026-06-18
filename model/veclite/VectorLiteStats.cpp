#include "veclite/VectorLiteStats.h"

namespace JCore {

void VectorLiteStats::Reset()
{
    m_totalCycle = 0;
    m_busyCycle = 0;
    m_workingCycle = 0;
    m_executeBlockNum = 0;
    m_executeUopNum = 0;

    m_stashStallCycle = 0;
    m_robStallCycle = 0;
    m_siqStallCycle = 0;
    m_uiqStallCycle = 0;
    m_tileRegRdStallCycle = 0;
    m_tileRegWrStallCycle = 0;
    m_tileRegWrDataNotRdyCycle = 0;

    m_tileRegRdCount = 0;
    m_tileRegWrCount = 0;
}

void VectorLiteStats::ReportLoadLatency() const {}

void VectorLiteStats::Report() const
{
    std::stringstream titleStr;
    titleStr << "Vector Lite Core " << m_coreId << " Detail Statistics";
    rpt->ReportTitle(titleStr.str());
    rpt->ReportVal("Vector-Lite Working Cycle", m_workingCycle);
    rpt->ReportVal("Vector-Lite Execute TEMP Block Cnt", m_executeBlockNum);
    rpt->ReportVal("Vector-Lite Execute TEMP Uop Cnt", m_executeUopNum);
    if (m_workingCycle == 0) {
        rpt->ReportVal("Vector IPC", 0.);
    } else {
        rpt->ReportVal("Vector-Lite IPC",
            static_cast<float>(m_executeUopNum) / static_cast<float>(m_workingCycle));
    }
    rpt->ReportVal("Vector-Lite Stash Stall Cycle", m_stashStallCycle);
    rpt->ReportVal("Vector-Lite ROB Stall Cycle", m_robStallCycle);
    rpt->ReportVal("Vector-Lite SIQ Stall Cycle", m_siqStallCycle);
    rpt->ReportVal("Vector-Lite UIQ Stall Cycle", m_uiqStallCycle);
    rpt->ReportVal("Vector-Lite TileReg Read Stall Cycle", m_tileRegRdStallCycle);
    rpt->ReportVal("Vector-Lite TileReg Write Stall Cycle", m_tileRegWrStallCycle);
    rpt->ReportVal("Vector-Lite TileReg Write Data Not Ready Cycle", m_tileRegWrDataNotRdyCycle);
    rpt->ReportVal("Vector-Lite TileReg Read Cnt", m_tileRegRdCount);
    rpt->ReportVal("Vector-Lite TileReg Write Cnt", m_tileRegWrCount);
}

void VectorLiteStats::SamplingStash()
{
    samplingCycle = m_totalCycle;
    samplingWorkingCycle = m_workingCycle;
    samplingExeBlockNum = m_executeBlockNum;
}

void VectorLiteStats::ReportSampling(const std::string &name) const
{
    (void)name;
    // rpt -> ReportTitle "VectorLite " + name + " Statistics "
    // rpt -> ReportVal "Vector core execute block number", m_executeBlockNum - samplingExeBlockNum
}

} // namespace JCore
