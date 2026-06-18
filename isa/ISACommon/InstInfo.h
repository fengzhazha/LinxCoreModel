#pragma once
#ifndef DECODE_INST_INFO_H
#define DECODE_INST_INFO_H

#include <iostream>

#include "OpcodeManager.h"
#include "DecodeUtiles.h"

namespace JCore {
const int64_t  INVALID_VALUE64 = 0x0fffffffffffffff;
class InstInfo {
public:
    Opcode              opcode = Opcode::OP_INVALID;

    // 16、32、64位指令共用成员
    int32_t             src0Type = INVALID32; // 0为reg 1为SIMM 2为UIMM 3为SSR(包括LB、LC) 4为tileReg
    int32_t             src1Type = INVALID32; // 0为reg 1为SIMM 2为UIMM 3为SSR 4为tileReg
    int32_t             src2Type = INVALID32; // 0为reg 1为SIMM 2为UIMM 3为SSR 4为tileReg
    int32_t             src3Type = INVALID32; // 0为reg 1为SIMM 2为UIMM 3为SSR 4为tileReg
    int32_t             src4Type = INVALID32; // 0为reg 1为SIMM 2为UIMM 3为SSR 4为tileReg
    int32_t             src5Type = INVALID32;
    int32_t             src6Type = INVALID32;
    int32_t             src7Type = INVALID32;
    int32_t             src8Type = INVALID32;
    int32_t             src9Type = INVALID32;
    int32_t             dst0Type = INVALID32;
    int32_t             dst1Type = INVALID32; // 0为reg 1为tileReg
    int64_t             src0 = INVALID_VALUE64;
    int64_t             src1 = INVALID_VALUE64;
    int64_t             src2 = INVALID_VALUE64;
    int64_t             src3 = INVALID_VALUE64;
    int64_t             src4 = INVALID_VALUE64;
    // 额外多出的src是为承载aq、rl、far等值
    int64_t             src5 = INVALID_VALUE64;
    int64_t             src6 = INVALID_VALUE64;
    int64_t             src7 = INVALID_VALUE64;
    int64_t             src8 = INVALID_VALUE64;
    int64_t             src9 = INVALID_VALUE64;
    int64_t             dst0 = INVALID_VALUE64;
    int64_t             dst1 = INVALID_VALUE64;
    int32_t             src0Shamt = 0;
    int32_t             src1Shamt = 0;
    int32_t             src2Shamt = 0;
    // 用于32位typeB类型指令及64位原子微指令
    int32_t             aq = INVALID32;
    int32_t             rl = INVALID32;
    int32_t             far = INVALID32;
    int32_t             rd = INVALID32;
    // 用于cvt指令
    int32_t             srcType = INVALID32;
    int32_t             dstType = INVALID32;
    int32_t             srcRType = INVALID32;
    int32_t             srcRIndex = 1;

    // 用于标识64位指令s/t及width信息
    // srcX所对应的srcXType非0时,不需要设置, 已经表明寄存器类型
    int32_t             src0InstWidth = INVALID32;
    int32_t             src1InstWidth = INVALID32;
    int32_t             src2InstWidth = INVALID32;
    int32_t             src3InstWidth = INVALID32;
    int32_t             src4InstWidth = INVALID32;
    int32_t             dst0InstWidth = INVALID32;
    int32_t             dst1InstWidth = INVALID32;
    // 用于标识64位向量指令的输入数据是否为打包类型
    int32_t             pack = INVALID32;
    int32_t             sat = 0;
    // 用于64位Load/Store中lane是否连续及是否写入tile
    int32_t             continuous = INVALID32;
    int32_t             local = INVALID32;
    // LC0 的偏移值，仅在
    int32_t             lc0Shamt = 0;

    // 以下用于块指令解码
    int32_t             isBinst = 0;    // 0为非块指令, 2为16位块指令, 4为32位块指令, 6为48位块指令, 8为64位块指令

    int32_t             s0R = INVALID32;            // b.iot/b.ioti中标记是否reuse
    int32_t             s1R = INVALID32;            // b.iot/b.ioti中标记是否reuse
    int32_t             last = INVALID32;           // b.iot/b.ioti中标记是否只有一条b.iot/b.ioti或为最后一条b.iot/b.ioti
    int32_t             tileFunc = INVALID32;        // b.iot中具体的Tile输入数量

    int32_t             reserve = INVALID32;        // 块指令中的保留部分
};

InstInfo* DecodeInst16(uint16_t bin);
InstInfo* DecodeInst32(uint32_t bin);
InstInfo* DecodeInst48(uint64_t bin);
InstInfo* DecodeInst64(uint64_t bin);

uint16_t EncodeInst16(InstInfo *inst);
uint32_t EncodeInst32(InstInfo *inst);
uint64_t EncodeInst64(InstInfo *inst);
}
#endif
