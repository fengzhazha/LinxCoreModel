#include "tmu/ring/Ring.h"

#include <algorithm>
#include <cstdlib>

namespace JCore {
using namespace std;
Ring::Ring(CrossStation::Direction direction, const std::vector<std::shared_ptr<CrossStation>>& nodes)
    : direction(direction), nodes(nodes)
{
    stat.Init(nodes.size());
}

SimSys* Ring::GetSim()
{
    return sim;
}

void Ring::Build()
{
    configs.overrideDefaultConfig(GetSim()->getCfgs());
    stat.rpt = GetSim()->getRpt();
    nodesAlive = vector<vector<BlockCommandPtr>>(pTileUtils->GetNodeNum(),
                 vector<BlockCommandPtr>(static_cast<size_t>(ChannelType::COUNT), nullptr));
}

void Ring::Reset() {}

void Ring::Xfer() {}

void Ring::SetVerboseOn()
{
    verbose = true;
}

void Ring::Work()
{
    stat.totalCubeRunningCycle += sim->cubeRunning;
    stat.totalMtcRunningCycle += sim->mtcRunning;
    stat.totalVecRunningCycle += sim->vecRunning;
    for (size_t channel = 0; channel < static_cast<size_t>(ChannelType::COUNT); ++channel) {
        RptStats(channel);
        RingDelivery(channel);
    }
    stat.cycles++;
}

void Ring::RptStats(size_t channel)
{
    for (size_t i = 0; i < nodes.size(); ++i) {
        auto &node = nodes[i];
        auto &ringState = direction == CrossStation::Direction::CLOCKWISE ? node->GetCWRingState()
                                                                          : node->GetCCRingState();
        stat.nodeOccuiedCycles[i][channel] += (ringState.channelRequests[channel] != nullptr);
        if (ringState.channelRequests[channel] != nullptr) {
            if (ringState.channelRequests[channel]->IsStash()) {
                stat.nodeStashCycles[i][channel]++;
            }
        }
        if (sim->cubeRunning) {
            stat.nodeOccuiedCubeCycle[i][channel] += (ringState.channelRequests[channel] != nullptr);
        }
        if (sim->mtcRunning) {
            stat.nodeOccuiedMtcCycle[i][channel] += (ringState.channelRequests[channel] != nullptr);
        }
        if (sim->vecRunning) {
            stat.nodeOccuiedVecCycle[i][channel] += (ringState.channelRequests[channel] != nullptr);
        }
        if (ringState.channelRequests[channel] == nullptr) {
            if (nodesAlive[i][channel] != nullptr) {
                nodesAlive[i][channel]->tileOp = GetRingTileOp(nodesAlive[i][channel]);
                AddSwimLane(nodesAlive[i][channel]);
                nodesAlive[i][channel] = nullptr;
            }
        } else {
            if (nodesAlive[i][channel] == nullptr) {
                nodesAlive[i][channel] = make_shared<BlockCommand>();
                nodesAlive[i][channel]->execMachineId = machineId
                                                        + pTileUtils->GetRingThreadID(i,
                                                        static_cast<ChannelType>(channel));
                nodesAlive[i][channel]->startCalCycle = GetSim()->getCycles();
            }
        }
    }
}

TileOp Ring::GetRingTileOp(BlockCommandPtr cmd)
{
    uint64_t continousTime = GetSim()->getCycles() - cmd->startCalCycle;
    if (continousTime < configs.ring_swim_continous_time_1) {
        return TileOp::TRING;
    }
    if (continousTime < configs.ring_swim_continous_time_2) {
        return TileOp::TRING_TIME_1;
    }
    if (continousTime < configs.ring_swim_continous_time_3) {
        return TileOp::TRING_TIME_2;
    }
    if (continousTime < configs.ring_swim_continous_time_4) {
        return TileOp::TRING_TIME_3;
    }
    return TileOp::TRING_TIME_4;
}

void Ring::ForwardReq(size_t nodeId, const std::shared_ptr<Request> req, size_t channel)
{
    auto &node = nodes[nodeId];
    auto &ringState = direction == CrossStation::Direction::CLOCKWISE ? node->GetCWRingState()
                                                                      : node->GetCCRingState();
    size_t nextNodeId = GetNextNodeId(nodeId);
    auto &nextNode = nodes[nextNodeId];
    auto &nextRingState = direction == CrossStation::Direction::CLOCKWISE ? nextNode->GetCWRingState()
                                                                          : nextNode->GetCCRingState();
    if (ShouldDropAtNode(req, nodeId) && ringState.requestDownStash[channel] == nullptr) {
        req->offRingTime = GetSim()->getCycles();
        stat.totalPktCnt++;
        size_t bufferIdx = static_cast<size_t>(ChannelType::COUNT) + static_cast<size_t>(req->GetBufferType());
        stat.numPackets[channel]++;
        stat.numPackets[bufferIdx]++;
        stat.sumIdealHops[channel] += req->GetIdealHops();
        stat.sumIdealHops[bufferIdx] += req->GetIdealHops();
        stat.sumRealHops[channel] += req->GetRealHops();
        stat.sumRealHops[bufferIdx] += req->GetRealHops();
        ASSERT(req->GetRealHops() >= req->GetIdealHops());
        ASSERT((req->offRingTime - req->onRingTime) == (req->GetRealHops() + 0));
        stat.sumRingTraversalLatency[channel] += (req->offRingTime - req->onRingTime);
        stat.sumRingTraversalLatency[bufferIdx] += (req->offRingTime - req->onRingTime);
        stat.totalRingCycle += (req->offRingTime - req->onRingTime);
        ringState.requestDownStash[channel] = req;
        std::string roundName = direction == CrossStation::Direction::CLOCKWISE ? "[Ring stage]: CW Ring"
                                                                                : "[Ring stage]: CC Ring";
        LOG_INFO_M(Unit::TMU, Stage::RING) << roundName << " Node " << nodeId << " dropped "
                                            << *req << " on channel " << channel;
        return;
    }

    nextRingState.nextChannelRequests[channel] = req;
    req->IncRealHops();
}

void Ring::RingDelivery(size_t channel)
{
    for (size_t i = 0; i < nodes.size(); ++i) {
        auto &node = nodes[i];
        auto &ringState = direction == CrossStation::Direction::CLOCKWISE ? node->GetCWRingState()
                                                                          : node->GetCCRingState();
        ringState.channelRequests[channel] = ringState.nextChannelRequests[channel];
    }

    size_t nodeId = 0;
    std::shared_ptr<Request> curRequest = nullptr;
    for (size_t i = 0; i < nodes.size(); ++i) {
        auto &node = nodes[nodeId];
        auto &ringState = direction == CrossStation::Direction::CLOCKWISE ? node->GetCWRingState()
                                                                          : node->GetCCRingState();
        std::shared_ptr<Request> curRequest = ringState.channelRequests[channel];
        size_t nextNodeId = GetNextNodeId(nodeId);
        auto &nextNode = nodes[nextNodeId];
        auto &nextRingState = direction == CrossStation::Direction::CLOCKWISE ? nextNode->GetCWRingState()
                                                                              : nextNode->GetCCRingState();
        if (curRequest == nullptr) {
            // Delivery none packet to next station
            nextRingState.nextChannelRequests[channel] = nullptr;
        } else if (ShouldDropAtNode(curRequest, nodeId) && ringState.requestDownStash[channel] == nullptr) {
            // Drop the request at the staion
            curRequest->offRingTime = GetSim()->getCycles();
            stat.totalPktCnt++;
            size_t bufferIdx = static_cast<size_t>(ChannelType::COUNT) +
                               static_cast<size_t>(curRequest->GetBufferType());
            stat.numPackets[channel]++;
            stat.numPackets[bufferIdx]++;
            stat.sumIdealHops[channel] += curRequest->GetIdealHops();
            stat.sumIdealHops[bufferIdx] += curRequest->GetIdealHops();
            stat.sumRealHops[channel] += curRequest->GetRealHops();
            stat.sumRealHops[bufferIdx] += curRequest->GetRealHops();
            stat.sumRingTraversalLatency[channel] += (curRequest->offRingTime - curRequest->onRingTime);
            stat.sumRingTraversalLatency[bufferIdx] += (curRequest->offRingTime - curRequest->onRingTime);
            stat.totalRingCycle += (curRequest->offRingTime - curRequest->onRingTime);
            nextRingState.nextChannelRequests[channel] = nullptr;
            ringState.requestDownStash[channel] = curRequest;
            if (configs.concurrent_on_off_ring) {
                ringState.channelRequests[channel] = nullptr;
            }
            std::string roundName = direction == CrossStation::Direction::CLOCKWISE ? "[Ring stage]: CW Ring"
                                                                                    : "[Ring stage]: CC Ring";
            LOG_INFO_M(Unit::TMU, Stage::RING) << roundName << " Node " << nodeId << " dropped "
                                               << *curRequest << " on channel " << channel;
        } else {
            // Delivery packet to next station
            stat.offRingBackPressure[nodeId][static_cast<size_t>(curRequest->GetChannel())]
                += ShouldDropAtNode(curRequest, nodeId);
            nextRingState.nextChannelRequests[channel] = curRequest;
            ASSERT(ringState.channelRequests[channel] != nullptr);
            curRequest->IncRealHops();
            stat.linkOccuiedCycles[nodeId][channel]++;
            stat.linkMultiRoundOccuiedCycles[nodeId][channel] += ((sim->getCycles() - curRequest->onRingTime)
                                                                  > curRequest->CalcDistance(configs.ring_node_num));
            if (sim->cubeRunning) {
                stat.linkOccuiedCubeCycle[nodeId][channel]++;
                stat.linkMultiRoundOccuiedCubeCycle[nodeId][channel] += ((sim->getCycles() - curRequest->onRingTime)
                                                                  > curRequest->CalcDistance(configs.ring_node_num));
            }
            if (sim->mtcRunning) {
                stat.linkOccuiedMtcCycle[nodeId][channel]++;
                stat.linkMultiRoundOccuiedMtcCycle[nodeId][channel] += ((sim->getCycles() - curRequest->onRingTime)
                                                                  > curRequest->CalcDistance(configs.ring_node_num));
            }
            if (sim->vecRunning) {
                stat.linkOccuiedVecCycle[nodeId][channel]++;
                stat.linkMultiRoundOccuiedVecCycle[nodeId][channel] += ((sim->getCycles() - curRequest->onRingTime)
                                                                  > curRequest->CalcDistance(configs.ring_node_num));
            }
            std::string roundName = direction == CrossStation::Direction::CLOCKWISE ? "[Ring stage]: CW Ring"
                                                                                    : "[Ring stage]: CC Ring";
            LOG_INFO_M(Unit::TMU, Stage::RING) << roundName << " Node " << nodeId << " send " << *curRequest
                                               << " on channel " << channel << " to Node " << nextNodeId;
        }
        nodeId = nextNodeId;
    }
}

void Ring::RouteRequest(const std::shared_ptr<Request>& req, uint32_t currentNodeId)
{
    uint32_t destNode = req->GetArcTgtNode();
    CrossStation::Direction route = DetermineRoute(currentNodeId, destNode);
    if (route == direction) {
        uint32_t nextNodeId = GetNextNodeId(currentNodeId);
        nodes[nextNodeId]->ReceiveRequest(req);
    }
}

uint32_t Ring::GetNextNodeId(uint32_t currentNodeId) const
{
    // Node connections: 0->1->3->5->7->6->4->2->0
    //               id: 0->1->2->3->4->5->6->7->0
    if (direction == CrossStation::Direction::CLOCKWISE) {
        return (currentNodeId + 1) % nodes.size();
    } else {
        return (currentNodeId + nodes.size() - 1) % nodes.size();
    }
}

bool Ring::ShouldDropAtNode(const std::shared_ptr<Request>& req, uint32_t nodeId) const
{
    return req->GetArcTgtNode() == nodeId;
}

uint32_t Ring::DetermineIdealHops(uint32_t srcNode, uint32_t destNode) const
{
    uint32_t nodesNum = pTileUtils->GetNodeNum();
    uint32_t clockwiseHops = (destNode - srcNode + nodesNum) % nodesNum;
    uint32_t counterClockwiseHops = (srcNode - destNode + nodesNum) % nodesNum;
    return clockwiseHops < counterClockwiseHops ? clockwiseHops : counterClockwiseHops;
}

CrossStation::Direction Ring::DetermineRoute(uint32_t srcNode, uint32_t destNode) const
{
    // Simple shortest path algorithm
    uint32_t nodesNum = pTileUtils->GetNodeNum();
    uint32_t clockwiseHops = (destNode - srcNode + nodesNum) % nodesNum;
    uint32_t counterClockwiseHops = (srcNode - destNode + nodesNum) % nodesNum;
    const int direction_num = 2;

    if (clockwiseHops < counterClockwiseHops) {
        return CrossStation::Direction::CLOCKWISE;
    } else if (clockwiseHops == counterClockwiseHops) {
        std::uint32_t rand_num = std::rand();
        if (rand_num % direction_num == 0) {
            return CrossStation::Direction::COUNTER_CLOCKWISE;
        } else {
            return CrossStation::Direction::CLOCKWISE;
        }
    } else {
        return CrossStation::Direction::COUNTER_CLOCKWISE;
    }
}

void Ring::AddSwimLane(BlockCommandPtr cmd)
{
    SwimLogData logData;
    logData.name = "Node working " + GetTileOpName(cmd->tileOp);
    logData.pid = CORE_TOP_MACHINE_ID;
    logData.tid = cmd->execMachineId;
    logData.sTime = cmd->startCalCycle;
    logData.eventId = cmd->eventId >= 0 ? cmd->eventId : GetSim()->GetEventId();
    logData.eTime = GetSim()->getCycles();
    logData.hint = cmd->DumpSwimInfo(swimLaneOffset);
    GetSim()->AddDuration(logData);
}

} // namespace JCore
