#pragma once
#ifndef GPR_H
#define GPR_H

#include <array>
#include <string>

namespace JCore {
enum class GPR : std::size_t {
    GPR_ZERO = 0,   // R0  Zero
    GPR_SP   = 1,   // R1  SP
    GPR_A0   = 2,   // R2  A0
    GPR_A1   = 3,   // R3  A1
    GPR_A2   = 4,   // R4  A2
    GPR_A3   = 5,   // R5  A3
    GPR_A4   = 6,   // R6  A4
    GPR_A5   = 7,   // R7  A5
    GPR_A6   = 8,   // R8  A6
    GPR_A7   = 9,   // R9  A7
    GPR_RA   = 10,  // R10 RA
    GPR_S0   = 11,  // R11 FP(S0)
    GPR_S1   = 12,  // R12 S1
    GPR_S2   = 13,  // R13 S2
    GPR_S3   = 14,  // R14 S3
    GPR_S4   = 15,  // R15 S4
    GPR_S5   = 16,  // R16 S5
    GPR_S6   = 17,  // R17 S6
    GPR_S7   = 18,  // R18 S7
    GPR_S8   = 19,  // R19 S8
    GPR_X0   = 20,  // R20 X0
    GPR_X1   = 21,  // R21 X1
    GPR_X2   = 22,  // R22 X2
    GPR_X3   = 23,  // R23 X3
    GPR_COUNT = 24,
};
inline const std::array<std::string, static_cast<std::size_t>(GPR::GPR_COUNT)>& GPRNames()
{
    static const std::array<std::string, static_cast<std::size_t>(GPR::GPR_COUNT)> GPR_NAMES = {{
        "zero",
        "sp",
        "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
        "ra",
        "fp(s0)",
        "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8",
        "x0", "x1", "x2", "x3"
    }};
    return GPR_NAMES;
}

inline const std::string& GetGPRNameRef(GPR reg)
{
    const std::size_t idx = static_cast<std::size_t>(reg);
    const auto& names = GPRNames();
    static const std::string INVALID_GPR = "invalidgpr";
    if (idx >= names.size()) {
        return INVALID_GPR;
    }
    return names[idx];
}
}
#endif