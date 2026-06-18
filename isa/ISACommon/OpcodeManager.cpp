#include "OpcodeManager.h"

#include "utils/error.h"

namespace JCore {
OpcodeManager::OpcodeManager()
{
    RegisterOpcode();
    registered.clear();
}

void OpcodeManager::RegisterInfo(OpcodeInfo info)
{
    ASSERT(info.opcode < Opcode::OP_NUMBER);
    // ensure that the assembly for each opcode is different. assert(strToEnum_.count(str) == 0);
    // assert(strToEnum_.count(str) == 0);
    assert(registered.count(info.opcode) == 0);
    registered.emplace(info.opcode);
    strToEnum_.emplace(info.str, info.opcode);
    opcodeInfos_[static_cast<int>(info.opcode)] = info;
}

void OpcodeManager::RegisterBlockSplitOpcode()
{
    RegisterInfo(OpcodeInfo(Opcode::OP_BSTOP, "BSTOP", InstGroup::BLOCK_SPLIT));
    RegisterInfo(OpcodeInfo(Opcode::OP_BSTART, "BSTART", InstGroup::BLOCK_SPLIT));
    RegisterInfo(OpcodeInfo(Opcode::OP_BSTART_DIRECT, "BSTART", InstGroup::BLOCK_SPLIT));
    RegisterInfo(OpcodeInfo(Opcode::OP_BSTART_COND, "BSTART", InstGroup::BLOCK_SPLIT));
    RegisterInfo(OpcodeInfo(Opcode::OP_EBREAK, "EBREAK", InstGroup::BLOCK_SPLIT));
    RegisterInfo(OpcodeInfo(Opcode::OP_XB, "XB", InstGroup::BLOCK_SPLIT));
    RegisterInfo(OpcodeInfo(Opcode::OP_MCOPY, "MCOPY", InstGroup::BLOCK_SPLIT));
    RegisterInfo(OpcodeInfo(Opcode::OP_MSET, "MSET", InstGroup::BLOCK_SPLIT));
    RegisterInfo(OpcodeInfo(Opcode::OP_FENTRY, "FENTRY", InstGroup::BLOCK_SPLIT));
    RegisterInfo(OpcodeInfo(Opcode::OP_FEXIT, "FEXIT", InstGroup::BLOCK_SPLIT));
    RegisterInfo(OpcodeInfo(Opcode::OP_FRET_RA, "FRET_RA", InstGroup::BLOCK_SPLIT));
    RegisterInfo(OpcodeInfo(Opcode::OP_FRET_STK, "FRET_STK", InstGroup::BLOCK_SPLIT));

    RegisterInfo(OpcodeInfo(Opcode::OP_B_TEXT, "B.TEXT", InstGroup::BLOCK_OFFSET));
    RegisterInfo(OpcodeInfo(Opcode::OP_B_IOR, "B.IOR", InstGroup::BLOCK_IO));
    RegisterInfo(OpcodeInfo(Opcode::OP_B_IOD, "B.IOD", InstGroup::BLOCK_IO));
    RegisterInfo(OpcodeInfo(Opcode::OP_B_IOT, "B.IOT", InstGroup::BLOCK_IO));

    RegisterInfo(OpcodeInfo(Opcode::OP_B_DATR, "B.DATR", InstGroup::BLOCK_ATTR));
    RegisterInfo(OpcodeInfo(Opcode::OP_B_CATR, "B.CATR", InstGroup::BLOCK_ATTR));
    RegisterInfo(OpcodeInfo(Opcode::OP_B_HINT_PREFETCH, "B.HINT", InstGroup::BLOCK_HINT));
    RegisterInfo(OpcodeInfo(Opcode::OP_B_HINT_TRACE, "B.HINT", InstGroup::BLOCK_SPLIT));

    RegisterInfo(OpcodeInfo(Opcode::OP_B_DIM, "B.DIM", InstGroup::BLOCK_ARGUMENT));
    RegisterInfo(OpcodeInfo(Opcode::OP_B_DIMI, "B.DIMI", InstGroup::BLOCK_ARGUMENT));
}

void OpcodeManager::RegisterArithmeticOpcode()
{
    // Group: Arithmetic Operation 64bit
    RegisterInfo(OpcodeInfo(Opcode::OP_ADD, "add", InstGroup::ARITHMETIC, true));
    RegisterInfo(OpcodeInfo(Opcode::OP_SUB, "sub", InstGroup::ARITHMETIC, true));
    RegisterInfo(OpcodeInfo(Opcode::OP_AND, "and", InstGroup::ARITHMETIC, true));
    RegisterInfo(OpcodeInfo(Opcode::OP_OR, "or", InstGroup::ARITHMETIC, true));
    RegisterInfo(OpcodeInfo(Opcode::OP_XOR, "xor", InstGroup::ARITHMETIC, true));
    RegisterInfo(OpcodeInfo(Opcode::OP_SRL, "srl", InstGroup::ARITHMETIC));
    RegisterInfo(OpcodeInfo(Opcode::OP_SRA, "sra", InstGroup::ARITHMETIC));
    RegisterInfo(OpcodeInfo(Opcode::OP_SLL, "sll", InstGroup::ARITHMETIC));

    RegisterInfo(OpcodeInfo(Opcode::OP_ADDI, "addi", InstGroup::ARITHMETIC));
    RegisterInfo(OpcodeInfo(Opcode::OP_SUBI, "subi", InstGroup::ARITHMETIC));
    RegisterInfo(OpcodeInfo(Opcode::OP_ANDI, "andi", InstGroup::ARITHMETIC));
    RegisterInfo(OpcodeInfo(Opcode::OP_ORI, "ori", InstGroup::ARITHMETIC));
    RegisterInfo(OpcodeInfo(Opcode::OP_XORI, "xori", InstGroup::ARITHMETIC));
    RegisterInfo(OpcodeInfo(Opcode::OP_SRLI, "srli", InstGroup::ARITHMETIC));
    RegisterInfo(OpcodeInfo(Opcode::OP_SRAI, "srai", InstGroup::ARITHMETIC));
    RegisterInfo(OpcodeInfo(Opcode::OP_SLLI, "slli", InstGroup::ARITHMETIC));
    // Group: Arithmetic Operation 32bit
    RegisterInfo(OpcodeInfo(Opcode::OP_ADDW, "addw", InstGroup::ARITHMETIC, true));
    RegisterInfo(OpcodeInfo(Opcode::OP_SUBW, "subw", InstGroup::ARITHMETIC, true));
    RegisterInfo(OpcodeInfo(Opcode::OP_ANDW, "andw", InstGroup::ARITHMETIC, true));
    RegisterInfo(OpcodeInfo(Opcode::OP_ORW, "orw", InstGroup::ARITHMETIC, true));
    RegisterInfo(OpcodeInfo(Opcode::OP_XORW, "xorw", InstGroup::ARITHMETIC, true));
    RegisterInfo(OpcodeInfo(Opcode::OP_SRLW, "srlw", InstGroup::ARITHMETIC));
    RegisterInfo(OpcodeInfo(Opcode::OP_SRAW, "sraw", InstGroup::ARITHMETIC));
    RegisterInfo(OpcodeInfo(Opcode::OP_SLLW, "sllw", InstGroup::ARITHMETIC));
    RegisterInfo(OpcodeInfo(Opcode::OP_ADDIW, "addiw", InstGroup::ARITHMETIC));
    RegisterInfo(OpcodeInfo(Opcode::OP_SUBIW, "subiw", InstGroup::ARITHMETIC));
    RegisterInfo(OpcodeInfo(Opcode::OP_ANDIW, "andiw", InstGroup::ARITHMETIC));
    RegisterInfo(OpcodeInfo(Opcode::OP_ORIW, "oriw", InstGroup::ARITHMETIC));
    RegisterInfo(OpcodeInfo(Opcode::OP_XORIW, "xoriw", InstGroup::ARITHMETIC));
    RegisterInfo(OpcodeInfo(Opcode::OP_SRLIW, "srliw", InstGroup::ARITHMETIC));
    RegisterInfo(OpcodeInfo(Opcode::OP_SRAIW, "sraiw", InstGroup::ARITHMETIC));
    RegisterInfo(OpcodeInfo(Opcode::OP_SLLIW, "slliw", InstGroup::ARITHMETIC));

    RegisterInfo(OpcodeInfo(Opcode::OP_ADDLI, "addli", InstGroup::IMMEDIATE));
}

void OpcodeManager::RegisterArithmeticFPOpcode()
{
    // Group: Floating-point Arithmetic
    RegisterInfo(OpcodeInfo(Opcode::OP_FADD, "fadd", InstGroup::ARITHMETIC_FP));
    RegisterInfo(OpcodeInfo(Opcode::OP_FSUB, "fsub", InstGroup::ARITHMETIC_FP));
    RegisterInfo(OpcodeInfo(Opcode::OP_FMUL, "fmul", InstGroup::ARITHMETIC_FP));
    RegisterInfo(OpcodeInfo(Opcode::OP_FDIV, "fdiv", InstGroup::ARITHMETIC_FP));
    RegisterInfo(OpcodeInfo(Opcode::OP_FMADD, "fmadd", InstGroup::ARITHMETIC_FP));
    RegisterInfo(OpcodeInfo(Opcode::OP_FMSUB, "fmsub", InstGroup::ARITHMETIC_FP));
    RegisterInfo(OpcodeInfo(Opcode::OP_FNMADD, "fnmadd", InstGroup::ARITHMETIC_FP));
    RegisterInfo(OpcodeInfo(Opcode::OP_FNMSUB, "fnmsub", InstGroup::ARITHMETIC_FP));

    // Group: Floating-point Arithmetic
    RegisterInfo(OpcodeInfo(Opcode::OP_FABS, "fabs", InstGroup::ARITHMETIC_FP));
    RegisterInfo(OpcodeInfo(Opcode::OP_FSQRT, "fsqrt", InstGroup::ARITHMETIC_FP));
    RegisterInfo(OpcodeInfo(Opcode::OP_FRECIP, "frecip", InstGroup::ARITHMETIC_FP));
    RegisterInfo(OpcodeInfo(Opcode::OP_FEXP, "fexp", InstGroup::ARITHMETIC_FP));
    RegisterInfo(OpcodeInfo(Opcode::OP_FCLASS, "fclass", InstGroup::ARITHMETIC_FP));
}

void OpcodeManager::RegisterCompareOpcode()
{
    RegisterInfo(OpcodeInfo(Opcode::OP_CMP_EQ, "cmp.eq", InstGroup::COMPARE));
    RegisterInfo(OpcodeInfo(Opcode::OP_CMP_NE, "cmp.ne", InstGroup::COMPARE));
    RegisterInfo(OpcodeInfo(Opcode::OP_CMP_AND, "cmp.and", InstGroup::COMPARE));
    RegisterInfo(OpcodeInfo(Opcode::OP_CMP_OR, "cmp.or", InstGroup::COMPARE));
    RegisterInfo(OpcodeInfo(Opcode::OP_CMP_LT, "cmp.lt", InstGroup::COMPARE));
    RegisterInfo(OpcodeInfo(Opcode::OP_CMP_GE, "cmp.ge", InstGroup::COMPARE));
    RegisterInfo(OpcodeInfo(Opcode::OP_CMP_LTU, "cmp.ltu", InstGroup::COMPARE));
    RegisterInfo(OpcodeInfo(Opcode::OP_CMP_GEU, "cmp.geu", InstGroup::COMPARE));
    RegisterInfo(OpcodeInfo(Opcode::OP_CMP_EQI, "cmp.eqi", InstGroup::COMPARE));
    RegisterInfo(OpcodeInfo(Opcode::OP_CMP_NEI, "cmp.nei", InstGroup::COMPARE));
    RegisterInfo(OpcodeInfo(Opcode::OP_CMP_ANDI, "cmp.andi", InstGroup::COMPARE));
    RegisterInfo(OpcodeInfo(Opcode::OP_CMP_ORI, "cmp.ori", InstGroup::COMPARE));
    RegisterInfo(OpcodeInfo(Opcode::OP_CMP_LTI, "cmp.lti", InstGroup::COMPARE));
    RegisterInfo(OpcodeInfo(Opcode::OP_CMP_GEI, "cmp.gei", InstGroup::COMPARE));
    RegisterInfo(OpcodeInfo(Opcode::OP_CMP_LTUI, "cmp.ltui", InstGroup::COMPARE));
    RegisterInfo(OpcodeInfo(Opcode::OP_CMP_GEUI, "cmp.geui", InstGroup::COMPARE));
}

void OpcodeManager::RegisterCompareFPOpcode()
{
    // Group: Floating-point Compare
    RegisterInfo(OpcodeInfo(Opcode::OP_FEQ, "feq", InstGroup::COMPARE_FP));
    RegisterInfo(OpcodeInfo(Opcode::OP_FNE, "fne", InstGroup::COMPARE_FP));
    RegisterInfo(OpcodeInfo(Opcode::OP_FLT, "flt", InstGroup::COMPARE_FP));
    RegisterInfo(OpcodeInfo(Opcode::OP_FGE, "fge", InstGroup::COMPARE_FP));
    RegisterInfo(OpcodeInfo(Opcode::OP_FEQS, "feqs", InstGroup::COMPARE_FP));
    RegisterInfo(OpcodeInfo(Opcode::OP_FNES, "fnes", InstGroup::COMPARE_FP));
    RegisterInfo(OpcodeInfo(Opcode::OP_FLTS, "flts", InstGroup::COMPARE_FP));
    RegisterInfo(OpcodeInfo(Opcode::OP_FGES, "fges", InstGroup::COMPARE_FP));
}

void OpcodeManager::RegisterSetcOpcode()
{
    // Group: Set Commit Argument
    RegisterInfo(OpcodeInfo(Opcode::OP_SETC_EQ, "setc.eq", InstGroup::SETC));
    RegisterInfo(OpcodeInfo(Opcode::OP_SETC_NE, "setc.ne", InstGroup::SETC));
    RegisterInfo(OpcodeInfo(Opcode::OP_SETC_AND, "setc.and", InstGroup::SETC));
    RegisterInfo(OpcodeInfo(Opcode::OP_SETC_OR, "setc.or", InstGroup::SETC));
    RegisterInfo(OpcodeInfo(Opcode::OP_SETC_LT, "setc.lt", InstGroup::SETC));
    RegisterInfo(OpcodeInfo(Opcode::OP_SETC_GE, "setc.ge", InstGroup::SETC));
    RegisterInfo(OpcodeInfo(Opcode::OP_SETC_LTU, "setc.ltu", InstGroup::SETC));
    RegisterInfo(OpcodeInfo(Opcode::OP_SETC_GEU, "setc.geu", InstGroup::SETC));
    RegisterInfo(OpcodeInfo(Opcode::OP_SETC_EQI, "setc.eqi", InstGroup::SETC, true));
    RegisterInfo(OpcodeInfo(Opcode::OP_SETC_NEI, "setc.nei", InstGroup::SETC, true));
    RegisterInfo(OpcodeInfo(Opcode::OP_SETC_ANDI, "setc.andi", InstGroup::SETC, true));
    RegisterInfo(OpcodeInfo(Opcode::OP_SETC_ORI, "setc.ori", InstGroup::SETC, true));
    RegisterInfo(OpcodeInfo(Opcode::OP_SETC_LTI, "setc.lti", InstGroup::SETC, true));
    RegisterInfo(OpcodeInfo(Opcode::OP_SETC_GEI, "setc.gei", InstGroup::SETC, true));
    RegisterInfo(OpcodeInfo(Opcode::OP_SETC_LTUI, "setc.ltui", InstGroup::SETC, true));
    RegisterInfo(OpcodeInfo(Opcode::OP_SETC_GEUI, "setc.geui", InstGroup::SETC, true));
}

void OpcodeManager::RegisterPCRelativeOpcode()
{
    // Group: PC-Relative
    RegisterInfo(OpcodeInfo(Opcode::OP_ADDTPC, "addtpc", InstGroup::PC));
    RegisterInfo(OpcodeInfo(Opcode::OP_SETRET, "setret", InstGroup::PC));
}

void OpcodeManager::RegisterImmediateOpcode()
{
    // Group: Immediate
    RegisterInfo(OpcodeInfo(Opcode::OP_LUI, "lui", InstGroup::IMMEDIATE));
    RegisterInfo(OpcodeInfo(Opcode::OP_LIS, "lis", InstGroup::IMMEDIATE));
    RegisterInfo(OpcodeInfo(Opcode::OP_LIU, "liu", InstGroup::IMMEDIATE));
}

void OpcodeManager::RegisterMoveOpcode()
{
    RegisterInfo(OpcodeInfo(Opcode::OP_MOVR, "movr", InstGroup::MOVE));
    RegisterInfo(OpcodeInfo(Opcode::OP_MOVI, "movi", InstGroup::MOVE));
}

void OpcodeManager::RegisterBranchOpcode()
{
    // Group: Branch
    RegisterInfo(OpcodeInfo(Opcode::OP_B_EQ, "b.eq", InstGroup::BRANCH));
    RegisterInfo(OpcodeInfo(Opcode::OP_B_NE, "b.ne", InstGroup::BRANCH));
    RegisterInfo(OpcodeInfo(Opcode::OP_B_LT, "b.lt", InstGroup::BRANCH));
    RegisterInfo(OpcodeInfo(Opcode::OP_B_GE, "b.ge", InstGroup::BRANCH));
    RegisterInfo(OpcodeInfo(Opcode::OP_B_LTU, "b.ltu", InstGroup::BRANCH));
    RegisterInfo(OpcodeInfo(Opcode::OP_B_GEU, "b.geu", InstGroup::BRANCH));
    RegisterInfo(OpcodeInfo(Opcode::OP_JR, "jr", InstGroup::BRANCH));
    RegisterInfo(OpcodeInfo(Opcode::OP_J, "j", InstGroup::BRANCH));
    RegisterInfo(OpcodeInfo(Opcode::OP_B_Z, "b.z", InstGroup::BRANCH));
    RegisterInfo(OpcodeInfo(Opcode::OP_B_NZ, "b.nz", InstGroup::BRANCH));
}

void OpcodeManager::RegisterMultiCycleOpcode()
{
    // Group: Multi-Cycle ALU
    RegisterInfo(OpcodeInfo(Opcode::OP_MUL, "mul", InstGroup::MULTICYCLE));
    RegisterInfo(OpcodeInfo(Opcode::OP_MULU, "mulu", InstGroup::MULTICYCLE));
    RegisterInfo(OpcodeInfo(Opcode::OP_MULW, "mulw", InstGroup::MULTICYCLE));
    RegisterInfo(OpcodeInfo(Opcode::OP_MULUW, "muluw", InstGroup::MULTICYCLE));
    RegisterInfo(OpcodeInfo(Opcode::OP_MADD, "madd", InstGroup::MULTICYCLE));
    RegisterInfo(OpcodeInfo(Opcode::OP_MADDW, "maddw", InstGroup::MULTICYCLE));
    RegisterInfo(OpcodeInfo(Opcode::OP_DIV, "div", InstGroup::MULTICYCLE));
    RegisterInfo(OpcodeInfo(Opcode::OP_DIVU, "divu", InstGroup::MULTICYCLE));
    RegisterInfo(OpcodeInfo(Opcode::OP_DIVW, "divw", InstGroup::MULTICYCLE));
    RegisterInfo(OpcodeInfo(Opcode::OP_DIVUW, "divuw", InstGroup::MULTICYCLE));
    RegisterInfo(OpcodeInfo(Opcode::OP_REM, "rem", InstGroup::MULTICYCLE));
    RegisterInfo(OpcodeInfo(Opcode::OP_REMU, "remu", InstGroup::MULTICYCLE));
    RegisterInfo(OpcodeInfo(Opcode::OP_REMW, "remw", InstGroup::MULTICYCLE));
    RegisterInfo(OpcodeInfo(Opcode::OP_REMUW, "remuw", InstGroup::MULTICYCLE));

    RegisterInfo(OpcodeInfo(Opcode::OP_MIADD, "miadd", InstGroup::MULTICYCLE));
    RegisterInfo(OpcodeInfo(Opcode::OP_MISUB, "misub", InstGroup::MULTICYCLE));
}

void OpcodeManager::RegisterBitOpcode()
{
    // Group: Bit Operation
    RegisterInfo(OpcodeInfo(Opcode::OP_BXS, "bxs", InstGroup::BIT));
    RegisterInfo(OpcodeInfo(Opcode::OP_BXU, "bxu", InstGroup::BIT));
    RegisterInfo(OpcodeInfo(Opcode::OP_BIC, "bic", InstGroup::BIT));
    RegisterInfo(OpcodeInfo(Opcode::OP_BIS, "bis", InstGroup::BIT));
    RegisterInfo(OpcodeInfo(Opcode::OP_CTZ, "ctz", InstGroup::BIT));
    RegisterInfo(OpcodeInfo(Opcode::OP_CLZ, "clz", InstGroup::BIT));
    RegisterInfo(OpcodeInfo(Opcode::OP_BCNT, "bcnt", InstGroup::BIT));
    RegisterInfo(OpcodeInfo(Opcode::OP_REV, "rev", InstGroup::BIT));

    RegisterInfo(OpcodeInfo(Opcode::OP_BFI, "bfi", InstGroup::BIT));
    RegisterInfo(OpcodeInfo(Opcode::OP_CCAT, "ccat", InstGroup::BIT));
    RegisterInfo(OpcodeInfo(Opcode::OP_CCATW, "ccatw", InstGroup::BIT));
}

void OpcodeManager::RegisterCommpoundOpcode()
{
    // Group: Compound Operation
    RegisterInfo(OpcodeInfo(Opcode::OP_CSEL, "csel", InstGroup::COMPOUND));
    RegisterInfo(OpcodeInfo(Opcode::OP_PSEL, "psel", InstGroup::COMPOUND));
}

void OpcodeManager::RegisterLoadOpcode()
{
    // Group: Load Register Offset
    RegisterInfo(OpcodeInfo(Opcode::OP_LB, "lb", InstGroup::LOAD, true, MakeAccMem(BYTE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LH, "lh", InstGroup::LOAD, true, MakeAccMem(HALF_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LW, "lw", InstGroup::LOAD, true, MakeAccMem(WORD_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LD, "ld", InstGroup::LOAD, true, MakeAccMem(DOUBLE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LBU, "lbu", InstGroup::LOAD, true, MakeAccMem(BYTE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LHU, "lhu", InstGroup::LOAD, true, MakeAccMem(HALF_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWU, "lwu", InstGroup::LOAD, true, MakeAccMem(WORD_SIZE, false, false)));

    // Group: Load Immediate Offset
    RegisterInfo(OpcodeInfo(Opcode::OP_LBI, "lbi", InstGroup::LOAD, MakeAccMem(BYTE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LHI, "lhi", InstGroup::LOAD, MakeAccMem(HALF_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWI, "lwi", InstGroup::LOAD, MakeAccMem(WORD_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LDI, "ldi", InstGroup::LOAD, MakeAccMem(DOUBLE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LBUI, "lbui", InstGroup::LOAD, MakeAccMem(BYTE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LHUI, "lhui", InstGroup::LOAD, MakeAccMem(HALF_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWUI, "lwui", InstGroup::LOAD, MakeAccMem(WORD_SIZE, false, false)));

    // Group: Load Immediate Offset UnScaled
    RegisterInfo(OpcodeInfo(Opcode::OP_LHI_U, "lhi.u", InstGroup::LOAD, MakeAccMem(HALF_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWI_U, "lwi.u", InstGroup::LOAD, MakeAccMem(WORD_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LDI_U, "ldi.u", InstGroup::LOAD, MakeAccMem(DOUBLE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LHUI_U, "lhui.u", InstGroup::LOAD, MakeAccMem(HALF_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWUI_U, "lwui.u", InstGroup::LOAD, MakeAccMem(WORD_SIZE, false, false)));

    // Group: Load Symbol
    RegisterInfo(OpcodeInfo(Opcode::OP_LB_PCR, "lb.pcr", InstGroup::LOAD, MakeAccMem(BYTE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LH_PCR, "lh.pcr", InstGroup::LOAD, MakeAccMem(HALF_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LW_PCR, "lw.pcr", InstGroup::LOAD, MakeAccMem(WORD_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LD_PCR, "ld.pcr", InstGroup::LOAD, MakeAccMem(DOUBLE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LBU_PCR, "lbu.pcr", InstGroup::LOAD, MakeAccMem(BYTE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LHU_PCR, "lhu.pcr", InstGroup::LOAD, MakeAccMem(HALF_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWU_PCR, "lwu.pcr", InstGroup::LOAD, MakeAccMem(WORD_SIZE, false, false)));

    // 48 Bits Group: Load Pair
    RegisterInfo(OpcodeInfo(Opcode::OP_LBP, "lbp", InstGroup::LOAD, true, MakeAccMem(BYTE_SIZE, true, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LHP, "lhp", InstGroup::LOAD, true, MakeAccMem(HALF_SIZE, true, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWP, "lwp", InstGroup::LOAD, true, MakeAccMem(WORD_SIZE, true, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LDP, "ldp", InstGroup::LOAD, true, MakeAccMem(DOUBLE_SIZE, true, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LBUP, "lbup", InstGroup::LOAD, true, MakeAccMem(BYTE_SIZE, false, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LHUP, "lhup", InstGroup::LOAD, true, MakeAccMem(HALF_SIZE, false, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWUP, "lwup", InstGroup::LOAD, true, MakeAccMem(WORD_SIZE, false, true)));

    RegisterInfo(OpcodeInfo(Opcode::OP_LBIP, "lbip", InstGroup::LOAD, MakeAccMem(BYTE_SIZE, true, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LHIP, "lhip", InstGroup::LOAD, MakeAccMem(HALF_SIZE, true, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWIP, "lwip", InstGroup::LOAD, MakeAccMem(WORD_SIZE, true, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LDIP, "ldip", InstGroup::LOAD, MakeAccMem(DOUBLE_SIZE, true, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LBUIP, "lbuip", InstGroup::LOAD, MakeAccMem(BYTE_SIZE, false, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LHUIP, "lhuip", InstGroup::LOAD, MakeAccMem(HALF_SIZE, false, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWUIP, "lwuip", InstGroup::LOAD, MakeAccMem(WORD_SIZE, false, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LHIP_U, "lhip.u", InstGroup::LOAD, MakeAccMem(HALF_SIZE, true, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWIP_U, "lwip.u", InstGroup::LOAD, MakeAccMem(WORD_SIZE, true, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LDIP_U, "ldip.u", InstGroup::LOAD, MakeAccMem(DOUBLE_SIZE, true, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LHUIP_U, "lhuip.u", InstGroup::LOAD, MakeAccMem(HALF_SIZE, true, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWUIP_U, "lwuip.u", InstGroup::LOAD, MakeAccMem(WORD_SIZE, true, true)));
    // 48 Bits Group: Load Pre-index
    RegisterInfo(OpcodeInfo(Opcode::OP_LB_PR, "lb.pr", InstGroup::LOAD, true, MakeAccMem(BYTE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LH_PR, "lh.pr", InstGroup::LOAD, true, MakeAccMem(HALF_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LW_PR, "lw.pr", InstGroup::LOAD, true, MakeAccMem(WORD_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LD_PR, "ld.pr", InstGroup::LOAD, true, MakeAccMem(DOUBLE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LBU_PR, "lbu.pr", InstGroup::LOAD, true, MakeAccMem(BYTE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LHU_PR, "lhu.pr", InstGroup::LOAD, true, MakeAccMem(HALF_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWU_PR, "lwu.pr", InstGroup::LOAD, true, MakeAccMem(WORD_SIZE, false, false)));

    RegisterInfo(OpcodeInfo(Opcode::OP_LBI_PR, "lbi.pr", InstGroup::LOAD, MakeAccMem(BYTE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LHI_PR, "lhi.pr", InstGroup::LOAD, MakeAccMem(HALF_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWI_PR, "lwi.pr", InstGroup::LOAD, MakeAccMem(WORD_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LDI_PR, "ldi.pr", InstGroup::LOAD, MakeAccMem(DOUBLE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LBUI_PR, "lbui.pr", InstGroup::LOAD, MakeAccMem(BYTE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LHUI_PR, "lhui.pr", InstGroup::LOAD, MakeAccMem(HALF_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWUI_PR, "lwui.pr", InstGroup::LOAD, MakeAccMem(WORD_SIZE, false, false)));

    RegisterInfo(OpcodeInfo(Opcode::OP_LHI_UPR, "lhi.upr", InstGroup::LOAD, MakeAccMem(HALF_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWI_UPR, "lwi.upr", InstGroup::LOAD, MakeAccMem(WORD_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LDI_UPR, "ldi.upr", InstGroup::LOAD, MakeAccMem(DOUBLE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LHUI_UPR, "lhui.upr", InstGroup::LOAD, MakeAccMem(HALF_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWUI_UPR, "lwui.upr", InstGroup::LOAD, MakeAccMem(WORD_SIZE, false, false)));

    RegisterInfo(OpcodeInfo(Opcode::OP_LB_PO, "lb.po", InstGroup::LOAD, true, MakeAccMem(BYTE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LH_PO, "lh.po", InstGroup::LOAD, true, MakeAccMem(HALF_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LW_PO, "lw.po", InstGroup::LOAD, true, MakeAccMem(WORD_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LD_PO, "ld.po", InstGroup::LOAD, true, MakeAccMem(DOUBLE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LBU_PO, "lbu.po", InstGroup::LOAD, true, MakeAccMem(BYTE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LHU_PO, "lhu.po", InstGroup::LOAD, true, MakeAccMem(HALF_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWU_PO, "lwu.po", InstGroup::LOAD, true, MakeAccMem(WORD_SIZE, false, false)));

    RegisterInfo(OpcodeInfo(Opcode::OP_LBI_PO, "lbi.po", InstGroup::LOAD, MakeAccMem(BYTE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LHI_PO, "lhi.po", InstGroup::LOAD, MakeAccMem(HALF_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWI_PO, "lwi.po", InstGroup::LOAD, MakeAccMem(WORD_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LDI_PO, "ldi.po", InstGroup::LOAD, MakeAccMem(DOUBLE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LBUI_PO, "lbui.po", InstGroup::LOAD, MakeAccMem(BYTE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LHUI_PO, "lhui.po", InstGroup::LOAD, MakeAccMem(HALF_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWUI_PO, "lwui.po", InstGroup::LOAD, MakeAccMem(WORD_SIZE, false, false)));

    RegisterInfo(OpcodeInfo(Opcode::OP_LHI_UPO, "lhi.upo", InstGroup::LOAD, MakeAccMem(HALF_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWI_UPO, "lwi.upo", InstGroup::LOAD, MakeAccMem(WORD_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LDI_UPO, "ldi.upo", InstGroup::LOAD, MakeAccMem(DOUBLE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LHUI_UPO, "lhui.upo", InstGroup::LOAD, MakeAccMem(HALF_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWUI_UPO, "lwui.upo", InstGroup::LOAD, MakeAccMem(WORD_SIZE, false, false)));

    // Group: Load Bridge Register Offset
    RegisterInfo(OpcodeInfo(Opcode::OP_LB_BRG, "lb.brg", InstGroup::LOAD, true, MakeAccMem(BYTE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LH_BRG, "lh.brg", InstGroup::LOAD, true, MakeAccMem(HALF_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LW_BRG, "lw.brg", InstGroup::LOAD, true, MakeAccMem(WORD_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LD_BRG, "ld.brg", InstGroup::LOAD, true, MakeAccMem(DOUBLE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LBU_BRG, "lbu.brg", InstGroup::LOAD, true, MakeAccMem(BYTE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LHU_BRG, "lhu.brg", InstGroup::LOAD, true, MakeAccMem(HALF_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWU_BRG, "lwu.brg", InstGroup::LOAD, true, MakeAccMem(WORD_SIZE, false, false)));

    // Group: Load Bridge Immediate Offset
    RegisterInfo(OpcodeInfo(Opcode::OP_LBI_BRG, "lbi.brg", InstGroup::LOAD, MakeAccMem(BYTE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LHI_BRG, "lhi.brg", InstGroup::LOAD, MakeAccMem(HALF_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWI_BRG, "lwi.brg", InstGroup::LOAD, MakeAccMem(WORD_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LDI_BRG, "ldi.brg", InstGroup::LOAD, MakeAccMem(DOUBLE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LBUI_BRG, "lbui.brg", InstGroup::LOAD, MakeAccMem(BYTE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LHUI_BRG, "lhui.brg", InstGroup::LOAD, MakeAccMem(HALF_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWUI_BRG, "lwui.brg", InstGroup::LOAD, MakeAccMem(WORD_SIZE, false, false)));

    // Group: Load Bridge Instruction UnScaled
    RegisterInfo(OpcodeInfo(Opcode::OP_LHI_U_BRG, "lhi.ubrg", InstGroup::LOAD, MakeAccMem(HALF_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWI_U_BRG, "lwi.ubrg", InstGroup::LOAD, MakeAccMem(WORD_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LDI_U_BRG, "ldi.ubrg", InstGroup::LOAD, MakeAccMem(DOUBLE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LHUI_U_BRG, "lhui.ubrg", InstGroup::LOAD, MakeAccMem(HALF_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LWUI_U_BRG, "lwui.ubrg", InstGroup::LOAD, MakeAccMem(WORD_SIZE, false, false)));
}

void OpcodeManager::RegisterStoreOpcode()
{
    // Group: Store Register Offset
    // 64 bit, srcR need shamt
    RegisterInfo(OpcodeInfo(Opcode::OP_SB, "sb", InstGroup::STORE, MakeAccMem(BYTE_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SH, "sh", InstGroup::STORE, MakeAccMem(HALF_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SW, "sw", InstGroup::STORE, MakeAccMem(WORD_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SD, "sd", InstGroup::STORE, MakeAccMem(DOUBLE_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SH_U, "sh.u", InstGroup::STORE, MakeAccMem(HALF_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SW_U, "sw.u", InstGroup::STORE, MakeAccMem(WORD_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SD_U, "sd.u", InstGroup::STORE, MakeAccMem(DOUBLE_SIZE, false)));

    // Group: Store Immediate Offset
    RegisterInfo(OpcodeInfo(Opcode::OP_SBI, "sbi", InstGroup::STORE, MakeAccMem(BYTE_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SHI, "shi", InstGroup::STORE, MakeAccMem(HALF_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SWI, "swi", InstGroup::STORE, MakeAccMem(WORD_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SDI, "sdi", InstGroup::STORE, MakeAccMem(DOUBLE_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SHI_U, "shi.u", InstGroup::STORE, MakeAccMem(HALF_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SWI_U, "swi.u", InstGroup::STORE, MakeAccMem(WORD_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SDI_U, "sdi.u", InstGroup::STORE, MakeAccMem(DOUBLE_SIZE, false)));

    // Group: Store Symbol
    RegisterInfo(OpcodeInfo(Opcode::OP_SB_PCR, "sb.pcr", InstGroup::STORE, MakeAccMem(BYTE_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SH_PCR, "sh.pcr", InstGroup::STORE, MakeAccMem(HALF_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SW_PCR, "sw.pcr", InstGroup::STORE, MakeAccMem(WORD_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SD_PCR, "sd.pcr", InstGroup::STORE, MakeAccMem(DOUBLE_SIZE, false)));

    // 48 bit Store Pair
    RegisterInfo(OpcodeInfo(Opcode::OP_SBP, "sbp", InstGroup::STORE, MakeAccMem(BYTE_SIZE, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SHP, "shp", InstGroup::STORE, MakeAccMem(HALF_SIZE, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SWP, "swp", InstGroup::STORE, MakeAccMem(WORD_SIZE, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SDP, "sdp", InstGroup::STORE, MakeAccMem(DOUBLE_SIZE, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SHP_U, "shp.u", InstGroup::STORE, MakeAccMem(HALF_SIZE, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SWP_U, "swp.u", InstGroup::STORE, MakeAccMem(WORD_SIZE, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SDP_U, "sdp.u", InstGroup::STORE, MakeAccMem(DOUBLE_SIZE, true)));

    RegisterInfo(OpcodeInfo(Opcode::OP_SBIP, "sbip", InstGroup::STORE, MakeAccMem(BYTE_SIZE, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SHIP, "ship", InstGroup::STORE, MakeAccMem(HALF_SIZE, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SWIP, "swip", InstGroup::STORE, MakeAccMem(WORD_SIZE, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SDIP, "sdip", InstGroup::STORE, MakeAccMem(DOUBLE_SIZE, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SHIP_U, "ship.u", InstGroup::STORE, MakeAccMem(HALF_SIZE, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SWIP_U, "swip.u", InstGroup::STORE, MakeAccMem(WORD_SIZE, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SDIP_U, "sdip.u", InstGroup::STORE, MakeAccMem(DOUBLE_SIZE, true)));
    // Store Pre-Index
    RegisterInfo(OpcodeInfo(Opcode::OP_SB_PR, "sb.pr", InstGroup::STORE, true, MakeAccMem(BYTE_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SH_PR, "sh.pr", InstGroup::STORE, true, MakeAccMem(HALF_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SW_PR, "sw.pr", InstGroup::STORE, true, MakeAccMem(WORD_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SD_PR, "sd.pr", InstGroup::STORE, true, MakeAccMem(DOUBLE_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SH_UPR, "sh.upr", InstGroup::STORE, true, MakeAccMem(HALF_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SW_UPR, "sw.upr", InstGroup::STORE, true, MakeAccMem(WORD_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SD_UPR, "sd.upr", InstGroup::STORE, true, MakeAccMem(DOUBLE_SIZE, false)));

    RegisterInfo(OpcodeInfo(Opcode::OP_SBI_PR, "sbi.pr", InstGroup::STORE, MakeAccMem(BYTE_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SHI_PR, "shi.pr", InstGroup::STORE, MakeAccMem(HALF_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SWI_PR, "swi.pr", InstGroup::STORE, MakeAccMem(WORD_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SDI_PR, "sdi.pr", InstGroup::STORE, MakeAccMem(DOUBLE_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SHI_UPR, "shi.upr", InstGroup::STORE, MakeAccMem(HALF_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SWI_UPR, "swi.upr", InstGroup::STORE, MakeAccMem(WORD_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SDI_UPR, "sdi.upr", InstGroup::STORE, MakeAccMem(DOUBLE_SIZE, false)));
    // Store Post-Index
    RegisterInfo(OpcodeInfo(Opcode::OP_SB_PO, "sb.po", InstGroup::STORE, MakeAccMem(BYTE_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SH_PO, "sh.po", InstGroup::STORE, MakeAccMem(HALF_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SW_PO, "sw.po", InstGroup::STORE, MakeAccMem(WORD_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SD_PO, "sd.po", InstGroup::STORE, MakeAccMem(DOUBLE_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SH_UPO, "sh.upo", InstGroup::STORE, MakeAccMem(HALF_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SW_UPO, "sw.upo", InstGroup::STORE, MakeAccMem(WORD_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SD_UPO, "sd.upo", InstGroup::STORE, MakeAccMem(DOUBLE_SIZE, false)));

    RegisterInfo(OpcodeInfo(Opcode::OP_SBI_PO, "sbi.po", InstGroup::STORE, MakeAccMem(BYTE_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SHI_PO, "shi.po", InstGroup::STORE, MakeAccMem(HALF_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SWI_PO, "swi.po", InstGroup::STORE, MakeAccMem(WORD_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SDI_PO, "sdi.po", InstGroup::STORE, MakeAccMem(DOUBLE_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SHI_UPO, "shi.upo", InstGroup::STORE, MakeAccMem(HALF_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SWI_UPO, "swi.upo", InstGroup::STORE, MakeAccMem(WORD_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SDI_UPO, "sdi.upo", InstGroup::STORE, MakeAccMem(DOUBLE_SIZE, false)));

    // Group: Store Bridge Register Offset
    RegisterInfo(OpcodeInfo(Opcode::OP_SB_BRG, "sb.brg", InstGroup::STORE, MakeAccMem(BYTE_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SH_BRG, "sh.brg", InstGroup::STORE, MakeAccMem(HALF_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SW_BRG, "sw.brg", InstGroup::STORE, MakeAccMem(WORD_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SD_BRG, "sd.brg", InstGroup::STORE, MakeAccMem(DOUBLE_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SH_U_BRG, "sh.ubrg", InstGroup::STORE, MakeAccMem(HALF_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SW_U_BRG, "sw.ubrg", InstGroup::STORE, MakeAccMem(WORD_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SD_U_BRG, "sd.ubrg", InstGroup::STORE, MakeAccMem(DOUBLE_SIZE, false)));

    // Group: Store Bridge Immediate Offset
    RegisterInfo(OpcodeInfo(Opcode::OP_SBI_BRG, "sbi.brg", InstGroup::STORE, MakeAccMem(BYTE_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SHI_BRG, "shi.brg", InstGroup::STORE, MakeAccMem(HALF_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SWI_BRG, "swi.brg", InstGroup::STORE, MakeAccMem(WORD_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SDI_BRG, "sdi.brg", InstGroup::STORE, MakeAccMem(DOUBLE_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SHI_U_BRG, "shi.ubrg", InstGroup::STORE, MakeAccMem(HALF_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SWI_U_BRG, "swi.ubrg", InstGroup::STORE, MakeAccMem(WORD_SIZE, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SDI_U_BRG, "sdi.ubrg", InstGroup::STORE, MakeAccMem(DOUBLE_SIZE, false)));
}

void OpcodeManager::RegisterPrefetchOpcode()
{
    RegisterInfo(OpcodeInfo(Opcode::OP_PRF, "prf", InstGroup::PREFETCH, true));
    RegisterInfo(OpcodeInfo(Opcode::OP_PRF_A, "prf.a", InstGroup::PREFETCH, true));
    RegisterInfo(OpcodeInfo(Opcode::OP_PRFI_U, "prfi.u", InstGroup::PREFETCH));
    RegisterInfo(OpcodeInfo(Opcode::OP_PRFI_UA, "prfi.ua", InstGroup::PREFETCH));
}

void OpcodeManager::RegisterAtomicOpcode()
{
    // Group: Atomic Operation
    RegisterInfo(OpcodeInfo(Opcode::OP_LR_B, "lr.b", InstGroup::ATOMIC, MakeAccMem(BYTE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LR_H, "lr.h", InstGroup::ATOMIC, MakeAccMem(HALF_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LR_W, "lr.w", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LR_D, "lr.d", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, true, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SC_B, "sc.b", InstGroup::ATOMIC, MakeAccMem(BYTE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SC_H, "sc.h", InstGroup::ATOMIC, MakeAccMem(HALF_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SC_W, "sc.w", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SC_D, "sc.d", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, false)));

    RegisterInfo(OpcodeInfo(Opcode::OP_LW_ADD, "lw.add", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LW_AND, "lw.and", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LW_OR, "lw.or", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LW_XOR, "lw.xor", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LW_MAX, "lw.max", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LW_MIN, "lw.min", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LW_SMAX, "lw.smax", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LW_SMIN, "lw.smin", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LW_UMAX, "lw.umax", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LW_UMIN, "lw.umin", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, false)));

    RegisterInfo(OpcodeInfo(Opcode::OP_SW_ADD, "sw.add", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SW_AND, "sw.and", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SW_OR, "sw.or", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SW_XOR, "sw.xor", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SW_MAX, "sw.max", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SW_MIN, "sw.min", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SW_SMAX, "sw.smax", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SW_SMIN, "sw.smin", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SW_UMAX, "sw.umax", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SW_UMIN, "sw.umin", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, false)));

    RegisterInfo(OpcodeInfo(Opcode::OP_LD_ADD, "ld.add", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LD_AND, "ld.and", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LD_OR, "ld.or", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LD_XOR, "ld.xor", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LD_MAX, "ld.max", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LD_MIN, "ld.min", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LD_SMAX, "ld.smax", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LD_SMIN, "ld.smin", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LD_UMAX, "ld.umax", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_LD_UMIN, "ld.umin", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, false)));

    RegisterInfo(OpcodeInfo(Opcode::OP_SD_ADD, "sd.add", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SD_AND, "sd.and", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SD_OR, "sd.or", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SD_XOR, "sd.xor", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SD_MAX, "sd.max", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SD_MIN, "sd.min", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SD_SMAX, "sd.smax", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SD_SMIN, "sd.smin", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SD_UMAX, "sd.umax", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SD_UMIN, "sd.umin", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, false)));

    RegisterInfo(OpcodeInfo(Opcode::OP_SWAPB, "swap.b", InstGroup::ATOMIC, MakeAccMem(BYTE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SWAPH, "swap.h", InstGroup::ATOMIC, MakeAccMem(HALF_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SWAPW, "swap.w", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_SWAPD, "swap.d", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, false)));

    RegisterInfo(OpcodeInfo(Opcode::OP_CASB, "casb", InstGroup::ATOMIC, MakeAccMem(BYTE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_CASH, "cash", InstGroup::ATOMIC, MakeAccMem(HALF_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_CASW, "casw", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_CASD, "casd", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_CASBP, "casbp", InstGroup::ATOMIC, MakeAccMem(BYTE_SIZE, false, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_CASHP, "cashp", InstGroup::ATOMIC, MakeAccMem(HALF_SIZE, false, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_CASWP, "caswp", InstGroup::ATOMIC, MakeAccMem(WORD_SIZE, false, true)));
    RegisterInfo(OpcodeInfo(Opcode::OP_CASDP, "casdp", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, true)));

    RegisterInfo(OpcodeInfo(Opcode::OP_DMA, "dma", InstGroup::ATOMIC, MakeAccMem(DOUBLE_SIZE, false, true)));
}

void OpcodeManager::RegisterExecuteControlOpcode()
{
    // Group: Execution Control
    RegisterInfo(OpcodeInfo(Opcode::OP_BSE, "bse", InstGroup::EXECUTE_CONTROL));
    RegisterInfo(OpcodeInfo(Opcode::OP_BWE, "bwe", InstGroup::EXECUTE_CONTROL));
    RegisterInfo(OpcodeInfo(Opcode::OP_BWI, "bwi", InstGroup::EXECUTE_CONTROL));
    RegisterInfo(OpcodeInfo(Opcode::OP_BWT, "bwt", InstGroup::EXECUTE_CONTROL));
    RegisterInfo(OpcodeInfo(Opcode::OP_ASSERT, "ASSERT", InstGroup::EXECUTE_CONTROL));
    RegisterInfo(OpcodeInfo(Opcode::OP_DSB, "dsb", InstGroup::EXECUTE_CONTROL));
    RegisterInfo(OpcodeInfo(Opcode::OP_ISB, "isb", InstGroup::EXECUTE_CONTROL));
    RegisterInfo(OpcodeInfo(Opcode::OP_ACRC, "ACRC", InstGroup::EXECUTE_CONTROL));
    RegisterInfo(OpcodeInfo(Opcode::OP_ACRE, "ACRE", InstGroup::EXECUTE_CONTROL));
}

void OpcodeManager::RegisterExtendOpcode()
{
    RegisterInfo(OpcodeInfo(Opcode::OP_SEXT_B, "sext.b", InstGroup::EXTEND));
    RegisterInfo(OpcodeInfo(Opcode::OP_SEXT_H, "sext.h", InstGroup::EXTEND));
    RegisterInfo(OpcodeInfo(Opcode::OP_SEXT_W, "sext.w", InstGroup::EXTEND));
    RegisterInfo(OpcodeInfo(Opcode::OP_ZEXT_B, "zext.b", InstGroup::EXTEND));
    RegisterInfo(OpcodeInfo(Opcode::OP_ZEXT_H, "zext.h", InstGroup::EXTEND));
    RegisterInfo(OpcodeInfo(Opcode::OP_ZEXT_W, "zext.w", InstGroup::EXTEND));
}

void OpcodeManager::RegisterCacheMaintainOpcode()
{
    RegisterInfo(OpcodeInfo(Opcode::OP_BC_IVA, "bc.iva", InstGroup::CACHE_MAINTAIN));
    RegisterInfo(OpcodeInfo(Opcode::OP_BC_IALL, "bc.iall", InstGroup::CACHE_MAINTAIN));
    RegisterInfo(OpcodeInfo(Opcode::OP_IC_IVA, "ic.iva", InstGroup::CACHE_MAINTAIN));
    RegisterInfo(OpcodeInfo(Opcode::OP_IC_IALL, "ic.iall", InstGroup::CACHE_MAINTAIN));
    RegisterInfo(OpcodeInfo(Opcode::OP_DC_IVA, "dc.iva", InstGroup::CACHE_MAINTAIN));
    RegisterInfo(OpcodeInfo(Opcode::OP_DC_IALL, "dc.iall", InstGroup::CACHE_MAINTAIN));
    RegisterInfo(OpcodeInfo(Opcode::OP_DC_CVA, "dc.cva", InstGroup::CACHE_MAINTAIN));
    RegisterInfo(OpcodeInfo(Opcode::OP_DC_CIVA, "dc.civa", InstGroup::CACHE_MAINTAIN));
    RegisterInfo(OpcodeInfo(Opcode::OP_DC_ISW, "dc.isw", InstGroup::CACHE_MAINTAIN));
    RegisterInfo(OpcodeInfo(Opcode::OP_DC_CSW, "dc.csw", InstGroup::CACHE_MAINTAIN));
    RegisterInfo(OpcodeInfo(Opcode::OP_DC_CISW, "dc.cisw", InstGroup::CACHE_MAINTAIN));
    RegisterInfo(OpcodeInfo(Opcode::OP_DC_ZVA, "dc.zva", InstGroup::CACHE_MAINTAIN, MakeAccMem(64, false, false)));
    RegisterInfo(OpcodeInfo(Opcode::OP_TLB_IA, "tlb.ia", InstGroup::CACHE_MAINTAIN));
    RegisterInfo(OpcodeInfo(Opcode::OP_TLB_IV, "tlb.iv", InstGroup::CACHE_MAINTAIN));
    RegisterInfo(OpcodeInfo(Opcode::OP_TLB_IAV, "tlb.iav", InstGroup::CACHE_MAINTAIN));
    RegisterInfo(OpcodeInfo(Opcode::OP_TLB_IALL, "tlb.iall", InstGroup::CACHE_MAINTAIN));
}

void OpcodeManager::RegisterGMOOpcode()
{
}

void OpcodeManager::RegisterSSROpcode()
{
    // Group: SSR Aaccelss
    RegisterInfo(OpcodeInfo(Opcode::OP_SSRGET, "ssrget", InstGroup::SSR));
    RegisterInfo(OpcodeInfo(Opcode::OP_SSRSET, "ssrset", InstGroup::SSR));
    RegisterInfo(OpcodeInfo(Opcode::OP_SSRSWAP, "ssrswap", InstGroup::SSR));
    RegisterInfo(OpcodeInfo(Opcode::OP_LSRGET, "lsrget", InstGroup::SSR));
    RegisterInfo(OpcodeInfo(Opcode::OP_SETC_TGT, "setc.tgt", InstGroup::SSR));
}

void OpcodeManager::RegisterMaxMinOpcode()
{
    // Group: Max-Min
    RegisterInfo(OpcodeInfo(Opcode::OP_MAX, "max", InstGroup::MAX_MIN));
    RegisterInfo(OpcodeInfo(Opcode::OP_MAXU, "maxu", InstGroup::MAX_MIN));
    RegisterInfo(OpcodeInfo(Opcode::OP_MIN, "min", InstGroup::MAX_MIN));
    RegisterInfo(OpcodeInfo(Opcode::OP_MINU, "minu", InstGroup::MAX_MIN));
    RegisterInfo(OpcodeInfo(Opcode::OP_FMAX, "fmax", InstGroup::MAX_MIN));
    RegisterInfo(OpcodeInfo(Opcode::OP_FMIN, "fmin", InstGroup::MAX_MIN));
}

void OpcodeManager::RegisterConvertOpcode()
{
    // Group: Format Convert
    RegisterInfo(OpcodeInfo(Opcode::OP_FCVT, "fcvt", InstGroup::CONVERT));
    RegisterInfo(OpcodeInfo(Opcode::OP_FCVTA, "fcvta", InstGroup::CONVERT));
    RegisterInfo(OpcodeInfo(Opcode::OP_FCVTM, "fcvtm", InstGroup::CONVERT));
    RegisterInfo(OpcodeInfo(Opcode::OP_FCVTN, "fcvtn", InstGroup::CONVERT));
    RegisterInfo(OpcodeInfo(Opcode::OP_FCVTP, "fcvtp", InstGroup::CONVERT));
    RegisterInfo(OpcodeInfo(Opcode::OP_FCVTZ, "fcvtz", InstGroup::CONVERT));
    RegisterInfo(OpcodeInfo(Opcode::OP_SCVTF, "scvtf", InstGroup::CONVERT));
    RegisterInfo(OpcodeInfo(Opcode::OP_UCVTF, "ucvtf", InstGroup::CONVERT));

    RegisterInfo(OpcodeInfo(Opcode::OP_FCVTI, "fcvti", InstGroup::CONVERT));
    RegisterInfo(OpcodeInfo(Opcode::OP_ICVT, "icvt", InstGroup::CONVERT));
    RegisterInfo(OpcodeInfo(Opcode::OP_ICVTF, "icvtf", InstGroup::CONVERT));
}

void OpcodeManager::RegisterReduceOpcode()
{
    // Group: Reduce Operation
    RegisterInfo(OpcodeInfo(Opcode::OP_RDADD, "rdadd", InstGroup::REDUCE));
    RegisterInfo(OpcodeInfo(Opcode::OP_RDAND, "rdand", InstGroup::REDUCE));
    RegisterInfo(OpcodeInfo(Opcode::OP_RDOR, "rdor", InstGroup::REDUCE));
    RegisterInfo(OpcodeInfo(Opcode::OP_RDXOR, "rdxor", InstGroup::REDUCE));
    RegisterInfo(OpcodeInfo(Opcode::OP_RDFADD, "rdfadd", InstGroup::REDUCE));
    RegisterInfo(OpcodeInfo(Opcode::OP_RDMAX, "rdmax", InstGroup::REDUCE));
    RegisterInfo(OpcodeInfo(Opcode::OP_RDMIN, "rdmin", InstGroup::REDUCE));
    RegisterInfo(OpcodeInfo(Opcode::OP_RDFMAX, "rdfmax", InstGroup::REDUCE));
    RegisterInfo(OpcodeInfo(Opcode::OP_RDFMIN, "rdfmin", InstGroup::REDUCE));
}

void OpcodeManager::RegisterShuffleOpcode()
{
    // Group: Shuffle
    RegisterInfo(OpcodeInfo(Opcode::OP_SHFL_UP, "shuffle.up", InstGroup::SHUFFLE));
    RegisterInfo(OpcodeInfo(Opcode::OP_SHFL_DOWN, "shuffle.down", InstGroup::SHUFFLE));
    RegisterInfo(OpcodeInfo(Opcode::OP_SHFL_BFLY, "shuffle.butterfly", InstGroup::SHUFFLE));
    RegisterInfo(OpcodeInfo(Opcode::OP_SHFL_IDX, "shuffle.index", InstGroup::SHUFFLE));
    RegisterInfo(OpcodeInfo(Opcode::OP_SHFLI_UP, "shuffle.i.up", InstGroup::SHUFFLE));
    RegisterInfo(OpcodeInfo(Opcode::OP_SHFLI_DOWN, "shuffle.i.down", InstGroup::SHUFFLE));
    RegisterInfo(OpcodeInfo(Opcode::OP_SHFLI_BFLY, "shuffle.i.butterfly", InstGroup::SHUFFLE));
    RegisterInfo(OpcodeInfo(Opcode::OP_SHFLI_IDX, "shuffle.i.index", InstGroup::SHUFFLE));
}

void OpcodeManager::RegisterSpecialOpcode()
{
    // Group: Special Operation
    RegisterInfo(OpcodeInfo(Opcode::OP_MOV, "mov", InstGroup::SPECIAL));
}

void OpcodeManager::RegisterOpcode()
{
    RegisterInfo(OpcodeInfo(Opcode::OP_INVALID, "invaild", InstGroup::UNDEF));

    RegisterBlockSplitOpcode();
    RegisterArithmeticOpcode();
    RegisterArithmeticFPOpcode();
    RegisterCompareOpcode();
    RegisterCompareFPOpcode();
    RegisterSetcOpcode();
    RegisterPCRelativeOpcode();
    RegisterImmediateOpcode();
    RegisterMoveOpcode();
    RegisterBranchOpcode();
    RegisterMultiCycleOpcode();
    RegisterBitOpcode();
    RegisterCommpoundOpcode();
    RegisterLoadOpcode();
    RegisterStoreOpcode();
    RegisterPrefetchOpcode();
    RegisterAtomicOpcode();
    RegisterExecuteControlOpcode();
    RegisterExtendOpcode();
    RegisterCacheMaintainOpcode();
    RegisterGMOOpcode();
    RegisterSSROpcode();
    RegisterMaxMinOpcode();
    RegisterConvertOpcode();
    RegisterReduceOpcode();
    RegisterShuffleOpcode();
    RegisterSpecialOpcode();
}

bool OpcodeManager::IsNeedAccumulateBlockInfo(Opcode opcode) const
{
    InstGroup grp = GetOpcodeGroup(opcode);
    /* e.g. BLOCK_SPLIT, BLOCK_OFFSET, BLOCK_IO, BLOCK_ATTR, BLOCK_HINT, BLOCK_ARGUMENT */
    return (static_cast<size_t>(grp) <= static_cast<size_t>(InstGroup::BLOCK_ARGUMENT));
}
} // namespace JCore