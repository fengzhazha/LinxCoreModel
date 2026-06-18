#include "ArithmeticFP.h"

#include <cmath>
#include "calculate/FloatPointUtils.h"

namespace JCore {
namespace Calculate {
namespace ArithmeticFP {

static double EleRealSize(OPConvertType type)
{
    switch (type) {
        case OPConvertType::OPCVT_FP64:
        case OPConvertType::OPCVT_S64:
        case OPConvertType::OPCVT_U64:
            return 8;
        case OPConvertType::OPCVT_FP32:
        case OPConvertType::OPCVT_TF32:
        case OPConvertType::OPCVT_HF32:
        case OPConvertType::OPCVT_S32:
        case OPConvertType::OPCVT_U32:
            return 4;

        case OPConvertType::OPCVT_FP16:
        case OPConvertType::OPCVT_BF16:
        case OPConvertType::OPCVT_S16:
        case OPConvertType::OPCVT_U16:
        case OPConvertType::OPCVT_FP16_X_2:
        case OPConvertType::OPCVT_U16_X_2:
        case OPConvertType::OPCVT_S16_X_2:
            return 2;

        case OPConvertType::OPCVT_HIF8:
        case OPConvertType::OPCVT_FP8:
        case OPConvertType::OPCVT_FP8_1:
        case OPConvertType::OPCVT_SF8:
        case OPConvertType::OPCVT_S8:
        case OPConvertType::OPCVT_U8:
        case OPConvertType::OPCVT_E4M3_X_4:
        case OPConvertType::OPCVT_U8_X_4:
        case OPConvertType::OPCVT_S8_X_4:
            return 1;
        case OPConvertType::OPCVT_FP4:
        case OPConvertType::OPCVT_FP4_1:
        case OPConvertType::OPCVT_HIF4:
        case OPConvertType::OPCVT_S4:
        case OPConvertType::OPCVT_U4:
            return 1;
        default:
            assert(false);
    }
    return 0;
}

static FloatRoundMode ConvertRoundMode(FRMMode frm)
{
    switch (frm) {
        case FRMMode::FRM_RNE: return float_round_nearest_even;
        case FRMMode::FRM_RTZ: return float_round_to_zero;
        case FRMMode::FRM_RDN: return float_round_down;
        case FRMMode::FRM_RUP: return float_round_up;
        case FRMMode::FRM_RNA: return float_round_ties_away;
        case FRMMode::FRM_RTO: return float_round_to_odd;
        case FRMMode::FRM_RHB: return float_round_nearest_even;
        default:
            assert(0);
    }
    assert(0);
    return float_round_nearest_even;
}

static bool CalcInstF(MInst &inst,
    const std::function<uint64_t(OPConvertType, uint64_t, uint64_t, FloatRoundMode)> &func)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    int vlen = inst.vlen;
    uint64_t res = 0;
    OPConvertType typ = inst.srcs[SRC0_IDX]->cvt;
    uint32_t elmSz = static_cast<uint32_t>(EleRealSize(typ));
    FloatRoundMode frm = ConvertRoundMode(inst.frm);
    uint64_t mask = (1ULL << (elmSz * 8)) - 1;
    if (elmSz == 8) {
        mask = static_cast<uint64_t>(-1LL);
    }
    uint64_t dat0 = inst.srcs[SRC0_IDX]->data;
    uint64_t dat1 = inst.srcs[SRC1_IDX]->data;

    for (int i = 0; i < vlen; ++i) {
        uint64_t in0 =  (dat0 >> (i * elmSz * 8)) & mask;
        uint64_t in1 =  (dat1 >> (i * elmSz * 8)) & mask;
        res |=  func(typ, in0, in1, frm) << (i * elmSz * 8);
    }
    inst.dsts[DST0_IDX]->data = res;
    return true;
}

static bool CalcInstFAdd(MInst &inst)
{
    return CalcInstF(inst, Fadd);
}

static bool CalcInstFSub(MInst &inst)
{
    return CalcInstF(inst, Fsub);
}

static bool CalcInstFMul(MInst &inst)
{
    return CalcInstF(inst, Fmul);
}

static bool CalcInstFDiv(MInst &inst)
{
    return CalcInstF(inst, Fdiv);
}

static bool CalcInstFM(MInst &inst,
    const std::function<uint64_t(OPConvertType, uint64_t, uint64_t, uint64_t, FloatRoundMode)> &func)
{
    if (inst.srcs.size() != SRC3_IDX) {
        return false;
    }
    int vlen = inst.vlen;
    uint64_t res = 0;
    OPConvertType typ = inst.srcs[SRC0_IDX]->cvt;
    uint32_t elmSz = static_cast<uint32_t>(EleRealSize(typ));
    FloatRoundMode frm = ConvertRoundMode(inst.frm);
    uint64_t mask = (1ULL << (elmSz * 8)) - 1;
    if (elmSz == 8) {
        mask = static_cast<uint64_t>(-1LL);
    }
    uint64_t dat0 = inst.srcs[SRC0_IDX]->data;
    uint64_t dat1 = inst.srcs[SRC1_IDX]->data;
    uint64_t dat2 = inst.srcs[SRC2_IDX]->data;

    for (int i = 0; i < vlen; ++i) {
        uint64_t in0 =  (dat0 >> (i * elmSz * 8)) & mask;
        uint64_t in1 =  (dat1 >> (i * elmSz * 8)) & mask;
        uint64_t in2 =  (dat2 >> (i * elmSz * 8)) & mask;
        res |=  func(typ, in0, in1, in2, frm) << (i * elmSz * 8);
    }
    inst.dsts[DST0_IDX]->data = res;
    return true;
}

static bool CalcInstFMAdd(MInst &inst)
{
    return CalcInstFM(inst, Fmadd);
}

static bool CalcInstFMSub(MInst &inst)
{
    return CalcInstFM(inst, Fmsub);
}

static bool CalcInstFNMAdd(MInst &inst)
{
    return CalcInstFM(inst, Fnmadd);
}

static bool CalcInstFNMSub(MInst &inst)
{
    return CalcInstFM(inst, Fnmsub);
}

static bool CalcInstFAbs(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = Fabs(inst.srcs[SRC0_IDX]->cvt, inst.srcs[SRC0_IDX]->data);
    return true;
}

static bool CalcInstFSqrt(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = Fsqrt(inst.srcs[SRC0_IDX]->cvt, inst.srcs[SRC0_IDX]->data);
    return true;
}

static bool CalcInstFRecip(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = Frecip(inst.srcs[SRC0_IDX]->cvt, inst.srcs[SRC0_IDX]->data);
    return true;
}

static bool CalcInstFExp(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = Fexp(inst.srcs[SRC0_IDX]->cvt, inst.srcs[SRC0_IDX]->data, float_round_nearest_even);
    return true;
}

static bool CalcInstFClass(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = Fclass(inst.srcs[SRC0_IDX]->cvt, inst.srcs[SRC0_IDX]->data);
    return true;
}

Handler Lookup(Opcode op)
{
    static const std::unordered_map<Opcode, Handler> ARITHFP_TABLE = std::unordered_map<Opcode, Handler> {
        {Opcode::OP_FADD, &CalcInstFAdd},
        {Opcode::OP_FSUB, &CalcInstFSub},
        {Opcode::OP_FMUL, &CalcInstFMul},
        {Opcode::OP_FDIV, &CalcInstFDiv},
        {Opcode::OP_FMADD, &CalcInstFMAdd},
        {Opcode::OP_FMSUB, &CalcInstFMSub},
        {Opcode::OP_FNMADD, &CalcInstFNMAdd},
        {Opcode::OP_FNMSUB, &CalcInstFNMSub},
        {Opcode::OP_FABS, &CalcInstFAbs},
        {Opcode::OP_FSQRT, &CalcInstFSqrt},
        {Opcode::OP_FRECIP, &CalcInstFRecip},
        {Opcode::OP_FEXP, &CalcInstFExp},
        {Opcode::OP_FCLASS, &CalcInstFClass},
    };
    auto it = ARITHFP_TABLE.find(op);
    return (it == ARITHFP_TABLE.end()) ? 0 : it->second;
}

bool CalculateArithmeticFP(MInst &inst)
{
    Handler fn = Lookup(inst.opcode);
    return fn ? fn(inst) : false;
}

} // namespace ArithmeticFP
} // namespace Calculate
} // namespace JCore