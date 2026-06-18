#include "Load.h"

namespace JCore {
namespace Calculate {
namespace Load {
/*
 * Uniformly placing the aaccelss addresses on dsts[DST0_IDX].
 * After the memory returns the data, it overwrites the addresses.
 * load PR and load PO type operation: ISA definition requires that the address be placed in dsts[DST1_IDX],
 * so both dsts[DST0_IDX] and dsts[DST1_IDX] need to be written
 */

static bool CalcVectorLoadAddr(MInst &inst)
{
    if (inst.accMemInfo->continuous) {
        if (inst.srcs.size() != SRC3_IDX || inst.dsts.size() != DST1_IDX) {
            return false;
        }
        inst.accMemInfo->accMemAddr = inst.srcs[SRC0_IDX]->data + inst.srcs[SRC1_IDX]->data + inst.srcs[SRC2_IDX]->data;
    } else {
        if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
            return false;
        }
        inst.accMemInfo->accMemAddr = inst.srcs[SRC0_IDX]->data + inst.srcs[SRC1_IDX]->data;
    }
    return true;
}

static bool CalcLoadAddr(MInst &inst)
{
    if (inst.codeLen == EncodeLen::ENL_V) {
        return CalcVectorLoadAddr(inst);
    }
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    inst.accMemInfo->accMemAddr = inst.srcs[SRC0_IDX]->data + inst.srcs[SRC1_IDX]->data;
    return true;
}

static bool CalcLoadPCRAddr(MInst &inst)
{
    if (inst.srcs.size() != SRC1_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    inst.accMemInfo->accMemAddr = inst.pc + inst.srcs[SRC0_IDX]->data;
    return true;
}

static bool CalcLoadPairAddr(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST2_IDX) {
        return false;
    }
    inst.accMemInfo->accMemAddr = inst.srcs[SRC0_IDX]->data + inst.srcs[SRC1_IDX]->data;
    return true;
}

static bool CalcLoadPRAddr(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST2_IDX) {
        return false;
    }
    inst.accMemInfo->accMemAddr = inst.srcs[SRC0_IDX]->data + inst.srcs[SRC1_IDX]->data;
    inst.dsts[DST1_IDX]->data = inst.srcs[SRC0_IDX]->data + inst.srcs[SRC1_IDX]->data;
    return true;
}

static bool CalcLoadPOAddr(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST2_IDX) {
        return false;
    }
    inst.accMemInfo->accMemAddr = inst.srcs[SRC0_IDX]->data;
    inst.dsts[DST1_IDX]->data = inst.srcs[SRC0_IDX]->data + inst.srcs[SRC1_IDX]->data;
    return true;
}

Handler Lookup(Opcode op)
{
    static const std::unordered_map<Opcode, Handler> LOAD_TABLE = std::unordered_map<Opcode, Handler> {
        // Group: Load Register Offset
        {Opcode::OP_LB, &CalcLoadAddr},
        {Opcode::OP_LH, &CalcLoadAddr},
        {Opcode::OP_LW, &CalcLoadAddr},
        {Opcode::OP_LD, &CalcLoadAddr},
        {Opcode::OP_LBU, &CalcLoadAddr},
        {Opcode::OP_LHU, &CalcLoadAddr},
        {Opcode::OP_LWU, &CalcLoadAddr},
        // Group: Load Immediate Offset
        {Opcode::OP_LBI, &CalcLoadAddr},
        {Opcode::OP_LHI, &CalcLoadAddr},
        {Opcode::OP_LWI, &CalcLoadAddr},
        {Opcode::OP_LDI, &CalcLoadAddr},
        {Opcode::OP_LBUI, &CalcLoadAddr},
        {Opcode::OP_LHUI, &CalcLoadAddr},
        {Opcode::OP_LWUI, &CalcLoadAddr},
        // Group: Load Immediate Offset UnScaled
        {Opcode::OP_LHI_U, &CalcLoadAddr},
        {Opcode::OP_LWI_U, &CalcLoadAddr},
        {Opcode::OP_LDI_U, &CalcLoadAddr},
        {Opcode::OP_LHUI_U, &CalcLoadAddr},
        {Opcode::OP_LWUI_U, &CalcLoadAddr},
        // Group: Load Symbol-PC-Relative
        {Opcode::OP_LB_PCR, &CalcLoadPCRAddr},
        {Opcode::OP_LH_PCR, &CalcLoadPCRAddr},
        {Opcode::OP_LW_PCR, &CalcLoadPCRAddr},
        {Opcode::OP_LD_PCR, &CalcLoadPCRAddr},
        {Opcode::OP_LBU_PCR, &CalcLoadPCRAddr},
        {Opcode::OP_LHU_PCR, &CalcLoadPCRAddr},
        {Opcode::OP_LWU_PCR, &CalcLoadPCRAddr},
        // 48 Bits Group: Load Pair
        {Opcode::OP_LBP, &CalcLoadPairAddr},
        {Opcode::OP_LHP, &CalcLoadPairAddr},
        {Opcode::OP_LWP, &CalcLoadPairAddr},
        {Opcode::OP_LDP, &CalcLoadPairAddr},
        {Opcode::OP_LBUP, &CalcLoadPairAddr},
        {Opcode::OP_LHUP, &CalcLoadPairAddr},
        {Opcode::OP_LWUP, &CalcLoadPairAddr},
        {Opcode::OP_LBIP, &CalcLoadPairAddr},
        {Opcode::OP_LHIP, &CalcLoadPairAddr},
        {Opcode::OP_LWIP, &CalcLoadPairAddr},
        {Opcode::OP_LDIP, &CalcLoadPairAddr},
        {Opcode::OP_LBUIP, &CalcLoadPairAddr},
        {Opcode::OP_LHUIP, &CalcLoadPairAddr},
        {Opcode::OP_LWUIP, &CalcLoadPairAddr},
        {Opcode::OP_LHIP_U, &CalcLoadPairAddr},
        {Opcode::OP_LWIP_U, &CalcLoadPairAddr},
        {Opcode::OP_LDIP_U, &CalcLoadPairAddr},
        {Opcode::OP_LHUIP_U, &CalcLoadPairAddr},
        {Opcode::OP_LWUIP_U, &CalcLoadPairAddr},
        // 48 Bits Group: Load Pre-index
        {Opcode::OP_LB_PR, &CalcLoadPRAddr},
        {Opcode::OP_LH_PR, &CalcLoadPRAddr},
        {Opcode::OP_LW_PR, &CalcLoadPRAddr},
        {Opcode::OP_LD_PR, &CalcLoadPRAddr},
        {Opcode::OP_LBU_PR, &CalcLoadPRAddr},
        {Opcode::OP_LHU_PR, &CalcLoadPRAddr},
        {Opcode::OP_LWU_PR, &CalcLoadPRAddr},
        {Opcode::OP_LBI_PR, &CalcLoadPRAddr},
        {Opcode::OP_LHI_PR, &CalcLoadPRAddr},
        {Opcode::OP_LWI_PR, &CalcLoadPRAddr},
        {Opcode::OP_LDI_PR, &CalcLoadPRAddr},
        {Opcode::OP_LBUI_PR, &CalcLoadPRAddr},
        {Opcode::OP_LHUI_PR, &CalcLoadPRAddr},
        {Opcode::OP_LWUI_PR, &CalcLoadPRAddr},
        {Opcode::OP_LHI_UPR, &CalcLoadPRAddr},
        {Opcode::OP_LWI_UPR, &CalcLoadPRAddr},
        {Opcode::OP_LDI_UPR, &CalcLoadPRAddr},
        {Opcode::OP_LHUI_UPR, &CalcLoadPRAddr},
        {Opcode::OP_LWUI_UPR, &CalcLoadPRAddr},
        // 48 Bits Group: Load Post-index
        {Opcode::OP_LB_PO, &CalcLoadPOAddr},
        {Opcode::OP_LH_PO, &CalcLoadPOAddr},
        {Opcode::OP_LW_PO, &CalcLoadPOAddr},
        {Opcode::OP_LD_PO, &CalcLoadPOAddr},
        {Opcode::OP_LBU_PO, &CalcLoadPOAddr},
        {Opcode::OP_LHU_PO, &CalcLoadPOAddr},
        {Opcode::OP_LWU_PO, &CalcLoadPOAddr},
        {Opcode::OP_LBI_PO, &CalcLoadPOAddr},
        {Opcode::OP_LHI_PO, &CalcLoadPOAddr},
        {Opcode::OP_LWI_PO, &CalcLoadPOAddr},
        {Opcode::OP_LDI_PO, &CalcLoadPOAddr},
        {Opcode::OP_LBUI_PO, &CalcLoadPOAddr},
        {Opcode::OP_LHUI_PO, &CalcLoadPOAddr},
        {Opcode::OP_LWUI_PO, &CalcLoadPOAddr},
        {Opcode::OP_LHI_UPO, &CalcLoadPOAddr},
        {Opcode::OP_LWI_UPO, &CalcLoadPOAddr},
        {Opcode::OP_LDI_UPO, &CalcLoadPOAddr},
        {Opcode::OP_LHUI_UPO, &CalcLoadPOAddr},
        {Opcode::OP_LWUI_UPO, &CalcLoadPOAddr},
        // Group: Load Bridge
        {Opcode::OP_LB_BRG, &CalcVectorLoadAddr},
        {Opcode::OP_LH_BRG, &CalcVectorLoadAddr},
        {Opcode::OP_LW_BRG, &CalcVectorLoadAddr},
        {Opcode::OP_LD_BRG, &CalcVectorLoadAddr},
        {Opcode::OP_LBU_BRG, &CalcVectorLoadAddr},
        {Opcode::OP_LHU_BRG, &CalcVectorLoadAddr},
        {Opcode::OP_LWU_BRG, &CalcVectorLoadAddr},
        {Opcode::OP_LBI_BRG, &CalcVectorLoadAddr},
        {Opcode::OP_LHI_BRG, &CalcVectorLoadAddr},
        {Opcode::OP_LWI_BRG, &CalcVectorLoadAddr},
        {Opcode::OP_LDI_BRG, &CalcVectorLoadAddr},
        {Opcode::OP_LBUI_BRG, &CalcVectorLoadAddr},
        {Opcode::OP_LHUI_BRG, &CalcVectorLoadAddr},
        {Opcode::OP_LWUI_BRG, &CalcVectorLoadAddr},
        {Opcode::OP_LHI_U_BRG, &CalcVectorLoadAddr},
        {Opcode::OP_LWI_U_BRG, &CalcVectorLoadAddr},
        {Opcode::OP_LDI_U_BRG, &CalcVectorLoadAddr},
        {Opcode::OP_LHUI_U_BRG, &CalcVectorLoadAddr},
        {Opcode::OP_LWUI_U_BRG, &CalcVectorLoadAddr},
    };
    auto it = LOAD_TABLE.find(op);
    return (it == LOAD_TABLE.end()) ? 0 : it->second;
}

bool CalculateLoad(MInst &inst)
{
    Handler fn = Lookup(inst.opcode);
    return fn ? fn(inst) : false;
}

} // namespace Load
} // namespace Calculate
} // namespace JCore