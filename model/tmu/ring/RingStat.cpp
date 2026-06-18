#include "tmu/ring/RingStat.h"

namespace JCore {

void RingStat::Init(uint32_t nodeNum)
{
    numCubeLoadReqs.resize(nodeNum, 0);
    numCubeStoreReqs.resize(nodeNum, 0);
    numVectorLoadReqs.resize(nodeNum, 0);
    numVectorStoreReqs.resize(nodeNum, 0);
    numTMALoadReqs.resize(nodeNum, 0);
    numTMAStoreReqs.resize(nodeNum, 0);
    numPackets.resize(static_cast<size_t>(ChannelType::COUNT) + static_cast<size_t>(BufferType::COUNT), 0);
    sumIdealHops.resize(static_cast<size_t>(ChannelType::COUNT) + static_cast<size_t>(BufferType::COUNT), 0);
    sumRealHops.resize(static_cast<size_t>(ChannelType::COUNT) + static_cast<size_t>(BufferType::COUNT), 0);
    sumOnRingLatency.resize(static_cast<size_t>(ChannelType::COUNT) + static_cast<size_t>(BufferType::COUNT), 0);
    sumRingTraversalLatency.resize(static_cast<size_t>(ChannelType::COUNT) + static_cast<size_t>(BufferType::COUNT), 0);
    sumOffRingLatency.resize(static_cast<size_t>(ChannelType::COUNT) + static_cast<size_t>(BufferType::COUNT), 0);
    for (uint32_t i = 0; i < nodeNum; i++) {
        nodeOccuiedCycles.emplace_back(std::vector<uint64_t>(static_cast<size_t>(ChannelType::COUNT), 0));
        nodeOccuiedCubeCycle.emplace_back(std::vector<uint64_t>(static_cast<size_t>(ChannelType::COUNT), 0));
        nodeStashCycles.emplace_back(std::vector<uint64_t>(static_cast<size_t>(ChannelType::COUNT), 0));
        nodeOccuiedMtcCycle.emplace_back(std::vector<uint64_t>(static_cast<size_t>(ChannelType::COUNT), 0));
        nodeOccuiedVecCycle.emplace_back(std::vector<uint64_t>(static_cast<size_t>(ChannelType::COUNT), 0));
        linkOccuiedCycles.emplace_back(std::vector<uint64_t>(static_cast<size_t>(ChannelType::COUNT), 0));
        linkOccuiedCubeCycle.emplace_back(std::vector<uint64_t>(static_cast<size_t>(ChannelType::COUNT), 0));
        linkOccuiedMtcCycle.emplace_back(std::vector<uint64_t>(static_cast<size_t>(ChannelType::COUNT), 0));
        linkOccuiedVecCycle.emplace_back(std::vector<uint64_t>(static_cast<size_t>(ChannelType::COUNT), 0));
        linkMultiRoundOccuiedCycles.emplace_back(std::vector<uint64_t>(static_cast<size_t>(ChannelType::COUNT), 0));
        linkMultiRoundOccuiedCubeCycle.emplace_back(std::vector<uint64_t>(static_cast<size_t>(ChannelType::COUNT), 0));
        linkMultiRoundOccuiedMtcCycle.emplace_back(std::vector<uint64_t>(static_cast<size_t>(ChannelType::COUNT), 0));
        linkMultiRoundOccuiedVecCycle.emplace_back(std::vector<uint64_t>(static_cast<size_t>(ChannelType::COUNT), 0));
        offRingBackPressure.emplace_back(std::vector<uint64_t>(static_cast<size_t>(ChannelType::COUNT), 0));
        samplingNodeOccuiedCycles.emplace_back(std::vector<uint64_t>(static_cast<size_t>(ChannelType::COUNT), 0));
    }
}

void RingStat::Reset() {}

// This function should only be called by cw ring!
void RingStat::ReportBothRing(RingStat ccRingStat)
{
    rpt->ReportTitle("Ring Load/Store Statistics");
    uint64_t numCubeLoads = 0;
    uint64_t numCubeStores = 0;
    uint64_t numVectorLoads = 0;
    uint64_t numVectorStores = 0;
    uint64_t numTMALoads = 0;
    uint64_t numTMAStores = 0;
    for (size_t i = 0; i < numCubeLoadReqs.size(); ++i) {
        numCubeLoads += numCubeLoadReqs[i];
        numCubeStores += numCubeStoreReqs[i];
        numVectorLoads += numVectorLoadReqs[i];
        numVectorStores += numVectorStoreReqs[i];
        numTMALoads += numTMALoadReqs[i];
        numTMAStores += numTMAStoreReqs[i];
    }
    rpt->ReportVal("Number of cube loads", numCubeLoads);
    size_t cubeIdx = 0;
    for (size_t i = 0; i < numCubeLoadReqs.size(); ++i) {
        if (nodeToPEMap[i].find("Cube") == std::string::npos ||
            nodeToPEMap[i].find("Read") == std::string::npos) {
            continue;
        }
        rpt->ReportVal("  |--" + nodeToPEMap[i] + " loads", numCubeLoadReqs[i]);
        ++cubeIdx;
    }
    rpt->ReportVal("Number of cube stores", numCubeStores);
    cubeIdx = 0;
    for (size_t i = 0; i < numCubeStoreReqs.size(); ++i) {
        if (nodeToPEMap[i].find("Cube") == std::string::npos ||
            nodeToPEMap[i].find("Write") == std::string::npos) {
            continue;
        }
        rpt->ReportVal("  |--" + nodeToPEMap[i] + " stores", numCubeStoreReqs[i]);
        ++cubeIdx;
    }
    rpt->ReportVal("Number of vector loads", numVectorLoads);
    size_t vectorPortIdx = 0;
    for (size_t i = 0; i < numVectorLoadReqs.size(); ++i) {
        if (nodeToPEMap[i].find("Vector") == std::string::npos ||
            nodeToPEMap[i].find("Read") == std::string::npos) {
            continue;
        }
        rpt->ReportVal("  |--" + nodeToPEMap[i] + " loads", numVectorLoadReqs[i]);
        ++vectorPortIdx;
    }
    rpt->ReportVal("Number of vector stores", numVectorStores);
    vectorPortIdx = 0;
    for (size_t i = 0; i < numVectorStoreReqs.size(); ++i) {
        if (nodeToPEMap[i].find("Vector") == std::string::npos ||
            nodeToPEMap[i].find("Write") == std::string::npos) {
            continue;
        }
        rpt->ReportVal("  |--" + nodeToPEMap[i] + " stores", numVectorStoreReqs[i]);
        ++vectorPortIdx;
    }
    rpt->ReportVal("Number of TMA loads", numTMALoads);
    size_t tmaPortIdx = 0;
    for (size_t i = 0; i < numTMALoadReqs.size(); ++i) {
        if (nodeToPEMap[i].find("TMA") == std::string::npos ||
            nodeToPEMap[i].find("Read") == std::string::npos) {
            continue;
        }
        rpt->ReportVal("  |--" + nodeToPEMap[i] + " loads", numTMALoadReqs[i]);
        ++tmaPortIdx;
    }
    rpt->ReportVal("Number of TMA stores", numTMAStores);
    tmaPortIdx = 0;
    for (size_t i = 0; i < numTMAStoreReqs.size(); ++i) {
        if (nodeToPEMap[i].find("TMA") == std::string::npos ||
            nodeToPEMap[i].find("Write") == std::string::npos) {
            continue;
        }
        rpt->ReportVal("  |--" + nodeToPEMap[i] + " stores", numTMAStoreReqs[i]);
        ++tmaPortIdx;
    }
    rpt->ReportTitle("Request 0 Channel Ring Statistics");
    size_t idx = static_cast<size_t>(ChannelType::REQ0);
    uint64_t numTotalPackets = numPackets[idx] + ccRingStat.numPackets[idx];
    uint64_t sumLatencyCW = sumOnRingLatency[idx] + sumRingTraversalLatency[idx] + sumOffRingLatency[idx];
    uint64_t sumLatencyCC = ccRingStat.sumOnRingLatency[idx] + ccRingStat.sumRingTraversalLatency[idx] +
                            ccRingStat.sumOffRingLatency[idx];
    rpt->ReportVal("Number of packets", numTotalPackets);
    rpt->ReportVal("  |--Average latency", float(sumLatencyCW + sumLatencyCC) / numTotalPackets);
    rpt->ReportVal("    |--Average on-ring latency",
        float(sumOnRingLatency[idx] + ccRingStat.sumOnRingLatency[idx]) / numTotalPackets);
    rpt->ReportVal("    |--Average ring-traversal latency",
        float(sumRingTraversalLatency[idx] + ccRingStat.sumRingTraversalLatency[idx]) / numTotalPackets);
    rpt->ReportVal("    |--Average off-ring latency",
        float(sumOffRingLatency[idx] + ccRingStat.sumOffRingLatency[idx]) / numTotalPackets);
    rpt->ReportVal("  |--Average ideal hops",
        float(sumIdealHops[idx] + ccRingStat.sumIdealHops[idx]) / numTotalPackets);
    rpt->ReportVal("  |--Average real hops", float(sumRealHops[idx] + ccRingStat.sumRealHops[idx]) / numTotalPackets);
    rpt->ReportVal("Number of packets (CW)", numPackets[idx]);
    rpt->ReportVal("  |--Average latency (CW)", float(sumLatencyCW) / numPackets[idx]);
    rpt->ReportVal("    |--Average on-ring latency (CW)", float(sumOnRingLatency[idx]) / numPackets[idx]);
    rpt->ReportVal("    |--Average ring-traversal latency (CW)",
        float(sumRingTraversalLatency[idx]) / numPackets[idx]);
    rpt->ReportVal("    |--Average off-ring latency (CW)", float(sumOffRingLatency[idx]) / numPackets[idx]);
    rpt->ReportVal("  |--Average ideal hops (CW)", float(sumIdealHops[idx]) / numPackets[idx]);
    rpt->ReportVal("  |--Average real hops (CW)", float(sumRealHops[idx]) / numPackets[idx]);
    rpt->ReportVal("Number of packets (CC)", ccRingStat.numPackets[idx]);
    rpt->ReportVal("  |--Average latency (CC)", float(sumLatencyCC) / ccRingStat.numPackets[idx]);
    rpt->ReportVal("    |--Average on-ring latency (CC)",
        float(ccRingStat.sumOnRingLatency[idx]) / ccRingStat.numPackets[idx]);
    rpt->ReportVal("    |--Average ring-traversal latency (CC)",
        float(ccRingStat.sumRingTraversalLatency[idx]) / ccRingStat.numPackets[idx]);
    rpt->ReportVal("    |--Average off-ring latency (CC)",
        float(ccRingStat.sumOffRingLatency[idx]) / ccRingStat.numPackets[idx]);
    rpt->ReportVal("  |--Average ideal hops (CC)", float(ccRingStat.sumIdealHops[idx]) / ccRingStat.numPackets[idx]);
    rpt->ReportVal("  |--Average real hops (CC)", float(ccRingStat.sumRealHops[idx]) / ccRingStat.numPackets[idx]);

    rpt->ReportTitle("Data Channel Statistics");
    idx = static_cast<size_t>(ChannelType::DATA);
    numTotalPackets = numPackets[idx] + ccRingStat.numPackets[idx];
    sumLatencyCW = sumOnRingLatency[idx] + sumRingTraversalLatency[idx] + sumOffRingLatency[idx];
    sumLatencyCC = ccRingStat.sumOnRingLatency[idx] + ccRingStat.sumRingTraversalLatency[idx] +
                   ccRingStat.sumOffRingLatency[idx];
    rpt->ReportVal("Number of packets", numTotalPackets);
    rpt->ReportVal("  |--Average latency", float(sumLatencyCW + sumLatencyCC) / numTotalPackets);
    rpt->ReportVal("    |--Average on-ring latency",
        float(sumOnRingLatency[idx] + ccRingStat.sumOnRingLatency[idx]) / numTotalPackets);
    rpt->ReportVal("    |--Average ring-traversal latency",
        float(sumRingTraversalLatency[idx] + ccRingStat.sumRingTraversalLatency[idx]) / numTotalPackets);
    rpt->ReportVal("    |--Average off-ring latency",
        float(sumOffRingLatency[idx] + ccRingStat.sumOffRingLatency[idx]) / numTotalPackets);
    rpt->ReportVal("  |--Average ideal hops",
        float(sumIdealHops[idx] + ccRingStat.sumIdealHops[idx]) / numTotalPackets);
    rpt->ReportVal("  |--Average real hops",
        float(sumRealHops[idx] + ccRingStat.sumRealHops[idx]) / numTotalPackets);
    rpt->ReportVal("Number of packets (CW)", numPackets[idx]);
    rpt->ReportVal("  |--Average latency (CW)", float(sumLatencyCW) / numPackets[idx]);
    rpt->ReportVal("    |--Average on-ring latency (CW)", float(sumOnRingLatency[idx]) / numPackets[idx]);
    rpt->ReportVal("    |--Average ring-traversal latency (CW)",
        float(sumRingTraversalLatency[idx]) / numPackets[idx]);
    rpt->ReportVal("    |--Average off-ring latency (CW)", float(sumOffRingLatency[idx]) / numPackets[idx]);
    rpt->ReportVal("  |--Average ideal hops (CW)", float(sumIdealHops[idx]) / numPackets[idx]);
    rpt->ReportVal("  |--Average real hops (CW)", float(sumRealHops[idx]) / numPackets[idx]);
    rpt->ReportVal("Number of packets (CC)", ccRingStat.numPackets[idx]);
    rpt->ReportVal("  |--Average latency (CC)", float(sumLatencyCC) / ccRingStat.numPackets[idx]);
    rpt->ReportVal("    |--Average on-ring latency (CC)",
        float(ccRingStat.sumOnRingLatency[idx]) / ccRingStat.numPackets[idx]);
    rpt->ReportVal("    |--Average ring-traversal latency (CC)",
        float(ccRingStat.sumRingTraversalLatency[idx]) / ccRingStat.numPackets[idx]);
    rpt->ReportVal("    |--Average off-ring latency (CC)",
        float(ccRingStat.sumOffRingLatency[idx]) / ccRingStat.numPackets[idx]);
    rpt->ReportVal("  |--Average ideal hops (CC)", float(ccRingStat.sumIdealHops[idx]) / ccRingStat.numPackets[idx]);
    rpt->ReportVal("  |--Average real hops (CC)", float(ccRingStat.sumRealHops[idx]) / ccRingStat.numPackets[idx]);
    idx = static_cast<size_t>(ChannelType::COUNT) + static_cast<size_t>(BufferType::LOAD_DATA_RES);
    numTotalPackets = numPackets[idx] + ccRingStat.numPackets[idx];
    sumLatencyCW = sumOnRingLatency[idx] + sumRingTraversalLatency[idx] + sumOffRingLatency[idx];
    sumLatencyCC = ccRingStat.sumOnRingLatency[idx] + ccRingStat.sumRingTraversalLatency[idx] +
                   ccRingStat.sumOffRingLatency[idx];
    rpt->ReportVal("Number of packets (Load-Data)", numTotalPackets);
    rpt->ReportVal("  |--Average latency (Load-Data)", float(sumLatencyCW + sumLatencyCC) / numTotalPackets);
    rpt->ReportVal("    |--Average on-ring latency (Load-Data)",
        float(sumOnRingLatency[idx] + ccRingStat.sumOnRingLatency[idx]) / numTotalPackets);
    rpt->ReportVal("    |--Average ring-traversal latency (Load-Data)",
        float(sumRingTraversalLatency[idx] + ccRingStat.sumRingTraversalLatency[idx]) / numTotalPackets);
    rpt->ReportVal("    |--Average off-ring latency (Load-Data)",
        float(sumOffRingLatency[idx] + ccRingStat.sumOffRingLatency[idx]) / numTotalPackets);
    rpt->ReportVal("  |--Average ideal hops (Load-Data)",
        float(sumIdealHops[idx] + ccRingStat.sumIdealHops[idx]) / numTotalPackets);
    rpt->ReportVal("  |--Average real hops (Load-Data)",
        float(sumRealHops[idx] + ccRingStat.sumRealHops[idx]) / numTotalPackets);
    idx = static_cast<size_t>(ChannelType::COUNT) + static_cast<size_t>(BufferType::STORE_DATA);
    numTotalPackets = numPackets[idx] + ccRingStat.numPackets[idx];
    sumLatencyCW = sumOnRingLatency[idx] + sumRingTraversalLatency[idx] + sumOffRingLatency[idx];
    sumLatencyCC = ccRingStat.sumOnRingLatency[idx] + ccRingStat.sumRingTraversalLatency[idx] +
                   ccRingStat.sumOffRingLatency[idx];
    rpt->ReportVal("Number of packets (Store-Data)", numTotalPackets);
    rpt->ReportVal("  |--Average latency (Store-Data)", float(sumLatencyCW + sumLatencyCC) / numTotalPackets);
    rpt->ReportVal("    |--Average on-ring latency (Store-Data)",
        float(sumOnRingLatency[idx] + ccRingStat.sumOnRingLatency[idx]) / numTotalPackets);
    rpt->ReportVal("    |--Average ring-traversal latency (Store-Data)",
        float(sumRingTraversalLatency[idx] + ccRingStat.sumRingTraversalLatency[idx]) / numTotalPackets);
    rpt->ReportVal("    |--Average off-ring latency (Store-Data)",
        float(sumOffRingLatency[idx] + ccRingStat.sumOffRingLatency[idx]) / numTotalPackets);
    rpt->ReportVal("  |--Average ideal hops (Load-Data)",
        float(sumIdealHops[idx] + ccRingStat.sumIdealHops[idx]) / numTotalPackets);
    rpt->ReportVal("  |--Average real hops (Load-Data)",
        float(sumRealHops[idx] + ccRingStat.sumRealHops[idx]) / numTotalPackets);

    rpt->ReportTitle("Resp Channel Statistics");
    idx = static_cast<size_t>(ChannelType::RSP);
    numTotalPackets = numPackets[idx] + ccRingStat.numPackets[idx];
    sumLatencyCW = sumOnRingLatency[idx] + sumRingTraversalLatency[idx] + sumOffRingLatency[idx];
    sumLatencyCC = ccRingStat.sumOnRingLatency[idx] + ccRingStat.sumRingTraversalLatency[idx] +
                   ccRingStat.sumOffRingLatency[idx];
    rpt->ReportVal("Number of packets", numTotalPackets);
    rpt->ReportVal("  |--Average latency", float(sumLatencyCW + sumLatencyCC) / numTotalPackets);
    rpt->ReportVal("    |--Average on-ring latency",
        float(sumOnRingLatency[idx] + ccRingStat.sumOnRingLatency[idx]) / numTotalPackets);
    rpt->ReportVal("    |--Average ring-traversal latency",
        float(sumRingTraversalLatency[idx] + ccRingStat.sumRingTraversalLatency[idx]) / numTotalPackets);
    rpt->ReportVal("    |--Average off-ring latency",
        float(sumOffRingLatency[idx] + ccRingStat.sumOffRingLatency[idx]) / numTotalPackets);
    rpt->ReportVal("  |--Average ideal hops",
        float(sumIdealHops[idx] + ccRingStat.sumIdealHops[idx]) / numTotalPackets);
    rpt->ReportVal("  |--Average real hops", float(sumRealHops[idx] + ccRingStat.sumRealHops[idx]) / numTotalPackets);
    rpt->ReportVal("Number of packets (CW)", numPackets[idx]);
    rpt->ReportVal("  |--Average latency (CW)", float(sumLatencyCW) / numPackets[idx]);
    rpt->ReportVal("    |--Average on-ring latency (CW)", float(sumOnRingLatency[idx]) / numPackets[idx]);
    rpt->ReportVal("    |--Average ring-traversal latency (CW)",
        float(sumRingTraversalLatency[idx]) / numPackets[idx]);
    rpt->ReportVal("    |--Average off-ring latency (CW)", float(sumOffRingLatency[idx]) / numPackets[idx]);
    rpt->ReportVal("  |--Average ideal hops (CW)", float(sumIdealHops[idx]) / numPackets[idx]);
    rpt->ReportVal("  |--Average real hops (CW)", float(sumRealHops[idx]) / numPackets[idx]);
    rpt->ReportVal("Number of packets (CC)", ccRingStat.numPackets[idx]);
    rpt->ReportVal("  |--Average latency (CC)", float(sumLatencyCC) / ccRingStat.numPackets[idx]);
    rpt->ReportVal("    |--Average on-ring latency (CC)",
        float(ccRingStat.sumOnRingLatency[idx]) / ccRingStat.numPackets[idx]);
    rpt->ReportVal("    |--Average ring-traversal latency (CC)",
        float(ccRingStat.sumRingTraversalLatency[idx]) / ccRingStat.numPackets[idx]);
    rpt->ReportVal("    |--Average off-ring latency (CC)",
        float(ccRingStat.sumOffRingLatency[idx]) / ccRingStat.numPackets[idx]);
    rpt->ReportVal("  |--Average ideal hops (CC)", float(ccRingStat.sumIdealHops[idx]) / ccRingStat.numPackets[idx]);
    rpt->ReportVal("  |--Average real hops (CC)", float(ccRingStat.sumRealHops[idx]) / ccRingStat.numPackets[idx]);
}

void RingStat::Report(std::string &name)
{
    rpt->ReportTitle(name + " statistics(node)");
    rpt->ReportAvg("Average round cycle on ring of request or response", static_cast<float>(totalRingCycle),
                   static_cast<float>(totalPktCnt));
    for (uint32_t i = 0; i < nodeOccuiedCycles.size(); i++) {
        std::string nodeName = "Node " + std::to_string(i);
        size_t channel = static_cast<size_t>(ChannelType::REQ0);
        rpt->ReportValAndPct(nodeName + ",request 0 channel work",
                             static_cast<float>(nodeOccuiedCycles[i][channel]),
                             cycles);
        rpt->ReportValAndPct("  |--stash request",
                             static_cast<float>(nodeStashCycles[i][channel]),
                             nodeOccuiedCycles[i][channel]);
        rpt->ReportValAndPct(nodeName + ",request 0 channel work(cube runtime)",
                             static_cast<float>(nodeOccuiedCubeCycle[i][channel]),
                             totalCubeRunningCycle);
        rpt->ReportValAndPct(nodeName + ",request 0 channel work(mtc runtime)",
                             static_cast<float>(nodeOccuiedMtcCycle[i][channel]),
                             totalMtcRunningCycle);
        rpt->ReportValAndPct(nodeName + ",request 0 channel work(vec runtime)",
                             static_cast<float>(nodeOccuiedVecCycle[i][channel]),
                             totalVecRunningCycle);
    }
    for (uint32_t i = 0; i < nodeOccuiedCycles.size(); i++) {
        std::string nodeName = "Node " + std::to_string(i);
        size_t channel = static_cast<size_t>(ChannelType::DATA);
        rpt->ReportValAndPct(nodeName + ",data channel work",
                             static_cast<float>(nodeOccuiedCycles[i][channel]),
                             cycles);
        rpt->ReportValAndPct("  |--stash request",
                             static_cast<float>(nodeStashCycles[i][channel]),
                             nodeOccuiedCycles[i][channel]);
        rpt->ReportValAndPct(nodeName + ",data channel work(cube runtime)",
                             static_cast<float>(nodeOccuiedCubeCycle[i][channel]),
                             totalCubeRunningCycle);
        rpt->ReportValAndPct(nodeName + ",data channel work(mtc runtime)",
                             static_cast<float>(nodeOccuiedMtcCycle[i][channel]),
                             totalMtcRunningCycle);
        rpt->ReportValAndPct(nodeName + ",data channel work(vec runtime)",
                             static_cast<float>(nodeOccuiedVecCycle[i][channel]),
                             totalVecRunningCycle);
    }
    rpt->ReportTitle(name + " statistics(link)");
    for (uint32_t i = 0; i < linkOccuiedCycles.size(); i++) {
        std::string linkName = "Node ";
        if (name == "Clockwise Ring") {
            linkName += std::to_string(i) + "->" + std::to_string((i + 1) % linkOccuiedCycles.size());
        } else {
            linkName += std::to_string((i + linkOccuiedCycles.size() - 1) % linkOccuiedCycles.size())
                        + "->" + std::to_string(i);
        }
        size_t channel = static_cast<size_t>(ChannelType::REQ0);
        rpt->ReportValAndPct(linkName + ",request 0 channel work",
                             static_cast<float>(linkOccuiedCycles[i][channel]),
                             cycles);
        rpt->ReportValAndPct("  |--More than one round",
                             static_cast<float>(linkMultiRoundOccuiedCycles[i][channel]),
                             linkOccuiedCycles[i][channel]);
        rpt->ReportValAndPct(linkName + ",request 0 channel work(cube runtime)",
                             static_cast<float>(linkOccuiedCubeCycle[i][channel]),
                             cycles);
        rpt->ReportValAndPct("  |--More than one round",
                             static_cast<float>(linkMultiRoundOccuiedCubeCycle[i][channel]),
                             linkOccuiedCubeCycle[i][channel]);
        rpt->ReportValAndPct(linkName + ",request 0 channel work(mtc runtime)",
                             static_cast<float>(linkOccuiedMtcCycle[i][channel]),
                             cycles);
        rpt->ReportValAndPct("  |--More than one round",
                             static_cast<float>(linkMultiRoundOccuiedMtcCycle[i][channel]),
                             linkOccuiedMtcCycle[i][channel]);
        rpt->ReportValAndPct(linkName + ",request 0 channel work(vec runtime)",
                             static_cast<float>(linkOccuiedVecCycle[i][channel]),
                             cycles);
        rpt->ReportValAndPct("  |--More than one round",
                             static_cast<float>(linkMultiRoundOccuiedVecCycle[i][channel]),
                             linkOccuiedVecCycle[i][channel]);
    }
    for (uint32_t i = 0; i < linkOccuiedCycles.size(); i++) {
        std::string linkName = "Node ";
        if (name == "Clockwise Ring") {
            linkName += std::to_string(i) + "->" + std::to_string((i + 1) % linkOccuiedCycles.size());
        } else {
            linkName += std::to_string((i + linkOccuiedCycles.size() - 1) % linkOccuiedCycles.size())
                        + "->" + std::to_string(i);
        }
        size_t channel = static_cast<size_t>(ChannelType::DATA);
        rpt->ReportValAndPct(linkName + ",data channel work",
                             static_cast<float>(linkOccuiedCycles[i][channel]),
                             cycles);
        rpt->ReportValAndPct("  |--More than one round",
                             static_cast<float>(linkMultiRoundOccuiedCycles[i][channel]),
                             linkOccuiedCycles[i][channel]);
        rpt->ReportValAndPct(linkName + ",data channel work(cube runtime)",
                             static_cast<float>(linkOccuiedCubeCycle[i][channel]),
                             cycles);
        rpt->ReportValAndPct("  |--More than one round",
                             static_cast<float>(linkMultiRoundOccuiedCubeCycle[i][channel]),
                             linkOccuiedCubeCycle[i][channel]);
        rpt->ReportValAndPct(linkName + ",data channel work(mtc runtime)",
                             static_cast<float>(linkOccuiedMtcCycle[i][channel]),
                             cycles);
        rpt->ReportValAndPct("  |--More than one round",
                             static_cast<float>(linkMultiRoundOccuiedMtcCycle[i][channel]),
                             linkOccuiedMtcCycle[i][channel]);
        rpt->ReportValAndPct(linkName + ",data channel work(vec runtime)",
                             static_cast<float>(linkOccuiedVecCycle[i][channel]),
                             cycles);
        rpt->ReportValAndPct("  |--More than one round",
                             static_cast<float>(linkMultiRoundOccuiedVecCycle[i][channel]),
                             linkOccuiedVecCycle[i][channel]);
    }
}

void RingStat::SamplingStash()
{
    samplingCycles = cycles;
    samplingTotalPktCnt = totalPktCnt;
    samplingTotalRingCycle = totalRingCycle;
    samplingTotalLdReqPktCnt = totalLdReqPktCnt;
    samplingTotalLdReqRespRingCycle = totalLdReqRespRingCycle;
    samplingNodeOccuiedCycles = nodeOccuiedCycles;
}

void RingStat::ReportSampling(std::string name)
{
    rpt->ReportTitle(name + " statistics");
    rpt->ReportAvg("Average round cycle on ring of request or response",
                   static_cast<float>(totalRingCycle - samplingTotalRingCycle),
                   static_cast<float>(totalPktCnt - samplingTotalPktCnt));
    for (uint32_t i = 0; i < nodeOccuiedCycles.size(); i++) {
        std::string nodeName = "Node " + std::to_string(i);
        size_t channel = static_cast<size_t>(ChannelType::REQ0);
        rpt->ReportValAndPct(nodeName + " work on ring of request 0 channel",
                             static_cast<float>(nodeOccuiedCycles[i][channel]
                             - samplingNodeOccuiedCycles[i][channel]),
                             cycles - samplingCycles);
        channel = static_cast<size_t>(ChannelType::DATA);
        rpt->ReportValAndPct(nodeName + " work on ring of data channel",
                             static_cast<float>(nodeOccuiedCycles[i][channel]
                             - samplingNodeOccuiedCycles[i][channel]),
                             cycles - samplingCycles);
    }
}

} // namespace JCore
