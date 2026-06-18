#ifndef BLOCKISA_MODEL_INTERFACE_TILEREGVECLDRES_H
#define BLOCKISA_MODEL_INTERFACE_TILEREGVECLDRES_H

#include <cstdint>
#include <cstring>
#include <iostream>
#include <ostream>
#include <string>
#include "interface/ConstConfig.h"
#include "ISACommon/TileOpManager.h"
#include "ISACommon/OperandType.h"
#include "ModelCommon/ROBID.h"
#include "ModelCommon/ModelCommon.h"
#include "core/Bus.h"
#include "utils/error.h"

namespace JCore {


// TILEREG_VEC_LD_RSP
class TileRegVecLdRes {
    const static uint32_t RES_DATA_SIZE = 32 * 8;
public:
    void SetTagId(uint32_t tagIdValue, OperandType handTypeValue)
    {
        tagId = tagIdValue;
        handType = handTypeValue;
    }
    uint32_t GetTagId() const { return tagId; }
    OperandType GetHandType() const { return handType; }
    uint32_t GetRespId() const { return respId; }
    void SetRespId(uint32_t value) { respId = value; }
    void SetSrc(uint64_t value) { src = value; }
    const uint8_t* GetData(uint32_t &size) const {return data;}
    uint8_t *GetData() { return data; }
    void SetData(const uint8_t* wdat, uint32_t size = MAX_TILE_DATA_BYTE)
    {
        ASSERT(size <= MAX_TILE_DATA_BYTE);
        memcpy(data, wdat, size);
    }
    void GetData(uint8_t *value)
    {
        for (uint32_t i = 0; i < MAX_TILE_DATA_BYTE; i++) {
            value[i] = data[i];
        }
    }
    void SetBid(ROBID bid) { this->bid = bid; }
    void SetGid(ROBID gid) { this->gid = gid; }
    void SetRid(ROBID rid) { this->rid = rid; }
    void SetTid(uint32_t tid) { this->tid = tid; }
    void SetStid(uint32_t stId) { this->stid = stId; }
    void SetReqCnt(uint32_t cnt) { this->loadCnt = cnt;}
    void SetWay(unsigned way) { this->wayId = way;}
    void SetPrefetch(bool state) { this->isPrefetch = state;}
    ROBID GetBid() { return bid; }
    ROBID GetGid() { return gid; }
    ROBID GetRid() { return rid; }
    uint32_t GetTid() { return tid; }
    uint32_t GetStid() const { return stid; }
    unsigned GetLdReqCnt() {return loadCnt;}
    bool IsPrefetch() const {return isPrefetch;}
    unsigned GetWay(bool enableAssert = true)
    {
        if (enableAssert) {
            ASSERT(wayId != -1U);
        }
        return wayId;
    }
    uint64_t GetAddr() const {return src;}
public:
    TileRegVecLdRes() : respId(0), tid(0) {}
    TileRegVecLdRes(uint32_t respId) : respId(respId), tid(0) {}
private:
    uint32_t respId = 0;
    uint64_t src;
    uint8_t data[MAX_TILE_DATA_BYTE];  // 2048 bits
    ROBID bid;
    ROBID gid;
    ROBID rid;
    bool isPrefetch = false;
    uint32_t tid = 0;
    uint32_t stid = 0;
    uint32_t loadCnt = 0;
    unsigned wayId = -1U;
    uint32_t tagId = 0;
    OperandType handType = OperandType::OPD_INVALID;
};

inline std::ostream& operator<<(std::ostream& out, TileRegVecLdRes& req)
{
    out << std::dec << "respId=" << req.GetRespId() << ",";
    out << std::dec << "wayId=" << req.GetWay(false) << ",";
    uint8_t *data = req.GetData();
    out << "data=[";
    for (uint32_t i = 0; i < MAX_TILE_DATA_BYTE; i++) {
        out << std::hex << static_cast<int>(data[i]);
        if (i < MAX_TILE_DATA_BYTE - 1) {
            out << ",";
        }
    }
    out << "]";
    return out;
}


} // namespace JCore

#endif  // BLOCKISA_MODEL_INTERFACE_TILEREGVECLDRES_H
