#include "BlockCommand.h"

namespace JCore {
using namespace std;
void BlockCommand::AccumulateBlockInfo(SimInst &inst)
{
    switch (inst->opcode) {
        case Opcode::OP_BSTOP:
            bIsComplete = true;
            break;
        case Opcode::OP_BSTART_DIRECT:
        case Opcode::OP_BSTART_COND:
        case Opcode::OP_BSTART: {
            bpc = inst->pc;
            HandleBSTART(*inst);
            break;
        }
        case Opcode::OP_MCOPY:
            bpc = inst->pc;
            HandleMCOPY(*inst);
            break;
        case Opcode::OP_MSET:
            bpc = inst->pc;
            HandleMSET(*inst);
            break;
        case Opcode::OP_FENTRY:
            bpc = inst->pc;
            HandleFENTRY(*inst);
            break;
        case Opcode::OP_FEXIT:
            bpc = inst->pc;
            HandleFEXIT(*inst);
            break;
        case Opcode::OP_FRET_RA:
            bpc = inst->pc;
            HandleFRETRA(*inst);
            break;
        case Opcode::OP_FRET_STK:
            bpc = inst->pc;
            HandleFRETSTK(*inst);
            break;
        case Opcode::OP_B_HINT_TRACE:
            bpc = inst->pc;
        case Opcode::OP_B_HINT_PREFETCH:
            HandleBHINT(*inst);
            break;
        case Opcode::OP_B_IOD:
        case Opcode::OP_B_IOR:
        case Opcode::OP_B_IOT:
        case Opcode::OP_B_DIM:
        case Opcode::OP_B_DIMI:
        case Opcode::OP_B_TEXT:
        case Opcode::OP_B_ATTR:
            // 后续SimInst 和UInst 统一后添加
            break;
        default:
            ASSERT(false && "Unsupport B.PARAMETER");
    }
}

void BlockCommand::Wakeup(BlockCommandPtr cmd, TileOperandPtr tileOpd)
{
    for (TileOperandPtr &srcTile : srcs) {
        if (srcTile->vld && srcTile->handType == tileOpd->handType && srcTile->tileTag == tileOpd->tileTag) {
            srcTile->ready = true;
            if (cmd && !LessEqual(cmd->bid, bid)) {
                LOG_ERROR_M(Unit::BCC, Stage::NA) << "wakeup error: wakeuped by cmd: " << cmd->Dump()
                    << " younger cmd is " << Dump();
            }
        }
    }
}

void BlockCommand::FlagDoubleOutput(TileOperandPtr tileOpd)
{
    for (TileOperandPtr &dstTile : dsts) {
        if (dstTile->vld && dstTile->handType == tileOpd->handType && dstTile->tileTag == tileOpd->tileTag) {
            doubleOutput = true;
            break;
        }
    }
}

bool BlockCommand::Ready()
{
    if (!bIsComplete) {
        return false;
    }
    bool tileRdy = true;
    for (TileOperandPtr &srcTile : srcs) {
        if (!srcTile->ready) {
            tileRdy = false;
            break;
        }
    }
    return tileRdy;
}

bool BlockCommand::StashReady(bool checkDst)
{
    for (auto &src : srcs) {
        ASSERT(src);
        if (src->vld && !src->stashReady) {
            return false;
        }
    }
    for (auto &dst : dsts) {
        ASSERT(dst);
        if (dst->vld && !dst->stashReady && checkDst) {
            return false;
        }
    }
    return true;
}

void BlockCommand::HandleUInstBSTOP(SimInst inst)
{
    (void)inst;
    bIsComplete = true;
    UpdateDstTileInfo();
}

void BlockCommand::HandleUInstBIOR(SimInst inst)
{
    for (auto &psrc : inst->psrcs_) {
        srcATag.push_back(psrc->value);
        srcPtag.push_back(psrc->ptag);
        srcData.push_back(psrc->data);
    }
    for (auto &pdst : inst->pdsts_) {
        barg.dstATag.push_back(pdst->value);
        dstPtag.push_back(pdst->ptag);
    }
}

void BlockCommand::HandleUInstBIOT(SimInst inst)
{
    for (auto &tileSrc : inst->psrcs_) {
        if (!OperandTypeIsTile(tileSrc->type)) {
            continue;
        }
        srcs.push_back(ConvertOperandToTileOperand(tileSrc, false));
        srcTile.push_back(srcs.back());
    }
    for (auto &tileDst : inst->pdsts_) {
        if (tileDst->type == OperandType::OPD_TILE_ACC) {
            continue;
        }
        dsts.push_back(ConvertOperandToTileOperand(tileDst, true));
        dstTile.push_back(dsts.back());
    }
}

bool BlockCommand::IsMoveOp() const
{
    return tileOp == TileOp::TMOV && (blockAttr == nullptr
        || (blockAttr != nullptr && blockAttr->layout == LayOut::NORM));
}

TileOperandPtr BlockCommand::GetFirstValidTileSrc()
{
    for (auto src : srcs) {
        if (src->vld && src->handType != OperandType::OPD_TILE_ACC) {
            return src;
        }
    }
    return nullptr;
}

std::string BlockCommand::Dump()
{
    std::stringstream oss;
    oss << "B" << std::dec << bid.val;
    oss << " STID" << std::dec << stid;
    if (biqType == BIQType::MCALL_IQ) {
        oss << " G" << std::dec << gid.val;
    }
    oss << " BPC 0x" << std::hex << bpc;
    oss << " [" << GetBlockTypeName(blockType);
    oss << " " << GetBlockBranchTypeName(branchType);
    if (opcode != Opcode::OP_INVALID) {
        oss << " " << GetOpcodeName(opcode);
    }
    oss << "]";
    if (!IsBlockTypeParallel(blockType)) {
        return oss.str();
    }
    if (IsBlockTypeNeedVReg(blockType)) {
        oss << " " << GetVREGModeName(vregMode);
    }
    oss << " rdy:" << Ready() << " ";
    if (tileOp != TileOp::TINVALID) {
        oss << " " << GetTileOpName(tileOp);
    }
    oss << ", lb0:" << std::dec << lb0 << ", lb1:" << lb1 << ", lb2:" << lb2 << " ";
    if (CubeTileOp(tileOp)) {
        oss << " accChainId:" << accChainId << " ";
    }
    oss << "stash ready:" << StashReady() << " ";
    oss << BriefDataType2String(dataType) << " ";
    switch (tileOp) {
        case TileOp::TLOAD:
            if (blockAttr) {
                oss << blockAttr->Dump() << " ";
            }
            if (srcData.size() == 1) {
                oss << "GMBase:0x" << std::hex << srcData[0];
            } else if (srcData.size() == 2) {
                oss << "GMBase:0x" << std::hex << srcData[0] << " stride:" << std::dec << srcData[1];
            }
            for (auto& dst : dsts) {
                oss << " -> " << dst->Dump();
            }
            break;
        case TileOp::TSTORE:
            if (blockAttr) {
                oss << blockAttr->Dump();
            }
            if (!srcs.empty()) {
                oss << " " << srcs[0]->Dump();
            }
            if (srcData.size() == 1) {
                oss << "-> GMBase:0x" << std::hex << srcData[0];
            } else if (srcData.size() == 2) {
                oss << "-> GMBase:0x" << std::hex << srcData[0] << " stride:" << std::dec << srcData[1];
            }
            break;
        case TileOp::TMOV:
        default:
            if (IsBlockTypeNeedVReg(blockType)) {
                oss << " textPC:0x" << std::hex << bTextPC << " ";
            }
            for (size_t i = 0; i < srcs.size(); i++) {
                oss << "T" << i << "[" << srcs[i]->Dump() << "],";
            }
            for (auto &dst : dsts) {
                oss << " -> " << dst->Dump();
            }
    }
    return oss.str();
}

std::string BlockCommand::DumpFormalName()
{
    std::stringstream oss;
    oss << bid.val;
    oss << " " << GetTileOpName(tileOp);
    if (tileOp == TileOp::TLOAD || tileOp == TileOp::TSTORE) {
        ASSERT(!srcData.empty());
        oss << " addr:0x" << std::hex << srcData[0];
    }
    return oss.str();
}

std::string BlockCommand::DumpVecBriefName(uint64_t laneNum)
{
    std::stringstream oss;
    oss << std::dec << bid.val;
    if (tileOp != TileOp::TINVALID) {
        oss << " " << GetTileOpName(tileOp);
    } else {
        oss << " " << GetBlockTypeName(blockType);
        oss << " Text 0x" << std::hex << bTextPC;
    }
    oss << " GroupNum:" << std::dec << GetGroupNum(laneNum);
    return oss.str();
}

std::string BlockCommand::DumpSwimInfo(uint32_t offset)
{
    std::stringstream oss;
    oss << TASK_LABEL << bid.val + offset;
    oss << " " << Dump();
    return oss.str();
}

bool BlockCommand::IsFromCube() const
{
    return blockType == BlockType::BLK_TYPE_CUBE;
}
}