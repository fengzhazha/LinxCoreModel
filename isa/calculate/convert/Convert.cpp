#include "Convert.h"

#include "calculate/FloatPointUtils.h"
#include "softfloat.h"

namespace JCore {
namespace Calculate {
/* Only basic type judgment is provided, and x2 type is not provided. */
uint32_t GetTypeWidth(OPConvertType type)
{
    switch (type) {
        case OPConvertType::OPCVT_FP64:
        case OPConvertType::OPCVT_S64:
        case OPConvertType::OPCVT_U64:
            return 64;
        case OPConvertType::OPCVT_FP32:
        case OPConvertType::OPCVT_TF32:
        case OPConvertType::OPCVT_HF32:
        case OPConvertType::OPCVT_S32:
        case OPConvertType::OPCVT_U32:
            return 32;

        case OPConvertType::OPCVT_FP16:
        case OPConvertType::OPCVT_BF16:
        case OPConvertType::OPCVT_S16:
        case OPConvertType::OPCVT_U16:
            return 16;

        case OPConvertType::OPCVT_HIF8:
        case OPConvertType::OPCVT_FP8:
        case OPConvertType::OPCVT_FP8_1:
        case OPConvertType::OPCVT_SF8:
        case OPConvertType::OPCVT_S8:
        case OPConvertType::OPCVT_U8:
            return 8;
        case OPConvertType::OPCVT_FP4:
        case OPConvertType::OPCVT_FP4_1:
        case OPConvertType::OPCVT_HIF4:
        case OPConvertType::OPCVT_S4:
        case OPConvertType::OPCVT_U4:
            return 4;
        default:
            assert(false);
    }
    return 0;
}
namespace Convert {

static bool CalcInstConvert(MInst &inst)
{
    uint64_t data = 0;
    OPConvertType dtyp, styp;
    uint32_t dpack = IsPacketOperandCVT(inst.dsts[0]->cvt);
    uint32_t spack = IsPacketOperandCVT(inst.srcs[SRC0_IDX]->cvt);
    styp = inst.srcs[SRC0_IDX]->cvt;
    dtyp = inst.dsts[DST0_IDX]->cvt;
    if (dtyp == OPConvertType::OPCVT_BF16_X_2) {
        dtyp = OPConvertType::OPCVT_BF16;
    } else if (dtyp == OPConvertType::OPCVT_FP16_X_2) {
        dtyp = OPConvertType::OPCVT_BF16;
    } else if (dtyp == OPConvertType::OPCVT_E1M2_X_2) {
        dtyp = OPConvertType::OPCVT_FP4_1; // e1m2
    } else if (dtyp == OPConvertType::OPCVT_HIF4_X_2) {
        dtyp = OPConvertType::OPCVT_HIF4;
    }
    if (styp == OPConvertType::OPCVT_BF16_X_2) {
        styp = OPConvertType::OPCVT_BF16;
    } else if (styp == OPConvertType::OPCVT_FP16_X_2) {
        styp = OPConvertType::OPCVT_FP16;
    } else if (styp == OPConvertType::OPCVT_HIF4_X_2) {
        styp = OPConvertType::OPCVT_HIF4;
    }

    uint32_t s_size = Calculate::GetTypeWidth(styp);
    uint32_t d_size = Calculate::GetTypeWidth(dtyp);
    uint64_t smask = s_size == 64 ? (uint64_t)(-1) : ((1 << s_size) - 1);

    if (spack==1) {
        if (dpack == 2) {
            data = Calculate::ConvertAggre(inst.srcs[SRC0_IDX]->data, styp, dtyp, static_cast<uint32_t>(inst.frm));
            data = data << d_size;
            data |= Calculate::ConvertAggre(inst.srcs[SRC1_IDX]->data, styp, dtyp, static_cast<uint32_t>(inst.frm));
        } else if (dpack == 1) {
            data = Calculate::ConvertAggre(inst.srcs[SRC0_IDX]->data,
                        styp, dtyp, static_cast<uint32_t>(inst.frm));
        } else {
            assert(false);
        }
    } else if (spack == 2) {
        if (dpack == 2) {
            uint64_t src = (inst.srcs[SRC0_IDX]->data >> s_size) & smask;
            data = Calculate::ConvertAggre(src, styp, dtyp, static_cast<uint32_t>(inst.frm));
            data = data << d_size;
            src = inst.srcs[SRC0_IDX]->data & smask;
            data |= Calculate::ConvertAggre(src, styp, dtyp, static_cast<uint32_t>(inst.frm));
        } else {
            assert(false);
        }
    }

    inst.dsts[DST0_IDX]->data = data;
    return true;
}

Handler Lookup(Opcode op)
{
    static const std::unordered_map<Opcode, Handler> CONVERT_TABLE = std::unordered_map<Opcode, Handler> {
        {Opcode::OP_FCVT, &CalcInstConvert},
        {Opcode::OP_FCVTA, &CalcInstConvert},
        {Opcode::OP_FCVTM, &CalcInstConvert},
        {Opcode::OP_FCVTN, &CalcInstConvert},
        {Opcode::OP_FCVTP, &CalcInstConvert},
        {Opcode::OP_FCVTZ, &CalcInstConvert},
        {Opcode::OP_SCVTF, &CalcInstConvert},
        {Opcode::OP_UCVTF, &CalcInstConvert},
        {Opcode::OP_FCVTI, &CalcInstConvert},
        {Opcode::OP_ICVT, &CalcInstConvert},
        {Opcode::OP_ICVTF, &CalcInstConvert},
    };
    auto it = CONVERT_TABLE.find(op);
    return (it == CONVERT_TABLE.end()) ? 0 : it->second;
}

bool CalculateConvert(MInst &inst)
{
    Handler fn = Lookup(inst.opcode);
    return fn ? fn(inst) : false;
}

} // namespace Convert
} // namespace Calculate
} // namespace JCore