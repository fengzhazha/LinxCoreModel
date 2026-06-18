#ifndef S_STASH_CTRL_UNIT_H
#define S_STASH_CTRL_UNIT_H

#include <unordered_map>
#include <memory>

#include "../configs/tmu_config.h"
#include "core/Bus.h"
#include "interface/InterfaceCommon.h"
#include "tmu/ring/RingNodeObj.h"
#include "tmu/ring/CrossStation.h"
#include "tmu/TileRegStat.h"
#include "../configs/vectorcore_config.h"

namespace JCore {

enum class StashFSM {
    FREE,
    EXCLUSIVE,
    SHARED,
};

struct FreelistLookupInfo {
    bool hit = false;
    StashFSM fsm = StashFSM::FREE;
    uint32_t wayId = -1U;
    bool dataFromTA = false;
};

struct WayFreeListInfo {
    StashFSM fsm = StashFSM::FREE;
    bool retired = false;
    bool isTA = false;
    OperandType type = OperandType::OPD_TILE_TLINK;
    uint64_t tag = 0;
    uint32_t totalCnt = 0;
    std::unordered_map<uint32_t, BlockCommandPtr> relateCntMap;
    BlockCommandPtr cmd = nullptr;
};

class StashCtrlUnit : public RingNodeObj {
public:
    StashCtrlUnit() {}
    ~StashCtrlUnit() final {}
    INPUT SimQueue<BlockCommandPtr>                 *stashCmdQ = nullptr;
    INPUT SimQueue<BlockCommandPtr>                 *stashCompQ = nullptr;
    INPUT SimQueue<TileOperandPtr>                  *stashRetireQ = nullptr;
    INPUT SimQueue<TileOperandPtr>                  *stashFreeQ = nullptr;
    OUTPUT std::vector<SimQueue<BlockCommandPtr>*>  stashAllocDoneArray;
    OUTPUT std::vector<SimQueue<BlockCommandPtr>*>  stashRslvArray;
    OUTPUT std::vector<SimQueue<TileOperandPtr>*>   toClearArray;
    SimQueue<std::shared_ptr<Request>>              stashReqQ;
    SimQueue<std::shared_ptr<Request>>              stashRespQ;

    INPUT SimQueue<BlockCommandPtr>                 *cubeStashCmdQ;
    INPUT SimQueue<BlockCommandPtr>                 *cubeStashCompQ;
    OUTPUT SimQueue<BlockCommandPtr>                *stashCubeAllocDoneQ;

    INPUT SimQueue<BlockCommandPtr>                 *vecliteStashCmdQ = nullptr;
    INPUT SimQueue<BlockCommandPtr>                 *vecliteStashCompQ = nullptr;
    OUTPUT std::vector<SimQueue<BlockCommandPtr>*>  stashVecliteAllocDoneArray;
    OUTPUT std::vector<SimQueue<BlockCommandPtr>*>  stashVecliteRslvArray;

    VectorCoreConfig                                vecConfigs;
    TMUConfig                                       tmuConfigs;
    uint32_t                                        bccReadSrcId = 4;
    uint32_t                                        coreNum = 0;
    uint32_t                                        swimLaneOffset = 1024;

    SimSys                                          *GetSim() final { return sim; }
    void                                            Build(TileRegStat* stats);
    void                                            Reset() final;
    void                                            Work() final;
    void                                            Xfer() final;
    void                                            ReportStat() final {}
    void                                            Flush(FlushBus &flushReq);
    void                                            FlushSingleListEntry(FlushBus &flushReq,
                                                                         uint32_t coreId, uint32_t eid);
    void                                            DumpWayFreeLists(uint32_t coreId);
    uint32_t                                        GetAvailWayNum(uint32_t coreId);
    bool                                            TrackFreeSize(uint32_t freeSize, uint32_t coreId);
private:
    TileRegStat*                                    ctrlStats = nullptr;
    std::vector<std::vector<WayFreeListInfo>>                 wayFreeLists;
    std::vector<uint32_t>                                freeSizes;
    uint32_t                                        reqId = 0;
    std::unordered_map<uint32_t, std::shared_ptr<Request>>    idMap;
    void                                            StashRetireFree();
    void                                            RecvStashReq();
    bool                                            LookupAndGenReq(BlockCommandPtr &cmd,
                                                                    TileOperandPtr &operand, bool isSrc,
                                                                    SimQueue<BlockCommandPtr> &allocDoneQ);
    void                                            SendStashReq();
    void                                            RecvStashResp();

    const uint32_t                                  GetMinHops(uint32_t srcNode, uint32_t destNode) const;
    void                                            GetStashDst(std::shared_ptr<Request> pkt, uint32_t& dstNodeId) const;
    uint32_t                                        AllocWayId(BlockCommandPtr cmd, TileOperandPtr operand, bool isTA);
    void                                            FreeWayId(uint32_t coreId, uint32_t eid);
    void                                            BlockStashCompelte();
    void                                            OperandComplete(BlockCommandPtr cmd,
                                                                    TileOperandPtr operand,
                                                                    bool isSrc);
    void                                            AddSwimLane(BlockCommandPtr cmd);
    FreelistLookupInfo                              Lookup(uint32_t coreId, OperandType type, uint64_t tag);
    void                                            FreeWayWithTag(OperandType type, uint64_t tag);
    void                                            RetireTileTag(OperandType type, uint64_t tag);
};

}

#endif
