#ifndef CROSS_STATION_H
#define CROSS_STATION_H

#include <array>
#include <memory>
#include <queue>
#include <vector>

#include "core/Bus.h"
#include "SimSys.h"
#include "Request.h"
#include "tmu/ring/StationStat.h"
#include "tmu/TileUtils.h"

namespace JCore {

class Ring;

struct RingState {
    uint32_t oriId; // Original cross station ID
    std::vector<std::shared_ptr<Request>> channelRequests; // put Requests per channel
    std::vector<std::shared_ptr<Request>> nextChannelRequests; // put Requests per channel
    std::vector<std::shared_ptr<Request>> requestDownStash; // put Requests per channel
    std::vector<uint32_t> channelLocks; // Locked req IDs per channelmn for on ring starvtion
    RingState() {}
    explicit RingState(uint32_t id);
};

class CrossStation : public SimObj {
public:
    enum class Direction {
        CLOCKWISE = 0,
        COUNTER_CLOCKWISE,
        RING_NUM,
    };

    enum class TileStage {
        PIPE_STAGE = 0,
        TILE_REG_STAGE,
        DATA_DONE,
    };

    struct TilePipeState {
        TileStage tStage = TileStage::PIPE_STAGE;
        uint64_t waitCycle = 0;
        std::shared_ptr<Request> req;
    };

    StationStat stat;

    CrossStation() {}
    explicit CrossStation(uint32_t cid);
    TMUConfig  configs;
    uint32_t GetId() const { return id; }

    void ConnectRings(std::shared_ptr<Ring> cRing, std::shared_ptr<Ring> wRing);
    void ReceiveRequest(const std::shared_ptr<Request>& req);

    bool SpbHasAvailEntry(BufferType type);
    uint32_t GetSpbAvailableCount(BufferType type);  // 获取SPB可用空间数量
    bool SpbHasStashEntry(BufferType type);
    bool WriteSpb(const std::shared_ptr<Request>& req);
    uint32_t GetSpbSize(BufferType type);
    bool TryPutInMGB(const std::shared_ptr<Request> req);

    // FRQ
    bool FrqCanAaccelptRequest(bool isRead) const;
    void FrqAddRequest(const std::shared_ptr<Request>& request);
    std::shared_ptr<Request> GetFrqReq(bool isRead);
    void PopReadyEntryFrq(bool isRead);
    bool HasNoResp(uint32_t coreId = 0);
    std::shared_ptr<Request> GetDataComReq(uint32_t coreId = 0);
    std::shared_ptr<Request> GetDataComReqFront(uint32_t coreId = 0);
    void PopDataComReq(uint32_t coreId = 0);
    void TransferTilePipe();

    RingState& GetCWRingState() { return ringState[static_cast<size_t>(Direction::CLOCKWISE)]; }
    RingState& GetCCRingState() { return ringState[static_cast<size_t>(Direction::COUNTER_CLOCKWISE)]; }
    const RingState& GetCWRingState() const { return ringState[static_cast<size_t>(Direction::CLOCKWISE)]; }
    const RingState& GetCCRingState() const { return ringState[static_cast<size_t>(Direction::COUNTER_CLOCKWISE)]; }
    void InitCoreBuffer(uint32_t coreNum);
    /* \brief cmd read request */
    std::shared_ptr<Request> Rxreq(uint32_t addr, uint32_t stid, uint32_t coreId = 0);
    /* \brief cmd data write request */
    std::shared_ptr<Request> Rxdat(uint32_t addr, uint32_t stid, uint32_t coreId = 0);
    /* \brief cmd data read response */
    std::shared_ptr<Request> Txdat(uint32_t addr, int srcNode, int tgtNode, uint32_t stid, uint32_t coreId = 0);
    /* \brief cmd write response */
    std::shared_ptr<Request> Txrsp(uint32_t addr, int srcNode, int tgtNode, uint32_t stid, uint32_t coreId = 0);
    std::shared_ptr<Request> RxvscatterReq(uint32_t addr, uint32_t txnId,
        int tgtNode, uint32_t coreId = 0) const;
    std::shared_ptr<Request> RxvscatterData(uint32_t addr, uint32_t txnId, uint8_t did,
        int srcNode, int tgtNode, uint32_t coreId = 0) const;

    // In buffer type respect
    bool SPBFull(BufferType type);
    bool MGBFull(BufferType type);
    bool SPBEmpty(BufferType type);
    bool MGBEmpty(BufferType type);
    std::shared_ptr<Request> PopSPB(BufferType type);
    std::shared_ptr<Request> PopMGB(BufferType type);
    std::shared_ptr<Request> ReadOnlySPB(BufferType type);
    std::shared_ptr<Request> ReadOnlyMGB(BufferType type);

    // In channel type respect
    bool SPBEmpty(ChannelType channel);
    bool MGBEmpty(ChannelType channel);
    std::shared_ptr<Request> PopSPB(ChannelType channel);
    std::shared_ptr<Request> PopMGB(ChannelType channel);
    std::shared_ptr<Request> ReadOnlySPB(ChannelType channel);
    std::shared_ptr<Request> ReadOnlyMGB(ChannelType channel);

    void WriteMGB(const std::shared_ptr<Request>& pkt);

    // debugging
    void CheckStatus();

    SimSys                                  *sim;
    SimSys                                  *GetSim();
    void                                    Build();
    void                                    Reset();
    void                                    Work();
    void                                    Xfer();
    void ReportStat() override {}
    // for debug
    void                                    SetVerboseOn();
    bool                                    verbose = false;
    // confgiuration
    TileUtils                               *pTileUtils;
private:
    const std::map<BufferType, ChannelType> buffer2Channel = {
        {BufferType::LOAD_REQ0, ChannelType::REQ0},
        {BufferType::REQ1, ChannelType::REQ1},
        {BufferType::STORE_DATA, ChannelType::DATA},
        {BufferType::LOAD_DATA_RES, ChannelType::DATA},
        {BufferType::STORE_DATA_RES, ChannelType::RSP},
        {BufferType::SNOOP, ChannelType::SNOOP},
    };
    uint32_t id;
    uint32_t vecLoadBeginPortId;
    uint32_t vecLoadEndPortId;
    // Counter-clockwise ring
    std::shared_ptr<Ring> ccRing;
    // Clockwise ring
    std::shared_ptr<Ring> cwRing;

    // Ring states, 0:cc 1:cw
    std::vector<RingState> ringState;

    // Source Push Buffer(fifo)
    std::vector<SimQueue<std::shared_ptr<Request>>> spbArray;
    // Merge Buffer(fifo)
    std::vector<SimQueue<std::shared_ptr<Request>>> mgbArray;
    // Front Request Queue
    std::vector<std::vector<TilePipeState>> frq;
    // send to Core
    std::vector<std::queue<std::shared_ptr<Request>>> toCoreBuff;

    void ProcessSPB();
    void ProcessSPBArray(size_t bufferId, std::vector<std::vector<bool>> &onRing,
        std::vector<std::vector<bool>> &onRingBp);
    void ProcessMGB();
    void CheckStarvation();

    // VEC Ring stub functions (方案A)
    bool IsVecReadPort();
    bool IsDbidResponse(const std::shared_ptr<Request>& pkt) const;
    void ProcessVecRingStub(std::shared_ptr<Request> pkt);
    std::shared_ptr<Request> GenerateStubAddrTile(uint32_t groupSlotId, uint32_t txnId, int tmaNodeId);
    std::shared_ptr<Request> GenerateStubDataTile(uint32_t groupSlotId, uint32_t txnId, int tmaNodeId);

    bool ShouldDropAtNode(const std::shared_ptr<Request>& req, uint32_t nodeId);
    bool CanPutOnRing(ChannelType channel, Direction direction, uint32_t rid);
    void LockChannel(ChannelType channel, uint32_t reqId, RingState& ringState);
    void UnlockChannel(ChannelType channel, RingState& ringState);
    void PrintStagePkt(std::string stageName, std::string operatorName, std::shared_ptr<Request> pkt);
};

} // namespace JCore

#endif // CROSS_STATION_H
