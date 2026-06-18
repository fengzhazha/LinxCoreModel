#pragma once

#ifndef CONVERT_AGGRE_H
#define CONVERT_AGGRE_H

#include <functional>
#include <cstring>
#include <cstdint>
#include <cmath>

#include "MInstCalculator.h"


namespace JCore {
namespace Calculate {
bool IsNaN(OPConvertType cvt,uint64_t data);
bool IsSNaN(OPConvertType cvt,uint64_t data);
bool IsNaNOrSNan(OPConvertType cvt,uint64_t data);

FloatRoundMode FRM2FloatRoundMode(uint32_t FRM);
uint64_t ConvertAggre(uint64_t data, OPConvertType from, OPConvertType to, uint32_t FRM, bool needCheck = true);
uint64_t Fadd(OPConvertType cvt, uint64_t data1, uint64_t data2, FloatRoundMode frm);
uint64_t Fsub(OPConvertType cvt, uint64_t data1, uint64_t data2, FloatRoundMode frm);
uint64_t Fmul(OPConvertType cvt, uint64_t data1, uint64_t data2, FloatRoundMode frm);
uint64_t Fdiv(OPConvertType cvt, uint64_t data1, uint64_t data2, FloatRoundMode frm);
uint64_t Fmadd(OPConvertType cvt, uint64_t data1, uint64_t data2, uint64_t data3, FloatRoundMode frm);
uint64_t Fmsub(OPConvertType cvt, uint64_t data1, uint64_t data2, uint64_t data3, FloatRoundMode frm);
uint64_t Fnmadd(OPConvertType cvt, uint64_t data1, uint64_t data2, uint64_t data3, FloatRoundMode frm);
uint64_t Fnmsub(OPConvertType cvt, uint64_t data1, uint64_t data2, uint64_t data3, FloatRoundMode frm);
uint64_t Fabs(OPConvertType cvt, uint64_t data);
uint64_t Fsqrt(OPConvertType cvt, uint64_t data);
uint64_t Frecip(OPConvertType cvt, uint64_t data);
uint64_t Fexp(OPConvertType cvt, uint64_t data, FloatRoundMode frm);
uint64_t Fclass(OPConvertType cvt,uint64_t data);
uint64_t Fmax(OPConvertType cvt, uint64_t data1, uint64_t data2, FloatRoundMode frm);
uint64_t Fmin(OPConvertType cvt, uint64_t data1, uint64_t data2, FloatRoundMode frm);
uint64_t Feq(OPConvertType cvt,uint64_t data1,uint64_t data2, FloatRoundMode frm);
uint64_t Feqs(OPConvertType cvt,uint64_t data1,uint64_t data2, FloatRoundMode frm);
uint64_t Flt(OPConvertType cvt,uint64_t data1,uint64_t data2, FloatRoundMode frm);
uint64_t Flts(OPConvertType cvt,uint64_t data1,uint64_t data2, FloatRoundMode frm);

template <typename T>
T Fexp(uint64_t data)
{
    T x{};
    if (sizeof(T) == 4) {
        uint32_t lo = static_cast<uint32_t>(data);
        std::memcpy(&x, &lo, sizeof(x));
    } else {
        std::memcpy(&x, &data, sizeof(x));
    }
    return std::exp(x);
}

template <typename T>
T Frecip(uint64_t data)
{
    T x{};
    if (sizeof(T) == 4) {
        uint32_t lo = static_cast<uint32_t>(data);
        std::memcpy(&x, &lo, sizeof(x));
    } else {
        std::memcpy(&x, &data, sizeof(x));
    }
    return static_cast<T>(1.0) / x;
}

} // namespace Calculate
} // namespace JCore
#endif