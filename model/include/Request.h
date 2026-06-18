#ifndef REQUEST_H
#define REQUEST_H

#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "interface/ConstConfig.h"
#include "ModelCommon/BlockCommand.h"
#include "DataStruct/MachineId.h"

namespace JCore {

enum class ChannelType {
    REQ0 = 0, // for cube/vec/mtc read
    REQ1, // for cache mode, read/write from TMU, vec fetch tlb/stash
    DATA, // for load data response, write data
    RSP, // for write response, stash comp
    SNOOP,
    COUNT
};

enum class BufferType {
    LOAD_REQ0 = 0,
    REQ1,
    LOAD_DATA_RES,
    STORE_DATA,
    STORE_DATA_RES,
    SNOOP,
    COUNT
};

enum class FlitType {
    HEADER,
    DATA,
    RESPONSE
};

// CHI协议消息类型
enum class CHICommand {
    ReadShared,
    ReadUnique,
    Write,
    CleanUnique,
    MakeUnique,
    Evict,
    SnpShared,
    SnpUnique,
    CompDBIDResp,
    Comp,
    RetryAck,
    // 其他CHI命令...
};

inline std::map<BufferType, ChannelType>& GetBufTypeCvtMap()
{
    static std::map<BufferType, ChannelType> bufTypeToChannelType = {
        {BufferType::LOAD_REQ0, ChannelType::REQ0},
        {BufferType::REQ1, ChannelType::REQ1},
        {BufferType::LOAD_DATA_RES, ChannelType::DATA},
        {BufferType::STORE_DATA, ChannelType::DATA},
        {BufferType::STORE_DATA_RES, ChannelType::RSP},
        {BufferType::SNOOP, ChannelType::SNOOP},
    };
    return bufTypeToChannelType;
}

const std::map<CHICommand, std::string> chiCmdNames = {
    {CHICommand::ReadShared, "ReadShared"},
    {CHICommand::ReadUnique, "ReadUnique"},
    {CHICommand::Write, "Write"},
    {CHICommand::CleanUnique, "CleanUnique"},
    {CHICommand::MakeUnique, "MakeUnique"},
    {CHICommand::Evict, "Evict"},
    {CHICommand::SnpShared, "SnpShared"},
    {CHICommand::SnpUnique, "SnpUnique"},
    {CHICommand::CompDBIDResp, "CompDBIDResp"},
    {CHICommand::Comp, "Comp"},
    {CHICommand::RetryAck, "RetryAck"},
};

struct CHIFlit {
    FlitType type;
    CHICommand cmd;
    uint32_t srcId;
    uint32_t tgtId;
    uint32_t addr;
    uint32_t txnId;
    // 用于DATA flit
    std::vector<uint8_t> data;
    uint8_t dataID; // for vscatter 0: data; 1: addr
    // 用于RESPONSE flit
    uint8_t respErr;
    uint8_t qos;
    uint8_t srcProt;
    uint8_t tgtProt;

    // 构造函数
    CHIFlit(FlitType t, CHICommand c, uint32_t s, uint32_t tg, uint32_t a, uint32_t tid)
        : type(t), cmd(c), srcId(s), tgtId(tg), addr(a), txnId(tid), dataID(0),
          respErr(0), qos(0), srcProt(0), tgtProt(0) {}

    CHIFlit(FlitType t, CHICommand c, uint32_t s, uint32_t tg, uint32_t a, uint32_t tid, uint8_t did)
        : type(t), cmd(c), srcId(s), tgtId(tg), addr(a), txnId(tid), dataID(did),
          respErr(0), qos(0), srcProt(0), tgtProt(0) {}
};

struct DataMask {
    const uint32_t SINGLE_MASK_SIZE = 64;
    uint32_t elem = 1;
    std::vector<uint64_t> mask;
    // Set mask in [start, end) with element size
    void InitWithElem(uint32_t start, uint32_t end, uint32_t elementSize)
    {
        elem = elementSize;
        mask = std::vector<uint64_t>(MAX_TILE_DATA_BYTE / elem / SINGLE_MASK_SIZE, 0UL);
        Set(start, end);
    }
    void Set(uint32_t start, uint32_t end)
    {
        ASSERT(start < end);
        uint32_t maskStart = start / elem;
        uint32_t maskEnd = (end -1) / elem;
        uint32_t idx = maskStart / SINGLE_MASK_SIZE;
        while (idx < mask.size() && idx <= maskEnd / SINGLE_MASK_SIZE) {
            uint64_t singleMaskStart = idx == maskStart / SINGLE_MASK_SIZE
                                       ? maskStart % SINGLE_MASK_SIZE : 0;
            uint64_t singleMaskEnd = idx == maskEnd / SINGLE_MASK_SIZE
                                     ? maskEnd % SINGLE_MASK_SIZE + 1 : SINGLE_MASK_SIZE;
            uint64_t highMask = singleMaskEnd == SINGLE_MASK_SIZE ? -1UL : ((1UL << singleMaskEnd) - 1);
            mask[idx] |= highMask & ~((1UL << singleMaskStart) - 1);
            idx++;
        }
    }
    bool IsValid(uint32_t bitIdx)
    {
        uint32_t idx = bitIdx / elem;
        return ((mask[idx / SINGLE_MASK_SIZE] >> (idx % SINGLE_MASK_SIZE)) & 1UL) == 1UL;
    }
    bool IsByteValid(uint32_t byteIdx)
    {
        return IsValid(byteIdx);
    }

    void Merge(const DataMask& other)
    {
        assert(elem == other.elem);
        assert(mask.size() == other.mask.size());

        for (size_t i = 0; i < mask.size(); i++) {
            mask[i] |= other.mask[i];
        }
    }

    bool IsFull() const
    {
        for (uint64_t m : mask) {
            if (m != -1UL) {
                return false;
            }
        }
        return true;
    }
    DataMask& operator=(DataMask const& dmask)
    {
        if (this != &dmask) {
            elem = dmask.elem;
            if (mask.size() != dmask.mask.size()) {
                mask.resize(dmask.mask.size());
            }
            mask = dmask.mask;
        }
        return *this;
    }
    void Reset()
    {
        mask = std::vector<uint64_t>(mask.size(), 0UL);
    }

    DataMask(uint32_t elementSize)
        : elem(elementSize), mask(MAX_TILE_DATA_BYTE / elem / SINGLE_MASK_SIZE, -1UL) {}
};

struct BroadcastInfo {
    bool stash = false;
    uint32_t stashWayId = -1U;
    uint32_t station = 0;
};

class Request {
    // Node connections: 0->1->3->5->7->6->4->2->0
std::map<int, int> archToPhyNetMap = {
    {0, 7},
    {1, 0},
    {3, 1},
    {5, 2},
    {7, 3},
    {6, 4},
    {4, 5},
    {2, 6}};
std::map<int, int> phyToArchNetMap = {
    {0, 1},
    {1, 3},
    {2, 5},
    {3, 7},
    {4, 6},
    {5, 4},
    {6, 2},
    {7, 0}};
public:
    DataMask dmask = DataMask(1);
    BlockCommandPtr cmd;
    uint32_t coreId = 0;
    uint32_t stid = -1U;
    Request(const CHIFlit& flit, ChannelType channel)
        : id(GetNextId()), flit(flit), channel(channel), cyclesWaiting(0), isTagged(false), isStashReq(false),
        isStashResp(false) {}

    uint32_t GetId() const { return id; }
    uint32_t GetAddr() const { return flit.addr;}
    uint32_t GetStashTimes() const { return stashTimes; }
    const CHIFlit& GetFlit() const { return flit; }
    ChannelType GetChannel() const { return channel; }
    MachineType GetPEType() const { return peType ; }
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

    Request(Request&&) = default;
    Request& operator=(Request&&) = default;
    uint32_t GetSrcNode() const { return phyToArchNetMap.at(flit.srcId); }
    uint32_t GetArcSrcNode() const { return flit.srcId; }
    uint32_t GetTgtNode() const { return phyToArchNetMap.at(flit.tgtId); }
    uint32_t GetArcTgtNode() const { return flit.tgtId; }
    uint32_t GetStashDstNode() const { return dstNodeId; }
    uint32_t GetCyclesWaiting() const { return cyclesWaiting; }
    uint32_t GetBufId(bool enableAssert = true) const
    {
        if (enableAssert) {
            ASSERT(bufId != UINT32_MAX);
        }
        return bufId;
    }
    uint32_t GetSize() const { return size; }
    uint8_t* GetData() { return data; }
    uint32_t GetCoreId() const { return coreId;}
    uint32_t GetStid() const { return stid;}
    bool IsTagged() const { return isTagged; }
    bool IsStashReq() const {return isStashReq;}
    bool IsStashResp() const {return isStashResp;}
    bool IsPrefetchReq() const {return isPrefetchReq;}
    bool IsPrefetchResp() const {return isPrefetchResp;}
    bool IsStashCompData() const { return isStashResp && (channel == ChannelType::DATA);}
    bool IsStash() const { return IsStashReq() || IsStashResp(); }
    bool IsPrefetch() const { return IsPrefetchReq() || IsPrefetchResp(); }
    unsigned GetWayId(bool enableAssert = true) const
    {
        if (enableAssert) {
            ASSERT(wayId != -1U);
        }
        return wayId;
    }
    unsigned GetTOStashBId() const {return toBid;}
    int GetDirection() const { return direction;};
    bool IsFromCore() const
    {
        return channel == ChannelType::REQ0
               || channel == ChannelType::REQ1
               || (channel == ChannelType::DATA && flit.cmd == CHICommand::Write);
    }
    bool IsFromTileRegister() const
    {
        return (flit.cmd == CHICommand::Comp || channel == ChannelType::RSP
               || (channel == ChannelType::DATA && flit.cmd == CHICommand::CompDBIDResp));
    };
    bool IsRead() { return (flit.cmd == CHICommand::ReadShared || flit.cmd == CHICommand::ReadUnique); }
    bool IsWrite() { return flit.cmd == CHICommand::Write; }

    void IncrementCyclesWaiting() { cyclesWaiting++; }
    void ResetCyclesWaiting() { cyclesWaiting = 0; }
    void SetCHICommand(CHICommand type) { flit.cmd = type; }

    BufferType GetBufferType()
    {
        if (channel == ChannelType::REQ0) {
            return BufferType::LOAD_REQ0;
        }
        if (channel == ChannelType::REQ1) {
            return BufferType::REQ1;
        }
        if (channel == ChannelType::DATA) {
            if (flit.cmd == CHICommand::Write) {
                return BufferType::STORE_DATA;
            }
            return BufferType::LOAD_DATA_RES;
        }
        if (channel == ChannelType::RSP) {
            return BufferType::STORE_DATA_RES;
        }
        return BufferType::SNOOP;
    }

    void SetTgtNodeId(uint32_t destId)
    {
        tgtNode = destId;
        pTgtNode = archToPhyNetMap[tgtNode];
    }
    void SetFlitTgtNodeId(uint32_t nid)
    {
        flit.tgtId = nid;
    }
    void SetFlitSrcNodeId(uint32_t nid)
    {
        flit.srcId = nid;
    }

    uint32_t CalcDistance(uint32_t nodeNum = 8)
    {
        return (nodeNum + flit.srcId - flit.tgtId) % nodeNum > (nodeNum + flit.tgtId - flit.srcId) % nodeNum
               ? (nodeNum + flit.tgtId - flit.srcId) % nodeNum
               : (nodeNum + flit.srcId - flit.tgtId) % nodeNum;
    }

    void SetToBid(unsigned bid) {toBid = bid;}
    void SetWayId(unsigned way) { wayId = way;}
    void SetStashReq(bool flag) { isStashReq = flag; }
    void SetStashResp(bool flag) { isStashResp = flag; }
    void SetPrefetchReq(bool flag) { isPrefetchReq = flag; }
    void SetPrefetchResp(bool flag) { isPrefetchResp = flag; }
    void SetTagged(bool tagged) { isTagged = tagged; }
    void SetDirection(int dir) { direction = dir;}
    void SetBufId(uint32_t id) { bufId = id; }
    void SetAddr(uint32_t addr) { flit.addr = addr; }
    void SetStashDstNode(uint32_t nid) { dstNodeId = nid; }
    void SetArcTgtNode(uint32_t nid) { flit.tgtId = nid; }
    void SetSize(uint32_t s) { size = s; }
    void SetStashTimes(uint32_t cnt) {stashTimes = cnt;}
    void SetCoreId(uint32_t cId) { this->coreId = cId;}
    void SetStid(uint32_t stId) { this->stid = stId; }
    void SetChannelType(ChannelType type) { channel = type; }
    void SetPEType(MachineType type) { peType = type;}
    void SetData(const uint8_t *d) { memcpy(data, d, MAX_TILE_DATA_BYTE); }
    template<typename T>
    void SetData(const std::vector<T> &wdat)
    {
        ASSERT((wdat.size() * sizeof(T)) >= MAX_TILE_DATA_BYTE);
        memcpy(data, wdat.data(), MAX_TILE_DATA_BYTE);
    }
    std::string GetChannelName() const
    {
        switch (channel) {
            case ChannelType::REQ0: return "REQ0_CHANNEL";
            case ChannelType::REQ1: return "REQ1_CHANNEL";
            case ChannelType::DATA: return "DATA_CHANNEL";
            case ChannelType::RSP: return "RSP_CHANNEL";
            case ChannelType::SNOOP: return "SNOOP";
            default: return "UNKNOWN";
        }
    }
    void SetIdealHops(uint32_t hops) { idealHops = hops; }
    uint32_t GetIdealHops() const { return idealHops; }
    void IncRealHops() { realHops++; }
    uint32_t GetRealHops() const { return realHops; }
public:
    // for debugging
    uint64_t uopId = 0;
    uint64_t inSpbTime = 0;
    uint64_t onRingTime = 0;
    uint64_t offRingTime = 0;
    uint64_t inMgbTime = 0;
    uint64_t outMgbTime = 0;
    uint64_t lastCycleProcessed = 0;
    // request time record in response
    uint64_t reqInSpbTime = 0;
    uint64_t reqOnRingTime = 0;
    uint64_t reqOffRingTime = 0;
    uint64_t reqInMgbTime = 0;
    uint64_t reqOutMgbTime = 0;
    uint64_t frqTime = 0;
    uint64_t tileTime = 0;
    bool broadcast = false;
    std::vector<BroadcastInfo> broadcastList;
    uint32_t broadcastTgt = 0;
private:
    int srcNode;
    int tgtNode;
    int pSrcNode;
    int pTgtNode;
    int direction = -1;

    static uint32_t GetNextId()
    {
        static std::atomic<uint32_t> s_nextId(1);
        uint32_t id = s_nextId.fetch_add(1, std::memory_order_relaxed);
        if (id == UINT32_MAX) {
            throw std::runtime_error("Request ID overflow");
        }
        return id;
    }

    const uint32_t id = 0;
    CHIFlit flit;
    uint32_t addr_;
    uint32_t size;
    uint32_t stashTimes = 0;
    uint32_t bufId = UINT32_MAX;
    alignas(8) uint8_t data[MAX_TILE_DATA_BYTE]; // 8-byte alignment due to uint64_t* casting
    ChannelType channel;
    MachineType peType = MachineType::UNKNOW_CORE;
    uint32_t cyclesWaiting;
    bool isTagged = false;
    bool isStashReq = false;
    bool isStashResp = false;
    bool isPrefetchReq = false;
    bool isPrefetchResp = false;
    uint32_t dstNodeId = 0;
    unsigned wayId = -1U;
    unsigned toBid = 0;

    uint32_t idealHops = 0;
    uint32_t realHops = 0;
};

inline std::ostream& operator<<(std::ostream& out, Request& req)
{
    out << std::dec << "Request[" << req.GetId() << "]:(";
    if (req.cmd) {
        out << "bid: " << req.cmd->bid.val << ", stid: " << req.cmd->stid << " ";
    }
    out << std::hex << "addr=0x" << req.GetFlit().addr;
    out << std::dec << ", Core reqId=" << req.GetBufId(false) << ",size=" << req.GetSize();
    out << " stash:" << req.IsStash();
    out << " prefetch:" << req.IsPrefetch();
    auto flit = req.GetFlit();
    out << std::dec << ",src_node=" << flit.srcId << ",tgt_node=" << flit.tgtId;
    out << ",command=" << chiCmdNames.at(req.GetFlit().cmd) << ", coreId=" << req.GetCoreId();
    // out << " wayId=" << std::dec << req.GetWayId(false);
    out << " uopId=" << std::dec << req.uopId;
    out << ")";
    return out;
}

} // namespace JCore

#endif // REQUEST_H
