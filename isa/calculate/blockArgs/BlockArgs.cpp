#include "BlockArgs.h"

#include <cmath>

namespace JCore {
namespace Calculate {
namespace BlockArgs {

static bool CalcInstBIOT(MInst &inst)
{
    if (inst.srcs.size() < SRC1_IDX) {
        return false;
    }
    if (!inst.dsts.empty()) {
        if (inst.srcs[SRC0_IDX]->type == OperandType::OPD_UIMM) {
            uint64_t imm5 = inst.srcs[SRC0_IDX]->data;
            if (imm5 == 0) {
                inst.dsts[DST0_IDX]->size = 0;
            } else {
                inst.dsts[DST0_IDX]->size = (NUM16 * (1U << imm5));
            }
        } else {
            ASSERT(false && "Unsupport B.IOT input");
        }
    }
    return true;
}

Handler Lookup(Opcode op)
{
    static const std::unordered_map<Opcode, Handler> BLK_ARGS_TABLE = std::unordered_map<Opcode, Handler> {
        {Opcode::OP_B_IOT, &CalcInstBIOT},
    };
    auto it = BLK_ARGS_TABLE.find(op);
    return (it == BLK_ARGS_TABLE.end()) ? 0 : it->second;
}

bool CalculateBlockArgs(MInst &inst)
{
    Handler fn = Lookup(inst.opcode);
    return fn ? fn(inst) : true;
}

} // namespace BlockArgs
} // namespace Calculate
} // namespace JCore