#ifndef BLOCKISA_MODEL_INTERFACE_MEMTTRANSSTRES_H
#define BLOCKISA_MODEL_INTERFACE_MEMTTRANSSTRES_H

#include <iostream>

namespace JCore {


// TTRANS_MEM_ST_RESP
class MemTTransStRes {
public:
    uint32_t GetCoreId() const { return coreId; }
    void SetCoreId(uint32_t value) { coreId = value; }
    uint32_t GetReqId() const { return reqId; }
    void SetReqId(uint32_t value) { reqId = value; }
    uint32_t GetRsp() const { return rsp; }
    void SetRsp(uint32_t value) { rsp = value; }
public:
    MemTTransStRes() : reqId(0), rsp(0) {}
    MemTTransStRes(uint32_t reqId, uint32_t rsp) : reqId(reqId), rsp(rsp) {}
private:
    uint32_t coreId = 0;
    uint32_t reqId;
    uint32_t rsp;
};

inline std::ostream& operator<<(std::ostream& out, const MemTTransStRes& req)
{
    out << std::dec << "reqId=" << req.GetReqId() << ", rsp=" << req.GetRsp();
    return out;
}


} // namespace JCore

#endif  // BLOCKISA_MODEL_INTERFACE_TTRANSMEMSTRES_H
