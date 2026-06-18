#pragma once
#ifndef BLOCK_BRANCH_TYPE
#define BLOCK_BRANCH_TYPE

#include <string>
#include <array>
#include <cstdint>

namespace JCore {

enum class BranchType {
    BLK_BR_INVAL = 0,
    BLK_BR_FALL = 1,    // 001
    BLK_BR_DIRECT = 2,  // 010
    BLK_BR_COND = 3,    // 011
    BLK_BR_CALL = 4,    // 100
    BLK_BR_IND = 5,     // 101
    BLK_BR_ICALL = 6,   // 110
    BLK_BR_RET = 7,     // 111
    BLK_BR_COUNT,
};

inline const std::array<std::string, static_cast<std::size_t>(BranchType::BLK_BR_COUNT)>& BlockBranchNames()
{
    static const std::array<std::string, static_cast<std::size_t>(BranchType::BLK_BR_COUNT)> BLK_BRANCH_NAMES = {{
        "RESERVE",
        "FALL",
        "DIRECT",
        "COND",
        "CALL",
        "IND",
        "ICALL",
        "RET",
    }};
    return BLK_BRANCH_NAMES;
}

inline const std::string& GetBlockBranchTypeName(BranchType type)
{
    const std::size_t idx = static_cast<std::size_t>(type);
    const auto& names = BlockBranchNames();
    static const std::string INVALID_BLOCK_BRANCH = "invalid_block_branch";
    if (idx >= names.size()) {
        return INVALID_BLOCK_BRANCH;
    }
    return names[idx];
}

constexpr uint64_t BR_ENCODE0 = 0;
constexpr uint64_t BR_ENCODE1 = 1;
constexpr uint64_t BR_ENCODE2 = 2;
constexpr uint64_t BR_ENCODE3 = 3;

inline const std::array<uint64_t, static_cast<std::size_t>(BranchType::BLK_BR_COUNT)>& BranchTypeBARGEncode()
{
    static const std::array<uint64_t, static_cast<std::size_t>(BranchType::BLK_BR_COUNT)> BR_BARG_ENCODE = {{
        BR_ENCODE0, // BLK_BR_INVAL = 0,
        BR_ENCODE0, // BLK_BR_FALL = 1,    // 001
        BR_ENCODE1, // BLK_BR_DIRECT = 2,  // 010
        BR_ENCODE2, // BLK_BR_COND = 3,    // 011
        BR_ENCODE1, // BLK_BR_CALL = 4,    // 100
        BR_ENCODE3, // BLK_BR_IND = 5,     // 101
        BR_ENCODE3, // BLK_BR_ICALL = 6,   // 110
        BR_ENCODE3, // BLK_BR_RET = 7,     // 111
    }};
    return BR_BARG_ENCODE;
}

inline uint64_t GetBranchBARGEncode(BranchType bt)
{
    const std::size_t idx = static_cast<std::size_t>(bt);
    const auto& names = BranchTypeBARGEncode();
    static const uint64_t INVALID_BR_TYPE_BARG_ENCODE = -1;
    if (idx >= static_cast<uint64_t>(BranchType::BLK_BR_COUNT)) {
        return INVALID_BR_TYPE_BARG_ENCODE;
    }
    return names[idx];
}

} // namespace JCore

#endif