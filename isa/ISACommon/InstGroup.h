#pragma once
#ifndef INST_GROUP_H
#define INST_GROUP_H

#include <array>
#include <string>

namespace JCore {
enum class InstGroup {
    BLOCK_SPLIT = 0,
    BLOCK_OFFSET,
    BLOCK_IO,
    BLOCK_ATTR,
    BLOCK_HINT,
    BLOCK_ARGUMENT,
    ARITHMETIC,
    ARITHMETIC_FP,
    COMPARE,
    COMPARE_FP,
    SETC,
    PC,
    IMMEDIATE,
    MOVE,
    BRANCH,
    MULTICYCLE,
    BIT,
    GQM,
    COMPOUND,
    PREFETCH,
    LOAD,
    STORE,
    ATOMIC,
    EXECUTE_CONTROL,
    EXTEND,
    CACHE_MAINTAIN,
    GMO,
    SSR,
    MAX_MIN,
    CONVERT,
    REDUCE,
    SHUFFLE,
    SPECIAL,
    CT_CUSTOM,
    UNDEF
};

inline const std::array<std::string, static_cast<std::size_t>(InstGroup::UNDEF)>& InstGroupNames()
{
    static const std::array<std::string, static_cast<std::size_t>(InstGroup::UNDEF)> INST_GROUP_NAMES = {{
        "BLOCK_SPLIT",
        "BLOCK_OFFSET",
        "BLOCK_IO",
        "BLOCK_ATTR",
        "BLOCK_HINT",
        "BLOCK_ARGUMENT",
        "ARITHMETIC",
        "ARITHMETIC_FP",
        "COMPARE",
        "COMPARE_FP",
        "SETC",
        "PC",
        "IMMEDIATE",
        "MOVE",
        "BRANCH",
        "MULTICYCLE",
        "BIT",
        "GQM",
        "COMPOUND",
        "PREFETCH",
        "LOAD",
        "STORE",
        "ATOMIC",
        "EXECUTE_CONTROL",
        "EXTEND",
        "CACHE_MAINTAIN",
        "GMO",
        "SSR",
        "MAX_MIN",
        "CONVERT",
        "REDUCE",
        "SHUFFLE",
        "SPECIAL",
        "CT_CUSTOM",
    }};
    return INST_GROUP_NAMES;
}

inline const std::string& GetInstGroupName(InstGroup grp)
{
    const std::size_t idx = static_cast<std::size_t>(grp);
    const auto& names = InstGroupNames();
    static const std::string INVALID_GRP_NAMES = "invalid_group";
    if (idx >= names.size()) {
        return INVALID_GRP_NAMES;
    }
    return names[idx];
}
}
#endif
