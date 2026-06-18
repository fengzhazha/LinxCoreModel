#pragma once
#ifndef BLOCK_TYPE_H
#define BLOCK_TYPE_H

#include <string>
#include <array>
#include <cstdint>
#include <unordered_set>

namespace JCore {
static const uint64_t TYPE_XB_ENCODING = 0x1f;
enum class BlockType {
    BLK_TYPE_STD = 0,
    BLK_TYPE_SYS,
    BLK_TYPE_FP,
    BLK_TYPE_MPAR,
    BLK_TYPE_MSEQ,
    BLK_TYPE_VPAR,
    BLK_TYPE_VSEQ,
    BLK_TYPE_TMA,
    BLK_TYPE_CUBE,
    BLK_TYPE_TEPL,
    BLK_TYPE_XB,
    BLK_TYPE_FIXP,
    BLK_TYPE_COUNT,
    BLK_TYPE_INVAL
};

inline const std::array<std::string, static_cast<std::size_t>(BlockType::BLK_TYPE_INVAL)>& BlockTypeNames()
{
    static const std::array<std::string, static_cast<std::size_t>(BlockType::BLK_TYPE_INVAL)> BLK_TYPE_NAMES = {{
        "STD",
        "SYS",
        "FP",
        "MPAR",
        "MSEQ",
        "VPAR",
        "VSEQ",
        "TMA",
        "CUBE",
        "TEPL",
        "XB",
    }};
    return BLK_TYPE_NAMES;
}

inline const std::string& GetBlockTypeName(BlockType bt)
{
    const std::size_t idx = static_cast<std::size_t>(bt);
    const auto& names = BlockTypeNames();
    static const std::string INVALID_BLOCK_TYPE = "invalid_block_type";
    if (idx >= names.size()) {
        return INVALID_BLOCK_TYPE;
    }
    return names[idx];
}

constexpr uint64_t ENCODE0 = 0;
constexpr uint64_t ENCODE1 = 1;
constexpr uint64_t ENCODE2 = 2;
constexpr uint64_t ENCODE3 = 3;
constexpr uint64_t ENCODE31 = 31;

inline const std::array<uint64_t, static_cast<std::size_t>(BlockType::BLK_TYPE_INVAL)>& BlockTypeBARGEncode()
{
    static const std::array<uint64_t, static_cast<std::size_t>(BlockType::BLK_TYPE_INVAL)> BLK_BARG_ENCODE = {{
        ENCODE0, // BLK_TYPE_STD = 0,
        ENCODE1, // BLK_TYPE_SYS,
        ENCODE2, // BLK_TYPE_FP,
        ENCODE3, // BLK_TYPE_MPAR,
        ENCODE3, // BLK_TYPE_MSEQ,
        ENCODE3, // BLK_TYPE_VPAR,
        ENCODE3, // BLK_TYPE_VSEQ,
        ENCODE3, // BLK_TYPE_TMA,
        ENCODE3, // BLK_TYPE_CUBE,
        ENCODE3, // BLK_TYPE_TEPL,
        ENCODE31, // BLK_TYPE_XB,
    }};
    return BLK_BARG_ENCODE;
}

inline uint64_t GetBlockBARGEncode(BlockType bt)
{
    const std::size_t idx = static_cast<std::size_t>(bt);
    const auto& names = BlockTypeBARGEncode();
    static const uint64_t INVALID_BLK_TYPE_BARG_ENCODE = -1;
    if (idx >= static_cast<uint64_t>(BlockType::BLK_TYPE_COUNT)) {
        return INVALID_BLK_TYPE_BARG_ENCODE;
    }
    return names[idx];
}

inline bool IsBlockTypeNeedVReg(BlockType bt)
{
    static const std::unordered_set<BlockType> VREG_BLK_SET = {
        BlockType::BLK_TYPE_MPAR, BlockType::BLK_TYPE_MSEQ, BlockType::BLK_TYPE_VPAR, BlockType::BLK_TYPE_VSEQ,
    };
    return VREG_BLK_SET.find(bt) != VREG_BLK_SET.end();
}

inline bool IsBlockTypeMCall(BlockType bt)
{
    static const std::unordered_set<BlockType> MCALL_BLK_SET = {
        BlockType::BLK_TYPE_MPAR, BlockType::BLK_TYPE_MSEQ,
    };
    return MCALL_BLK_SET.find(bt) != MCALL_BLK_SET.end();
}

inline bool IsBlockTypeVCall(BlockType bt)
{
    static const std::unordered_set<BlockType> MCALL_BLK_SET = {
        BlockType::BLK_TYPE_VPAR, BlockType::BLK_TYPE_VSEQ,
    };
    return MCALL_BLK_SET.find(bt) != MCALL_BLK_SET.end();
}

inline bool IsBlockTypeParallel(BlockType bt)
{
    static const std::unordered_set<BlockType> PAR_BLK_SET = {
        BlockType::BLK_TYPE_MPAR, BlockType::BLK_TYPE_MSEQ, BlockType::BLK_TYPE_VPAR, BlockType::BLK_TYPE_VSEQ,
        BlockType::BLK_TYPE_TMA, BlockType::BLK_TYPE_CUBE, BlockType::BLK_TYPE_TEPL,
    };
    return PAR_BLK_SET.find(bt) != PAR_BLK_SET.end();
}

} // namespace JCore
#endif // BLOCK_TYPE_H