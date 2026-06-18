#ifndef BLOCKISA_MODEL_INTERFACE_MEMBCCLDRES_H
#define BLOCKISA_MODEL_INTERFACE_MEMBCCLDRES_H

namespace JCore {


// BCC_MEM_LD_RESP
class MemBCCLdRes {
public:
    uint32_t GetReqId() const { return reqId; }
    void SetReqId(uint32_t value) { reqId = value; }
public:
    MemBCCLdRes() : reqId(0), data{0} {}
    MemBCCLdRes(uint32_t reqId) : reqId(reqId) {}
private:
    uint32_t reqId;
    uint64_t data[8]; // 512 bits
};


} // namespace JCore

#endif  // BLOCKISA_MODEL_INTERFACE_MemBCCLdRes_H
