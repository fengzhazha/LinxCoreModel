#include "Compare.h"

namespace JCore {
namespace Calculate {
namespace Compare {

static bool CalcCmpEQ(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (inst.srcs[SRC0_IDX]->data == inst.srcs[SRC1_IDX]->data) {
        inst.dsts[DST0_IDX]->data = 1;
    } else {
        inst.dsts[DST0_IDX]->data = 0;
    }
    return true;
}

static bool CalcCmpNE(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (inst.srcs[SRC0_IDX]->data != inst.srcs[SRC1_IDX]->data) {
        inst.dsts[DST0_IDX]->data = 1;
    } else {
        inst.dsts[DST0_IDX]->data = 0;
    }
    return true;
}

static bool CalcCmpAnd(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if ((inst.srcs[SRC0_IDX]->data & inst.srcs[SRC1_IDX]->data) != 0) {
        inst.dsts[DST0_IDX]->data = 1;
    } else {
        inst.dsts[DST0_IDX]->data = 0;
    }
    return true;
}

static bool CalcCmpOr(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if ((inst.srcs[SRC0_IDX]->data | inst.srcs[SRC1_IDX]->data) != 0) {
        inst.dsts[DST0_IDX]->data = 1;
    } else {
        inst.dsts[DST0_IDX]->data = 0;
    }
    return true;
}

static bool CalcCmpLessThan(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (static_cast<int64_t>(inst.srcs[SRC0_IDX]->data) < static_cast<int64_t>(inst.srcs[SRC1_IDX]->data)) {
        inst.dsts[DST0_IDX]->data = 1;
    } else {
        inst.dsts[DST0_IDX]->data = 0;
    }
    return true;
}

static bool CalcCmpLessThanU(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (inst.srcs[SRC0_IDX]->data < inst.srcs[SRC1_IDX]->data) {
        inst.dsts[DST0_IDX]->data = 1;
    } else {
        inst.dsts[DST0_IDX]->data = 0;
    }
    return true;
}

static bool CalcCmpGraterThanOrEqual(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (static_cast<int64_t>(inst.srcs[SRC0_IDX]->data) >= static_cast<int64_t>(inst.srcs[SRC1_IDX]->data)) {
        inst.dsts[DST0_IDX]->data = 1;
    } else {
        inst.dsts[DST0_IDX]->data = 0;
    }
    return true;
}

static bool CalcCmpGraterThanOrEqualU(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (inst.srcs[SRC0_IDX]->data >= inst.srcs[SRC1_IDX]->data) {
        inst.dsts[DST0_IDX]->data = 1;
    } else {
        inst.dsts[DST0_IDX]->data = 0;
    }
    return true;
}


Handler Lookup(Opcode op)
{
    static const std::unordered_map<Opcode, Handler> COMPARE_TABLE = std::unordered_map<Opcode, Handler> {
        {Opcode::OP_CMP_EQ, &CalcCmpEQ},
        {Opcode::OP_CMP_NE, &CalcCmpNE},
        {Opcode::OP_CMP_AND, &CalcCmpAnd},
        {Opcode::OP_CMP_OR, &CalcCmpOr},
        {Opcode::OP_CMP_LT, &CalcCmpLessThan},
        {Opcode::OP_CMP_GE, &CalcCmpGraterThanOrEqual},
        {Opcode::OP_CMP_LTU, &CalcCmpLessThanU},
        {Opcode::OP_CMP_GEU, &CalcCmpGraterThanOrEqualU},
        {Opcode::OP_CMP_EQI, &CalcCmpEQ},
        {Opcode::OP_CMP_NEI, &CalcCmpNE},
        {Opcode::OP_CMP_ANDI, &CalcCmpAnd},
        {Opcode::OP_CMP_ORI, &CalcCmpOr},
        {Opcode::OP_CMP_LTI, &CalcCmpLessThan},
        {Opcode::OP_CMP_GEI, &CalcCmpGraterThanOrEqual},
        {Opcode::OP_CMP_LTUI, &CalcCmpLessThanU},
        {Opcode::OP_CMP_GEUI, &CalcCmpGraterThanOrEqualU},
    };
    auto it = COMPARE_TABLE.find(op);
    return (it == COMPARE_TABLE.end()) ? 0 : it->second;
}

bool CalculateCompare(MInst &inst)
{
    Handler fn = Lookup(inst.opcode);
    return fn ? fn(inst) : false;
}

} // namespace Compare
} // namespace Calculate
} // namespace JCore