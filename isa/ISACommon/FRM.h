#pragma once

#ifndef FRM_H
#define FRM_H

#include <cstdint>

namespace JCore {
constexpr std::uint64_t FRM_BIT_BEGIN = 37;
constexpr std::uint64_t FRM_BIT_END = 39;
enum class FRMMode {
    FRM_NONE = 0,
    FRM_RNE, // CSTATE编码可控，向最近偶数舍入(Round to Nearest, ties to Even)
    FRM_RTZ, // CSTATE编码可控，向零舍入(Round towards Zero)
    FRM_RDN, // CSTATE编码可控，向负无穷舍入(Round Down/towards -∞)
    FRM_RUP, // CSTATE编码可控，向正无穷舍入(Round Up/towards +∞)
    FRM_RNA, // 向远离 0 的方向舍入
    FRM_RTO, // Round to Odd（向最近奇数舍入）
    FRM_RHB, // Hybrid Rounding（混合舍入模式）
    FRM_INV,
};

enum class Saturation {
    NONE = 0,
    SATURATION = 1,
};

enum class FCVTMode {
    FCVT_NORMAL = 0, // 格式转换
    FCVT_QUANT, // 量化
    FCVT_DEQUANT, // 反量化
    FCVT_RESVERVE,
};
}
#endif