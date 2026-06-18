#include <cmath>

#include "bctrl/BCtrl.h"
#include "bctrl/spe/CodeTemplate.h"
#include "core/Core.h"
#include "DFX/InstTracer.h"

namespace JCore {


using namespace std;

Opcode genOpcode(bool isLoad, uint32_t aligned)
{
    Opcode opcode = Opcode::OP_INVALID;
    if (isLoad) {
        switch (aligned) {
            case 8:
                opcode = Opcode::OP_LDI;
                break;
            case 4:
                opcode = Opcode::OP_LWI;
                break;
            case 2:
                opcode = Opcode::OP_LHI;
                break;
            case 1:
                opcode = Opcode::OP_LBI;
                break;
            default:
                cerr << "aligned is: " << dec << aligned << endl;
                ASSERT(0 && "Not support!");
        }
        return opcode;
    }

    switch (aligned) {
        case 8:
            opcode = Opcode::OP_SDI_U;
            break;
        case 4:
            opcode = Opcode::OP_SWI_U;
            break;
        case 2:
            opcode = Opcode::OP_SHI_U;
            break;
        case 1:
            opcode = Opcode::OP_SBI;
            break;
        default:
            cerr << "aligned is: " << dec << aligned << endl;
            ASSERT(0 && "Not support!");
    }

    return opcode;
}

SimInst GenUInst(bool isLoad, ROBID bid, uint64_t bpc, uint32_t stid, uint32_t width)
{
    SimInst uInst = std::make_shared<SimInstInfo>();
    uInst->opcode = genOpcode(isLoad, width);

    uInst->bid = bid;
    uInst->bpc = bpc;
    uInst->stid = stid;
    uInst->storeSplit = false;
    return uInst;
}

bool CodeTemplate::GenMemSetTemplate()
{
    BlockCommandPtr cmd = ctBlock;
    switch (curStage) {
        case GenMemStage::START:
            return genMsetRequstToRegData(cmd);
        case GenMemStage::WAIT_RF:
            ReadRf(cmd);
            return false;
        case GenMemStage::GEN_INS:
            return GenMemSetInst();
        case GenMemStage::COMPLETE:
            return false;
        default:
            return false;
    }
    return false;
}

bool CodeTemplate::GenMemCopyTemplate()
{
    BlockCommandPtr cmd = ctBlock;
    switch (curStage) {
        case GenMemStage::START:
            return genMcopyRequstToRegData(cmd);
        case GenMemStage::WAIT_RF:
            ReadRf(cmd);
            // Next cycle to continue
            return false;
        case GenMemStage::GEN_INS:
            return GenMemCopyInst();
        case GenMemStage::COMPLETE:
            nextStage = GenMemStage::START;
            nextMemcopyStage = GenMemCopyStage::GEN_ALIGN;
            ctBlock = std::shared_ptr<BlockCommand>(nullptr);
            return false;
        default:
            break;
    }
    return false;
}

bool CodeTemplate::GenMemCopyInst()
{
    switch (curMemcopyStage) {
        case GenMemCopyStage::GEN_ALIGN:
            return GenMemcopyAlignInst();
        case GenMemCopyStage::GEN_FOLLOW:
            GenMemcopyFollowInst();
            // Next cycle to continue
            return false;
        case GenMemCopyStage::GEN_ENDING:
            return GenMemcopyEndingInst();
        default:
            return false;
    }

    return false;
}

bool CodeTemplate::GenMemcopyAlignInst()
{
    BlockCommandPtr cmd = ctBlock;
    LOG_DEBUG_M(Unit::BCC, Stage::D1) << __FUNCTION__;

    for (uint32_t i = 0; i < GetSim()->core->bctrl->configs.bctrl_bandwidth; i++) {
        if (genCntAtCyc == GetSim()->core->bctrl->configs.bctrl_bandwidth) break;
        SimInst inst = GenTemplateInitSimInst(cmd->stid);
        if (!alignInstQ.Empty()) {
            inst = alignInstQ.Read();
        } else {
            break;
        }
        RecordGenCycle(inst);
        inst->autogen = false;
        if (!GenSameTemplate(inst, __FUNCTION__)) {
            break;
        }

        // split the last sd in the first phase
        if (OpcodeIsStore(inst->opcode)) {
            inst->storeSplit = true;
        }

        if (inst->isLastInBlock) {
            nextStage = GenMemStage::START;
            nextMemcopyStage = GenMemCopyStage::GEN_ALIGN;
            ctBlock = std::shared_ptr<BlockCommand>(nullptr);
            break;
        }
    }
    if (alignInstQ.Empty()) nextMemcopyStage = GenMemCopyStage::GEN_FOLLOW;
    return nextMemcopyStage != GenMemCopyStage::GEN_ALIGN;
}

bool CodeTemplate::GenMemcopyFollowInst()
{
    LOG_INFO_M(Unit::SPE, Stage::D1) << __FUNCTION__;
    BlockCommandPtr cmd = ctBlock;
    auto &ldCnt = ct_ldCnt;
    auto &stCnt = ct_stCnt;
    auto &ldOffset = srcOffset;
    auto &stOffset = dstOffset;
    auto &totalOffset = srcTotalOffset;
    if (totalOffset == 0) totalOffset = ldOffset;
    uint32_t src_bits = nextRegData[static_cast<int>(MemCopyRegType::MCOPY_SRC_ADDR)];
    uint32_t dst_bits = nextRegData[static_cast<int>(MemCopyRegType::MCOPY_DST_ADDR)];
    uint32_t ldMaxOffset = 2032;
    // uint32_t stMaxOffset = 56;
    uint32_t src_3_bits = nextRegData[static_cast<int>(MemCopyRegType::MCOPY_SRC_ADDR)] & 0x7;
    uint32_t dst_3_bits = nextRegData[static_cast<int>(MemCopyRegType::MCOPY_DST_ADDR)] & 0x7;
    for (uint32_t i = 0; i < GetSim()->core->bctrl->configs.bctrl_bandwidth; i++) {
        if (genCntAtCyc == GetSim()->core->bctrl->configs.bctrl_bandwidth) break;
        if (ldCnt == 0 && followStatus == MemCopyStatus::FOLLOW_LD) break;
        SimInst inst = GenTemplateInitSimInst(cmd->stid);
        if (followStatus == MemCopyStatus::FOLLOW_MAX) {
            if (stOffset != 0) {
                // generate(addi u#2, ct.stOt, ->u);
                inst->opcode = Opcode::OP_ADDI;
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, stOffset);
                GenTemplateInstDst(inst, OperandType::OPD_ULINK, 0);
                stOffset = 0;
            } else {
                // generate(addi u#2, ct.ldOt, ->u);
                inst->opcode = Opcode::OP_ADDI;
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, ldOffset);
                GenTemplateInstDst(inst, OperandType::OPD_ULINK, 0);
                ldOffset = 0;
                followStatus = MemCopyStatus::FOLLOW_LD;
                if (ldCnt == 0 && stCnt == 0) inst->isLastInBlock = true;
            }
        } else {
            if (followStatus == MemCopyStatus::FOLLOW_LD) {
                // generate(ldi.u [u#1, ct.ldOt], ->t);
                inst->opcode = Opcode::OP_LDI_U;
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 0);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, ldOffset);
                GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                ldOffset += 8;
                uint32_t ldWidth = 8;
                stCnt += std::min(ldWidth, ldCnt);
                ldCnt -= std::min(ldWidth, ldCnt);
                if (src_3_bits != dst_3_bits) {
                    followStatus = MemCopyStatus::FOLLOW_CONCAT;
                } else if (stCnt >= 8) {
                    followStatus = MemCopyStatus::FOLLOW_ST;
                } else if (ldOffset >= ldMaxOffset) {
                    followStatus = MemCopyStatus::FOLLOW_MAX;
                }
                uint64_t temp_src = (src_bits >> 3) << 3;
                if (((temp_src + 8 + totalOffset) > dst_bits) && (dst_bits > (temp_src + totalOffset))) {
                    // 因为gfsim那边会有load比store先执性好的情况，所以在这里舍弃掉addrsrc和addrdst有交集的位置
                    // 这里有个问题，如果有ld-st 冲突，不应该改动ldwidth。要么改ct 方案，要么改opcode，要么改lsu 机制
                    // inst->ldwidth = dst_bits - (temp_src + totalOffset);
                }
                totalOffset += 8;
                ct_vld = false;
                if (followStatus == MemCopyStatus::FOLLOW_LD && ldCnt == 0 && stCnt == 0) inst->isLastInBlock = true;
            } else if (followStatus == MemCopyStatus::FOLLOW_CONCAT) {
                // generate(ccat t#1, t#3, shamt);
                inst->opcode = Opcode::OP_CCAT;
                GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 2);
                uint32_t shamt = 0;
                if (src_3_bits > dst_3_bits) {
                    shamt = 64 - ((src_3_bits - dst_3_bits) * 8);
                } else if (src_3_bits < dst_3_bits) {
                    shamt = (dst_3_bits - src_3_bits) * 8;
                }
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, shamt);
                GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);

                if (stCnt >= 8) {
                    followStatus = MemCopyStatus::FOLLOW_ST;
                } else if (ldOffset >= ldMaxOffset) {
                    followStatus = MemCopyStatus::FOLLOW_MAX;
                } else {
                    followStatus = MemCopyStatus::FOLLOW_LD;
                }
                if (followStatus == MemCopyStatus::FOLLOW_LD && ldCnt == 0 && stCnt == 0) inst->isLastInBlock = true;
            } else if (followStatus == MemCopyStatus::FOLLOW_ST) {
                // generate(sdi.u t#1, [u#2, ct.stOt]);
                inst->opcode = Opcode::OP_SDI_U;
                inst->storeSplit = false;
                GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, stOffset);
                stOffset += 8;
                stCnt -= 8;
                ct_vld = true;
                if (ldOffset >= ldMaxOffset) {
                    followStatus = MemCopyStatus::FOLLOW_MAX;
                } else {
                    followStatus = MemCopyStatus::FOLLOW_LD;
                }
                if (followStatus == MemCopyStatus::FOLLOW_LD && ldCnt == 0 && stCnt == 0) inst->isLastInBlock = true;
            }
        }
        RecordGenCycle(inst);
        inst->autogen = false;
        if (!GenSameTemplate(inst, __FUNCTION__)) {
            break;
        }

        if (!inst)
            break;

        if (inst->isLastInBlock) {
            nextStage = GenMemStage::START;
            nextMemcopyStage = GenMemCopyStage::GEN_ALIGN;
            ctBlock = std::shared_ptr<BlockCommand>(nullptr);
            break;
        }
    }
    if (ldCnt == 0 && followStatus == MemCopyStatus::FOLLOW_LD && nextMemcopyStage == GenMemCopyStage::GEN_FOLLOW) {
        nextMemcopyStage = GenMemCopyStage::GEN_ENDING;
        InsertEndingInstToQ();
    }
    LOG_DEBUG_M(Unit::BCC, Stage::D1);
    return nextMemcopyStage != GenMemCopyStage::GEN_FOLLOW;
}

bool CodeTemplate::GenMemcopyEndingInst()
{
    if (!ctBlock) return false;
    decState.vld = true;
    LOG_INFO_M(Unit::SPE, Stage::D1) << __FUNCTION__;
    BlockCommandPtr cmd = ctBlock;
    for (uint32_t i = 0; i < GetSim()->core->bctrl->configs.bctrl_bandwidth; i++) {
        if (genCntAtCyc == GetSim()->core->bctrl->configs.bctrl_bandwidth) break;
        SimInst inst = GenTemplateInitSimInst(cmd->stid);
        if (!endingInstQ.Empty()) {
            inst = endingInstQ.Read();
        } else {
            break;
        }
        RecordGenCycle(inst);
        inst->autogen = false;
        if (!GenSameTemplate(inst, __FUNCTION__)) {
            break;
        }

        if (inst->isLastInBlock) {
            nextStage = GenMemStage::START;
            nextMemcopyStage = GenMemCopyStage::GEN_ALIGN;
            ctBlock = std::shared_ptr<BlockCommand>(nullptr);
            break;
        }
    }
    return nextMemcopyStage != GenMemCopyStage::GEN_ENDING;
}


void CodeTemplate::InsertAlignInstToQ()
{
    if (!ctBlock) return;
    LOG_INFO_M(Unit::SPE, Stage::D1) <<__FUNCTION__;
    BlockCommandPtr cmd = ctBlock;
    uint32_t src_3_bits = nextRegData[static_cast<int>(MemCopyRegType::MCOPY_SRC_ADDR)] & 0x7;
    uint32_t dst_3_bits = nextRegData[static_cast<int>(MemCopyRegType::MCOPY_DST_ADDR)] & 0x7;
    auto &ldCnt = ct_ldCnt;
    auto &stCnt = ct_stCnt;
    auto &ldOffset = srcOffset;
    auto &stOffset = dstOffset;
    ldCnt = nextRegData[static_cast<int>(MemCopyRegType::MCOPY_SIZE)];

    if (1) {
        if (src_3_bits > dst_3_bits) {
            if (dst_3_bits > 0) {
                // generate(andi u#2, 0xff8, ->u);
                SimInst inst = GenTemplateInitSimInst(cmd->stid);
                inst->opcode = Opcode::OP_ANDI;
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0xff8);
                GenTemplateInstDst(inst, OperandType::OPD_ULINK, 0);
                alignInstQ.Write(inst);

                // generate(andi u#2, 0xff8, ->u);
                inst = GenTemplateInitSimInst(cmd->stid);
                inst->opcode = Opcode::OP_ANDI;
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0xff8);
                GenTemplateInstDst(inst, OperandType::OPD_ULINK, 0);
                alignInstQ.Write(inst);

                // generate(ldi.u [u#1, 0], ->t);
                inst = GenTemplateInitSimInst(cmd->stid);
                inst->opcode = Opcode::OP_LDI_U;
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 0);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0);
                GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                ldOffset += 8;
                uint32_t shfitData = 8 - src_3_bits;
                stCnt += min(shfitData, ldCnt);
                ldCnt -= stCnt;
                alignInstQ.Write(inst);

                // generate(ldi.u [u#2, 0], ->t);
                inst = GenTemplateInitSimInst(cmd->stid);
                inst->opcode = Opcode::OP_LDI_U;
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0);
                GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                alignInstQ.Write(inst);

                if (ldCnt > 0) {
                    // generate(ldi.u [u#1, ct.ldOt], ->t);
                    inst = GenTemplateInitSimInst(cmd->stid);
                    inst->opcode = Opcode::OP_LDI_U;
                    GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 0);
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, ldOffset);
                    ldOffset += 8;
                    uint32_t shfitData = 8;
                    stCnt += min(shfitData, ldCnt);
                    ldCnt -= min(shfitData, ldCnt);
                    GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                    alignInstQ.Write(inst);

                    // generate(hl.ccat t#1, t#3, shamt, ->t)
                    inst = GenTemplateInitSimInst(cmd->stid);
                    inst->opcode = Opcode::OP_CCAT;
                    GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                    GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 2);
                    uint32_t shamt = 64 - (src_3_bits * 8);
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, shamt);
                    GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0); // to do: 隐含T寄存器输出
                    alignInstQ.Write(inst);

                    // generate(addi t#2, 0, ->t);
                    inst = GenTemplateInitSimInst(cmd->stid);
                    inst->opcode = Opcode::OP_ADDI;
                    GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 1);
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0);
                    GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                    alignInstQ.Write(inst);

                    // generate(bfi t#4, t#2, ct.dstAr[2:0], N, ->t)
                    inst = GenTemplateInitSimInst(cmd->stid);
                    inst->opcode = Opcode::OP_BFI;
                    GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 3);
                    GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 1);
                    uint32_t M = dst_3_bits * NUM8;
                    uint32_t N = min(8 - dst_3_bits, (uint32_t)stCnt) * NUM8;
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, M);
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, N);
                    GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                    alignInstQ.Write(inst);

                    // generate(sdi.u t#1, [u#2, ct.stOt]);
                    inst = GenTemplateInitSimInst(cmd->stid);
                    inst->opcode = Opcode::OP_SDI_U;
                    GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                    GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, stOffset);
                    inst->storeSplit = false;
                    stOffset += 8;
                    stCnt -= min(8 - dst_3_bits, stCnt);
                    if (ldCnt == 0 && stCnt == 0) inst->isLastInBlock = true;
                    ct_vld = true;
                    alignInstQ.Write(inst);
                } else if (ldCnt == 0) {
                    // generate(srli t#2, ct.srcAr[2:0]*8, ->t);
                    inst = GenTemplateInitSimInst(cmd->stid);
                    inst->opcode = Opcode::OP_SRLI;
                    GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 1);
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, src_3_bits * 8);
                    GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                    alignInstQ.Write(inst);

                    // generate(bfi t#2, t#1, ct.dstAr[2:0], N, ->t);
                    inst = GenTemplateInitSimInst(cmd->stid);
                    inst->opcode = Opcode::OP_BFI;
                    GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 1);
                    GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                    uint32_t M = dst_3_bits * NUM8;
                    uint32_t N = min(8 - dst_3_bits, (uint32_t)stCnt) * NUM8;
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, M);
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, N);
                    GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                    alignInstQ.Write(inst);

                    // generate(sdi.u t#1, [u#2, ct.stOt]);
                    inst = GenTemplateInitSimInst(cmd->stid);
                    inst->opcode = Opcode::OP_SDI_U;
                    GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                    GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, stOffset);
                    inst->storeSplit = false;
                    stOffset += 8;
                    stCnt -= min(8 - dst_3_bits, stCnt);
                    if (ldCnt == 0 && stCnt == 0) inst->isLastInBlock = true;
                    alignInstQ.Write(inst);
                }
            } else if (dst_3_bits == 0) {
                SimInst inst = GenTemplateInitSimInst(cmd->stid);
                // generate(addi u#2, 0, ->u);
                inst->opcode = Opcode::OP_ADDI;
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0);
                GenTemplateInstDst(inst, OperandType::OPD_ULINK, 0);
                alignInstQ.Write(inst);

                // generate(andi u#2, 0xff8, ->u);
                inst = GenTemplateInitSimInst(cmd->stid);
                inst->opcode = Opcode::OP_ANDI;
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0xff8);
                GenTemplateInstDst(inst, OperandType::OPD_ULINK, 0);
                alignInstQ.Write(inst);

                // generate(ldi.u [u#1, 0], ->t);
                inst = GenTemplateInitSimInst(cmd->stid);
                inst->opcode = Opcode::OP_LDI_U;
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 0);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0);
                GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                ldOffset += 8;
                uint32_t shfitData = 8 - src_3_bits;
                stCnt += min(shfitData, ldCnt);
                ldCnt -= stCnt;
                alignInstQ.Write(inst);

                if (ldCnt > 0) {
                    // generate(addi zero, 0, ->t);
                    inst = GenTemplateInitSimInst(cmd->stid);
                    inst->opcode = Opcode::OP_ADDI;
                    GenTemplateInstSrc(inst, OperandType::OPD_ZERO, 0);
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0);
                    GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                    alignInstQ.Write(inst);
                } else {
                    // generate(srli t#1, ct.srcAddr[2:0]*8, ->t);
                    inst = GenTemplateInitSimInst(cmd->stid);
                    inst->opcode = Opcode::OP_SRLI;
                    GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, src_3_bits * 8);
                    GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                    alignInstQ.Write(inst);
                }
                if (ldCnt == 0 && stCnt == 0) inst->isLastInBlock = true;
            }

        } else if (src_3_bits < dst_3_bits) {
            if (src_3_bits > 0) {
                SimInst inst = GenTemplateInitSimInst(cmd->stid);
                // generate(andi u#2, 0xff8, ->u);
                inst->opcode = Opcode::OP_ANDI;
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0xff8);
                GenTemplateInstDst(inst, OperandType::OPD_ULINK, 0);
                alignInstQ.Write(inst);

                // generate(ldi.u [u#1, 0], ->t);
                inst = GenTemplateInitSimInst(cmd->stid);
                inst->opcode = Opcode::OP_LDI_U;
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 0);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0);
                GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                alignInstQ.Write(inst);

                inst = GenTemplateInitSimInst(cmd->stid);
                // generate(andi u#2, 0xff8, ->u);
                inst->opcode = Opcode::OP_ANDI;
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0xff8);
                GenTemplateInstDst(inst, OperandType::OPD_ULINK, 0);
                // 不应该用到dst1 才对
                // inst->dst1.atag = reg[static_cast<int>(MemCopyRegType::MCOPY_DST_ADDR)];
                alignInstQ.Write(inst);

                // generate(ldi.u [u#1, 0], ->t);
                inst = GenTemplateInitSimInst(cmd->stid);
                inst->opcode = Opcode::OP_LDI_U;
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 0);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0);
                GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                ldOffset += 8;
                uint32_t shfitData = 8 - src_3_bits;
                stCnt += min(shfitData, ldCnt);
                ldCnt -= stCnt;
                alignInstQ.Write(inst);

                // generate(srli t#1, ct.srcAr[2:0]*8, ->t);
                inst = GenTemplateInitSimInst(cmd->stid);
                inst->opcode = Opcode::OP_SRLI;
                GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, src_3_bits * 8);
                GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                alignInstQ.Write(inst);

                // generate(addi t#2, 0, ->t);
                inst = GenTemplateInitSimInst(cmd->stid);
                inst->opcode = Opcode::OP_ADDI;
                GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 1);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0);
                GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                alignInstQ.Write(inst);

                // generate(bfi t#4, t#2, ct.dstAr[2:0], N, ->t);
                inst = GenTemplateInitSimInst(cmd->stid);
                inst->opcode = Opcode::OP_BFI;
                GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 3);
                GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 1);
                uint32_t M = dst_3_bits * NUM8;
                uint32_t N = min(8 - dst_3_bits, (uint32_t)stCnt) * NUM8;
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, M);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, N);
                GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                alignInstQ.Write(inst);

                // generate(sdi.u t#1, [u#2, ct.stOt]);
                inst = GenTemplateInitSimInst(cmd->stid);
                inst->opcode = Opcode::OP_SDI_U;
                GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, stOffset);
                stOffset += 8;
                stCnt -= min(8 - dst_3_bits, stCnt);
                ct_vld = true;
                if (ldCnt == 0 && stCnt == 0) inst->isLastInBlock = true;
                alignInstQ.Write(inst);
            } else if (src_3_bits == 0) {
                SimInst inst = GenTemplateInitSimInst(cmd->stid);
                // generate(andi u#2, 0xff8, ->u);
                inst->opcode = Opcode::OP_ANDI;
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0xff8);
                GenTemplateInstDst(inst, OperandType::OPD_ULINK, 0);
                alignInstQ.Write(inst);

                // generate(ldi.u [u#1, 0], ->t);
                inst = GenTemplateInitSimInst(cmd->stid);
                inst->opcode = Opcode::OP_LDI_U;
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 0);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0);
                GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                alignInstQ.Write(inst);

                // generate(addi u#2, 0, ->u);
                inst = GenTemplateInitSimInst(cmd->stid);
                inst->opcode = Opcode::OP_ADDI;
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0);
                GenTemplateInstDst(inst, OperandType::OPD_ULINK, 0);
                alignInstQ.Write(inst);

                // generate(ldi.u [u#1, 0], ->t);
                inst = GenTemplateInitSimInst(cmd->stid);
                inst->opcode = Opcode::OP_LDI_U;
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 0);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0);
                GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                ldOffset += 8;
                uint32_t shfitData = 8 - src_3_bits;
                stCnt += min(shfitData, ldCnt);
                ldCnt -= stCnt;
                alignInstQ.Write(inst);

                // generate(bfi t#2, t#1, ct.dstAr[2:0], N, ->t);
                inst = GenTemplateInitSimInst(cmd->stid);
                inst->opcode = Opcode::OP_BFI;
                GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 1);
                GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                uint32_t M = dst_3_bits * NUM8;
                uint32_t N = min(8 - dst_3_bits, (uint32_t)stCnt) * NUM8;
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, M);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, N);
                GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                alignInstQ.Write(inst);

                // generate(sdi.u t#1, [u#2, ct.stOt]);
                inst = GenTemplateInitSimInst(cmd->stid);
                inst->opcode = Opcode::OP_SDI_U;
                GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, stOffset);
                stOffset += 8;
                stCnt -= min(8 - dst_3_bits, stCnt);
                ct_vld = true;
                if (ldCnt == 0 && stCnt == 0) inst->isLastInBlock = true;
                alignInstQ.Write(inst);
            }
        } else if (src_3_bits == dst_3_bits) {
            if (src_3_bits > 0) {
                SimInst inst = GenTemplateInitSimInst(cmd->stid);
                // generate(andi u#2, 0xff8, ->u);
                inst->opcode = Opcode::OP_ANDI;
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0xff8);
                GenTemplateInstDst(inst, OperandType::OPD_ULINK, 0);
                alignInstQ.Write(inst);

                // generate(ldi.u [u#1, 0], ->t);
                inst = GenTemplateInitSimInst(cmd->stid);
                inst->opcode = Opcode::OP_LDI_U;
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 0);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0);
                GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                alignInstQ.Write(inst);

                // generate(andi u#2, 0xff8, ->u);
                inst = GenTemplateInitSimInst(cmd->stid);
                inst->opcode = Opcode::OP_ANDI;
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0xff8);
                GenTemplateInstDst(inst, OperandType::OPD_ULINK, 0);
                alignInstQ.Write(inst);

                // generate(ldi.u [u#1, 0], ->t);
                inst = GenTemplateInitSimInst(cmd->stid);
                inst->opcode = Opcode::OP_LDI_U;
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 0);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0);
                GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                ldOffset += 8;
                uint32_t shfitData = 8 - src_3_bits;
                stCnt += min(shfitData, ldCnt);
                ldCnt -= stCnt;
                alignInstQ.Write(inst);

                // generate(srli t#1, ct.srcAr[2:0]*8, ->t);
                inst = GenTemplateInitSimInst(cmd->stid);
                inst->opcode = Opcode::OP_SRLI;
                GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, src_3_bits * 8);
                GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                alignInstQ.Write(inst);

                // generate(bfi t#3, t#1, ct.dstAr[2:0], ct.stCnt, ->t);
                inst = GenTemplateInitSimInst(cmd->stid);
                inst->opcode = Opcode::OP_BFI;
                GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 2);
                GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                uint32_t M = dst_3_bits * NUM8;
                uint32_t N = min(8 - dst_3_bits, (uint32_t)stCnt) * NUM8;
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, M);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, N);
                GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                alignInstQ.Write(inst);

                // generate(sdi.u t#1, [u#2, ct.stOt]);
                inst = GenTemplateInitSimInst(cmd->stid);
                inst->opcode = Opcode::OP_SDI_U;
                GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, stOffset);
                stOffset += 8;
                stCnt = 0;
                ct_vld = true;
                if (ldCnt == 0 && stCnt == 0) inst->isLastInBlock = true;
                alignInstQ.Write(inst);
            } else {
            }
        }
    }
    nextMemcopyStage = GenMemCopyStage::GEN_ALIGN;
}

void CodeTemplate::InsertEndingInstToQ()
{
    LOG_INFO_M(Unit::SPE, Stage::D1) << __FUNCTION__;
    BlockCommandPtr cmd = ctBlock;
    uint32_t src_3_bits = nextRegData[static_cast<int>(MemCopyRegType::MCOPY_SRC_ADDR)] & 0x7;
    uint32_t dst_3_bits = nextRegData[static_cast<int>(MemCopyRegType::MCOPY_DST_ADDR)] & 0x7;
    auto &stCnt = ct_stCnt;
    auto &stOffset = dstOffset;
    if (stCnt == 0) {
        SimInst inst = GenTemplateInitSimInst(cmd->stid);
        inst->opcode = Opcode::OP_BSTOP;
        inst->isLastInBlock = true;
        endingInstQ.Write(inst);
        return;
    }
    if (ct_vld) {
        SimInst inst = GenTemplateInitSimInst(cmd->stid);
        inst->opcode = Opcode::OP_SRLI;
        GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 1);
        GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
        if (src_3_bits > dst_3_bits) {
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, (src_3_bits - dst_3_bits) * 8);
        } else if (src_3_bits < dst_3_bits) {
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, (8 + src_3_bits - dst_3_bits) * 8);
        }
        // generate(srli t#2, imm, ->t);
        endingInstQ.Write(inst);
    }
    SimInst inst = GenTemplateInitSimInst(cmd->stid);
    switch (stCnt) {
        case 1:
            inst->opcode = Opcode::OP_SBI;
            GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
            GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, stOffset);
            stOffset += 1;
            stCnt -= 1;
            inst->isLastInBlock = true;
            endingInstQ.Write(inst);
            break;
        case 2:
            // generate(shi.u t#1, [u#2, ct.stOt]);
            inst->opcode = Opcode::OP_SHI_U;
            GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
            GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, stOffset);
            stOffset += 2;
            stCnt -= 2;
            inst->isLastInBlock = true;
            endingInstQ.Write(inst);
            break;
        case 3:
            // generate(shi.u t#1, [u#2, ct.stOt]);
            inst->opcode = Opcode::OP_SHI;
            GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
            GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, stOffset);
            stOffset += 2;
            stCnt -= 2;
            endingInstQ.Write(inst);

            // generate(srli t#1, 16, ->t);
            inst = GenTemplateInitSimInst(cmd->stid);
            inst->opcode = Opcode::OP_SRLI;
            GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 16);
            GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
            endingInstQ.Write(inst);

            // generate(sbi t#1, [u#2, ct.stOt]);
            inst = GenTemplateInitSimInst(cmd->stid);
            inst->opcode = Opcode::OP_SBI;
            GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
            GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, stOffset);
            stOffset += 1;
            stCnt -= 1;
            inst->isLastInBlock = true;
            endingInstQ.Write(inst);
            break;
        case 4:
            inst->opcode = Opcode::OP_SWI_U;
            GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
            GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, stOffset);
            stOffset += 4;
            stCnt -= 4;
            inst->isLastInBlock = true;
            endingInstQ.Write(inst);
            break;
        case 5:
            inst->opcode = Opcode::OP_SWI_U;
            GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
            GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, stOffset);
            stOffset += 4;
            stCnt -= 4;
            endingInstQ.Write(inst);

            // generate(srli t#1, 32, ->t);
            inst = GenTemplateInitSimInst(cmd->stid);
            inst->opcode = Opcode::OP_SRLI;
            GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 32);
            GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
            endingInstQ.Write(inst);

            // generate(sbi t#1, [u#2, ct.stOt]);
            inst = GenTemplateInitSimInst(cmd->stid);
            inst->opcode = Opcode::OP_SBI;
            GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
            GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, stOffset);
            stOffset += 1;
            stCnt -= 1;
            inst->isLastInBlock = true;
            endingInstQ.Write(inst);
            break;
        case 6:
            inst->opcode = Opcode::OP_SWI_U;
            GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
            GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, stOffset);
            stOffset += 4;
            stCnt -= 4;
            endingInstQ.Write(inst);

            // generate(srli t#1, 32, ->t);
            inst = GenTemplateInitSimInst(cmd->stid);
            inst->opcode = Opcode::OP_SRLI;
            GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 32);
            GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
            endingInstQ.Write(inst);

            // generate(shi.u t#1, [u#2, ct.stOt]);
            inst = GenTemplateInitSimInst(cmd->stid);
            inst->opcode = Opcode::OP_SHI_U;
            GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
            GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, stOffset);
            stOffset += 2;
            stCnt -= 2;
            inst->isLastInBlock = true;
            endingInstQ.Write(inst);
            break;
        case 7:
            inst->opcode = Opcode::OP_SWI_U;
            GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
            GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, stOffset);
            stOffset += 4;
            stCnt -= 4;
            endingInstQ.Write(inst);

            // generate(srli t#1, 32, ->t);
            inst = GenTemplateInitSimInst(cmd->stid);
            inst->opcode = Opcode::OP_SRLI;
            GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 32);
            GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
            endingInstQ.Write(inst);

            // generate(shi.u t#1, [u#2, ct.stOt]);  /* ct.stOt += 2 */
            inst = GenTemplateInitSimInst(cmd->stid);
            inst->opcode = Opcode::OP_SHI_U;
            GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
            GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, stOffset);
            stOffset += 2;
            stCnt -= 2;
            endingInstQ.Write(inst);

            // generate(srli t#1, 16, ->t);
            inst = GenTemplateInitSimInst(cmd->stid);
            inst->opcode = Opcode::OP_SRLI;
            GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 16);
            GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
            endingInstQ.Write(inst);

            // generate(sbi t#1, [u#2, ct.stOt]);
            inst = GenTemplateInitSimInst(cmd->stid);
            inst->opcode = Opcode::OP_SBI;
            GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
            GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 1);
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, stOffset);
            stOffset += 1;
            stCnt -= 1;
            inst->isLastInBlock = true;
            endingInstQ.Write(inst);
            break;
        default:
            ASSERT(0 && "stcnt is illegal data");
    }
}

bool CodeTemplate::GenEcall() {
    SimSys *sim = top->GetSim();
    ReferenceInfo &ref = sim->refInfo;
    constexpr uint32_t shortInstOffset = 4;

    decState.vld = true;
    LOG_INFO_M(Unit::SPE, Stage::D1) << __FUNCTION__;

    BlockCommandPtr cmd = ctBlock;
    totalInstCnt = ref.getEcallInstCount(cmd->bpc);
    if (totalInstCnt == 0) {
        GetSim()->core->bctrl->blockROB.reportException(cmd->bid, cmd->stid, "ECALL genarate instcnt is 0");
        return false;
    }
    for (uint32_t j = 0; j < GetSim()->core->bctrl->configs.bctrl_bandwidth; ++j) {
        if (genCntAtCyc == GetSim()->core->bctrl->configs.bctrl_bandwidth) break;
        SimInst inst = GenTemplateInitSimInst(cmd->stid);
        RecordGenCycle(inst);
        uint64_t i = genInstCnt;
        inst->bid = ctBlock->bid;
        inst->bpc = cmd->bpc;
        inst->pc = i * 4;
        inst->peID = cmd->peid;
        if (i < 24) {
            inst->opcode = Opcode::OP_ADDI;
            GenTemplateInstSrc(inst, OperandType::OPD_ZERO, 0);
            // gfrun中获取
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, ref.getEcallData(cmd->bpc, i * 4, inst->bid.val));
            GenTemplateInstDst(inst, OperandType::OPD_GREG, i);
        } else if (genInstCnt == totalInstCnt - 1 && !(top->top->sim->ckpt_file)) {
            inst->opcode = Opcode::OP_ACRC;
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, ref.getEcallData(cmd->bpc, i * 4, inst->bid.val));
        } else if ((i - 24) % 3 != 2) {
            inst->opcode = Opcode::OP_ADDI;
            GenTemplateInstSrc(inst, OperandType::OPD_ZERO, 0);
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM,
                               ref.getEcallData(cmd->bpc, i * shortInstOffset, inst->bid.val));
            GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
        } else {
            inst->opcode = Opcode::OP_SDI;
            GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 1);
            GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0);
        }
        inst->isLastInBlock = (totalInstCnt-1 == genInstCnt) ? true : false;
        inst->autogen = true;
        inst->isSysStateInst = true;
        if (!GenSameTemplate(inst, __FUNCTION__)) {
            break;
        }
        if (inst->isLastInBlock) {
            ctBlock = std::shared_ptr<BlockCommand>(nullptr);
            break;
        }
    }

    LOG_DEBUG_M(Unit::BCC, Stage::D1);

    return genInstCnt == totalInstCnt;
}

bool CodeTemplate::genMcopyRequstToRegData(BlockCommandPtr cmd) {
    LOG_INFO_M(Unit::SPE, Stage::D1) << __FUNCTION__;
    reg[SRC0_IDX] = cmd->srcATag[SRC0_IDX];
    reg[SRC1_IDX] = cmd->srcATag[SRC1_IDX];
    reg[SRC2_IDX] = cmd->srcATag[SRC2_IDX];

    for (uint32_t i = 0; i < GetSim()->core->bctrl->configs.bctrl_bandwidth; i++) {
        if (genCntAtCyc == GetSim()->core->bctrl->configs.bctrl_bandwidth || genInstCnt == MEM_REG_NUM) break;
        SimInst inst = GenTemplateInitSimInst(cmd->stid);
        uint32_t idx = 0;
        if (genInstCnt == 0) {
            idx = 2;
        } else if (genInstCnt == 2) {
            idx = 1;
        }
        RecordGenCycle(inst);
        inst->opcode = Opcode::OP_ADDI;
        GenTemplateInstSrc(inst, OperandType::OPD_GREG, reg[idx]);
        GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0);
        inst->backToCodeTemplate = true;
        GenTemplateInstDst(inst, OperandType::OPD_ULINK, 0);

        GenSameTemplate(inst, __FUNCTION__);
        inst->autogen = false;

        if (!inst) {
            break;
        }
        if (genInstCnt == MEM_REG_NUM) {
            nextStage = GenMemStage::WAIT_RF;
            break;
        }
    }

    return genInstCnt == MEM_REG_NUM;
}

bool CodeTemplate::genMsetRequstToRegData(BlockCommandPtr cmd) {
    LOG_INFO_M(Unit::SPE, Stage::D1) << __FUNCTION__;

    reg.clear();
    reg.push_back(cmd->srcATag[SRC0_IDX]);
    reg.push_back(cmd->srcATag[SRC1_IDX]);
    reg.push_back(cmd->srcATag[SRC2_IDX]);
    ASSERT(reg.size() == MEM_REG_NUM && "mask error!");

    for (uint32_t i = 0; i < GetSim()->core->bctrl->configs.bctrl_bandwidth; i++) {
        if (genCntAtCyc == GetSim()->core->bctrl->configs.bctrl_bandwidth || genInstCnt == MEM_REG_NUM) break;
        SimInst inst = GenTemplateInitSimInst(cmd->stid);
        RecordGenCycle(inst);
        inst->opcode = Opcode::OP_ADDI;
        inst->backToCodeTemplate = true;

        if (genInstCnt == 0) {
            // addi RegSrc0, 0, -> t
            GenTemplateInstSrc(inst, OperandType::OPD_GREG, reg[0]);
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0);
            GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
        } else if (genInstCnt == 1) {
            // addi RegSrc2, 0, -> u
            GenTemplateInstSrc(inst, OperandType::OPD_GREG, reg[2]);
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0);
            GenTemplateInstDst(inst, OperandType::OPD_ULINK, 0);
        } else if (genInstCnt == 2) {
            // addi RegSrc1, 0, -> u
            GenTemplateInstSrc(inst, OperandType::OPD_GREG, reg[1]);
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0);
            GenTemplateInstDst(inst, OperandType::OPD_ULINK, 0);
        }

        GenSameTemplate(inst, __FUNCTION__);
        inst->autogen = false;

        if (!inst) {
            break;
        }

        if (genInstCnt == MEM_REG_NUM) {
            nextStage = GenMemStage::WAIT_RF;
            break;
        }
    }

    return genInstCnt == MEM_REG_NUM;
}

void CodeTemplate::ReadRf(BlockCommandPtr &cmd)
{
    if (rf_ct_q->Size() < MEM_REG_NUM) {
        return;
    }
    vector<SimInst> retInsts;
    uint32_t count = 0;
    while (count < MEM_REG_NUM) {
        if (rf_ct_q->Empty()) break;
        if (rf_ct_q->Front()->bid == cmd->bid) {
            retInsts.push_back(rf_ct_q->Read());
            count++;
        } else {
            rf_ct_q->Write(rf_ct_q->Read());
        }
    }
    if (retInsts.size() < MEM_REG_NUM) {
        return;
    }
    for (uint32_t i = 0; i < MEM_REG_NUM; i++) {
        SimInst inst = FindInstWithRegId(retInsts, reg[i]);
        auto index = find(reg.begin(), reg.end(), inst->psrcs_[SRC0_IDX]->value);
        nextRegData[index - reg.begin()] = inst->pdsts_[DST0_IDX]->data;
    }
    if (cmd->opcode == Opcode::OP_MSET) {
        GenbfiInstMset(cmd->stid);
    }
    uint32_t stid = cmd->stid;
    top->waitMcopy[stid] = false;
    top->readFile[stid] = true;
    top->regData[stid] = nextRegData;
    if (cmd->opcode == Opcode::OP_MCOPY && nextRegData[static_cast<int>(MemCopyRegType::MCOPY_SIZE)] == 0) {
        SimInst inst = GenTemplateInitSimInst(cmd->stid);
        inst->autogen = false;
        GenSameTemplate(inst, __FUNCTION__);
        inst->opcode = Opcode::OP_BSTOP;
        inst->isLastInBlock = true;
        inst->autogen = true;
        nextStage = GenMemStage::COMPLETE;
        return;
    }
    InsertAlignInstToQ();
    nextStage = GenMemStage::GEN_INS;
}

SimInst CodeTemplate::FindInstWithRegId(vector<SimInst> &insts, uint32_t regId)
{
    for (auto inst : insts) {
        ASSERT(!inst->psrcs_.empty());
        if (inst->psrcs_[SRC0_IDX]->value == regId) {
            return inst;
        }
    }
    ASSERT(false && "not found reg");
    return nullptr;
}

size_t GetMemWidth(uint64_t address, size_t size)
{
    if ((address & 7) == 0 && size >= 8) {
        return 8;
    }

    if ((address & 3) == 0 && size >= 4) {
        return 4;
    }

    if ((address & 1) == 0 && size >= 2) {
        return 2;
    }

    if (size > 0) {
        return 1;
    }

    return 0;
}

bool CodeTemplate::GenMemSetInst()
{
    BlockCommandPtr cmd = ctBlock;
    uint32_t width = instWdith;
    uint64_t addr = curRegData[static_cast<int>(MemSetRegType::MSET_DST_ADDR)];
    uint64_t size = curRegData[static_cast<int>(MemSetRegType::MSET_SIZE)];
    uint64_t data = curRegData[static_cast<int>(MemSetRegType::MSET_DATA)];
    uint32_t maxOffset = 2040;
    if (data != 0) {
        maxOffset = 2032;
    }
    LOG_DEBUG_M(Unit::BCC, Stage::NA) << __FUNCTION__ << ", cmd: " << cmd->Dump()
                                     << ", addr: 0x" << hex << addr << ", size: " << dec
                                     << size << ", data: 0x" << hex << data
                                     << ", addrOffset: 0x" << hex << addrOffset
                                     << ", gendczva: " << boolalpha << isGenDczvaInst;
    decState.vld = true;
    LOG_INFO_M(Unit::SPE, Stage::D1) << __FUNCTION__;
    for (uint32_t i = 0; i < GetSim()->core->bctrl->configs.bctrl_bandwidth; i++) {
        if (genCntAtCyc == GetSim()->core->bctrl->configs.bctrl_bandwidth) break;
        SimInst inst = GenTemplateInitSimInst(cmd->stid);
        if (addrOffset < maxOffset) {
            if (data == 0 && (addr & 0x3f) == 0 && size >= 64) {
                if (isGenDczvaInst) {
                    inst->opcode = Opcode::OP_DC_ZVA;
                    GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                    inst->storeSplit = false;
                    isGenDczvaInst = false;
                    width = 64;
                    addr += width;
                    size -= width;
                    nextRegData[static_cast<int>(MemSetRegType::MSET_DST_ADDR)] = addr;
                    nextRegData[static_cast<int>(MemSetRegType::MSET_SIZE)] = size;
                    // generate(dc.zva t#1)
                } else {
                    inst->opcode = Opcode::OP_ADDI;
                    GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, addrOffset);
                    GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                    isGenDczvaInst = true;
                    addrOffset = 64;
                    // generate(addi t#1, ct.offset, -> t)    /* 更新地址 */
                }
            } else if (!extendInstQ.Empty() && data != 0) {
                // generate several bfi instructions
                inst = extendInstQ.Read();
            } else if (size > 0) {
                // generate several store instructions (sdi, swi, shi,sbi)
                width = GetMemWidth(addr, size);
                inst = GenUInst(false, cmd->bid, cmd->bpc, cmd->stid, width);
                if (data == 0) {
                    GenTemplateInstSrc(inst, OperandType::OPD_ZERO, 0);
                } else {
                    GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 0);
                }
                GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, addrOffset);
                addrOffset += width;
                // Set next step parameter.
                addr += width;
                size -= width;
                nextRegData[static_cast<int>(MemSetRegType::MSET_DST_ADDR)] = addr;
                nextRegData[static_cast<int>(MemSetRegType::MSET_SIZE)] = size;
            } else if (size == 0) {
                // generate bend
                inst->opcode = Opcode::OP_BSTOP;
                inst->isLastInBlock = true;
            }
        } else {
            // generate(addi t#1, ct.offset, ->t)
            inst->opcode = Opcode::OP_ADDI;
            GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
            GenTemplateInstSrc(inst, OperandType::OPD_UIMM, addrOffset);
            addrOffset = 0;
            GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
        }
        RecordGenCycle(inst);
        instWdith = width;
        GenSameTemplate(inst, __FUNCTION__);
        inst->autogen = false;

        if (!inst) {
            break;
        }

        if (inst->isLastInBlock) {
            nextStage = GenMemStage::START;
            addrOffset = 0;
            ctBlock = std::shared_ptr<BlockCommand>(nullptr);
            break;
        }
    }

    return genInstCnt == totalInstCnt;
}

void CodeTemplate::GenbfiInstMset(uint32_t stid)
{
    if (nextRegData[static_cast<int>(MemSetRegType::MSET_DATA)] != 0) {
        // bfi u#1, u#1, 1, 1, ->u
        SimInst inst = GenTemplateInitSimInst(stid);
        inst->opcode = Opcode::OP_BFI;
        GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 0);
        GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 0);
        GenTemplateInstSrc(inst, OperandType::OPD_UIMM, NUM8);
        GenTemplateInstSrc(inst, OperandType::OPD_UIMM, NUM8);
        GenTemplateInstDst(inst, OperandType::OPD_ULINK, 0);
        extendInstQ.Write(inst);

        // bfi u#1, u#1, 2, 2, ->u
        inst = GenTemplateInitSimInst(stid);
        inst->opcode = Opcode::OP_BFI;
        GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 0);
        GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 0);
        GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 2 * NUM8);
        GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 2 * NUM8);
        GenTemplateInstDst(inst, OperandType::OPD_ULINK, 0);
        extendInstQ.Write(inst);

        // bfi u#1, u#1, 4, 4, ->u
        inst = GenTemplateInitSimInst(stid);
        inst->opcode = Opcode::OP_BFI;
        GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 0);
        GenTemplateInstSrc(inst, OperandType::OPD_ULINK, 0);
        GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 4 * NUM8);
        GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 4 * NUM8);
        GenTemplateInstDst(inst, OperandType::OPD_ULINK, 0);
        extendInstQ.Write(inst);
    }
}

bool CodeTemplate::GenFExitTemplate()
{
    decState.vld = true;
    LOG_INFO_M(Unit::SPE, Stage::D1) << __FUNCTION__;

    uint32_t regptrInx = 1;
    uint32_t regGenCount = 0;
    BlockCommandPtr cmd = ctBlock;
    int32_t imm = cmd->srcData[SRC0_IDX];
    uint32_t hignUimm = ((uint32_t)imm >> LOW_UIMM_LEN) & HIGH_UIMM_MASK;
    uint32_t lowUimm = imm & LOW_UIMM_MASK;
    uint32_t regptr = obtainSetBitID(localGetMask, regptrInx);

    // instruction count calculation
    uint32_t instCount = (hignUimm == 0x0) ? 2 : 4;
    totalInstCnt = localRegCnt + instCount - 1;

    for (uint32_t i = 0; i < GetSim()->core->bctrl->configs.bctrl_bandwidth; i++) {
        if (genCntAtCyc == GetSim()->core->bctrl->configs.bctrl_bandwidth) break;
        SimInst inst = GenTemplateInitSimInst(cmd->stid);
        RecordGenCycle(inst);

        if (hignUimm != 0) {
            if (genInstCnt == 0) {
                // Generate(lui uimm[14:12], ->t);
                inst->opcode = Opcode::OP_LUI;
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, hignUimm);
                inst->srcs[SRC0_IDX]->shamt = 12;
                GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
            } else if (genInstCnt == 1) {
                // addi t#1, uimm[11:0], ->t
                inst->opcode = Opcode::OP_ADDI;
                GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, lowUimm);
                GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
            } else if (genInstCnt == 2) {
                // add sp, t#1, ->SP
                inst->opcode = Opcode::OP_ADD;
                GenTemplateInstSrc(inst, OperandType::OPD_GREG, static_cast<uint64_t>(GPR::GPR_SP));
                GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                GenTemplateInstDst(inst, OperandType::OPD_GREG, static_cast<uint64_t>(GPR::GPR_SP));
                localSetMask &= (~(0x1 << regptr));
            } else {
                for (uint32_t reg = 0; reg < static_cast<uint32_t>(GPR::GPR_COUNT);) {
                    uint32_t regMask = 0x1 << reg;
                    if ((localSetMask & regMask) == 0) {
                        ++reg;
                        continue;
                    }
                    ++regGenCount;
                    if (regGenCount <= genNoFixInstCnt) {
                        continue;
                    }
                    genNoFixInstCnt++;
                    // Generate(ldi [sp, offset], -> R[idx]);
                    inst->opcode = Opcode::OP_LDI;
                    GenTemplateInstSrc(inst, OperandType::OPD_GREG, static_cast<uint32_t>(GPR::GPR_SP));
                    int offset = genInstCnt - 2;
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, offset * (-8));
                    GenTemplateInstDst(inst, OperandType::OPD_GREG, reg);
                    localSetMask &= (~(0x1 << reg));
                    break;
                }
            }
        } else if (hignUimm == 0) {
            if (genInstCnt == 0) {
                // Generate(addi sp, uimm[11:0], ->sp);
                inst->opcode = Opcode::OP_ADDI;
                GenTemplateInstSrc(inst, OperandType::OPD_GREG, static_cast<uint32_t>(GPR::GPR_SP));
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, lowUimm);
                GenTemplateInstDst(inst, OperandType::OPD_GREG, static_cast<uint32_t>(GPR::GPR_SP));
                localSetMask &= (~(0x1 << regptr));
            } else {
                for (uint32_t reg = 0; reg < static_cast<uint32_t>(GPR::GPR_COUNT);) {
                    uint32_t regMask = 0x1 << reg;
                    if ((localSetMask & regMask) == 0) {
                        ++reg;
                        continue;
                    }
                    ++regGenCount;
                    if (regGenCount <= genNoFixInstCnt) {
                        continue;
                    }
                    genNoFixInstCnt++;
                    inst->opcode = Opcode::OP_LDI;
                    GenTemplateInstSrc(inst, OperandType::OPD_GREG, static_cast<uint32_t>(GPR::GPR_SP));
                    int offset = genInstCnt;
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, offset * (-8));
                    GenTemplateInstDst(inst, OperandType::OPD_GREG, reg);
                    localSetMask &= (~(0x1 << reg));
                    break;
                }
            }
        }

        ASSERT(inst);
        inst->autogen = false;
        if (!GenSameTemplate(inst, __FUNCTION__)) {
            break;
        }
        if (inst->isLastInBlock) {
            ctBlock = std::shared_ptr<BlockCommand>(nullptr);
            break;
        }
    }
    return genInstCnt == totalInstCnt;
}

bool CodeTemplate::GenFEntryTemplate()
{
    BlockCommandPtr cmd = ctBlock;
    uint64_t imm = cmd->srcData[SRC0_IDX];
    uint64_t uimm14_12 = GetBits(imm, 14, 12);
    uint64_t uimm11_0 = GetBits(imm, 11, 0);
    // [a0,a1,a2],ra,a4, when decoding, setBitMask will put a4 into the mask,
    // but the actual instruction generated should refer to the number of registers in [].
    uint32_t instcount = uimm14_12 == 0 ? 1 : 4;

    uint64_t m = ctBlock->srcData[SRC1_IDX];
    uint64_t n = ctBlock->srcData[SRC2_IDX];
    // R2-R23
    if (m < static_cast<uint64_t>(GPR::GPR_A0) || n > static_cast<uint64_t>(GPR::GPR_X3)) {
        top->GetSim()->core->bctrl->blockROB.reportException(ctBlock->bid, ctBlock->stid,
                                                             "FEntry template source error");
        LOG_ERROR_M(Unit::BCC, Stage::D2) << "[CT    Stage]: Report block exception at error FEntry."
            << ctBlock->bid.val;
        return false;
    }
    if (m > n) {
        n += static_cast<uint64_t>(GPR::GPR_COUNT);
    }

    uint32_t regNum = 0;
    for (uint64_t i = m; i <= n; i++) {
        uint64_t idx = i % static_cast<uint64_t>(GPR::GPR_COUNT);
        if (idx < static_cast<uint64_t>(GPR::GPR_A0)) {
            continue;
        }
        regNum++;
    }
    totalInstCnt = regNum + instcount;
    int offset = -1;

    for (uint32_t j = 0; j < GetSim()->core->bctrl->configs.bctrl_bandwidth; ++j) {
        if (genCntAtCyc == GetSim()->core->bctrl->configs.bctrl_bandwidth) break;
        SimInst inst = GenTemplateInitSimInst(cmd->stid);
        RecordGenCycle(inst);
        uint64_t i = genInstCnt;
        if (uimm14_12 == 0) {
            if (i == 0) {
                // Generate(subi sp, uimm[11:0], =>sp);
                inst->opcode = Opcode::OP_SUBI;
                GenTemplateInstSrc(inst, OperandType::OPD_GREG, static_cast<uint64_t>(GPR::GPR_SP));
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, uimm11_0);
                GenTemplateInstDst(inst, OperandType::OPD_GREG, static_cast<uint64_t>(GPR::GPR_SP));
            } else {
                uint32_t reg = (m + (i - 1)) % static_cast<uint32_t>(GPR::GPR_COUNT);
                if (reg > (uint32_t)n) {
                    break;
                }
                if (reg == static_cast<uint32_t>(GPR::GPR_ZERO) || reg == static_cast<uint32_t>(GPR::GPR_SP)) {
                    continue;
                }
                // Generate(sdi R[idx], [sp, offset]); --> Generate(sdi R[idx], [sp, uimm - offset]);
                inst->opcode = Opcode::OP_SDI;
                GenTemplateInstSrc(inst, OperandType::OPD_GREG, reg);
                GenTemplateInstSrc(inst, OperandType::OPD_GREG, static_cast<uint64_t>(GPR::GPR_SP));
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, imm - (i << 3));
            }
        } else {
            if (i == 0) {
                // Generate(addi sp, 0, ->t);
                inst->opcode = Opcode::OP_ADDI;
                GenTemplateInstSrc(inst, OperandType::OPD_GREG, static_cast<uint32_t>(GPR::GPR_SP));
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, 0);
                GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
            } else if(i == 1) {
                // Generate(lui uimm[14:12], ->t);
                inst->opcode = Opcode::OP_LUI;
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, uimm14_12);
                inst->srcs[SRC0_IDX]->shamt = 12;
                GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
            } else if(i == 2) {
                // Generate(addi t#1, uimm[11:0], ->t);
                inst->opcode = Opcode::OP_ADDI;
                GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, uimm11_0);
                GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
            } else if(i == 3) {
                // Generate(sub sp, t#1, =>sp);
                inst->opcode = Opcode::OP_SUB;
                GenTemplateInstSrc(inst, OperandType::OPD_GREG, static_cast<uint32_t>(GPR::GPR_SP));
                GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                GenTemplateInstDst(inst, OperandType::OPD_GREG, static_cast<uint32_t>(GPR::GPR_SP));
            } else {
                // Generate(sdi R[idx], [sp, offset]); -> Generate(sdi R[idx], [t#3, offset]);
                uint32_t reg = (m + (i - 4)) % static_cast<uint32_t>(GPR::GPR_COUNT);
                if (reg > (uint32_t)n) {
                    break;
                }
                if (reg == static_cast<uint32_t>(GPR::GPR_ZERO) || reg == static_cast<uint32_t>(GPR::GPR_SP)) {
                    continue;
                }
                inst->opcode = Opcode::OP_SDI;
                GenTemplateInstSrc(inst, OperandType::OPD_GREG, reg);
                GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 2);
                uint64_t immVal = (offset - (i - 4)) << 3;  // means "offset--"
                GenTemplateInstSrc(inst, OperandType::OPD_UIMM, immVal);
            }
        }

        ASSERT(inst);
        inst->autogen = false;
        if (!GenSameTemplate(inst, __FUNCTION__)) {
            break;
        }

        if (inst->isLastInBlock) {
            ctBlock = std::shared_ptr<BlockCommand>(nullptr);
            break;
        }
    }
    return genInstCnt == totalInstCnt;
}

uint32_t CodeTemplate::calculte_bits(uint32_t mask) {
    uint32_t num = 0;
    while(mask != 0) {
        if (mask & 1) {
            ++num;
        }
        mask >>= 1;
    }
    return num;
}

uint32_t CodeTemplate::obtainSetBitID(uint32_t mask, uint32_t n) {
    uint32_t pos = 0;
    while(mask && n) {
        if (mask & 1) {
            --n;
        }
        if (0 == n) break;
        ++pos;
        mask >>= 1;
    }
    return pos;
}

bool CodeTemplate::GenFRetTemplate()
{
    LOG_INFO_M(Unit::SPE, Stage::D1) << __FUNCTION__;

    BlockCommandPtr cmd = ctBlock;
    uint64_t imm = cmd->srcData[SRC0_IDX];
    uint64_t highimm = (imm >> LOW_UIMM_LEN) & HIGH_UIMM_MASK; // 12-14
    uint64_t lowimm = imm & LOW_UIMM_MASK; // 0-11
    // [a0,a1,a2],ra,a4, when decoding, setBitMask will put a4 into the mask,
    // but the actual instruction generated should refer to the number of registers in [].
    uint64_t m = ctBlock->srcData[SRC1_IDX];
    uint64_t n = ctBlock->srcData[SRC2_IDX];
    // R2-R23
    if (m < static_cast<uint64_t>(GPR::GPR_A0) || n > static_cast<uint64_t>(GPR::GPR_X3)) {
        top->GetSim()->core->bctrl->blockROB.reportException(ctBlock->bid, ctBlock->stid,
                                                             "FEntry template source error");
        LOG_ERROR_M(Unit::BCC, Stage::D2) << "[CT    Stage]: Report block exception at error FEntry."
            << ctBlock->bid.val;
        return false;
    }
    if (m > n) {
        n += static_cast<uint64_t>(GPR::GPR_COUNT);
    }
    uint32_t regNum = 0;
    for (uint64_t i = m; i <= n; i++) {
        uint64_t idx = i % static_cast<uint64_t>(GPR::GPR_COUNT);
        if (idx < static_cast<uint64_t>(GPR::GPR_A0)) {
            continue;
        }
        regNum++;
    }
    if (cmd->opcode == Opcode::OP_FRET_RA) {
        uint32_t instcount = highimm == 0 ? 2 : 4;
        totalInstCnt = regNum + instcount;
    } else if (cmd->opcode == Opcode::OP_FRET_STK) {
        uint32_t instcount = highimm == 0 ? 2 : 4;
        totalInstCnt = regNum + instcount;
    } else {
        cout << "Bstart type is " << GetOpcodeName(cmd->opcode) << " not support" << endl;
        ASSERT(0);
    }

    for (uint32_t j = 0; j < GetSim()->core->bctrl->configs.bctrl_bandwidth; ++j) {
        if (genCntAtCyc == GetSim()->core->bctrl->configs.bctrl_bandwidth) break;
        SimInst inst = GenTemplateInitSimInst(cmd->stid);
        RecordGenCycle(inst);
        uint64_t i = genInstCnt;
        if (cmd->opcode == Opcode::OP_FRET_RA) {
            if (i == 0) {
                // Generate(setc.tgt Ra);
                inst->opcode = Opcode::OP_SETC_TGT;
                GenTemplateInstSrc(inst, OperandType::OPD_GREG, static_cast<uint64_t>(GPR::GPR_RA));
            } else if (highimm == 0) {
                if (i == 1) {
                    // Generate(addi sp, uimm[11:0], ->SP);
                    inst->opcode = Opcode::OP_ADDI;
                    GenTemplateInstSrc(inst, OperandType::OPD_GREG, static_cast<uint64_t>(GPR::GPR_SP));
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, lowimm);
                    GenTemplateInstDst(inst, OperandType::OPD_GREG, static_cast<uint32_t>(GPR::GPR_SP));
                } else {
                    uint32_t regOffset = i - 2;
                    uint32_t reg = (m + regOffset) % static_cast<uint64_t>(GPR::GPR_COUNT);
                    if (reg == static_cast<uint32_t>(GPR::GPR_ZERO) || reg == static_cast<uint32_t>(GPR::GPR_SP)) {
                        continue;
                    }
                    // Generate(ldi [sp, offset], => R[idx]);
                    inst->opcode = Opcode::OP_LDI;
                    GenTemplateInstSrc(inst, OperandType::OPD_GREG, static_cast<uint32_t>(GPR::GPR_SP));
                    uint64_t immVal = -8 * ((int32_t)regOffset + 1);
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, immVal);
                    GenTemplateInstDst(inst, OperandType::OPD_GREG, reg);
                }
            } else {
                if(i == 1) {
                    // Generate(lui uimm[14:12], ->t);
                    inst->opcode = Opcode::OP_LUI;
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, highimm);
                    inst->srcs[SRC0_IDX]->shamt = 12;
                    GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                } else if(i == 2) {
                    // Generate(addi t#1, uimm[11:0], ->t);
                    inst->opcode = Opcode::OP_ADDI;
                    GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, lowimm);
                    GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                } else if(i == 3) {
                    // Generate(add sp, t#1, ->sp);
                    inst->opcode = Opcode::OP_ADD;
                    GenTemplateInstSrc(inst, OperandType::OPD_GREG, static_cast<uint32_t>(GPR::GPR_SP));
                    GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                    GenTemplateInstDst(inst, OperandType::OPD_GREG, static_cast<uint32_t>(GPR::GPR_SP));
                } else {
                    uint32_t regOffset = i - 4;
                    uint32_t reg = (m + regOffset) % static_cast<uint32_t>(GPR::GPR_COUNT);
                    if (reg == static_cast<uint32_t>(GPR::GPR_ZERO) || reg == static_cast<uint32_t>(GPR::GPR_SP)) {
                        continue;
                    }
                    // Generate(ldi [SP, offset], => R[idx]);
                    inst->opcode = Opcode::OP_LDI;
                    GenTemplateInstSrc(inst, OperandType::OPD_GREG, static_cast<uint32_t>(GPR::GPR_SP));
                    uint64_t immVal = -8 * (int32_t)(regOffset + 1);
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, immVal);
                    GenTemplateInstDst(inst, OperandType::OPD_GREG, reg);
                }
            }
        } else if (cmd->opcode == Opcode::OP_FRET_STK) {
            if(0 == highimm) {
                if(i == 0) {
                    // Generate(addi sp, uimm[11:0], ->sp);
                    inst->opcode = Opcode::OP_ADDI;
                    GenTemplateInstSrc(inst, OperandType::OPD_GREG, static_cast<uint32_t>(GPR::GPR_SP));
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, lowimm);
                    GenTemplateInstDst(inst, OperandType::OPD_GREG, static_cast<uint32_t>(GPR::GPR_SP));
                } else if(i == 1) {
                    // Generate(ldi [t#1, -1], -> R[M]);
                    inst->opcode = Opcode::OP_LDI;
                    GenTemplateInstSrc(inst, OperandType::OPD_GREG, static_cast<uint32_t>(GPR::GPR_SP));
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, -8);
                    GenTemplateInstDst(inst, OperandType::OPD_GREG, m);
                } else if(i == 2) {
                    // Generate(setc.tgt R[M]);
                    inst->opcode = Opcode::OP_SETC_TGT;
                    GenTemplateInstSrc(inst, OperandType::OPD_GREG, m);
                } else {
                    uint32_t regOffset = i - 3;
                    uint32_t reg = (m + regOffset + 1) % static_cast<uint32_t>(GPR::GPR_COUNT);
                    if (reg == static_cast<uint32_t>(GPR::GPR_ZERO) || reg == static_cast<uint32_t>(GPR::GPR_SP)) {
                        continue;
                    }
                    // Generate(ldi [sp, offset], =>R[idx]);
                    inst->opcode = Opcode::OP_LDI;
                    GenTemplateInstSrc(inst, OperandType::OPD_GREG, static_cast<uint32_t>(GPR::GPR_SP));
                    uint64_t immVal = -8 * (int32_t)(regOffset + 2);
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, immVal);
                    GenTemplateInstDst(inst, OperandType::OPD_GREG, reg);
                }
            } else {
                if(i == 0) {
                    // Generate(lui uimm[14:12], ->t);
                    inst->opcode = Opcode::OP_LUI;
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, highimm);
                    inst->srcs[SRC0_IDX]->shamt = 12;
                    GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                } else if(i == 1) {
                    // Generate(addi t#1, uimm[11:0], ->t);
                    inst->opcode = Opcode::OP_ADDI;
                    GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, lowimm);
                    GenTemplateInstDst(inst, OperandType::OPD_TLINK, 0);
                } else if(i == 2) {
                    // Generate(add sp, t#1, ->sp);
                    inst->opcode = Opcode::OP_ADD;
                    GenTemplateInstSrc(inst, OperandType::OPD_GREG, static_cast<uint64_t>(GPR::GPR_SP));
                    GenTemplateInstSrc(inst, OperandType::OPD_TLINK, 0);
                    GenTemplateInstDst(inst, OperandType::OPD_GREG, static_cast<uint64_t>(GPR::GPR_SP));
                } else if(i == 3) {
                    // Generate(ldi [t#1, -1], -> R[M]);
                    inst->opcode = Opcode::OP_LDI;
                    GenTemplateInstSrc(inst, OperandType::OPD_GREG, static_cast<uint32_t>(GPR::GPR_SP));
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, -8);
                    GenTemplateInstDst(inst, OperandType::OPD_GREG, m);
                } else if(i == 4) {
                    // Generate(setc.tgt R[M]);
                    inst->opcode = Opcode::OP_SETC_TGT;
                    GenTemplateInstSrc(inst, OperandType::OPD_GREG, m);
                } else {
                    uint32_t regOffset = i - 5;
                    uint32_t reg = (m + regOffset + 1) % static_cast<uint32_t>(GPR::GPR_COUNT);
                    if (reg == static_cast<uint32_t>(GPR::GPR_ZERO) || reg == static_cast<uint32_t>(GPR::GPR_SP)) {
                        continue;
                    }
                    // Generate(ldi [sp, offset], =>R[idx]);
                    inst->opcode = Opcode::OP_LDI;
                    GenTemplateInstSrc(inst, OperandType::OPD_GREG, static_cast<uint32_t>(GPR::GPR_SP));
                    uint64_t immVal = -8 * (int32_t)(regOffset + 2);
                    GenTemplateInstSrc(inst, OperandType::OPD_UIMM, immVal);
                    GenTemplateInstDst(inst, OperandType::OPD_GREG, reg);
                }
            }
        }

        ASSERT(inst);
        inst->autogen = false;
        if (!GenSameTemplate(inst, __FUNCTION__)) {
            break;
        }

        if (inst->isLastInBlock) {
            ctBlock = std::shared_ptr<BlockCommand>(nullptr);
            break;
        }
    }

    return genInstCnt == totalInstCnt;
}

SimInst CodeTemplate::GenTemplateInitSimInst(uint32_t stid)
{
    SimInst inst = std::make_shared<SimInstInfo>();
    inst->stid = stid;
    inst->biqType = BIQType::BCC_IQ;
    return inst;
}

void CodeTemplate::GenTemplateInstSrc(SimInst &inst, OperandType type, uint64_t value)
{
    POperandPtr operand = std::make_shared<PhysicalOperand>(type, value);
    operand->isDst = false;
    inst->srcs.push_back(operand);
    inst->psrcs_.push_back(operand);
}

void CodeTemplate::GenTemplateInstDst(SimInst &inst, OperandType type, uint64_t value)
{
    POperandPtr operand = std::make_shared<PhysicalOperand>(type, value);
    operand->isDst = true;
    inst->dsts.push_back(operand);
    inst->pdsts_.push_back(operand);
}

bool CodeTemplate::GenSameTemplate(SimInst &inst, const char* funcName)
{
    BlockCommandPtr cmd = ctBlock;
    inst->bpc = cmd->bpc;
    inst->peID = cmd->peid;
    inst->pc = genInstCnt * WIDTH_32;
    inst->bid = cmd->bid;
    inst->ctGen = true;
    inst->stid = cmd->stid;
    inst->InitInstGroupInfo(nullptr);

    genInstCnt++;
    genCntAtCyc++;
    inst->biqType = BIQType::BCC_IQ;
    GetLast(inst, cmd);
    inst->lsID = decState.lsID;
    inst->sid = cmd->startSID + decState.sid;
    inst->load_id = cmd->startLoadID + decState.load_id;

    auto header = GetSim()->core->bctrl->blockROB.getBlockHeader(inst->bid, inst->stid);
    // assign load/store id
    // DC.ZVA 也需要走 STA Pipe 进 LSU，所以这里也分配 lsid
    if (OpcodeIsLoad(inst->opcode) || OpcodeIsStore(inst->opcode) || OpcodeIsDCZVA(inst->opcode)) {
        IncROBID(decState.lsID, LSU_ID_RANGE);
        if (OpcodeIsStore(inst->opcode) || OpcodeIsDCZVA(inst->opcode)) {
            decState.sid++;
            /* load/store Count 会用来更新 BROB non-flush ptr，需要保留 */
            header->storeCount++;
        } else {
            decState.load_id++;
            header->loadCount++;
        }
    }

    pinst_ct_top_q->emplace_back(inst);
    ++top->statsInfo[inst->peID].genCTInstCnt;
    LOG_INFO_M(Unit::BCC, Stage::D1) << funcName << " gen " << inst->Dump();
    return true;
}

void CodeTemplate::RecordGenCycle(SimInst &inst)
{
    ASSERT(inst->stid != -1U);
    if (GetSim()->GetViewManager(inst->stid)->config.printPipeView || GetSim()->core->pTracer->IsEnabled()) {
        const uint64_t currentCycle = GetSim()->getCycles();
        inst->pipeCycle->createCycle = currentCycle;
        inst->pipeCycle->f0Cycle = currentCycle;
        inst->pipeCycle->f1Cycle = currentCycle;
        inst->pipeCycle->f2Cycle = currentCycle;
        inst->pipeCycle->f3Cycle = currentCycle;
        inst->pipeCycle->decodeCycle = currentCycle;
    }
}

void CodeTemplate::GetLast(SimInst &inst, BlockCommandPtr cmd) {
    if (cmd->opcode == Opcode::OP_FENTRY ||cmd->opcode == Opcode::OP_FEXIT ||
        cmd->opcode == Opcode::OP_FRET_RA || cmd->opcode == Opcode::OP_FRET_STK) {
        inst->isLastInBlock = (totalInstCnt == genInstCnt) ? true : false;
    }
}

} // namespace JCore
