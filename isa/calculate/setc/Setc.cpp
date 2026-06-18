#include "Setc.h"

namespace JCore {
namespace Calculate {
namespace Setc {

static bool CalcSetcEQ(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (inst.srcs[SRC0_IDX]->data == inst.srcs[SRC1_IDX]->data) {
        inst.setcInfo->setcTaken = true;
    } else {
        inst.setcInfo->setcTaken = false;
    }
    return true;
}

static bool CalcSetcNE(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (inst.srcs[SRC0_IDX]->data != inst.srcs[SRC1_IDX]->data) {
        inst.setcInfo->setcTaken = true;
    } else {
        inst.setcInfo->setcTaken = false;
    }
    return true;
}

static bool CalcSetcAnd(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if ((inst.srcs[SRC0_IDX]->data & inst.srcs[SRC1_IDX]->data) != 0) {
        inst.setcInfo->setcTaken = true;
    } else {
        inst.setcInfo->setcTaken = false;
    }
    return true;
}

static bool CalcSetcOr(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if ((inst.srcs[SRC0_IDX]->data | inst.srcs[SRC1_IDX]->data) != 0) {
        inst.setcInfo->setcTaken = true;
    } else {
        inst.setcInfo->setcTaken = false;
    }
    return true;
}

static bool CalcSetcLessThan(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (static_cast<int64_t>(inst.srcs[SRC0_IDX]->data) < static_cast<int64_t>(inst.srcs[SRC1_IDX]->data)) {
        inst.setcInfo->setcTaken = true;
    } else {
        inst.setcInfo->setcTaken = false;
    }
    return true;
}

static bool CalcSetcLessThanU(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (inst.srcs[SRC0_IDX]->data < inst.srcs[SRC1_IDX]->data) {
        inst.setcInfo->setcTaken = true;
    } else {
        inst.setcInfo->setcTaken = false;
    }
    return true;
}

static bool CalcSetcGraterThan(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (static_cast<int64_t>(inst.srcs[SRC0_IDX]->data) >= static_cast<int64_t>(inst.srcs[SRC1_IDX]->data)) {
        inst.setcInfo->setcTaken = true;
    } else {
        inst.setcInfo->setcTaken = false;
    }
    return true;
}

static bool CalcSetcGraterThanU(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (inst.srcs[SRC0_IDX]->data >= inst.srcs[SRC1_IDX]->data) {
        inst.setcInfo->setcTaken = true;
    } else {
        inst.setcInfo->setcTaken = false;
    }
    return true;
}

Handler Lookup(Opcode op)
{
    static const std::unordered_map<Opcode, Handler> SETC_TABLE = std::unordered_map<Opcode, Handler> {
        {Opcode::OP_SETC_EQ, &CalcSetcEQ},
        {Opcode::OP_SETC_NE, &CalcSetcNE},
        {Opcode::OP_SETC_AND, &CalcSetcAnd},
        {Opcode::OP_SETC_OR, &CalcSetcOr},
        {Opcode::OP_SETC_LT, &CalcSetcLessThan},
        {Opcode::OP_SETC_GE, &CalcSetcGraterThan},
        {Opcode::OP_SETC_LTU, &CalcSetcLessThanU},
        {Opcode::OP_SETC_GEU, &CalcSetcGraterThanU},
        {Opcode::OP_SETC_EQI, &CalcSetcEQ},
        {Opcode::OP_SETC_NEI, &CalcSetcNE},
        {Opcode::OP_SETC_ANDI, &CalcSetcAnd},
        {Opcode::OP_SETC_ORI, &CalcSetcOr},
        {Opcode::OP_SETC_LTI, &CalcSetcLessThan},
        {Opcode::OP_SETC_GEI, &CalcSetcGraterThan},
        {Opcode::OP_SETC_LTUI, &CalcSetcLessThanU},
        {Opcode::OP_SETC_GEUI, &CalcSetcGraterThanU},
    };
    auto it = SETC_TABLE.find(op);
    return (it == SETC_TABLE.end()) ? 0 : it->second;
}

bool CalculateSetc(MInst &inst)
{
    Handler fn = Lookup(inst.opcode);
    return fn ? fn(inst) : false;
}

} // namespace Setc
} // namespace Calculate
} // namespace JCore