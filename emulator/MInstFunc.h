#pragma once

#ifndef MINST_FUNC_H
#define MINST_FUNC_H

#include "ISA.h"
#include "ArchStatus.h"

namespace JCore {
constexpr int WIDTH6 = 6;
constexpr int WIDTH8 = 8;
constexpr int WIDTH16 = 16;
constexpr int MAX_SRC_NUM = 3;
constexpr int WIDTH_INSTNUM = 7;
constexpr int WIDTH_MINST = 5;
constexpr int WIDTH_PC = 6;
constexpr int WIDTH_OPCODE = 18;
constexpr int WIDTH_INSTNAME = 14;
constexpr int WIDTH_LANE = 3;
constexpr int WIDTH_REGDATA = 27;
class MInstFunc : public JCore::MInst {
public:
    uint64_t threadId = 0;
    uint64_t execSeq = 0; // Total MInst sequence
    uint64_t localExecSeq = 0; // MInst sequence in this block

    /* 下一个块的BSTART 不参与当前块执行，因此发生跳转时转为BSTOP, 反之跳过本指令 */
    bool skipCurrentMinst = false;

    explicit MInstFunc() = default;
    explicit MInstFunc(JCore::MInst &inst) : JCore::MInst(inst) {}

    ~MInstFunc() = default;

    std::string DumpOperands(std::vector<std::shared_ptr<Operand>> &operands, bool isSource)
    {
        std::stringstream oss;
        if (!isSource && operands.empty()) {
            oss << std::left << std::setw(WIDTH_REGDATA + 1) << "|None";
            return oss.str();
        }
        size_t size = operands.size();
        if (size > SRC4_IDX) {
            size = 0;
        }
        for (size_t i = 0; i < size; i++) {
            std::stringstream opdStr;
            opdStr << operands[i]->Dump(false);
            if (OperandTypeIsTile(operands[i]->type)) {
                opdStr << " [0x" << std::hex << operands[i]->baseAddr << "]";
            }
            oss << "|" << std::left << std::setw(WIDTH_REGDATA) << opdStr.str();
        }
        if (isSource) {
            for (size_t i = size; i <= SRC2_IDX; i++) {
                oss << "|" << std::left << std::setw(WIDTH_REGDATA) << "";
            }
        }
        return oss.str();
    }
    std::string Dump(uint64_t lane = 0, bool firstLane = true)
    {
        std::stringstream oss;
        if (codeLen != EncodeLen::ENL_V || firstLane) {
            oss << "M" << std::left << std::setw(WIDTH_INSTNUM) << std::dec << execSeq;
            oss << "|TPC:0x" << std::setw(WIDTH_PC) << std::hex << pc;
            oss << " I" << std::setw(WIDTH_MINST) << std::dec << localExecSeq;
            oss << " T" << std::setw(1) << std::dec << threadId << " ";
        } else {
            int white = NUM5 + WIDTH_PC + NUM3 + WIDTH_MINST + NUM4;
            oss << std::right << std::setw(WIDTH_INSTNUM + NUM2) << "|" << std::setw(white) << "";
        }
        oss << "L" << std::left << std::setw(WIDTH_LANE) << std::dec << lane;
        oss << DumpOperands(dsts, false);
        oss << DumpOperands(srcs, true);

        if (codeLen != EncodeLen::ENL_V || firstLane) {
            oss << "|" << MInst::Assemble();
            oss << " |bin: 0x" << std::hex << binary;
        }
        if (accMemInfo != nullptr) {
            oss << " AccMemAddr 0x" << std::hex << accMemInfo->accMemAddr;
        }
        return oss.str();
    }
};
using MInstFuncPtr = std::shared_ptr<MInstFunc>;

class BlockFunc : public JCore::Block {
public:
    uint64_t threadId = 0;
    uint64_t execSeq = 0;
    uint64_t commitInstNum = 0;
    uint64_t nextBlockFetchPC = 0;
    uint64_t ctLocalPC = 0;
    struct CTStatus {
        /* mcopy */
        uint64_t dstAddr = 0;
        uint64_t srcAddr = 0;
        uint64_t ldCnt = 0; /* 状态机的内部缓存，用来记录已从源地址加载但未写入目的地址的字节数 */
        uint64_t stCnt = 0; /* 状态机的内部缓存，用来记录已从源地址加载但未写入目的地址的字节数 */
        uint64_t ldOffset = 0; /* 状态机的内部缓存，用来记录读内存地址偏移 */
        uint64_t stOffset = 0; /* 状态机的内部缓存，用来记录写内存地址偏移 */
        bool vld = false; /* 状态机的内部缓存，用来记录进入第三个阶段前的阶段中，最后是否有8字节写内存，如果有则置为1 */
        /* mset */
        uint64_t addr = 0;
        uint64_t count = 0;
        uint64_t data = 0;
        uint64_t offset = 0;
    };
    MInstFuncPtr startMinst = nullptr;
    std::shared_ptr<CTStatus> ctStatus = nullptr;
    std::vector<MInstFuncPtr>   ctInsts; // For template code gen

    std::vector<MInstFuncPtr>   minsts; // For Reference core

    Layer2ArchStatus            localArchStatus;

    bool                        simt = false;
    bool                        simtFirst = false;
    uint64_t                    execLane = 1;
    uint64_t                    completedBodyIters = 0;
    uint64_t                    completedGroup = 0;

    explicit BlockFunc() = default;
    explicit BlockFunc(JCore::Block &blk) : JCore::Block(blk) {}

    ~BlockFunc() = default;
    std::string Dump()
    {
        std::stringstream oss;
        oss << "B" << std::dec << execSeq << " ";
        oss << " T" << std::dec << threadId << " ";
        oss << Block::Dump();
        return oss.str();
    }

    inline uint64_t GetDstTileCount(OperandType type)
    {
        uint64_t count = 0;
        for (auto &tile : this->dstTile) {
            if (tile->type == type) {
                count++;
            }
        }
        return count;
    };
};
using BlockFuncPtr = std::shared_ptr<BlockFunc>;
}
#endif