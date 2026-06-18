#include "Block.h"

namespace JCore {

std::string Block::Dump()
{
    std::stringstream oss;
    oss << "BPC 0x" << std::hex << barg.bpc;
    oss << " [" << GetBlockTypeName(blockType) << "]";
    oss << " [" << GetBlockBranchTypeName(branchType) << "]";
    if (IsBlockTypeNeedVReg(blockType)) {
        oss << " [" << GetVREGModeName(vregMode) << "]";
    }
    if (tileOp != TileOp::TINVALID) {
        oss << " [" << GetTileOpName(tileOp) << "]";
    }
    if (opcode != Opcode::OP_INVALID) {
        oss << " [" << OpcodeManager::Inst().GetOpcodeStr(opcode) << "] ";
        oss << DumpCT();
    }
    oss << " BNext 0x" << std::hex << barg.target;
    if (bTextOffset != 0) {
        oss << ", BText 0x" << std::hex << bTextPC;
    }
    return oss.str();
}

std::string Block::DumpCT()
{
    std::stringstream oss;
    switch (opcode) {
        case Opcode::OP_MCOPY:
        case Opcode::OP_MSET: {
            oss << "[";
            ASSERT(srcATag.size() == SRC3_IDX && "The number of src registers for MCOPY and MSET must be three.");
            oss << GetGPRNameRef(static_cast<GPR>(srcATag[SRC0_IDX]));
            oss << ", " << GetGPRNameRef(static_cast<GPR>(srcATag[SRC1_IDX]));
            oss << ", " << GetGPRNameRef(static_cast<GPR>(srcATag[SRC2_IDX]));
            oss << "]";
            break;
        }
        case Opcode::OP_FENTRY:
        case Opcode::OP_FEXIT:
        case Opcode::OP_FRET_RA:
        case Opcode::OP_FRET_STK:
            ASSERT(srcATag.size() == SRC3_IDX)
                << "The number of src registers for FENTRY/FEXIT/FRET_RA/_STK must be three.";
            oss << "[";
            oss << GetGPRNameRef(static_cast<GPR>(srcData[SRC1_IDX]));
            oss << " ~ " << GetGPRNameRef(static_cast<GPR>(srcData[SRC2_IDX]));
            oss << "], sp! ";
            oss << std::dec << srcData[SRC0_IDX];
            break;
        default:
            oss << "Undefined";
    }
    return oss.str();
}

void Block::SetBlockType(BlockType blkType)
{
    blockType = blkType;
    barg.blockTypeEncode = GetBlockBARGEncode(blkType);
}

void Block::SetBranchType(BranchType brType)
{
    branchType = brType;
    barg.branchTypeEncode = GetBranchBARGEncode(brType);
}

void Block::HandleBSTART(MInst &inst)
{
    barg.bpc = inst.pc;
    switch (inst.opcode) {
        case Opcode::OP_BSTART: {
            HandleBSTARTBlockType(inst);
            break;
        }
        case Opcode::OP_BSTART_DIRECT:
            barg.target = inst.pc + inst.srcs[SRC0_IDX]->data;
            SetBranchType(BranchType::BLK_BR_DIRECT);
            SetBlockType(BlockType::BLK_TYPE_STD);
            break;
        case Opcode::OP_BSTART_COND:
            barg.target = inst.pc + inst.srcs[SRC0_IDX]->data;
            SetBranchType(BranchType::BLK_BR_COND);
            SetBlockType(BlockType::BLK_TYPE_STD);
            break;
        default:
            ASSERT("Undefined BSTART");
            break;
    }
    inst.opcode = Opcode::OP_BSTART;
    inst.SetAssembleStr(GetBStartStr());
}
void Block::HandleBSTARTBlockType(MInst &inst)
{
    ASSERT(inst.srcs.size() == SRC3_IDX);
    BlockType blkType = inst.srcs[SRC0_IDX]->data != TYPE_XB_ENCODING ? static_cast<BlockType>(inst.srcs[SRC0_IDX]->data) :
                                                                BlockType::BLK_TYPE_XB;
    switch (blkType) {
        case BlockType::BLK_TYPE_STD:
            SetBlockType(BlockType::BLK_TYPE_STD);
            HandleBSTARTBranchType(inst);
            break;
        case BlockType::BLK_TYPE_SYS:
            SetBlockType(BlockType::BLK_TYPE_SYS);
            HandleBSTARTBranchType(inst);
            break;
        case BlockType::BLK_TYPE_FP:
            SetBlockType(BlockType::BLK_TYPE_FP);
            HandleBSTARTBranchType(inst);
            break;
        case BlockType::BLK_TYPE_MPAR:
            SetBlockType(BlockType::BLK_TYPE_MPAR);
            HandleBSTARTParallel(inst);
            break;
        case BlockType::BLK_TYPE_XB:
            SetBlockType(BlockType::BLK_TYPE_XB);
            break;
        default:
            break;
    }
}

void Block::HandleBSTARTBranchType(MInst &inst)
{
    BranchType brType = static_cast<BranchType>(inst.srcs[SRC1_IDX]->data);
    SetBranchType(brType);
    uint64_t payload = inst.srcs[SRC2_IDX]->data;
    switch (brType) {
        case BranchType::BLK_BR_FALL:
        case BranchType::BLK_BR_DIRECT:
        case BranchType::BLK_BR_COND:
        case BranchType::BLK_BR_CALL:
            barg.target = inst.pc + payload;
            break;
        default:
            break;
    }
}

void Block::HandleBSTARTParallel(MInst &inst)
{
    SetBranchType(BranchType::BLK_BR_FALL);
    uint64_t payload = inst.binary;
    uint64_t parOp = GetBits(payload, PAR32_OPCODE_END, PAR32_OPCODE_BEGIN);
    uint64_t function = GetBits(payload, PAR32_FUNC_END, PAR32_FUNC_BEGIN);
    uint64_t mode = GetBits(payload, PAR32_MODE_END, PAR32_MODE_BEGIN);
    uint64_t dType = GetBits(payload, PAR32_DATATYPE_END, PAR32_DATATYPE_BEGIN);
    switch (static_cast<ParOpcodeID>(parOp)) {
        case ParOpcodeID::MPAR: {
            SetBlockType(BlockType::BLK_TYPE_MPAR);
            vregMode = static_cast<VREGMode>(mode);
            break;
        }
        case ParOpcodeID::MSEQ: {
            SetBlockType(BlockType::BLK_TYPE_MSEQ);
            vregMode = static_cast<VREGMode>(mode);
            break;
        }
        case ParOpcodeID::TMA: {
            SetBlockType(BlockType::BLK_TYPE_TMA);
            dataType = static_cast<DataType>(dType);
            HandleBSTARTTMA(function);
            break;
        }
        case ParOpcodeID::TEPL: {
            SetBlockType(BlockType::BLK_TYPE_TEPL);
            vregMode = static_cast<VREGMode>(mode);
            dataType = static_cast<DataType>(dType);
            HandleBSTARTTEPL(function, mode);
            break;
        }
        case ParOpcodeID::VPAR: {
            SetBlockType(BlockType::BLK_TYPE_VPAR);
            vregMode = static_cast<VREGMode>(mode);
            break;
        }
        case ParOpcodeID::VSEQ: {
            SetBlockType(BlockType::BLK_TYPE_VSEQ);
            vregMode = static_cast<VREGMode>(mode);
            break;
        }
        case ParOpcodeID::CUBE: {
            SetBlockType(BlockType::BLK_TYPE_CUBE);
            dataType = static_cast<DataType>(dType);
            HandleBSTARTCUBE(function);
            break;
        }
        case ParOpcodeID::FIXP: {
            SetBlockType(BlockType::BLK_TYPE_FIXP);
            dataType = static_cast<DataType>(dType);
            break;
        }
        default:
            ASSERT(false && "Undefined PARALLEL BlockType");
    }
}

void Block::HandleBSTARTTMA(uint64_t function)
{
    bIsTemplate = true;
    switch (static_cast<ParFunction>(function)) {
        case ParFunction::TMA_TLOAD:
            tileOp = TileOp::TLOAD;
            break;
        case ParFunction::TMA_TSTORE:
            tileOp = TileOp::TSTORE;
            break;
        case ParFunction::TMA_TMOV:
            tileOp = TileOp::TMOV;
            break;
        case ParFunction::TMA_MGATHER:
            tileOp = TileOp::MGATHER;
            break;
        case ParFunction::TMA_MSCATTER:
            tileOp = TileOp::MSCATTER;
            break;
        default:
            ASSERT(false && "Undefined TMA PARALLEL function");
    }
}

void Block::HandleBSTARTCUBE(uint64_t function)
{
    bIsTemplate = true;
    switch (static_cast<ParFunction>(function)) {
        case ParFunction::CUBE_TMATMUL:
            tileOp = TileOp::TMATMUL;
            break;
        case ParFunction::CUBE_TMATMUL_BIAS:
            tileOp = TileOp::TMATMUL_BIAS;
            break;
        case ParFunction::CUBE_TMATMUL_ACC:
            tileOp = TileOp::TMATMUL_ACC;
            break;
        case ParFunction::CUBE_TMATMULMX:
            tileOp = TileOp::TMATMULMX;
            break;
        case ParFunction::CUBE_TMATMULMX_BIAS:
            tileOp = TileOp::TMATMULMX_BIAS;
            break;
        case ParFunction::CUBE_TMATMULMX_ACC:
            tileOp = TileOp::TMATMULMX_ACC;
            break;
        case ParFunction::CUBE_ACCCVT:
            tileOp = TileOp::ACCCVT;
            break;
        case ParFunction::CUBE_TGEMV:
            tileOp = TileOp::TGEMV;
            break;
        case ParFunction::CUBE_TGEMV_BIAS:
            tileOp = TileOp::TGEMV_BIAS;
            break;
        case ParFunction::CUBE_TGEMV_ACC:
            tileOp = TileOp::TGEMV_ACC;
            break;
        default:
            ASSERT(false && "Undefined CUBE PARALLEL function");
    }
}

void Block::HandleBSTARTTEPL(uint64_t function, uint64_t mode)
{
    bIsTemplate = true;

    ParFunctionTEPL tepl = static_cast<ParFunctionTEPL>(function);

    switch (static_cast<ParFunctionMode>(mode)) {
        case ParFunctionMode::ONLYTILE:
            tileOp = GetTeplTileOp(tepl);
            break;
        case ParFunctionMode::TILEANDSCALAR:
            tileOp = GetTeplTileOp(tepl + ParFunctionTEPL::TEPL_TADDS);
            break;
        case ParFunctionMode::REDUCEANDBROADCAST:
            tileOp = GetTeplTileOp(tepl + ParFunctionTEPL::TEPL_TROWSUM);
            break;
        case ParFunctionMode::EXCEPTION:
            tileOp = GetTeplTileOp(tepl + ParFunctionTEPL::EXCEPTION__RESERVE_1);
            break;
        default:
            break;
    }
    if (tileOp == TileOp::TINVALID) {
        assert(0 && "Such TEPL template blocks not yet supported");
    }
}

void Block::HandleBSTOP(MInst &inst)
{
    (void)inst;
    bIsComplete = true;
}

void Block::HandleBIOR(MInst &inst)
{
    for (auto &src : inst.srcs) {
        srcATag.push_back(src->value);
        srcData.push_back(src->data);
    }
    for (auto &dst : inst.dsts) {
        barg.dstATag.push_back(dst->value);
        dstData.push_back(dst->data);
    }
}

void Block::HandleBIOT(MInst &inst)
{
    for (auto &src : inst.srcs) {
        if (!OperandTypeIsTile(src->type)) {
            continue;
        }
        srcTile.push_back(src);
    }
    for (auto &dst : inst.dsts) {
        dstTile.push_back(dst);
    }
    if (inst.iotLast) {
        iotLast = inst.iotLast;
    }
}

void Block::HandleBDIM(MInst &inst)
{
    switch (inst.opcode) {
        case Opcode::OP_B_DIM: {
            switch (inst.srcs[SRC0_IDX]->data) {
                case 0:
                    lb0 = inst.srcs[SRC1_IDX]->data;
                    if (inst.codeLen == EncodeLen::ENL_W) {
                        lb0 += inst.srcs[SRC2_IDX]->data;
                    }
                    break;
                case 1:
                    lb1 = inst.srcs[SRC1_IDX]->data;
                    if (inst.codeLen == EncodeLen::ENL_W) {
                        lb1 += inst.srcs[SRC2_IDX]->data;
                    }
                    break;
                case 2:
                    lb2 = inst.srcs[SRC1_IDX]->data;
                    if (inst.codeLen == EncodeLen::ENL_W) {
                        lb2 += inst.srcs[SRC2_IDX]->data;
                    }
                    break;
                default:
                    ASSERT(false && "Undefied loop nest");
            }
            break;
        }
        case Opcode::OP_B_DIMI: {
            switch (inst.srcs[SRC0_IDX]->data) {
                case 0:
                    lb0 = inst.srcs[SRC1_IDX]->data;
                    break;
                case 1:
                    lb1 = inst.srcs[SRC1_IDX]->data;
                    break;
                case 2:
                    lb2 = inst.srcs[SRC1_IDX]->data;
                    break;
                default:
                    ASSERT(false && "Undefied loop nest");
            }
            break;
        }
        default:
            ASSERT(false && "Undefined BDIM instrution");
    }
}

void Block::HandleBCATR(MInst &inst)
{
    if (!blockAttr) {
        blockAttr = std::make_shared<BlockAttribute>();
    }
    blockAttr->rl = static_cast<bool>(inst.srcs[SRC0_IDX]->data);
    blockAttr->aq = static_cast<bool>(inst.srcs[SRC1_IDX]->data);
    blockAttr->atomic = static_cast<bool>(inst.srcs[SRC2_IDX]->data);
    blockAttr->far = static_cast<bool>(inst.srcs[SRC3_IDX]->data);
    blockAttr->trap = static_cast<bool>(inst.srcs[SRC4_IDX]->data);
    blockAttr->dimReduction = static_cast<bool>(inst.srcs[SRC5_IDX]->data);
    inst.SetAssembleStr(GetBAttrStr());
}

void Block::HandleBDATR(MInst &inst)
{
    if (!blockAttr) {
        blockAttr = std::make_shared<BlockAttribute>();
    }
    blockAttr->layout = static_cast<LayOut>(inst.srcs[SRC0_IDX]->data);
    blockAttr->rMode = static_cast<FRMMode>(inst.srcs[SRC1_IDX]->data);
    blockAttr->dataType = static_cast<DataType>(inst.srcs[SRC2_IDX]->data);
    blockAttr->canon = static_cast<bool>(inst.srcs[SRC3_IDX]->data);
    blockAttr->saturation = static_cast<bool>(inst.srcs[SRC4_IDX]->data);
    blockAttr->padValue = static_cast<PadValue>(inst.srcs[SRC5_IDX]->data);
    blockAttr->cMode = static_cast<CMode>(inst.srcs[SRC6_IDX]->data);
    inst.SetAssembleStr(GetBAttrStr());
}

void Block::HandleBTEXT(MInst &inst)
{
    barg.localRetAddr = inst.pc + GetEncodeLenVal(inst.codeLen);
    bTextOffset = inst.srcs[SRC0_IDX]->data;
    bTextPC = inst.pc + inst.srcs[SRC0_IDX]->data;
    inst.SetAssembleStr(GetBTextStr());
    UpdateDstTileInfo();
}

void Block::HandleBHINT(MInst &inst)
{
    blockHint = inst.blockHint;
    if (inst.opcode == Opcode::OP_B_HINT_TRACE) {
        barg.bpc = inst.pc;
        SetBlockType(BlockType::BLK_TYPE_STD);
        SetBranchType(BranchType::BLK_BR_FALL);
    }
    inst.SetAssembleStr(GetHintStr());
}

void Block::HandleMCOPY(MInst &inst)
{
    barg.bpc = inst.pc;
    bIsTemplate = true;
    SetBlockType(BlockType::BLK_TYPE_STD);
    SetBranchType(BranchType::BLK_BR_FALL);
    opcode = Opcode::OP_MCOPY;
    for (auto &src : inst.srcs) {
        srcATag.push_back(src->value);
        srcData.push_back(src->data);
    }
    inst.SetAssembleStr(GetSTDTemplateStr());
}

void Block::HandleMSET(MInst &inst)
{
    barg.bpc = inst.pc;
    bIsTemplate = true;
    SetBlockType(BlockType::BLK_TYPE_STD);
    SetBranchType(BranchType::BLK_BR_FALL);
    opcode = Opcode::OP_MSET;
    for (auto &src : inst.srcs) {
        srcATag.push_back(src->value);
        srcData.push_back(src->data);
    }
    inst.SetAssembleStr(GetSTDTemplateStr());
}

void Block::HandleFENTRY(MInst &inst)
{
    barg.bpc = inst.pc;
    bIsTemplate = true;
    SetBlockType(BlockType::BLK_TYPE_STD);
    SetBranchType(BranchType::BLK_BR_FALL);
    opcode = Opcode::OP_FENTRY;
    for (auto &src : inst.srcs) {
        srcATag.push_back(src->value);
        srcData.push_back(src->data);
    }
    inst.SetAssembleStr(GetSTDTemplateStr());
}

void Block::HandleFEXIT(MInst &inst)
{
    barg.bpc = inst.pc;
    bIsTemplate = true;
    SetBlockType(BlockType::BLK_TYPE_STD);
    SetBranchType(BranchType::BLK_BR_FALL);
    opcode = Opcode::OP_FEXIT;
    for (auto &src : inst.srcs) {
        srcATag.push_back(src->value);
        srcData.push_back(src->data);
    }
    inst.SetAssembleStr(GetSTDTemplateStr());
}

void Block::HandleFRETRA(MInst &inst)
{
    barg.bpc = inst.pc;
    bIsTemplate = true;
    SetBlockType(BlockType::BLK_TYPE_STD);
    SetBranchType(BranchType::BLK_BR_RET);
    opcode = Opcode::OP_FRET_RA;
    for (auto &src : inst.srcs) {
        srcATag.push_back(src->value);
        srcData.push_back(src->data);
    }
    inst.SetAssembleStr(GetSTDTemplateStr());
}

void Block::HandleFRETSTK(MInst &inst)
{
    barg.bpc = inst.pc;
    bIsTemplate = true;
    SetBlockType(BlockType::BLK_TYPE_STD);
    SetBranchType(BranchType::BLK_BR_RET);
    opcode = Opcode::OP_FRET_STK;
    for (auto &src : inst.srcs) {
        srcATag.push_back(src->value);
        srcData.push_back(src->data);
    }
    inst.SetAssembleStr(GetSTDTemplateStr());
}

std::string Block::GetBStartStr()
{
    std::stringstream oss;
    oss << "." << GetBlockTypeName(blockType);
    switch (branchType) {
        case BranchType::BLK_BR_INVAL:
            oss << "INVALID";
            break;
        case BranchType::BLK_BR_FALL:
            break;
        case BranchType::BLK_BR_DIRECT:
        case BranchType::BLK_BR_COND:
        case BranchType::BLK_BR_CALL:
            oss << " " << GetBlockBranchTypeName(branchType) << ", 0x" << std::hex << barg.target;
            break;
        case BranchType::BLK_BR_IND:
        case BranchType::BLK_BR_ICALL:
        case BranchType::BLK_BR_RET:
            oss << " " << GetBlockBranchTypeName(branchType);
            break;
        default:
            ASSERT(false && "unsupport branchtype value:") << static_cast<uint64_t>(branchType);
    }
    switch (blockType) {
        case BlockType::BLK_TYPE_MPAR:
        case BlockType::BLK_TYPE_MSEQ:
        case BlockType::BLK_TYPE_VPAR:
        case BlockType::BLK_TYPE_VSEQ: {
            oss << " " << GetVREGModeName(vregMode);
            break;
        }
        case BlockType::BLK_TYPE_TMA:
        case BlockType::BLK_TYPE_CUBE:
        case BlockType::BLK_TYPE_TEPL:
        case BlockType::BLK_TYPE_FIXP: {
            oss << " " << GetTileOpName(tileOp);
            oss << " " << BriefDataType2String(dataType);
            break;
        }
        case BlockType::BLK_TYPE_XB:
            oss << "ACR-ID,C-ID";
            break;
        default:
            break;
    }
    return oss.str();
}

std::string Block::GetSTDTemplateStr()
{
    std::stringstream oss;
    oss << " ";
    switch (opcode) {
        case Opcode::OP_MCOPY:
        case Opcode::OP_MSET: {
            oss << "[" << GetGPRNameRef(static_cast<GPR>(srcATag[SRC0_IDX]));
            oss << ", " << GetGPRNameRef(static_cast<GPR>(srcATag[SRC1_IDX]));
            oss << ", " << GetGPRNameRef(static_cast<GPR>(srcATag[SRC2_IDX]));
            oss << "]";
            break;
        }
        case Opcode::OP_FENTRY:
        case Opcode::OP_FEXIT:
        case Opcode::OP_FRET_RA:
        case Opcode::OP_FRET_STK: {
            oss << "[" << GetGPRNameRef(static_cast<GPR>(srcATag[SRC1_IDX]));
            oss << " ~ " << GetGPRNameRef(static_cast<GPR>(srcATag[SRC2_IDX]));
            oss << "], sp!, 0x" << std::hex << srcATag[SRC0_IDX];
            break;
        }
        default:
            break;
    }
    return oss.str();
}

std::string Block::GetBTextStr()
{
    std::stringstream oss;
    oss << " 0x" << std::hex << bTextPC;
    return oss.str();
}

std::string Block::GetBAttrStr()
{
    std::stringstream oss;
    oss << " " << blockAttr->Dump();
    return oss.str();
}

std::string Block::GetHintStr()
{
    std::stringstream oss;
    oss << " " << blockHint->Dump();
    return oss.str();
}

void Block::SetBlockIsComplete()
{
    bIsComplete = true;
    if (!dstTile.empty()) {
        UpdateDstTileInfo();
    }
}

TileInfoPtr Block::GetDstTileInfo(uint64_t tileIdx) const
{
    TileInfoPtr ptr = std::make_shared<TileInfo>();
    ptr->dataType = dataType;
    // 此处仅未支持gather/scatter 地址特性，剩余TileInfo 信息需要指令集确定。
    // tileIdx 用于支持多输出，例如。 ACCCVT 的RowMax 扩展。
    // 目前设置TileInfo 的时机，为B.IOT 执行时，若需要其他例如B.DIM 信息，可以在整个Block完整后，重新设置TileInfo
    (void)tileIdx;
    return ptr;
}

void Block::UpdateDstTileInfo() const
{
    if (dataType == DataType::INVALID && !dstTile.empty()) {
        for (auto &dst : dstTile) {
            if (dst->type == OperandType::OPD_TILE_STACK && IsBlockTypeMCall(blockType)) {
                continue;
            }
            uint64_t shapeInfo = lb0 * lb1 * lb2;
            uint64_t elemWidth = dst->size / shapeInfo;
            switch (elemWidth) {
                case 1:
                    dst->tileInfo->dataType = DataType::UINT8;
                    break;
                case WIDTH_16:
                    dst->tileInfo->dataType = DataType::UINT16;
                    break;
                case WIDTH_32:
                    dst->tileInfo->dataType = DataType::UINT32;
                    break;
                case WIDTH_64:
                    dst->tileInfo->dataType = DataType::UINT64;
                    break;
                default:
                    dst->tileInfo->dataType = DataType::UINT32;
                    LOG_INFO("unable to infer dst data type from Tile size & dim, set as default U32");
                    break;
            }
        }
        return;
    }
    /* 由于B.IOT 和B.DIM 顺序未做要求，因此若初始化TileInfo 的col/row 信息需要在块完整后再初始化 */
    for (auto &dst : dstTile) {
        (void)dst;
    }
}

bool Block::IsReduceDimension()
{
    if (blockAttr != nullptr) {
        return blockAttr->dimReduction;
    }
    return false;
}

uint64_t Block::GetGroupNum(uint64_t laneNum)
{
    totalBodyIters = lb0 * lb1 * lb2;
    if (IsReduceDimension()) {
        totalGroupNum = CeilDiv(totalBodyIters, laneNum);
    } else {
        uint64_t chunks0 = CeilDiv(lb0, laneNum);
        totalGroupNum = lb2 * lb1 * chunks0;
    }
    return totalGroupNum;
}

uint64_t Block::GetFlatIter(uint64_t groupId, uint64_t landNum, uint64_t lane)
{
    // 仅 DimReduction 有意义；MultiDim 下不要用
    return groupId * landNum + lane;
}

uint64_t Block::GetLC0(uint64_t groupId, uint64_t landNum, uint64_t lane)
{
    uint64_t res = 0;
    if (IsReduceDimension()) {
        uint64_t flat = groupId * landNum + lane;
        res = flat % lb0;
    } else {
        uint64_t chunks0 = CeilDiv(lb0, landNum);
        uint64_t chunk = groupId % chunks0;
        res = chunk * landNum + lane;
    }
    return res;
}

uint64_t Block::GetLC1(uint64_t groupId, uint64_t landNum, uint64_t lane)
{
    uint64_t res = 0;
    if (IsReduceDimension()) {
        uint64_t flat = groupId * landNum + lane;
        uint64_t t = flat / lb0;
        res = t % lb1;
    } else {
        uint64_t chunks0 = CeilDiv(lb0, landNum);
        uint64_t t = groupId / chunks0;
        res = t % lb1;
    }
    return res;
}

uint64_t Block::GetLC2(uint64_t groupId, uint64_t landNum, uint64_t lane)
{
    uint64_t res = 0;
    if (IsReduceDimension()) {
        uint64_t flat = groupId * landNum + lane;
        uint64_t t = flat / lb0;
        res = t / lb1;
    } else {
        uint64_t chunks0 = CeilDiv(lb0, landNum);
        uint64_t t = groupId / chunks0;
        res = t / lb1;
    }
    return res;
}

uint64_t Block::GetCurrentGroupID(uint64_t completedIters, uint64_t laneNum)
{
    if (laneNum == 0) {
        return 0;
    }
    // 允许 completedIters 落在末尾（== totalBodyIters）
    if (completedIters > totalBodyIters) {
        completedIters = totalBodyIters;
    }

    if (IsReduceDimension()) {
        // DimReduction: flat 后每 laneNum 个 iter 一个 group
        return completedIters / laneNum;
    }

    // MultiDim:
    // groupId = (lc2*lb1 + lc1) * chunks0 + chunk
    // 其中 chunks0 = ceil(lb0 / laneNum), chunk = floor(lc0 / laneNum)
    uint64_t chunks0 = CeilDiv(lb0, laneNum);

    uint64_t t     = (completedIters / lb0); // t = lc2*lb1 + lc1
    uint64_t off0  = (completedIters % lb0);
    uint64_t chunk = off0 / laneNum;

    return t * chunks0 + chunk;
}

uint64_t Block::GetCurrentGroupIters(uint64_t completedIters, uint64_t laneNum)
{
    if (laneNum == 0) {
        return 0;
    }
    if (completedIters >= totalBodyIters) {
        return 0;
    }
    if (IsReduceDimension()) {
        uint64_t remaining = totalBodyIters - completedIters;
        return (remaining < laneNum) ? remaining : laneNum;
    }
    // MultiDim：在当前 (lc2,lc1) 平面内，最多跑到本平面 lb0 结束
    uint64_t off0 = (completedIters % lb0);
    uint64_t remainingInPlane = (lb0 > off0) ? (lb0 - off0) : 0;
    // 同时也不能超过 totalBodyIters 的末尾
    uint64_t remainingTotal = totalBodyIters - completedIters;
    uint64_t remaining = (remainingInPlane < remainingTotal) ? remainingInPlane : remainingTotal;
    return (remaining < laneNum) ? remaining : laneNum;
}

}
