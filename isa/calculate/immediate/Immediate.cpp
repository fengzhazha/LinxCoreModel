#include "Immediate.h"

namespace JCore {
namespace Calculate {
namespace Immediate {

static bool CalcInstLUI(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    switch (inst.codeLen) {
        case EncodeLen::ENL_W:
            inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data & (~MASK_BIT12);
            break;
        case EncodeLen::ENL_H:
            inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data & (~MASK_BIT32);
            break;
        default:
            inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data;
    }
    return true;
}

static bool CalcInstLIS(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    int32_t v = static_cast<int32_t>(inst.srcs[SRC0_IDX]->data & MASK_BIT32);
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(static_cast<int64_t>(v));
    return true;
}

static bool CalcInstLIU(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    int32_t v = static_cast<int32_t>(inst.srcs[SRC0_IDX]->data & MASK_BIT32);
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(static_cast<int64_t>(v));
    return true;
}

static bool CalcInstAdd(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data + inst.srcs[SRC1_IDX]->data;
    return true;
}

Handler Lookup(Opcode op)
{
    static const std::unordered_map<Opcode, Handler> IMMEDIATE_TABLE = std::unordered_map<Opcode, Handler> {
        {Opcode::OP_LUI, &CalcInstLUI},
        {Opcode::OP_LIS, &CalcInstLIS},
        {Opcode::OP_LIU, &CalcInstLIU},
        {Opcode::OP_ADDLI, &CalcInstAdd},
    };
    auto it = IMMEDIATE_TABLE.find(op);
    return (it == IMMEDIATE_TABLE.end()) ? 0 : it->second;
}

bool CalculateImmediate(MInst &inst)
{
    Handler fn = Lookup(inst.opcode);
    return fn ? fn(inst) : false;
}

} // namespace Immediate
} // namespace Calculate
} // namespace JCore