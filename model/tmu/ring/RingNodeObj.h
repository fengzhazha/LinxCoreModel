#pragma once
#include "SimSys.h"
#include "tmu/ring/CrossStation.h"
#include "tmu/TileUtils.h"

namespace JCore {

class RingNodeObj : public SimObj {
public:
    RingNodeObj() {}
    virtual ~RingNodeObj() {}
    virtual void Work() {}
    virtual void Xfer() {}
    virtual void Reset() {}
    virtual SimSys *GetSim() { return sim; }
    TileUtils                                    *pTileUtils = nullptr;
    SimSys                                       *sim = nullptr;
    std::vector<std::shared_ptr<CrossStation>>   stations;
    size_t                                       stationReadRange = 0;
    std::map<std::shared_ptr<Request>, bool>     tileRequestMap;

    bool                                         isRoundRobin = false;
    size_t                                       roundRobinReadPort = 0;

    void DeliveryPipeInfo(CycleBus pipe, std::shared_ptr<Request> req)
    {
        pipe->reqSpbCycle = req->reqInSpbTime;
        pipe->reqOnRingCycle = req->reqOnRingTime;
        pipe->reqOffRingCycle = req->reqOffRingTime;
        pipe->reqMgbCycle = req->reqInMgbTime;
        pipe->frqCycle = req->frqTime;
        pipe->tileWrCycle = req->tileTime;
        pipe->resSpbCycle = req->inSpbTime;
        pipe->resOnRingCycle = req->onRingTime;
        pipe->resMgbCycle = req->inMgbTime;
        pipe->resToCoreCycle = req->outMgbTime;
    }

    void AddRequestToTileRequestMap(std::shared_ptr<Request> req)
    {
        tileRequestMap.emplace(req, true);
    }

    void DeleteRequestInTileRequestMap(std::shared_ptr<Request> req)
    {
        for (auto it = tileRequestMap.begin(); it != tileRequestMap.end(); ++it) {
            if (req->GetBufId() == it->first->GetBufId()) {
                tileRequestMap.erase(it);
                break;
            }
        }
    }

    bool HasValidSpbReadEntry()
    {
        for (size_t i = 0; i < stationReadRange; ++i) {
            if (stations[i]->SpbHasAvailEntry(BufferType::LOAD_REQ0)) {
                return true;
            }
        }
        return false;
    }

    bool HasValidSpbReadEntry(uint32_t addr, bool strict)
    {
        uint32_t destNode = pTileUtils->GetDstNode(addr);
        std::shared_ptr<CrossStation> station = nullptr;
        uint32_t minDist = -1U;
        for (size_t i = 0; i < stationReadRange; ++i) {
            if (!strict && !stations[i]->SpbHasAvailEntry(BufferType::LOAD_REQ0)) {
                continue;
            }
            uint32_t srcNode = stations[i]->GetId();
            uint32_t cwDist = (destNode - srcNode + pTileUtils->GetNodeNum()) % pTileUtils->GetNodeNum();
            uint32_t ccDist = (srcNode - destNode + pTileUtils->GetNodeNum()) % pTileUtils->GetNodeNum();
            uint32_t dist = cwDist < ccDist ? cwDist : ccDist;
            if (dist < minDist) {
                minDist = dist;
                station = stations[i];
            }
        }
        if (strict && !station->SpbHasAvailEntry(BufferType::LOAD_REQ0)) {
            return false;
        }
        return true;
    }

    std::shared_ptr<CrossStation> GetFreeReadStation()
    {
        std::shared_ptr<CrossStation> station = nullptr;
        if (isRoundRobin) {
            size_t port = roundRobinReadPort;
            for (size_t i = 0; i < stationReadRange; ++i) {
                if (stations[port]->SpbHasAvailEntry(BufferType::LOAD_REQ0)) {
                    station = stations[port];
                    roundRobinReadPort++;
                    roundRobinReadPort = (roundRobinReadPort == stationReadRange) ? 0 : roundRobinReadPort;
                    break;
                } else {
                    port++;
                    port = (port == stationReadRange) ? 0 : port;
                }
            }
        } else { // fewest spb size
            uint32_t curSize = -1U;
            for (size_t i = 0; i < stationReadRange; ++i) {
                if (stations[i]->SpbHasAvailEntry(BufferType::LOAD_REQ0)
                    && stations[i]->GetSpbSize(BufferType::LOAD_REQ0) < curSize) {
                    station = stations[i];
                    curSize = stations[i]->GetSpbSize(BufferType::LOAD_REQ0);
                }
            }
        }
        return station;
    }

    // least distance
    std::shared_ptr<CrossStation> GetFreeReadStation(uint32_t addr, bool strict)
    {
        uint32_t destNode = pTileUtils->GetDstNode(addr);
        std::shared_ptr<CrossStation> station = nullptr;
        uint32_t minDist = -1U;
        for (size_t i = 0; i < stationReadRange; ++i) {
            if (!strict && !stations[i]->SpbHasAvailEntry(BufferType::LOAD_REQ0)) {
                continue;
            }
            uint32_t srcNode = stations[i]->GetId();
            uint32_t cwDist = (destNode - srcNode + pTileUtils->GetNodeNum()) % pTileUtils->GetNodeNum();
            uint32_t ccDist = (srcNode - destNode + pTileUtils->GetNodeNum()) % pTileUtils->GetNodeNum();
            uint32_t dist = cwDist < ccDist ? cwDist : ccDist;
            if (dist < minDist) {
                minDist = dist;
                station = stations[i];
            }
        }
        if (strict && !station->SpbHasAvailEntry(BufferType::LOAD_REQ0)) {
            return nullptr;
        }
        return station;
    }

    bool HasValidSpbWriteEntry()
    {
        for (size_t i = stationReadRange; i < stations.size(); ++i) {
            if (stations[i]->SpbHasAvailEntry(BufferType::STORE_DATA)) {
                return true;
            }
        }
        return false;
    }

    bool HasValidSpbWriteEntry(BufferType type, uint32_t requiredCount)  // 函数签名：(BufferType, uint32_t)
    {
        for (size_t i = stationReadRange; i < stations.size(); ++i) {
            uint32_t availableCount = stations[i]->GetSpbAvailableCount(type);
            if (availableCount >= requiredCount) {
                return true;
            }
        }
        return false;
    }

    bool HasValidSpbWriteEntry(uint32_t addr, bool strict)
    {
        uint32_t destNode = pTileUtils->GetDstNode(addr);
        std::shared_ptr<CrossStation> station = nullptr;
        uint32_t minDist = -1U;
        for (size_t i = stationReadRange; i < stations.size(); ++i) {
            if (!strict && !stations[i]->SpbHasAvailEntry(BufferType::STORE_DATA)) {
                continue;
            }
            uint32_t srcNode = stations[i]->GetId();
            uint32_t cwDist = (destNode - srcNode + pTileUtils->GetNodeNum()) % pTileUtils->GetNodeNum();
            uint32_t ccDist = (srcNode - destNode + pTileUtils->GetNodeNum()) % pTileUtils->GetNodeNum();
            uint32_t dist = cwDist < ccDist ? cwDist : ccDist;
            if (dist < minDist) {
                minDist = dist;
                station = stations[i];
            }
        }
        if (strict && !station->SpbHasAvailEntry(BufferType::STORE_DATA)) {
            return false;
        }
        return true;
    }

    std::shared_ptr<CrossStation> GetFreeWriteStation()
    {
        for (size_t i = stationReadRange; i < stations.size(); ++i) {
            if (stations[i]->SpbHasAvailEntry(BufferType::STORE_DATA)) {
                return stations[i];
            }
        }
        return nullptr;
    }

    std::shared_ptr<CrossStation> GetFreeWriteStation(BufferType type)
    {
        for (size_t i = stationReadRange; i < stations.size(); ++i) {
            if (stations[i]->SpbHasAvailEntry(type)) {
                return stations[i];
            }
        }
        return nullptr;
    }

    std::shared_ptr<CrossStation> GetFreeWriteStation(uint32_t addr, bool strict)
    {
        uint32_t destNode = pTileUtils->GetDstNode(addr);
        std::shared_ptr<CrossStation> station = nullptr;
        uint32_t minDist = -1U;
        for (size_t i = stationReadRange; i < stations.size(); ++i) {
            if (!strict && !stations[i]->SpbHasAvailEntry(BufferType::STORE_DATA)) {
                continue;
            }
            uint32_t srcNode = stations[i]->GetId();
            uint32_t cwDist = (destNode - srcNode + pTileUtils->GetNodeNum()) % pTileUtils->GetNodeNum();
            uint32_t ccDist = (srcNode - destNode + pTileUtils->GetNodeNum()) % pTileUtils->GetNodeNum();
            uint32_t dist = cwDist < ccDist ? cwDist : ccDist;
            if (dist < minDist) {
                minDist = dist;
                station = stations[i];
            }
        }
        if (strict && !station->SpbHasAvailEntry(BufferType::STORE_DATA)) {
            return nullptr;
        }
        return station;
    }

    void AddNode(std::shared_ptr<CrossStation> station)
    {
        if (station != nullptr) {
            stations.push_back(station);
        }
    }
};

} // namespace JCore
