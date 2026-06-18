#pragma once

#ifndef LAY_OUT
#define LAY_OUT

#include <string>
#include <array>
#include <map>

namespace JCore {

enum class LayoutType {
    N,
    D,
    Z,
    INVALID,
};

enum class FractalType {
    // Row major
    RD = 0,
    // Col major
    CD,
    ZZ,
    ZN,
    NZ,
    NN,
    FRAC_COUNT,
};

enum class LayOut {
    NORM = 0,
    ND2DN = 1,
    ND2ZZ = 2,
    ND2ZN = 3,
    ND2NZ = 4,
    ND2NN = 5,
    DN2ND = 6,
    DN2ZZ = 7,
    DN2ZN = 8,
    DN2NZ = 9,
    DN2NN = 10,
    ZZ2ND = 11,
    ZZ2DN = 12,
    ZZ2ZN = 13,
    ZZ2NZ = 14,
    ZZ2NN = 15,
    ZN2ND = 17,
    ZN2DN = 18,
    ZN2ZZ = 19,
    ZN2NZ = 20,
    ZN2NN = 21,
    NN2ND = 22,
    NN2DN = 23,
    NN2ZZ = 24,
    NN2ZN = 25,
    NN2NZ = 26,
    NZ2ND = 27,
    NZ2DN = 28,
    NZ2ZZ = 29,
    NZ2ZN = 30,
    NZ2NN = 31,
    LAYOUT_COUNT,
};


inline void ExtractShapeInfo(LayOut layout, FractalType &srcType, FractalType &dstType)
{
    const static std::map<LayOut, std::pair<FractalType, FractalType>> fracMap = {
        // NORMAL时分型不变换, 默认为大N小Z
        {LayOut::NORM , {FractalType::NZ, FractalType::NZ}},

        {LayOut::ND2DN, {FractalType::RD, FractalType::CD}},
        {LayOut::ND2ZZ, {FractalType::RD, FractalType::ZZ}},
        {LayOut::ND2ZN, {FractalType::RD, FractalType::ZN}},
        {LayOut::ND2NZ, {FractalType::RD, FractalType::NZ}},
        {LayOut::ND2NN, {FractalType::RD, FractalType::NN}},

        {LayOut::DN2ND, {FractalType::CD, FractalType::RD}},
        {LayOut::DN2ZZ, {FractalType::CD, FractalType::ZZ}},
        {LayOut::DN2ZN, {FractalType::CD, FractalType::ZN}},
        {LayOut::DN2NZ, {FractalType::CD, FractalType::NZ}},
        {LayOut::DN2NN, {FractalType::CD, FractalType::NN}},

        {LayOut::ZZ2ND, {FractalType::ZZ, FractalType::RD}},
        {LayOut::ZZ2DN, {FractalType::ZZ, FractalType::CD}},
        {LayOut::ZZ2ZN, {FractalType::ZZ, FractalType::ZN}},
        {LayOut::ZZ2NZ, {FractalType::ZZ, FractalType::NZ}},
        {LayOut::ZZ2NN, {FractalType::ZZ, FractalType::NN}},

        {LayOut::ZN2ND, {FractalType::ZN, FractalType::RD}},
        {LayOut::ZN2DN, {FractalType::ZN, FractalType::CD}},
        {LayOut::ZN2ZZ, {FractalType::ZN, FractalType::ZZ}},
        {LayOut::ZN2NZ, {FractalType::ZN, FractalType::NZ}},
        {LayOut::ZN2NN, {FractalType::ZN, FractalType::NN}},

        {LayOut::NN2ND, {FractalType::NN, FractalType::RD}},
        {LayOut::NN2DN, {FractalType::NN, FractalType::CD}},
        {LayOut::NN2ZZ, {FractalType::NN, FractalType::ZZ}},
        {LayOut::NN2ZN, {FractalType::NN, FractalType::ZN}},
        {LayOut::NN2NZ, {FractalType::NN, FractalType::NZ}},

        {LayOut::NZ2ND, {FractalType::NZ, FractalType::RD}},
        {LayOut::NZ2DN, {FractalType::NZ, FractalType::CD}},
        {LayOut::NZ2ZZ, {FractalType::NZ, FractalType::ZZ}},
        {LayOut::NZ2ZN, {FractalType::NZ, FractalType::ZN}},
        {LayOut::NZ2NN, {FractalType::NZ, FractalType::NN}},
    };

    auto it = fracMap.find(layout);
    if (it == fracMap.end()) {
        srcType = FractalType::RD;
        dstType = FractalType::RD;
    } else {
        srcType = it->second.first;
        dstType = it->second.second;
    }
}

inline const std::array<std::string, static_cast<std::size_t>(FractalType::FRAC_COUNT)>& FractalNames()
{
    static const std::array<std::string, static_cast<std::size_t>(FractalType::FRAC_COUNT)> FRACTAL_NAMES = {{
        "ND",
        "DN",
        "ZZ",
        "ZN",
        "NZ",
        "NN"
    }};
    return FRACTAL_NAMES;
}

inline const std::string& GetFractalName(FractalType frac)
{
    const std::size_t idx = static_cast<std::size_t>(frac);
    const auto& names = FractalNames();
    static const std::string INVALID_FRACTAL_NAMES = "invalid_fractal";
    if (idx >= names.size()) {
        return INVALID_FRACTAL_NAMES;
    }
    return names[idx];
}

inline const std::array<std::string, static_cast<std::size_t>(LayOut::LAYOUT_COUNT)>& LayOutNames()
{
    static const std::array<std::string, static_cast<std::size_t>(LayOut::LAYOUT_COUNT)> LAYOUT_NAMES = {{
        "NORM",
        "ND2DN",
        "ND2ZZ",
        "ND2ZN",
        "ND2NZ",
        "ND2NN",
        "DN2ND",
        "DN2ZZ",
        "DN2ZN",
        "DN2NZ",
        "DN2NN",
        "ZZ2ND",
        "ZZ2DN",
        "ZZ2ZN",
        "ZZ2NZ",
        "ZZ2NN",
        "RESERVER",
        "ZN2ND",
        "ZN2DN",
        "ZN2ZZ",
        "ZN2NZ",
        "ZN2NN",
        "NN2ND",
        "NN2DN",
        "NN2ZZ",
        "NN2ZN",
        "NN2NZ",
        "NZ2ND",
        "NZ2DN",
        "NZ2ZZ",
        "NZ2ZN",
        "NZ2NN",
    }};
    return LAYOUT_NAMES;
}

inline const std::string& GetLayOutName(LayOut layout)
{
    const std::size_t idx = static_cast<std::size_t>(layout);
    const auto& names = LayOutNames();
    static const std::string INVALID_LAYOUT_NAMES = "invalid_layout";
    if (idx >= names.size()) {
        return INVALID_LAYOUT_NAMES;
    }
    return names[idx];
}
}
#endif