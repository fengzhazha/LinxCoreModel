#include "MaxMin.h"

#include "calculate/FloatPointUtils.h"
#include "softfloat.h"

namespace JCore {
namespace Calculate {
namespace MaxMin {

static bool CalcInstMax(MInst &inst)  // 有符号最大值
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    int64_t a = static_cast<int64_t>(inst.srcs[SRC0_IDX]->data);
    int64_t b = static_cast<int64_t>(inst.srcs[SRC1_IDX]->data);
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(std::max(a, b));
    return true;
}

static bool CalcInstMaxU(MInst &inst) // 无符号最大值
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    uint64_t a = inst.srcs[SRC0_IDX]->data;
    uint64_t b = inst.srcs[SRC1_IDX]->data;
    inst.dsts[DST0_IDX]->data = std::max(a, b);
    return true;
}

static bool CalcInstMin(MInst &inst)  // 有符号最小值
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    int64_t a = static_cast<int64_t>(inst.srcs[SRC0_IDX]->data);
    int64_t b = static_cast<int64_t>(inst.srcs[SRC1_IDX]->data);
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(std::min(a, b));
    return true;
}

static bool CalcInstMinU(MInst &inst) // 无符号最小值
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    uint64_t a = inst.srcs[SRC0_IDX]->data;
    uint64_t b = inst.srcs[SRC1_IDX]->data;
    inst.dsts[DST0_IDX]->data = std::min(a, b);
    return true;
}

static bool CalcInstFMax(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = Fmax(inst.srcs[SRC0_IDX]->cvt, inst.srcs[SRC0_IDX]->data, inst.srcs[SRC1_IDX]->data,
                                     float_round_nearest_even);
    return true;
}

static bool CalcInstFMin(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = Fmin(inst.srcs[SRC0_IDX]->cvt, inst.srcs[SRC0_IDX]->data, inst.srcs[SRC1_IDX]->data,
                                     float_round_nearest_even);
    return true;
}

Handler Lookup(Opcode op)
{
    static const std::unordered_map<Opcode, Handler> MAXMIN_TABLE = std::unordered_map<Opcode, Handler> {
        {Opcode::OP_MAX, &CalcInstMax},
        {Opcode::OP_MAXU, &CalcInstMaxU},
        {Opcode::OP_MIN, &CalcInstMin},
        {Opcode::OP_MINU, &CalcInstMinU},
        {Opcode::OP_FMAX, &CalcInstFMax},
        {Opcode::OP_FMIN, &CalcInstFMin},
    };
    auto it = MAXMIN_TABLE.find(op);
    return (it == MAXMIN_TABLE.end()) ? 0 : it->second;
}

bool CalculateMaxMin(MInst &inst)
{
    Handler fn = Lookup(inst.opcode);
    return fn ? fn(inst) : false;
}

} // namespace MaxMin
} // namespace Calculate
} // namespace JCore