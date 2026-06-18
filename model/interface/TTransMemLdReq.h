#include <iostream>

#include "interface/ConstConfig.h"
#include "core/Bus.h"

#ifndef BLOCKISA_MODEL_INTERFACE_TTRANSMEMLDREQ_H
#define BLOCKISA_MODEL_INTERFACE_TTRANSMEMLDREQ_H

namespace JCore {


// TTRANS_MEM_LD_REQ
class TTransMemLdReq {
public:
    uint32_t GetReqId() const { return reqId; }
    void SetReqId(uint32_t value) { reqId = value; }
    uint64_t GetAddr() const { return addr; }
    void SetAddr(uint64_t value) { addr = value; }
    uint32_t GetSize() const { return size; }
    void SetSize(uint32_t value) { size = value; }
    uint8_t GetOpcode() const { return opcode; }
    void SetOpcode(uint8_t value) { opcode = value; }
    uint32_t GetCoreId() const { return coreId; }
    void SetCoreId(uint32_t value) { coreId = value; }
    void SetBid(ROBID bid) { this->bid = bid; }
    ROBID GetBid() { return bid; }
public:
    TTransMemLdReq() : reqId(0), addr(INVALID_PC), size(0), opcode(0) {}
    TTransMemLdReq(uint32_t reqId, uint64_t addr, uint32_t size, uint8_t opcode)
        : reqId(reqId), addr(addr), size(size), opcode(opcode) {}
private:
    uint32_t reqId;
    uint64_t addr;
    uint32_t size;   // TODO: Convert to Enumeration Type
    uint8_t opcode; // TODO: Convert to Enumeration Type
    uint32_t coreId = 0;
    // For flush
    ROBID bid;
};

inline std::ostream& operator<<(std::ostream& out, const TTransMemLdReq& req)
{
    out << std::dec << "reqId=" << req.GetReqId() << ", ";
    out << std::hex << "addr=" << req.GetAddr() << ", ";
    out << std::dec << " size=" << req.GetSize() << ", opcode=" << req.GetOpcode();
    return out;
}


} // namespace JCore

#endif  // BLOCKISA_MODEL_INTERFACE_TTRANSMEMLDREQ_H
