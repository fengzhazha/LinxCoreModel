#ifndef BLOCKISA_MODEL_INTERFACE_VECTILEREGLDREQ_H
#define BLOCKISA_MODEL_INTERFACE_VECTILEREGLDREQ_H

#include <iostream>

namespace JCore {


// VEC_TILEREG_LD_REQ
class VecTileRegLdReq {
public:
    void SetTagId(uint32_t tagIdValue, OperandType handTypeValue)
    {
        tagId = tagIdValue;
        handType = handTypeValue;
    }
    uint32_t GetTagId() const { return tagId; }
    OperandType GetHandType() const { return handType; }
    uint32_t GetReqId() const { return reqId; }
    uint32_t GetStid() const { return stid; }
    void SetReqId(uint32_t value) { reqId = value; }
    uint32_t GetSize() const { return size; }
    void SetSize(uint32_t value) { size = value; }
    uint64_t GetSrc() const { return src; }
    void SetSrc(uint64_t value) { src = value; }
    void SetBid(ROBID bid) { this->bid = bid; }
    void SetGid(ROBID gid) { this->gid = gid; }
    void SetRid(ROBID rid) { this->rid = rid; }
    void SetTid(uint32_t tid) { this->tid = tid; }
    void SetStid(uint32_t stId) { this->stid = stId; }
    void SetStashFlag() { this->isStash = true; }
    ROBID GetBid() const { return bid; }
    ROBID GetGid() const { return gid; }
    ROBID GetRid() const { return rid; }
    uint32_t GetTid() const { return tid; }
    bool GetIsStash() const {return isStash;}
    uint64_t GetAddr() const {return src;}
public:
    VecTileRegLdReq() : reqId(0), size(0), src(0), tid(0), isStash(false) {}
    VecTileRegLdReq(uint32_t reqId, uint32_t size, uint64_t src)
        : reqId(reqId), size(size), src(src), tid(0), isStash(false) {}
private:
    uint32_t reqId;
    uint32_t size;
    uint64_t src;
    ROBID bid;
    ROBID gid;
    ROBID rid;
    uint32_t tid;
    uint32_t stid = 0;
    bool isStash;
    uint32_t tagId = 0;
    OperandType handType = OperandType::OPD_INVALID;
};

inline std::ostream& operator<<(std::ostream& out, const VecTileRegLdReq& req)
{
    out << std::dec << "B" << req.GetBid() << "-I" << req.GetRid() << "-G" << req.GetGid() << "-T" << req.GetTid();
    out << std::dec << "reqId=" << req.GetReqId() << ", size=" << req.GetSize();
    out << std::hex << ", src=0x" << req.GetSrc();
    return out;
}


} // namespace JCore

#endif  // BLOCKISA_MODEL_INTERFACE_VECTILEREGLDREQ_H
