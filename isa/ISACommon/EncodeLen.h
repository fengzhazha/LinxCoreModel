#pragma once

#ifndef ENCODE_LEN
#define ENCODE_LEN

#include <unordered_map>
#include <string>
#include <vector>
#include <cstdint>

namespace JCore {
enum class EncodeLen {
    ENL_C = 2,      // 16
    ENL_W = 4,      // 32
    ENL_H = 6,      // 48
    ENL_L = 8,      // 64
    ENL_V = 9,      // 64
};

static const std::unordered_map<EncodeLen, std::vector<std::string>> ENCODE_LEN_NAMES = {
    {EncodeLen::ENL_C, {"c.", "C."}},
    {EncodeLen::ENL_W, {"", ""}},
    {EncodeLen::ENL_H, {"hl.", "HL."}},
    {EncodeLen::ENL_L, {"l.", "L."}},
    {EncodeLen::ENL_V, {"v.", "V."}},
};
inline std::string GetEncodeLenName(EncodeLen len, bool upper = false)
{
    return ENCODE_LEN_NAMES.at(len).at(static_cast<size_t>(upper));
}
inline uint64_t GetEncodeLenVal(EncodeLen len)
{
    switch (len) {
        case EncodeLen::ENL_C:
            return static_cast<uint64_t>(EncodeLen::ENL_C);
        case EncodeLen::ENL_W:
            return static_cast<uint64_t>(EncodeLen::ENL_W);
        case EncodeLen::ENL_H:
            return static_cast<uint64_t>(EncodeLen::ENL_H);
        case EncodeLen::ENL_L:
        case EncodeLen::ENL_V:
            return static_cast<uint64_t>(EncodeLen::ENL_L);
        default:
            return static_cast<uint64_t>(EncodeLen::ENL_W);
    }
}
inline bool IsScalarInst(EncodeLen len)
{
    switch (len) {
        case EncodeLen::ENL_V:
            return false;
        default:
            return true;
    }
}
}
#endif