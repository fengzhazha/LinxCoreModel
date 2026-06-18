#include "tmu/xbar/Island.h"

namespace JCore {
using namespace std;
SimSys* Island::GetSim()
{
    return sim;
}

void Island::Build()
{
    configs.overrideDefaultConfig(GetSim()->getCfgs());
    selectSPBNodeID = vector<vector<uint32_t>>(static_cast<size_t>(ChannelType::COUNT),
                      vector<uint32_t>(pTileUtils->GetNodeNum(), 0));
}

void Island::Work()
{
    ArbitrateSPB();
}

void Island::ArbitrateSPB()
{
    for (uint32_t c = 0; c < static_cast<uint32_t>(ChannelType::COUNT) && pTileUtils->IsXbarMode(); ++c) {
        for (uint32_t n = 0; n < nodes.size(); ++n) {
            ChannelType cType = static_cast<ChannelType>(c);
            ArbitratePipe(cType, n);
        }
    }
}

void Island::ArbitratePipe(ChannelType cType, uint32_t nodeId)
{
    for (uint32_t n = 0; n < nodes.size(); ++n) {
        uint32_t c = static_cast<uint32_t>(cType);
        uint32_t curId = selectSPBNodeID[c][nodeId];
        IncSPBSelectID(cType, nodeId);
        if (nodes[curId]->SPBEmpty(cType)) {
            continue;
        }
        shared_ptr<Request> pkt = nodes[curId]->ReadOnlySPB(cType);
        ASSERT(pkt != nullptr);
        if (pkt->GetArcTgtNode() != nodeId || nodes[nodeId]->MGBFull(pkt->GetBufferType())) {
            continue;
        }
        nodes[nodeId]->WriteMGB(pkt);
        nodes[curId]->PopSPB(pkt->GetBufferType());
        break;
    }
}

void Island::Xfer()
{
}

void Island::Reset()
{
}

void Island::IncSPBSelectID(ChannelType channel, uint32_t nodeId)
{
    auto &sid = selectSPBNodeID[static_cast<uint32_t>(channel)][nodeId];
    sid = (sid + 1) % pTileUtils->GetNodeNum();
}

}