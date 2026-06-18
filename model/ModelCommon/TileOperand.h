#ifndef TILE_OPERAND_H
#define TILE_OPERAND_H

#include <memory>

#include "ISA.h"
#include "ROBID.h"

namespace JCore {

class TileOperand : public Operand {
public:
    bool vld = false;
    bool renamed = false;
    bool ready = false;
    bool isZero = false;

    OperandType handType = OperandType::OPD_INVALID;
    uint64_t link = 0; // value + 1
    uint64_t tileTag = 0;
    ROBID bid;
    uint32_t stid = 0;

    /* for stash */
    bool wayAllocated = false;
    bool stashReady = false;
    uint32_t dstNodeId = 0;
    uint32_t wayId = 0;
    uint64_t row = 0;
    uint64_t col = 0;
    uint64_t validRow = 0;
    uint64_t validCol = 0;
    uint64_t padValue = 0;
    uint64_t size = 0; // Byte
    LayoutType bigLayout = LayoutType::INVALID;
    LayoutType smallLayout = LayoutType::INVALID;

    TileOperand() = default;

    std::string Dump()
    {
        std::stringstream oss;
        oss << GetTileTypeStr(handType);
        if (!isDst) {
            oss << "#" << (value + 1);
        }
        oss << " TileTag:" << std::dec << tileTag;
        oss << " addr:0x" << std::hex << baseAddr;
        oss << " size:" << std::dec << size;
        oss << " way:" << std::dec << wayId;
        oss << " ready:" << ready;
        oss << " isZero: " << isZero;
        return oss.str();
    }
};
using TileOperandPtr = std::shared_ptr<TileOperand>;

inline TileOperandPtr GetTOOperand(std::vector<TileOperandPtr> dsts, OperandType formalType, BlockType bType)
{
    if (formalType == OperandType::OPD_TO_GPR_REG) {
        for (auto dst : dsts) {
            if (dst->handType == OperandType::OPD_TILE_MCALL_GPR) {
                return dst;
            }
        }
    } else if (IsBlockTypeMCall(bType) && formalType == OperandType::OPD_TO1_REG) {
        for (auto dst : dsts) {
            if (dst->handType == OperandType::OPD_TILE_MCALL_STK) {
                return dst;
            }
        }
    }
    uint64_t idx = static_cast<uint64_t>(formalType) - static_cast<uint64_t>(OperandType::OPD_TO_REG);
    if (idx >= dsts.size()) {
        idx = 0;
    }
    return dsts[idx];
}

}
#endif
