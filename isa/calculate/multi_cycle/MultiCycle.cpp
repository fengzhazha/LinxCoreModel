#include "MultiCycle.h"

namespace JCore {
namespace Calculate {
namespace MultiCycle {

static bool CalcInstMul(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    // 按有符号解释输入的位模式，但用 uint64_t 做模 2^64 乘法，结果即低 64 位（无 UB）
    uint64_t a = static_cast<uint64_t>(static_cast<int64_t>(inst.srcs[SRC0_IDX]->data));
    uint64_t b = static_cast<uint64_t>(static_cast<int64_t>(inst.srcs[SRC1_IDX]->data));
    inst.dsts[DST0_IDX]->data = a * b;
    return true;
}

static bool CalcInstMulU(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    inst.dsts[DST0_IDX]->data = inst.srcs[SRC0_IDX]->data * inst.srcs[SRC1_IDX]->data;
    return true;
}

static bool CalcInstMulW(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    // 取低 32 位并按有符号解释
    int32_t a = static_cast<int32_t>(static_cast<uint32_t>(inst.srcs[SRC0_IDX]->data));
    int32_t b = static_cast<int32_t>(static_cast<uint32_t>(inst.srcs[SRC1_IDX]->data));
    // 只需低 32 位：用 uint32_t 模乘避免任何有符号溢出 UB
    uint32_t lo32 = static_cast<uint32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
    // 低32位结果符号扩展到 64 位写回
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(lo32)));
    return true;
}

static bool CalcInstMulUW(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    uint32_t srcL = static_cast<uint32_t>(inst.srcs[SRC0_IDX]->data);
    uint32_t srcR = static_cast<uint32_t>(inst.srcs[SRC1_IDX]->data);
    uint64_t res = srcL * srcR;
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(static_cast<uint32_t>(res));
    return true;
}

static bool CalcInstMAdd(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }

    uint64_t a = static_cast<uint64_t>(static_cast<int64_t>(inst.srcs[SRC0_IDX]->data));
    uint64_t b = static_cast<uint64_t>(static_cast<int64_t>(inst.srcs[SRC1_IDX]->data));
    uint64_t c = static_cast<uint64_t>(static_cast<int64_t>(inst.srcs[SRC2_IDX]->data));

    inst.dsts[DST0_IDX]->data = a * b + c; // uint64_t 回绕 => 低64位
    return true;
}

static bool CalcInstMAddW(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }

    int32_t a = static_cast<int32_t>(static_cast<uint32_t>(inst.srcs[SRC0_IDX]->data));
    int32_t b = static_cast<int32_t>(static_cast<uint32_t>(inst.srcs[SRC1_IDX]->data));
    int32_t c = static_cast<int32_t>(static_cast<uint32_t>(inst.srcs[SRC2_IDX]->data));

    // 只关心低32位：用 uint32_t 模 2^32 运算避免任何有符号溢出 UB
    uint32_t lo = static_cast<uint32_t>(
        static_cast<uint32_t>(a) * static_cast<uint32_t>(b) + static_cast<uint32_t>(c)
    );

    // 低32位按有符号解释并符号扩展到64
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(lo)));
    return true;
}

static bool CalcInstMIAdd(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }

    uint64_t a = static_cast<uint64_t>(static_cast<int64_t>(inst.srcs[SRC0_IDX]->data));
    uint64_t b = static_cast<uint64_t>(static_cast<int64_t>(inst.srcs[SRC1_IDX]->data));
    uint64_t c = inst.srcs[SRC2_IDX]->data;

    inst.dsts[DST0_IDX]->data = a * b + c; // uint64_t 回绕 => 低64位
    return true;
}

static bool CalcInstMISub(MInst &inst)
{
    if (inst.srcs.size() != SRC3_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }

    uint64_t a = static_cast<uint64_t>(static_cast<int64_t>(inst.srcs[SRC0_IDX]->data));
    uint64_t b = static_cast<uint64_t>(static_cast<int64_t>(inst.srcs[SRC1_IDX]->data));
    uint64_t c = inst.srcs[SRC2_IDX]->data;

    inst.dsts[DST0_IDX]->data = a * b - c; // uint64_t 回绕 => 低64位
    return true;
}

static bool CalcInstDiv(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    int64_t a = static_cast<int64_t>(inst.srcs[SRC0_IDX]->data);
    int64_t b = static_cast<int64_t>(inst.srcs[SRC1_IDX]->data);
    if (b == 0) {
        inst.dsts[DST0_IDX]->data = UINT64_MAX; // -1
        return true;
    }
    if (a == INT64_MIN && b == -1) {
        inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(INT64_MIN);
        return true;
    }
    int64_t q = a / b; // C++ 向 0 舍入
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(q);
    return true;
}

static bool CalcInstDivU(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    uint64_t a = inst.srcs[SRC0_IDX]->data;
    uint64_t b = inst.srcs[SRC1_IDX]->data;

    if (b == 0) {
        inst.dsts[DST0_IDX]->data = UINT64_MAX;
        return true;
    }

    inst.dsts[DST0_IDX]->data = a / b;
    return true;
}

static bool CalcInstDivW(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    int32_t a = static_cast<int32_t>(static_cast<uint32_t>(inst.srcs[SRC0_IDX]->data));
    int32_t b = static_cast<int32_t>(static_cast<uint32_t>(inst.srcs[SRC1_IDX]->data));

    if (b == 0) {
        inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(static_cast<int64_t>(-1)); // 0xFFFF..FFFF
        return true;
    }
    if (a == INT32_MIN && b == -1) {
        inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(static_cast<int64_t>(INT32_MIN)); // 符号扩展
        return true;
    }

    int32_t q = a / b; // 向0舍入
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(static_cast<int64_t>(q)); // 符号扩展到64
    return true;
}

static bool CalcInstDivUW(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    uint32_t a = static_cast<uint32_t>(inst.srcs[SRC0_IDX]->data);
    uint32_t b = static_cast<uint32_t>(inst.srcs[SRC1_IDX]->data);

    if (b == 0) {
        inst.dsts[DST0_IDX]->data = MASK_BIT32;
        return true;
    }

    uint32_t q = static_cast<uint32_t>(a / b);
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(q); // 零扩展
    return true;
}

static bool CalcInstRem(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    int64_t a = static_cast<int64_t>(inst.srcs[SRC0_IDX]->data);
    int64_t b = static_cast<int64_t>(inst.srcs[SRC1_IDX]->data);
    if (b == 0) {
        inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(a);
        return true;
    }
    if (a == INT64_MIN && b == -1) {
        inst.dsts[DST0_IDX]->data = 0; // 余数为0
        return true;
    }
    int64_t r = a % b; // 余数符号与被除数一致，配合向0舍入
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(r);
    return true;
}

static bool CalcInstRemU(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    uint64_t a = inst.srcs[SRC0_IDX]->data;
    uint64_t b = inst.srcs[SRC1_IDX]->data;

    if (b == 0) {
        inst.dsts[DST0_IDX]->data = a;
        return true;
    }

    inst.dsts[DST0_IDX]->data = a % b;
    return true;
}

static bool CalcInstRemW(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    int32_t a = static_cast<int32_t>(static_cast<uint32_t>(inst.srcs[SRC0_IDX]->data));
    int32_t b = static_cast<int32_t>(static_cast<uint32_t>(inst.srcs[SRC1_IDX]->data));

    if (b == 0) {
        inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(static_cast<int64_t>(a)); // 取low32后符号扩展
        return true;
    }
    if (a == INT32_MIN && b == -1) {
        inst.dsts[DST0_IDX]->data = 0; // 余数0
        return true;
    }

    int32_t r = a % b;
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(static_cast<int64_t>(r)); // 符号扩展到64
    return true;
}

static bool CalcInstRemUW(MInst &inst)
{
    if (inst.srcs.size() != SRC2_IDX || inst.dsts.size() != DST1_IDX) {
        return false;
    }
    uint32_t a = static_cast<uint32_t>(inst.srcs[SRC0_IDX]->data);
    uint32_t b = static_cast<uint32_t>(inst.srcs[SRC1_IDX]->data);

    if (b == 0) {
        inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(a); // 零扩展
        return true;
    }

    uint32_t r = static_cast<uint32_t>(a % b);
    inst.dsts[DST0_IDX]->data = static_cast<uint64_t>(r); // 零扩展
    return true;
}


Handler Lookup(Opcode op)
{
    static const std::unordered_map<Opcode, Handler> MULTI_CYCLE_TABLE = std::unordered_map<Opcode, Handler> {
        {Opcode::OP_MUL, &CalcInstMul},
        {Opcode::OP_MULU, &CalcInstMulU},
        {Opcode::OP_MULW, &CalcInstMulW},
        {Opcode::OP_MULUW, &CalcInstMulUW},
        {Opcode::OP_MADD, &CalcInstMAdd},
        {Opcode::OP_MADDW, &CalcInstMAddW},
        {Opcode::OP_MIADD, &CalcInstMIAdd},
        {Opcode::OP_MISUB, &CalcInstMISub},
        {Opcode::OP_DIV, &CalcInstDiv},
        {Opcode::OP_DIVU, &CalcInstDivU},
        {Opcode::OP_DIVW, &CalcInstDivW},
        {Opcode::OP_DIVUW, &CalcInstDivUW},
        {Opcode::OP_REM, &CalcInstRem},
        {Opcode::OP_REMU, &CalcInstRemU},
        {Opcode::OP_REMW, &CalcInstRemW},
        {Opcode::OP_REMUW, &CalcInstRemUW},
    };
    auto it = MULTI_CYCLE_TABLE.find(op);
    return (it == MULTI_CYCLE_TABLE.end()) ? 0 : it->second;
}

bool CalculateMultiCycle(MInst &inst)
{
    Handler fn = Lookup(inst.opcode);
    return fn ? fn(inst) : false;
}

} // namespace MultiCycle
} // namespace Calculate
} // namespace JCore