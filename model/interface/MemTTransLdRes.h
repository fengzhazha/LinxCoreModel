#ifndef BLOCKISA_MODEL_INTERFACE_MEMTTRANSLDRES_H
#define BLOCKISA_MODEL_INTERFACE_MEMTTRANSLDRES_H

#include <iostream>

#include "ConstConfig.h"

namespace JCore {


// TTRANS_MEM_LD_RESP
class MemTTransLdRes {
public:
    uint32_t GetCoreId() const { return coreId; }
    void SetCoreId(uint32_t value) { coreId = value; }
    uint32_t GetReqId() const { return reqId; }
    void SetReqId(uint32_t value) { reqId = value; }
    uint8_t *GetData() { return data; }
    void SetData(uint8_t *value)
    {
        for (uint32_t i = 0; i < MAX_TILE_DATA_BYTE; i++) {
            data[i] = value[i];
        }
    }
    void GetData(uint8_t *value)
    {
        for (uint32_t i = 0; i < MAX_TILE_DATA_BYTE; i++) {
            value[i] = data[i];
        }
    }
public:
    MemTTransLdRes() : reqId(0) {}
    MemTTransLdRes(uint32_t reqId) : reqId(reqId) {}
private:
    uint32_t coreId = 0;
    uint32_t reqId;
    uint8_t data[32 * 8]; // 2048 bits
};

inline std::ostream& operator<<(std::ostream& out, const MemTTransLdRes& req)
{
    out << std::dec << "reqId=" << req.GetReqId();
    return out;
}


} // namespace JCore

#endif  // BLOCKISA_MODEL_INTERFACE_TTRANSMEMLDRES_H
