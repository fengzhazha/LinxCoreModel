#include "CompareFP.h"

#include "calculate/FloatPointUtils.h"
#include "softfloat.h"

namespace JCore {
namespace Calculate {
namespace CompareFP {

static bool CalcInstFEQ(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = Feq(inst.srcs[SRC0_IDX]->cvt, inst.srcs[SRC0_IDX]->data, inst.srcs[SRC1_IDX]->data,
                                    float_round_nearest_even);
    return true;
}

static bool CalcInstFNE(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = Feq(inst.srcs[SRC0_IDX]->cvt, inst.srcs[SRC0_IDX]->data, inst.srcs[SRC1_IDX]->data,
                                    float_round_nearest_even) ^ 1;
    return true;
}

static bool CalcInstFLT(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = Flt(inst.srcs[SRC0_IDX]->cvt, inst.srcs[SRC0_IDX]->data, inst.srcs[SRC1_IDX]->data,
                                    float_round_nearest_even);
    return true;
}

static bool CalcInstFGE(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = Flt(inst.srcs[SRC0_IDX]->cvt, inst.srcs[SRC0_IDX]->data, inst.srcs[SRC1_IDX]->data,
                                    float_round_nearest_even) ^ 1;
    return true;
}

static bool CalcInstFEQS(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = Feqs(inst.srcs[SRC0_IDX]->cvt, inst.srcs[SRC0_IDX]->data, inst.srcs[SRC1_IDX]->data,
                                     float_round_nearest_even);
    return true;
}

static bool CalcInstFNES(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = Feqs(inst.srcs[SRC0_IDX]->cvt, inst.srcs[SRC0_IDX]->data, inst.srcs[SRC1_IDX]->data,
                                     float_round_nearest_even) ^ 1;
    return true;
}

static bool CalcInstFLTS(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = Flts(inst.srcs[SRC0_IDX]->cvt, inst.srcs[SRC0_IDX]->data, inst.srcs[SRC1_IDX]->data,
                                     float_round_nearest_even);
    return true;
}
static bool CalcInstFGES(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = Flts(inst.srcs[SRC0_IDX]->cvt, inst.srcs[SRC0_IDX]->data, inst.srcs[SRC1_IDX]->data,
                                     float_round_nearest_even) ^ 1;
    return true;
}

Handler Lookup(Opcode op)
{
    static const std::unordered_map<Opcode, Handler> COMPAREFP_TABLE = std::unordered_map<Opcode, Handler> {
        {Opcode::OP_FEQ, &CalcInstFEQ},
        {Opcode::OP_FNE, &CalcInstFNE},
        {Opcode::OP_FLT, &CalcInstFLT},
        {Opcode::OP_FGE, &CalcInstFGE},
        {Opcode::OP_FEQS, &CalcInstFEQS},
        {Opcode::OP_FNES, &CalcInstFNES},
        {Opcode::OP_FLTS, &CalcInstFLTS},
        {Opcode::OP_FGES, &CalcInstFGES},
    };
    auto it = COMPAREFP_TABLE.find(op);
    return (it == COMPAREFP_TABLE.end()) ? 0 : it->second;
}

bool CalculateCompareFP(MInst &inst)
{
    Handler fn = Lookup(inst.opcode);
    return fn ? fn(inst) : false;
}

} // namespace CompareFP
} // namespace Calculate
} // namespace JCore