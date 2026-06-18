#ifndef BLOCKISA_MODEL_INTERFACE_VECTILEREGSTREQ_H
#define BLOCKISA_MODEL_INTERFACE_VECTILEREGSTREQ_H

#include <iostream>
#include <stdint.h>
#include <string.h>

#include "interface/ConstConfig.h"
#include "core/Bus.h"
#include "Request.h"
#include "utils/error.h"

namespace JCore {


// VEC_TILEREG_ST_REQ
class VecTileRegStReq {
public:
    void SetVgather() { isVgather = true; }
    bool GetVgather() const { return isVgather; }
    void SetGroupSlotId(uint32_t tmp)
    {
        groupSlotId = tmp;
    }

    uint32_t GetGroupSlotId() const
    {
        return groupSlotId;
    }
    uint32_t GetSize() const { return size; }
    void SetSize(uint32_t value) { size = value; }
    uint32_t GetReqId() const { return reqId; }
    void SetReqId(uint32_t value) { reqId = value; }

    uint64_t GetDest() const { return dest; }
    void SetDest(uint64_t value) { dest = value; }

    uint64_t IsLast() const { return isLast; }
    void SetLast() { isLast = true; }

    uint8_t* GetData() { return data; }
    const uint8_t* GetData(uint32_t &size) const
    {
        size = MAX_TILE_DATA_BYTE;
        return data;
    }
    void SetData(const uint8_t* wdat, uint32_t size = MAX_TILE_DATA_BYTE)
    {
        ASSERT(size > 0 && size <= MAX_TILE_DATA_BYTE);
        memcpy(data, wdat, size);
    }
    void SetData(const std::vector<uint64_t> &wdat) { memcpy(data, wdat.data(), MAX_TILE_DATA_BYTE); }
    uint32_t GetCoreId() const { return coreId; }
    void SetCoreId(uint32_t value) { coreId = value; }
    void SetBid(ROBID bid) { this->bid = bid; }
    void SetGid(ROBID gid) { this->gid = gid; }
    void SetRid(ROBID rid) { this->rid = rid; }
    void SetTid(uint32_t tid) { this->tid = tid; }
    void SetStid(uint32_t stId) { this->stid = stId; }
    void SetCTInst() { this->fromCTInst = true; }
    void SetDataMask(DataMask dm) { this->mask = dm; }
    ROBID GetBid() { return bid; }
    ROBID GetGid() { return gid; }
    ROBID GetRid() { return rid; }
    DataMask GetDataMask() { return mask; }
    uint32_t GetTid() { return tid; }
    uint32_t GetStid() const { return stid; }
    bool GetIsCTInst() const {return fromCTInst;}
public:
    VecTileRegStReq() : reqId(0), size(0), dest(0), tid(0), mask(1) {}
    VecTileRegStReq(uint32_t reqId, uint32_t size, uint64_t dest)
        : reqId(reqId), size(size), dest(dest), tid(0), mask(1) {}
private:
    bool isVgather = false;
    uint32_t groupSlotId = 0;
    uint32_t reqId = 0;
    uint32_t size = 0;
    uint64_t dest = 0;
    uint8_t data[MAX_TILE_DATA_BYTE] = {0}; // 2048bits
    uint32_t coreId = 0;
    ROBID bid;
    ROBID gid;
    ROBID rid;
    uint32_t tid;
    uint32_t stid = 0;
    bool fromCTInst = false;
    bool isLast = false;
    DataMask mask;
};

inline std::ostream& operator<<(std::ostream& out, VecTileRegStReq req)
{
    out << std::dec << "reqId=" << req.GetReqId() << ", size=" << req.GetSize();
    out << std::dec << ", B" << req.GetBid() << "-G" << req.GetGid() << "-T" << req.GetTid() << "-R" << req.GetRid();
    out << std::hex << ", dest=0x" << req.GetDest();
    out << "]";
    return out;
}


} // namespace JCore

#endif  // BLOCKISA_MODEL_INTERFACE_VECTILEREGSTREQ_H
