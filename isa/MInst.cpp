#include "MInst.h"

namespace JCore {
std::string Operand::Assemble(bool detail)
{
    std::stringstream oss;
    if (cvt != OPConvertType::OPCVT_NO_CVT || shamt != 0) {
        oss << "[";
    }
    oss << GetOperandType(type);
    switch (type) {
        case OperandType::OPD_GREG:
            oss << GetGPRNameRef(static_cast<GPR>(value));
            break;
        case OperandType::OPD_TLINK:
        case OperandType::OPD_ULINK: {
            if (!isDst) {
                oss << "#" << std::dec << (value + 1);
            }
            break;
        }
        case OperandType::OPD_VTLINK:
        case OperandType::OPD_VULINK:
        case OperandType::OPD_VMLINK:
        case OperandType::OPD_VNLINK:
            if (!isDst) {
                oss << "#" << std::dec << (value + 1) << (reuse && detail ? ".reuse" : "");
            }
            break;
        case OperandType::OPD_TILE_TLINK:
        case OperandType::OPD_TILE_ULINK:
        case OperandType::OPD_TILE_MLINK:
        case OperandType::OPD_TILE_NLINK:
        case OperandType::OPD_TILE_ACC:
        case OperandType::OPD_TILE_STACK:
        case OperandType::OPD_TILE_DLINK: {
            if (!isDst) {
                oss << "#" << std::dec << (value + 1) << (reuse && detail ? ".reuse" : "");
            } else {
                oss << "<";
                if (size % NUM1024 == 0) {
                    oss << std::dec << (size / NUM1024) << "KB";
                } else {
                    oss << std::dec << size << "B";
                }
                oss << ">";
            }
            break;
        }
        case OperandType::OPD_SYS:
            oss << std::dec << value << "(" << GetSysRegName(value) << ")";
            break;
        case OperandType::OPD_RI:
        case OperandType::OPD_RO:
            oss << std::dec << value;
            break;
        case OperandType::OPD_SIMM:
            oss << std::dec << static_cast<int64_t>(value);
            break;
        case OperandType::OPD_UIMM:
            oss << "0x" << std::hex << value;
            break;
        default:
            break;
    }
    if (cvt != OPConvertType::OPCVT_NO_CVT || shamt != 0) {
        if (cvt != OPConvertType::OPCVT_NO_CVT) {
            oss << GetOperandConvertType(cvt);
        }
        if (shamt != 0) {
            oss << "<<" << std::dec << shamt;
        }
        oss << "]";
    }
    return oss.str();
}

std::string Operand::Dump(bool detail)
{
    std::stringstream oss;
    oss << Assemble(detail);
    if (!OperandTypeIsImm(type) && !OperandTypeIsTile(type)) {
        oss << ":0x" << std::hex << data;
    }
    return oss.str();
}

std::string MInst::Assemble()
{
    std::stringstream oss;
    oss << GetEncodeLenName(codeLen, OpcodeManager::Inst().IsNeedAccumulateBlockInfo(opcode));
    oss << OpcodeManager::Inst().GetOpcodeStr(opcode);
    if (!assembleStr.empty()) {
        oss << assembleStr;
        return oss.str();
    }
    if (accMemInfo != nullptr) {
        oss << accMemInfo->Assemble();
    }
    if (blockHint != nullptr) {
        oss << " " << blockHint->Dump();
    }
    if (atomicInfo != nullptr) {
        oss << "." << atomicInfo->Dump();
    }
    if (!srcs.empty()) {
        oss << " ";
    }
    size_t size = srcs.size();
    for (size_t i = 0; i < size; i++) {
        oss << srcs[i]->Assemble();
        if (i != size - 1) {
            oss << ", ";
        }
    }
    if (!dsts.empty()) {
        oss << " -> ";
    }
    size = dsts.size();
    for (size_t i = 0; i < size; i++) {
        oss << dsts[i]->Assemble();
        if (i != size - 1) {
            oss << ", ";
        }
    }
    return oss.str();
}

std::string MInst::Dump()
{
    std::stringstream oss;
    oss << Assemble();
    return oss.str();
}

MInst::MInst(const MInst& other)
    : binary(other.binary),
        pc(other.pc),
        opcode(other.opcode),
        codeLen(other.codeLen),
        srcs(other.srcs),
        dsts(other.dsts),
        ctGen(other.ctGen),
        check(other.check),
        iotLast(other.iotLast),
        frm(other.frm),
        laneID(other.laneID)
{
    if (other.blockHint) {
        blockHint = std::make_shared<BlockHintInfo>(*other.blockHint);
    } else {
        blockHint = nullptr;
    }

    if (other.accMemInfo) {
        accMemInfo = std::make_shared<AaccelssMemInfo>(*other.accMemInfo);
    } else {
        accMemInfo = nullptr;
    }

    if (other.atomicInfo) {
        atomicInfo = std::make_shared<AtomicInfo>(*other.atomicInfo);
    } else {
        atomicInfo = nullptr;
    }

    if (other.setcInfo) {
        setcInfo = std::make_shared<SetcInfo>(*other.setcInfo);
    } else {
        setcInfo = nullptr;
    }

    if (other.ssrInfo) {
        ssrInfo = std::make_shared<SSRSetInfo>(*other.ssrInfo);
    } else {
        ssrInfo = nullptr;
    }

    if (other.reduceInfo) {
        reduceInfo = std::make_shared<ReduceInfo>(*other.reduceInfo);
    } else {
        reduceInfo = nullptr;
    }

    if (other.brInfo) {
        brInfo = std::make_shared<BranchInfo>(*other.brInfo);
    } else {
        brInfo = nullptr;
    }
}

MInst& MInst::operator=(const MInst& other)
{
    if (this != &other) {
        binary = other.binary;
        pc = other.pc;
        opcode = other.opcode;
        codeLen = other.codeLen;
        srcs = other.srcs;
        dsts = other.dsts;
        ctGen = other.ctGen;
        check = other.check;
        iotLast = other.iotLast;
        frm = other.frm;
        laneID = other.laneID;

        blockHint = other.blockHint ? std::make_shared<BlockHintInfo>(*other.blockHint) : nullptr;
        accMemInfo = other.accMemInfo ? std::make_shared<AaccelssMemInfo>(*other.accMemInfo) : nullptr;
        atomicInfo = other.atomicInfo ? std::make_shared<AtomicInfo>(*other.atomicInfo) : nullptr;
        setcInfo = other.setcInfo ? std::make_shared<SetcInfo>(*other.setcInfo) : nullptr;
        ssrInfo = other.ssrInfo ? std::make_shared<SSRSetInfo>(*other.ssrInfo) : nullptr;
        reduceInfo = other.reduceInfo ? std::make_shared<ReduceInfo>(*other.reduceInfo) : nullptr;
        brInfo = other.brInfo ? std::make_shared<BranchInfo>(*other.brInfo) : nullptr;
    }
    return *this;
}
uint64_t MInst::GetResult()
{
    InstGroup grp = OpcodeManager::Inst().GetOpcodeGroup(opcode);
    switch (grp) {
        case InstGroup::STORE:
            return accMemInfo->accMemAddr;
        case InstGroup::SETC:
            return setcInfo->setcTaken;
        case InstGroup::SSR:
            if (opcode == Opcode::OP_SETC_TGT) {
                return setcInfo->setcTarget;
            }
            break;
        default:
            break;
    }
    return dsts.size() >= DST1_IDX ? dsts[DST0_IDX]->data : 0;
};

uint64_t MInst::GetFallPC()
{
    return pc + GetEncodeLenVal(codeLen);
}

std::string MInst::GetAssembleStr()
{
    return assembleStr;
}

void MInst::SetAssembleStr(std::string str)
{
    assembleStr = std::move(str);
}


std::string AtomicInfo::Dump()
{
    std::stringstream oss;
    if (aq) {
        oss << "aq";
    }
    if (rl) {
        oss << "rl";
    }
    if (far) {
        oss << "far";
    }
    return oss.str();
}

void MInst::DecodeBin(uint64_t bin, uint32_t size)
{
    InstInfo* instInfo = nullptr;

    if (IsBSTOP(bin, size)) {
        this->opcode = Opcode::OP_BSTOP;
        this->codeLen = static_cast<EncodeLen>(size);
        return;
    }

    if (size == WIDTH_16) {
        instInfo = JCore::DecodeInst16(bin);
    } else if (size == WIDTH_32) {
        instInfo = JCore::DecodeInst32(bin);
    } else if (size == WIDTH_48) {
        instInfo = JCore::DecodeInst48(bin);
    } else if (size == WIDTH_64) {
        instInfo = JCore::DecodeInst64(bin);
    } else {
        std::cout << "size: " << std::dec << size << std::endl;
        ASSERT(0);
    }
    InstInfo2MInst(instInfo, bin, static_cast<EncodeLen>(size));
    ConvertImm2Data();
}

void MInst::ConvertImm2Data()
{
    for (auto &src : srcs) {
        switch (src->type) {
            case OperandType::OPD_SIMM:
                src->data = static_cast<int64_t>(src->value);
                break;
            case OperandType::OPD_UIMM:
                src->data = src->value;
                break;
            default:
                break;
        }
    }
}

void MInst::InstInfo2MInst(InstInfo* instInfo, uint64_t bin, EncodeLen encodeLen)
{
    opcode = instInfo->opcode;
    codeLen = encodeLen;
    binary = bin;
    switch (codeLen) {
        case EncodeLen::ENL_C:
        case EncodeLen::ENL_W:
        case EncodeLen::ENL_H:
            SetOperand(instInfo);
            break;
        case EncodeLen::ENL_L:
            if (GetBits(bin, 6, 1) == 0b000111) {
                SetOperand(instInfo);
            } else {    // 向量指令
                SetSimtOperand(instInfo);
            }
            break;
        default:
            break;
    }

    InitInstGroupInfo(instInfo);
    if (IsNeedMergeShamtToSrcR()) {
        MergeSrcR();
    }
    delete instInfo;
}

void MInst::InitInstGroupInfo(InstInfo* instInfo)
{
    InstGroup grp = OpcodeManager::Inst().GetOpcodeGroup(opcode);
    switch (grp) {
        case InstGroup::LOAD:
        case InstGroup::STORE:
        case InstGroup::PREFETCH:
        case InstGroup::CACHE_MAINTAIN: {
            if (instInfo != nullptr) {
                accMemInfo = std::make_shared<AaccelssMemInfo>(instInfo->continuous, instInfo->local);
            } else {
                accMemInfo = std::make_shared<AaccelssMemInfo>(false, false);
            }
            break;
        }
        case InstGroup::ATOMIC:
            if (instInfo != nullptr) {
                atomicInfo = std::make_shared<AtomicInfo>(instInfo->aq, instInfo->rl, instInfo->far);
            } else {
                atomicInfo = std::make_shared<AtomicInfo>();
            }
            accMemInfo = std::make_shared<AaccelssMemInfo>();
            break;
        case InstGroup::SETC:
            setcInfo = std::make_shared<SetcInfo>();
            break;
        case InstGroup::SSR: {
            ssrInfo = std::make_shared<SSRSetInfo>();
            if (opcode == Opcode::OP_SETC_TGT) {
                setcInfo = std::make_shared<SetcInfo>();
            }
            break;
        }
        case InstGroup::REDUCE:
            reduceInfo = std::make_shared<ReduceInfo>();
            break;
        case InstGroup::BRANCH:
            brInfo = std::make_shared<BranchInfo>();
            break;
        case InstGroup::BLOCK_SPLIT: {
            if (opcode == Opcode::OP_B_HINT_TRACE) {
                blockHint = std::make_shared<BlockHintInfo>();
                blockHint->InitTrace(srcs[SRC0_IDX]->value);
            }
            break;
        }
        case InstGroup::BLOCK_HINT: {
            if (opcode == Opcode::OP_B_HINT_PREFETCH) {
                blockHint = std::make_shared<BlockHintInfo>();
                blockHint->InitPref(srcs[SRC0_IDX]->value, srcs[SRC1_IDX]->value, srcs[SRC2_IDX]->value,
                                    srcs[SRC3_IDX]->value);
            }
            break;
        }
        case InstGroup::CONVERT: {
            floatInfo = std::make_shared<FloatInfo>();
            if (codeLen == EncodeLen::ENL_V || codeLen == EncodeLen::ENL_L) {
                std::vector<std::shared_ptr<Operand>> tmp = srcs;
                srcs.clear();
                srcs.push_back(tmp[SRC0_IDX]);
                srcs.push_back(tmp[SRC1_IDX]);
                floatInfo->rm = static_cast<FRMMode>(tmp[SRC2_IDX]->value);
                floatInfo->mode = static_cast<FCVTMode>(tmp[SRC3_IDX]->value);
                floatInfo->sat = static_cast<Saturation>(instInfo->sat);
            }
            break;
        }
        default:
            break;
    }
}

void MInst::InitFRM(FRMMode sysFRM)
{
    switch (opcode) {
        case Opcode::OP_FCVT:
        case Opcode::OP_SCVTF:
        case Opcode::OP_UCVTF:
        case Opcode::OP_ICVT:
        case Opcode::OP_ICVTF:
            frm = sysFRM;
            break;
        case Opcode::OP_FCVTA:
            frm = FRMMode::FRM_RNA;
            break;
        case Opcode::OP_FCVTM:
            frm = FRMMode::FRM_RDN;
            break;
        case Opcode::OP_FCVTN:
            frm = FRMMode::FRM_RNE;
            break;
        case Opcode::OP_FCVTP:
            frm = FRMMode::FRM_RUP;
            break;
        case Opcode::OP_FCVTZ:
        case Opcode::OP_FCVTI:
            frm = FRMMode::FRM_RTZ;
            break;
        default:
            break;
    }
}

static const std::unordered_map<uint32_t, OPConvertType> SCVTMap = {
    {0, OPConvertType::OPCVT_S64},
    {1, OPConvertType::OPCVT_S32},
    {2, OPConvertType::OPCVT_S16},
    {3, OPConvertType::OPCVT_S8}
};

static const std::unordered_map<uint32_t, OPConvertType> UCVTMap = {
    {0, OPConvertType::OPCVT_U64},
    {1, OPConvertType::OPCVT_U32},
    {2, OPConvertType::OPCVT_U16},
    {3, OPConvertType::OPCVT_U8}
};

static const std::unordered_map<uint32_t, OPConvertType> SUVTMap = {
    {0, OPConvertType::OPCVT_U64},
    {1, OPConvertType::OPCVT_U32},
    {2, OPConvertType::OPCVT_U16},
    {3, OPConvertType::OPCVT_U8},
    {4, OPConvertType::OPCVT_S64},
    {5, OPConvertType::OPCVT_S32},
    {6, OPConvertType::OPCVT_S16},
    {7, OPConvertType::OPCVT_S8}
};

static const std::unordered_map<uint32_t, OPConvertType> FCVTMap = {
    {0, OPConvertType::OPCVT_FP64},
    {1, OPConvertType::OPCVT_FP32},
    {2, OPConvertType::OPCVT_FP16},
    {3, OPConvertType::OPCVT_FP8}
};

static const std::unordered_map<uint32_t, OPConvertType> L_FMap = {
    {0, OPConvertType::OPCVT_FP64},
    {1, OPConvertType::OPCVT_FP32},
    {2, OPConvertType::OPCVT_TF32},
    {3, OPConvertType::OPCVT_HF32},
    {4, OPConvertType::OPCVT_FP16},
    {5, OPConvertType::OPCVT_BF16},
    {6, OPConvertType::OPCVT_HIF8},
    {7, OPConvertType::OPCVT_FP8},
    {8, OPConvertType::OPCVT_FP8_1},
    {9, OPConvertType::OPCVT_FP6},
    {10, OPConvertType::OPCVT_FP6_1},
    {11, OPConvertType::OPCVT_FP4},
    {12, OPConvertType::OPCVT_FP4_1},
    {13, OPConvertType::OPCVT_SF8},
    {14, OPConvertType::OPCVT_HIF4}
};

static const std::unordered_map<uint32_t, OPConvertType> L_IMap = {
    {0, OPConvertType::OPCVT_U64},
    {1, OPConvertType::OPCVT_U32},
    {2, OPConvertType::OPCVT_U16},
    {3, OPConvertType::OPCVT_U8},
    {4, OPConvertType::OPCVT_U4},
    {8, OPConvertType::OPCVT_S64},
    {9, OPConvertType::OPCVT_S32},
    {10, OPConvertType::OPCVT_S16},
    {11, OPConvertType::OPCVT_S8},
    {12, OPConvertType::OPCVT_S4}
};

// SrcRType encoding matches the compiler/QEMU contract:
// 0=.sw, 1=.uw, 2=.neg/.not, 3=no modifier.
static OPConvertType GetSrcRCvtTypeA(uint32_t srcRType)
{
    switch(srcRType) {
        case 0:
            return OPConvertType::OPCVT_S32;
        case 1:
            return OPConvertType::OPCVT_U32;
        case 2:
            return OPConvertType::OPCVT_NEG;
        default:
            return OPConvertType::OPCVT_NO_CVT;
    }
    return OPConvertType::OPCVT_NO_CVT;
}

// SrcRType encoding matches the compiler/QEMU contract:
// 0=.sw, 1=.uw, 2=.neg/.not, 3=no modifier.
static OPConvertType GetSrcRCvtTypeB(uint32_t srcRType)
{
    switch(srcRType) {
        case 0:
            return OPConvertType::OPCVT_S32;
        case 1:
            return OPConvertType::OPCVT_U32;
        case 2:
            return OPConvertType::OPCVT_NOT;
        default:
            return OPConvertType::OPCVT_NO_CVT;
    }
    return OPConvertType::OPCVT_NO_CVT;
}

// 设置32位cvt类指令的srcR的cvt类型
static OPConvertType SetSrcRCvt(Opcode opcode, uint32_t srcRType)
{
    switch (opcode) {
        // 32bit-sw,uw,neg
        // 64bit add sub csel
        case JCore::Opcode::OP_ADD:
        case JCore::Opcode::OP_SUB:
        case JCore::Opcode::OP_ADDW:
        case JCore::Opcode::OP_SUBW:
            return GetSrcRCvtTypeA(srcRType);
        // 32bit-sw,uw,not
        // 64bit and or xor
        case JCore::Opcode::OP_AND:
        case JCore::Opcode::OP_OR:
        case JCore::Opcode::OP_XOR:
        case JCore::Opcode::OP_ANDW:
        case JCore::Opcode::OP_ORW:
        case JCore::Opcode::OP_XORW:
        case JCore::Opcode::OP_CMP_AND:
        case JCore::Opcode::OP_CMP_OR:
        case JCore::Opcode::OP_SETC_AND:
        case JCore::Opcode::OP_SETC_OR:
            return GetSrcRCvtTypeB(srcRType);
        // 32bit-sw,uw
        case JCore::Opcode::OP_CMP_EQ:
        case JCore::Opcode::OP_CMP_NE:
        case JCore::Opcode::OP_CMP_LT:
        case JCore::Opcode::OP_CMP_GE:
        case JCore::Opcode::OP_CMP_LTU:
        case JCore::Opcode::OP_CMP_GEU:
        case JCore::Opcode::OP_SETC_EQ:
        case JCore::Opcode::OP_SETC_NE:
        case JCore::Opcode::OP_SETC_LT:
        case JCore::Opcode::OP_SETC_GE:
        case JCore::Opcode::OP_SETC_LTU:
        case JCore::Opcode::OP_SETC_GEU:
        case JCore::Opcode::OP_LB:
        case JCore::Opcode::OP_LH:
        case JCore::Opcode::OP_LW:
        case JCore::Opcode::OP_LD:
        case JCore::Opcode::OP_LBU:
        case JCore::Opcode::OP_LHU:
        case JCore::Opcode::OP_LWU:
        case JCore::Opcode::OP_SB:
        case JCore::Opcode::OP_SH:
        case JCore::Opcode::OP_SW:
        case JCore::Opcode::OP_SD:
        case JCore::Opcode::OP_SH_U:
        case JCore::Opcode::OP_SW_U:
        case JCore::Opcode::OP_SD_U:
            return GetSrcRCvtTypeA(srcRType);
        // 32bit-neg
        case JCore::Opcode::OP_CSEL:
            return GetSrcRCvtTypeA(srcRType);
        default:
            return OPConvertType::OPCVT_NO_CVT;
    }
}

// 设置32位cvt类指令的源cvt类型
static OPConvertType SetSrcCvt(Opcode opcode, uint32_t srcType)
{
    if (opcode == Opcode::OP_SCVTF) {
        auto opcode = SCVTMap.find(srcType & 0x3);
        if (opcode == SCVTMap.end()) {
            return OPConvertType::OPCVT_NO_CVT;
        } else {
            return opcode->second;
        }
    } else if (opcode == Opcode::OP_UCVTF) {
        auto opcode = UCVTMap.find(srcType & 0x3);
        if (opcode == UCVTMap.end()) {
            return OPConvertType::OPCVT_NO_CVT;
        } else {
            return opcode->second;
        }
    } else {
        auto opcode = FCVTMap.find(srcType & 0x3);
        if (opcode == FCVTMap.end()) {
            return OPConvertType::OPCVT_NO_CVT;
        } else {
            return opcode->second;
        }
    }
    return OPConvertType::OPCVT_NO_CVT;
}

// 设置32位cvt类指令的目的cvt类型
static OPConvertType SetDstCvt(Opcode opcode, uint32_t dstType)
{
    if (opcode == Opcode::OP_FCVT || opcode == Opcode::OP_SCVTF || opcode == Opcode::OP_UCVTF) {
        auto opcode = FCVTMap.find(dstType & 0x7);
        if (opcode == FCVTMap.end()) {
            return OPConvertType::OPCVT_NO_CVT;
        } else {
            return opcode->second;
        }
    } else {
        auto opcode = SUVTMap.find(dstType & 0x7);
        if (opcode == SUVTMap.end()) {
            return OPConvertType::OPCVT_NO_CVT;
        } else {
            return opcode->second;
        }
    }
    return OPConvertType::OPCVT_NO_CVT;
}

// 设置64位cvt类指令的源cvt类型
static OPConvertType SetSimtSrcCvt(Opcode opcode, uint32_t srcType)
{
    if (opcode == Opcode::OP_FCVT) {
        switch (srcType) {
            case CVTF_FP16x2:
                return OPConvertType::OPCVT_FP16_X_2;
            case CVTF_FP64:
                return OPConvertType::OPCVT_FP64;
            case CVTF_FP32:
                return OPConvertType::OPCVT_FP32;
            case CVTF_BF16:
                return OPConvertType::OPCVT_BF16;
            case CVTF_E5M2:
                return OPConvertType::OPCVT_FP8_1;
            case CVTF_FP16:
                return OPConvertType::OPCVT_FP16;
            case CVTF_E4M3:
                return OPConvertType::OPCVT_FP8;
            case CVTF_BF16x2:
                return OPConvertType::OPCVT_BF16_X_2;
            case CVTF_HIF4x2:
                return OPConvertType::OPCVT_HIF4_X_2;
            case CVTF_E6M2:
                return OPConvertType::OPCVT_E6M2;
            case CVTF_E8M0:
                return OPConvertType::OPCVT_SF8;
            case CVTF_TF32:
                return OPConvertType::OPCVT_TF32;
            case CVTF_E1M2x2:
                return OPConvertType::OPCVT_E1M2_X_2;
            case CVTF_HF32:
            case CVTF_HIF8:
            case CVTF_E3M2:
            case CVTF_E2M3:
            case CVTF_E2M1x2:
            case CVTF_E4M3x4:
            default:
                return OPConvertType::OPCVT_NO_CVT;
        }
    } else if (opcode == Opcode::OP_FCVTI) {
        auto srcCvtType = L_FMap.find(srcType);
        if (srcCvtType == L_FMap.end()) {
            return OPConvertType::OPCVT_NO_CVT;
        } else {
            return srcCvtType->second;
        }
    } else if (opcode == Opcode::OP_ICVT || opcode == Opcode::OP_ICVTF) {
        auto srcCvtType = L_IMap.find(srcType);
        if (srcCvtType == L_IMap.end()) {
            return OPConvertType::OPCVT_NO_CVT;
        } else {
            return srcCvtType->second;
        }
    }
    return OPConvertType::OPCVT_NO_CVT;
}

// 设置64位cvt类指令的目的cvt类型
static OPConvertType SetSimtDstCvt(Opcode opcode, uint32_t dstType)
{
    if (opcode == Opcode::OP_FCVT) {
        switch (dstType) {
            case CVTF_FP16x2:
                return OPConvertType::OPCVT_FP16_X_2;
            case CVTF_FP64:
                return OPConvertType::OPCVT_FP64;
            case CVTF_FP32:
                return OPConvertType::OPCVT_FP32;
            case CVTF_BF16:
                return OPConvertType::OPCVT_BF16;
            case CVTF_E5M2:
                return OPConvertType::OPCVT_FP8_1;
            case CVTF_FP16:
                return OPConvertType::OPCVT_FP16;
            case CVTF_E4M3:
                return OPConvertType::OPCVT_FP8;
            case CVTF_BF16x2:
                return OPConvertType::OPCVT_BF16_X_2;
            case CVTF_HIF4x2:
                return OPConvertType::OPCVT_HIF4_X_2;
            case CVTF_E6M2:
                return OPConvertType::OPCVT_E6M2;
            case CVTF_E8M0:
                return OPConvertType::OPCVT_SF8;
            case CVTF_TF32:
                return OPConvertType::OPCVT_TF32;
            case CVTF_E1M2x2:
                return OPConvertType::OPCVT_E1M2_X_2;
            case CVTF_HF32:
            case CVTF_HIF8:
            case CVTF_E3M2:
            case CVTF_E2M3:
            case CVTF_E2M1x2:
            case CVTF_E4M3x4:
            default:
                return OPConvertType::OPCVT_NO_CVT;
        }
    } else if (opcode == Opcode::OP_ICVTF) {
        auto dstCvtType = L_FMap.find(dstType);
        if (dstCvtType == L_FMap.end()) {
            return OPConvertType::OPCVT_NO_CVT;
        } else {
            return dstCvtType->second;
        }
    } else if (opcode == Opcode::OP_FCVTI || opcode == Opcode::OP_ICVT) {
        auto dstCvtType = L_IMap.find(dstType);
        if (dstCvtType == L_IMap.end()) {
            return OPConvertType::OPCVT_NO_CVT;
        } else {
            return dstCvtType->second;
        }
    }
    return OPConvertType::OPCVT_NO_CVT;
}

void MInst::SetOperand(InstInfo* instInfo)
{
    // 检查数据合法性
    // B.IOR 中 src/dst 表示 invalid 的编码和 zero 相同
    auto CheckS64Valid = [this](int64_t value) -> bool {
        if (this->opcode == Opcode::OP_B_IOR) {
            return value != 0 && value != INVALID_VALUE64;
        }
        return value != INVALID_VALUE64;
    };

    auto CheckS32Valid = [this](int32_t value) -> bool {
        if (this->opcode == Opcode::OP_B_IOR) {
            return value != 0 && value != INVALID32;
        }
        return value != INVALID32;
    };

    if (CheckS64Valid(instInfo->src0)) {
        OperandPtr src0 = std::make_shared<Operand>();
        SetSrcOperand(*src0, instInfo->src0, instInfo->src0Type);
        src0->shamt = instInfo->src0Shamt;
        srcs.push_back(src0);
    }
    if (CheckS64Valid(instInfo->src1)) {
        OperandPtr src1 = std::make_shared<Operand>();
        SetSrcOperand(*src1, instInfo->src1, instInfo->src1Type);
        if (instInfo->srcRIndex == SRC1_IDX && CheckS32Valid(instInfo->srcRType)) {
            src1->cvt = SetSrcRCvt(opcode, static_cast<uint32_t>(instInfo->srcRType));
        }
        src1->shamt = instInfo->src1Shamt;
        srcs.push_back(src1);
    }
    if (CheckS64Valid(instInfo->src2)) {
        OperandPtr src2 = std::make_shared<Operand>();
        SetSrcOperand(*src2, instInfo->src2, instInfo->src2Type);
        if (instInfo->srcRIndex == SRC2_IDX && CheckS32Valid(instInfo->srcRType)) {
            src2->cvt = SetSrcRCvt(opcode, static_cast<uint32_t>(instInfo->srcRType));
        }
        src2->shamt = instInfo->src2Shamt;
        srcs.push_back(src2);
    }
    if (CheckS64Valid(instInfo->src3)) {
        OperandPtr src3 = std::make_shared<Operand>();
        SetSrcOperand(*src3, instInfo->src3, instInfo->src3Type);
        srcs.push_back(src3);
    }
    if (CheckS64Valid(instInfo->src4)) {
        OperandPtr src4 = std::make_shared<Operand>();
        SetSrcOperand(*src4, instInfo->src4, instInfo->src4Type);
        srcs.push_back(src4);
    }
    if (CheckS64Valid(instInfo->src5)) {
        OperandPtr src5 = std::make_shared<Operand>();
        SetSrcOperand(*src5, instInfo->src5, instInfo->src5Type);
        srcs.push_back(src5);
    }
    if (CheckS64Valid(instInfo->src6)) {
        OperandPtr src6 = std::make_shared<Operand>();
        SetSrcOperand(*src6, instInfo->src6, instInfo->src6Type);
        srcs.push_back(src6);
    }
    if (CheckS64Valid(instInfo->src7)) {
        OperandPtr src7 = std::make_shared<Operand>();
        SetSrcOperand(*src7, instInfo->src7, instInfo->src7Type);
        srcs.push_back(src7);
    }
    if (CheckS64Valid(instInfo->src8)) {
        OperandPtr src8 = std::make_shared<Operand>();
        SetSrcOperand(*src8, instInfo->src8, instInfo->src8Type);
        srcs.push_back(src8);
    }
    if (CheckS64Valid(instInfo->src9)) {
        OperandPtr src9 = std::make_shared<Operand>();
        SetSrcOperand(*src9, instInfo->src9, instInfo->src9Type);
        srcs.push_back(src9);
    }
    if (CheckS64Valid(instInfo->dst0)) {
        OperandPtr dst0 = std::make_shared<Operand>();
        SetDstOperand(*dst0, instInfo->dst0, instInfo->dst0Type);
        if (dst0->type != OperandType::OPD_INVALID) {
            dsts.push_back(dst0);
        }
    }
    if (CheckS64Valid(instInfo->dst1)) {
        OperandPtr dst1 = std::make_shared<Operand>();
        SetDstOperand(*dst1, instInfo->dst1, instInfo->dst1Type);
        dsts.push_back(dst1);
    }
    if (CheckS32Valid(instInfo->srcType)) {
        for (auto &i : srcs) {
            i->cvt = SetSrcCvt(opcode, static_cast<uint32_t>(instInfo->srcType));
        }
    }
    if (CheckS32Valid(instInfo->dstType)) {
        for (auto &i : dsts) {
            i->cvt = SetDstCvt(opcode, static_cast<uint32_t>(instInfo->dstType));
        }
    }
    if (CheckS32Valid(instInfo->tileFunc)) {
        auto it = srcs.begin() + 1;
        // 后面再封装, 先保证正确性
        if (instInfo->tileFunc == 0b100) {
            (*it)->reuse = (instInfo->s0R == 0) ? false : true;
            ++it;
            (*it)->reuse = (instInfo->s1R == 0) ? false : true;
        } else if (instInfo->tileFunc == 0b101) {
            (*it)->reuse = (instInfo->s0R == 0) ? false : true;
            srcs.erase(it + 1);
        } else if (instInfo->tileFunc == 0b110) {
            srcs.erase(srcs.begin() + 1, srcs.begin() + 3);
        }
    }
    if (CheckS32Valid(instInfo->last)) {
        iotLast = (instInfo->last == 1) ? true : false;
    }
}

bool MInst::IsNeedMergeShamtToSrcR()
{
    if (OpcodeManager::Inst().IsOpcodeSrcRNeedMerge(opcode)) {
        return true;
    }
    if (codeLen == EncodeLen::ENL_L || codeLen == EncodeLen::ENL_V) {
        if (OpcodeManager::Inst().GetOpcodeGroup(opcode) == InstGroup::STORE) {
            static const std::unordered_set<Opcode> STORE_NEED_SHAMT_SET {
                Opcode::OP_SB, Opcode::OP_SH, Opcode::OP_SW, Opcode::OP_SD, Opcode::OP_SH_U, Opcode::OP_SW_U,
                Opcode::OP_SD_U,
            };
            return STORE_NEED_SHAMT_SET.find(opcode) != STORE_NEED_SHAMT_SET.end() || accMemInfo->continuous;
        }
    }
    return false;
}

void MInst::MergeSrcR()
{
    if (codeLen == EncodeLen::ENL_C) {
        return;
    }
    InstGroup grp = OpcodeManager::Inst().GetOpcodeGroup(opcode);
    switch (grp) {
        case InstGroup::ARITHMETIC:
        case InstGroup::LOAD: {
            std::vector<OperandPtr> tmp = srcs;
            srcs.clear();
            srcs.push_back(tmp[SRC0_IDX]);
            if (accMemInfo == nullptr || !accMemInfo->continuous) {
                tmp[SRC1_IDX]->shamt = tmp[SRC2_IDX]->value;
                srcs.push_back(tmp[SRC1_IDX]);
            } else {
                // src1 is LC0
                srcs.push_back(tmp[SRC1_IDX]);
                tmp[SRC2_IDX]->shamt = tmp[SRC3_IDX]->value;
                srcs.push_back(tmp[SRC2_IDX]);
            }
            break;
        }
        case InstGroup::STORE: {
            std::vector<OperandPtr> tmp = srcs;
            srcs.clear();
            srcs.push_back(tmp[SRC0_IDX]);
            if (accMemInfo == nullptr || !accMemInfo->continuous) {
                srcs.push_back(tmp[SRC1_IDX]);
                tmp[SRC2_IDX]->shamt = tmp[SRC3_IDX]->value;
                srcs.push_back(tmp[SRC2_IDX]);
            } else {
                // src1 is LC0
                if (tmp.size() > SRC4_IDX) {
                    // need merge shamt and swap LC0 location
                    srcs.push_back(tmp[SRC2_IDX]);
                    srcs.push_back(tmp[SRC1_IDX]);
                    tmp[SRC3_IDX]->shamt = tmp[SRC4_IDX]->value;
                    srcs.push_back(tmp[SRC3_IDX]);
                } else {
                    // swap LC0 location
                    srcs.push_back(tmp[SRC2_IDX]);
                    srcs.push_back(tmp[SRC1_IDX]);
                    srcs.push_back(tmp[SRC3_IDX]);
                }
            }
            break;
        }
        case InstGroup::SETC: {
            std::vector<OperandPtr> tmp = srcs;
            srcs.clear();
            srcs.push_back(tmp[SRC1_IDX]); // srcL
            tmp[SRC2_IDX]->shamt = tmp[SRC0_IDX]->value;
            srcs.push_back(tmp[SRC2_IDX]);
            break;
        }
        default:
            LOG_INFO << "\n" << Dump();
            ASSERT(false && "undefined");
            return;
    }
}

void MInst::SetSimtOperand(InstInfo* instInfo)
{
    // 检查数据合法性
    auto CheckS64Valid = [](int64_t value) -> bool {
        return (value != INVALID_VALUE64);
    };

    auto CheckS32Valid = [](int32_t value) -> bool {
        return (value != INVALID32);
    };

    if (CheckS64Valid(instInfo->src0)) {
        OperandPtr src0 = std::make_shared<Operand>();
        SetSrcOperandSimt(*src0, instInfo->src0, instInfo->src0Type);
        if (CheckS64Valid(instInfo->src0InstWidth)) {
            SetSrcCVTndWIDTH(*src0, opcode, instInfo->src0InstWidth);
        }
        srcs.push_back(src0);
    }
    if (CheckS32Valid(instInfo->continuous) && instInfo->continuous == static_cast<int32_t>(1)) {
        OperandPtr lc0 = std::make_shared<Operand>(OperandType::OPD_LC0, 0);
        lc0->shamt = instInfo->lc0Shamt;
        lc0->isDst = false;
        srcs.push_back(lc0);
    }
    if (CheckS64Valid(instInfo->src1)) {
        OperandPtr src1 = std::make_shared<Operand>();
        SetSrcOperandSimt(*src1, instInfo->src1, instInfo->src1Type);
        if (CheckS64Valid(instInfo->src1InstWidth)) {
            SetSrcCVTndWIDTH(*src1, opcode, instInfo->src1InstWidth);
        }
        if (instInfo->srcRIndex == SRC1_IDX && CheckS32Valid(instInfo->srcRType)) {
            src1->cvt = SetSrcRCvt(opcode, static_cast<uint32_t>(instInfo->srcRType));
        }
        srcs.push_back(src1);
    }
    if (CheckS64Valid(instInfo->src2)) {
        OperandPtr src2 = std::make_shared<Operand>();
        SetSrcOperandSimt(*src2, instInfo->src2, instInfo->src2Type);
        if (CheckS64Valid(instInfo->src2InstWidth)) {
            SetSrcCVTndWIDTH(*src2, opcode, instInfo->src2InstWidth);
        }
        if (instInfo->srcRIndex == SRC2_IDX && CheckS32Valid(instInfo->srcRType)) {
            src2->cvt = SetSrcRCvt(opcode, static_cast<uint32_t>(instInfo->srcRType));
        }
        srcs.push_back(src2);
    }
    if (CheckS64Valid(instInfo->src3)) {
        OperandPtr src3 = std::make_shared<Operand>();
        SetSrcOperandSimt(*src3, instInfo->src3, instInfo->src3Type);
        if (CheckS64Valid(instInfo->src3InstWidth)) {
            SetSrcCVTndWIDTH(*src3, opcode, instInfo->src3InstWidth);
        }
        srcs.push_back(src3);
    }
    if (CheckS64Valid(instInfo->src4)) {
        OperandPtr src4 = std::make_shared<Operand>();
        SetSrcOperandSimt(*src4, instInfo->src4, instInfo->src4Type);
        if (CheckS64Valid(instInfo->src4InstWidth)) {
            SetSrcCVTndWIDTH(*src4, opcode, instInfo->src4InstWidth);
        }
        srcs.push_back(src4);
    }
    if (CheckS64Valid(instInfo->dst0)) {
        OperandPtr dst0 = std::make_shared<Operand>();
        SetDstOperandSimt(*dst0, instInfo->dst0);
        if (CheckS64Valid(instInfo->dst0InstWidth)) {
            SetDstCVTndWIDTH(*dst0, opcode, instInfo->dst0InstWidth);
        }
        dsts.push_back(dst0);
    }
    if (CheckS64Valid(instInfo->dst1)) {
        OperandPtr dst1 = std::make_shared<Operand>();
        SetDstOperandSimt(*dst1, instInfo->dst1);
        if (CheckS64Valid(instInfo->dst1InstWidth)) {
            SetDstCVTndWIDTH(*dst1, opcode, instInfo->dst1InstWidth);
        }
        dsts.push_back(dst1);
    }
    if (CheckS32Valid(instInfo->srcType)) {
        for (auto &i : srcs) {
            i->cvt = SetSimtSrcCvt(opcode, static_cast<uint32_t>(instInfo->srcType));
        }
    }
    if (CheckS32Valid(instInfo->dstType)) {
        for (auto &i : dsts) {
            i->cvt = SetSimtDstCvt(opcode, static_cast<uint32_t>(instInfo->dstType));
        }
    }
    if (CheckS32Valid(instInfo->pack)) {
        const int len[4] = {1, 2, 4};
        vlen = len[instInfo->pack];
    }
    if (instInfo->sat == 1) {
        std::cout << " Currently, the sat flag is not supported." << std::endl;
        sat = 1;
        assert(false);
    }
    DetermineIsSimt();
}

// 对向量指令源操作数进行cvt及width设置
void MInst::SetSrcCVTndWIDTH(Operand& operand, Opcode opcode, int32_t instSTndWidth)
{
    // TODO: FMAX/FMIN can be both MAX_MIN and FP
    if (OpcodeIsFp(opcode) || opcode == Opcode::OP_FMAX || opcode == Opcode::OP_FMIN) {
        switch (instSTndWidth) {
            case DPRECISION_U:
                operand.cvt = OPConvertType::OPCVT_FP64;
                operand.width = OperandWidth::OPDW_D;
                return;
            case WPRECISION_U:
                operand.cvt = OPConvertType::OPCVT_FP32;
                operand.width = OperandWidth::OPDW_W;
                return;
            case HPRECISION_U:
                operand.cvt = OPConvertType::OPCVT_FP16;
                operand.width = OperandWidth::OPDW_H;
                return;
            case BPRECISION_U:
                operand.cvt = OPConvertType::OPCVT_FP8;
                operand.width = OperandWidth::OPDW_B;
                return;
            case HPRECISION_S:
                operand.cvt = OPConvertType::OPCVT_BF16;
                operand.width = OperandWidth::OPDW_H;
                return;
            case BPRECISION_S:
                operand.cvt = OPConvertType::OPCVT_FP8_1;
                operand.width = OperandWidth::OPDW_B;
                return;
            default:
                return;
        }
    } else {    // icvtf指令在输入时和普通64位指令的cvt及width一致
        switch (instSTndWidth) {
            case DPRECISION_U:
                operand.cvt = OPConvertType::OPCVT_U64;
                operand.width = OperandWidth::OPDW_D;
                return;
            case WPRECISION_U:
                operand.cvt = OPConvertType::OPCVT_U32;
                operand.width = OperandWidth::OPDW_W;
                return;
            case HPRECISION_U:
                operand.cvt = OPConvertType::OPCVT_U16;
                operand.width = OperandWidth::OPDW_H;
                return;
            case BPRECISION_U:
                operand.cvt = OPConvertType::OPCVT_U8;
                operand.width = OperandWidth::OPDW_B;
                return;
            case DPRECISION_S:
                operand.cvt = OPConvertType::OPCVT_S64;
                operand.width = OperandWidth::OPDW_D;
                return;
            case WPRECISION_S:
                operand.cvt = OPConvertType::OPCVT_S32;
                operand.width = OperandWidth::OPDW_W;
                return;
            case HPRECISION_S:
                operand.cvt = OPConvertType::OPCVT_S16;
                operand.width = OperandWidth::OPDW_H;
                return;
            case BPRECISION_S:
                operand.cvt = OPConvertType::OPCVT_S8;
                operand.width = OperandWidth::OPDW_B;
                return;
            default:
                return;
        }
    }
}

// 对向量指令目的操作数进行cvt及width设置
void MInst::SetDstCVTndWIDTH(Operand& operand, Opcode opcode, int32_t instSTndWidth)
{
    if ((OpcodeIsFp(opcode) && opcode != Opcode::OP_FCVTI) || opcode == Opcode::OP_ICVTF) { // icvtf指令输出的特殊情况
        switch (instSTndWidth) {
            case DPRECISION_U:
                operand.cvt = OPConvertType::OPCVT_FP64;
                operand.width = OperandWidth::OPDW_D;
                return;
            case WPRECISION_U:
                operand.cvt = OPConvertType::OPCVT_FP32;
                operand.width = OperandWidth::OPDW_W;
                return;
            case HPRECISION_U:
                operand.cvt = OPConvertType::OPCVT_FP16;
                operand.width = OperandWidth::OPDW_H;
                return;
            case BPRECISION_U:
                operand.cvt = OPConvertType::OPCVT_FP8;
                operand.width = OperandWidth::OPDW_B;
                return;
            case HPRECISION_S:
                operand.cvt = OPConvertType::OPCVT_BF16;
                operand.width = OperandWidth::OPDW_H;
                return;
            case BPRECISION_S:
                operand.cvt = OPConvertType::OPCVT_FP8_1;
                operand.width = OperandWidth::OPDW_B;
                return;
            default:
                return;
        }
    } else {
        switch (instSTndWidth) {
            case DPRECISION_U:
                operand.cvt = OPConvertType::OPCVT_U64;
                operand.width = OperandWidth::OPDW_D;
                return;
            case WPRECISION_U:
                operand.cvt = OPConvertType::OPCVT_U32;
                operand.width = OperandWidth::OPDW_W;
                return;
            case HPRECISION_U:
                operand.cvt = OPConvertType::OPCVT_U16;
                operand.width = OperandWidth::OPDW_H;
                return;
            case BPRECISION_U:
                operand.cvt = OPConvertType::OPCVT_U8;
                operand.width = OperandWidth::OPDW_B;
                return;
            case DPRECISION_S:
                operand.cvt = OPConvertType::OPCVT_S64;
                operand.width = OperandWidth::OPDW_D;
                return;
            case WPRECISION_S:
                operand.cvt = OPConvertType::OPCVT_S32;
                operand.width = OperandWidth::OPDW_W;
                return;
            case HPRECISION_S:
                operand.cvt = OPConvertType::OPCVT_S16;
                operand.width = OperandWidth::OPDW_H;
                return;
            case BPRECISION_S:
                operand.cvt = OPConvertType::OPCVT_S8;
                operand.width = OperandWidth::OPDW_B;
                return;
            default:
                return;
        }
    }
}

void MInst::SetTileDstOperand(Operand& operand, int64_t dst)
{
    if (dst == TILE_T_BOUND) {
        operand = Operand(OperandType::OPD_TILE_TLINK, dst);
    } else if (dst == TILE_U_BOUND) {
        operand = Operand(OperandType::OPD_TILE_ULINK, dst);
    } else if (dst == TILE_M_BOUND) {
        operand = Operand(OperandType::OPD_TILE_MLINK, dst);
    } else if (dst == TILE_N_BOUND) {
        operand = Operand(OperandType::OPD_TILE_NLINK, dst);
    } else if (dst == TILE_ACC_BOUND) {
        operand = Operand(OperandType::OPD_TILE_ACC, dst);
    }
    return;
}

// 对达成simt的条件进行分类, 已默认为64bit
// 只有向量版本
static const std::unordered_set<Opcode> SIMT_TOTAL {
    Opcode::OP_RDADD, Opcode::OP_RDAND, Opcode::OP_RDOR, Opcode::OP_RDXOR,
    Opcode::OP_RDFADD, Opcode::OP_RDMAX, Opcode::OP_RDMIN, Opcode::OP_RDFMAX,
    Opcode::OP_RDFMIN,
    Opcode::OP_LB_BRG, Opcode::OP_LH_BRG, Opcode::OP_LW_BRG, Opcode::OP_LD_BRG,
    Opcode::OP_LBU_BRG, Opcode::OP_LHU_BRG, Opcode::OP_LWU_BRG, Opcode::OP_LBI_BRG,
    Opcode::OP_LHI_BRG, Opcode::OP_LWI_BRG, Opcode::OP_LDI_BRG, Opcode::OP_LBUI_BRG,
    Opcode::OP_LHUI_BRG, Opcode::OP_LWUI_BRG, Opcode::OP_LHI_U_BRG, Opcode::OP_LWI_U_BRG,
    Opcode::OP_LDI_U_BRG, Opcode::OP_LHUI_U_BRG, Opcode::OP_LWUI_U_BRG,
    Opcode::OP_SHFL_UP, Opcode::OP_SHFL_DOWN, Opcode::OP_SHFL_BFLY, Opcode::OP_SHFL_IDX,
    Opcode::OP_SHFLI_UP, Opcode::OP_SHFLI_DOWN, Opcode::OP_SHFLI_BFLY, Opcode::OP_SHFLI_IDX,
    Opcode::OP_SB_BRG, Opcode::OP_SH_BRG, Opcode::OP_SW_BRG, Opcode::OP_SD_BRG, Opcode::OP_SH_U_BRG,
    Opcode::OP_SW_U_BRG, Opcode::OP_SD_U, Opcode::OP_SBI_BRG, Opcode::OP_SHI_BRG, Opcode::OP_SWI_BRG,
    Opcode::OP_SDI_BRG, Opcode::OP_SHI_U_BRG, Opcode::OP_SWI_U_BRG, Opcode::OP_SDI_U_BRG,
    Opcode::OP_MOV
};

// simt: 目的寄存器为vt/vu/vm/vn
static const std::unordered_set<Opcode> SIMT_Vreg {
    Opcode::OP_ADD, Opcode::OP_SUB, Opcode::OP_AND, Opcode::OP_OR, Opcode::OP_XOR,
    Opcode::OP_SRL, Opcode::OP_SRA, Opcode::OP_SLL, Opcode::OP_ADDI, Opcode::OP_SUBI,
    Opcode::OP_ANDI, Opcode::OP_ORI, Opcode::OP_XORI, Opcode::OP_SRLI, Opcode::OP_SRAI,
    Opcode::OP_SLLI,
    Opcode::OP_LW_ADD, Opcode::OP_LW_AND, Opcode::OP_LW_OR, Opcode::OP_LW_XOR,
    Opcode::OP_LW_MAX, Opcode::OP_LW_MIN, Opcode::OP_LD_ADD, Opcode::OP_LD_AND,
    Opcode::OP_LD_OR, Opcode::OP_LD_XOR, Opcode::OP_LD_MAX, Opcode::OP_LD_MIN,
    Opcode::OP_MUL, Opcode::OP_MADD, Opcode::OP_DIV, Opcode::OP_REM,
    Opcode::OP_FADD, Opcode::OP_FSUB, Opcode::OP_FMUL, Opcode::OP_FDIV,
    Opcode::OP_FMADD, Opcode::OP_FMSUB, Opcode::OP_FNMADD, Opcode::OP_FNMSUB,
    Opcode::OP_BXS, Opcode::OP_BXU, Opcode::OP_BIC, Opcode::OP_BIS,
    Opcode::OP_CTZ, Opcode::OP_CLZ, Opcode::OP_BCNT, Opcode::OP_REV,
    Opcode::OP_FEQ, Opcode::OP_FNE, Opcode::OP_FLT, Opcode::OP_FGE,
    Opcode::OP_MAX, Opcode::OP_MIN, Opcode::OP_FMAX, Opcode::OP_FMIN,
    Opcode::OP_CSEL, Opcode::OP_PSEL, Opcode::OP_FCVT, Opcode::OP_FCVTI, Opcode::OP_ICVT, Opcode::OP_ICVTF,
    Opcode::OP_LB, Opcode::OP_LH, Opcode::OP_LW, Opcode::OP_LD,
    Opcode::OP_LBU, Opcode::OP_LHU, Opcode::OP_LWU,
    Opcode::OP_LBI, Opcode::OP_LHI, Opcode::OP_LWI, Opcode::OP_LDI,
    Opcode::OP_LBUI, Opcode::OP_LHUI, Opcode::OP_LWUI,
    Opcode::OP_LHI_U, Opcode::OP_LWI_U, Opcode::OP_LDI_U,
    Opcode::OP_LHUI_U, Opcode::OP_LWUI_U,
    Opcode::OP_FABS, Opcode::OP_FSQRT, Opcode::OP_FRECIP, Opcode::OP_FEXP, Opcode::OP_FCLASS,
    Opcode::OP_QPUSH, Opcode::OP_QPOP
};

// simt: 目的寄存器为vt/vu/vm/vn或P寄存器
static const std::unordered_set<Opcode> SIMT_Vreg_Preg {
    Opcode::OP_CMP_EQ, Opcode::OP_CMP_NE, Opcode::OP_CMP_AND, Opcode::OP_CMP_OR,
    Opcode::OP_CMP_LT, Opcode::OP_CMP_GE, Opcode::OP_CMP_LTU, Opcode::OP_CMP_GEU,
    Opcode::OP_CMP_EQI, Opcode::OP_CMP_NEI, Opcode::OP_CMP_ANDI, Opcode::OP_CMP_ORI,
    Opcode::OP_CMP_LTI, Opcode::OP_CMP_GEI, Opcode::OP_CMP_LTUI, Opcode::OP_CMP_GEUI
};

// simt: 任意输入为向量寄存器即为向量(任意输入寄存器为vt/vu/vm/vn或P)
static const std::unordered_set<Opcode> SIMT_Input {
    Opcode::OP_SW_ADD, Opcode::OP_SW_AND, Opcode::OP_SW_OR, Opcode::OP_SW_XOR,
    Opcode::OP_SW_MAX, Opcode::OP_SW_MIN, Opcode::OP_SD_ADD, Opcode::OP_SD_AND,
    Opcode::OP_SD_OR, Opcode::OP_SD_XOR, Opcode::OP_SD_MAX, Opcode::OP_SD_MIN,
    Opcode::OP_SB, Opcode::OP_SH, Opcode::OP_SW, Opcode::OP_SD, Opcode::OP_SH_U,
    Opcode::OP_SW_U, Opcode::OP_SD_U, Opcode::OP_SBI, Opcode::OP_SHI, Opcode::OP_SWI,
    Opcode::OP_SDI, Opcode::OP_SHI_U, Opcode::OP_SWI_U, Opcode::OP_SDI_U
};

void MInst::DetermineIsSimt()
{
    auto it = SIMT_TOTAL.find(opcode);
    if (it != SIMT_TOTAL.end()) {
        this->codeLen = EncodeLen::ENL_V;
    }
    it = SIMT_Vreg.find(opcode);
    if (it != SIMT_Vreg.end()) {
        for (auto i : this->dsts) {
            if (i->type == OperandType::OPD_VTLINK ||
                i->type == OperandType::OPD_VULINK ||
                i->type == OperandType::OPD_VMLINK ||
                i->type == OperandType::OPD_VNLINK) {
                this->codeLen = EncodeLen::ENL_V;
                return;
            }
        }
    }
    it = SIMT_Vreg_Preg.find(opcode);
    if (it != SIMT_Vreg_Preg.end()) {
        for (auto i : this->dsts) {
            if (i->type == OperandType::OPD_VTLINK ||
                i->type == OperandType::OPD_VULINK ||
                i->type == OperandType::OPD_VMLINK ||
                i->type == OperandType::OPD_VNLINK ||
                i->type == OperandType::OPD_PREDMASK) {
                this->codeLen = EncodeLen::ENL_V;
                return;
            }
        }
    }
    it = SIMT_Input.find(opcode);
    if (it != SIMT_Input.end()) {
        for (auto i : this->srcs) {
            if (i->type == OperandType::OPD_VTLINK ||
                i->type == OperandType::OPD_VULINK ||
                i->type == OperandType::OPD_VMLINK ||
                i->type == OperandType::OPD_VNLINK ||
                i->type == OperandType::OPD_PREDMASK ||
                i->type == OperandType::OPD_LC0 ||
                i->type == OperandType::OPD_LC1 ||
                i->type == OperandType::OPD_LC2) {
                this->codeLen = EncodeLen::ENL_V;
                return;
            }
        }
    }
}

void MInst::SetSrcOperand(Operand& operand, int64_t src, int32_t type)
{
    if (type == STORE_TO_REG) {
        if (src == 0) {                                                                 // r0
            operand = Operand(OperandType::OPD_ZERO, 0);
        } else if (src >= GREG_LEFT_BOUND && src <= GREG_RIGHT_BOUND) {                 // r1-r23
            operand = Operand(OperandType::OPD_GREG, static_cast<uint64_t>(src));
        } else if (src >= LINK_LEFT_BOUND && src <= LINK_RIGHT_BOUND) {                 // t1-t4
            operand = Operand(OperandType::OPD_TLINK, static_cast<uint64_t>(src - LINK_LEFT_BOUND));
        } else if (src >= UREG_LEFT_BOUND && src <= UREG_RIGHT_BOUND) {                 // u1-u4
            operand = Operand(OperandType::OPD_ULINK, static_cast<uint64_t>(src - UREG_LEFT_BOUND));
        } else {
            operand = Operand(OperandType::OPD_INVALID, static_cast<uint64_t>(src));
        }
    } else if (type == STORE_TO_SIMM) {
        operand = Operand(OperandType::OPD_SIMM, src);
    } else if (type == STORE_TO_UIMM) {
        operand = Operand(OperandType::OPD_UIMM, src);
    } else if (type == STORE_TO_SYS) {
        operand = Operand(OperandType::OPD_SYS, src);
    } else if (type == STORE_TO_TILEREG) {
        if (src >= TILE_T_LEFT_BOUND && src <= TILE_T_RIGHT_BOUND) {
            operand = Operand(OperandType::OPD_TILE_TLINK, static_cast<uint64_t>(src));
        } else if (src >= TILE_U_LEFT_BOUND && src <= TILE_U_RIGHT_BOUND) {
            operand = Operand(OperandType::OPD_TILE_ULINK, static_cast<uint64_t>(src - TILE_U_LEFT_BOUND));
        } else if (src >= TILE_M_LEFT_BOUND && src <= TILE_M_RIGHT_BOUND) {
            operand = Operand(OperandType::OPD_TILE_MLINK, static_cast<uint64_t>(src - TILE_M_LEFT_BOUND));
        } else if (src >= TILE_N_LEFT_BOUND && src <= TILE_N_RIGHT_BOUND) {
            operand = Operand(OperandType::OPD_TILE_NLINK, static_cast<uint64_t>(src - TILE_N_LEFT_BOUND));
        }
    }
    operand.isDst = false;
}

void MInst::SetSrcOperandSimt(Operand& operand, int64_t srcValue, int32_t srcType)
{
    if (srcType == STORE_TO_REG) {
        if (srcValue >= VLINK_LEFT_BOUND_SIMT && srcValue <= VLINK_RIGHT_BOUND_SIMT) {          // vt1-vt4
            operand = Operand(OperandType::OPD_VTLINK, static_cast<uint64_t>(srcValue));
            operand.reuse = false;
        } else if (srcValue >= VUREG_LEFT_BOUND_SIMT && srcValue <= VUREG_RIGHT_BOUND_SIMT) {   // vu1-vu4
            operand = Operand(OperandType::OPD_VULINK, static_cast<uint64_t>(srcValue - VUREG_LEFT_BOUND_SIMT));
            operand.reuse = false;
        } else if (srcValue >= VMREG_LEFT_BOUND_SIMT && srcValue <= VMREG_RIGHT_BOUND_SIMT) {   // vm1-vm4
            operand = Operand(OperandType::OPD_VMLINK, static_cast<uint64_t>(srcValue - VMREG_LEFT_BOUND_SIMT));
            operand.reuse = false;
        } else if (srcValue >= VNREG_LEFT_BOUND_SIMT && srcValue <= VNREG_RIGHT_BOUND_SIMT) {   // vn1-vn4
            operand = Operand(OperandType::OPD_VNLINK, static_cast<uint64_t>(srcValue - VNREG_LEFT_BOUND_SIMT));
            operand.reuse = false;
        } else if (srcValue >= RI_LEFT_BOUND && srcValue <= RI_RIGHT_BOUND) {                   // ri0-ri11
            operand = Operand(OperandType::OPD_RI, static_cast<uint64_t>(srcValue - RI_LEFT_BOUND));
        } else if (srcValue >= LINK_LEFT_BOUND_SIMT && srcValue <= LINK_RIGHT_BOUND_SIMT) {     // T1-T8
            operand = Operand(OperandType::OPD_TLINK, static_cast<uint64_t>(srcValue - LINK_LEFT_BOUND_SIMT));
        } else if (srcValue >= UREG_LEFT_BOUND_SIMT && srcValue <= UREG_RIGHT_BOUND_SIMT) {     // U1-U8
            operand = Operand(OperandType::OPD_ULINK, static_cast<uint64_t>(srcValue - UREG_LEFT_BOUND_SIMT));
        } else if (srcValue == L_C0_BOUND) {
            operand = Operand(OperandType::OPD_LC0, 0);
        } else if (srcValue == L_B0_BOUND) {
            operand = Operand(OperandType::OPD_LB0, 0);
        } else if (srcValue == L_C1_BOUND) {
            operand = Operand(OperandType::OPD_LC1, 0);
        } else if (srcValue == L_B1_BOUND) {
            operand = Operand(OperandType::OPD_LB1, 0);
        } else if (srcValue == L_C2_BOUND) {
            operand = Operand(OperandType::OPD_LC2, 0);
        } else if (srcValue == L_B2_BOUND) {
            operand = Operand(OperandType::OPD_LB2, 0);
        } else if (srcValue == TILE_TA_BOUND) {
            operand = Operand(OperandType::OPD_TA_REG, 0);
        } else if (srcValue == TILE_TB_BOUND) {
            operand = Operand(OperandType::OPD_TB_REG, 0);
        } else if (srcValue == TILE_TC_BOUND) {
            operand = Operand(OperandType::OPD_TC_REG, 0);
        } else if (srcValue == TILE_TD_BOUND) {
            operand = Operand(OperandType::OPD_TD_REG, 0);
        } else if (srcValue == TILE_TE_BOUND) {
            operand = Operand(OperandType::OPD_TE_REG, 0);
        } else if (srcValue == TILE_TF_BOUND) {
            operand = Operand(OperandType::OPD_TF_REG, 0);
        } else if (srcValue == TILE_TG_BOUND) {
            operand = Operand(OperandType::OPD_TG_REG, 0);
        } else if (srcValue == TILE_TH_BOUND) {
            operand = Operand(OperandType::OPD_TH_REG, 0);
        } else if (srcValue == TILE_TO_BOUND) {
            operand = Operand(OperandType::OPD_TO_REG, 0);
        } else if (srcValue == TILE_TO1_BOUND) {
            operand = Operand(OperandType::OPD_TO1_REG, 0);
        } else if (srcValue == TILE_TO2_BOUND) {
            operand = Operand(OperandType::OPD_TO2_REG, 0);
        } else if (srcValue == TILE_TO3_BOUND) {
            operand = Operand(OperandType::OPD_TO3_REG, 0);
        } else if (srcValue == PREDICATE_MASK_BOUND) {
            operand = Operand(OperandType::OPD_PREDMASK, 0);
        } else if (srcValue == GREG_ZERO) {
            operand = Operand(OperandType::OPD_ZERO, 0);
        } else if (srcValue >= VLINK_LEFT_BOUND_SIMT_REUSE && srcValue <= VLINK_RIGHT_BOUND_SIMT_REUSE) {
            // vt1-vt4 reuse
            operand = Operand(OperandType::OPD_VTLINK, static_cast<uint64_t>(srcValue - VLINK_LEFT_BOUND_SIMT_REUSE));
            operand.reuse = true;
        } else if (srcValue >= VUREG_LEFT_BOUND_SIMT_REUSE && srcValue <= VUREG_RIGHT_BOUND_SIMT_REUSE) {
            // vu1-vu4 reuse
            operand = Operand(OperandType::OPD_VULINK, static_cast<uint64_t>(srcValue - VUREG_LEFT_BOUND_SIMT_REUSE));
            operand.reuse = true;
        } else if (srcValue >= VMREG_LEFT_BOUND_SIMT_REUSE && srcValue <= VMREG_RIGHT_BOUND_SIMT_REUSE) {
            // vm1-vm4 reuse
            operand = Operand(OperandType::OPD_VMLINK, static_cast<uint64_t>(srcValue - VMREG_LEFT_BOUND_SIMT_REUSE));
            operand.reuse = true;
        } else if (srcValue >= VNREG_LEFT_BOUND_SIMT_REUSE && srcValue <= VNREG_RIGHT_BOUND_SIMT_REUSE) {
            // vn1-vn4 reuse
            operand = Operand(OperandType::OPD_VNLINK, static_cast<uint64_t>(srcValue - VNREG_LEFT_BOUND_SIMT_REUSE));
            operand.reuse = true;
        } else {
            return;
        }
    } else if (srcType == STORE_TO_SIMM) {
        operand = Operand(OperandType::OPD_SIMM, static_cast<uint64_t>(srcValue));
    } else if (srcType == STORE_TO_UIMM) {
        operand = Operand(OperandType::OPD_UIMM, static_cast<uint64_t>(srcValue));
    }
    operand.isDst = false;
}

void MInst::SetDstOperand(Operand& operand, int64_t dst, int32_t dstType)
{
    if (dstType == STORE_TO_REG_DST) {
        if (dst >= DST_GREG_LEFT_BOUND && dst <= DST_GREG_RIGHT_BOUND) {                    // r1-r23
            operand = Operand(OperandType::OPD_GREG, static_cast<uint64_t>(dst));
        } else if (dst == DST_UREG_BOUND) {                                                 // U队列, value无意义
            operand = Operand(OperandType::OPD_ULINK, 0);
        } else if (dst == DST_LINK_BOUND) {                                                 // T队列, value无意义
            operand = Operand(OperandType::OPD_TLINK, 0);
        }
    } else if (dstType == STORE_TO_REG_TILE) {
        if (dst == DST_TILE_TLINK) {
            operand = Operand(OperandType::OPD_TILE_TLINK, 0);                              // tile_T队列, value无意义
        } else if (dst == DST_TILE_ULINK) {
            operand = Operand(OperandType::OPD_TILE_ULINK, 0);                              // tile_U队列, value无意义
        } else if (dst == DST_TILE_MLINK) {
            operand = Operand(OperandType::OPD_TILE_MLINK, 0);                              // tile_M队列, value无意义
        } else if (dst == DST_TILE_NLINK) {
            operand = Operand(OperandType::OPD_TILE_NLINK, 0);                              // tile_N队列, value无意义
        } else if (dst == DST_TILE_ACC) {
            operand = Operand(OperandType::OPD_TILE_ACC, 0);                                // tile_ACC队列, value无意义
        } else if (dst == DST_TILE_STACK) {
            operand = Operand(OperandType::OPD_TILE_STACK, 0);                              // tile_S队列, value无意义
        }
    }
    operand.isDst = true;
}

void MInst::SetDstOperandSimt(Operand &operand, int64_t dstValue)
{
    if (dstValue == DST_VLINK_BOUND) {
        operand = Operand(OperandType::OPD_VTLINK, static_cast<uint64_t>(dstValue));
    } else if (dstValue == DST_VUREG_BOUND) {
        operand = Operand(OperandType::OPD_VULINK, static_cast<uint64_t>(dstValue));
    } else if (dstValue == DST_VMREG_BOUND) {
        operand = Operand(OperandType::OPD_VMLINK, static_cast<uint64_t>(dstValue));
    } else if (dstValue == DST_VNREG_BOUND) {
        operand = Operand(OperandType::OPD_VNLINK, static_cast<uint64_t>(dstValue));
    } else if (dstValue >= DST_RO_LEFT_BOUND && dstValue <= DST_RO_RIGHT_BOUND) {
        operand = Operand(OperandType::OPD_RO, static_cast<uint64_t>(dstValue - DST_RO_LEFT_BOUND));
    } else if (dstValue == DST_SIMT_UREG_BOUND) {
        operand = Operand(OperandType::OPD_ULINK, static_cast<uint64_t>(dstValue));
    } else if (dstValue == DST_SIMT_LINK_BOUND) {
        operand = Operand(OperandType::OPD_TLINK, static_cast<uint64_t>(dstValue));
    } else if (dstValue == PREDICATE_MASK_BOUND) {
        operand = Operand(OperandType::OPD_PREDMASK, 0);
    } else {
        return;
    }
    operand.isDst = true;
}

}
