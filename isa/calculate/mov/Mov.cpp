#include "Mov.h"

namespace JCore {
namespace Calculate {
namespace Mov {

Handler Lookup(Opcode op)
{
    ASSERT(op == Opcode::OP_MOV);
    return &CalculateMov;
}

bool CalculateMov(MInst &inst)
{
    inst.dsts[0]->data = inst.srcs[SRC0_IDX]->data;
    return true;
}

} // namespace Mov
} // namespace Calculate
} // namespace JCore
