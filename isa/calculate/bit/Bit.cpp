#include "Bit.h"

namespace JCore {
namespace Calculate {
namespace Bit {

// W: 1..64
static inline uint64_t FullMask64(unsigned w)
{
    return (w >= 64) ? ~0ULL : ((1ULL << w) - 1ULL);
}

// n: 0..W
static inline uint64_t LowMaskN(unsigned n, unsigned W)
{
    if (n == 0) {
        return 0;
    }
    if (n >= W) {
        return FullMask64(W);
    }
    return (1ULL << n) - 1ULL;
}

// W:1..64
static inline uint64_t RotateRightN(uint64_t x, uint32_t s, unsigned w)
{
    x &= FullMask64(w);
    if (w == 1) {
        return x;
    }
    s %= w;
    if (s == 0) {
        return x;
    }
    uint64_t r = (x >> s) | (x << (w - s));
    return r & FullMask64(w);
}

// 在 W 位环内：从 bit m 开始长度 n 的环形掩码
static inline uint64_t RMaskN(unsigned m, unsigned n, unsigned w)
{
    if (n == 0) {
        return 0ULL;
    }
    if (n >= w) {
        return FullMask64(w);
    }
    m %= w;
    uint64_t low = LowMaskN(n, w);
    return RotateRightN(low, static_cast<uint32_t>((w - m) % w), w);
}

// 在 W 位环内：截取 [m, m+n) 对齐到低位
static inline uint64_t ExtractRangeToLSBN(uint64_t x, unsigned m, unsigned n, unsigned w)
{
    if (n == 0) {
        return 0ULL;
    }
    x &= FullMask64(w);
    m %= w;
    uint64_t y = RotateRightN(x, static_cast<uint32_t>(m), w);
    return y & LowMaskN(n, w);
}

// 将 v 的低 width 位做符号扩展到 64 位 // width:1..64
static inline uint64_t SignExtendTo64(uint64_t v, uint32_t width)
{
    if (width >= 64) {
        return v;
    }
    uint64_t mask = (1ULL << width) - 1ULL;
    v &= mask;
    uint64_t sign = 1ULL << (width - 1);
    return (v ^ sign) - sign;
}

// 写回时按 dstW 截断（不改变符号语义，只做寄存器位宽约束） // dstW:1..64
static inline uint64_t TruncToWidth(uint64_t v, unsigned dstW)
{
    return v & FullMask64(dstW);
}

static bool CalcInstBXS(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }

    uint64_t data = inst.srcs[SRC0_IDX]->data;
    uint64_t imms = inst.srcs[SRC1_IDX]->data;
    uint64_t imml = inst.srcs[SRC2_IDX]->data;
    if (imms >= NUM64 || imml > NUM64 || imml == 0) {
        return false;
    }
    uint32_t srcWidth = GetOperandConvertWidth(inst.srcs[SRC0_IDX]->cvt);
    uint32_t dstWidth = GetOperandConvertWidth(inst.dsts[DST0_IDX]->cvt);

    uint64_t val = ExtractRangeToLSBN(data, imms, imml, srcWidth);
    uint64_t se  = SignExtendTo64(val, imml);
    inst.dsts[DST0_IDX]->data = TruncToWidth(se, dstWidth);
    return true;
}

static bool CalcInstBXU(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }

    uint64_t data = inst.srcs[SRC0_IDX]->data;
    uint64_t imms = inst.srcs[SRC1_IDX]->data;
    uint64_t imml = inst.srcs[SRC2_IDX]->data;
    if (imms >= NUM64 || imml > NUM64 || imml == 0) {
        return false;
    }
    uint32_t srcWidth = GetOperandConvertWidth(inst.srcs[SRC0_IDX]->cvt);
    uint32_t dstWidth = GetOperandConvertWidth(inst.dsts[DST0_IDX]->cvt);

    uint64_t val = ExtractRangeToLSBN(data, imms, imml, srcWidth); // 低 N 位
    inst.dsts[DST0_IDX]->data = TruncToWidth(val, dstWidth);
    return true;
}

static bool CalcInstBIC(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }

    uint64_t data = inst.srcs[SRC0_IDX]->data;
    uint64_t imms = inst.srcs[SRC1_IDX]->data;
    uint64_t imml = inst.srcs[SRC2_IDX]->data;
    if (imms >= NUM64 || imml > NUM64 || imml == 0) {
        return false;
    }

    uint32_t srcWidth = GetOperandConvertWidth(inst.srcs[SRC0_IDX]->cvt);
    uint32_t dstWidth = GetOperandConvertWidth(inst.dsts[DST0_IDX]->cvt);
    uint64_t fm   = FullMask64(srcWidth);
    uint64_t mask = RMaskN(imms, imml, srcWidth);
    uint64_t result = ((data & fm) & (~mask & fm));
    inst.dsts[DST0_IDX]->data = TruncToWidth(result, dstWidth);
    return true;
}

static bool CalcInstBIS(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }

    uint64_t data = inst.srcs[SRC0_IDX]->data;
    uint64_t imms = inst.srcs[SRC1_IDX]->data;
    uint64_t imml = inst.srcs[SRC2_IDX]->data;
    if (imms >= NUM64 || imml > NUM64 || imml == 0) {
        return false;
    }
    uint32_t srcWidth = GetOperandConvertWidth(inst.srcs[SRC0_IDX]->cvt);
    uint32_t dstWidth = GetOperandConvertWidth(inst.dsts[DST0_IDX]->cvt);
    uint64_t fm   = FullMask64(srcWidth);
    uint64_t mask = RMaskN(imms, imml, srcWidth);
    uint64_t result = ((data & fm) | mask) & fm;
    inst.dsts[DST0_IDX]->data = TruncToWidth(result, dstWidth);
    return true;
}

static bool CalcInstCTZ(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    uint64_t data = inst.srcs[SRC0_IDX]->data;
    uint64_t imms = inst.srcs[SRC1_IDX]->data; // M
    uint64_t imml = inst.srcs[SRC2_IDX]->data; // N
    uint32_t srcWidth = GetOperandConvertWidth(inst.srcs[SRC0_IDX]->cvt);
    uint32_t dstWidth = GetOperandConvertWidth(inst.dsts[DST0_IDX]->cvt);
    if (srcWidth == 0 || srcWidth > 64 || dstWidth == 0 || dstWidth > 64) {
        return false;
    }
    // 允许环绕：M 任意（在 W 位环上取模）；N 必须 1..W
    if (imml == 0 || imml > srcWidth) {
        return false;
    }
    uint64_t val = ExtractRangeToLSBN(data, static_cast<unsigned>(imms), static_cast<unsigned>(imml), srcWidth);
    if (val == 0) {
        inst.dsts[DST0_IDX]->data = imml;
        return true;
    }
    uint32_t c = 0;
    while ((val & 1ULL) == 0ULL) {
        ++c;
        val >>= 1;
    }
    inst.dsts[DST0_IDX]->data = c;
    return true;
}

static bool CalcInstCLZ(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    uint64_t data = inst.srcs[SRC0_IDX]->data;
    uint64_t imms = inst.srcs[SRC1_IDX]->data; // M
    uint64_t imml = inst.srcs[SRC2_IDX]->data; // N
    uint32_t srcWidth = GetOperandConvertWidth(inst.srcs[SRC0_IDX]->cvt);
    uint32_t dstWidth = GetOperandConvertWidth(inst.dsts[DST0_IDX]->cvt);
    if (srcWidth == 0 || srcWidth > 64 || dstWidth == 0 || dstWidth > 64) {
        return false;
    }
    if (imml == 0 || imml > srcWidth) {
        return false;
    }
    uint64_t val = ExtractRangeToLSBN(data, static_cast<unsigned>(imms), static_cast<unsigned>(imml), srcWidth);
    uint64_t result = 0;
    if (val == 0) {
        inst.dsts[DST0_IDX]->data = imml;
        return true;
    }

    uint32_t c = 0;
    uint32_t n = static_cast<uint32_t>(imml);
    uint64_t topBit = (n == 64) ? (1ULL << 63) : (1ULL << (n - 1U));
    while ((val & topBit) == 0ULL) {
        ++c;
        topBit >>= 1;
    }
    result = c;
    inst.dsts[DST0_IDX]->data = TruncToWidth(result, dstWidth);
    return true;
}

static bool CalcInstBCNT(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    uint64_t data = inst.srcs[SRC0_IDX]->data;
    uint64_t imms = inst.srcs[SRC1_IDX]->data; // M
    uint64_t imml = inst.srcs[SRC2_IDX]->data; // N
    uint32_t srcWidth = GetOperandConvertWidth(inst.srcs[SRC0_IDX]->cvt);
    uint32_t dstWidth = GetOperandConvertWidth(inst.dsts[DST0_IDX]->cvt);
    if (srcWidth == 0 || srcWidth > 64 || dstWidth == 0 || dstWidth > 64) {
        return false;
    }
    if (imml == 0 || imml > srcWidth) {
        return false;
    }
    uint64_t val = ExtractRangeToLSBN(data, static_cast<unsigned>(imms), static_cast<unsigned>(imml), srcWidth);

    // Brian Kernighan: 每次清除最低位的 1
    uint32_t c = 0;
    while (val != 0) {
        val &= (val - 1);
        ++c;
    }
    inst.dsts[DST0_IDX]->data = c;
    return true;
}

static inline bool IsValidM(uint64_t m) {
    switch (m) {
        case NUM2:
        case NUM4:
        case NUM8:
        case NUM16:
        case NUM32:
        case NUM64:
            return true;
        default:
            return false;
    }
}
static inline bool IsValidN(uint64_t n) {
    switch (n) {
        case 1:
        case NUM2:
        case NUM4:
        case NUM8:
        case NUM16:
        case NUM32:
            return true;
        default:
            return false;
    }
}

static bool CalcInstReverse(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    uint64_t data = inst.srcs[SRC0_IDX]->data;
    uint64_t m = inst.srcs[SRC1_IDX]->data;
    uint64_t n = inst.srcs[SRC2_IDX]->data;
    if (!IsValidM(m) || !IsValidN(n) || n > m) {
        return false;
    }
    uint32_t srcWidth = GetOperandConvertWidth(inst.srcs[SRC0_IDX]->cvt);
    uint32_t dstWidth = GetOperandConvertWidth(inst.dsts[DST0_IDX]->cvt);
    if (srcWidth == 0 || srcWidth > 64 || dstWidth == 0 || dstWidth > 64) {
        return false;
    }
    // 在源位宽范围内做 reverse；m 不能超过源位宽（否则 “每个 M 位范围内” 无从定义）
    if (m > srcWidth) {
        return false;
    }
    const unsigned units = static_cast<unsigned>(m / n);           // 每块含多少个 N-bit 单元
    const uint64_t unitMask  = LowMaskN(static_cast<unsigned>(n), 64); // 提取单元用：低 N 位为 1
    const uint64_t chunkMask = LowMaskN(static_cast<unsigned>(m), 64); // 提取块用：低 M 位为 1
    const uint64_t fm = FullMask64(srcWidth);
    data &= fm;
    uint64_t out = data; // 先拷贝：未处理到的位默认保留原样（仅限 srcWidth 内）
    // base 是当前块在寄存器位宽中的起始 bit 位置：0, M, 2M, ...
    // 仅处理 base+M <= srcWidth 的“完整块”
    for (unsigned base = 0; base + m <= srcWidth; base += static_cast<unsigned>(m)) {
        // 取出当前 M-bit 块，右对齐到 bit[0..M-1]
        // chunk 仅包含这 M 位，块外的位被 mask 掉
        uint64_t chunk = (data >> base) & chunkMask;
        uint64_t revChunk = 0;
        // i 表示源单元索引（从低到高：0..units-1）
        for (unsigned i = 0; i < units; ++i) {
            // 提取第 i 个 N-bit 单元（仍然右对齐）
            uint64_t u = (chunk >> (i * static_cast<unsigned>(n))) & unitMask;
            // 逆序后它应该放到第 (units-1-i) 个位置
            unsigned dst = (units - 1U - i) * static_cast<unsigned>(n);
            // 放回 revChunk 的对应位置
            revChunk |= (u << dst);
        }
        // 将 out 中当前块清零，然后写回 revChunk
        out &= ~(chunkMask << base);     // 清掉 out 的这 M 位
        out |=  (revChunk << base);      // 写入新块
    }
    out &= fm;
    inst.dsts[DST0_IDX]->data = TruncToWidth(out, dstWidth);
    return true;
}

// 位域插入(Bit Field Insert)
// 从右源寄存器取低 N 位，替换源寄存器的 [M..M+N-1] 位，写回目的寄存器
static bool CalcInstBFI(MInst &inst)
{
    if (inst.srcs.size() != SRC4_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }

    const uint64_t left  = inst.srcs[SRC0_IDX]->data; // 左源
    const uint64_t right = inst.srcs[SRC1_IDX]->data; // 右源（取低N位）
    const uint32_t m     = static_cast<uint32_t>(inst.srcs[SRC2_IDX]->data); // 起始位M
    const uint32_t n     = static_cast<uint32_t>(inst.srcs[SRC3_IDX]->data); // 位宽N

    // 生成 [m..m+n-1] 掩码（复用 GetMaskSafe）
    const uint64_t fieldMask = GetMaskSafe(static_cast<unsigned>(m + n - 1U),
                                          static_cast<unsigned>(m));
    // 取右源低 n 位并对齐到 m
    const uint64_t lowMask   = (n >= 64U) ? ~0ULL : ((1ULL << n) - 1ULL);
    const uint64_t insertVal = (right & lowMask) << m;

    // 清空左源对应位段，再写入
    inst.dsts[DST0_IDX]->data = (left & ~fieldMask) | (insertVal & fieldMask);
    return true;
}

//==============================
// 128 位循环左移：用两个 64 位实现
// in:  hi:高64, lo:低64
// shamt: 0..127（超出部分按 128 取模）
// out: ohi:高64, olo:低64
//==============================
static inline void RotateLeft128(uint64_t hi, uint64_t lo, uint32_t shamt,
                                 uint64_t& ohi, uint64_t& olo)
{
    shamt &= 127U;
    if (shamt == 0U) {
        ohi = hi;
        olo = lo;
        return;
    }
    if (shamt < 64U) {
        // (hi:lo) 左转 k：
        // new_hi = (hi<<k) | (lo>>(64-k))
        // new_lo = (lo<<k) | (hi>>(64-k))
        const uint32_t k = shamt;
        ohi = (hi << k) | (lo >> (64U - k));
        olo = (lo << k) | (hi >> (64U - k));
        return;
    }
    if (shamt == 64U) {
        // 左转 64 位：高低 64 位交换
        ohi = lo;
        olo = hi;
        return;
    }
    // shamt > 64：等价于先交换，再左转 (shamt-64)
    const uint32_t k = shamt - 64U;
    ohi = (lo << k) | (hi >> (64U - k));
    olo = (hi << k) | (lo >> (64U - k));
}

// 拼接(Concatenate)
// (rs1<<64) + rs2 形成 128 位，然后循环左移 shamt 位
// 结果高64写到第一个目的寄存器，低64写到第二个目的寄存器
static bool CalcInstCCAT(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX || (inst.dsts.size() != DST2_IDX && inst.dsts.size() != DST1_IDX)) {
        return false;
    }

    uint64_t hi = inst.srcs[SRC0_IDX]->data; // 左源 -> 128位高64
    uint64_t lo = inst.srcs[SRC1_IDX]->data; // 右源 -> 128位低64
    const uint32_t shamt = static_cast<uint32_t>(inst.srcs[SRC2_IDX]->data);

    uint64_t ohi = 0;
    uint64_t olo = 0;
    RotateLeft128(hi, lo, shamt, ohi, olo); // 复用 128 位循环左移

    inst.dsts[DST0_IDX]->data = ohi; // 高64
    if (inst.dsts.size() == DST2_IDX) {
        inst.dsts[DST1_IDX]->data = olo; // 低64
    }
    return true;
}

// 拼接·字(Concatenate Word)
// (low32(rs1)<<32) + low32(rs2) 得到 64 位，然后循环左移 shamt 位（按 64 位循环）
// 结果高32 符号扩展写到第一个目的寄存器，低32 符号扩展写到第二个目的寄存器
static bool CalcInstCCATW(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX || inst.dsts.size() != DST2_IDX) {
        return false;
    }

    const uint32_t a = static_cast<uint32_t>(inst.srcs[SRC0_IDX]->data); // rs1 低32
    const uint32_t b = static_cast<uint32_t>(inst.srcs[SRC1_IDX]->data); // rs2 低32
    const uint32_t shamt = static_cast<uint32_t>(inst.srcs[SRC2_IDX]->data);

    uint64_t x = (static_cast<uint64_t>(a) << 32) + static_cast<uint64_t>(b);

    // 复用 RotateRight64 实现循环左移：rol(x,s) = ror(x,64-s)
    const uint32_t s = shamt; // 已保证不超范围
    x = (s == 0U) ? x : RotateRightN(x, 64U - s, NUM64);

    const uint32_t high32 = static_cast<uint32_t>(x >> 32);
    const uint32_t low32  = static_cast<uint32_t>(x);

    // 复用 SignExtend：32 位符号扩展到 64 位
    inst.dsts[DST0_IDX]->data = SignExtendTo64(static_cast<uint64_t>(high32), 32U);
    inst.dsts[DST1_IDX]->data = SignExtendTo64(static_cast<uint64_t>(low32), 32U);
    return true;
}

Handler Lookup(Opcode op)
{
    static const std::unordered_map<Opcode, Handler> BIT_TABLE = std::unordered_map<Opcode, Handler> {
        {Opcode::OP_BXS, &CalcInstBXS},
        {Opcode::OP_BXU, &CalcInstBXU},
        {Opcode::OP_BIC, &CalcInstBIC},
        {Opcode::OP_BIS, &CalcInstBIS},
        {Opcode::OP_CTZ, &CalcInstCTZ},
        {Opcode::OP_CLZ, &CalcInstCLZ},
        {Opcode::OP_BCNT, &CalcInstBCNT},
        {Opcode::OP_REV, &CalcInstReverse},
        {Opcode::OP_BFI, &CalcInstBFI},
        {Opcode::OP_CCAT, &CalcInstCCAT},
        {Opcode::OP_CCATW, &CalcInstCCATW},
    };
    auto it = BIT_TABLE.find(op);
    return (it == BIT_TABLE.end()) ? 0 : it->second;
}

bool CalculateBit(MInst &inst)
{
    Handler fn = Lookup(inst.opcode);
    return fn ? fn(inst) : false;
}

} // namespace Bit
} // namespace Calculate
} // namespace JCore