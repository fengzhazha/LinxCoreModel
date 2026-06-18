#include "interface/ConstConfig.h"

#ifndef BLOCKISA_MODEL_INTERFACE_BCCMMUSTREQ_H
#define BLOCKISA_MODEL_INTERFACE_BCCMMUSTREQ_H

namespace JCore {


// BCC_MMU_REQ
class BCCMMUStReq {
public:
    uint8_t GetTyp() const { return typ; }
    void SetTyp(uint8_t value) { typ = value; }
    uint32_t GetReqId() const { return reqId; }
    void SetReqId(uint32_t value) { reqId = value; }
    uint64_t GetVa() const { return va; }
    void SetVa(uint64_t value) { va = value; }
public:
    BCCMMUStReq() : typ(0), reqId(0), va(INVALID_PC) {}
    BCCMMUStReq(uint8_t typ, uint32_t reqId, uint64_t va)
        : typ(typ), reqId(reqId), va(va) {}
private:
    uint8_t typ;  // ld or st TODO: Convert to Enumeration Type
    uint32_t reqId;
    uint64_t va;
};


} // namespace JCore

#endif  // BLOCKISA_MODEL_INTERFACE_BCCMMUSTREQ_H
