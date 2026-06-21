#include "Compound.h"

namespace JCore {
namespace Calculate {
namespace Compound {

static bool CalcInstCSEL(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }

    uint64_t srcP = inst.srcs[SRC0_IDX]->data;
    uint64_t srcL = inst.srcs[SRC1_IDX]->data;
    uint64_t srcR = inst.srcs[SRC2_IDX]->data;
    if (srcP == 0) {
        inst.dsts[DST0_IDX]->data = srcL;
    } else {
        inst.dsts[DST0_IDX]->data = srcR;
    }
    return true;
}

static bool CalcInstPSEL(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }

    bool srcP = GetBit(inst.srcs[SRC0_IDX]->data, inst.laneID);
    uint64_t srcL = inst.srcs[SRC1_IDX]->data;
    uint64_t srcR = inst.srcs[SRC2_IDX]->data;
    if (!srcP) {
        inst.dsts[DST0_IDX]->data = srcL;
    } else {
        inst.dsts[DST0_IDX]->data = srcR;
    }
    return true;
}

Handler Lookup(Opcode op)
{
    static const std::unordered_map<Opcode, Handler> COMPOUND_TABLE = std::unordered_map<Opcode, Handler> {
        {Opcode::OP_CSEL, &CalcInstCSEL},
        {Opcode::OP_PSEL, &CalcInstPSEL},
    };
    auto it = COMPOUND_TABLE.find(op);
    return (it == COMPOUND_TABLE.end()) ? 0 : it->second;
}

bool CalculateCompound(MInst &inst)
{
    Handler fn = Lookup(inst.opcode);
    return fn ? fn(inst) : false;
}

} // namespace Compound
} // namespace Calculate
} // namespace JCore
