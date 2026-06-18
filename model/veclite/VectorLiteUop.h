#ifndef BLOCKISA_MODEL_VECLITE_UOP_H
#define BLOCKISA_MODEL_VECLITE_UOP_H
#include <atomic>
#include <memory>

#include "core/Bus.h"

namespace JCore {

// All UOPs use fixed 512B granularity
constexpr uint64_t UOP_GRANULARITY = 512;  // 512B

class InstPipeViewInfo;
using InstPipeViewPtr = std::shared_ptr<InstPipeViewInfo>;

struct VectorLiteUop {
    uint64_t uid { 0 };
    uint32_t idx { 0 }; // slice index after split
    BlockCommandPtr cmd { nullptr };
    TileOp tileOp { TileOp::TINVALID };

    // Fixed 512B granularity
    uint64_t size { UOP_GRANULARITY };  // Always 512B
    uint64_t offset { 0 };               // Offset in original TileOP
    uint32_t sliceIdx { 0 };             // Index of this slice (0, 1, 2, ...)
    uint32_t totalSlices { 1 };          // Total number of slices
    bool colSumLocked { false };
    CycleBus cycleInfo { nullptr };
    uint32_t readRegLatency { 0 };
    uint32_t latency { 0 };             // execution pipe latency

    VectorLiteUop(uint32_t idxVal, const BlockCommandPtr& bcmdVal, TileOp tileOpVal);
    ~VectorLiteUop();

    std::string Dump() const;
    InstPipeViewPtr GetPipeViewInfo();

    static std::atomic<uint64_t> nextUid;
    static uint64_t GetNextUid();
};
using VectorLiteUopT = std::shared_ptr<VectorLiteUop>;

} // namespace JCore

#endif // BLOCKISA_MODEL_VECLITE_UOP_H
