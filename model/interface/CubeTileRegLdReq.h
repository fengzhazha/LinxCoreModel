#ifndef BLOCKISA_MODEL_INTERFACE_CUBETILEREGLDREQ_H
#define BLOCKISA_MODEL_INTERFACE_CUBETILEREGLDREQ_H

#include <iostream>

namespace JCore {


// CUBE_TILEREG_LD_REQ
class CubeTileRegLdReq {
public:
    uint32_t GetReqId() const { return reqId; }
    void SetReqId(uint32_t value) { reqId = value; }
    uint32_t GetSize() const { return size; }
    void SetSize(uint32_t value) { size = value; }
    uint32_t GetSrc() const { return src; }
    uint32_t GetStid() const { return stid; }
    void SetSrc(uint32_t value) { src = value; }
    uint8_t GetDestBuf() const { return destBuf; }
    void SetDestBuf(uint8_t value) { destBuf = value; }
    uint32_t GetCoreId() const { return coreId; }
    void SetCoreId(uint32_t value) { coreId = value; }
    void SetStid(uint32_t id) { stid = id; }
    // Cube pipeview: Show tmu load from which part of src matrix
    uint64_t cubeUopIndex = 0;
    bool isL0ALoad = false;
    bool isL0BLoad = false;
    bool isL0CLoad = false;
    std::pair<uint64_t, uint64_t> cubeTileIndex;
public:
    CubeTileRegLdReq() : reqId(0), size(0), src(0), destBuf(0) {}
    CubeTileRegLdReq(uint32_t reqId, uint32_t size, uint16_t src, uint8_t destBuf)
        : reqId(reqId), size(size), src(src), destBuf(destBuf) {}
private:
    uint32_t reqId;
    uint32_t size;
    uint32_t src;
    uint8_t destBuf; // TODO: Convert to Enumeration Type
    uint32_t coreId = 0;
    uint32_t stid = 0;
};

inline std::ostream& operator<<(std::ostream& out, const CubeTileRegLdReq& req)
{
    out << std::dec << "reqId=" << req.GetReqId() << ", size=" << req.GetSize();
    out << std::hex << ", src=0x" << req.GetSrc();
    out << std::dec << ", destBuf=" << static_cast<uint32_t>(req.GetDestBuf());
    return out;
}


} // namespace JCore

#endif  // BLOCKISA_MODEL_INTERFACE_CUBETILEREGLDREQ_H
