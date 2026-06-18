#include "Branch.h"

#include <algorithm>

namespace JCore {
namespace Calculate {
namespace Branch {

static bool CalcBranchEQ(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX) {
        return false;
    }
    if (inst.srcs[SRC0_IDX]->data == inst.srcs[SRC1_IDX]->data) {
        inst.brInfo->brTaken = true;
        inst.brInfo->targetPC = inst.pc + inst.srcs[SRC2_IDX]->data;
    } else {
        inst.brInfo->brTaken = false;
        inst.brInfo->targetPC = inst.GetFallPC();
    }
    return true;
}

static bool CalcBranchNE(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX) {
        return false;
    }
    if (inst.srcs[SRC0_IDX]->data != inst.srcs[SRC1_IDX]->data) {
        inst.brInfo->brTaken = true;
        inst.brInfo->targetPC = inst.pc + inst.srcs[SRC2_IDX]->data;
    } else {
        inst.brInfo->brTaken = false;
        inst.brInfo->targetPC = inst.GetFallPC();
    }
    return true;
}

static bool CalcBranchLessThan(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX) {
        return false;
    }
    if (static_cast<int64_t>(inst.srcs[SRC0_IDX]->data) < static_cast<int64_t>(inst.srcs[SRC1_IDX]->data)) {
        inst.brInfo->brTaken = true;
        inst.brInfo->targetPC = inst.pc + inst.srcs[SRC2_IDX]->data;
    } else {
        inst.brInfo->brTaken = false;
        inst.brInfo->targetPC = inst.GetFallPC();
    }
    return true;
}

static bool CalcBranchLessThanU(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX) {
        return false;
    }
    if (inst.srcs[SRC0_IDX]->data < inst.srcs[SRC1_IDX]->data) {
        inst.brInfo->brTaken = true;
        inst.brInfo->targetPC = inst.pc + inst.srcs[SRC2_IDX]->data;
    } else {
        inst.brInfo->brTaken = false;
        inst.brInfo->targetPC = inst.GetFallPC();
    }
    return true;
}

static bool CalcBranchGraterThanOrEqual(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX) {
        return false;
    }
    if (static_cast<int64_t>(inst.srcs[SRC0_IDX]->data) >= static_cast<int64_t>(inst.srcs[SRC1_IDX]->data)) {
        inst.brInfo->brTaken = true;
        inst.brInfo->targetPC = inst.pc + inst.srcs[SRC2_IDX]->data;
    } else {
        inst.brInfo->brTaken = false;
        inst.brInfo->targetPC = inst.GetFallPC();
    }
    return true;
}

static bool CalcBranchGraterThanOrEqualU(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX) {
        return false;
    }
    if (inst.srcs[SRC0_IDX]->data >= inst.srcs[SRC1_IDX]->data) {
        inst.brInfo->brTaken = true;
        inst.brInfo->targetPC = inst.pc + inst.srcs[SRC2_IDX]->data;
    } else {
        inst.brInfo->brTaken = false;
        inst.brInfo->targetPC = inst.GetFallPC();
    }
    return true;
}

static bool CalcBranchJ(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX) {
        return false;
    }
    inst.brInfo->brTaken = true;
    inst.brInfo->targetPC = inst.pc + inst.srcs[SRC0_IDX]->data;
    return true;
}

static bool CalcBranchJR(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    inst.brInfo->brTaken = true;
    inst.brInfo->targetPC = inst.srcs[SRC0_IDX]->data + inst.srcs[SRC1_IDX]->data;
    return true;
}

static bool CalcBranchBZ(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX || inst.brInfo->predMask.empty()) {
        return false;
    }
    bool allZero = std::none_of(inst.brInfo->predMask.begin(), inst.brInfo->predMask.end(), [](bool v){ return v; });
    if (allZero) {
        inst.brInfo->brTaken = true;
        inst.brInfo->targetPC = inst.pc + inst.srcs[SRC0_IDX]->data;
    } else {
        inst.brInfo->brTaken = false;
        inst.brInfo->targetPC = inst.GetFallPC();
    }
    return true;
}

static bool CalcBranchBNZ(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX || inst.brInfo->predMask.empty()) {
        return false;
    }
    bool allZero = std::none_of(inst.brInfo->predMask.begin(), inst.brInfo->predMask.end(), [](bool v){ return v; });
    if (!allZero) {
        inst.brInfo->brTaken = true;
        inst.brInfo->targetPC = inst.pc + inst.srcs[SRC0_IDX]->data;
    } else {
        inst.brInfo->brTaken = false;
        inst.brInfo->targetPC = inst.GetFallPC();
    }
    return true;
}

Handler Lookup(Opcode op)
{
    static const std::unordered_map<Opcode, Handler> BRANCH_TABLE = std::unordered_map<Opcode, Handler> {
        {Opcode::OP_B_EQ, &CalcBranchEQ},
        {Opcode::OP_B_NE, &CalcBranchNE},
        {Opcode::OP_B_LT, &CalcBranchLessThan},
        {Opcode::OP_B_GE, &CalcBranchGraterThanOrEqual},
        {Opcode::OP_B_LTU, &CalcBranchLessThanU},
        {Opcode::OP_B_GEU, &CalcBranchGraterThanOrEqualU},
        {Opcode::OP_JR, &CalcBranchJR},
        {Opcode::OP_J, &CalcBranchJ},
        {Opcode::OP_B_Z, &CalcBranchBZ},
        {Opcode::OP_B_NZ, &CalcBranchBNZ},
    };
    auto it = BRANCH_TABLE.find(op);
    return (it == BRANCH_TABLE.end()) ? 0 : it->second;
}

bool CalculateBranch(MInst &inst)
{
    Handler fn = Lookup(inst.opcode);
    return fn ? fn(inst) : false;
}

} // namespace Branch
} // namespace Calculate
} // namespace JCore