#pragma once

#ifndef OPERAND_TYPE
#define OPERAND_TYPE

#include <string>
#include <array>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace JCore {

constexpr uint32_t SGPR_HAND_COUNT = 2;

enum class OperandType {
    OPD_INVALID = 0,
    OPD_ZERO,
    OPD_GREG,
    OPD_TLINK, // 标量指令的标量T 寄存器
    OPD_ULINK, // 标量指令的标量U 寄存器

    OPD_VTLINK, // 向量指令的向量T 寄存器
    OPD_VULINK, // 向量指令的向量U 寄存器
    OPD_VMLINK, // 向量指令的向量M 寄存器
    OPD_VNLINK, // 向量指令的向量N 寄存器

    OPD_TILE_TLINK, // B.IOT/B.IOTI T Tile寄存器
    OPD_TILE_ULINK, // B.IOT/B.IOTI U Tile寄存器
    OPD_TILE_MLINK, // B.IOT/B.IOTI M Tile寄存器
    OPD_TILE_NLINK, // B.IOT/B.IOTI N Tile寄存器
    OPD_TILE_ACC,   // B.IOT/B.IOTI ACC Tile寄存器
    OPD_TILE_STACK, // B.IOT/B.IOTI Stack Tile寄存器
    OPD_TILE_DLINK,   // B.IOD D 寄存器
    OPD_TILE_MCALL_GPR,
    OPD_TILE_MCALL_STK,

    OPD_TA_REG,     // 向量指令形参访问Tile 寄存器
    OPD_TB_REG,     // 向量指令形参访问Tile 寄存器
    OPD_TC_REG,     // 向量指令形参访问Tile 寄存器
    OPD_TD_REG,     // 向量指令形参访问Tile 寄存器
    OPD_TE_REG,     // 向量指令形参访问Tile 寄存器
    OPD_TF_REG,     // 向量指令形参访问Tile 寄存器
    OPD_TG_REG,     // 向量指令形参访问Tile 寄存器
    OPD_TH_REG,     // 向量指令形参访问Tile 寄存器
    OPD_TO_REG,     // 向量指令形参访问Tile 寄存器
    OPD_TO1_REG,    // 向量指令形参访问Tile 寄存器
    OPD_TO2_REG,    // 向量指令形参访问Tile 寄存器
    OPD_TO3_REG,    // 向量指令形参访问Tile 寄存器
    OPD_TO_GPR_REG, // 向量指令形参访问Mcall GPR Tile 寄存器

    OPD_RI,         // 向量指令形参访问标量GPR寄存器
    OPD_RO,         // 向量指令形参访问标量GPR寄存器

    OPD_PREDMASK,   // 向量指令访问Lane 的掩码寄存器

    OPD_SIMM,       // 指令src 通过有符号立即数传入
    OPD_UIMM,       // 指令src 通过无符号立即数传入
    OPD_CARG,
    OPD_SYS,
    OPD_LB0,
    OPD_LB1,
    OPD_LB2,
    OPD_LC0,
    OPD_LC1,
    OPD_LC2,

    // Excution type
    STACK_POINTER,
    LS_MDB_DEPENDENCY,

    OPD_COUNT,
};

inline const std::array<std::string, static_cast<std::size_t>(OperandType::OPD_COUNT)>& OperandTypeNames()
{
    static const std::array<std::string, static_cast<std::size_t>(OperandType::OPD_COUNT)> OPD_NAMES = {{
        "OPD_INVALID",  // OPD_INVALID
        "zero",         // OPD_ZERO
        "",          // OPD_GREG
        "t",            // OPD_TLINK
        "u",            // OPD_ULINK
        "vt",           // OPD_VTLINK
        "vu",           // OPD_VULINK
        "vm",           // OPD_VMLINK
        "vn",           // OPD_VNLINK
        "T",            // OPD_TILE_TLINK
        "U",            // OPD_TILE_ULINK
        "M",            // OPD_TILE_MLINK
        "N",            // OPD_TILE_NLINK
        "ACC",          // OPD_TILE_ACC
        "TILE_STACK",   // OPD_TILE_STACK
        "D",            // OPD_TILE_DLINK
        "MCALL_GPR",    // OPD_TILE_MCALL_GPR,
        "MCALL_STACK",  // OPD_TILE_MCALL_STK,
        "TA",           // OPD_TA_REG
        "TB",           // OPD_TB_REG
        "TC",           // OPD_TC_REG
        "TD",           // OPD_TD_REG
        "TE",           // OPD_TE_REG
        "TF",           // OPD_TF_REG
        "TG",           // OPD_TG_REG
        "TH",           // OPD_TH_REG
        "TO",           // OPD_TO_REG
        "TO1",          // OPD_TO1_REG
        "TO2",          // OPD_TO2_REG
        "TO3",          // OPD_TO3_REG
        "TOGPR",        // OPD_TO_GPR_REG
        "ri",           // OPD_RI
        "ro",           // OPD_RO
        "pred",         // OPD_PREDMASK
        "",             // OPD_SIMM
        "",             // OPD_UIMM
        "BARG",         // OPD_CARG
        "",             // OPD_SYS
        "LB0",          // OPD_LB0
        "LB1",          // OPD_LB1
        "LB2",          // OPD_LB2
        "LC0",          // OPD_LC0
        "LC1",          // OPD_LC1
        "LC2",          // OPD_LC2
    }};
    return OPD_NAMES;
}

inline uint32_t SGPRType2Idx(OperandType type)
{
    static std::unordered_map<OperandType, uint32_t> SGPR_IDX_MAP = {
        {OperandType::OPD_TLINK, 0},
        {OperandType::OPD_ULINK, 1},
    };
    return SGPR_IDX_MAP.at(type);
}

inline const std::string& GetOperandType(OperandType type)
{
    const std::size_t idx = static_cast<std::size_t>(type);
    const auto& names = OperandTypeNames();
    static const std::string INVALID_OPD_TYPE = "invalid_operandtype";
    if (idx >= names.size()) {
        return INVALID_OPD_TYPE;
    }
    return names[idx];
}

inline bool OperandTypeIsImm(OperandType type)
{
    switch (type) {
        case OperandType::OPD_SIMM:
        case OperandType::OPD_UIMM:
            return true;
        default:
            return false;
    }
}

inline bool OperandTypeIsSGPR(OperandType type)
{
    switch (type) {
        case OperandType::OPD_TLINK:
        case OperandType::OPD_ULINK:
            return true;
        default:
            return false;
    }
}

inline bool OperandTypeIsVReg(OperandType type)
{
    switch (type) {
        case OperandType::OPD_VTLINK:
        case OperandType::OPD_VULINK:
        case OperandType::OPD_VMLINK:
        case OperandType::OPD_VNLINK:
            return true;
        default:
            return false;
    }
}

inline bool OperandTypeNeedRename(OperandType type)
{
    switch (type) {
        case OperandType::OPD_GREG:
        case OperandType::OPD_TLINK:
        case OperandType::OPD_ULINK:
        case OperandType::OPD_VTLINK:
        case OperandType::OPD_VULINK:
        case OperandType::OPD_VMLINK:
        case OperandType::OPD_VNLINK:
        case OperandType::OPD_TA_REG:
        case OperandType::OPD_TB_REG:
        case OperandType::OPD_TC_REG:
        case OperandType::OPD_TD_REG:
        case OperandType::OPD_TE_REG:
        case OperandType::OPD_TF_REG:
        case OperandType::OPD_TG_REG:
        case OperandType::OPD_TH_REG:
        case OperandType::OPD_TO_REG:
        case OperandType::OPD_TO1_REG:
        case OperandType::OPD_TO2_REG:
        case OperandType::OPD_TO3_REG:
        case OperandType::OPD_TO_GPR_REG:
        case OperandType::OPD_RI:
        case OperandType::OPD_RO:
        case OperandType::OPD_PREDMASK:
            return true;
        default:
            return false;
    }
}

inline bool OperandTypeIsAlwaysNotReady(OperandType type)
{
    switch (type) {
        case OperandType::LS_MDB_DEPENDENCY:
            return true;
        default:
            break;
    }
    return false;
}

inline bool OperandTypeIsLocalReg(OperandType type)
{
    const std::unordered_set<OperandType> LOCAL_OPD_TYPES {
        OperandType::OPD_TLINK, OperandType::OPD_ULINK, OperandType::OPD_VTLINK,
        OperandType::OPD_VULINK, OperandType::OPD_VMLINK, OperandType::OPD_VNLINK,
        OperandType::OPD_PREDMASK,
    };
    return LOCAL_OPD_TYPES.count(type) != 0;
}

inline bool OperandTypeDefaultReady(OperandType type)
{
    const std::unordered_set<OperandType> DEFAULT_READY_OPD_TYPES {
        OperandType::OPD_ZERO,
        OperandType::OPD_TA_REG, OperandType::OPD_TB_REG, OperandType::OPD_TC_REG, OperandType::OPD_TD_REG,
        OperandType::OPD_TE_REG, OperandType::OPD_TF_REG, OperandType::OPD_TG_REG, OperandType::OPD_TH_REG,
        OperandType::OPD_TO_REG, OperandType::OPD_TO1_REG, OperandType::OPD_TO2_REG, OperandType::OPD_TO3_REG,
        OperandType::OPD_TO_GPR_REG,
        // RI 对应的GPR 在B.IOR 时已经确保ready，对应的Local Reg 应该在收到BlockCmd 时已经ready
        OperandType::OPD_RI, OperandType::OPD_RO,
        OperandType::OPD_SIMM, OperandType::OPD_UIMM, OperandType::OPD_CARG, OperandType::OPD_SYS,
        OperandType::OPD_LB0, OperandType::OPD_LB1, OperandType::OPD_LB2,
        OperandType::OPD_LC0, OperandType::OPD_LC1, OperandType::OPD_LC2,
    };
    return DEFAULT_READY_OPD_TYPES.count(type) != 0;
}

inline bool OperandTypeIsTile(OperandType type)
{
    const std::unordered_set<OperandType> TILE_OPD_TYPES {
        OperandType::OPD_TILE_TLINK, OperandType::OPD_TILE_ULINK, OperandType::OPD_TILE_MLINK,
        OperandType::OPD_TILE_NLINK, OperandType::OPD_TILE_ACC, OperandType::OPD_TILE_STACK,
        OperandType::OPD_TILE_DLINK, OperandType::OPD_TILE_MCALL_GPR, OperandType::OPD_TILE_MCALL_STK,
    };
    return TILE_OPD_TYPES.count(type) != 0;
}

inline bool OperandTypeIsMcallTile(OperandType type)
{
    static const std::unordered_set<OperandType> TILE_OPD_TYPES {
        OperandType::OPD_TILE_MCALL_GPR, OperandType::OPD_TILE_MCALL_STK,
    };
    return TILE_OPD_TYPES.count(type) != 0;
}

inline bool OperandTypeIsZero(OperandType type)
{
    return type == OperandType::OPD_ZERO;
}

inline bool OperandTypeIsLGPR(OperandType type)
{
    const std::unordered_set<OperandType> LGPR_OPD_TYPES {
        OperandType::OPD_RI, OperandType::OPD_RO,
    };
    return LGPR_OPD_TYPES.count(type) != 0;
}

inline bool OperandTypeIsLTAR(OperandType type)
{
    const std::unordered_set<OperandType> LTAR_OPD_TYPES {
        OperandType::OPD_TA_REG, OperandType::OPD_TB_REG, OperandType::OPD_TC_REG, OperandType::OPD_TD_REG,
        OperandType::OPD_TE_REG, OperandType::OPD_TF_REG, OperandType::OPD_TG_REG, OperandType::OPD_TH_REG,
        OperandType::OPD_TO_REG, OperandType::OPD_TO1_REG, OperandType::OPD_TO2_REG, OperandType::OPD_TO3_REG,
    };
    return LTAR_OPD_TYPES.count(type) != 0;
}

inline bool OperandTypeIsLaneBoundReg(OperandType type)
{
    const std::unordered_set<OperandType> LB_OPD_TYPES {
        OperandType::OPD_LB0, OperandType::OPD_LB1, OperandType::OPD_LB2,
    };
    return LB_OPD_TYPES.count(type) != 0;
}

inline bool OperandTypeIsLaneCountReg(OperandType type)
{
    const std::unordered_set<OperandType> LC_OPD_TYPES {
        OperandType::OPD_LC0, OperandType::OPD_LC1, OperandType::OPD_LC2,
    };
    return LC_OPD_TYPES.count(type) != 0;
}

inline bool OperandTypeIsLoopReg(OperandType type)
{
    return OperandTypeIsLaneBoundReg(type) || OperandTypeIsLaneCountReg(type);
}

inline bool OperandTypeNeedVecData(OperandType type)
{
    return OperandTypeIsVReg(type) || OperandTypeIsLoopReg(type);
}

enum class OPConvertType {
    /* OPCVT_S64 need be 0 for 32inst decode */
    OPCVT_DW = -2,  /* as d */
    OPCVT_NOT,
    OPCVT_NEG,
    OPCVT_FP64,      // e11m52
    OPCVT_FP32,      // e8m23
    OPCVT_TF32,      // e8m10
    OPCVT_HF32,      // e8m11
    OPCVT_FP16,      // e5m10
    OPCVT_BF16,      // e8m7
    OPCVT_HIF8,      // Generic custom structure
    OPCVT_FP8,       // e4m3
    OPCVT_FP8_1,     // e5m2
    OPCVT_FP6,       // e3m2
    OPCVT_FP6_1,     // e2m3
    OPCVT_FP4,       // e2m1
    OPCVT_FP4_1,     // e1m2
    OPCVT_SF8,       // e8m0
    OPCVT_HIF4,      // Generic custom structure e1m2
    OPCVT_E6M2,
    OPCVT_S4,
    OPCVT_S8,
    OPCVT_S16,
    OPCVT_S32,
    OPCVT_S64,
    OPCVT_U4,
    OPCVT_U8,
    OPCVT_U16,
    OPCVT_U32,
    OPCVT_U64,
    OPCVT_HIF4_X_2,
    OPCVT_FP16_X_2,
    OPCVT_BF16_X_2,
    OPCVT_E4M3_X_4,
    OPCVT_U16_X_2,
    OPCVT_U8_X_4,
    OPCVT_S16_X_2,
    OPCVT_S8_X_4,
    OPCVT_E1M2_X_2,
    OPCVT_NO_CVT,
    OPCVT_COUNT,
};

inline const std::unordered_map<OPConvertType, std::string>& OPCVTTypeNames()
{
    static const std::unordered_map<OPConvertType, std::string> OPCVT_NAMES = {{
        {OPConvertType::OPCVT_DW, ".d",},
        {OPConvertType::OPCVT_NOT, ".not"},
        {OPConvertType::OPCVT_NEG, ".neg"},
        {OPConvertType::OPCVT_FP64, ".fd"},
        {OPConvertType::OPCVT_FP32, ".fs"},
        {OPConvertType::OPCVT_TF32, ".tfs"},
        {OPConvertType::OPCVT_HF32, ".hfs"},
        {OPConvertType::OPCVT_FP16, ".fh"},
        {OPConvertType::OPCVT_BF16, ".bf"},
        {OPConvertType::OPCVT_HIF8, ".hifb"},
        {OPConvertType::OPCVT_FP8, ".fb"},
        {OPConvertType::OPCVT_FP8_1, ".flb"},
        {OPConvertType::OPCVT_FP6, ".f6(e3m2)"},
        {OPConvertType::OPCVT_FP6_1, ".f6(e2m3)"},
        {OPConvertType::OPCVT_FP4, ".f4(e2m1)"},
        {OPConvertType::OPCVT_FP4_1, ".f4(e1m2)"},
        {OPConvertType::OPCVT_SF8, ".sfb"},
        {OPConvertType::OPCVT_HIF4, ".hif4"},
        {OPConvertType::OPCVT_S4, ".s4"},
        {OPConvertType::OPCVT_S8, ".s8"},
        {OPConvertType::OPCVT_S16, ".sh"},
        {OPConvertType::OPCVT_S32, ".sw"},
        {OPConvertType::OPCVT_S64, ".sd"},
        {OPConvertType::OPCVT_U4, ".u4"},
        {OPConvertType::OPCVT_U8, ".ub"},
        {OPConvertType::OPCVT_U16, ".uh"},
        {OPConvertType::OPCVT_U32, ".uw"},
        {OPConvertType::OPCVT_U64, ".ud"},
        {OPConvertType::OPCVT_FP16_X_2, "fp16x2"},
        {OPConvertType::OPCVT_E4M3_X_4, "e4m3x2"},
        {OPConvertType::OPCVT_U16_X_2, "u16x2"},
        {OPConvertType::OPCVT_U8_X_4, "u8x4"},
        {OPConvertType::OPCVT_S16_X_2, "s16x2"},
        {OPConvertType::OPCVT_S8_X_4, "s8x4"},
        {OPConvertType::OPCVT_NO_CVT, ""},
    }};
    return OPCVT_NAMES;
}

inline const std::string& GetOperandConvertType(OPConvertType type)
{
    const auto& names = OPCVTTypeNames();
    static const std::string INVALID_OPCVT_TYPE = "invalid_opcvt_type";
    if (names.find(type) == names.end()) {
        return INVALID_OPCVT_TYPE;
    }
    return names.at(type);
}

inline const std::unordered_map<OPConvertType, uint32_t>& OPCVTTypeWidth()
{
    static const std::unordered_map<OPConvertType, uint32_t> OPCVT_WIDTHS = {{
        {OPConvertType::OPCVT_DW, 64},
        {OPConvertType::OPCVT_NOT, 64},
        {OPConvertType::OPCVT_NEG, 64},
        {OPConvertType::OPCVT_FP64, 64},
        {OPConvertType::OPCVT_FP32, 64},
        {OPConvertType::OPCVT_TF32, 32},
        {OPConvertType::OPCVT_HF32, 32},
        {OPConvertType::OPCVT_FP16, 16},
        {OPConvertType::OPCVT_BF16, 16},
        {OPConvertType::OPCVT_HIF8, 8},
        {OPConvertType::OPCVT_FP8, 8},
        {OPConvertType::OPCVT_FP8_1, 8},
        {OPConvertType::OPCVT_FP6, 6},
        {OPConvertType::OPCVT_FP6_1, 6},
        {OPConvertType::OPCVT_FP4, 4},
        {OPConvertType::OPCVT_FP4_1, 4},
        {OPConvertType::OPCVT_SF8, 8},
        {OPConvertType::OPCVT_HIF4, 4},
        {OPConvertType::OPCVT_S4, 4},
        {OPConvertType::OPCVT_S8, 8},
        {OPConvertType::OPCVT_S16, 16},
        {OPConvertType::OPCVT_S32, 32},
        {OPConvertType::OPCVT_S64, 64},
        {OPConvertType::OPCVT_U4, 4},
        {OPConvertType::OPCVT_U8, 8},
        {OPConvertType::OPCVT_U16, 16},
        {OPConvertType::OPCVT_U32, 32},
        {OPConvertType::OPCVT_U64, 64},
        {OPConvertType::OPCVT_NO_CVT, 64},
    }};
    return OPCVT_WIDTHS;
}

inline uint32_t GetOperandConvertWidth(OPConvertType type)
{
    const auto& names = OPCVTTypeWidth();
    if (names.find(type) == names.end()) {
        return 0;
    }
    return names.at(type);
}

inline bool IsSignedConvertType(OPConvertType cvt) {
    switch (cvt) {
        case OPConvertType::OPCVT_S64:
        case OPConvertType::OPCVT_S32:
        case OPConvertType::OPCVT_S16:
        case OPConvertType::OPCVT_S8:
            return true;
        default:
            return false;
    }
}

inline uint64_t GetCvtVal(OPConvertType type, uint32_t shamat, uint64_t data)
{
    switch (type) {
        // AU and LU type
        case OPConvertType::OPCVT_DW:
            break;
        case OPConvertType::OPCVT_U32:
            data = static_cast<uint64_t>(static_cast<uint32_t>(data));
            break;
        case OPConvertType::OPCVT_U16:
            data = static_cast<uint64_t>(static_cast<uint16_t>(data));
            break;
        case OPConvertType::OPCVT_U8:
            data = static_cast<uint64_t>(static_cast<uint8_t>(data));
            break;
        case OPConvertType::OPCVT_S32:
            data = static_cast<int64_t>(static_cast<int32_t>(data));
            break;
        case OPConvertType::OPCVT_S16:
            data = static_cast<int64_t>(static_cast<int16_t>(data));
            break;
        case OPConvertType::OPCVT_S8:
            data = static_cast<int64_t>(static_cast<int8_t>(data));
            break;
        case OPConvertType::OPCVT_NOT:
            data = ~data;
            break;
        case OPConvertType::OPCVT_NEG:
            data = (~data) + 1;
            break;

        // FCVT type
        case OPConvertType::OPCVT_FP8:
        case OPConvertType::OPCVT_FP16:
        case OPConvertType::OPCVT_FP32:
        case OPConvertType::OPCVT_FP64:
        case OPConvertType::OPCVT_FP8_1:
        case OPConvertType::OPCVT_BF16:

        case OPConvertType::OPCVT_S64:
        case OPConvertType::OPCVT_U64:
        case OPConvertType::OPCVT_NO_CVT:
            break;

        default:
            break;
    }

    return data << shamat;
}

inline int IsPacketOperandCVT(OPConvertType type)
{
    switch (type) {
        case OPConvertType::OPCVT_U4:
        case OPConvertType::OPCVT_S4:
        case OPConvertType::OPCVT_FP4:
        case OPConvertType::OPCVT_FP4_1:
        case OPConvertType::OPCVT_HIF4:
            return 1;
        case OPConvertType::OPCVT_U16_X_2:
        case OPConvertType::OPCVT_S16_X_2:
        case OPConvertType::OPCVT_FP16_X_2:
        case OPConvertType::OPCVT_BF16_X_2:
        case OPConvertType::OPCVT_E1M2_X_2:
        case OPConvertType::OPCVT_HIF4_X_2:
            return 2;
        case OPConvertType::OPCVT_U8_X_4:
        case OPConvertType::OPCVT_S8_X_4:
        case OPConvertType::OPCVT_E4M3_X_4: {
            return 4;
        }
        default:
            break;
    }
    return 1;
}

enum class OperandWidth {
    OPDW_B = 0x8,
    OPDW_H = 0x10,
    OPDW_W = 0x20,
    OPDW_D = 0x40,
};
inline const std::unordered_map<OperandWidth, std::string>& OPWNames()
{
    static const std::unordered_map<OperandWidth, std::string> OPW_NAMES = {{
        {OperandWidth::OPDW_B, ".b",},
        {OperandWidth::OPDW_D, ".d"},
        {OperandWidth::OPDW_W, ".w"},
        {OperandWidth::OPDW_D, ".d"},
    }};
    return OPW_NAMES;
}
inline const std::string& GetOperandWidthName(OperandWidth type)
{
    const auto& names = OPWNames();
    static const std::string INVALID_OPW_TYPE = "invalid_opperan_width";
    if (names.find(type) == names.end()) {
        return INVALID_OPW_TYPE;
    }
    return names.at(type);
}

}
#endif
