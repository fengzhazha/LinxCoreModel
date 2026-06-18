#pragma once

#ifndef DECODE_UTILS_H
#define DECODE_UTILS_H

#include <cstdint>
#include <cassert>

namespace JCore {
constexpr int32_t  INVALID32 = -1;
constexpr uint32_t INVALIDU32 = 0xffffffff;
constexpr int64_t  SRC_INVALID_VALUE64 = 0x0fffffffffffffff;
constexpr uint64_t WIDTH_16 = 2;
constexpr uint64_t WIDTH_32 = 4;
constexpr uint64_t WIDTH_48 = 6;
constexpr uint64_t WIDTH_64 = 8;
constexpr uint64_t INST_MAX_PC = 0xffffffffffffffff;

constexpr uint64_t HEADER_16_BEGIN = 1;
constexpr uint64_t HEADER_16_END = 3;

constexpr uint64_t HEADER_32_BEGIN = 1;
constexpr uint64_t HEADER_32_END = 3;
const int SHIFT_LEFT1 = 1;
const int SHIFT_LEFT2 = 2;
const int SHIFT_LEFT3 = 3;
const int SHIFT_LEFT12 = 12;

// 16/32位指令源寄存器左右边界
const int64_t GREG_LEFT_BOUND = 1;
const int64_t GREG_RIGHT_BOUND = 23;
const int64_t LINK_LEFT_BOUND = 24;
const int64_t LINK_RIGHT_BOUND = 27;
const int64_t UREG_LEFT_BOUND = 28;
const int64_t UREG_RIGHT_BOUND = 31;

// 16/32位指令目的寄存器左右边界
const int64_t DST_GREG_LEFT_BOUND = 1;
const int64_t DST_GREG_RIGHT_BOUND = 23;
const int64_t DST_UREG_BOUND = 30;
const int64_t DST_LINK_BOUND = 31;
const int64_t DST_UREG_LIMIT = 16;
const int64_t DST_TILE_TLINK = 0;
const int64_t DST_TILE_ULINK = 1;
const int64_t DST_TILE_MLINK = 2;
const int64_t DST_TILE_NLINK = 3;
const int64_t DST_TILE_ACC = 4;
const int64_t DST_TILE_STACK = 5;

// 64位指令源寄存器左右边界
/* VT#1-VT#4 */
const int64_t VLINK_LEFT_BOUND_SIMT = 0;
const int64_t VLINK_RIGHT_BOUND_SIMT = 3;
const int64_t VLINK_LEFT_BOUND_SIMT_REUSE = 96;
const int64_t VLINK_RIGHT_BOUND_SIMT_REUSE = 99;
/* VU#1-VU#4 */
const int64_t VUREG_LEFT_BOUND_SIMT = 8;
const int64_t VUREG_RIGHT_BOUND_SIMT = 11;
const int64_t VUREG_LEFT_BOUND_SIMT_REUSE = 104;
const int64_t VUREG_RIGHT_BOUND_SIMT_REUSE = 107;
/* VM#1-VM#4 */
const int64_t VMREG_LEFT_BOUND_SIMT = 16;
const int64_t VMREG_RIGHT_BOUND_SIMT = 19;
const int64_t VMREG_LEFT_BOUND_SIMT_REUSE = 112;
const int64_t VMREG_RIGHT_BOUND_SIMT_REUSE = 115;
/* VN#1-VN#4 */
const int64_t VNREG_LEFT_BOUND_SIMT = 24;
const int64_t VNREG_RIGHT_BOUND_SIMT = 27;
const int64_t VNREG_LEFT_BOUND_SIMT_REUSE = 120;
const int64_t VNREG_RIGHT_BOUND_SIMT_REUSE = 123;
/* R0 */
const int64_t GREG_ZERO = 95;
/* RI0-RI11 */
const int64_t RI_LEFT_BOUND = 32;
const int64_t RI_RIGHT_BOUND = 43;
/* RO0-RO3 */
const int64_t DST_RO_LEFT_BOUND = 32;
const int64_t DST_RO_RIGHT_BOUND = 35;
/* T#1-T#4 */
const int64_t LINK_LEFT_BOUND_SIMT = 56;
const int64_t LINK_RIGHT_BOUND_SIMT = 59;
/* U#1-U#4 */
const int64_t UREG_LEFT_BOUND_SIMT = 60;
const int64_t UREG_RIGHT_BOUND_SIMT = 63;

const int64_t L_C0_BOUND = 64;
const int64_t L_B0_BOUND = 65;
const int64_t L_C1_BOUND = 68;
const int64_t L_B1_BOUND = 69;
const int64_t L_C2_BOUND = 72;
const int64_t L_B2_BOUND = 73;
const int64_t TILE_TA_BOUND = 80;
const int64_t TILE_TB_BOUND = 81;
const int64_t TILE_TC_BOUND = 82;
const int64_t TILE_TD_BOUND = 83;
const int64_t TILE_TE_BOUND = 84;
const int64_t TILE_TF_BOUND = 85;
const int64_t TILE_TG_BOUND = 86;
const int64_t TILE_TH_BOUND = 87;
const int64_t TILE_TO_BOUND = 88;
const int64_t TILE_TO1_BOUND = 89;
const int64_t TILE_TO2_BOUND = 90;
const int64_t TILE_TO3_BOUND = 91;
const int64_t PREDICATE_MASK_BOUND = 92;

// 64位指令目的寄存器左右边界
const int64_t DST_VLINK_BOUND = 0;
const int64_t DST_VUREG_BOUND = 1;
const int64_t DST_VMREG_BOUND = 2;
const int64_t DST_VNREG_BOUND = 3;
const int64_t DST_SIMT_UREG_BOUND = 62;
const int64_t DST_SIMT_LINK_BOUND = 63;

// 源Tile寄存器索引边界
const int64_t TILE_T_LEFT_BOUND = 0;
const int64_t TILE_T_RIGHT_BOUND = 15;
const int64_t TILE_U_LEFT_BOUND = 16;
const int64_t TILE_U_RIGHT_BOUND = 31;
const int64_t TILE_M_LEFT_BOUND = 32;
const int64_t TILE_M_RIGHT_BOUND = 47;
const int64_t TILE_N_LEFT_BOUND = 48;
const int64_t TILE_N_RIGHT_BOUND = 63;

// 目的Tile寄存器索引边界
const int64_t TILE_T_BOUND = 0;
const int64_t TILE_U_BOUND = 1;
const int64_t TILE_M_BOUND = 2;
const int64_t TILE_N_BOUND = 3;
const int64_t TILE_ACC_BOUND = 4;

// 64位指令s/t_width情况
const int32_t DPRECISION_U = 0;
const int32_t WPRECISION_U = 1;
const int32_t HPRECISION_U = 2;
const int32_t BPRECISION_U = 3;
const int32_t DPRECISION_S = 4;
const int32_t WPRECISION_S = 5;
const int32_t HPRECISION_S = 6;
const int32_t BPRECISION_S = 7;

const uint32_t CVTF_FP64    =   0b00000;
const uint32_t CVTF_FP32    =   0b00001;
const uint32_t CVTF_TF32    =   0b00010;
const uint32_t CVTF_HF32    =   0b00011;
const uint32_t CVTF_FP16    =   0b00100;
const uint32_t CVTF_BF16    =   0b00101;
const uint32_t CVTF_HIF8    =   0b00110;
const uint32_t CVTF_E4M3    =   0b00111;
const uint32_t CVTF_E5M2    =   0b01000;
const uint32_t CVTF_E3M2    =   0b01001;
const uint32_t CVTF_E2M3    =   0b01010;
const uint32_t CVTF_E2M1x2  =   0b01011;
const uint32_t CVTF_E1M2x2  =   0b01100;
const uint32_t CVTF_HIF4x2  =   0b01101;
const uint32_t CVTF_E8M0    =   0b01110;
const uint32_t CVTF_E6M2    =   0b01111;
const uint32_t CVTF_FP16x2  =   0b10000;
const uint32_t CVTF_BF16x2  =   0b10001;
const uint32_t CVTF_E4M3x4  =   0b10010;
const uint32_t CVTF_E5M2x4  =   0b10011;
const uint32_t CVTF_E4M3x2  =   0b10011;
const uint32_t CVTF_E5M2x2  =   0b10011;
const uint32_t CVTF_E6M2x2  =   0b10011;

const uint32_t CVTI_U64     =   0b00000;
const uint32_t CVTI_U32     =   0b00001;
const uint32_t CVTI_U16     =   0b00010;
const uint32_t CVTI_U8      =   0b00011;
const uint32_t CVTI_U4X2    =   0b00100;
const uint32_t CVTI_U16X2   =   0b00101;
const uint32_t CVTI_U8X4    =   0b00110;
const uint32_t CVTI_S64     =   0b01000;
const uint32_t CVTI_S32     =   0b01001;
const uint32_t CVTI_S16     =   0b01010;
const uint32_t CVTI_S8      =   0b01011;
const uint32_t CVTI_S4X2    =   0b01100;
const uint32_t CVTI_S16X2   =   0b01101;
const uint32_t CVTI_S8X4    =   0b01110;

constexpr int32_t STORE_TO_REG = 0;
constexpr int32_t STORE_TO_SIMM = 1;
constexpr int32_t STORE_TO_UIMM = 2;
constexpr int32_t STORE_TO_SYS = 3;
constexpr int32_t STORE_TO_TILEREG = 4;

constexpr int32_t STORE_TO_REG_DST = 0;
constexpr int32_t STORE_TO_REG_TILE = 1;

// generate a mask. bits[m:n] all are 1, others are 0
constexpr uint64_t GetMask(unsigned m, unsigned n = 0)                  { return ~(~0ULL << (m - n + 1)) << n; }
inline uint64_t GetMaskSafe(unsigned m, unsigned n = 0) { // [n..m]
    if (m < n) {
        return 0;
    }
    unsigned width = m - n + 1;
    if (width >= 64) {
        return ~0ULL;          // 覆盖 n==0 且 m==63 的情况
    }
    return ((1ULL << width) - 1ULL) << n;
}

// get bits[m:n]
constexpr uint64_t GetBits(uint64_t bits, unsigned m, unsigned n = 0)   { return (bits & GetMask(m, n)) >> n; }
constexpr bool BitsTest(uint64_t bits, unsigned i)                      { return (bits & (1ULL << i)) != 0ULL; }
constexpr bool GetBit(uint64_t bits, unsigned i)                        { return BitsTest(bits, i); }

inline uint64_t Uextract(uint64_t dat, uint32_t start, uint32_t len) { return (dat >> start) & ((1 << len) - 1);}
inline int64_t Sextract64(uint64_t uDat, uint32_t start, uint32_t len)
{
    int64_t sDat = static_cast<int64_t>((uDat >> start) & ((1 << len) - 1));
    const int64_t num = 64;
    sDat = (sDat << (num - len)) >> (num - len);
    return sDat;
}

inline uint32_t CheckMInstSize(uint64_t bin)
{
    if ((bin & 0x1) == 0) {
        // 16 bit
        if (GetBits(bin, HEADER_16_END, HEADER_16_BEGIN) == 0b111) {
            return WIDTH_48;
        }
        return WIDTH_16;
    } else {
        if (GetBits(bin, HEADER_32_END, HEADER_32_BEGIN) == 0b111) {
            return WIDTH_64;
        }
        return WIDTH_32;
    }
}
class DecodeUtiles {
public:
    static int64_t                          Extract32(uint32_t bin, int start, int length);
    static int64_t                          Sextract32(uint32_t bin, int start, int length);
    static int64_t                          Extract48(uint64_t bin, int start, int length);
    static int64_t                          Sextract48(uint64_t bin, int start, int length);
    static int64_t                          Extract64(uint64_t bin, int start, int length);
    static int64_t                          Sextract64(uint64_t bin, int start, int length);
    // For left shift
    static int64_t                          ExShift1(uint64_t bin) {return bin << SHIFT_LEFT1;};
    static int64_t                          ExShift2(uint64_t bin) {return bin << SHIFT_LEFT2;};
    static int64_t                          ExShift3(uint64_t bin) {return bin << SHIFT_LEFT3;};
    static int64_t                          ExShift12(uint64_t bin) {return bin << SHIFT_LEFT12;};
    static int64_t                          ExShift1Right(int64_t bin) {return bin >> SHIFT_LEFT1;};
    static int64_t                          ExShift2Right(int64_t bin) {return bin >> SHIFT_LEFT2;};
    static int64_t                          ExShift3Right(int64_t bin) {return bin >> SHIFT_LEFT3;};
    static int64_t                          ExShift12Right(int64_t bin) {return bin >> SHIFT_LEFT12;};
    // To concatenate multiple segments of binary
    static uint64_t                         Deposit(uint64_t second, int shift, uint64_t first);
    static uint32_t                         Deposit32(uint32_t value, int start, int length, uint32_t fieldval);
    static uint64_t                         Deposit64(uint64_t value, int start, int length, uint64_t fieldval);
    static uint64_t                         AutoInc1(uint64_t bin);
    static uint64_t                         AutoInc2(uint64_t bin);
    static uint64_t                         AutoInc3(uint64_t bin);

};
}
#endif