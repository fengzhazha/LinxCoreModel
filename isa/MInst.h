#pragma once

#include "ISACommon/BranchType.h"
#ifndef MINST_STRUCT
#define MINST_STRUCT

#include "../common/Common.h"
#include "../common/DataStruct/VecData.h"
#include "../softfloat/include/softfloat-types.h"
#include "ISACommon/DecodeUtiles.h"
#include "ISACommon/InstInfo.h"
#include "ISACommon/OpcodeManager.h"
#include "ISACommon/TileOpManager.h"
#include "ISACommon/SystemReg.h"
#include "ISACommon/GPR.h"
#include "ISACommon/OperandType.h"
#include "ISACommon/EncodeLen.h"
#include "ISACommon/LayOut.h"
#include "ISACommon/FRM.h"
#include "ISACommon/BlockVerifyInfo.h"
#include "ISACommon/BlockHint.h"
#include "ISACommon/TileInfo.h"
#include "ISACommon/DataNum.h"

namespace JCore {

enum class ACRC_REQUEST_TYPE {
    SCT_MAC = 0,
    SCT_SYS,
    SCT_SEC,
    SCT_INVAL, // inval parameter
};

constexpr size_t SRC0_IDX = 0;
constexpr size_t SRC1_IDX = 1;
constexpr size_t SRC2_IDX = 2;
constexpr size_t SRC3_IDX = 3;
constexpr size_t SRC4_IDX = 4;
constexpr size_t SRC5_IDX = 5;
constexpr size_t SRC6_IDX = 6;
constexpr size_t SRC7_IDX = 7;
constexpr size_t SRC8_IDX = 8;
constexpr size_t SRC9_IDX = 9;
constexpr size_t DST0_IDX = 0;
constexpr size_t DST1_IDX = 1;
constexpr size_t DST2_IDX = 2;

static const uint64_t MASK_BIT6 = 0x3f;
static const uint64_t MASK_BIT5 = 0x1f;
static const uint64_t MASK_BIT8 = 0xff;
static const uint64_t MASK_BIT12 = 0xfff;
static const uint64_t MASK_BIT16 = 0xffff;
static const uint64_t MASK_BIT17 = 0x1ffff;
static const uint64_t MASK_BIT32 = 0xffffffff;

constexpr uint64_t NUM2 = 2;
constexpr uint64_t NUM3 = 3;
constexpr uint64_t NUM4 = 4;
constexpr uint64_t NUM5 = 5;
constexpr uint64_t NUM8 = 8;
constexpr uint64_t NUM16 = 16;
constexpr uint64_t NUM32 = 32;
constexpr uint64_t NUM64 = 64;
constexpr uint64_t NUM256 = 256;
constexpr uint64_t NUM1024 = 1024;
constexpr uint64_t NUM2048 = 2048;
constexpr uint64_t NUM65580 = 65580;

struct AtomicInfo {
    // Decoding information
    bool aq = false;
    bool rl = false;
    bool far = false;

    // Execution information
    bool loadDone = false;
    uint64_t loadData = 0; // 原子指令需要先load 源地址的数据
    uint64_t loadData1 = 0; // 原子指令需要先load 源地址的数据
    bool writeBackVld = true;
    uint64_t writeBackData = 0; // 原子指令在执行完运算后，需要将运算结果写回源地址
    uint64_t writeBackData1 = 0; // 原子指令在执行完运算后，需要将运算结果写回源地址
    AtomicInfo() = default;
    AtomicInfo(bool a, bool r, bool f) : aq(a), rl(r), far(f) {}
    std::string Dump();
};
using AtomicInfoPtr = std::shared_ptr<AtomicInfo>;

struct AaccelssMemInfo {
    // Decoding information
    bool continuous = false; // only for vector load inst
    bool local = false; // only for vector load inst

    // Execution information
    uint64_t accMemAddr = 0;
    bool vecDataVld = false;
    VecData vecData;
    AaccelssMemInfo() = default;
    AaccelssMemInfo(bool cont, bool loc) : continuous(cont), local(loc), accMemAddr(0U) {}
    AaccelssMemInfo(int32_t cont, int32_t loc) : accMemAddr(0U)
    {
        continuous = (cont == 1);
        local = (loc == 1);
    }
    void SetVecDataVld(bool valid)
    {
        vecDataVld = valid;
    }
    std::string Assemble()
    {
        return local ? ".local" : "";
    }
};
using AaccelssMemPtr = std::shared_ptr<AaccelssMemInfo>;

struct SetcInfo {
    // Execution information
    bool                                    setcTaken = false;
    uint64_t                                setcTarget = 0;
    SetcInfo() = default;
};
using SetcInfoPtr = std::shared_ptr<SetcInfo>;

struct SSRSetInfo {
    // Execution information
    bool vld = false;
    uint64_t ssrId = static_cast<uint64_t>(SystemReg::SYS_INVALID);
    uint64_t data = 0;
    SSRSetInfo() = default;
};
using SSRSetPtr = std::shared_ptr<SSRSetInfo>;

struct ReduceInfo {
    // Execution information
    bool first = true;
    uint64_t data = 0;
    ReduceInfo() = default;
};
using ReduceInfoPtr = std::shared_ptr<ReduceInfo>;

struct BranchInfo {
    // Execution information
    bool brTaken = false;
    uint64_t targetPC = 0;
    std::vector<bool> predMask;
    BranchInfo() = default;
};
using BranchInfoPtr = std::shared_ptr<BranchInfo>;
using TileInfoPtr = std::shared_ptr<TileInfo>;

struct FloatInfo {
    FRMMode rm = FRMMode::FRM_NONE;
    Saturation sat = Saturation::NONE;
    FCVTMode mode = FCVTMode::FCVT_NORMAL;
};
using FloatInfoPtr = std::shared_ptr<FloatInfo>;

struct Operand {
    bool        isDst = false;
    /* \brief Type of operand */
    OperandType type = OperandType::OPD_INVALID;
    /* \brief Type of convert */
    OPConvertType cvt = OPConvertType::OPCVT_DW;
    /* \brief Type of convert */
    OperandWidth width = OperandWidth::OPDW_D;
    /* \brief The value of shifting */
    uint32_t    shamt = 0;
    /* \brief Register index, immediate data */
    uint64_t    value = static_cast<uint64_t>(GPR::GPR_A0);
    /* \brief data in Register */
    uint64_t    data = 0;
    /* \brief mark for prefetch */
    uint32_t    model = 0;
    /* \brief tile base addr */
    uint64_t    baseAddr = 0;
    /* \brief tile size */
    uint64_t    size = 0;

    bool        reuse = false;

    TileInfoPtr tileInfo = nullptr;

    Operand()
        :type(OperandType::OPD_INVALID), cvt(OPConvertType::OPCVT_DW), width(OperandWidth::OPDW_D), shamt(0),
        value(0), data(0), model(0) {}

    Operand(OperandType typ, uint64_t val, uint64_t data)
        :type(typ), cvt(OPConvertType::OPCVT_NO_CVT), width(OperandWidth::OPDW_D), shamt(0),
        value(val), data(data), model(0) {}

    Operand(OperandType typ, uint64_t val)
        :type(typ), cvt(OPConvertType::OPCVT_NO_CVT), width(OperandWidth::OPDW_D), shamt(0),
        value(val), data(0), model(0) {}

    Operand(OperandType typ, uint64_t val, OPConvertType convert)
        :type(typ), cvt(convert), width(OperandWidth::OPDW_D), shamt(0),
        value(val), data(0), model(0) {}

    Operand(OperandType typ, uint64_t val, OPConvertType convert, OperandWidth width)
        :type(typ), cvt(convert), width(width), shamt(0),
        value(val), data(0), model(0) {}
    std::string Assemble(bool detail = true);
    std::string Dump(bool detail = true);
};
using OperandPtr = std::shared_ptr<Operand>;

static inline bool IsBSTOP(uint64_t bin, uint32_t size)
{
    if (size == WIDTH_16) {
        return (bin == 0);
    } else if (size == WIDTH_32) {
        return (bin == 0x1);
    } else if (size == WIDTH_48) {
        return false;
    } else if (size == WIDTH_64) {
        return (bin == 0x10000000F);
    }
    return false;
}

class MInst {
public:
    uint64_t                                binary = 0;
    uint64_t                                pc = 0;
    Opcode                                  opcode = Opcode::OP_INVALID;
    EncodeLen                               codeLen = EncodeLen::ENL_W;
    DataNum                                 dataNum = DataNum::RESERVE;

    std::vector<std::shared_ptr<Operand>>   srcs;
    std::vector<std::shared_ptr<Operand>>   dsts;

    bool                                    ctGen = false;
    bool                                    check = true;          // minst是否需要与gfsim校验的标记
    bool                                    iotLast = false;       // b.iot/b.ioti执行时用于标识是否为最后一条b.iot/b.ioti指令
    bool                                    saturation = false;
    /* 指示一个寄存器中放了几个元素 vlen: 1, 2, 4 */
    int32_t                                 vlen = 1;
    int32_t                                 sat = 0;
    std::shared_ptr<BlockHintInfo>          blockHint = nullptr;
    AaccelssMemPtr                            accMemInfo = nullptr;
    AtomicInfoPtr                           atomicInfo = nullptr;
    SetcInfoPtr                             setcInfo = nullptr;
    SSRSetPtr                               ssrInfo = nullptr;
    ReduceInfoPtr                           reduceInfo = nullptr;
    BranchInfoPtr                           brInfo = nullptr;
    FloatInfoPtr                            floatInfo = nullptr;
    FRMMode                                 frm = FRMMode::FRM_RNE;
    uint32_t                                laneID = 0;

    void DecodeBin(uint64_t bin, uint32_t size);
    void ConvertImm2Data();

    void InitFRM(FRMMode sysFRM);
    std::string Dump();
    std::string Assemble();

    MInst() = default;
    MInst(const MInst& other);
    MInst& operator=(const MInst& other);

    uint64_t GetResult();
    uint64_t GetFallPC();

    std::string GetAssembleStr();
    void SetAssembleStr(std::string str);

    void InitInstGroupInfo(InstInfo* instInfo);
private:
    void InstInfo2MInst(InstInfo* instInfo, uint64_t bin, EncodeLen encodeLen);
    bool IsNeedMergeShamtToSrcR();
    void MergeSrcR();
    void SetOperand(InstInfo* instInfo);
    void SetSimtOperand(InstInfo* instInfo);
    void SetSrcCVTndWIDTH(Operand& operand, Opcode opcode, int32_t instSTndWidth);
    void SetDstCVTndWIDTH(Operand& operand, Opcode opcode, int32_t instSTndWidth);
    void SetTileDstOperand(Operand& operand, int64_t dst);
    void SetSrcOperand(Operand& operand, int64_t src, int32_t type);
    void SetSrcOperandSimt(Operand& operand, int64_t srcValue, int32_t srcType);
    void SetDstOperand(Operand& operand, int64_t dst, int32_t dstType);
    void SetDstOperandSimt(Operand &operand, int64_t dstValue);
    void DetermineIsSimt();
protected:
    std::string assembleStr = ""; // 用于BlockSplit 类指令的打印。
};

inline void OperandCvtVal(OperandPtr src)
{
    switch (src->cvt) {
        // AU and LU type
        case OPConvertType::OPCVT_DW:
            break;
        case OPConvertType::OPCVT_U32:
            src->data = static_cast<uint64_t>(static_cast<uint32_t>(src->data));
            break;
        case OPConvertType::OPCVT_U16:
            src->data = static_cast<uint64_t>(static_cast<uint16_t>(src->data));
            break;
        case OPConvertType::OPCVT_U8:
            src->data = static_cast<uint64_t>(static_cast<uint8_t>(src->data));
            break;
        case OPConvertType::OPCVT_S32:
            src->data = static_cast<int64_t>(static_cast<int32_t>(src->data));
            break;
        case OPConvertType::OPCVT_S16:
            src->data = static_cast<int64_t>(static_cast<int16_t>(src->data));
            break;
        case OPConvertType::OPCVT_S8:
            src->data = static_cast<int64_t>(static_cast<int8_t>(src->data));
            break;
        case OPConvertType::OPCVT_NOT:
            src->data = ~src->data;
            break;
        case OPConvertType::OPCVT_NEG:
            src->data = (~src->data) + 1;
            break;

        // FCVT type
        case OPConvertType::OPCVT_FP8:
        case OPConvertType::OPCVT_FP16:
        case OPConvertType::OPCVT_FP32:
        case OPConvertType::OPCVT_FP64:
        case OPConvertType::OPCVT_FP8_1:
        case OPConvertType::OPCVT_BF16:
        case OPConvertType::OPCVT_S64:
        case OPConvertType::OPCVT_U64:
        case OPConvertType::OPCVT_NO_CVT:
            break;
        default:
            break;
    }
    src->data = src->data << src->shamt;
}
using MInstPtr = std::shared_ptr<MInst>;
}
#endif
