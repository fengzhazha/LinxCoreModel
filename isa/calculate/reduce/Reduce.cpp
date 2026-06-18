#include "Reduce.h"

#include "calculate/FloatPointUtils.h"

namespace JCore {
namespace Calculate {
namespace Reduce {

static bool CalcInstRDADD(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data + inst.reduceInfo->data;
    inst.reduceInfo->data = inst.dsts[DST0_IDX]->data;
    return true;
}

static bool CalcInstRDAND(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX) {
        return false;
    }
    if (inst.reduceInfo->first) {
        inst.reduceInfo->first = false;
        inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data;
    } else {
        inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data & inst.reduceInfo->data;
    }
    inst.reduceInfo->data = inst.dsts[DST0_IDX]->data;
    return true;
}

static bool CalcInstRDOR(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX) {
        return false;
    }
    if (inst.reduceInfo->first) {
        inst.reduceInfo->first = false;
        inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data;
    } else {
        inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data | inst.reduceInfo->data;
    }
    inst.reduceInfo->data = inst.dsts[DST0_IDX]->data;
    return true;
}

static bool CalcInstRDXOR(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX) {
        return false;
    }
    if (inst.reduceInfo->first) {
        inst.reduceInfo->first = false;
        inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data;
    } else {
        inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data ^ inst.reduceInfo->data;
    }
    inst.reduceInfo->data = inst.dsts[DST0_IDX]->data;
    return true;
}

static bool CalcInstRDFADD(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX) {
        return false;
    }
    if (inst.reduceInfo->first) {
        inst.reduceInfo->first = false;
        inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data;
    } else {
        inst.dsts[DST0_IDX]->data = Fadd(inst.srcs[SRC0_IDX]->cvt, inst.srcs[SRC0_IDX]->data, inst.srcs[SRC1_IDX]->data,
                                         float_round_nearest_even);
    }
    inst.reduceInfo->data = inst.dsts[DST0_IDX]->data;
    return true;
}

static bool CalcInstRDMAX(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX) {
        return false;
    }
    if (inst.reduceInfo->first) {
        inst.reduceInfo->first = false;
        inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data;
    } else {
        switch (inst.srcs[SRC0_IDX]->cvt) {
            case OPConvertType::OPCVT_S8:
            case OPConvertType::OPCVT_S16:
            case OPConvertType::OPCVT_S32:
            case OPConvertType::OPCVT_S64: {
                int64_t a = static_cast<int64_t>(inst.srcs[SRC0_IDX]->data);
                int64_t b = static_cast<int64_t>(inst.reduceInfo->data);
                inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(std::max(a, b));
                break;
            }
            default: {
                uint64_t a = inst.srcs[SRC0_IDX]->data;
                uint64_t b = inst.reduceInfo->data;
                inst.dsts[DST0_IDX]->data = std::max(a, b);
            }
        }
        
    }
    inst.reduceInfo->data = inst.dsts[DST0_IDX]->data;
    return true;
}

static bool CalcInstRDMIN(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX) {
        return false;
    }
    if (inst.reduceInfo->first) {
        inst.reduceInfo->first = false;
        inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data;
    } else {
        switch (inst.srcs[SRC0_IDX]->cvt) {
            case OPConvertType::OPCVT_S8:
            case OPConvertType::OPCVT_S16:
            case OPConvertType::OPCVT_S32:
            case OPConvertType::OPCVT_S64: {
                int64_t a = static_cast<int64_t>(inst.srcs[SRC0_IDX]->data);
                int64_t b = static_cast<int64_t>(inst.reduceInfo->data);
                inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(std::min(a, b));
                break;
            }
            default: {
                uint64_t a = inst.srcs[SRC0_IDX]->data;
                uint64_t b = inst.reduceInfo->data;
                inst.dsts[DST0_IDX]->data = std::min(a, b);
            }
        }
    }
    inst.reduceInfo->data = inst.dsts[DST0_IDX]->data;
    return true;
}

static bool CalcInstRDFMAX(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX) {
        return false;
    }
    if (inst.reduceInfo->first) {
        inst.reduceInfo->first = false;
        inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data;
    } else {
        inst.dsts[DST0_IDX]->data = Fmax(inst.srcs[SRC0_IDX]->cvt, inst.srcs[SRC0_IDX]->data, inst.reduceInfo->data,
                                         float_round_nearest_even);;
    }
    inst.reduceInfo->data = inst.dsts[DST0_IDX]->data;
    return true;
}

static bool CalcInstRDFMIN(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX) {
        return false;
    }
    if (inst.reduceInfo->first) {
        inst.reduceInfo->first = false;
        inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data;
    } else {
        inst.dsts[DST0_IDX]->data = Fmin(inst.srcs[SRC0_IDX]->cvt, inst.srcs[SRC0_IDX]->data, inst.reduceInfo->data,
                                         float_round_nearest_even);;
    }
    inst.reduceInfo->data = inst.dsts[DST0_IDX]->data;
    return true;
}

Handler Lookup(Opcode op)
{
    static const std::unordered_map<Opcode, Handler> REDUCE_TABLE = std::unordered_map<Opcode, Handler> {
        {Opcode::OP_RDADD, &CalcInstRDADD},
        {Opcode::OP_RDAND, &CalcInstRDAND},
        {Opcode::OP_RDOR, &CalcInstRDOR},
        {Opcode::OP_RDXOR, &CalcInstRDXOR},
        {Opcode::OP_RDFADD, &CalcInstRDFADD},
        {Opcode::OP_RDMAX, &CalcInstRDMAX},
        {Opcode::OP_RDMIN, &CalcInstRDMIN},
        {Opcode::OP_RDFMAX, &CalcInstRDFMAX},
        {Opcode::OP_RDFMIN, &CalcInstRDFMIN},
    };
    auto it = REDUCE_TABLE.find(op);
    return (it == REDUCE_TABLE.end()) ? 0 : it->second;
}

bool CalculateReduce(MInst &inst)
{
    Handler fn = Lookup(inst.opcode);
    return fn ? fn(inst) : false;
}

} // namespace Reduce
} // namespace Calculate
} // namespace JCore