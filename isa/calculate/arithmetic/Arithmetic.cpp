#include "Arithmetic.h"

namespace JCore {
namespace Calculate {
namespace Arithmetic {

static bool CalcThreddAdd(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data + inst.srcs[SRC1_IDX]->data + inst.srcs[SRC2_IDX]->data;
    return true;
}

static bool CalcInstAdd(MInst &inst)
{
    if (inst.accMemInfo && inst.accMemInfo->continuous) {
        return CalcThreddAdd(inst);
    }
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data + inst.srcs[SRC1_IDX]->data;
    return true;
}

static bool CalcInstAddw(MInst &inst)
{
    if (!CalcInstAdd(inst)) {
        return false;
    }
    int32_t low32 = static_cast<int32_t>(inst.dsts[DST0_IDX]->data & MASK_BIT32);
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(static_cast<int64_t>(low32));
    return true;
}

static bool CalcInstSub(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data - inst.srcs[SRC1_IDX]->data;
    return true;
}

static bool CalcInstSubw(MInst &inst)
{
    if (!CalcInstSub(inst)) {
        return false;
    }
    int32_t low32 = static_cast<int32_t>(inst.dsts[DST0_IDX]->data & MASK_BIT32);
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(static_cast<int64_t>(low32));
    return true;
}

static bool CalcInstAnd(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data & inst.srcs[SRC1_IDX]->data;
    return true;
}

static bool CalcInstAndi(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    auto getSignExt = [](uint64_t origin) {
        if (((origin >> 11) & 0x1) == 1) {
            uint64_t temp = -1;
            temp = temp >> 12 << 12;
            return origin | temp;
        }
        return origin;
    };
    inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data & getSignExt(inst.srcs[SRC1_IDX]->data);
    return true;
}

static bool CalcInstAndw(MInst &inst)
{
    if (!CalcInstAnd(inst)) {
        return false;
    }
    int32_t low32 = static_cast<int32_t>(inst.dsts[DST0_IDX]->data & MASK_BIT32);
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(static_cast<int64_t>(low32));
    return true;
}

static bool CalcInstOr(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data | inst.srcs[SRC1_IDX]->data;
    return true;
}

static bool CalcInstOrw(MInst &inst)
{
    if (!CalcInstOr(inst)) {
        return false;
    }
    int32_t low32 = static_cast<int32_t>(inst.dsts[DST0_IDX]->data & MASK_BIT32);
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(static_cast<int64_t>(low32));
    return true;
}

static bool CalcInstXor(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data ^ inst.srcs[SRC1_IDX]->data;
    return true;
}

static bool CalcInstXorw(MInst &inst)
{
    if (!CalcInstXor(inst)) {
        return false;
    }
    int32_t low32 = static_cast<int32_t>(inst.dsts[DST0_IDX]->data & MASK_BIT32);
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(static_cast<int64_t>(low32));
    return true;
}

static bool CalcInstSLL(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data << (inst.srcs[SRC1_IDX]->data & MASK_BIT6);
    return true;
}

static bool CalcInstSLLW(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    uint32_t srcL = static_cast<uint32_t>(inst.srcs[SRC0_IDX]->data & MASK_BIT32);
    uint32_t srcR = static_cast<uint32_t>(inst.srcs[SRC1_IDX]->data & MASK_BIT5);
    int32_t res = static_cast<int32_t>(srcL << srcR);
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(static_cast<int64_t>(res));
    return true;
}


uint64_t DoLogicShiftRight(uint64_t data, uint64_t shift, OperandWidth width)
{
    switch (width) {
        case OperandWidth::OPDW_W: {
            data &= MASK_BIT32;
        } break;
        case OperandWidth::OPDW_H: {
            data &= MASK_BIT16;
        } break;
        case OperandWidth::OPDW_B: {
            data &= MASK_BIT8;
        } break;
        default:
            break;
    }

    return data >> shift;
}

static bool CalcInstSRL(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    uint64_t shift = inst.srcs[SRC1_IDX]->data & MASK_BIT6;
    auto src0 = inst.srcs[SRC0_IDX];
    inst.dsts[DST0_IDX]->data = DoLogicShiftRight(src0->data, shift, src0->width);
    return true;
}

static bool CalcInstSRLW(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    uint32_t srcL = static_cast<uint32_t>(inst.srcs[SRC0_IDX]->data & MASK_BIT32);
    uint32_t srcR = static_cast<uint32_t>(inst.srcs[SRC1_IDX]->data & MASK_BIT5);
    uint32_t res = static_cast<uint32_t>(srcL >> srcR);
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(res)));
    return true;
}

template<typename T>
int64_t DoArithShiftRight(uint64_t data, uint64_t shift)
{
    T val = static_cast<T>(data);
    return static_cast<int64_t>(val >> shift);
}

static bool CalcInstSRA(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    int64_t res = static_cast<int64_t>(inst.srcs[SRC0_IDX]->data) >> (inst.srcs[SRC1_IDX]->data & MASK_BIT6);
    uint64_t srcL = inst.srcs[SRC0_IDX]->data;
    uint64_t srcR = inst.srcs[SRC1_IDX]->data & MASK_BIT6;
    switch (inst.srcs[SRC0_IDX]->cvt) {
        case OPConvertType::OPCVT_U32:
            res = srcL >> srcR;
            break;
        case OPConvertType::OPCVT_S32:
            res = DoArithShiftRight<int32_t>(srcL, srcR);
            break;
        case OPConvertType::OPCVT_S16:
            res = DoArithShiftRight<int16_t>(srcL, srcR);
            break;
        case OPConvertType::OPCVT_U16:
            res = srcL >> srcR;
            break;
        case OPConvertType::OPCVT_S8:
            res = DoArithShiftRight<int8_t>(srcL, srcR);
            break;
        case OPConvertType::OPCVT_U8:
            res = srcL >> srcR;
            break;
        default:
            res = static_cast<int64_t>(srcL) >> srcR;
            break;
    }
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(res);
    return true;
}

static bool CalcInstSRAW(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    int32_t srcL = static_cast<int32_t>(inst.srcs[SRC0_IDX]->data & MASK_BIT32);
    uint32_t srcR = static_cast<uint32_t>(inst.srcs[SRC1_IDX]->data & MASK_BIT5);
    int32_t res = static_cast<int32_t>(srcL >> srcR);
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(res);
    return true;
}

Handler Lookup(Opcode op)
{
    static const std::unordered_map<Opcode, Handler> ARITH_TABLE = std::unordered_map<Opcode, Handler> {
        /* 64 bits */
        /* SrcL:reg, SrcR:Reg */
        {Opcode::OP_ADD, &CalcInstAdd},
        {Opcode::OP_SUB, &CalcInstSub},
        {Opcode::OP_AND, &CalcInstAnd},
        {Opcode::OP_OR, &CalcInstOr},
        {Opcode::OP_XOR, &CalcInstXor},
        {Opcode::OP_SRL, &CalcInstSRL},
        {Opcode::OP_SRA, &CalcInstSRA},
        {Opcode::OP_SLL, &CalcInstSLL},
        /* SrcL:reg, SrcR:imm */
        {Opcode::OP_ADDI, &CalcInstAdd},
        {Opcode::OP_SUBI, &CalcInstSub},
        {Opcode::OP_ANDI, &CalcInstAndi},
        {Opcode::OP_ORI, &CalcInstOr},
        {Opcode::OP_XORI, &CalcInstXor},
        {Opcode::OP_SRLI, &CalcInstSRL},
        {Opcode::OP_SRAI, &CalcInstSRA},
        {Opcode::OP_SLLI, &CalcInstSLL},

        {Opcode::OP_ADDW, &CalcInstAddw},
        {Opcode::OP_SUBW, &CalcInstSubw},
        {Opcode::OP_ANDW, &CalcInstAndw},
        {Opcode::OP_ORW, &CalcInstOrw},
        {Opcode::OP_XORW, &CalcInstXorw},
        {Opcode::OP_SRLW, &CalcInstSRLW},
        {Opcode::OP_SRAW, &CalcInstSRAW},
        {Opcode::OP_SLLW, &CalcInstSLLW},

        {Opcode::OP_ADDIW, &CalcInstAddw},
        {Opcode::OP_SUBIW, &CalcInstSubw},
        {Opcode::OP_ANDIW, &CalcInstAndw},
        {Opcode::OP_ORIW, &CalcInstOrw},
        {Opcode::OP_XORIW, &CalcInstXorw},
        {Opcode::OP_SRLIW, &CalcInstSRLW},
        {Opcode::OP_SRAIW, &CalcInstSRAW},
        {Opcode::OP_SLLIW, &CalcInstSLLW},
    };
    auto it = ARITH_TABLE.find(op);
    return (it == ARITH_TABLE.end()) ? 0 : it->second;
}
bool CalculateArithmetic(MInst &inst)
{
    Handler fn = Lookup(inst.opcode);
    return fn ? fn(inst) : false;
}

} // namespace Arithmetic
} // namespace Calculate
} // namespace JCore
