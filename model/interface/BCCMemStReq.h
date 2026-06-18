#include "interface/ConstConfig.h"

#ifndef BLOCKISA_MODEL_INTERFACE_BCCMEMSTREQ_H
#define BLOCKISA_MODEL_INTERFACE_BCCMEMSTREQ_H

namespace JCore {


// BCC_MEM_ST_REQ
class BCCMemStReq {
public:
    uint32_t GetReqId() const { return reqId; }
    void SetReqId(uint32_t value) { reqId = value; }
    uint8_t GetOpcode() const { return opcode; }
    void SetOpcode(uint8_t value) { opcode = value; }
    uint64_t GetAddr() const { return addr; }
    void SetAddr(uint64_t value) { addr = value; }
public:
    BCCMemStReq() : reqId(0), opcode(0), addr(INVALID_PC) {}
    BCCMemStReq(uint32_t reqId, uint8_t opcode, uint64_t addr)
        : reqId(reqId), opcode(opcode), addr(addr) {}
private:
    uint32_t reqId;
    uint8_t opcode;  // TODO: Convert to Enumeration Type
    uint64_t addr;
    uint8_t data[8 * 8]; // 512 bits
};


} // namespace JCore

#endif  // BLOCKISA_MODEL_INTERFACE_BCCMEMSTREQ_H
