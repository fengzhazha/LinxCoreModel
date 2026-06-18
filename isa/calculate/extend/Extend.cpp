#include "Extend.h"

#include <cstdint>

namespace JCore {
namespace Calculate {
namespace Extend {

static bool CalcInstSEXTB(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    int8_t v = static_cast<int8_t>(inst.srcs[SRC0_IDX]->data & MASK_BIT8);
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(static_cast<int64_t>(v));
    return true;
}

static bool CalcInstSEXTH(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    int16_t v = static_cast<int16_t>(inst.srcs[SRC0_IDX]->data & MASK_BIT16);
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(static_cast<int64_t>(v));
    return true;
}

static bool CalcInstSEXTW(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    int32_t v = static_cast<int32_t>(inst.srcs[SRC0_IDX]->data & MASK_BIT32);
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(static_cast<int64_t>(v));
    return true;
}

static bool CalcInstZEXTB(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(inst.srcs[SRC0_IDX]->data & MASK_BIT8);
    return true;
}

static bool CalcInstZEXTH(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(inst.srcs[SRC0_IDX]->data & MASK_BIT16);
    return true;
}

static bool CalcInstZEXTW(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(inst.srcs[SRC0_IDX]->data & MASK_BIT32);
    return true;
}

Handler Lookup(Opcode op)
{
    static const std::unordered_map<Opcode, Handler> EXTEND_TABLE = std::unordered_map<Opcode, Handler> {
        {Opcode::OP_SEXT_B, &CalcInstSEXTB},
        {Opcode::OP_SEXT_H, &CalcInstSEXTH},
        {Opcode::OP_SEXT_W, &CalcInstSEXTW},
        {Opcode::OP_ZEXT_B, &CalcInstZEXTB},
        {Opcode::OP_ZEXT_H, &CalcInstZEXTH},
        {Opcode::OP_ZEXT_W, &CalcInstZEXTW},
    };
    auto it = EXTEND_TABLE.find(op);
    return (it == EXTEND_TABLE.end()) ? 0 : it->second;
}

bool CalculateExtend(MInst &inst)
{
    Handler fn = Lookup(inst.opcode);
    return fn ? fn(inst) : false;
}

} // namespace Extend
} // namespace Calculate
} // namespace JCore