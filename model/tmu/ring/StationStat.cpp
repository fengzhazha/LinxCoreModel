#include "tmu/ring/StationStat.h"

namespace JCore {

void StationStat::Reset() {}

void StationStat::Report(std::string &name)
{
    rpt->ReportTitle(name + ": Node " + std::to_string(id) + " Statistics");
    // latency from request to response
    rpt->ReportAvg("Average latency from request to response", static_cast<float>(totalReq2RespLatency),
                   static_cast<float>(ldRespCntOfEnd + stRespCntOfEnd));
    size_t idx = static_cast<size_t>(BufferType::LOAD_REQ0);
    rpt->ReportValAndPct("SPB of ld request occupied cycles", spbOccupiedCycle[idx], cycles);
    rpt->ReportValAndPct("SPB of ld request full cycles", spbFullCycle[idx], cycles);
    rpt->ReportAvg("SPB of ld request average occupied size", spbOccupiedSize[idx], spbOccupiedCycle[idx]);
    rpt->ReportValAndPct("MGB of ld request occupied cycles", mgbOccupiedCycle[idx], cycles);
    rpt->ReportValAndPct("MGB of ld request full cycles", mgbFullCycle[idx], cycles);
    rpt->ReportAvg("MGB of ld request average occupied size", mgbOccupiedSize[idx], spbOccupiedCycle[idx]);
    idx = static_cast<size_t>(BufferType::LOAD_DATA_RES);
    rpt->ReportValAndPct("SPB of ld data occupied cycles", spbOccupiedCycle[idx], cycles);
    rpt->ReportValAndPct("SPB of ld data full cycles", spbFullCycle[idx], cycles);
    rpt->ReportAvg("SPB of ld data average occupied size", spbOccupiedSize[idx], spbOccupiedCycle[idx]);
    rpt->ReportValAndPct("MGB of ld data occupied cycles", mgbOccupiedCycle[idx], cycles);
    rpt->ReportValAndPct("MGB of ld data full cycles", mgbFullCycle[idx], cycles);
    rpt->ReportAvg("MGB of ld data average occupied size", mgbOccupiedSize[idx], spbOccupiedCycle[idx]);
    idx = static_cast<size_t>(BufferType::STORE_DATA);
    rpt->ReportValAndPct("SPB of st data occupied cycles", spbOccupiedCycle[idx], cycles);
    rpt->ReportValAndPct("SPB of st data full cycles", spbFullCycle[idx], cycles);
    rpt->ReportAvg("SPB of st data average occupied size", spbOccupiedSize[idx], spbOccupiedCycle[idx]);
    rpt->ReportValAndPct("MGB of st data occupied cycles", mgbOccupiedCycle[idx], cycles);
    rpt->ReportValAndPct("MGB of st data full cycles", mgbFullCycle[idx], cycles);
    rpt->ReportAvg("MGB of st data average occupied size", mgbOccupiedSize[idx], spbOccupiedCycle[idx]);
    rpt->ReportVal("Read FRQ occupuied cycle", frqReadOccupiedCycle);
    rpt->ReportValAndPct("Read FRQ full cycle", frqReadFullCycle, cycles);
    rpt->ReportAvg("Read FRQ average occupied size", frqReadOccupiedSize, frqReadOccupiedCycle);
    rpt->ReportVal("Write FRQ occupuied cycle", frqWriteOccupiedCycle);
    rpt->ReportValAndPct("Write FRQ full cycle", frqWriteFullCycle, cycles);
    rpt->ReportAvg("Write FRQ average occupied size", frqWriteOccupiedSize, frqWriteOccupiedCycle);
    rpt->ReportValAndPct("On ring cycles", onRingCycle, cycles);
    rpt->ReportValAndPct("Off ring cycles", offRingCycle, cycles);

    idx = static_cast<size_t>(ChannelType::REQ0);
    rpt->ReportVal("Read Req Total Cnt", reqTotalCnt[idx]);
    rpt->ReportVal("CW Req Channel Total Blocked Cycle", cwNotOnRingTotal[idx]);
    rpt->ReportValAndPct("  |--CW Req Blocked By Vector From TileReg Cycle",
        (cwNotOnRingByVec[idx] - cwNotOnRingByVecFromCore[idx]), cwNotOnRingTotal[idx]);
    rpt->ReportValAndPct("  |--CW Req Blocked By Vector From Core Cycle",
        cwNotOnRingByVecFromCore[idx], cwNotOnRingTotal[idx]);
    rpt->ReportValAndPct("  |--CW Req Blocked By Cube From TileReg Cycle",
        (cwNotOnRingByCube[idx] - cwNotOnRingByCubeFromCore[idx]), cwNotOnRingTotal[idx]);
    rpt->ReportValAndPct("  |--CW Req Blocked By Cube From Core Cycle",
        cwNotOnRingByCubeFromCore[idx], cwNotOnRingTotal[idx]);
    rpt->ReportValAndPct("  |--CW Req Blocked By TMA From TileReg Cycle",
        (cwNotOnRingByTma[idx] - cwNotOnRingByTmaFromCore[idx]), cwNotOnRingTotal[idx]);
    rpt->ReportValAndPct("  |--CW Req Blocked By TMA From Core Cycle",
        cwNotOnRingByTmaFromCore[idx], cwNotOnRingTotal[idx]);
    
    rpt->ReportVal("CC Req Channel Total Blocked Cycle", ccNotOnRingTotal[idx]);
    rpt->ReportValAndPct("  |--CC Req Blocked By Vector From TileReg Cycle",
        (ccNotOnRingByVec[idx] - ccNotOnRingByVecFromCore[idx]), ccNotOnRingTotal[idx]);
    rpt->ReportValAndPct("  |--CC Req Blocked By Vector From Core Cycle",
        ccNotOnRingByVecFromCore[idx], ccNotOnRingTotal[idx]);
    rpt->ReportValAndPct("  |--CC Req Blocked By Cube From TileReg Cycle",
        (ccNotOnRingByCube[idx] - ccNotOnRingByCubeFromCore[idx]), ccNotOnRingTotal[idx]);
    rpt->ReportValAndPct("  |--CC Req Blocked By Cube From Core Cycle",
        ccNotOnRingByCubeFromCore[idx], ccNotOnRingTotal[idx]);
    rpt->ReportValAndPct("  |--CC Req Blocked By TMA From TileReg Cycle",
        (ccNotOnRingByTma[idx] - ccNotOnRingByTmaFromCore[idx]), ccNotOnRingTotal[idx]);
    rpt->ReportValAndPct("  |--CC Req Blocked By TMA From Core Cycle",
        ccNotOnRingByTmaFromCore[idx], ccNotOnRingTotal[idx]);

    rpt->ReportVal("Request 0 channel ring busy(CW)", ringBusyCW[idx]);
    rpt->ReportVal("Request 0 channel ring busy(CC)", ringBusyCC[idx]);

    rpt->ReportValAndPct("Request 0 channel on ring back-pressure(CW)", onRingCWBackPressure[idx], cycles);
    rpt->ReportValAndPct("Request 0 channel off ring back-pressure(CW)", offRingCWBackPressure[idx], cycles);
    rpt->ReportValAndPct("Request 0 channel on ring back-pressure(CC)", onRingCCBackPressure[idx], cycles);
    rpt->ReportValAndPct("Request 0 channel off ring back-pressure(CC)", offRingCCBackPressure[idx], cycles);

    idx = static_cast<size_t>(ChannelType::DATA);
    rpt->ReportVal("Write Req Total Cnt", dataTotalCnt[idx]);
    rpt->ReportVal("CW Data Channel Total Blocked Cycle", cwNotOnRingTotal[idx]);
    rpt->ReportValAndPct("|--CW Data Blocked By Vector From TileReg Cycle",
        (cwNotOnRingByVec[idx] - cwNotOnRingByVecFromCore[idx]), cwNotOnRingTotal[idx]);
    rpt->ReportValAndPct("|--CW Ring Data By Vector From Core Cycle",
        cwNotOnRingByVecFromCore[idx], cwNotOnRingTotal[idx]);
    rpt->ReportValAndPct("|--CW Ring Data By Cube From TileReg Cycle",
        (cwNotOnRingByCube[idx] - cwNotOnRingByCubeFromCore[idx]), cwNotOnRingTotal[idx]);
    rpt->ReportValAndPct("|--CW Ring Data By Cube From Core Cycle",
        cwNotOnRingByCubeFromCore[idx], cwNotOnRingTotal[idx]);
    rpt->ReportValAndPct("|--CW Ring Data By TMA From TileReg Cycle",
        (cwNotOnRingByTma[idx] - cwNotOnRingByTmaFromCore[idx]), cwNotOnRingTotal[idx]);
    rpt->ReportValAndPct("|--CW Ring Data By TMA From Core Cycle",
        cwNotOnRingByTmaFromCore[idx], cwNotOnRingTotal[idx]);

    rpt->ReportVal("CC Data Channel Total Blocked Cycle", ccNotOnRingTotal[idx]);
    rpt->ReportValAndPct("|--CC Data Blocked By Vector From TileReg Cycle",
        (ccNotOnRingByVec[idx] - ccNotOnRingByVecFromCore[idx]), ccNotOnRingTotal[idx]);
    rpt->ReportValAndPct("|--CC Data Blocked By Vector From Core Cycle",
        ccNotOnRingByVecFromCore[idx], ccNotOnRingTotal[idx]);
    rpt->ReportValAndPct("|--CC Data Blocked By Cube From TileReg Cycle",
        (ccNotOnRingByCube[idx] - ccNotOnRingByCubeFromCore[idx]), ccNotOnRingTotal[idx]);
    rpt->ReportValAndPct("|--CC Data Blocked By Cube From Core Cycle",
        ccNotOnRingByCubeFromCore[idx], ccNotOnRingTotal[idx]);
    rpt->ReportValAndPct("|--CC Data Blocked By TMA From TileReg Cycle",
        (ccNotOnRingByTma[idx] - ccNotOnRingByTmaFromCore[idx]), ccNotOnRingTotal[idx]);
    rpt->ReportValAndPct("|--CC Data Blocked By TMA From Core Cycle",
        ccNotOnRingByTmaFromCore[idx], ccNotOnRingTotal[idx]);

    rpt->ReportVal("Data channel ring busy(CW)", ringBusyCW[idx]);
    rpt->ReportVal("Data channel ring busy(CC)", ringBusyCC[idx]);

    rpt->ReportValAndPct("Data channel on ring back-pressure(CW)", onRingCWBackPressure[idx], cycles);
    rpt->ReportValAndPct("Data channel off ring back-pressure(CW)", offRingCWBackPressure[idx], cycles);
    rpt->ReportValAndPct("Data channel on ring back-pressure(CC)", onRingCCBackPressure[idx], cycles);
    rpt->ReportValAndPct("Data channel off ring back-pressure(CC)", offRingCCBackPressure[idx], cycles);

    idx = static_cast<size_t>(ChannelType::RSP);
    rpt->ReportVal("Resp Not On CW Ring Total Blocked Cycle", cwNotOnRingTotal[idx]);
    rpt->ReportVal("Resp Not On CW Ring Blocked By Vector Cycle", cwNotOnRingByVec[idx]);
    rpt->ReportVal("Resp Not On CW Ring Blocked By Cube Cycle", cwNotOnRingByCube[idx]);
    rpt->ReportVal("Resp Not On CW Ring Blocked By TMA Cycle", cwNotOnRingByTma[idx]);

    rpt->ReportVal("Resp Not On CC Ring Total Blocked Cycle", ccNotOnRingTotal[idx]);
    rpt->ReportVal("Resp Not On CC Ring Blocked By Vector Cycle", ccNotOnRingByVec[idx]);
    rpt->ReportVal("Resp Not On CC Ring Blocked By Cube Cycle", ccNotOnRingByCube[idx]);
    rpt->ReportVal("Resp Not On CC Ring Blocked By TMA Cycle", ccNotOnRingByTma[idx]);

    rpt->ReportVal("Resp channel ring busy(CW)", ringBusyCW[idx]);
    rpt->ReportVal("Resp channel ring busy(CC)", ringBusyCC[idx]);

    rpt->ReportValAndPct("Resp channel on ring back-pressure(CW)", onRingCWBackPressure[idx], cycles);
    rpt->ReportValAndPct("Resp channel off ring back-pressure(CW)", offRingCWBackPressure[idx], cycles);
    rpt->ReportValAndPct("Resp channel on ring back-pressure(CC)", onRingCCBackPressure[idx], cycles);
    rpt->ReportValAndPct("Resp channel off ring back-pressure(CC)", offRingCCBackPressure[idx], cycles);
}

void StationStat::SamplingStash()
{
    samplingCycles = cycles;
    samplingLdReqCntOfstart = ldReqCntOfstart;
    samplingStReqCntOfstart = stReqCntOfstart;
    samplingLdReqCntOfEnd = ldReqCntOfEnd;
    samplingStReqCntOfEnd = stReqCntOfEnd;
    samplingLdRespCntOfStart = ldRespCntOfStart;
    samplingLdRespCntOfEnd = ldRespCntOfEnd;
    samplingStRespCntOfStart = stRespCntOfStart;
    samplingStRespCntOfEnd = stRespCntOfEnd;

    samplingLdReqCntOfstartSize = ldReqCntOfstartSize;
    samplingStReqCntOfstartSize = stReqCntOfstartSize;
    samplingLdReqCntOfEndSize = ldReqCntOfEndSize;
    samplingStReqCntOfEndSize = stReqCntOfEndSize;
    samplingLdRespCntOfStartSize = ldRespCntOfStartSize;
    samplingLdRespCntOfEndSize = ldRespCntOfEndSize;
    samplingStRespCntOfStartSize = stRespCntOfStartSize;
    samplingStRespCntOfEndSize = stRespCntOfEndSize;

    samplingTotalReq2RespLatency = totalReq2RespLatency;
    for (size_t i = 0; i < static_cast<size_t>(BufferType::COUNT); i++) {
        samplingSpbOccupiedCycle[i] = spbOccupiedCycle[i];
        samplingSpbOccupiedSize[i] = spbOccupiedSize[i];
        samplingSpbFullCycle[i] = spbFullCycle[i];
        samplingMgbOccupiedCycle[i] = mgbOccupiedCycle[i];
        samplingMgbFullCycle[i] = mgbFullCycle[i];
        samplingMgbOccupiedSize[i] = mgbOccupiedSize[i];
    }
    samplingFrqReadOccupiedCycle = frqReadOccupiedCycle;
    samplingFrqReadFullCycle = frqReadFullCycle;
    samplingFrqReadOccupiedSize = frqReadOccupiedSize;
    samplingFrqWriteOccupiedCycle = frqWriteOccupiedCycle;
    samplingFrqWriteFullCycle = frqWriteFullCycle;
    samplingFrqWriteOccupiedSize = frqWriteOccupiedSize;
    samplingOnRingCycle = onRingCycle;
    samplingOffRingCycle = offRingCycle;
}

void StationStat::ReportSampling(std::string name)
{
    rpt->ReportTitle(name + " statistics");
    uint64_t totalCnt = ldReqCntOfstart + stReqCntOfstart + ldReqCntOfEnd + stReqCntOfEnd
                        + ldRespCntOfStart + stRespCntOfStart + ldRespCntOfEnd + stRespCntOfEnd;
    totalCnt -= (samplingLdReqCntOfstart + samplingStReqCntOfstart + samplingLdReqCntOfEnd
                        + samplingStReqCntOfEnd + samplingLdRespCntOfStart + samplingStRespCntOfStart
                        + samplingLdRespCntOfEnd + samplingStRespCntOfEnd);
    // counter of request or response start/end from/at this station
    rpt->ReportVal("Total request and response counter", totalCnt);
    rpt->ReportValAndPct("  |--" + nodeName + "-->" + name + ",load request counter",
                         ldReqCntOfstart - samplingLdReqCntOfstart, totalCnt);
    rpt->ReportValAndPct("  |--" + nodeName + "-->" + name + ",store request counter",
                         stReqCntOfstart - samplingStReqCntOfstart, totalCnt);
    rpt->ReportValAndPct("  |--" + name + "-->TileReg,load request counter",
                         ldReqCntOfEnd - samplingLdReqCntOfEnd, totalCnt);
    rpt->ReportValAndPct("  |--" + name + "-->TileReg,store request counter",
                         stReqCntOfEnd - samplingStReqCntOfEnd, totalCnt);
    rpt->ReportValAndPct("  |--TileReg-->" + name + ",load response counter",
                         ldRespCntOfStart - samplingLdRespCntOfStart, totalCnt);
    rpt->ReportValAndPct("  |--TileReg-->"+ name + ",store response counter",
                         stRespCntOfStart - samplingStRespCntOfStart, totalCnt);
    rpt->ReportValAndPct("  |--" + name + "-->" + nodeName + ",load response counter",
                         ldRespCntOfEnd - samplingLdRespCntOfEnd, totalCnt);
    rpt->ReportValAndPct("  |--" + name + "-->" + nodeName + ",store response counter",
                         stRespCntOfEnd - samplingStRespCntOfEnd, totalCnt);
    // size of request start/end from/at this station
    uint64_t channelWidth = (cycles - samplingCycles) * MAX_TILE_DATA_BYTE * LOG_2;
    rpt->ReportVal("Max request channel width", channelWidth);
    std::string tileName = "  |--" + nodeName + "-->" + name + ",load request size";
    rpt->ReportValAndPct(tileName, ldReqCntOfstartSize - samplingLdReqCntOfstartSize, channelWidth);
    tileName = "  |--" + nodeName + "-->" + name + ",store request size";
    rpt->ReportValAndPct(tileName, stReqCntOfstartSize - samplingStReqCntOfstartSize, channelWidth);
    tileName = "  |--" + name + "-->TileReg,load request size";
    rpt->ReportValAndPct(tileName, ldReqCntOfEndSize - samplingLdReqCntOfEndSize, channelWidth);
    tileName = "  |--" + name + "-->TileReg,store request size";
    rpt->ReportValAndPct(tileName, stReqCntOfEndSize - samplingStReqCntOfEndSize, channelWidth);
    // size of response start/end from/at this station
    rpt->ReportVal("Max response channel width", channelWidth);
    tileName = "  |--TileReg-->" + name + ",load response size";
    rpt->ReportValAndPct(tileName, ldRespCntOfStartSize - samplingLdRespCntOfStartSize, channelWidth);
    tileName = "  |--TileReg-->" + name + ",store response size";
    rpt->ReportValAndPct(tileName, stRespCntOfStartSize - samplingStRespCntOfStartSize, channelWidth);
    tileName = "  |--" + name + "-->" + nodeName + ",load response size";
    rpt->ReportValAndPct(tileName, ldRespCntOfEndSize - samplingLdRespCntOfEndSize, channelWidth);
    tileName = "  |--" + name + "-->" + nodeName + ",store response size";
    rpt->ReportValAndPct(tileName, stRespCntOfEndSize - samplingStRespCntOfEndSize, channelWidth);
    // latency from request to response
    rpt->ReportAvg("Average latency from request to response",
                   static_cast<float>(totalReq2RespLatency - samplingTotalReq2RespLatency),
                   static_cast<float>(ldRespCntOfEnd + stRespCntOfEnd - samplingLdRespCntOfEnd - stRespCntOfEnd));
    size_t idx = static_cast<size_t>(BufferType::LOAD_REQ0);
    rpt->ReportValAndPct("SPB of ld request occupied cycles",
                         spbOccupiedCycle[idx] - samplingSpbOccupiedCycle[idx],
                         cycles - samplingCycles);
    rpt->ReportValAndPct("SPB of ld request full cycles",
                         spbFullCycle[idx] - samplingSpbFullCycle[idx],
                         cycles - samplingCycles);
    rpt->ReportAvg("SPB of ld request average occupied size",
                   spbOccupiedSize[idx] - samplingSpbOccupiedSize[idx],
                   spbOccupiedCycle[idx] - samplingSpbOccupiedCycle[idx]);
    rpt->ReportValAndPct("MGB of ld request occupied cycles",
                         mgbOccupiedCycle[idx] - samplingMgbOccupiedCycle[idx],
                         cycles - samplingCycles);
    rpt->ReportValAndPct("MGB of ld request full cycles",
                         mgbFullCycle[idx] - samplingMgbFullCycle[idx],
                         cycles - samplingCycles);
    rpt->ReportAvg("MGB of ld request average occupied size",
                   mgbOccupiedSize[idx] - samplingMgbOccupiedSize[idx],
                   spbOccupiedCycle[idx] - samplingSpbOccupiedCycle[idx]);
    idx = static_cast<size_t>(BufferType::LOAD_DATA_RES);
    rpt->ReportValAndPct("SPB of ld data occupied cycles",
                         spbOccupiedCycle[idx] - samplingSpbOccupiedCycle[idx],
                         cycles - samplingCycles);
    rpt->ReportValAndPct("SPB of ld data full cycles",
                         spbFullCycle[idx] - samplingSpbFullCycle[idx],
                         cycles - samplingCycles);
    rpt->ReportAvg("SPB of ld data average occupied size",
                   spbOccupiedSize[idx] - samplingSpbOccupiedSize[idx],
                   spbOccupiedCycle[idx] - samplingSpbOccupiedCycle[idx]);
    rpt->ReportValAndPct("MGB of ld data occupied cycles",
                         mgbOccupiedCycle[idx] - samplingMgbOccupiedCycle[idx],
                         cycles - samplingCycles);
    rpt->ReportValAndPct("MGB of ld data full cycles",
                         mgbFullCycle[idx] - samplingMgbFullCycle[idx],
                         cycles - samplingCycles);
    rpt->ReportAvg("MGB of ld data average occupied size",
                   mgbOccupiedSize[idx] - samplingMgbOccupiedSize[idx],
                   spbOccupiedCycle[idx] - samplingSpbOccupiedCycle[idx]);
    idx = static_cast<size_t>(BufferType::STORE_DATA);
    rpt->ReportValAndPct("SPB of st data occupied cycles",
                         spbOccupiedCycle[idx] - samplingSpbOccupiedCycle[idx],
                         cycles - samplingCycles);
    rpt->ReportValAndPct("SPB of st data full cycles",
                         spbFullCycle[idx] - samplingSpbFullCycle[idx],
                         cycles - samplingCycles);
    rpt->ReportAvg("SPB of st data average occupied size",
                   spbOccupiedSize[idx] - samplingSpbOccupiedSize[idx],
                   spbOccupiedCycle[idx] - samplingSpbOccupiedCycle[idx]);
    rpt->ReportValAndPct("MGB of st data occupied cycles",
                         mgbOccupiedCycle[idx] - samplingMgbOccupiedCycle[idx],
                         cycles - samplingCycles);
    rpt->ReportValAndPct("MGB of st data full cycles",
                         mgbFullCycle[idx] - samplingMgbFullCycle[idx],
                         cycles - samplingCycles);
    rpt->ReportAvg("MGB of st data average occupied size",
                   mgbOccupiedSize[idx] - samplingMgbOccupiedSize[idx],
                   spbOccupiedCycle[idx] - samplingSpbOccupiedCycle[idx]);
    rpt->ReportVal("Read FRQ occupuied cycle", frqReadOccupiedCycle - samplingFrqReadOccupiedCycle);
    rpt->ReportValAndPct("Read FRQ full cycle", frqReadFullCycle - samplingFrqReadFullCycle, cycles - samplingCycles);
    rpt->ReportAvg("Read FRQ average occupied size", frqReadOccupiedSize - samplingFrqReadOccupiedSize,
                   frqReadOccupiedCycle - samplingFrqReadOccupiedCycle);
    rpt->ReportVal("Write FRQ occupuied cycle", frqWriteOccupiedCycle - samplingFrqWriteOccupiedCycle);
    rpt->ReportValAndPct("Write FRQ full cycle", frqWriteFullCycle - samplingFrqWriteFullCycle,
                         cycles - samplingCycles);
    rpt->ReportAvg("Write FRQ average occupied size", frqWriteOccupiedSize - samplingFrqWriteOccupiedSize,
                   frqWriteOccupiedCycle - samplingFrqWriteOccupiedCycle);
    rpt->ReportValAndPct("On ring cycles", onRingCycle - samplingOnRingCycle, cycles - samplingCycles);
    rpt->ReportValAndPct("Off ring cycles", offRingCycle - samplingOffRingCycle, cycles - samplingCycles);
}

} // namespace JCore
