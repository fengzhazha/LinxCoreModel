#include "Others.h"

namespace JCore {
namespace Calculate {
namespace Others {

static bool CalcInstMove(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data;
    return true;
}

static bool CalcInstPrefetch(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX) {
        return false;
    }
    inst.accMemInfo->accMemAddr = inst.srcs[SRC0_IDX]->data;
    return true;
}

static bool CalcInstACRC(MInst &inst)
{
    return inst.srcs.size() == SRC1_IDX;
}

Handler Lookup(Opcode op)
{
    static const std::unordered_map<Opcode, Handler> OTHERS_TABLE = std::unordered_map<Opcode, Handler> {
        /* Move */
        {Opcode::OP_MOVR, &CalcInstMove},
        {Opcode::OP_MOVI, &CalcInstMove},
        /* Prefetch */
        {Opcode::OP_PRF, &CalcInstPrefetch},
        {Opcode::OP_PRF_A, &CalcInstPrefetch},
        {Opcode::OP_PRFI_U, &CalcInstPrefetch},
        {Opcode::OP_PRFI_UA, &CalcInstPrefetch},
        /* Execute control */
        {Opcode::OP_ACRC, &CalcInstACRC},
    };
    auto it = OTHERS_TABLE.find(op);
    return (it == OTHERS_TABLE.end()) ? 0 : it->second;
}

bool CalculateOthers(MInst &inst)
{
    Handler fn = Lookup(inst.opcode);
    return fn ? fn(inst) : false;
}

} // namespace Others
} // namespace Calculate
} // namespace JCore