#include "iex_latency.h"

namespace JCore {

using namespace std;

const unordered_map<Opcode, IexLatencyInfo> IexLatency::VEC_LATENCY = {
    // Vector/Vector-Scalar
    { Opcode::OP_ADD, { 2, IexExecUnit::IALU } },
    { Opcode::OP_ADDI, { 2, IexExecUnit::IALU } },
    { Opcode::OP_AND, { 2, IexExecUnit::IALU } },
    { Opcode::OP_ANDI, { 2, IexExecUnit::IALU } },
    { Opcode::OP_BXU, { 2, IexExecUnit::IALU } },
    { Opcode::OP_CMP_EQ, { 2, IexExecUnit::IALU } },
    { Opcode::OP_CMP_EQI, { 2, IexExecUnit::IALU } },
    { Opcode::OP_CMP_GE, { 2, IexExecUnit::IALU } },
    { Opcode::OP_CMP_LT, { 2, IexExecUnit::IALU } },
    { Opcode::OP_CSEL, { 2, IexExecUnit::PERMUTE } },
    { Opcode::OP_FADD, { 3, IexExecUnit::FMLA } },
    { Opcode::OP_FCVT, { 3, IexExecUnit::FCVT } },
    { Opcode::OP_FCVTI, { 3, IexExecUnit::FCVT } },
    { Opcode::OP_FDIV, { 6, IexExecUnit::FDIV } }, // 64+6
    { Opcode::OP_FEQ, { 2, IexExecUnit::FMLA } },
    { Opcode::OP_FEXP, { 4, IexExecUnit::FMLA } },
    { Opcode::OP_FMADD, { 5, IexExecUnit::FMLA } },
    { Opcode::OP_FMSUB, { 5, IexExecUnit::FMLA } },
    { Opcode::OP_FMAX, { 3, IexExecUnit::FMLA } },
    { Opcode::OP_FMUL, { 4, IexExecUnit::FMLA } },
    { Opcode::OP_FSQRT, { 6, IexExecUnit::FDIV } }, // 64+6
    { Opcode::OP_FSUB, { 3, IexExecUnit::FMLA } },
    { Opcode::OP_ICVTF, { 3, IexExecUnit::FCVT } },
    { Opcode::OP_ICVT, { 3, IexExecUnit::FCVT } },
    { Opcode::OP_MADD, { 4, IexExecUnit::IMAC } },
    { Opcode::OP_MUL, { 4, IexExecUnit::IMAC } },
    { Opcode::OP_OR, { 2, IexExecUnit::IALU } },
    { Opcode::OP_ORI, { 2, IexExecUnit::IALU } },
    { Opcode::OP_SHFLI_BFLY, { 3, IexExecUnit::PERMUTE } },
    { Opcode::OP_SLLI, { 3, IexExecUnit::IALU } },
    { Opcode::OP_SRLI, { 3, IexExecUnit::IALU } },
    { Opcode::OP_SUBI, { 2, IexExecUnit::IALU } },
    { Opcode::OP_XOR, { 2, IexExecUnit::IALU } },
    { Opcode::OP_BIC, { 2, IexExecUnit::IALU } },
    { Opcode::OP_ADDTPC, { 2, IexExecUnit::UNDEF } },
    { Opcode::OP_ADDW, { 2, IexExecUnit::IALU } },
    { Opcode::OP_ANDW, { 2, IexExecUnit::UNDEF } },
    { Opcode::OP_BIS, { 2, IexExecUnit::UNDEF } },
    { Opcode::OP_BXS, { 2, IexExecUnit::UNDEF } },
    { Opcode::OP_CMP_NEI, { 2, IexExecUnit::IALU } },
    { Opcode::OP_MOVI, { 2, IexExecUnit::UNDEF } },
    { Opcode::OP_MOVR, { 2, IexExecUnit::UNDEF } }, // TO BE UPDATED
    { Opcode::OP_CMP_AND, { 2, IexExecUnit::IALU } },
    { Opcode::OP_CMP_ANDI, { 2, IexExecUnit::IALU } },
    { Opcode::OP_CMP_LTI, { 2, IexExecUnit::IALU } },
    { Opcode::OP_CMP_LTU, { 2, IexExecUnit::IALU } },
    { Opcode::OP_CMP_LTUI, { 2, IexExecUnit::IALU } },
    { Opcode::OP_CMP_NE, { 2, IexExecUnit::IALU } },
    { Opcode::OP_CMP_OR, { 2, IexExecUnit::IALU } },
    { Opcode::OP_SEXT_W, { 2, IexExecUnit::UNDEF } },
    { Opcode::OP_SUB, { 2, IexExecUnit::IALU } },
    { Opcode::OP_ZEXT_W, { 2, IexExecUnit::UNDEF } },
    { Opcode::OP_FCVTZ, { 3, IexExecUnit::FCVT } },
    { Opcode::OP_ADDLI, { 2, IexExecUnit::IALU } },
    { Opcode::OP_RDADD, { 4, IexExecUnit::PERMUTE } },
    { Opcode::OP_RDMAX, { 6, IexExecUnit::PERMUTE } },
    { Opcode::OP_LUI, { 2, IexExecUnit::UNDEF } },
    { Opcode::OP_ORW, { 2, IexExecUnit::UNDEF } },
    { Opcode::OP_REMUW, { 6, IexExecUnit::UNDEF } },
    { Opcode::OP_SCVTF, { 3, IexExecUnit::FCVT } },
    { Opcode::OP_SLL, { 3, IexExecUnit::UNDEF } },
    { Opcode::OP_SLLW, { 3, IexExecUnit::UNDEF } },
    { Opcode::OP_SRAI, { 3, IexExecUnit::UNDEF } },
    { Opcode::OP_SRL, { 3, IexExecUnit::UNDEF } },
    { Opcode::OP_SRLW, { 3, IexExecUnit::UNDEF } },
    { Opcode::OP_SUBW, { 2, IexExecUnit::IALU } },
    { Opcode::OP_XORI, { 2, IexExecUnit::UNDEF } },
    { Opcode::OP_REM, { 6, IexExecUnit::FDIV } },
    { Opcode::OP_SHFL_BFLY, { 3, IexExecUnit::PERMUTE } },
    { Opcode::OP_SHFLI_IDX, { 3, IexExecUnit::PERMUTE } },
    { Opcode::OP_SHI, { 3, IexExecUnit::UNDEF } },
    { Opcode::OP_SRA, { 3, IexExecUnit::IALU } },
};

const unordered_map<IexExecUnit, uint32_t> IexLatency::VEC_ITER_CYCLES = {
    { IexExecUnit::FDIV, 64 },
};

bool IexLatency::GetInstExLatency(const SimInst& inst, uint32_t& latency)
{
    ASSERT(inst != nullptr);
    const auto& latencyTable = IexLatency::VEC_LATENCY;
    const Opcode opcode = inst->opcode;
    if (latencyTable.find(opcode) != latencyTable.cend()) {
        latency = latencyTable.at(opcode).latency;
        return true;
    }
    return false;
}

string IexLatency::GetExecUnitString(IexExecUnit unit)
{
    string unitStr = "UNDEF";
    switch (unit) {
        case IexExecUnit::IALU:
            unitStr = "IALU";
            break;
        case IexExecUnit::FCVT:
            unitStr = "FCVT";
            break;
        case IexExecUnit::FDIV:
            unitStr = "FDIV";
            break;
        case IexExecUnit::FMLA:
            unitStr = "FMLA";
            break;
        case IexExecUnit::IMAC:
            unitStr = "IMAC";
            break;
        case IexExecUnit::PERMUTE:
            unitStr = "PERMUTE";
            break;
        default:
            break;
    }
    return unitStr;
}


} // namespace JCore
