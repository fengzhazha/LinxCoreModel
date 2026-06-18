#ifndef BLOCKISA_MODEL_VECLITE_UOP_INFO_H
#define BLOCKISA_MODEL_VECLITE_UOP_INFO_H
#include <cstdint>
#include <map>

#include "ISACommon/TileOpManager.h"

namespace JCore {

struct VectorLiteUopInfo {
    enum class ExePipe {
        FMLA = 0,
        IALU,
        FDIV,
        PERMUTE,
        IMAC,
        FCVT,
        NUM,
        UNDEF = 0xff
    };
    std::map<TileOp, uint32_t> latencyTable;
    std::map<TileOp, ExePipe> pipeTable;

    uint32_t GetLatency(TileOp tileOp);
    ExePipe GetPipe(TileOp tileOp);

    static VectorLiteUopInfo& Inst()
    {
        static VectorLiteUopInfo inst;
        return inst;
    }

private:
    VectorLiteUopInfo();
    ~VectorLiteUopInfo() = default;
};

} // namespace JCore

#endif // BLOCKISA_MODEL_VECLITE_UOP_INFO_H
