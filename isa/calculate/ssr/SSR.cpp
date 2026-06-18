#include "SSR.h"

namespace JCore {
namespace Calculate {
namespace SSR {

static bool CalcInstSetcTgt(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX) {
        return false;
    }
    inst.setcInfo->setcTarget = inst.srcs[SRC0_IDX]->data;
    return true;
}

static bool CalcInstSSRGet(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data;
    return true;
}

static bool CalcInstSSRSet(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }

    inst.ssrInfo->vld = true;
    inst.ssrInfo->ssrId = inst.srcs[SRC1_IDX]->value;
    inst.ssrInfo->data = inst.srcs[SRC0_IDX]->data;
    return true;
}

static bool CalcInstSSRSwap(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = inst.srcs[SRC1_IDX]->data;
    inst.ssrInfo->vld = true;
    inst.ssrInfo->ssrId = inst.srcs[SRC1_IDX]->value;
    inst.ssrInfo->data = inst.srcs[SRC0_IDX]->data;
    return true;
}

static bool CalcInstSSRLSRGet(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = inst.srcs[SRC1_IDX]->data;
    return true;
}

Handler Lookup(Opcode op)
{
    static const std::unordered_map<Opcode, Handler> SSR_TABLE = std::unordered_map<Opcode, Handler> {
        {Opcode::OP_SETC_TGT, &CalcInstSetcTgt},
        {Opcode::OP_SSRGET, &CalcInstSSRGet},
        {Opcode::OP_SSRSET, &CalcInstSSRSet},
        {Opcode::OP_SSRSWAP, &CalcInstSSRSwap},
        {Opcode::OP_LSRGET, &CalcInstSSRLSRGet},
    };
    auto it = SSR_TABLE.find(op);
    return (it == SSR_TABLE.end()) ? 0 : it->second;
}

bool CalculateSSR(MInst &inst)
{
    Handler fn = Lookup(inst.opcode);
    return fn ? fn(inst) : false;
}

} // namespace SSR
} // namespace Calculate
} // namespace JCore