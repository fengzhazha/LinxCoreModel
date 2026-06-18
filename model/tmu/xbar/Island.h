#ifndef ISLAND_NODE_H
#define ISLAND_NODE_H

#include "tmu/ring/CrossStation.h"

namespace JCore {

class Island final : public SimObj {
public:
    Island() {}
    ~Island() final {}

    TMUConfig configs;
    SimSys *sim;
    TileUtils *pTileUtils;
    std::vector<std::shared_ptr<CrossStation>> nodes;

    SimSys *GetSim() final;
    void Build();
    void Reset() final;
    void Work() final;
    void Xfer() final;
    void ReportStat() final {}
private:
    std::vector<std::vector<uint32_t>> selectSPBNodeID;
    void IncSPBSelectID(ChannelType channel, uint32_t nodeId);
    void ArbitrateSPB();
    void ArbitratePipe(ChannelType cType, uint32_t nodeId);
};

}

#endif