#include "CacheMaintain.h"

namespace JCore {
namespace Calculate {
namespace CacheMaintain {

static bool CalcAddr(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX || inst.dsts.size() != DST0_IDX) {
        return false;
    }
    inst.accMemInfo->accMemAddr = inst.srcs[SRC0_IDX]->data;
    return true;
}

static bool CalcAll(MInst &inst)
{
    if (inst.srcs.size() != SRC0_IDX || inst.dsts.size() != DST0_IDX) {
        return false;
    }
    return true;
}

Handler Lookup(Opcode op)
{
    static const std::unordered_map<Opcode, Handler> CACHE_MAINTAIN_TABLE = std::unordered_map<Opcode, Handler> {
        {Opcode::OP_BC_IVA, &CalcAddr},
        {Opcode::OP_BC_IALL, &CalcAll},
        {Opcode::OP_IC_IVA, &CalcAddr},
        {Opcode::OP_IC_IALL, &CalcAll},
        {Opcode::OP_DC_IVA, &CalcAddr},
        {Opcode::OP_DC_IALL, &CalcAll},
        {Opcode::OP_DC_CVA, &CalcAddr},
        {Opcode::OP_DC_CIVA, &CalcAddr},
        {Opcode::OP_DC_ISW, &CalcAddr},
        {Opcode::OP_DC_CSW, &CalcAddr},
        {Opcode::OP_DC_CISW, &CalcAddr},
        {Opcode::OP_DC_ZVA, &CalcAddr},
        {Opcode::OP_TLB_IA, &CalcAddr},
        {Opcode::OP_TLB_IV, &CalcAddr},
        {Opcode::OP_TLB_IAV, &CalcAddr},
        {Opcode::OP_TLB_IALL, &CalcAll},
    };
    auto it = CACHE_MAINTAIN_TABLE.find(op);
    return (it == CACHE_MAINTAIN_TABLE.end()) ? 0 : it->second;
}

bool CalculateCacheMaintain(MInst &inst)
{
    Handler fn = Lookup(inst.opcode);
    return fn ? fn(inst) : false;
}

} // namespace CacheMaintain
} // namespace Calculate
} // namespace JCore