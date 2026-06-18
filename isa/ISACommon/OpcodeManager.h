#pragma once
#ifndef OPCODE_MANAGER
#define OPCODE_MANAGER

#include <array>
#include <cassert>
#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include<iostream>

#include "InstGroup.h"

namespace JCore {
const uint64_t BYTE_SIZE = 1;
const uint64_t HALF_SIZE = 2;
const uint64_t WORD_SIZE = 4;
const uint64_t DOUBLE_SIZE = 8;

struct AccMemBase {
    uint64_t bytes = BYTE_SIZE;
    bool signedLoad = true; // load reuslt sign extension or unsigned extension
    bool loadStorePair = false;
    AccMemBase() = default;
    AccMemBase(uint64_t size, bool pair) : bytes(size), signedLoad(false), loadStorePair(pair) {}
    AccMemBase(uint64_t size, bool sign, bool pair) : bytes(size), signedLoad(sign), loadStorePair(pair) {}
};
using AccMemBasePtr = std::shared_ptr<AccMemBase>;
template<class... Args>
inline AccMemBasePtr MakeAccMem(Args&&... args) {
    return std::make_shared<AccMemBase>(std::forward<Args>(args)...);
}

enum class Opcode {
    OP_INVALID,

    /* ----------------------------Block instructions---------------------------- */
    OP_BSTOP,
    OP_BSTART,
    OP_BSTART_DIRECT,
    OP_BSTART_COND,
    OP_B_TEXT,
    OP_B_IOR,
    OP_B_IOD,
    OP_B_IOT,
    OP_B_ATTR,
    OP_B_CATR,
    OP_B_DATR,
    OP_B_HINT_PREFETCH,
    OP_B_HINT_TRACE,
    OP_B_DIM,
    OP_B_DIMI,
    OP_EBREAK,
    OP_XB,
    OP_MCOPY,
    OP_MSET,
    OP_FENTRY,
    OP_FEXIT,
    OP_FRET_RA,
    OP_FRET_STK,

    /* ----------------------------Execute instructions---------------------------- */
    // Group: Move
    OP_MOVR,
    OP_MOVI,

    // Group: Set Commit Argument
    OP_SETC_EQ,
    OP_SETC_NE,
    OP_SETC_AND,
    OP_SETC_OR,
    OP_SETC_LT,
    OP_SETC_GE,
    OP_SETC_LTU,
    OP_SETC_GEU,
    OP_SETC_EQI,
    OP_SETC_NEI,
    OP_SETC_ANDI,
    OP_SETC_ORI,
    OP_SETC_LTI,
    OP_SETC_GEI,
    OP_SETC_LTUI,
    OP_SETC_GEUI,

    // Group: Arithmetic
    OP_ADD,
    OP_SUB,
    OP_AND,
    OP_OR,
    OP_XOR,
    OP_SRL,
    OP_SRA,
    OP_SLL,
    OP_ADDI,
    OP_SUBI,
    OP_ANDI,
    OP_ORI,
    OP_XORI,
    OP_SRLI,
    OP_SRAI,
    OP_SLLI,
    OP_ADDW,
    OP_SUBW,
    OP_ANDW,
    OP_ORW,
    OP_XORW,
    OP_SRLW,
    OP_SRAW,
    OP_SLLW,
    OP_ADDIW,
    OP_SUBIW,
    OP_ANDIW,
    OP_ORIW,
    OP_XORIW,
    OP_SRLIW,
    OP_SRAIW,
    OP_SLLIW,

    // Group: Prefetch
    OP_PRF,
    OP_PRF_A,
    OP_PRFI_U,
    OP_PRFI_UA,

    // Group: Load
    // Group: Load Register Offset
    OP_LB,
    OP_LH,
    OP_LW,
    OP_LD,
    OP_LBU,
    OP_LHU,
    OP_LWU,
    // Group: Load Immediate Offset
    OP_LBI,
    OP_LHI,
    OP_LWI,
    OP_LDI,
    OP_LBUI,
    OP_LHUI,
    OP_LWUI,
    // Group: Load Immediate Offset UnScaled
    OP_LHI_U,
    OP_LWI_U,
    OP_LDI_U,
    OP_LHUI_U,
    OP_LWUI_U,
    // Group: Load Symbol(PC-Relative)
    OP_LB_PCR,
    OP_LH_PCR,
    OP_LW_PCR,
    OP_LD_PCR,
    OP_LBU_PCR,
    OP_LHU_PCR,
    OP_LWU_PCR,
    // 48 Bits Group: Load Pair
    OP_LBP,
    OP_LHP,
    OP_LWP,
    OP_LDP,
    OP_LBUP,
    OP_LHUP,
    OP_LWUP,
    OP_LBIP,
    OP_LHIP,
    OP_LWIP,
    OP_LDIP,
    OP_LBUIP,
    OP_LHUIP,
    OP_LWUIP,
    OP_LHIP_U,
    OP_LWIP_U,
    OP_LDIP_U,
    OP_LHUIP_U,
    OP_LWUIP_U,
    // 48 Bits Group: Load Pre-index
    OP_LB_PR,
    OP_LH_PR,
    OP_LW_PR,
    OP_LD_PR,
    OP_LBU_PR,
    OP_LHU_PR,
    OP_LWU_PR,
    OP_LBI_PR,
    OP_LHI_PR,
    OP_LWI_PR,
    OP_LDI_PR,
    OP_LBUI_PR,
    OP_LHUI_PR,
    OP_LWUI_PR,
    OP_LHI_UPR,
    OP_LWI_UPR,
    OP_LDI_UPR,
    OP_LHUI_UPR,
    OP_LWUI_UPR,
    // 48 Bits Group: Load Post-index
    OP_LB_PO,
    OP_LH_PO,
    OP_LW_PO,
    OP_LD_PO,
    OP_LBU_PO,
    OP_LHU_PO,
    OP_LWU_PO,
    OP_LBI_PO,
    OP_LHI_PO,
    OP_LWI_PO,
    OP_LDI_PO,
    OP_LBUI_PO,
    OP_LHUI_PO,
    OP_LWUI_PO,
    OP_LHI_UPO,
    OP_LWI_UPO,
    OP_LDI_UPO,
    OP_LHUI_UPO,
    OP_LWUI_UPO,
    // Group: Load Bridge Register Offset
    OP_LB_BRG,
    OP_LH_BRG,
    OP_LW_BRG,
    OP_LD_BRG,
    OP_LBU_BRG,
    OP_LHU_BRG,
    OP_LWU_BRG,
    // Group: Load Bridge Immediate Offset
    OP_LBI_BRG,
    OP_LHI_BRG,
    OP_LWI_BRG,
    OP_LDI_BRG,
    OP_LBUI_BRG,
    OP_LHUI_BRG,
    OP_LWUI_BRG,
    // Group: Load Bridge Instruction UnScaled
    OP_LHI_U_BRG,
    OP_LWI_U_BRG,
    OP_LDI_U_BRG,
    OP_LHUI_U_BRG,
    OP_LWUI_U_BRG,

    // Group: Store
    // Group: Store Register Offset
    OP_SB,
    OP_SH,
    OP_SW,
    OP_SD,
    OP_SH_U,
    OP_SW_U,
    OP_SD_U,
    // Group: Store Immediate Offset
    OP_SBI,
    OP_SHI,
    OP_SWI,
    OP_SDI,
    OP_SHI_U,
    OP_SWI_U,
    OP_SDI_U,
    // Group: Store Symbol(PC-Relative)
    OP_SB_PCR,
    OP_SH_PCR,
    OP_SW_PCR,
    OP_SD_PCR,
    // 48 bit Store Pair
    OP_SBP,
    OP_SHP,
    OP_SWP,
    OP_SDP,
    OP_SHP_U,
    OP_SWP_U,
    OP_SDP_U,
    OP_SBIP,
    OP_SHIP,
    OP_SWIP,
    OP_SDIP,
    OP_SHIP_U,
    OP_SWIP_U,
    OP_SDIP_U,
    // Store Pre-Index
    OP_SB_PR,
    OP_SH_PR,
    OP_SW_PR,
    OP_SD_PR,
    OP_SH_UPR,
    OP_SW_UPR,
    OP_SD_UPR,
    OP_SBI_PR,
    OP_SHI_PR,
    OP_SWI_PR,
    OP_SDI_PR,
    OP_SHI_UPR,
    OP_SWI_UPR,
    OP_SDI_UPR,
    // Store Post-Index
    OP_SB_PO,
    OP_SH_PO,
    OP_SW_PO,
    OP_SD_PO,
    OP_SH_UPO,
    OP_SW_UPO,
    OP_SD_UPO,
    OP_SBI_PO,
    OP_SHI_PO,
    OP_SWI_PO,
    OP_SDI_PO,
    OP_SHI_UPO,
    OP_SWI_UPO,
    OP_SDI_UPO,
    // Group: Store Bridge Register Offset
    OP_SB_BRG,
    OP_SH_BRG,
    OP_SW_BRG,
    OP_SD_BRG,
    OP_SH_U_BRG,
    OP_SW_U_BRG,
    OP_SD_U_BRG,
    // Group: Store Bridge Immediate Offset
    OP_SBI_BRG,
    OP_SHI_BRG,
    OP_SWI_BRG,
    OP_SDI_BRG,
    OP_SHI_U_BRG,
    OP_SWI_U_BRG,
    OP_SDI_U_BRG,

    // Group: Unary
    OP_SEXT_B,
    OP_SEXT_H,
    OP_SEXT_W,
    OP_ZEXT_B,
    OP_ZEXT_H,
    OP_ZEXT_W,

    // Group: Compare
    OP_CMP_EQ,
    OP_CMP_NE,
    OP_CMP_AND,
    OP_CMP_OR,
    OP_CMP_LT,
    OP_CMP_GE,
    OP_CMP_LTU,
    OP_CMP_GEU,
    OP_CMP_EQI,
    OP_CMP_NEI,
    OP_CMP_ANDI,
    OP_CMP_ORI,
    OP_CMP_LTI,
    OP_CMP_GEI,
    OP_CMP_LTUI,
    OP_CMP_GEUI,

    // Group: PC-Relative
    OP_ADDTPC,
    OP_SETRET,

    // Group: Immediate
    OP_LUI,

    // Group: Branch
    OP_B_EQ,
    OP_B_NE,
    OP_B_LT,
    OP_B_GE,
    OP_B_LTU,
    OP_B_GEU,
    OP_JR,
    OP_J,
    OP_B_Z,
    OP_B_NZ,

    // Group: Multi-Cycle
    OP_MUL,
    OP_MULU,
    OP_MULW,
    OP_MULUW,
    OP_MADD,
    OP_MADDW,
    OP_DIV,
    OP_DIVU,
    OP_DIVW,
    OP_DIVUW,
    OP_REM,
    OP_REMU,
    OP_REMW,
    OP_REMUW,

    // Group: Bit Operation
    OP_BXS,
    OP_BXU,
    OP_BIC,
    OP_BIS,
    OP_CTZ,
    OP_CLZ,
    OP_BCNT,
    OP_REV,

    // Group: Compound
    OP_CSEL,
    OP_PSEL,

    // Group: Atomic Operation
    OP_LR_B,
    OP_LR_H,
    OP_LR_W,
    OP_LR_D,
    OP_SC_B,
    OP_SC_H,
    OP_SC_W,
    OP_SC_D,
    OP_LW_ADD,
    OP_LW_AND,
    OP_LW_OR,
    OP_LW_XOR,
    OP_LW_MAX,
    OP_LW_MIN,
    OP_LW_SMAX,
    OP_LW_SMIN,
    OP_LW_UMAX,
    OP_LW_UMIN,
    OP_SW_ADD,
    OP_SW_AND,
    OP_SW_OR,
    OP_SW_XOR,
    OP_SW_MAX,
    OP_SW_MIN,
    OP_SW_SMAX,
    OP_SW_SMIN,
    OP_SW_UMAX,
    OP_SW_UMIN,
    OP_LD_ADD,
    OP_LD_AND,
    OP_LD_OR,
    OP_LD_XOR,
    OP_LD_MAX,
    OP_LD_MIN,
    OP_LD_SMAX,
    OP_LD_SMIN,
    OP_LD_UMAX,
    OP_LD_UMIN,
    OP_SD_ADD,
    OP_SD_AND,
    OP_SD_OR,
    OP_SD_XOR,
    OP_SD_MAX,
    OP_SD_MIN,
    OP_SD_SMAX,
    OP_SD_SMIN,
    OP_SD_UMAX,
    OP_SD_UMIN,
    OP_SWAPB,
    OP_SWAPH,
    OP_SWAPW,
    OP_SWAPD,
    OP_CASB,
    OP_CASH,
    OP_CASW,
    OP_CASD,
    OP_CASBP,
    OP_CASHP,
    OP_CASWP,
    OP_CASDP,
    OP_DMA,

    // Group: Execution Control
    OP_BSE,
    OP_BWE,
    OP_BWI,
    OP_BWT,
    OP_ASSERT,
    OP_DSB,
    OP_ISB,
    OP_ACRC,
    OP_ACRE,

    // Group: Cache Maintain
    OP_BC_IVA,
    OP_BC_IALL,
    OP_IC_IVA,
    OP_IC_IALL,
    OP_DC_IVA,
    OP_DC_IALL,
    OP_DC_CVA,
    OP_DC_CIVA,
    OP_DC_ISW,
    OP_DC_CSW,
    OP_DC_CISW,
    OP_DC_ZVA,
    OP_TLB_IA,
    OP_TLB_IV,
    OP_TLB_IAV,
    OP_TLB_IALL,

    // Group: SSR Aaccelss
    OP_SSRGET,
    OP_SSRSET,
    OP_SSRSWAP,
    OP_LSRGET,
    OP_SETC_TGT,

    // Group: Floating-Point
    OP_FADD,
    OP_FSUB,
    OP_FMUL,
    OP_FDIV,
    OP_FMADD,
    OP_FMSUB,
    OP_FNMADD,
    OP_FNMSUB,
    OP_FEQ,
    OP_FNE,
    OP_FLT,
    OP_FGE,
    OP_FEQS,
    OP_FNES,
    OP_FLTS,
    OP_FGES,
    OP_FABS,
    OP_FSQRT,
    OP_FRECIP,
    OP_FEXP,
    OP_FCLASS,

    // Group: Max-Min
    OP_MAX,
    OP_MAXU,
    OP_MIN,
    OP_MINU,
    OP_FMAX,
    OP_FMIN,

    // Group: Format Convert
    OP_FCVT,
    OP_FCVTA,
    OP_FCVTI,
    OP_FCVTM,
    OP_FCVTN,
    OP_FCVTP,
    OP_FCVTZ,
    OP_SCVTF,
    OP_UCVTF,
    OP_ICVT,
    OP_ICVTF,

    // Group: Long Immediate
    OP_LIS,
    OP_LIU,
    OP_ADDLI,

    // Group:
    OP_MIADD,
    OP_MISUB,
    OP_BFI,

    // Group: Concat
    OP_CCAT,
    OP_CCATW,

    // Group: Queue
    OP_QMT,
    OP_QPUSH,
    OP_QPOP,

    // Group: Reduce
    OP_RDADD,
    OP_RDAND,
    OP_RDOR,
    OP_RDXOR,
    OP_RDFADD,
    OP_RDMAX,
    OP_RDMIN,
    OP_RDFMAX,
    OP_RDFMIN,

    // Group: Shuffle
    OP_SHFL_UP,
    OP_SHFL_DOWN,
    OP_SHFL_BFLY,
    OP_SHFL_IDX,
    OP_SHFLI_UP,
    OP_SHFLI_DOWN,
    OP_SHFLI_BFLY,
    OP_SHFLI_IDX,

    // Group: Special Operations
    OP_MOV,

    // OP_NUMBER must be placed at the end.
    OP_TLD,
    OP_TSD,
    OP_NUMBER,

};

class OpcodeManager {
public:
    static OpcodeManager &Inst()
    {
        static OpcodeManager inst;
        return inst;
    }

    bool HasOpcode(Opcode opcode) const
    {
        return static_cast<int>(opcode) >= 0 && static_cast<size_t>(opcode) < opcodeInfos_.size();
    }
    bool HasOpcode(const std::string &str) const
    {
        return strToEnum_.count(str) > 0;
    }

    Opcode GetOpcode(const std::string &str) const
    {
        auto it = strToEnum_.find(str);
        assert(it != strToEnum_.end());
        return it->second;
    }
    const std::string &GetOpcodeStr(Opcode opcode) const
    {
        assert(HasOpcode(opcode));
        return opcodeInfos_[static_cast<int>(opcode)].str;
    }

    const InstGroup &GetOpcodeGroup(Opcode opcode) const
    {
        assert(HasOpcode(opcode));
        return opcodeInfos_[static_cast<int>(opcode)].group;
    }

    bool IsOpcodeSrcRNeedMerge(Opcode opcode) const
    {
        assert(HasOpcode(opcode));
        return opcodeInfos_[static_cast<int>(opcode)].srcRNeedMerge;
    }

    const AccMemBasePtr &GetOpcodeAccMemBaseInfo(Opcode opcode) const
    {
        assert(HasOpcode(opcode));
        return opcodeInfos_[static_cast<int>(opcode)].accMemBaseInfo;
    }

    std::string PrintSupportOpcodes() const
    {
        std::stringstream ss;
        ss << "[";
        bool isFirst = true;
        for (const auto &info : opcodeInfos_) {
            if (!isFirst) {
                ss << ", ";
            }
            isFirst = false;
            ss << info.str;
        }
        ss << "]";
        return ss.str();
    }

    bool IsNeedAccumulateBlockInfo(Opcode opcode) const;

private:
    struct OpcodeInfo {
        Opcode opcode = Opcode::OP_INVALID;
        std::string str = "";
        InstGroup group = InstGroup::UNDEF;
        bool srcRNeedMerge = false;
        AccMemBasePtr accMemBaseInfo = nullptr;
        OpcodeInfo() = default;
        explicit OpcodeInfo(Opcode opcode_, std::string name, InstGroup grp)
            : opcode(opcode_), str(std::move(name)), group(grp) {}
        explicit OpcodeInfo(Opcode op, std::string name, InstGroup grp, bool srcR)
            : OpcodeInfo(op, std::move(name), grp)
        {
            srcRNeedMerge = srcR;
        }
        explicit OpcodeInfo(Opcode op, std::string name, InstGroup grp, AccMemBasePtr memAttr)
            : OpcodeInfo(op, std::move(name), grp)
        {
            accMemBaseInfo = memAttr;
        }
        explicit OpcodeInfo(Opcode op, std::string name, InstGroup grp, bool srcR, AccMemBasePtr memAttr)
            : OpcodeInfo(op, std::move(name), grp, srcR)
        {
            accMemBaseInfo = memAttr;
        }
    };
private:
    OpcodeManager();

    // Register the information for each opcode; currently, only the name has been implemented.
    std::unordered_set<Opcode> registered;
    // void RegisterInfo(Opcode opcode, std::string str, InstGroup grp, AccMemBasePtr accMem = nullptr);
    void RegisterInfo(OpcodeInfo info);
    void RegisterOpcode();
    void RegisterBlockSplitOpcode();
    void RegisterArithmeticOpcode();
    void RegisterArithmeticFPOpcode();
    void RegisterCompareOpcode();
    void RegisterCompareFPOpcode();
    void RegisterSetcOpcode();
    void RegisterPCRelativeOpcode();
    void RegisterImmediateOpcode();
    void RegisterMoveOpcode();
    void RegisterBranchOpcode();
    void RegisterMultiCycleOpcode();
    void RegisterBitOpcode();
    void RegisterCommpoundOpcode();
    void RegisterLoadOpcode();
    void RegisterStoreOpcode();
    void RegisterPrefetchOpcode();
    void RegisterAtomicOpcode();
    void RegisterExecuteControlOpcode();
    void RegisterExtendOpcode();
    void RegisterCacheMaintainOpcode();
    void RegisterGMOOpcode();
    void RegisterSSROpcode();
    void RegisterMaxMinOpcode();
    void RegisterConvertOpcode();
    void RegisterReduceOpcode();
    void RegisterShuffleOpcode();
    void RegisterSpecialOpcode();

private:
    std::array<OpcodeInfo, static_cast<int>(Opcode::OP_NUMBER)> opcodeInfos_{};
    std::unordered_map<std::string, Opcode> strToEnum_;
};

inline std::string GetOpcodeName(Opcode opcode)
{
    return OpcodeManager::Inst().GetOpcodeStr(opcode);
}

inline bool OpcodeIsFp(Opcode opcode)
{
    return OpcodeManager::Inst().GetOpcodeGroup(opcode) == InstGroup::ARITHMETIC_FP ||
        OpcodeManager::Inst().GetOpcodeGroup(opcode) == InstGroup::COMPARE_FP;
}

inline bool OpcodeIsLoad(Opcode opcode)
{
    return OpcodeManager::Inst().GetOpcodeGroup(opcode) == InstGroup::LOAD;
}

inline bool OpcodeIsStore(Opcode opcode)
{
    return OpcodeManager::Inst().GetOpcodeGroup(opcode) == InstGroup::STORE;
}

inline bool OpcodeInInstGroup(Opcode opcode, InstGroup tgt)
{
    return OpcodeManager::Inst().GetOpcodeGroup(opcode) == tgt;
}

inline bool OpcodeIsCMD(Opcode opcode)
{
    return OpcodeInInstGroup(opcode, InstGroup::BLOCK_SPLIT) || OpcodeInInstGroup(opcode, InstGroup::BLOCK_OFFSET) ||
           OpcodeInInstGroup(opcode, InstGroup::BLOCK_IO) || OpcodeInInstGroup(opcode, InstGroup::BLOCK_ATTR) ||
           OpcodeInInstGroup(opcode, InstGroup::BLOCK_HINT) || OpcodeInInstGroup(opcode, InstGroup::BLOCK_ARGUMENT);
}

inline uint64_t GetLoadStoreBytes(Opcode opcode)
{
    return OpcodeManager::Inst().GetOpcodeAccMemBaseInfo(opcode)->bytes;
}

inline bool IsSignedLoad(Opcode opcode)
{
    return OpcodeManager::Inst().GetOpcodeAccMemBaseInfo(opcode)->signedLoad;
}

inline bool IsLoadStorePair(Opcode opcode)
{
    return OpcodeManager::Inst().GetOpcodeAccMemBaseInfo(opcode)->loadStorePair;
}

inline bool OpcodeIsBDIM(Opcode opcode)
{
    static const std::unordered_set<Opcode> OPCODEISBDIM_OPCODES {
        Opcode::OP_B_DIM, Opcode::OP_B_DIMI,
    };
    return OPCODEISBDIM_OPCODES.count(opcode) != 0;
}

inline bool OpcodeIsDirectInnerJump(Opcode opcode)
{
    static const std::unordered_set<Opcode> DIRECT_INNER_JUMP_OPCODES {
        Opcode::OP_J, Opcode::OP_JR,
    };
    return DIRECT_INNER_JUMP_OPCODES.count(opcode) != 0;
}

inline bool OpcodeIsCondInnerJump(Opcode opcode)
{
    static const std::unordered_set<Opcode> COND_INNER_JUMP_OPCODES {
        Opcode::OP_B_EQ, Opcode::OP_B_NE, Opcode::OP_B_LT, Opcode::OP_B_GE, Opcode::OP_B_LTU,
        Opcode::OP_B_GEU,
    };
    return COND_INNER_JUMP_OPCODES.count(opcode) != 0;
}

inline bool OpcodeIsInnerJump(Opcode opcode)
{
    return OpcodeIsDirectInnerJump(opcode) || OpcodeIsCondInnerJump(opcode);
}

inline bool OpcodeIsMovr(Opcode opcode)
{
    static const std::unordered_set<Opcode> OPCODEISMOVR_OPCODES {
        Opcode::OP_ADDI, Opcode::OP_SUBI, Opcode::OP_MOVR
    };
    return OPCODEISMOVR_OPCODES.count(opcode) != 0;
}

inline bool OpcodeCiscUper(Opcode opcode)
{
    static const std::unordered_set<Opcode> OPCODE_CISC_UPER_OPCODES {
        Opcode::OP_ADD, Opcode::OP_ADDI, Opcode::OP_SUB, Opcode::OP_SUBI, Opcode::OP_ORI,
    };
    return OPCODE_CISC_UPER_OPCODES.count(opcode) != 0;
}

inline bool OpcodeIsMoveType(Opcode opcode)
{
    static const std::unordered_set<Opcode> MOVE_TYPE_OPCODES {
        Opcode::OP_ADDI, Opcode::OP_SUBI, Opcode::OP_ORI,
    };
    return MOVE_TYPE_OPCODES.count(opcode) != 0;
}

inline bool OpcodeCanBeMov0(Opcode opcode)
{
    static const std::unordered_set<Opcode> OPCODECANBEMOV0_OPCODES {
        Opcode::OP_ORI, Opcode::OP_ANDI, Opcode::OP_SRLI, Opcode::OP_SRAI, Opcode::OP_SLLI,
    };
    return OPCODECANBEMOV0_OPCODES.count(opcode) != 0;
}

inline bool OpcodeIsLoadReg(Opcode opcode)
{
    static const std::unordered_set<Opcode> LOAD_REG_OPCODES {
        Opcode::OP_LB, Opcode::OP_LH, Opcode::OP_LW, Opcode::OP_LD, Opcode::OP_LBU, Opcode::OP_LHU, Opcode::OP_LWU,
    };
    return LOAD_REG_OPCODES.count(opcode) != 0;
}

inline bool OpcodeIsStoreReg(Opcode opcode)
{
    const std::unordered_set<Opcode> STORE_REG_OPCODES {
        Opcode::OP_SB, Opcode::OP_SH, Opcode::OP_SW, Opcode::OP_SD, Opcode::OP_SH_U, Opcode::OP_SW_U, Opcode::OP_SD_U,
    };
    return STORE_REG_OPCODES.count(opcode) != 0;
}

inline bool OpcodeIsLoadImm(Opcode opcode)
{
    static const std::unordered_set<Opcode> LOAD_IMM_OPCODES {
        Opcode::OP_LBI, Opcode::OP_LHI, Opcode::OP_LWI, Opcode::OP_LDI, Opcode::OP_LBUI, Opcode::OP_LHUI,
        Opcode::OP_LWUI, Opcode::OP_LR_D, Opcode::OP_LR_W, Opcode::OP_LHI_U, Opcode::OP_LWI_U, Opcode::OP_LDI_U,
        Opcode::OP_LHUI_U, Opcode::OP_LWUI_U,
    };
    return LOAD_IMM_OPCODES.count(opcode) != 0;
}

inline bool OpcodeIsSetcEqNe(Opcode opcode)
{
    const std::unordered_set<Opcode> SETC_EQ_NE_OPCODES {
        Opcode::OP_SETC_EQ, Opcode::OP_SETC_NE,
    };
    return SETC_EQ_NE_OPCODES.count(opcode) != 0;
}

inline bool OpcodeIsSetc(Opcode opcode)
{
    return OpcodeInInstGroup(opcode, InstGroup::SETC) || opcode == Opcode::OP_SETC_TGT;
}

inline bool OpcodeIsVCvt(Opcode opcode)
{
    static const std::unordered_set<Opcode> OPCODEISVCVT_OPCODES = {
        Opcode::OP_FCVT, Opcode::OP_FCVTI, Opcode::OP_ICVT, Opcode::OP_ICVTF
    };
    return OPCODEISVCVT_OPCODES.count(opcode) != 0;
}
inline bool OpcodeIsVFp(Opcode opcode)
{
    static const std::unordered_set<Opcode> OPCODEISVFP_OPCODES = {
        Opcode::OP_FADD, Opcode::OP_FMUL, Opcode::OP_FMADD, Opcode::OP_FSUB,
        Opcode::OP_FEQ, Opcode::OP_FNE, Opcode::OP_FLT, Opcode::OP_FGE, Opcode::OP_FEQS, Opcode::OP_FNES,
        Opcode::OP_FLTS, Opcode::OP_FGES, Opcode::OP_FMAX, Opcode::OP_FMIN,
    };

    return OPCODEISVFP_OPCODES.count(opcode) != 0;
}
inline bool OpcodeIsDivSqrt(Opcode opcode)
{
    static const std::unordered_set<Opcode> OPCODEISDIVSQRT_OPCODES = {
        Opcode::OP_FDIV, Opcode::OP_FSQRT, Opcode::OP_REM,
    };

    return OPCODEISDIVSQRT_OPCODES.count(opcode) != 0;
}

inline bool OpcodeIsShf(Opcode opcode)
{
    const std::unordered_set<Opcode> OPCODEISSHUFFLE_OPCODES = {
        Opcode::OP_SHFL_BFLY, Opcode::OP_SHFL_DOWN, Opcode::OP_SHFL_UP, Opcode::OP_SHFL_IDX,
        Opcode::OP_SHFLI_BFLY, Opcode::OP_SHFLI_DOWN, Opcode::OP_SHFLI_UP, Opcode::OP_SHFLI_IDX,
    };
    return OPCODEISSHUFFLE_OPCODES.count(opcode) != 0;
}

inline bool CanbeMovi(Opcode opcode)
{
    static const std::unordered_set<Opcode> CANBEMOVI_OPCODES {
        Opcode::OP_ADDI, Opcode::OP_SUBI, Opcode::OP_ORI, Opcode::OP_XORI, Opcode::OP_SRLI, Opcode::OP_SRAI,
        Opcode::OP_SLLI,
    };
    return CANBEMOVI_OPCODES.count(opcode) != 0;
}

inline bool CanbeMovr(Opcode opcode)
{
    const std::unordered_set<Opcode> CANBEMOVR_OPCODES {
        Opcode::OP_ADD, Opcode::OP_SUB, Opcode::OP_OR, Opcode::OP_XOR,
        Opcode::OP_SRL, Opcode::OP_SRA, Opcode::OP_SLL,
    };
    return CANBEMOVR_OPCODES.count(opcode) != 0;
}

inline bool OpcodeNotCheck(Opcode opcode)
{
    return opcode == Opcode::OP_BSTOP || OpcodeManager::Inst().IsNeedAccumulateBlockInfo(opcode);
}

inline bool OpcodeIsDCZVA(Opcode opcode)
{
    return opcode == Opcode::OP_DC_ZVA;
}

} // namespace JCore
#endif
