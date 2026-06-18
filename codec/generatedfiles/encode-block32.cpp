#include "Instruction.h"
#include <unordered_map>

namespace JCore {
std::unordered_map<Opcode, uint32_t> op2bin_32 = {
    {OP_ADD, 0b00000000000000000000000000000101},
    {OP_SUB, 0b00000000000000000001000000000101},
    {OP_AND, 0b00000000000000000010000000000101},
    {OP_OR, 0b00000000000000000011000000000101},
    {OP_XOR, 0b00000000000000000100000000000101},
    {OP_SRL, 0b00000000000000000101000000000101},
    {OP_SRA, 0b00000000000000000110000000000101},
    {OP_SLL, 0b00000000000000000111000000000101},
    {OP_ADDI, 0b00000000000000000000000000010101},
    {OP_SUBI, 0b00000000000000000001000000010101},
    {OP_ANDI, 0b00000000000000000010000000010101},
    {OP_ORI, 0b00000000000000000011000000010101},
    {OP_XORI, 0b00000000000000000100000000010101},
    {OP_SRLI, 0b00000000000000000101000000010101},
    {OP_SRAI, 0b00000000000000000110000000010101},
    {OP_SLLI, 0b00000000000000000111000000010101},
    {OP_ADDW, 0b00000000000000000000000000100101},
    {OP_SUBW, 0b00000000000000000001000000100101},
    {OP_ANDW, 0b00000000000000000010000000100101},
    {OP_ORW, 0b00000000000000000011000000100101},
    {OP_XORW, 0b00000000000000000100000000100101},
    {OP_SRLW, 0b00000000000000000101000000100101},
    {OP_SRAW, 0b00000000000000000110000000100101},
    {OP_SLLW, 0b00000000000000000111000000100101},
    {OP_ADDIW, 0b00000000000000000000000000110101},
    {OP_SUBIW, 0b00000000000000000001000000110101},
    {OP_ANDIW, 0b00000000000000000010000000110101},
    {OP_ORIW, 0b00000000000000000011000000110101},
    {OP_XORIW, 0b00000000000000000100000000110101},
    {OP_SRLIW, 0b00000000000000000101000000110101},
    {OP_SRAIW, 0b00000000000000000110000000110101},
    {OP_SLLIW, 0b00000000000000000111000000110101},
    {OP_CMP_EQ, 0b00000000000000000000000001000101},
    {OP_CMP_NE, 0b00000000000000000001000001000101},
    {OP_CMP_AND, 0b00000000000000000010000001000101},
    {OP_CMP_OR, 0b00000000000000000011000001000101},
    {OP_CMP_LT, 0b00000000000000000100000001000101},
    {OP_CMP_GE, 0b00000000000000000101000001000101},
    {OP_CMP_LTU, 0b00000000000000000110000001000101},
    {OP_CMP_GEU, 0b00000000000000000111000001000101},
    {OP_CMP_EQ_I, 0b00000000000000000000000001010101},
    {OP_CMP_NE_I, 0b00000000000000000001000001010101},
    {OP_CMP_AND_I, 0b00000000000000000010000001010101},
    {OP_CMP_OR_I, 0b00000000000000000011000001010101},
    {OP_CMP_LT_I, 0b00000000000000000100000001010101},
    {OP_CMP_GE_I, 0b00000000000000000101000001010101},
    {OP_CMP_LTU_I, 0b00000000000000000110000001010101},
    {OP_CMP_GEU_I, 0b00000000000000000111000001010101},
    {OP_SETC_EQ, 0b00000000000000000000000001100101},
    {OP_SETC_NE, 0b00000000000000000001000001100101},
    {OP_SETC_AND, 0b00000000000000000010000001100101},
    {OP_SETC_OR, 0b00000000000000000011000001100101},
    {OP_SETC_LT, 0b00000000000000000100000001100101},
    {OP_SETC_GE, 0b00000000000000000101000001100101},
    {OP_SETC_LTU, 0b00000000000000000110000001100101},
    {OP_SETC_GEU, 0b00000000000000000111000001100101},
    {OP_SETC_EQ_I, 0b00000000000000000000000001110101},
    {OP_SETC_NE_I, 0b00000000000000000001000001110101},
    {OP_SETC_AND_I, 0b00000000000000000010000001110101},
    {OP_SETC_OR_I, 0b00000000000000000011000001110101},
    {OP_SETC_LT_I, 0b00000000000000000100000001110101},
    {OP_SETC_GE_I, 0b00000000000000000101000001110101},
    {OP_SETC_LTU_I, 0b00000000000000000110000001110101},
    {OP_SETC_GEU_I, 0b00000000000000000111000001110101},
    {OP_BXS, 0b00000000000000000000000001100111},
    {OP_BXU, 0b00000000000000000001000001100111},
    {OP_BIC, 0b00000000000000000010000001100111},
    {OP_BIS, 0b00000000000000000011000001100111},
    {OP_MUL, 0b00000000000000000000000001000111},
    {OP_MULU, 0b00000000000000000001000001000111},
    {OP_MULW, 0b00000000000000000010000001000111},
    {OP_MULUW, 0b00000000000000000011000001000111},
    {OP_MADD, 0b00000000000000000110000001000111},
    {OP_MADDW, 0b00000000000000000111000001000111},
    {OP_SETRET, 0b00000000000000000000010100000111},
    {OP_ADDTPC, 0b00000000000000000000000000000111},
    {OP_LUI, 0b00000000000000000000000000010111},
    {OP_LB, 0b00000000000000000000000000001001},
    {OP_LH, 0b00000000000000000001000000001001},
    {OP_LW, 0b00000000000000000010000000001001},
    {OP_LD, 0b00000000000000000011000000001001},
    {OP_LBU, 0b00000000000000000100000000001001},
    {OP_LHU, 0b00000000000000000101000000001001},
    {OP_LWU, 0b00000000000000000110000000001001},
    {OP_LBI, 0b00000000000000000000000000011001},
    {OP_LHI, 0b00000000000000000001000000011001},
    {OP_LWI, 0b00000000000000000010000000011001},
    {OP_LDI, 0b00000000000000000011000000011001},
    {OP_LBUI, 0b00000000000000000100000000011001},
    {OP_LHUI, 0b00000000000000000101000000011001},
    {OP_LWUI, 0b00000000000000000110000000011001},
    {OP_LHI_U, 0b00000000000000000001000000101001},
    {OP_LWI_U, 0b00000000000000000010000000101001},
    {OP_LDI_U, 0b00000000000000000011000000101001},
    {OP_LHUI_U, 0b00000000000000000101000000101001},
    {OP_LWUI_U, 0b00000000000000000110000000101001},
    {OP_SB, 0b00000000000000000000000001001001},
    {OP_SH, 0b00000000000000000001000001001001},
    {OP_SW, 0b00000000000000000010000001001001},
    {OP_SD, 0b00000000000000000011000001001001},
    {OP_SH_U, 0b00000000000000000101000001001001},
    {OP_SW_U, 0b00000000000000000110000001001001},
    {OP_SD_U, 0b00000000000000000111000001001001},
    {OP_SBI, 0b00000000000000000000000001011001},
    {OP_SHI, 0b00000000000000000001000001011001},
    {OP_SWI, 0b00000000000000000010000001011001},
    {OP_SDI, 0b00000000000000000011000001011001},
    {OP_SHI_U, 0b00000000000000000101000001011001},
    {OP_SWI_U, 0b00000000000000000110000001011001},
    {OP_SDI_U, 0b00000000000000000111000001011001},
    {OP_DIV, 0b00000000000000000000000001010111},
    {OP_DIVU, 0b00000000000000000001000001010111},
    {OP_DIVW, 0b00000000000000000010000001010111},
    {OP_DIVUW, 0b00000000000000000011000001010111},
    {OP_REM, 0b00000000000000000100000001010111},
    {OP_REMU, 0b00000000000000000101000001010111},
    {OP_REMW, 0b00000000000000000110000001010111},
    {OP_REMUW, 0b00000000000000000111000001010111},
    {OP_FENCE_I, 0b00010000000000000010000000101011},
    {OP_BSE, 0b00000000000000000000000000101011},
    {OP_BWE, 0b00000000000100000000000000101011},
    {OP_BWI, 0b00000000001000000000000000101011},
    {OP_ASSERT, 0b00000000000000000001000000101011},
    {OP_SETC_TGT, 0b00000000000000000100000000111011},
    {OP_ACRC, 0b00000000000000000011000000101011},
    {OP_ACRE, 0b00000001000000000011000000101011},
    {OP_MAX, 0b00000000000000000100000001011011},
    {OP_MAXU, 0b00001000000000000100000001011011},
    {OP_MIN, 0b00000000000000000101000001011011},
    {OP_MINU, 0b00001000000000000101000001011011},
    {OP_FABS, 0b00000000000000000000000001111011},
    {OP_FSQRT, 0b00000000000000000001000001111011},
    {OP_FEXP, 0b00000000000000000010000001111011},
    {OP_FRECIP, 0b00000000000000000110000001111011},
    {OP_FMAX, 0b00000000000000000110000001011011},
    {OP_FMIN, 0b00000000000000000111000001011011},
    {OP_FADD, 0b00000000000000000000000001001011},
    {OP_FSUB, 0b00000000000000000001000001001011},
    {OP_FMUL, 0b00000000000000000010000001001011},
    {OP_FDIV, 0b00000000000000000011000001001011},
    {OP_FEQ, 0b00000000000000000000000001011011},
    {OP_FNE, 0b00000000000000000001000001011011},
    {OP_FLT, 0b00000000000000000010000001011011},
    {OP_FGE, 0b00000000000000000011000001011011},
    {OP_FEQS, 0b00001000000000000000000001011011},
    {OP_FNES, 0b00001000000000000001000001011011},
    {OP_FLTS, 0b00001000000000000010000001011011},
    {OP_FGES, 0b00001000000000000011000001011011},
    {OP_SW_ADD, 0b00000000000000000011000000001011},
    {OP_SW_AND, 0b00010000000000000011000000001011},
    {OP_SW_OR, 0b00100000000000000011000000001011},
    {OP_SW_XOR, 0b00110000000000000011000000001011},
    {OP_SW_SMAX, 0b01000000000000000011000000001011},
    {OP_SW_SMIN, 0b01010000000000000011000000001011},
    {OP_SW_UMAX, 0b01100000000000000011000000001011},
    {OP_SW_UMIN, 0b01110000000000000011000000001011},
    {OP_SD_ADD, 0b00000000000000000101000000001011},
    {OP_SD_AND, 0b00010000000000000101000000001011},
    {OP_SD_OR, 0b00100000000000000101000000001011},
    {OP_SD_XOR, 0b00110000000000000101000000001011},
    {OP_SD_SMAX, 0b01000000000000000101000000001011},
    {OP_SD_SMIN, 0b01010000000000000101000000001011},
    {OP_SD_UMAX, 0b01100000000000000101000000001011},
    {OP_SD_UMIN, 0b01110000000000000101000000001011},
    {OP_LR_B, 0b00000000000000000000000000001011},
    {OP_LR_H, 0b00010000000000000000000000001011},
    {OP_LR_W, 0b00100000000000000000000000001011},
    {OP_LR_D, 0b00110000000000000000000000001011},
    {OP_FENCE_D, 0b00000000000000000010000000101011},
    {OP_SC_B, 0b00000000000000000001000000001011},
    {OP_SC_H, 0b00010000000000000001000000001011},
    {OP_SC_W, 0b00100000000000000001000000001011},
    {OP_SC_D, 0b00110000000000000001000000001011},
    {OP_LW_ADD, 0b00000000000000000010000000001011},
    {OP_LW_AND, 0b00010000000000000010000000001011},
    {OP_LW_OR, 0b00100000000000000010000000001011},
    {OP_LW_XOR, 0b00110000000000000010000000001011},
    {OP_LW_SMAX, 0b01000000000000000010000000001011},
    {OP_LW_SMIN, 0b01010000000000000010000000001011},
    {OP_LW_UMAX, 0b01100000000000000010000000001011},
    {OP_LW_UMIN, 0b01110000000000000010000000001011},
    {OP_LD_ADD, 0b00000000000000000100000000001011},
    {OP_LD_AND, 0b00010000000000000100000000001011},
    {OP_LD_OR, 0b00100000000000000100000000001011},
    {OP_LD_XOR, 0b00110000000000000100000000001011},
    {OP_LD_SMAX, 0b01000000000000000100000000001011},
    {OP_LD_SMIN, 0b01010000000000000100000000001011},
    {OP_LD_UMAX, 0b01100000000000000100000000001011},
    {OP_LD_UMIN, 0b01110000000000000100000000001011},
    {OP_SWAPB, 0b00000000000000000110000000001011},
    {OP_SWAPH, 0b00010000000000000110000000001011},
    {OP_SWAPW, 0b00100000000000000110000000001011},
    {OP_SWAPD, 0b00110000000000000110000000001011},
    {OP_FCVT, 0b00000000000000000000000001101011},
    {OP_FCVTA, 0b00000000000000000001000001101011},
    {OP_FCVTM, 0b00000000000000000010000001101011},
    {OP_FCVTN, 0b00000000000000000011000001101011},
    {OP_FCVTP, 0b00000000000000000100000001101011},
    {OP_FCVTZ, 0b00000000000000000101000001101011},
    {OP_SCVTF, 0b00000000000000000110000001101011},
    {OP_UCVTF, 0b00000000000000000111000001101011},
    {OP_SSRGET, 0b00000000000000000000000000111011},
    {OP_FMADD, 0b00000000000000000100000001001011},
    {OP_FMSUB, 0b00000000000000000101000001001011},
    {OP_FNMADD, 0b00000000000000000110000001001011},
    {OP_FNMSUB, 0b00000000000000000111000001001011},
    {OP_SSRSET, 0b00000000000000000001000000111011},
    {OP_CSEL, 0b00000000000000000000000001110111},
    {OP_CTZ, 0b00000000000000000100000001100111},
    {OP_CLZ, 0b00000000000000000101000001100111},
    {OP_BCNT, 0b00000000000000000110000001100111},
    {OP_REV, 0b00000000000000000111000001100111},
    {OP_LB_PCR, 0b00000000000000000000000000111001},
    {OP_LH_PCR, 0b00000000000000000001000000111001},
    {OP_LW_PCR, 0b00000000000000000010000000111001},
    {OP_LD_PCR, 0b00000000000000000011000000111001},
    {OP_LBU_PCR, 0b00000000000000000100000000111001},
    {OP_LHU_PCR, 0b00000000000000000101000000111001},
    {OP_LWU_PCR, 0b00000000000000000110000000111001},
    {OP_SB_PCR, 0b00000000000000000000000001101001},
    {OP_SH_PCR, 0b00000000000000000001000001101001},
    {OP_SW_PCR, 0b00000000000000000010000001101001},
    {OP_SD_PCR, 0b00000000000000000011000001101001},
    {OP_B_EQ, 0b00000000000000000000000000100111},
    {OP_B_NE, 0b00000000000000000001000000100111},
    {OP_B_LT, 0b00000000000000000010000000100111},
    {OP_B_GE, 0b00000000000000000011000000100111},
    {OP_B_LTU, 0b00000000000000000100000000100111},
    {OP_B_GEU, 0b00000000000000000101000000100111},
    {OP_JR, 0b00000000000000000110000000100111},
    {OP_J, 0b00000000000000000000000000110111},
    {OP_B_Z, 0b00000000000000000001000000110111},
    {OP_B_NZ, 0b00000000000000000010000000110111},
};
static void EncodeBlock32_extract_CSEL(InstInfo *inst, uint32_t &bin)
{
    // inst->srcRType = MInst::Extract32(bin, 25, 2);
    bin = MInst::Deposit32(bin, 25, 2, inst->srcRType);
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src1 = MInst::Extract32(bin, 20, 5);
    bin = MInst::Deposit32(bin, 20, 5, inst->src1);
    // inst->src2 = MInst::Extract32(bin, 27, 5);
    bin = MInst::Deposit32(bin, 27, 5, inst->src2);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_CVT(InstInfo *inst, uint32_t &bin)
{
    // inst->dstType = MInst::Extract32(bin, 27, 5);
    bin = MInst::Deposit32(bin, 27, 5, inst->dstType);
    // inst->srcType = MInst::Extract32(bin, 25, 2);
    bin = MInst::Deposit32(bin, 25, 2, inst->srcType);
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_FENCE_D(InstInfo *inst, uint32_t &bin)
{
    // inst->predImm = MInst::Extract32(bin, 24, 4);
    bin = MInst::Deposit32(bin, 24, 4, inst->predImm);
    // inst->succImm = MInst::Extract32(bin, 20, 4);
    bin = MInst::Deposit32(bin, 20, 4, inst->succImm);
}
static void EncodeBlock32_extract_FENCE_I(InstInfo *inst, uint32_t &bin)
{
}
static void EncodeBlock32_extract_J(InstInfo *inst, uint32_t &bin)
{
    // inst->src2 = MInst::ExShift1(MInst::Deposit(MInst::Extract32(bin, 15, 17), 17, MInst::Sextract32(bin, 7, 5)));
    {
    auto tmp = MInst::ExShift1Right(inst->src2);
    bin = MInst::Deposit32(bin, 15, 17, tmp);
    bin = MInst::Deposit32(bin, 7, 5, tmp >> 17);
    }
}
static void EncodeBlock32_extract_JR(InstInfo *inst, uint32_t &bin)
{
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src2 = MInst::ExShift1(MInst::Deposit(MInst::Extract32(bin, 25, 7), 7, MInst::Sextract32(bin, 7, 5)));
    {
    auto tmp = MInst::ExShift1Right(inst->src2);
    bin = MInst::Deposit32(bin, 25, 7, tmp);
    bin = MInst::Deposit32(bin, 7, 5, tmp >> 7);
    }
}
static void EncodeBlock32_extract_LoadSymbol(InstInfo *inst, uint32_t &bin)
{
    // inst->src2 = MInst::Extract32(bin, 15, 17);
    bin = MInst::Deposit32(bin, 15, 17, inst->src2);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_MADD(InstInfo *inst, uint32_t &bin)
{
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src1 = MInst::Extract32(bin, 20, 5);
    bin = MInst::Deposit32(bin, 20, 5, inst->src1);
    // inst->src2 = MInst::Extract32(bin, 27, 5);
    bin = MInst::Deposit32(bin, 27, 5, inst->src2);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_SETC_TGT(InstInfo *inst, uint32_t &bin)
{
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
}
static void EncodeBlock32_extract_SETRET(InstInfo *inst, uint32_t &bin)
{
    // inst->src2 = MInst::Sextract32(bin, 12, 20);
    bin = MInst::Deposit32(bin, 12, 20, inst->src2);
}
static void EncodeBlock32_extract_SSRGET(InstInfo *inst, uint32_t &bin)
{
    // inst->src2 = MInst::Extract32(bin, 20, 12);
    bin = MInst::Deposit32(bin, 20, 12, inst->src2);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_SSRSET(InstInfo *inst, uint32_t &bin)
{
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src2 = MInst::Extract32(bin, 20, 12);
    bin = MInst::Deposit32(bin, 20, 12, inst->src2);
}
static void EncodeBlock32_extract_StoreSymbol(InstInfo *inst, uint32_t &bin)
{
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src2 = MInst::Deposit(MInst::Extract32(bin, 20, 12), 12, MInst::Sextract32(bin, 7, 5));
    bin = MInst::Deposit32(bin, 20, 12, inst->src2);
    bin = MInst::Deposit32(bin, 7, 5, inst->src2 >> 12);
}
static void EncodeBlock32_extract_acrc(InstInfo *inst, uint32_t &bin)
{
    // inst->requestType = MInst::Extract32(bin, 20, 4);
    bin = MInst::Deposit32(bin, 20, 4, inst->requestType);
}
static void EncodeBlock32_extract_acre(InstInfo *inst, uint32_t &bin)
{
    // inst->requestType = MInst::Extract32(bin, 20, 4);
    bin = MInst::Deposit32(bin, 20, 4, inst->requestType);
}
static void EncodeBlock32_extract_arith(InstInfo *inst, uint32_t &bin)
{
    // inst->shamt = MInst::Extract32(bin, 27, 5);
    bin = MInst::Deposit32(bin, 27, 5, inst->shamt);
    // inst->srcRType = MInst::Extract32(bin, 25, 2);
    bin = MInst::Deposit32(bin, 25, 2, inst->srcRType);
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src1 = MInst::Extract32(bin, 20, 5);
    bin = MInst::Deposit32(bin, 20, 5, inst->src1);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_arith_si(InstInfo *inst, uint32_t &bin)
{
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_arith_ui(InstInfo *inst, uint32_t &bin)
{
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src2 = MInst::Extract32(bin, 20, 12);
    bin = MInst::Deposit32(bin, 20, 12, inst->src2);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_atomic_ld(InstInfo *inst, uint32_t &bin)
{
    // inst->aq = MInst::Extract32(bin, 26, 1);
    bin = MInst::Deposit32(bin, 26, 1, inst->aq);
    // inst->rl = MInst::Extract32(bin, 25, 1);
    bin = MInst::Deposit32(bin, 25, 1, inst->rl);
    // inst->far = MInst::Extract32(bin, 27, 1);
    bin = MInst::Deposit32(bin, 27, 1, inst->far);
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src1 = MInst::Extract32(bin, 20, 5);
    bin = MInst::Deposit32(bin, 20, 5, inst->src1);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_atomic_st(InstInfo *inst, uint32_t &bin)
{
    // inst->rl = MInst::Extract32(bin, 25, 1);
    bin = MInst::Deposit32(bin, 25, 1, inst->rl);
    // inst->far = MInst::Extract32(bin, 27, 1);
    bin = MInst::Deposit32(bin, 27, 1, inst->far);
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src1 = MInst::Extract32(bin, 20, 5);
    bin = MInst::Deposit32(bin, 20, 5, inst->src1);
}
static void EncodeBlock32_extract_bo(InstInfo *inst, uint32_t &bin)
{
    // inst->imms = MInst::Extract32(bin, 26, 6);
    bin = MInst::Deposit32(bin, 26, 6, inst->imms);
    // inst->imml = MInst::Extract32(bin, 20, 6);
    bin = MInst::Deposit32(bin, 20, 6, inst->imml);
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_branch(InstInfo *inst, uint32_t &bin)
{
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src1 = MInst::Extract32(bin, 20, 5);
    bin = MInst::Deposit32(bin, 20, 5, inst->src1);
    // inst->src2 = MInst::ExShift1(MInst::Deposit(MInst::Extract32(bin, 25, 7), 7, MInst::Sextract32(bin, 7, 5)));
    {
    auto tmp = MInst::ExShift1Right(inst->src2);
    bin = MInst::Deposit32(bin, 25, 7, tmp);
    bin = MInst::Deposit32(bin, 7, 5, tmp >> 7);
    }
}
static void EncodeBlock32_extract_compare(InstInfo *inst, uint32_t &bin)
{
    // inst->srcRType = MInst::Extract32(bin, 25, 2);
    bin = MInst::Deposit32(bin, 25, 2, inst->srcRType);
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src1 = MInst::Extract32(bin, 20, 5);
    bin = MInst::Deposit32(bin, 20, 5, inst->src1);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_constant(InstInfo *inst, uint32_t &bin)
{
    // inst->src2 = MInst::ExShift12(MInst::Sextract32(bin, 12, 20));
    {
    auto tmp = MInst::ExShift12Right(inst->src2);
    bin = MInst::Deposit32(bin, 12, 20, tmp);
    }
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_exec_ctrl(InstInfo *inst, uint32_t &bin)
{
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
}
static void EncodeBlock32_extract_fmax_fmin(InstInfo *inst, uint32_t &bin)
{
    // inst->srcType = MInst::Extract32(bin, 25, 2);
    bin = MInst::Deposit32(bin, 25, 2, inst->srcType);
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src1 = MInst::Extract32(bin, 20, 5);
    bin = MInst::Deposit32(bin, 20, 5, inst->src1);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_fpa(InstInfo *inst, uint32_t &bin)
{
    // inst->srcType = MInst::Extract32(bin, 25, 2);
    bin = MInst::Deposit32(bin, 25, 2, inst->srcType);
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src1 = MInst::Extract32(bin, 20, 5);
    bin = MInst::Deposit32(bin, 20, 5, inst->src1);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_fpa2(InstInfo *inst, uint32_t &bin)
{
    // inst->srcType = MInst::Extract32(bin, 25, 2);
    bin = MInst::Deposit32(bin, 25, 2, inst->srcType);
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src1 = MInst::Extract32(bin, 20, 5);
    bin = MInst::Deposit32(bin, 20, 5, inst->src1);
    // inst->src2 = MInst::Extract32(bin, 27, 5);
    bin = MInst::Deposit32(bin, 27, 5, inst->src2);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_fpa3(InstInfo *inst, uint32_t &bin)
{
    // inst->srcType = MInst::Extract32(bin, 25, 2);
    bin = MInst::Deposit32(bin, 25, 2, inst->srcType);
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_fpc(InstInfo *inst, uint32_t &bin)
{
    // inst->srcType = MInst::Extract32(bin, 25, 2);
    bin = MInst::Deposit32(bin, 25, 2, inst->srcType);
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src1 = MInst::Extract32(bin, 20, 5);
    bin = MInst::Deposit32(bin, 20, 5, inst->src1);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_lr(InstInfo *inst, uint32_t &bin)
{
    // inst->aq = MInst::Extract32(bin, 26, 1);
    bin = MInst::Deposit32(bin, 26, 1, inst->aq);
    // inst->rl = MInst::Extract32(bin, 25, 1);
    bin = MInst::Deposit32(bin, 25, 1, inst->rl);
    // inst->far = MInst::Extract32(bin, 27, 1);
    bin = MInst::Deposit32(bin, 27, 1, inst->far);
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_max_min(InstInfo *inst, uint32_t &bin)
{
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src1 = MInst::Extract32(bin, 20, 5);
    bin = MInst::Deposit32(bin, 20, 5, inst->src1);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_multi(InstInfo *inst, uint32_t &bin)
{
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src1 = MInst::Extract32(bin, 20, 5);
    bin = MInst::Deposit32(bin, 20, 5, inst->src1);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_one_src(InstInfo *inst, uint32_t &bin)
{
    // inst->imms = MInst::Extract32(bin, 26, 6);
    bin = MInst::Deposit32(bin, 26, 6, inst->imms);
    // inst->imml = MInst::Extract32(bin, 20, 6);
    bin = MInst::Deposit32(bin, 20, 6, inst->imml);
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_sc(InstInfo *inst, uint32_t &bin)
{
    // inst->aq = MInst::Extract32(bin, 26, 1);
    bin = MInst::Deposit32(bin, 26, 1, inst->aq);
    // inst->rl = MInst::Extract32(bin, 25, 1);
    bin = MInst::Deposit32(bin, 25, 1, inst->rl);
    // inst->far = MInst::Extract32(bin, 27, 1);
    bin = MInst::Deposit32(bin, 27, 1, inst->far);
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src1 = MInst::Extract32(bin, 20, 5);
    bin = MInst::Deposit32(bin, 20, 5, inst->src1);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_setc(InstInfo *inst, uint32_t &bin)
{
    // inst->srcRType = MInst::Extract32(bin, 25, 2);
    bin = MInst::Deposit32(bin, 25, 2, inst->srcRType);
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src1 = MInst::Extract32(bin, 20, 5);
    bin = MInst::Deposit32(bin, 20, 5, inst->src1);
}
static void EncodeBlock32_extract_setc_si(InstInfo *inst, uint32_t &bin)
{
    // inst->shamt = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->shamt);
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src2 = MInst::Sextract32(bin, 20, 12);
    bin = MInst::Deposit32(bin, 20, 12, inst->src2);
}
static void EncodeBlock32_extract_setc_ui(InstInfo *inst, uint32_t &bin)
{
    // inst->shamt = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->shamt);
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src2 = MInst::Extract32(bin, 20, 12);
    bin = MInst::Deposit32(bin, 20, 12, inst->src2);
}
static void EncodeBlock32_extract_shift(InstInfo *inst, uint32_t &bin)
{
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src1 = MInst::Extract32(bin, 20, 5);
    bin = MInst::Deposit32(bin, 20, 5, inst->src1);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_shift_i(InstInfo *inst, uint32_t &bin)
{
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src2 = MInst::Extract32(bin, 20, 6);
    bin = MInst::Deposit32(bin, 20, 6, inst->src2);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_shift_i_w(InstInfo *inst, uint32_t &bin)
{
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src2 = MInst::Extract32(bin, 20, 5);
    bin = MInst::Deposit32(bin, 20, 5, inst->src2);
    // inst->dst0 = MInst::Extract32(bin, 7, 5);
    bin = MInst::Deposit32(bin, 7, 5, inst->dst0);
}
static void EncodeBlock32_extract_store(InstInfo *inst, uint32_t &bin)
{
    // inst->srcRType = MInst::Extract32(bin, 25, 2);
    bin = MInst::Deposit32(bin, 25, 2, inst->srcRType);
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src1 = MInst::Extract32(bin, 20, 5);
    bin = MInst::Deposit32(bin, 20, 5, inst->src1);
    // inst->src2 = MInst::Extract32(bin, 27, 5);
    bin = MInst::Deposit32(bin, 27, 5, inst->src2);
}
static void EncodeBlock32_extract_store_imm(InstInfo *inst, uint32_t &bin)
{
    // inst->src0 = MInst::Extract32(bin, 15, 5);
    bin = MInst::Deposit32(bin, 15, 5, inst->src0);
    // inst->src1 = MInst::Extract32(bin, 20, 5);
    bin = MInst::Deposit32(bin, 20, 5, inst->src1);
}
uint32_t EncodeBlock32(InstInfo* inst){
    uint32_t bin{0};
    auto it = op2bin_32.find(inst->opcode);
    if (it == op2bin_32.end()) {
        std::cerr << "Warning: inst->opcode is illegal: " << inst->opcode << std::endl;
        return 0ull;
    }
    bin = it->second;
    switch(inst->opcode) {
        case OP_ADD:
            EncodeBlock32_extract_arith(inst, bin);
            break;
        case OP_ADDI:
            EncodeBlock32_extract_arith_ui(inst, bin);
            break;
        case OP_ADDW:
            EncodeBlock32_extract_arith(inst, bin);
            break;
        case OP_ADDIW:
            EncodeBlock32_extract_arith_ui(inst, bin);
            break;
        case OP_CMP_EQ:
            EncodeBlock32_extract_compare(inst, bin);
            break;
        case OP_CMP_EQ_I:
            // inst->src2 = MInst::Sextract32(bin, 20, 12);
            bin = MInst::Deposit32(bin, 20, 12, inst->src2);
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_SETC_EQ:
            EncodeBlock32_extract_setc(inst, bin);
            break;
        case OP_SETC_EQ_I:
            EncodeBlock32_extract_setc_si(inst, bin);
            break;
        case OP_SUB:
            EncodeBlock32_extract_arith(inst, bin);
            break;
        case OP_SUBI:
            EncodeBlock32_extract_arith_ui(inst, bin);
            break;
        case OP_SUBW:
            EncodeBlock32_extract_arith(inst, bin);
            break;
        case OP_SUBIW:
            EncodeBlock32_extract_arith_ui(inst, bin);
            break;
        case OP_CMP_NE:
            EncodeBlock32_extract_compare(inst, bin);
            break;
        case OP_CMP_NE_I:
            // inst->src2 = MInst::Sextract32(bin, 20, 12);
            bin = MInst::Deposit32(bin, 20, 12, inst->src2);
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_SETC_NE:
            EncodeBlock32_extract_setc(inst, bin);
            break;
        case OP_SETC_NE_I:
            EncodeBlock32_extract_setc_si(inst, bin);
            break;
        case OP_AND:
            EncodeBlock32_extract_arith(inst, bin);
            break;
        case OP_ANDI:
            // inst->src2 = MInst::Sextract32(bin, 20, 12);
            bin = MInst::Deposit32(bin, 20, 12, inst->src2);
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_ANDW:
            EncodeBlock32_extract_arith(inst, bin);
            break;
        case OP_ANDIW:
            // inst->src2 = MInst::Sextract32(bin, 20, 12);
            bin = MInst::Deposit32(bin, 20, 12, inst->src2);
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_CMP_AND:
            EncodeBlock32_extract_compare(inst, bin);
            break;
        case OP_CMP_AND_I:
            // inst->src2 = MInst::Sextract32(bin, 20, 12);
            bin = MInst::Deposit32(bin, 20, 12, inst->src2);
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_SETC_AND:
            EncodeBlock32_extract_setc(inst, bin);
            break;
        case OP_SETC_AND_I:
            EncodeBlock32_extract_setc_si(inst, bin);
            break;
        case OP_OR:
            EncodeBlock32_extract_arith(inst, bin);
            break;
        case OP_ORI:
            // inst->src2 = MInst::Sextract32(bin, 20, 12);
            bin = MInst::Deposit32(bin, 20, 12, inst->src2);
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_ORW:
            EncodeBlock32_extract_arith(inst, bin);
            break;
        case OP_ORIW:
            // inst->src2 = MInst::Sextract32(bin, 20, 12);
            bin = MInst::Deposit32(bin, 20, 12, inst->src2);
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_CMP_OR:
            EncodeBlock32_extract_compare(inst, bin);
            break;
        case OP_CMP_OR_I:
            // inst->src2 = MInst::Sextract32(bin, 20, 12);
            bin = MInst::Deposit32(bin, 20, 12, inst->src2);
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_SETC_OR:
            EncodeBlock32_extract_setc(inst, bin);
            break;
        case OP_SETC_OR_I:
            EncodeBlock32_extract_setc_si(inst, bin);
            break;
        case OP_XOR:
            EncodeBlock32_extract_arith(inst, bin);
            break;
        case OP_XORI:
            // inst->src2 = MInst::Sextract32(bin, 20, 12);
            bin = MInst::Deposit32(bin, 20, 12, inst->src2);
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_XORW:
            EncodeBlock32_extract_arith(inst, bin);
            break;
        case OP_XORIW:
            // inst->src2 = MInst::Sextract32(bin, 20, 12);
            bin = MInst::Deposit32(bin, 20, 12, inst->src2);
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_CMP_LT:
            EncodeBlock32_extract_compare(inst, bin);
            break;
        case OP_CMP_LT_I:
            // inst->src2 = MInst::Sextract32(bin, 20, 12);
            bin = MInst::Deposit32(bin, 20, 12, inst->src2);
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_SETC_LT:
            EncodeBlock32_extract_setc(inst, bin);
            break;
        case OP_SETC_LT_I:
            EncodeBlock32_extract_setc_si(inst, bin);
            break;
        case OP_SRL:
            EncodeBlock32_extract_shift(inst, bin);
            break;
        case OP_SRLI:
            EncodeBlock32_extract_shift_i(inst, bin);
            break;
        case OP_SRLW:
            EncodeBlock32_extract_shift(inst, bin);
            break;
        case OP_SRLIW:
            EncodeBlock32_extract_shift_i_w(inst, bin);
            break;
        case OP_CMP_GE:
            EncodeBlock32_extract_compare(inst, bin);
            break;
        case OP_CMP_GE_I:
            // inst->src2 = MInst::Sextract32(bin, 20, 12);
            bin = MInst::Deposit32(bin, 20, 12, inst->src2);
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_SETC_GE:
            EncodeBlock32_extract_setc(inst, bin);
            break;
        case OP_SETC_GE_I:
            EncodeBlock32_extract_setc_si(inst, bin);
            break;
        case OP_SRA:
            EncodeBlock32_extract_shift(inst, bin);
            break;
        case OP_SRAI:
            EncodeBlock32_extract_shift_i(inst, bin);
            break;
        case OP_SRAW:
            EncodeBlock32_extract_shift(inst, bin);
            break;
        case OP_SRAIW:
            EncodeBlock32_extract_shift_i_w(inst, bin);
            break;
        case OP_CMP_LTU:
            EncodeBlock32_extract_compare(inst, bin);
            break;
        case OP_CMP_LTU_I:
            EncodeBlock32_extract_arith_ui(inst, bin);
            break;
        case OP_SETC_LTU:
            EncodeBlock32_extract_setc(inst, bin);
            break;
        case OP_SETC_LTU_I:
            EncodeBlock32_extract_setc_ui(inst, bin);
            break;
        case OP_SLL:
            EncodeBlock32_extract_shift(inst, bin);
            break;
        case OP_SLLI:
            EncodeBlock32_extract_shift_i(inst, bin);
            break;
        case OP_SLLW:
            EncodeBlock32_extract_shift(inst, bin);
            break;
        case OP_SLLIW:
            EncodeBlock32_extract_shift_i_w(inst, bin);
            break;
        case OP_CMP_GEU:
            EncodeBlock32_extract_compare(inst, bin);
            break;
        case OP_CMP_GEU_I:
            EncodeBlock32_extract_arith_ui(inst, bin);
            break;
        case OP_SETC_GEU:
            EncodeBlock32_extract_setc(inst, bin);
            break;
        case OP_SETC_GEU_I:
            EncodeBlock32_extract_setc_ui(inst, bin);
            break;
        case OP_SETRET:
            EncodeBlock32_extract_SETRET(inst, bin);
            break;
        case OP_ADDTPC:
            EncodeBlock32_extract_constant(inst, bin);
            break;
        case OP_LUI:
            EncodeBlock32_extract_constant(inst, bin);
            break;
        case OP_B_EQ:
            EncodeBlock32_extract_branch(inst, bin);
            break;
        case OP_B_NE:
            EncodeBlock32_extract_branch(inst, bin);
            break;
        case OP_B_LT:
            EncodeBlock32_extract_branch(inst, bin);
            break;
        case OP_B_GE:
            EncodeBlock32_extract_branch(inst, bin);
            break;
        case OP_B_LTU:
            EncodeBlock32_extract_branch(inst, bin);
            break;
        case OP_B_GEU:
            EncodeBlock32_extract_branch(inst, bin);
            break;
        case OP_JR:
            EncodeBlock32_extract_JR(inst, bin);
            break;
        case OP_J:
            EncodeBlock32_extract_J(inst, bin);
            break;
        case OP_B_Z:
            EncodeBlock32_extract_J(inst, bin);
            break;
        case OP_B_NZ:
            EncodeBlock32_extract_J(inst, bin);
            break;
        case OP_MUL:
            EncodeBlock32_extract_shift(inst, bin);
            break;
        case OP_MULU:
            EncodeBlock32_extract_shift(inst, bin);
            break;
        case OP_MULW:
            EncodeBlock32_extract_shift(inst, bin);
            break;
        case OP_MULUW:
            EncodeBlock32_extract_shift(inst, bin);
            break;
        case OP_MADD:
            EncodeBlock32_extract_MADD(inst, bin);
            break;
        case OP_MADDW:
            EncodeBlock32_extract_MADD(inst, bin);
            break;
        case OP_DIV:
            EncodeBlock32_extract_multi(inst, bin);
            break;
        case OP_DIVU:
            EncodeBlock32_extract_multi(inst, bin);
            break;
        case OP_DIVW:
            EncodeBlock32_extract_multi(inst, bin);
            break;
        case OP_DIVUW:
            EncodeBlock32_extract_multi(inst, bin);
            break;
        case OP_REM:
            EncodeBlock32_extract_multi(inst, bin);
            break;
        case OP_REMU:
            EncodeBlock32_extract_multi(inst, bin);
            break;
        case OP_REMW:
            EncodeBlock32_extract_multi(inst, bin);
            break;
        case OP_REMUW:
            EncodeBlock32_extract_multi(inst, bin);
            break;
        case OP_BXS:
            EncodeBlock32_extract_bo(inst, bin);
            break;
        case OP_BXU:
            EncodeBlock32_extract_bo(inst, bin);
            break;
        case OP_BIC:
            EncodeBlock32_extract_bo(inst, bin);
            break;
        case OP_BIS:
            EncodeBlock32_extract_bo(inst, bin);
            break;
        case OP_CTZ:
            EncodeBlock32_extract_one_src(inst, bin);
            break;
        case OP_CLZ:
            EncodeBlock32_extract_one_src(inst, bin);
            break;
        case OP_BCNT:
            EncodeBlock32_extract_one_src(inst, bin);
            break;
        case OP_REV:
            EncodeBlock32_extract_one_src(inst, bin);
            break;
        case OP_CSEL:
            EncodeBlock32_extract_CSEL(inst, bin);
            break;
        case OP_LB:
            EncodeBlock32_extract_arith(inst, bin);
            break;
        case OP_LBI:
            // inst->src2 = MInst::Sextract32(bin, 20, 12);
            bin = MInst::Deposit32(bin, 20, 12, inst->src2);
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_LB_PCR:
            EncodeBlock32_extract_LoadSymbol(inst, bin);
            break;
        case OP_SB:
            EncodeBlock32_extract_store(inst, bin);
            break;
        case OP_SBI:
            // inst->src2 = MInst::Deposit(MInst::Extract32(bin, 25, 7), 7, MInst::Sextract32(bin, 7, 5));
            bin = MInst::Deposit32(bin, 25, 7, inst->src2);
            bin = MInst::Deposit32(bin, 7, 5, inst->src2 >> 7);
            EncodeBlock32_extract_store_imm(inst, bin);
            break;
        case OP_SB_PCR:
            EncodeBlock32_extract_StoreSymbol(inst, bin);
            break;
        case OP_LH:
            EncodeBlock32_extract_arith(inst, bin);
            break;
        case OP_LHI:
            // inst->src2 = MInst::ExShift1(MInst::Sextract32(bin, 20, 12));
            {
            auto tmp = MInst::ExShift1Right(inst->src2);
            bin = MInst::Deposit32(bin, 20, 12, tmp);
            }
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_LHI_U:
            // inst->src2 = MInst::Sextract32(bin, 20, 12);
            bin = MInst::Deposit32(bin, 20, 12, inst->src2);
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_LH_PCR:
            EncodeBlock32_extract_LoadSymbol(inst, bin);
            break;
        case OP_SH:
            EncodeBlock32_extract_store(inst, bin);
            break;
        case OP_SHI:
            // inst->src2 = MInst::ExShift1(MInst::Deposit(MInst::Extract32(bin, 25, 7), 7, MInst::Sextract32(bin, 7, 5)));
            {
            auto tmp = MInst::ExShift1Right(inst->src2);
            bin = MInst::Deposit32(bin, 25, 7, tmp);
            bin = MInst::Deposit32(bin, 7, 5, tmp >> 7);
            }
            EncodeBlock32_extract_store_imm(inst, bin);
            break;
        case OP_SH_PCR:
            EncodeBlock32_extract_StoreSymbol(inst, bin);
            break;
        case OP_LW:
            EncodeBlock32_extract_arith(inst, bin);
            break;
        case OP_LWI:
            // inst->src2 = MInst::ExShift2(MInst::Sextract32(bin, 20, 12));
            {
            auto tmp = MInst::ExShift2Right(inst->src2);
            bin = MInst::Deposit32(bin, 20, 12, tmp);
            }
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_LWI_U:
            // inst->src2 = MInst::Sextract32(bin, 20, 12);
            bin = MInst::Deposit32(bin, 20, 12, inst->src2);
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_LW_PCR:
            EncodeBlock32_extract_LoadSymbol(inst, bin);
            break;
        case OP_SW:
            EncodeBlock32_extract_store(inst, bin);
            break;
        case OP_SWI:
            // inst->src2 = MInst::ExShift2(MInst::Deposit(MInst::Extract32(bin, 25, 7), 7, MInst::Sextract32(bin, 7, 5)));
            {
            auto tmp = MInst::ExShift2Right(inst->src2);
            bin = MInst::Deposit32(bin, 25, 7, tmp);
            bin = MInst::Deposit32(bin, 7, 5, tmp >> 7);
            }
            EncodeBlock32_extract_store_imm(inst, bin);
            break;
        case OP_SW_PCR:
            EncodeBlock32_extract_StoreSymbol(inst, bin);
            break;
        case OP_LD:
            EncodeBlock32_extract_arith(inst, bin);
            break;
        case OP_LDI:
            // inst->src2 = MInst::ExShift3(MInst::Sextract32(bin, 20, 12));
            {
            auto tmp = MInst::ExShift3Right(inst->src2);
            bin = MInst::Deposit32(bin, 20, 12, tmp);
            }
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_LDI_U:
            // inst->src2 = MInst::Sextract32(bin, 20, 12);
            bin = MInst::Deposit32(bin, 20, 12, inst->src2);
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_LD_PCR:
            EncodeBlock32_extract_LoadSymbol(inst, bin);
            break;
        case OP_SD:
            EncodeBlock32_extract_store(inst, bin);
            break;
        case OP_SDI:
            // inst->src2 = MInst::ExShift3(MInst::Deposit(MInst::Extract32(bin, 25, 7), 7, MInst::Sextract32(bin, 7, 5)));
            {
            auto tmp = MInst::ExShift3Right(inst->src2);
            bin = MInst::Deposit32(bin, 25, 7, tmp);
            bin = MInst::Deposit32(bin, 7, 5, tmp >> 7);
            }
            EncodeBlock32_extract_store_imm(inst, bin);
            break;
        case OP_SD_PCR:
            EncodeBlock32_extract_StoreSymbol(inst, bin);
            break;
        case OP_LBU:
            EncodeBlock32_extract_arith(inst, bin);
            break;
        case OP_LBUI:
            // inst->src2 = MInst::Sextract32(bin, 20, 12);
            bin = MInst::Deposit32(bin, 20, 12, inst->src2);
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_LBU_PCR:
            EncodeBlock32_extract_LoadSymbol(inst, bin);
            break;
        case OP_LHU:
            EncodeBlock32_extract_arith(inst, bin);
            break;
        case OP_LHUI:
            // inst->src2 = MInst::ExShift1(MInst::Sextract32(bin, 20, 12));
            {
            auto tmp = MInst::ExShift1Right(inst->src2);
            bin = MInst::Deposit32(bin, 20, 12, tmp);
            }
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_LHUI_U:
            // inst->src2 = MInst::Sextract32(bin, 20, 12);
            bin = MInst::Deposit32(bin, 20, 12, inst->src2);
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_LHU_PCR:
            EncodeBlock32_extract_LoadSymbol(inst, bin);
            break;
        case OP_SH_U:
            EncodeBlock32_extract_store(inst, bin);
            break;
        case OP_SHI_U:
            // inst->src2 = MInst::Deposit(MInst::Extract32(bin, 25, 7), 7, MInst::Sextract32(bin, 7, 5));
            bin = MInst::Deposit32(bin, 25, 7, inst->src2);
            bin = MInst::Deposit32(bin, 7, 5, inst->src2 >> 7);
            EncodeBlock32_extract_store_imm(inst, bin);
            break;
        case OP_LWU:
            EncodeBlock32_extract_arith(inst, bin);
            break;
        case OP_LWUI:
            // inst->src2 = MInst::ExShift2(MInst::Sextract32(bin, 20, 12));
            {
            auto tmp = MInst::ExShift2Right(inst->src2);
            bin = MInst::Deposit32(bin, 20, 12, tmp);
            }
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_LWUI_U:
            // inst->src2 = MInst::Sextract32(bin, 20, 12);
            bin = MInst::Deposit32(bin, 20, 12, inst->src2);
            EncodeBlock32_extract_arith_si(inst, bin);
            break;
        case OP_LWU_PCR:
            EncodeBlock32_extract_LoadSymbol(inst, bin);
            break;
        case OP_SW_U:
            EncodeBlock32_extract_store(inst, bin);
            break;
        case OP_SWI_U:
            // inst->src2 = MInst::Deposit(MInst::Extract32(bin, 25, 7), 7, MInst::Sextract32(bin, 7, 5));
            bin = MInst::Deposit32(bin, 25, 7, inst->src2);
            bin = MInst::Deposit32(bin, 7, 5, inst->src2 >> 7);
            EncodeBlock32_extract_store_imm(inst, bin);
            break;
        case OP_SD_U:
            EncodeBlock32_extract_store(inst, bin);
            break;
        case OP_SDI_U:
            // inst->src2 = MInst::Deposit(MInst::Extract32(bin, 25, 7), 7, MInst::Sextract32(bin, 7, 5));
            bin = MInst::Deposit32(bin, 25, 7, inst->src2);
            bin = MInst::Deposit32(bin, 7, 5, inst->src2 >> 7);
            EncodeBlock32_extract_store_imm(inst, bin);
            break;
        case OP_FENCE_I:
            EncodeBlock32_extract_FENCE_I(inst, bin);
            break;
        case OP_BSE:
            EncodeBlock32_extract_exec_ctrl(inst, bin);
            break;
        case OP_BWE:
            EncodeBlock32_extract_exec_ctrl(inst, bin);
            break;
        case OP_BWI:
            EncodeBlock32_extract_exec_ctrl(inst, bin);
            break;
        case OP_ASSERT:
            EncodeBlock32_extract_exec_ctrl(inst, bin);
            break;
        case OP_SETC_TGT:
            EncodeBlock32_extract_SETC_TGT(inst, bin);
            break;
        case OP_ACRC:
            EncodeBlock32_extract_acrc(inst, bin);
            break;
        case OP_ACRE:
            EncodeBlock32_extract_acre(inst, bin);
            break;
        case OP_MAX:
            EncodeBlock32_extract_max_min(inst, bin);
            break;
        case OP_MAXU:
            EncodeBlock32_extract_max_min(inst, bin);
            break;
        case OP_MIN:
            EncodeBlock32_extract_max_min(inst, bin);
            break;
        case OP_MINU:
            EncodeBlock32_extract_max_min(inst, bin);
            break;
        case OP_FABS:
            EncodeBlock32_extract_fpa3(inst, bin);
            break;
        case OP_FSQRT:
            EncodeBlock32_extract_fpa3(inst, bin);
            break;
        case OP_FEXP:
            EncodeBlock32_extract_fpa3(inst, bin);
            break;
        case OP_FRECIP:
            EncodeBlock32_extract_fpa3(inst, bin);
            break;
        case OP_FMAX:
            EncodeBlock32_extract_fmax_fmin(inst, bin);
            break;
        case OP_FMIN:
            EncodeBlock32_extract_fmax_fmin(inst, bin);
            break;
        case OP_FADD:
            EncodeBlock32_extract_fpa(inst, bin);
            break;
        case OP_FSUB:
            EncodeBlock32_extract_fpa(inst, bin);
            break;
        case OP_FMUL:
            EncodeBlock32_extract_fpa(inst, bin);
            break;
        case OP_FDIV:
            EncodeBlock32_extract_fpa(inst, bin);
            break;
        case OP_FEQ:
            EncodeBlock32_extract_fpc(inst, bin);
            break;
        case OP_FNE:
            EncodeBlock32_extract_fpc(inst, bin);
            break;
        case OP_FLT:
            EncodeBlock32_extract_fpc(inst, bin);
            break;
        case OP_FGE:
            EncodeBlock32_extract_fpc(inst, bin);
            break;
        case OP_FEQS:
            EncodeBlock32_extract_fpc(inst, bin);
            break;
        case OP_FNES:
            EncodeBlock32_extract_fpc(inst, bin);
            break;
        case OP_FLTS:
            EncodeBlock32_extract_fpc(inst, bin);
            break;
        case OP_FGES:
            EncodeBlock32_extract_fpc(inst, bin);
            break;
        case OP_SW_ADD:
            EncodeBlock32_extract_atomic_st(inst, bin);
            break;
        case OP_SW_AND:
            EncodeBlock32_extract_atomic_st(inst, bin);
            break;
        case OP_SW_OR:
            EncodeBlock32_extract_atomic_st(inst, bin);
            break;
        case OP_SW_XOR:
            EncodeBlock32_extract_atomic_st(inst, bin);
            break;
        case OP_SW_SMAX:
            EncodeBlock32_extract_atomic_st(inst, bin);
            break;
        case OP_SW_SMIN:
            EncodeBlock32_extract_atomic_st(inst, bin);
            break;
        case OP_SW_UMAX:
            EncodeBlock32_extract_atomic_st(inst, bin);
            break;
        case OP_SW_UMIN:
            EncodeBlock32_extract_atomic_st(inst, bin);
            break;
        case OP_SD_ADD:
            EncodeBlock32_extract_atomic_st(inst, bin);
            break;
        case OP_SD_AND:
            EncodeBlock32_extract_atomic_st(inst, bin);
            break;
        case OP_SD_OR:
            EncodeBlock32_extract_atomic_st(inst, bin);
            break;
        case OP_SD_XOR:
            EncodeBlock32_extract_atomic_st(inst, bin);
            break;
        case OP_SD_SMAX:
            EncodeBlock32_extract_atomic_st(inst, bin);
            break;
        case OP_SD_SMIN:
            EncodeBlock32_extract_atomic_st(inst, bin);
            break;
        case OP_SD_UMAX:
            EncodeBlock32_extract_atomic_st(inst, bin);
            break;
        case OP_SD_UMIN:
            EncodeBlock32_extract_atomic_st(inst, bin);
            break;
        case OP_LR_B:
            EncodeBlock32_extract_lr(inst, bin);
            break;
        case OP_LR_H:
            EncodeBlock32_extract_lr(inst, bin);
            break;
        case OP_LR_W:
            EncodeBlock32_extract_lr(inst, bin);
            break;
        case OP_LR_D:
            EncodeBlock32_extract_lr(inst, bin);
            break;
        case OP_FENCE_D:
            EncodeBlock32_extract_FENCE_D(inst, bin);
            break;
        case OP_SC_B:
            EncodeBlock32_extract_sc(inst, bin);
            break;
        case OP_SC_H:
            EncodeBlock32_extract_sc(inst, bin);
            break;
        case OP_SC_W:
            EncodeBlock32_extract_sc(inst, bin);
            break;
        case OP_SC_D:
            EncodeBlock32_extract_sc(inst, bin);
            break;
        case OP_LW_ADD:
            EncodeBlock32_extract_atomic_ld(inst, bin);
            break;
        case OP_LW_AND:
            EncodeBlock32_extract_atomic_ld(inst, bin);
            break;
        case OP_LW_OR:
            EncodeBlock32_extract_atomic_ld(inst, bin);
            break;
        case OP_LW_XOR:
            EncodeBlock32_extract_atomic_ld(inst, bin);
            break;
        case OP_LW_SMAX:
            EncodeBlock32_extract_atomic_ld(inst, bin);
            break;
        case OP_LW_SMIN:
            EncodeBlock32_extract_atomic_ld(inst, bin);
            break;
        case OP_LW_UMAX:
            EncodeBlock32_extract_atomic_ld(inst, bin);
            break;
        case OP_LW_UMIN:
            EncodeBlock32_extract_atomic_ld(inst, bin);
            break;
        case OP_LD_ADD:
            EncodeBlock32_extract_atomic_ld(inst, bin);
            break;
        case OP_LD_AND:
            EncodeBlock32_extract_atomic_ld(inst, bin);
            break;
        case OP_LD_OR:
            EncodeBlock32_extract_atomic_ld(inst, bin);
            break;
        case OP_LD_XOR:
            EncodeBlock32_extract_atomic_ld(inst, bin);
            break;
        case OP_LD_SMAX:
            EncodeBlock32_extract_atomic_ld(inst, bin);
            break;
        case OP_LD_SMIN:
            EncodeBlock32_extract_atomic_ld(inst, bin);
            break;
        case OP_LD_UMAX:
            EncodeBlock32_extract_atomic_ld(inst, bin);
            break;
        case OP_LD_UMIN:
            EncodeBlock32_extract_atomic_ld(inst, bin);
            break;
        case OP_SWAPB:
            EncodeBlock32_extract_sc(inst, bin);
            break;
        case OP_SWAPH:
            EncodeBlock32_extract_sc(inst, bin);
            break;
        case OP_SWAPW:
            EncodeBlock32_extract_sc(inst, bin);
            break;
        case OP_SWAPD:
            EncodeBlock32_extract_sc(inst, bin);
            break;
        case OP_FCVT:
            EncodeBlock32_extract_CVT(inst, bin);
            break;
        case OP_FCVTA:
            EncodeBlock32_extract_CVT(inst, bin);
            break;
        case OP_FCVTM:
            EncodeBlock32_extract_CVT(inst, bin);
            break;
        case OP_FCVTN:
            EncodeBlock32_extract_CVT(inst, bin);
            break;
        case OP_FCVTP:
            EncodeBlock32_extract_CVT(inst, bin);
            break;
        case OP_FCVTZ:
            EncodeBlock32_extract_CVT(inst, bin);
            break;
        case OP_SCVTF:
            EncodeBlock32_extract_CVT(inst, bin);
            break;
        case OP_UCVTF:
            EncodeBlock32_extract_CVT(inst, bin);
            break;
        case OP_SSRGET:
            EncodeBlock32_extract_SSRGET(inst, bin);
            break;
        case OP_FMADD:
            EncodeBlock32_extract_fpa2(inst, bin);
            break;
        case OP_FMSUB:
            EncodeBlock32_extract_fpa2(inst, bin);
            break;
        case OP_FNMADD:
            EncodeBlock32_extract_fpa2(inst, bin);
            break;
        case OP_FNMSUB:
            EncodeBlock32_extract_fpa2(inst, bin);
            break;
        case OP_SSRSET:
            EncodeBlock32_extract_SSRSET(inst, bin);
            break;
        default:
            std::cerr << "Warning: Not match any encoding func of inst->opcode: " << inst->opcode << std::endl;
            break;
    }
    return bin;
}
} // namespace JCore