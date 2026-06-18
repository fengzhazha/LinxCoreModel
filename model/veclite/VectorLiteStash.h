#ifndef BLOCKISA_MODEL_VECLITE_STASH_H
#define BLOCKISA_MODEL_VECLITE_STASH_H
#include <vector>

#include "core/Bus.h"

namespace JCore {
struct TileSrcRequest {
    ROBID bid;
    uint32_t size;
    uint32_t srcIndex;
    uint32_t tagId;
    OperandType handType;
    uint32_t stid;
    uint64_t addr;

    // 带参数的构造函数
    TileSrcRequest(ROBID bidVal,
                   uint32_t sizeVal,
                   uint32_t srcIndexVal,
                   uint32_t tagIdVal,
                   OperandType handTypeVal,
                   uint32_t stidVal,
                   uint64_t addrVal = 0)
        : bid(bidVal), size(sizeVal), srcIndex(srcIndexVal),
          tagId(tagIdVal), handType(handTypeVal), stid(stidVal), addr(addrVal)
    {}
    ~TileSrcRequest() = default;
};

class VectorLite;
class VectorLiteStash {
public:
    VectorLiteStash();
    ~VectorLiteStash();
    VectorLite*                                top = nullptr;
    void                                       Xfer() const;
    void                                       Work();
    void                                       Build();
    bool                                       CheckStall() const;
    SimSys*                                    GetSim();
    void                                       SetSim(SimSys *s);

    void                                       SetFlush(const FlushBus &bus) const;
    bool                                       IsIdle() const;
    void                                       Stats() const;
    void                                       SendReadReq();
    void                                       GenReadReq(BlockCommandPtr& cmd);

    OUTPUT SimQueue<BlockCommandPtr>           *vecliteStashCmdQ;
    INPUT SimQueue<BlockCommandPtr>            *stashAllocDoneQ;
    INPUT SimQueue<BlockCommandPtr>            *stashRslvQ;

    INPUT SimQueue<BlockCommandPtr>            *m_fetchStashInQ;
    OUTPUT SimQueue<ROBID>                     *m_stashSiqWakeuQ;
private:
    SimSys*                                    sim = nullptr;
    uint32_t                                   m_stashReqQueDepth = 0;
    std::deque<BlockCommandPtr>                cmdMonitor;
    std::vector<TileSrcRequest>                pendingReadRegQueue;

    void                                       ReceivedStash();
    bool                                       SendStashReq(const BlockCommandPtr &bcmd);
    void                                       CheckStashAllocDone();
    void                                       HandleWayId(const BlockCommandPtr &bcmd);
    void                                       CheckStashDone();
    void                                       HandleStashResolve(const BlockCommandPtr &bcmd);
    void                                       Issue();
    void                                       StashWakeup(const ROBID &bid) const;
};
} // namespace JCore

#endif
