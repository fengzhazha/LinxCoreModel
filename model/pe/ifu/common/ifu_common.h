#pragma once

#include <vector>

#include "../configs/ifu_config.h"
#include "core/Bus.h"
#include "ModelSpec.h"
#include "SimSys.h"
#include "pe/ifu/common/ifu_stats.h"
#include "ModelCommon/SimInstInfo.h"

namespace JCore {


namespace PE_IFU {

constexpr uint64_t ADDR_MASK = 0xFFFFFFFFFFFFFFC0;

enum IFUStage {
    PF0 = 0, PF1, PF2, PF3, NSTAGE, NIL
};

enum PipeState {
    INVALID = 0, VALID, STALL,
};

struct Pipe {
    PipeState state;
    std::vector<FetchReqPtr> fetchReq;
    bool pipeDone = false;
    Pipe() : state(INVALID) {};
    Pipe(uint32_t size) : state(INVALID), fetchReq(size, nullptr), pipeDone(false) {};
    void Flush()
    {
        state = INVALID;
        std::fill(fetchReq.begin(), fetchReq.end(), nullptr);
    }
    void Build(uint32_t size)
    {
        fetchReq.resize(size);
        state = INVALID;
    }
};


struct MissQEntry {
    uint64_t    addr;
    bool        sent;
    bool        isPref; // request type
    time_t      enqueueTime; // for timeout check
    MissQEntry(uint64_t addr, bool isPref, time_t enqueueTime)
        : addr(addr), sent(false), isPref(isPref), enqueueTime(enqueueTime) {}
};

struct BranchEntry {
    uint64_t                            src;
    uint64_t                            dst;
};

enum PredictorState {
    S_NT,
    W_NT,
    W_T,
    S_T
};

struct PredictEntry {
    PredictorState                      state = W_T;
    uint64_t                            dst = 0;
};

struct FlushFrontInfo {
    ROBID bid;
    ROBID gid;
    bool flush = false;
    bool flushf3 = false;
    uint64_t tpc = 0;
    uint32_t tid = 0;;
    uint64_t fid = 0;
    uint32_t stid = 0;
};

enum InnerBrType {
    NOBRANCH,
    BEND,
    INNER_DIRECT,
    INNER_INDIRECT,
    INNER_COND
};

inline InnerBrType getInstBrType(uint64_t bin, BlockType btype, uint64_t width)
{
    MInst inst = MInst();
    (void)btype;
    inst.DecodeBin(bin, width);

    switch (inst.opcode) {
        case Opcode::OP_BSTOP:
            return BEND;
        case Opcode::OP_J:
            return INNER_DIRECT;
        case Opcode::OP_JR:
            return INNER_INDIRECT;
        case Opcode::OP_B_EQ:
        case Opcode::OP_B_NE:
        case Opcode::OP_B_LT:
        case Opcode::OP_B_GE:
        case Opcode::OP_B_LTU:
        case Opcode::OP_B_GEU:
            return INNER_COND;
        default:
            return NOBRANCH;
    }
    return NOBRANCH;
}
}

} // namespace JCore
