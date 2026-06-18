#include "interface/ConstConfig.h"

// BCC_MEM_LD_REQ
#ifndef BLOCKISA_MODEL_INTERFACE_BCCMEMLDREQ_H
#define BLOCKISA_MODEL_INTERFACE_BCCMEMLDREQ_H

namespace JCore {


class BCCMemLdReq {
public:
    uint32_t GetReqId() const { return reqId; }
    void SetReqId(uint32_t value) { reqId = value; }
    uint8_t GetOpcode() const { return opcode; }
    void SetOpcode(uint8_t value) { opcode = value; }
    uint64_t GetAddr() const { return addr; }
    void SetAddr(uint64_t value) { addr = value; }
public:
    BCCMemLdReq() : reqId(0), opcode(0), addr(INVALID_PC) {}
    BCCMemLdReq(uint32_t reqId, uint8_t opcode, uint64_t addr)
        : reqId(reqId), opcode(opcode), addr(addr) {}
private:
    uint32_t reqId = 0UL;
    uint8_t opcode = 0;
    uint64_t addr = 0ULL;
};


} // namespace JCore

#endif  // BLOCKISA_MODEL_INTERFACE_BCCMEMLDREQ_H
