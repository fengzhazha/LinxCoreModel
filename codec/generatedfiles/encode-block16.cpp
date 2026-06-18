#include "Instruction.h"
#include <unordered_map>

namespace JCore {
std::unordered_map<Opcode, uint16_t> op2bin_16 = {
    {OP_C_MOVR, 0b0000000000000110},
    {OP_C_MOVI, 0b0000000000010110},
    {OP_C_SETRET, 0b0101000000010110},
    {OP_C_ADD, 0b0000000000001000},
    {OP_C_SUB, 0b0000000000011000},
    {OP_C_AND, 0b0000000000101000},
    {OP_C_OR, 0b0000000000111000},
    {OP_C_SETC_EQ, 0b0000000000100110},
    {OP_C_SETC_NE, 0b0000000000110110},
    {OP_C_LWI, 0b0000000000001010},
    {OP_C_LDI, 0b0000000000011010},
    {OP_C_SWI, 0b0000000000101010},
    {OP_C_SDI, 0b0000000000111010},
    {OP_C_ADDI, 0b0000000000001100},
    {OP_C_SETC_TGT, 0b0000000000011100},
    {OP_C_SEXT_B, 0b0100000000011100},
    {OP_C_SEXT_H, 0b0100100000011100},
    {OP_C_SEXT_W, 0b0101000000011100},
    {OP_C_ZEXT_B, 0b0101100000011100},
    {OP_C_ZEXT_H, 0b0110000000011100},
    {OP_C_ZEXT_W, 0b0110100000011100},
    {OP_C_CMP_EQ_I, 0b0000000000101100},
    {OP_C_CMP_NE_I, 0b0000100000101100},
    {OP_C_SLLI, 0b0001000000101100},
    {OP_C_SRLI, 0b0001100000101100},
    {OP_C_SSRGET, 0b1000000000101100},
};
static void EncodeBlock16_extract_C_ARITH(InstInfo *inst, uint16_t &bin)
{
    // inst->src1 = MInst::Extract32(bin, 11, 5);
    bin = MInst::Deposit32(bin, 11, 5, inst->src1);
    // inst->src0 = MInst::Extract32(bin, 6, 5);
    bin = MInst::Deposit32(bin, 6, 5, inst->src0);
}
static void EncodeBlock16_extract_C_CMP_S(InstInfo *inst, uint16_t &bin)
{
    // inst->src2 = MInst::Sextract32(bin, 6, 5);
    bin = MInst::Deposit32(bin, 6, 5, inst->src2);
}
static void EncodeBlock16_extract_C_CMP_U(InstInfo *inst, uint16_t &bin)
{
    // inst->src2 = MInst::Extract32(bin, 6, 5);
    bin = MInst::Deposit32(bin, 6, 5, inst->src2);
}
static void EncodeBlock16_extract_C_EXT(InstInfo *inst, uint16_t &bin)
{
    // inst->src0 = MInst::Extract32(bin, 6, 5);
    bin = MInst::Deposit32(bin, 6, 5, inst->src0);
}
static void EncodeBlock16_extract_C_LD_SD(InstInfo *inst, uint16_t &bin)
{
}
static void EncodeBlock16_extract_C_MOVI(InstInfo *inst, uint16_t &bin)
{
    // inst->dst0 = MInst::Extract32(bin, 11, 5);
    bin = MInst::Deposit32(bin, 11, 5, inst->dst0);
    // inst->src2 = MInst::Sextract32(bin, 6, 5);
    bin = MInst::Deposit32(bin, 6, 5, inst->src2);
}
static void EncodeBlock16_extract_C_MOVR(InstInfo *inst, uint16_t &bin)
{
    // inst->dst0 = MInst::Extract32(bin, 11, 5);
    bin = MInst::Deposit32(bin, 11, 5, inst->dst0);
    // inst->src0 = MInst::Extract32(bin, 6, 5);
    bin = MInst::Deposit32(bin, 6, 5, inst->src0);
}
static void EncodeBlock16_extract_C_SETRET(InstInfo *inst, uint16_t &bin)
{
    // inst->src2 = MInst::Extract32(bin, 6, 5);
    bin = MInst::Deposit32(bin, 6, 5, inst->src2);
}
uint16_t EncodeBlock16(InstInfo* inst){
    uint16_t bin{0};
    auto it = op2bin_16.find(inst->opcode);
    if (it == op2bin_16.end()) {
        std::cerr << "Warning: inst->opcode is illegal: " << inst->opcode << std::endl;
        return 0ull;
    }
    bin = it->second;
    switch(inst->opcode) {
        case OP_C_MOVR:
            EncodeBlock16_extract_C_MOVR(inst, bin);
            break;
        case OP_C_ADD:
            EncodeBlock16_extract_C_ARITH(inst, bin);
            break;
        case OP_C_LWI:
            // inst->src2 = MInst::Sextract32(bin, 11, 5);
            bin = MInst::Deposit32(bin, 11, 5, inst->src2);
            // inst->src0 = MInst::Extract32(bin, 6, 5);
            bin = MInst::Deposit32(bin, 6, 5, inst->src0);
            EncodeBlock16_extract_C_LD_SD(inst, bin);
            break;
        case OP_C_ADDI:
            // inst->src0 = MInst::Extract32(bin, 6, 5);
            bin = MInst::Deposit32(bin, 6, 5, inst->src0);
            // inst->src2 = MInst::Sextract32(bin, 11, 5);
            bin = MInst::Deposit32(bin, 11, 5, inst->src2);
            EncodeBlock16_extract_C_LD_SD(inst, bin);
            break;
        case OP_C_MOVI:
            EncodeBlock16_extract_C_MOVI(inst, bin);
            break;
        case OP_C_SETRET:
            EncodeBlock16_extract_C_MOVI(inst, bin);
            EncodeBlock16_extract_C_SETRET(inst, bin);
            break;
        case OP_C_SUB:
            EncodeBlock16_extract_C_ARITH(inst, bin);
            break;
        case OP_C_LDI:
            // inst->src2 = MInst::Sextract32(bin, 11, 5);
            bin = MInst::Deposit32(bin, 11, 5, inst->src2);
            // inst->src0 = MInst::Extract32(bin, 6, 5);
            bin = MInst::Deposit32(bin, 6, 5, inst->src0);
            EncodeBlock16_extract_C_LD_SD(inst, bin);
            break;
        case OP_C_SETC_TGT:
            EncodeBlock16_extract_C_EXT(inst, bin);
            break;
        case OP_C_SEXT_B:
            EncodeBlock16_extract_C_EXT(inst, bin);
            break;
        case OP_C_SEXT_H:
            EncodeBlock16_extract_C_EXT(inst, bin);
            break;
        case OP_C_SEXT_W:
            EncodeBlock16_extract_C_EXT(inst, bin);
            break;
        case OP_C_ZEXT_B:
            EncodeBlock16_extract_C_EXT(inst, bin);
            break;
        case OP_C_ZEXT_H:
            EncodeBlock16_extract_C_EXT(inst, bin);
            break;
        case OP_C_ZEXT_W:
            EncodeBlock16_extract_C_EXT(inst, bin);
            break;
        case OP_C_SETC_EQ:
            EncodeBlock16_extract_C_ARITH(inst, bin);
            break;
        case OP_C_AND:
            EncodeBlock16_extract_C_ARITH(inst, bin);
            break;
        case OP_C_SWI:
            // inst->src2 = MInst::Sextract32(bin, 11, 5);
            bin = MInst::Deposit32(bin, 11, 5, inst->src2);
            // inst->src1 = MInst::Extract32(bin, 6, 5);
            bin = MInst::Deposit32(bin, 6, 5, inst->src1);
            EncodeBlock16_extract_C_LD_SD(inst, bin);
            break;
        case OP_C_CMP_EQ_I:
            EncodeBlock16_extract_C_CMP_S(inst, bin);
            break;
        case OP_C_CMP_NE_I:
            EncodeBlock16_extract_C_CMP_S(inst, bin);
            break;
        case OP_C_SLLI:
            EncodeBlock16_extract_C_CMP_U(inst, bin);
            break;
        case OP_C_SRLI:
            EncodeBlock16_extract_C_CMP_U(inst, bin);
            break;
        case OP_C_SSRGET:
            EncodeBlock16_extract_C_CMP_U(inst, bin);
            break;
        case OP_C_SETC_NE:
            EncodeBlock16_extract_C_ARITH(inst, bin);
            break;
        case OP_C_OR:
            EncodeBlock16_extract_C_ARITH(inst, bin);
            break;
        case OP_C_SDI:
            // inst->src2 = MInst::Sextract32(bin, 11, 5);
            bin = MInst::Deposit32(bin, 11, 5, inst->src2);
            // inst->src1 = MInst::Extract32(bin, 6, 5);
            bin = MInst::Deposit32(bin, 6, 5, inst->src1);
            EncodeBlock16_extract_C_LD_SD(inst, bin);
            break;
        default:
            std::cerr << "Warning: Not match any encoding func of inst->opcode: " << inst->opcode << std::endl;
            break;
    }
    return bin;
}
} // namespace JCore