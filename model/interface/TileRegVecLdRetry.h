#ifndef BLOCKISA_MODEL_INTERFACE_TILEREGVECLDRETRY_H
#define BLOCKISA_MODEL_INTERFACE_TILEREGVECLDRETRY_H

#include <iostream>
#include "core/Bus.h"

namespace JCore {


// TILEREG_VEC_LD_RETRY
class TileRegVecLdRetry {
public:
    uint32_t GetRespId() const { return respId; }
    void SetRespId(uint32_t value) { respId = value; }
    void SetBid(ROBID bid) { this->bid = bid; }
    void SetGid(ROBID gid) { this->gid = gid; }
    void SetRid(ROBID rid) { this->rid = rid; }
    void SetTid(uint32_t tid) { this->tid = tid; }
    ROBID GetBid() { return bid; }
    ROBID GetGid() { return gid; }
    ROBID GetRid() { return rid; }
    uint32_t GetTid() { return tid; }
public:
    TileRegVecLdRetry() : respId(0) {}
    TileRegVecLdRetry(uint32_t respId) : respId(respId) {}
private:
    uint32_t respId; // 3 bits
    ROBID bid;
    ROBID gid;
    ROBID rid;
    uint32_t tid;
};

inline std::ostream& operator<<(std::ostream& out, const TileRegVecLdRetry& req)
{
    out << std::dec << "respId=" << req.GetRespId();
    return out;
}


} // namespace JCore

#endif  // BLOCKISA_MODEL_INTERFACE_TILEREGVECLDRETRY_H
