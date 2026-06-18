#include "Atomic.h"

namespace JCore {
namespace Calculate {
namespace Atomic {

static bool CalcLoadAddr(MInst &inst)
{
    inst.accMemInfo->accMemAddr = inst.srcs[SRC0_IDX]->data;
    return true;
}

template <typename Op>
static bool CalcInstAtomicOp32(MInst &inst, Op op)
{
    const uint32_t a = static_cast<uint32_t>(inst.atomicInfo->loadData);
    const uint32_t b = static_cast<uint32_t>(inst.srcs[SRC1_IDX]->data);
    const uint32_t r = op(a, b);
    inst.atomicInfo->writeBackData = static_cast<uint64_t>(static_cast<int64_t>(r));
    return true;
}

template <typename Op>
static bool CalcInstAtomicOp64(MInst &inst, Op op)
{
    const uint64_t a = inst.atomicInfo->loadData;
    const uint64_t b = inst.srcs[SRC1_IDX]->data;
    const uint64_t r = op(a, b);
    inst.atomicInfo->writeBackData = r;
    return true;
}

static bool CalcInstLoadAdd(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }
    return CalcInstAtomicOp64(inst, [](uint64_t a, uint64_t b) -> uint64_t {
        return a + b;
    });
}

static bool CalcInstLoadWAdd(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }
    return CalcInstAtomicOp32(inst, [](uint32_t a, uint32_t b) -> uint32_t {
        return static_cast<uint32_t>(a + b);
    });
}

static bool CalcInstLoadAnd(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }
    return CalcInstAtomicOp64(inst, [](uint64_t a, uint64_t b) -> uint64_t {
        return a & b;
    });
}

static bool CalcInstLoadWAnd(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }
    return CalcInstAtomicOp32(inst, [](uint32_t a, uint32_t b) -> uint32_t {
        return a & b;
    });
}

static bool CalcInstLoadOr(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }
    return CalcInstAtomicOp64(inst, [](uint64_t a, uint64_t b) -> uint64_t {
        return a | b;
    });
}

static bool CalcInstLoadWOr(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }
    return CalcInstAtomicOp32(inst, [](uint32_t a, uint32_t b) -> uint32_t {
        return a | b;
    });
}

static bool CalcInstLoadXor(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }
    return CalcInstAtomicOp64(inst, [](uint64_t a, uint64_t b) -> uint64_t {
        return a ^ b;
    });
}

static bool CalcInstLoadWXor(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }
    return CalcInstAtomicOp32(inst, [](uint32_t a, uint32_t b) -> uint32_t {
        return a ^ b;
    });
}

static bool CalcInstLoadSMax(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }
    return CalcInstAtomicOp64(inst, [](uint64_t a, uint64_t b) -> uint64_t {
        const int64_t sa = static_cast<int64_t>(a);
        const int64_t sb = static_cast<int64_t>(b);
        return static_cast<uint64_t>((sa > sb) ? sa : sb);
    });
}

static bool CalcInstLoadWSMax(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }
    return CalcInstAtomicOp32(inst, [](uint32_t a, uint32_t b) -> uint32_t {
        const int32_t sa = static_cast<int32_t>(a);
        const int32_t sb = static_cast<int32_t>(b);
        return static_cast<uint32_t>((sa > sb) ? sa : sb);
    });
}

static bool CalcInstLoadSMin(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }
    return CalcInstAtomicOp64(inst, [](uint64_t a, uint64_t b) -> uint64_t {
        const int64_t sa = static_cast<int64_t>(a);
        const int64_t sb = static_cast<int64_t>(b);
        return static_cast<uint64_t>((sa < sb) ? sa : sb);
    });
}

static bool CalcInstLoadWSMin(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }
    return CalcInstAtomicOp32(inst, [](uint32_t a, uint32_t b) -> uint32_t {
        const int32_t sa = static_cast<int32_t>(a);
        const int32_t sb = static_cast<int32_t>(b);
        return static_cast<uint32_t>((sa < sb) ? sa : sb);
    });
}

static bool CalcInstLoadUMax(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }
    return CalcInstAtomicOp64(inst, [](uint64_t a, uint64_t b) -> uint64_t {
        return ((a > b) ? a : b);
    });
}

static bool CalcInstLoadWUMax(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }
    return CalcInstAtomicOp32(inst, [](uint32_t a, uint32_t b) -> uint32_t {
        return ((a > b) ? a : b);
    });
}

static bool CalcInstLoadUMin(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }
    return CalcInstAtomicOp64(inst, [](uint64_t a, uint64_t b) -> uint64_t {
        return ((a < b) ? a : b);
    });
}

static bool CalcInstLoadWUMin(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }
    return CalcInstAtomicOp32(inst, [](uint32_t a, uint32_t b) -> uint32_t {
        return ((a < b) ? a : b);
    });
}

static bool CalcInstLoadMax(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }
    switch (inst.srcs[SRC1_IDX]->cvt) {
        case JCore::OPConvertType::OPCVT_S64:
        case JCore::OPConvertType::OPCVT_S32:
            return CalcInstAtomicOp64(inst, [](uint64_t a, uint64_t b) -> uint64_t {
                const int64_t sa = static_cast<int64_t>(a);
                const int64_t sb = static_cast<int64_t>(b);
                return static_cast<uint64_t>((sa > sb) ? sa : sb);
            });
        default:
            return CalcInstAtomicOp64(inst, [](uint64_t a, uint64_t b) -> uint64_t {
                return ((a > b) ? a : b);
            });
    }
}

static bool CalcInstLoadWMax(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }
    switch (inst.srcs[SRC1_IDX]->cvt) {
        case JCore::OPConvertType::OPCVT_S64:
        case JCore::OPConvertType::OPCVT_S32:
            return CalcInstAtomicOp32(inst, [](uint32_t a, uint32_t b) -> uint32_t {
                const int32_t sa = static_cast<int32_t>(a);
                const int32_t sb = static_cast<int32_t>(b);
                return static_cast<uint32_t>((sa > sb) ? sa : sb);
            });
        default:
            return CalcInstAtomicOp32(inst, [](uint32_t a, uint32_t b) -> uint32_t {
                return ((a > b) ? a : b);
            });
    }
}

static bool CalcInstLoadMin(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }
    switch (inst.srcs[SRC1_IDX]->cvt) {
        case JCore::OPConvertType::OPCVT_S64:
        case JCore::OPConvertType::OPCVT_S32:
            return CalcInstAtomicOp64(inst, [](uint64_t a, uint64_t b) -> uint64_t {
                const int64_t sa = static_cast<int64_t>(a);
                const int64_t sb = static_cast<int64_t>(b);
                return static_cast<uint64_t>((sa < sb) ? sa : sb);
            });
        default:
            return CalcInstAtomicOp64(inst, [](uint64_t a, uint64_t b) -> uint64_t {
                return ((a < b) ? a : b);
            });
    }
}

static bool CalcInstLoadWMin(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }
    switch (inst.srcs[SRC1_IDX]->cvt) {
        case JCore::OPConvertType::OPCVT_S64:
        case JCore::OPConvertType::OPCVT_S32:
            return CalcInstAtomicOp32(inst, [](uint32_t a, uint32_t b) -> uint32_t {
                const int32_t sa = static_cast<int32_t>(a);
                const int32_t sb = static_cast<int32_t>(b);
                return static_cast<uint32_t>((sa < sb) ? sa : sb);
            });
        default:
            return CalcInstAtomicOp32(inst, [](uint32_t a, uint32_t b) -> uint32_t {
                return ((a < b) ? a : b);
            });
    }
}

static bool CalcInstSWAP(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        inst.accMemInfo->accMemAddr = inst.srcs[SRC0_IDX]->data;
        return true;
    }
    inst.atomicInfo->writeBackData = inst.srcs[SRC1_IDX]->data;
    return true;
}

/* Compare and Swap */
static bool CalcInstCASB(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }

    uint8_t cmpVal = static_cast<uint8_t>(inst.srcs[SRC1_IDX]->data & MASK_BIT8);
    uint8_t oldVal = static_cast<uint8_t>(inst.atomicInfo->loadData & MASK_BIT8);
    uint8_t newVal = static_cast<uint8_t>(inst.srcs[SRC2_IDX]->data & MASK_BIT8);
    if (cmpVal == oldVal) {
        inst.atomicInfo->writeBackVld = true;
        inst.atomicInfo->writeBackData = static_cast<uint64_t>(newVal);
    } else {
        inst.atomicInfo->writeBackVld = false;
    }
    return true;
}

static bool CalcInstCASH(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }

    uint16_t cmpVal = static_cast<uint16_t>(inst.srcs[SRC1_IDX]->data & MASK_BIT16);
    uint16_t oldVal = static_cast<uint16_t>(inst.atomicInfo->loadData & MASK_BIT16);
    uint16_t newVal = static_cast<uint16_t>(inst.srcs[SRC2_IDX]->data & MASK_BIT16);
    if (cmpVal == oldVal) {
        inst.atomicInfo->writeBackVld = true;
        inst.atomicInfo->writeBackData = static_cast<uint64_t>(newVal);
    } else {
        inst.atomicInfo->writeBackVld = false;
    }
    return true;
}

static bool CalcInstCASW(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }

    uint32_t cmpVal = static_cast<uint32_t>(inst.srcs[SRC1_IDX]->data & MASK_BIT32);
    uint32_t oldVal = static_cast<uint32_t>(inst.atomicInfo->loadData & MASK_BIT32);
    uint32_t newVal = static_cast<uint32_t>(inst.srcs[SRC2_IDX]->data & MASK_BIT32);
    if (cmpVal == oldVal) {
        inst.atomicInfo->writeBackVld = true;
        inst.atomicInfo->writeBackData = static_cast<uint64_t>(newVal);
    } else {
        inst.atomicInfo->writeBackVld = false;
    }
    return true;
}

static bool CalcInstCASD(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }

    uint64_t cmpVal = inst.srcs[SRC1_IDX]->data;
    uint64_t oldVal = inst.atomicInfo->loadData;
    uint64_t newVal = inst.srcs[SRC2_IDX]->data;
    if (cmpVal == oldVal) {
        inst.atomicInfo->writeBackVld = true;
        inst.atomicInfo->writeBackData = newVal;
    } else {
        inst.atomicInfo->writeBackVld = false;
    }
    return true;
}

/* Compare and Swap pair */
static bool CalcInstCASBP(MInst &inst)
{
    if (inst.srcs.size() != SRC5_IDX || inst.dsts.size() != DST2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }

    uint8_t cmpVal0 = static_cast<uint8_t>(inst.srcs[SRC1_IDX]->data & MASK_BIT8);
    uint8_t cmpVal1 = static_cast<uint8_t>(inst.srcs[SRC2_IDX]->data & MASK_BIT8);
    uint8_t oldVal0 = static_cast<uint8_t>(inst.dsts[DST0_IDX]->data & MASK_BIT8);
    uint8_t oldVal1 = static_cast<uint8_t>(inst.dsts[DST1_IDX]->data & MASK_BIT8);
    uint8_t newVal0 = static_cast<uint8_t>(inst.srcs[SRC3_IDX]->data & MASK_BIT8);
    uint8_t newVal1 = static_cast<uint8_t>(inst.srcs[SRC4_IDX]->data & MASK_BIT8);
    if (cmpVal0 == oldVal0 && cmpVal1 == oldVal1) {
        inst.atomicInfo->writeBackVld = true;
        inst.atomicInfo->writeBackData = static_cast<uint64_t>(newVal0);
        inst.atomicInfo->writeBackData1 = static_cast<uint64_t>(newVal1);
    } else {
        inst.atomicInfo->writeBackVld = false;
    }
    return true;
}

static bool CalcInstCASHP(MInst &inst)
{
    if (inst.srcs.size() != SRC5_IDX || inst.dsts.size() != DST2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }

    uint16_t cmpVal0 = static_cast<uint16_t>(inst.srcs[SRC1_IDX]->data & MASK_BIT16);
    uint16_t cmpVal1 = static_cast<uint16_t>(inst.srcs[SRC2_IDX]->data & MASK_BIT16);
    uint16_t oldVal0 = static_cast<uint16_t>(inst.dsts[DST0_IDX]->data & MASK_BIT16);
    uint16_t oldVal1 = static_cast<uint16_t>(inst.dsts[DST1_IDX]->data & MASK_BIT16);
    uint16_t newVal0 = static_cast<uint16_t>(inst.srcs[SRC3_IDX]->data & MASK_BIT16);
    uint16_t newVal1 = static_cast<uint16_t>(inst.srcs[SRC4_IDX]->data & MASK_BIT16);
    if (cmpVal0 == oldVal0 && cmpVal1 == oldVal1) {
        inst.atomicInfo->writeBackVld = true;
        inst.atomicInfo->writeBackData = static_cast<uint64_t>(newVal0);
        inst.atomicInfo->writeBackData1 = static_cast<uint64_t>(newVal1);
    } else {
        inst.atomicInfo->writeBackVld = false;
    }
    return true;
}

static bool CalcInstCASWP(MInst &inst)
{
    if (inst.srcs.size() != SRC5_IDX || inst.dsts.size() != DST2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }

    uint32_t cmpVal0 = static_cast<uint32_t>(inst.srcs[SRC1_IDX]->data & MASK_BIT32);
    uint32_t cmpVal1 = static_cast<uint32_t>(inst.srcs[SRC2_IDX]->data & MASK_BIT32);
    uint32_t oldVal0 = static_cast<uint32_t>(inst.dsts[DST0_IDX]->data & MASK_BIT32);
    uint32_t oldVal1 = static_cast<uint32_t>(inst.dsts[DST1_IDX]->data & MASK_BIT32);
    uint32_t newVal0 = static_cast<uint32_t>(inst.srcs[SRC3_IDX]->data & MASK_BIT32);
    uint32_t newVal1 = static_cast<uint32_t>(inst.srcs[SRC4_IDX]->data & MASK_BIT32);
    if (cmpVal0 == oldVal0 && cmpVal1 == oldVal1) {
        inst.atomicInfo->writeBackVld = true;
        inst.atomicInfo->writeBackData = static_cast<uint64_t>(newVal0);
        inst.atomicInfo->writeBackData1 = static_cast<uint64_t>(newVal1);
    } else {
        inst.atomicInfo->writeBackVld = false;
    }
    return true;
}

static bool CalcInstCASDP(MInst &inst)
{
    if (inst.srcs.size() != SRC5_IDX || inst.dsts.size() != DST2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        return CalcLoadAddr(inst);
    }

    uint64_t cmpVal0 = inst.srcs[SRC1_IDX]->data;
    uint64_t cmpVal1 = inst.srcs[SRC2_IDX]->data;
    uint64_t oldVal0 = inst.dsts[DST0_IDX]->data;
    uint64_t oldVal1 = inst.dsts[DST1_IDX]->data;
    uint64_t newVal0 = inst.srcs[SRC3_IDX]->data;
    uint64_t newVal1 = inst.srcs[SRC4_IDX]->data;
    if (cmpVal0 == oldVal0 && cmpVal1 == oldVal1) {
        inst.atomicInfo->writeBackVld = true;
        inst.atomicInfo->writeBackData = newVal0;
        inst.atomicInfo->writeBackData1 = newVal1;
    } else {
        inst.atomicInfo->writeBackVld = false;
    }
    return true;
}

static bool CalcInstDMA(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX) {
        return false;
    }
    if (!inst.atomicInfo->loadDone) {
        inst.accMemInfo->accMemAddr = inst.srcs[SRC0_IDX]->data;
        return true;
    }
    return true;
}

Handler Lookup(Opcode op)
{
    static const std::unordered_map<Opcode, Handler> ATOMIC_TABLE = std::unordered_map<Opcode, Handler> {
        /* LW 和SW 的区别是，LW 需要将加载到的数据，写到目的寄存器。两者均需要将运算后的结果写回源地址内存 */
        {Opcode::OP_LW_ADD, &CalcInstLoadWAdd},
        {Opcode::OP_LW_AND, &CalcInstLoadWAnd},
        {Opcode::OP_LW_OR, &CalcInstLoadWOr},
        {Opcode::OP_LW_XOR, &CalcInstLoadWXor},
        {Opcode::OP_LW_MAX, &CalcInstLoadWMax},
        {Opcode::OP_LW_MIN, &CalcInstLoadWMin},
        {Opcode::OP_LW_SMAX, &CalcInstLoadWSMax},
        {Opcode::OP_LW_SMIN, &CalcInstLoadWSMin},
        {Opcode::OP_LW_UMAX, &CalcInstLoadWUMax},
        {Opcode::OP_LW_UMIN, &CalcInstLoadWUMin},
        {Opcode::OP_SW_ADD, &CalcInstLoadWAdd},
        {Opcode::OP_SW_AND, &CalcInstLoadWAnd},
        {Opcode::OP_SW_OR, &CalcInstLoadWOr},
        {Opcode::OP_SW_XOR, &CalcInstLoadWXor},
        {Opcode::OP_SW_MAX, &CalcInstLoadWMax}, // Unimplement
        {Opcode::OP_SW_MIN, &CalcInstLoadWMin}, // Unimplement
        {Opcode::OP_SW_SMAX, &CalcInstLoadWSMax},
        {Opcode::OP_SW_SMIN, &CalcInstLoadWSMin},
        {Opcode::OP_SW_UMAX, &CalcInstLoadWUMax},
        {Opcode::OP_SW_UMIN, &CalcInstLoadWUMin},
        {Opcode::OP_LD_ADD, &CalcInstLoadAdd},
        {Opcode::OP_LD_AND, &CalcInstLoadAnd},
        {Opcode::OP_LD_OR, &CalcInstLoadOr},
        {Opcode::OP_LD_XOR, &CalcInstLoadXor},
        {Opcode::OP_LD_MAX, &CalcInstLoadMax},
        {Opcode::OP_LD_MIN, &CalcInstLoadMin},
        {Opcode::OP_LD_SMAX, &CalcInstLoadSMax},
        {Opcode::OP_LD_SMIN, &CalcInstLoadSMin},
        {Opcode::OP_LD_UMAX, &CalcInstLoadUMax},
        {Opcode::OP_LD_UMIN, &CalcInstLoadUMin},
        {Opcode::OP_SD_ADD, &CalcInstLoadAdd},
        {Opcode::OP_SD_AND, &CalcInstLoadAnd},
        {Opcode::OP_SD_OR, &CalcInstLoadOr},
        {Opcode::OP_SD_XOR, &CalcInstLoadXor},
        {Opcode::OP_SD_MAX, &CalcInstLoadMax},
        {Opcode::OP_SD_MIN, &CalcInstLoadMin},
        {Opcode::OP_SD_SMAX, &CalcInstLoadSMax},
        {Opcode::OP_SD_SMIN, &CalcInstLoadSMin},
        {Opcode::OP_SD_UMAX, &CalcInstLoadUMax},
        {Opcode::OP_SD_UMIN, &CalcInstLoadUMin},
        {Opcode::OP_SWAPB, &CalcInstSWAP},
        {Opcode::OP_SWAPH, &CalcInstSWAP},
        {Opcode::OP_SWAPW, &CalcInstSWAP},
        {Opcode::OP_SWAPD, &CalcInstSWAP},
        {Opcode::OP_DMA, &CalcInstDMA},
        {Opcode::OP_CASB, &CalcInstCASB},
        {Opcode::OP_CASH, &CalcInstCASH},
        {Opcode::OP_CASW, &CalcInstCASW},
        {Opcode::OP_CASD, &CalcInstCASD},
        {Opcode::OP_CASBP, &CalcInstCASBP},
        {Opcode::OP_CASHP, &CalcInstCASHP},
        {Opcode::OP_CASWP, &CalcInstCASWP},
        {Opcode::OP_CASDP, &CalcInstCASDP},
    };
    auto it = ATOMIC_TABLE.find(op);
    return (it == ATOMIC_TABLE.end()) ? 0 : it->second;
}

bool CalculateAtomic(MInst &inst)
{
    Handler fn = Lookup(inst.opcode);
    return fn ? fn(inst) : false;
}

} // namespace Atomic
} // namespace Calculate
} // namespace JCore