#ifndef RING_H
#define RING_H

#include <map>
#include <memory>
#include <vector>

#include "tmu/ring/RingNodeObj.h"
#include "SimSys.h"
#include "Request.h"
#include "tmu/ring/CrossStation.h"
#include "tmu/ring/RingStat.h"

namespace JCore {

class Ring : public RingNodeObj {
public:
    Ring(CrossStation::Direction direction, const std::vector<std::shared_ptr<CrossStation>>& nodes);
    RingStat                                stat;
    SimSys                                  *sim;
    SimSys                                  *GetSim();
    void                                    Build();
    void                                    Reset();
    void                                    Work();
    void                                    Xfer();
    void ReportStat() override {}
    void RouteRequest(const std::shared_ptr<Request>& req, uint32_t currentNodeId);
    CrossStation::Direction GetDirection() const { return direction; }
    uint32_t DetermineIdealHops(uint32_t srcNode, uint32_t destNode) const;
    CrossStation::Direction DetermineRoute(uint32_t srcNode, uint32_t destNode) const;
    void ForwardReq(size_t nodeId, const std::shared_ptr<Request> req, size_t channel);

    // for debug
    void                                     SetVerboseOn();
    bool                                     verbose = false;
    // Configuration
    TileUtils                                *pTileUtils;

private:
    TMUConfig configs;
    CrossStation::Direction direction;
    const std::vector<std::shared_ptr<CrossStation>>& nodes;
    std::vector<std::vector<BlockCommandPtr>> nodesAlive;

    uint32_t GetNextNodeId(uint32_t currentNodeId) const;
    bool ShouldDropAtNode(const std::shared_ptr<Request>& req, uint32_t nodeId) const;
    void RingDelivery(size_t channel);
    void RptStats(size_t channel);
    void AddSwimLane(BlockCommandPtr cmd);
    uint64_t swimLaneOffset = 2048;
    TileOp GetRingTileOp(BlockCommandPtr cmd);
};


} // namespace JCore

#endif
