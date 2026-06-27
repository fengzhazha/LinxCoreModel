#include <cmath>

#include "../../softfloat/include/softfloat.h"
#include "FloatPointUtils.h"

namespace JCore {
namespace Calculate {
bool IsNaN(OPConvertType cvt,uint64_t data)
{
    uint64_t exponent = 0;
    uint64_t mantissa = 0;
    switch (cvt) {
        case OPConvertType::OPCVT_FP64: {
            exponent = (((uint64_t)data >> 52) & 0x7ff);
            mantissa = data & 0xfffffffffffff;
            if(exponent == 0x7ff && mantissa != 0x0){
                return true;
            }
            break;
        }
        case OPConvertType::OPCVT_FP32: {
            exponent = (((uint64_t)data >> 23) & 0xff);
            mantissa = data & 0x7fffff;
            if(exponent == 0xff && mantissa != 0x0){
                return true;
            }
            break;
        }
        case OPConvertType::OPCVT_FP16: { // 1 5 10
            exponent = (((uint64_t)data >> 10) & 0x1f);
            mantissa = data & 0x3ff;
            if(exponent == 0x1f && mantissa != 0x0){
                return true;
            }
            break;
        }
        case OPConvertType::OPCVT_FP8: { // E4M3
            exponent = (((uint64_t)data >> 3) & 0xf);
            mantissa = data & 0x7;
            if(exponent == 0xf && mantissa != 0x0){
                return true;
            }
            break;
        }
        case OPConvertType::OPCVT_FP8_1: { // E5M2
            exponent = (((uint64_t)data >> 2) & 0x1f);
            mantissa = data & 0x3;
            if(exponent == 0x1f && mantissa != 0x0){
                return true;
            }
            break;
        }
        case OPConvertType::OPCVT_BF16: {// 1 8 7
            exponent = (((uint64_t)data >> 7) & 0xff);
            mantissa = data & 0x7f;
            if(exponent == 0xff && mantissa != 0x0){
                return true;
            }
            break;
        }
        default:
            return false;
    }
    return false;
}

bool IsSNaN(OPConvertType cvt,uint64_t data)
{
    uint64_t mantissa = 0;
    switch (cvt) {
        case OPConvertType::OPCVT_FP64: { // 1 11 52
            mantissa = data & 0xfffffffffffff;
            uint64_t msbit = (mantissa >> 51) & 0x1; // Mantissa most significant bit
            if(IsNaN(OPConvertType::OPCVT_FP64, data) && msbit == 0){ // SNaN
                return true;
            }
            break;
        }
        case OPConvertType::OPCVT_FP32: { // 1 8 23
            mantissa = data & 0x7fffff;
            uint64_t msbit = (mantissa >> 22) & 0x1; // Mantissa most significant bit
            if(IsNaN(OPConvertType::OPCVT_FP32, data) && msbit == 0) { // SNaN
                return true;
            }
            break;
        }
        case OPConvertType::OPCVT_FP16: { // 1 5 10
            mantissa = data & 0x3ff;
            uint64_t msbit = (mantissa >> 9) & 0x1;// Mantissa most significant bit
            if(IsNaN(OPConvertType::OPCVT_FP16, data) && msbit == 0){// SNaN
                return true;
            }
            break;
        }
        case OPConvertType::OPCVT_FP8: { // E4M3
            mantissa = data & 0x7;
            uint64_t msbit = (mantissa >> 2) & 0x1;// Mantissa most significant bit
            if(IsNaN(OPConvertType::OPCVT_FP8, data) && msbit == 0){// SNaN
                return true;
            }
            break;
        }
        case OPConvertType::OPCVT_FP8_1: { // E5M2
            mantissa = data & 0x3;
            uint64_t msbit = (mantissa >> 1) & 0x1;// Mantissa most significant bit
            if(IsNaN(OPConvertType::OPCVT_FP8_1, data) && msbit == 0){// SNaN
                return true;
            }
            break;
        }
        case OPConvertType::OPCVT_BF16: { // 1 8 7
            mantissa = data & 0x7f;
            uint64_t msbit = (mantissa >> 6) & 0x1;// Mantissa most significant bit
            if(IsNaN(OPConvertType::OPCVT_BF16, data) && msbit == 0){// SNaN
                return true;
            }
            break;
        }
        default:
            return false;
    }
    return false;
}

bool IsNaNOrSNan(OPConvertType cvt,uint64_t data) {
    return IsNaN(cvt, data) || IsSNaN(cvt, data);
}

FloatRoundMode FRM2FloatRoundMode(uint32_t FRM) {
    switch (FRM) {
        case RNE_MODE:
            return float_round_nearest_even;
        case RTZ_MODE:
            return float_round_to_zero;
        case RDN_MODE:
            return float_round_down;
        case RUP_MODE:
            return float_round_up;
        case RNA_MODE:
            return float_round_ties_away;
        default:
            assert(0 && "error FloatRoundMode\r\n");
            return float_round_to_zero;
    }
}
static void InitConvertMapFp(std::map<std::pair<OPConvertType, OPConvertType>,
                             std::function<uint64_t(uint64_t, FloatRoundMode)>> &funcMap)
{
    // OPCVT_FP64 -> other
    funcMap[{OPConvertType::OPCVT_FP64, OPConvertType::OPCVT_U64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float64_to_uint64_scalbn(
                                        static_cast<float64>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP64, OPConvertType::OPCVT_U32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float64_to_uint32_scalbn(
                                        static_cast<float64>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP64, OPConvertType::OPCVT_U16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float64_to_uint16_scalbn(
                                        static_cast<float64>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP64, OPConvertType::OPCVT_U8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float64_to_uint8_scalbn(
                                        static_cast<float64>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP64, OPConvertType::OPCVT_S64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float64_to_int64_scalbn(
                                        static_cast<float64>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP64, OPConvertType::OPCVT_S32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float64_to_int32_scalbn(
                                        static_cast<float64>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP64, OPConvertType::OPCVT_S16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float64_to_int16_scalbn(
                                        static_cast<float64>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP64, OPConvertType::OPCVT_S8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float64_to_int8_scalbn(
                                        static_cast<float64>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP64, OPConvertType::OPCVT_FP64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {return data;};
    funcMap[{OPConvertType::OPCVT_FP64, OPConvertType::OPCVT_FP32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float64_to_float32(static_cast<float64>(data), &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP64, OPConvertType::OPCVT_FP16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float64_to_float16(static_cast<float64>(data), true, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP64, OPConvertType::OPCVT_FP4}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float64_to_float4(static_cast<float64>(data), &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP64, OPConvertType::OPCVT_FP4_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float64_to_float4_1(static_cast<float64>(data), &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP64, OPConvertType::OPCVT_FP8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float64_to_float8(static_cast<float64>(data), &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP64, OPConvertType::OPCVT_FP8_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float64_to_float8_1(static_cast<float64>(data), &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP64, OPConvertType::OPCVT_BF16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float64_to_bfloat16(static_cast<float64>(data), &status);
                                };

    // OPConvertType::OPCVT_FP32 -> other
    funcMap[{OPConvertType::OPCVT_FP32, OPConvertType::OPCVT_U64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float32_to_uint64_scalbn(
                                        static_cast<float32>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP32, OPConvertType::OPCVT_U32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float32_to_uint32_scalbn(
                                        static_cast<float32>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP32, OPConvertType::OPCVT_U16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float32_to_uint16_scalbn(
                                        static_cast<float32>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP32, OPConvertType::OPCVT_U8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float32_to_uint8_scalbn(
                                        static_cast<float32>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP32, OPConvertType::OPCVT_S64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float32_to_int64_scalbn(
                                        static_cast<float32>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP32, OPConvertType::OPCVT_S32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float32_to_int32_scalbn(
                                        static_cast<float32>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP32, OPConvertType::OPCVT_S16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float32_to_int16_scalbn(
                                        static_cast<float32>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP32, OPConvertType::OPCVT_S8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float32_to_int8_scalbn(
                                        static_cast<float32>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP32, OPConvertType::OPCVT_FP64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float32_to_float64(static_cast<float32>(data), &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP32, OPConvertType::OPCVT_FP32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {return data;};
    funcMap[{OPConvertType::OPCVT_FP32, OPConvertType::OPCVT_FP16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float32_to_float16(static_cast<float32>(data), true, &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP32, OPConvertType::OPCVT_FP4}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float32_to_float4(static_cast<float32>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP32, OPConvertType::OPCVT_FP4_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float32_to_float4_1(static_cast<float32>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP32, OPConvertType::OPCVT_FP8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float32_to_float8(static_cast<float32>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP32, OPConvertType::OPCVT_FP8_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float32_to_float8_1(static_cast<float32>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP32, OPConvertType::OPCVT_BF16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float32_to_bfloat16(static_cast<float32>(data), &status));
                                };

    // OPConvertType::OPCVT_FP16 -> other
    funcMap[{OPConvertType::OPCVT_FP16, OPConvertType::OPCVT_U64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float16_to_uint64_scalbn(
                                        static_cast<float16>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP16, OPConvertType::OPCVT_U32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float16_to_uint32_scalbn(
                                        static_cast<float16>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP16, OPConvertType::OPCVT_U16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float16_to_uint16_scalbn(
                                        static_cast<float16>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP16, OPConvertType::OPCVT_U8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float16_to_uint8_scalbn(
                                        static_cast<float16>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP16, OPConvertType::OPCVT_S64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float16_to_int64_scalbn(
                                        static_cast<float16>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP16, OPConvertType::OPCVT_S32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float16_to_int32_scalbn(
                                        static_cast<float16>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP16, OPConvertType::OPCVT_S16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float16_to_int16_scalbn(
                                        static_cast<float16>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP16, OPConvertType::OPCVT_S8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float16_to_int8_scalbn(
                                        static_cast<float16>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP16, OPConvertType::OPCVT_FP64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float16_to_float64(static_cast<float16>(data), true, &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP16, OPConvertType::OPCVT_FP32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float16_to_float32(static_cast<float16>(data), true, &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP16, OPConvertType::OPCVT_FP16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {return data;};
    funcMap[{OPConvertType::OPCVT_FP16, OPConvertType::OPCVT_FP4}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float16_to_float4(static_cast<float16>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP16, OPConvertType::OPCVT_FP4_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float16_to_float4_1(static_cast<float16>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP16, OPConvertType::OPCVT_FP8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float16_to_float8(static_cast<float16>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP16, OPConvertType::OPCVT_FP8_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float16_to_float8_1(static_cast<float16>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP16, OPConvertType::OPCVT_BF16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float16_to_bfloat16(static_cast<float16>(data), &status));
                                };

    // OPConvertType::OPCVT_FP8 -> other
    funcMap[{OPConvertType::OPCVT_FP8, OPConvertType::OPCVT_U64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float8_to_uint64_scalbn(
                                        static_cast<float8>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP8, OPConvertType::OPCVT_U32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float8_to_uint32_scalbn(
                                        static_cast<float8>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP8, OPConvertType::OPCVT_U16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float8_to_uint16_scalbn(
                                        static_cast<float8>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP8, OPConvertType::OPCVT_U8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float8_to_uint8_scalbn(
                                        static_cast<float8>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP8, OPConvertType::OPCVT_S64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float8_to_int64_scalbn(
                                        static_cast<float8>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP8, OPConvertType::OPCVT_S32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float8_to_int32_scalbn(
                                        static_cast<float8>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP8, OPConvertType::OPCVT_S16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float8_to_int16_scalbn(
                                        static_cast<float8>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP8, OPConvertType::OPCVT_S8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float8_to_int8_scalbn(
                                        static_cast<float8>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP8, OPConvertType::OPCVT_FP64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float8_to_float64(static_cast<float8>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP8, OPConvertType::OPCVT_FP32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float8_to_float32(static_cast<float8>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP8, OPConvertType::OPCVT_FP16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float8_to_float16(static_cast<float8>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP8, OPConvertType::OPCVT_FP4}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float8_to_float4(static_cast<float8>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP8, OPConvertType::OPCVT_FP4_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float8_to_float4_1(static_cast<float8>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP8, OPConvertType::OPCVT_FP8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {return data;};
    funcMap[{OPConvertType::OPCVT_FP8, OPConvertType::OPCVT_FP8_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float8_to_float8_1(static_cast<float8>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP8, OPConvertType::OPCVT_BF16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float8_to_bfloat16(static_cast<float8>(data), &status));
                                };

    // OPConvertType::OPCVT_FP8_1 -> other
    funcMap[{OPConvertType::OPCVT_FP8_1, OPConvertType::OPCVT_U64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float8_1_to_uint64_scalbn(
                                        static_cast<float8_1>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP8_1, OPConvertType::OPCVT_U32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float8_1_to_uint32_scalbn(
                                        static_cast<float8_1>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP8_1, OPConvertType::OPCVT_U16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float8_1_to_uint16_scalbn(
                                        static_cast<float8_1>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP8_1, OPConvertType::OPCVT_U8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float8_1_to_uint8_scalbn(
                                        static_cast<float8_1>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP8_1, OPConvertType::OPCVT_S64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float8_1_to_int64_scalbn(
                                        static_cast<float8_1>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP8_1, OPConvertType::OPCVT_S32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float8_1_to_int32_scalbn(
                                        static_cast<float8_1>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP8_1, OPConvertType::OPCVT_S16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float8_1_to_int16_scalbn(
                                        static_cast<float8_1>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP8_1, OPConvertType::OPCVT_S8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float8_1_to_int8_scalbn(
                                        static_cast<float8_1>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP8_1, OPConvertType::OPCVT_FP64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float8_1_to_float64(static_cast<float8_1>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP8_1, OPConvertType::OPCVT_FP32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float8_1_to_float32(static_cast<float8_1>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP8_1, OPConvertType::OPCVT_FP16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float8_1_to_float16(static_cast<float8_1>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP8_1, OPConvertType::OPCVT_FP4}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float8_1_to_float4(static_cast<float8>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP8_1, OPConvertType::OPCVT_FP4_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float8_1_to_float4_1(static_cast<float8>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP8_1, OPConvertType::OPCVT_FP8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return  static_cast<uint64_t>(
                                        float8_1_to_float8(static_cast<float8_1>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP8_1, OPConvertType::OPCVT_FP8_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {return data;};
    funcMap[{OPConvertType::OPCVT_FP8_1, OPConvertType::OPCVT_BF16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float8_1_to_bfloat16(static_cast<float8_1>(data), &status));
                                };

    // OPConvertType::OPCVT_BF16 -> other
    funcMap[{OPConvertType::OPCVT_BF16, OPConvertType::OPCVT_U64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return bfloat16_to_uint64_scalbn(
                                        static_cast<bfloat16>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_BF16, OPConvertType::OPCVT_U32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return bfloat16_to_uint32_scalbn(
                                        static_cast<bfloat16>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_BF16, OPConvertType::OPCVT_U16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return bfloat16_to_uint16_scalbn(
                                        static_cast<bfloat16>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_BF16, OPConvertType::OPCVT_U8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return bfloat16_to_uint8_scalbn(
                                        static_cast<bfloat16>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_BF16, OPConvertType::OPCVT_S64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return bfloat16_to_int64_scalbn(
                                        static_cast<bfloat16>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_BF16, OPConvertType::OPCVT_S32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return bfloat16_to_int32_scalbn(
                                        static_cast<bfloat16>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_BF16, OPConvertType::OPCVT_S16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return bfloat16_to_int16_scalbn(
                                        static_cast<bfloat16>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_BF16, OPConvertType::OPCVT_S8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return bfloat16_to_int8_scalbn(
                                        static_cast<bfloat16>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_BF16, OPConvertType::OPCVT_FP64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        bfloat16_to_float64(static_cast<bfloat16>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_BF16, OPConvertType::OPCVT_FP32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        bfloat16_to_float32(static_cast<bfloat16>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_BF16, OPConvertType::OPCVT_FP16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        bfloat16_to_float16(static_cast<bfloat16>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_BF16, OPConvertType::OPCVT_FP8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        bfloat16_to_float8(static_cast<bfloat16>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_BF16, OPConvertType::OPCVT_FP8_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        bfloat16_to_float8_1(static_cast<bfloat16>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_BF16, OPConvertType::OPCVT_E6M2}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        bfloat16_to_e6m2(static_cast<bfloat16>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_BF16, OPConvertType::OPCVT_FP4}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        bfloat16_to_float4(static_cast<bfloat16>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_BF16, OPConvertType::OPCVT_FP4_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        bfloat16_to_float4_1(static_cast<bfloat16>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_BF16, OPConvertType::OPCVT_HIF4}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        bfloat16_to_float4_1(static_cast<bfloat16>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_BF16, OPConvertType::OPCVT_BF16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {return data;};

    // OPConvertType::OPCVT_FP4_1 -> other
    funcMap[{OPConvertType::OPCVT_FP4_1, OPConvertType::OPCVT_U64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float4_1_to_uint64_scalbn(
                                        static_cast<float4_1>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP4_1, OPConvertType::OPCVT_U32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float4_1_to_uint32_scalbn(
                                        static_cast<float4_1>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP4_1, OPConvertType::OPCVT_U16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float4_1_to_uint16_scalbn(
                                        static_cast<float4_1>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP4_1, OPConvertType::OPCVT_U8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float4_1_to_uint8_scalbn(
                                        static_cast<float4_1>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP4_1, OPConvertType::OPCVT_S64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float4_1_to_int64_scalbn(
                                        static_cast<float4_1>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP4_1, OPConvertType::OPCVT_S32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float4_1_to_int32_scalbn(
                                        static_cast<float4_1>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP4_1, OPConvertType::OPCVT_S16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float4_1_to_int16_scalbn(
                                        static_cast<float4_1>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP4_1, OPConvertType::OPCVT_S8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float4_1_to_int8_scalbn(
                                        static_cast<float4_1>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP4_1, OPConvertType::OPCVT_FP64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(float4_1_to_float64(
                                        static_cast<float4_1>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP4_1, OPConvertType::OPCVT_FP32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(float4_1_to_float32(
                                        static_cast<float4_1>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP4_1, OPConvertType::OPCVT_FP16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(float4_1_to_float16(
                                        static_cast<float4_1>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP4_1, OPConvertType::OPCVT_FP8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(float4_1_to_float8(
                                        static_cast<float4_1>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP4_1, OPConvertType::OPCVT_FP8_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(float4_1_to_float8_1(
                                        static_cast<float4_1>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP4_1, OPConvertType::OPCVT_FP4_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {return data;};
    funcMap[{OPConvertType::OPCVT_FP4_1, OPConvertType::OPCVT_FP4}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(float4_1_to_float4(
                                        static_cast<float4_1>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP4_1, OPConvertType::OPCVT_BF16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(float4_1_to_bfloat16(
                                        static_cast<float4_1>(data), &status));
                                };

    // OPConvertType::OPCVT_FP4 -> other
    funcMap[{OPConvertType::OPCVT_FP4, OPConvertType::OPCVT_U64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float4_to_uint64_scalbn(
                                        static_cast<float4>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP4, OPConvertType::OPCVT_U32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float4_to_uint32_scalbn(
                                        static_cast<float4>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP4, OPConvertType::OPCVT_U16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float4_to_uint16_scalbn(
                                        static_cast<float4>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP4, OPConvertType::OPCVT_U8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float4_to_uint8_scalbn(
                                        static_cast<float4>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP4, OPConvertType::OPCVT_S64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float4_to_int64_scalbn(
                                        static_cast<float4>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP4, OPConvertType::OPCVT_S32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float4_to_int32_scalbn(
                                        static_cast<float4>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP4, OPConvertType::OPCVT_S16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float4_to_int16_scalbn(
                                        static_cast<float4>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP4, OPConvertType::OPCVT_S8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return float4_to_int8_scalbn(
                                        static_cast<float4>(data), status.float_rounding_mode, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_FP4, OPConvertType::OPCVT_FP64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float4_to_float64(static_cast<float4>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP4, OPConvertType::OPCVT_FP32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float4_to_float32(static_cast<float4>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP4, OPConvertType::OPCVT_FP16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float4_to_float16(static_cast<float4>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP4, OPConvertType::OPCVT_FP8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float4_to_float8(static_cast<float4>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP4, OPConvertType::OPCVT_FP8_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float4_to_float8_1(static_cast<float4>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP4, OPConvertType::OPCVT_FP4}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {return data;};
    funcMap[{OPConvertType::OPCVT_FP4, OPConvertType::OPCVT_FP4_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float4_to_float4_1(static_cast<float4>(data), &status));
                                };
    funcMap[{OPConvertType::OPCVT_FP4, OPConvertType::OPCVT_BF16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return static_cast<uint64_t>(
                                        float4_to_bfloat16(static_cast<float4>(data), &status));
                                };
}

static void InitConvertMapInt(std::map<std::pair<OPConvertType, OPConvertType>,
                              std::function<uint64_t(uint64_t, FloatRoundMode)>> &funcMap)
{
    funcMap[{OPConvertType::OPCVT_U64, OPConvertType::OPCVT_U64}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return data;
                                };
    funcMap[{OPConvertType::OPCVT_U64, OPConvertType::OPCVT_U32}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<uint32_t>(data)) & MASK_BIT32;
                                };
    funcMap[{OPConvertType::OPCVT_U64, OPConvertType::OPCVT_U16}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<uint16_t>(data)) & MASK_BIT16;
                                };
    funcMap[{OPConvertType::OPCVT_U64, OPConvertType::OPCVT_U8}]   = [&](uint64_t data, FloatRoundMode) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<uint8_t>(data)) & MASK_BIT8;
                                };
    funcMap[{OPConvertType::OPCVT_U64, OPConvertType::OPCVT_S64}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<int64_t>(data));
                                };
    funcMap[{OPConvertType::OPCVT_U64, OPConvertType::OPCVT_S32}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<int32_t>(data)) & MASK_BIT32;
                                };
    funcMap[{OPConvertType::OPCVT_U64, OPConvertType::OPCVT_S16}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<int16_t>(data)) & MASK_BIT16;
                                };
    funcMap[{OPConvertType::OPCVT_U64, OPConvertType::OPCVT_S8}]   = [&](uint64_t data, FloatRoundMode) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<int8_t>(data)) & MASK_BIT8;
                                };
    funcMap[{OPConvertType::OPCVT_U64, OPConvertType::OPCVT_FP64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float64_scalbn(data, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U64, OPConvertType::OPCVT_FP32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float32_scalbn(data, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U64, OPConvertType::OPCVT_FP16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float16_scalbn(data, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U64, OPConvertType::OPCVT_FP4}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float4_scalbn(data, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U64, OPConvertType::OPCVT_FP4_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float4_1_scalbn(data, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U64, OPConvertType::OPCVT_FP8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float8_scalbn(data, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U64, OPConvertType::OPCVT_FP8_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float8_1_scalbn(data, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U64, OPConvertType::OPCVT_BF16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_bfloat16_scalbn(data, 0, &status);
                                };

    // OPConvertType::OPCVT_S64 -> other
    funcMap[{OPConvertType::OPCVT_S64, OPConvertType::OPCVT_U64}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(data);
                                };
    funcMap[{OPConvertType::OPCVT_S64, OPConvertType::OPCVT_U32}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<uint32_t>(data)) & MASK_BIT32;
                                };
    funcMap[{OPConvertType::OPCVT_S64, OPConvertType::OPCVT_U16}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<uint16_t>(data)) & MASK_BIT16;
                                };
    funcMap[{OPConvertType::OPCVT_S64, OPConvertType::OPCVT_U8}]   = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<uint8_t>(data)) & MASK_BIT8;
                                };
    funcMap[{OPConvertType::OPCVT_S64, OPConvertType::OPCVT_S64}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return data;
                                };
    funcMap[{OPConvertType::OPCVT_S64, OPConvertType::OPCVT_S32}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<int32_t>(data)) & MASK_BIT32;
                                };
    funcMap[{OPConvertType::OPCVT_S64, OPConvertType::OPCVT_S16}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<int16_t>(data)) & MASK_BIT16;
                                };
    funcMap[{OPConvertType::OPCVT_S64, OPConvertType::OPCVT_S8}]   = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<int8_t>(data)) & MASK_BIT8;
                                };
    funcMap[{OPConvertType::OPCVT_S64, OPConvertType::OPCVT_FP64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float64_scalbn(data, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S64, OPConvertType::OPCVT_FP32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float32_scalbn(data, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S64, OPConvertType::OPCVT_FP16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float16_scalbn(data, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S64, OPConvertType::OPCVT_FP4}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float4_scalbn(data, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S64, OPConvertType::OPCVT_FP4_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float4_1_scalbn(data, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S64, OPConvertType::OPCVT_FP8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float8_scalbn(data, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S64, OPConvertType::OPCVT_FP8_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float8_1_scalbn(data, 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S64, OPConvertType::OPCVT_BF16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_bfloat16_scalbn(data, 0, &status);
                                };

    // OPConvertType::OPCVT_S32 -> other
    funcMap[{OPConvertType::OPCVT_S32, OPConvertType::OPCVT_U64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(data)));
                                };
    funcMap[{OPConvertType::OPCVT_S32, OPConvertType::OPCVT_U32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<uint32_t>(
                                        static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(data)))));
                                };
    funcMap[{OPConvertType::OPCVT_S32, OPConvertType::OPCVT_U16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<uint16_t>(
                                        static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(data)))));
                                };
    funcMap[{OPConvertType::OPCVT_S32, OPConvertType::OPCVT_U8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<uint8_t>(
                                        static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(data)))));
                                };
    funcMap[{OPConvertType::OPCVT_S32, OPConvertType::OPCVT_S64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(data)));
                                };
    funcMap[{OPConvertType::OPCVT_S32, OPConvertType::OPCVT_S32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return data;
                                };
    funcMap[{OPConvertType::OPCVT_S32, OPConvertType::OPCVT_S16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<int16_t>(
                                        static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(data)))));
                                };
    funcMap[{OPConvertType::OPCVT_S32, OPConvertType::OPCVT_S8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<int8_t>(
                                        static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(data)))));
                                };
    funcMap[{OPConvertType::OPCVT_S32, OPConvertType::OPCVT_FP64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float64_scalbn(
                                        static_cast<int64_t>(static_cast<int32_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S32, OPConvertType::OPCVT_FP32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float32_scalbn(
                                        static_cast<int64_t>(static_cast<int32_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S32, OPConvertType::OPCVT_FP16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float16_scalbn(
                                        static_cast<int64_t>(static_cast<int32_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S32, OPConvertType::OPCVT_FP4}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float4_scalbn(
                                        static_cast<int64_t>(static_cast<int32_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S32, OPConvertType::OPCVT_FP4_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float4_1_scalbn(
                                        static_cast<int64_t>(static_cast<int32_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S32, OPConvertType::OPCVT_FP8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float8_scalbn(
                                        static_cast<int64_t>(static_cast<int32_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S32, OPConvertType::OPCVT_FP8_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float8_1_scalbn(
                                        static_cast<int64_t>(static_cast<int32_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S32, OPConvertType::OPCVT_BF16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_bfloat16_scalbn(
                                        static_cast<int64_t>(static_cast<int32_t>(data)), 0, &status);
                                };

    // OPConvertType::OPCVT_U32 -> other
    funcMap[{OPConvertType::OPCVT_U32, OPConvertType::OPCVT_U64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<uint32_t>(data));
                                };
    funcMap[{OPConvertType::OPCVT_U32, OPConvertType::OPCVT_U32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return data;
                                };
    funcMap[{OPConvertType::OPCVT_U32, OPConvertType::OPCVT_U16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(
                                        static_cast<uint16_t>(static_cast<uint64_t>(static_cast<uint32_t>(data))));
                                };
    funcMap[{OPConvertType::OPCVT_U32, OPConvertType::OPCVT_U8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(
                                        static_cast<uint8_t>(static_cast<uint64_t>(static_cast<uint32_t>(data))));
                                };
    funcMap[{OPConvertType::OPCVT_U32, OPConvertType::OPCVT_S64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(
                                        static_cast<int64_t>(static_cast<uint64_t>(static_cast<uint32_t>(data))));
                                };
    funcMap[{OPConvertType::OPCVT_U32, OPConvertType::OPCVT_S32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(
                                        static_cast<int32_t>(static_cast<uint64_t>(static_cast<uint32_t>(data))));
                                };
    funcMap[{OPConvertType::OPCVT_U32, OPConvertType::OPCVT_S16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(
                                        static_cast<int16_t>(static_cast<uint64_t>(static_cast<uint32_t>(data))));
                                };
    funcMap[{OPConvertType::OPCVT_U32, OPConvertType::OPCVT_S8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(
                                        static_cast<int8_t>(static_cast<uint64_t>(static_cast<uint32_t>(data))));
                                };
    funcMap[{OPConvertType::OPCVT_U32, OPConvertType::OPCVT_FP64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float64_scalbn(
                                        static_cast<uint64_t>(static_cast<uint32_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U32, OPConvertType::OPCVT_FP32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float32_scalbn(
                                        static_cast<uint64_t>(static_cast<uint32_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U32, OPConvertType::OPCVT_FP16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float16_scalbn(
                                        static_cast<uint64_t>(static_cast<uint32_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U32, OPConvertType::OPCVT_FP4}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float4_scalbn(
                                        static_cast<uint64_t>(static_cast<uint32_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U32, OPConvertType::OPCVT_FP4_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float4_1_scalbn(
                                        static_cast<uint64_t>(static_cast<uint32_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U32, OPConvertType::OPCVT_FP8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float8_scalbn(
                                        static_cast<uint64_t>(static_cast<uint32_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U32, OPConvertType::OPCVT_FP8_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float8_1_scalbn(
                                        static_cast<uint64_t>(static_cast<uint32_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U32, OPConvertType::OPCVT_BF16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_bfloat16_scalbn(
                                        static_cast<uint64_t>(static_cast<uint32_t>(data)), 0, &status);
                                };

    // OPConvertType::OPCVT_U16 -> other
    funcMap[{OPConvertType::OPCVT_U16, OPConvertType::OPCVT_U64}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<uint16_t>(data));
                                };
    funcMap[{OPConvertType::OPCVT_U16, OPConvertType::OPCVT_U32}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<uint32_t>(
                                        static_cast<uint64_t>(static_cast<uint16_t>(data))));
                                };
    funcMap[{OPConvertType::OPCVT_U16, OPConvertType::OPCVT_U16}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return data;
                                };
    funcMap[{OPConvertType::OPCVT_U16, OPConvertType::OPCVT_U8}]   = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(
                                        static_cast<uint8_t>(static_cast<uint64_t>(static_cast<uint16_t>(data))));
                                };
    funcMap[{OPConvertType::OPCVT_U16, OPConvertType::OPCVT_S64}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(
                                        static_cast<int64_t>(static_cast<uint64_t>(static_cast<uint16_t>(data))));
                                };
    funcMap[{OPConvertType::OPCVT_U16, OPConvertType::OPCVT_S32}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(
                                        static_cast<int32_t>(static_cast<uint64_t>(static_cast<uint16_t>(data))));
                                };
    funcMap[{OPConvertType::OPCVT_U16, OPConvertType::OPCVT_S16}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(
                                        static_cast<int16_t>(static_cast<uint64_t>(static_cast<uint16_t>(data))));
                                };
    funcMap[{OPConvertType::OPCVT_U16, OPConvertType::OPCVT_S8}]   = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(
                                        static_cast<int8_t>(static_cast<uint64_t>(static_cast<uint16_t>(data))));
                                };
    funcMap[{OPConvertType::OPCVT_U16, OPConvertType::OPCVT_FP64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float64_scalbn(
                                        static_cast<uint64_t>(static_cast<uint16_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U16, OPConvertType::OPCVT_FP32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float32_scalbn(
                                        static_cast<uint64_t>(static_cast<uint16_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U16, OPConvertType::OPCVT_FP16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float16_scalbn(
                                        static_cast<uint64_t>(static_cast<uint16_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U16, OPConvertType::OPCVT_FP4}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float4_scalbn(
                                        static_cast<uint64_t>(static_cast<uint16_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U16, OPConvertType::OPCVT_FP4_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float4_1_scalbn(
                                        static_cast<uint64_t>(static_cast<uint16_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U16, OPConvertType::OPCVT_FP8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float8_scalbn(
                                        static_cast<uint64_t>(static_cast<uint16_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U16, OPConvertType::OPCVT_FP8_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float8_1_scalbn(
                                        static_cast<uint64_t>(static_cast<uint16_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U16, OPConvertType::OPCVT_BF16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_bfloat16_scalbn(
                                        static_cast<uint64_t>(static_cast<uint16_t>(data)), 0, &status);
                                };


    // OPConvertType::OPCVT_U8 -> other
    funcMap[{OPConvertType::OPCVT_U8, OPConvertType::OPCVT_U64}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<uint8_t>(data));
                                };
    funcMap[{OPConvertType::OPCVT_U8, OPConvertType::OPCVT_U32}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(
                                        static_cast<uint32_t>(static_cast<uint64_t>(static_cast<uint8_t>(data))));
                                };
    funcMap[{OPConvertType::OPCVT_U8, OPConvertType::OPCVT_U16}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return data;
                                };
    funcMap[{OPConvertType::OPCVT_U8, OPConvertType::OPCVT_U8}]   = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(
                                        static_cast<uint8_t>(static_cast<uint64_t>(static_cast<uint8_t>(data))));
                                };
    funcMap[{OPConvertType::OPCVT_U8, OPConvertType::OPCVT_S64}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(
                                        static_cast<int64_t>(static_cast<uint64_t>(static_cast<uint8_t>(data))));
                                };
    funcMap[{OPConvertType::OPCVT_U8, OPConvertType::OPCVT_S32}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(
                                        static_cast<int32_t>(static_cast<uint64_t>(static_cast<uint8_t>(data))));
                                };
    funcMap[{OPConvertType::OPCVT_U8, OPConvertType::OPCVT_S16}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(
                                        static_cast<int16_t>(static_cast<uint64_t>(static_cast<uint8_t>(data))));
                                };
    funcMap[{OPConvertType::OPCVT_U8, OPConvertType::OPCVT_S8}]   = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(
                                        static_cast<int8_t>(static_cast<uint64_t>(static_cast<uint8_t>(data))));
                                };
    funcMap[{OPConvertType::OPCVT_U8, OPConvertType::OPCVT_FP64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float64_scalbn(
                                        static_cast<uint64_t>(static_cast<uint8_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U8, OPConvertType::OPCVT_FP32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float32_scalbn(
                                        static_cast<uint64_t>(static_cast<uint8_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U8, OPConvertType::OPCVT_FP16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float16_scalbn(
                                        static_cast<uint64_t>(static_cast<uint8_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U8, OPConvertType::OPCVT_FP4}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float4_scalbn(
                                        static_cast<uint64_t>(static_cast<uint8_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U8, OPConvertType::OPCVT_FP4_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float4_1_scalbn(
                                        static_cast<uint64_t>(static_cast<uint8_t>(data)), 0, &status);
                                    };
    funcMap[{OPConvertType::OPCVT_U8, OPConvertType::OPCVT_FP8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float8_scalbn(
                                        static_cast<uint64_t>(static_cast<uint8_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_U8, OPConvertType::OPCVT_FP8_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_float8_1_scalbn(
                                        static_cast<uint64_t>(static_cast<uint8_t>(data)), 0, &status);
                                    };
    funcMap[{OPConvertType::OPCVT_U8, OPConvertType::OPCVT_BF16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return uint64_to_bfloat16_scalbn(
                                        static_cast<uint64_t>(static_cast<uint8_t>(data)), 0, &status);
                                };


    // OPConvertType::OPCVT_S16 -> other
    funcMap[{OPConvertType::OPCVT_S16, OPConvertType::OPCVT_U64}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(data)));
                                };
    funcMap[{OPConvertType::OPCVT_S16, OPConvertType::OPCVT_U32}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<uint32_t>(
                                        static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(data)))));
                                };
    funcMap[{OPConvertType::OPCVT_S16, OPConvertType::OPCVT_U16}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<uint16_t>(
                                        static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(data)))));
                                };
    funcMap[{OPConvertType::OPCVT_S16, OPConvertType::OPCVT_U8}]   = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<uint8_t>(
                                        static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(data)))));
                                };
    funcMap[{OPConvertType::OPCVT_S16, OPConvertType::OPCVT_S64}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(
                                        static_cast<int32_t>(static_cast<int16_t>(data)));
                                };
    funcMap[{OPConvertType::OPCVT_S16, OPConvertType::OPCVT_S32}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<int32_t>(
                                        static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(data)))));
                                };
    funcMap[{OPConvertType::OPCVT_S16, OPConvertType::OPCVT_S16}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return data;
                                };
    funcMap[{OPConvertType::OPCVT_S16, OPConvertType::OPCVT_S8}]   = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<int8_t>(
                                        static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(data)))));
                                };
    funcMap[{OPConvertType::OPCVT_S16, OPConvertType::OPCVT_FP64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float64_scalbn(static_cast<int64_t>(
                                        static_cast<int16_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S16, OPConvertType::OPCVT_FP32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float32_scalbn(
                                        static_cast<int64_t>(static_cast<int16_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S16, OPConvertType::OPCVT_FP16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float16_scalbn(static_cast<int64_t>(
                                        static_cast<int16_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S16, OPConvertType::OPCVT_FP4}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float4_scalbn(static_cast<int64_t>(
                                        static_cast<int16_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S16, OPConvertType::OPCVT_FP4_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float4_1_scalbn(static_cast<int64_t>(
                                        static_cast<int16_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S16, OPConvertType::OPCVT_FP8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float8_scalbn(static_cast<int64_t>(
                                        static_cast<int16_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S16, OPConvertType::OPCVT_FP8_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float8_1_scalbn(static_cast<int64_t>(
                                        static_cast<int16_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S16, OPConvertType::OPCVT_BF16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_bfloat16_scalbn(static_cast<int64_t>(
                                        static_cast<int16_t>(data)), 0, &status);
                                };

    // OPConvertType::OPCVT_S8 -> other
    funcMap[{OPConvertType::OPCVT_S8, OPConvertType::OPCVT_U64}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int8_t>(data)));
                                };
    funcMap[{OPConvertType::OPCVT_S8, OPConvertType::OPCVT_U32}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<uint32_t>(
                                        static_cast<uint64_t>(static_cast<int64_t>(static_cast<int8_t>(data)))));
                                };
    funcMap[{OPConvertType::OPCVT_S8, OPConvertType::OPCVT_U16}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<uint16_t>(
                                        static_cast<uint64_t>(static_cast<int64_t>(static_cast<int8_t>(data)))));
                                };
    funcMap[{OPConvertType::OPCVT_S8, OPConvertType::OPCVT_U8}]   = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<uint8_t>(
                                        static_cast<uint64_t>(static_cast<int64_t>(static_cast<int8_t>(data)))));
                                };
    funcMap[{OPConvertType::OPCVT_S8, OPConvertType::OPCVT_S64}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<int64_t>(
                                        static_cast<int8_t>(data)));
                                };
    funcMap[{OPConvertType::OPCVT_S8, OPConvertType::OPCVT_S32}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<int32_t>(
                                        static_cast<uint64_t>(static_cast<int64_t>(static_cast<int8_t>(data)))));
                                };
    funcMap[{OPConvertType::OPCVT_S8, OPConvertType::OPCVT_S16}]  = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return static_cast<uint64_t>(static_cast<int16_t>(
                                        static_cast<uint64_t>(static_cast<int64_t>(static_cast<int8_t>(data)))));
                                };
    funcMap[{OPConvertType::OPCVT_S8, OPConvertType::OPCVT_S8}]   = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    return data;
                                };
    funcMap[{OPConvertType::OPCVT_S8, OPConvertType::OPCVT_FP64}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float64_scalbn(static_cast<int64_t>(
                                        static_cast<int8_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S8, OPConvertType::OPCVT_FP32}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float32_scalbn(static_cast<int64_t>(
                                        static_cast<int8_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S8, OPConvertType::OPCVT_FP16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float16_scalbn(static_cast<int64_t>(
                                        static_cast<int8_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S8, OPConvertType::OPCVT_FP4}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float4_scalbn(static_cast<int64_t>(
                                        static_cast<int8_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S8, OPConvertType::OPCVT_FP4_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float4_1_scalbn(static_cast<int64_t>(
                                        static_cast<int8_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S8, OPConvertType::OPCVT_FP8}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float8_scalbn(static_cast<int64_t>(
                                        static_cast<int8_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S8, OPConvertType::OPCVT_FP8_1}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_float8_1_scalbn(static_cast<int64_t>(
                                        static_cast<int8_t>(data)), 0, &status);
                                };
    funcMap[{OPConvertType::OPCVT_S8, OPConvertType::OPCVT_BF16}] = [&](uint64_t data, FloatRoundMode frm) -> uint64_t {
                                    float_status status;
                                    memset(&status, 0, sizeof(float_status));
                                    status.float_rounding_mode = frm;
                                    return int64_to_bfloat16_scalbn(static_cast<int64_t>(
                                        static_cast<int8_t>(data)), 0, &status);
                                };
}

uint64_t ConvertAggre(uint64_t data, OPConvertType from, OPConvertType to, uint32_t FRM, bool needCheck)
{
    static std::map<std::pair<OPConvertType, OPConvertType>, std::function<uint64_t(uint64_t, FloatRoundMode)>> funcMap;
    static bool convertMapInit = false;

    FloatRoundMode tmp_frm = FRM2FloatRoundMode(FRM);
    if (!convertMapInit) {
        convertMapInit = true;
        InitConvertMapInt(funcMap);
        InitConvertMapFp(funcMap);
    }

    auto it = funcMap.find({from, to});


    if (it != funcMap.end()) {
        return it->second(data, tmp_frm);
    } else if (needCheck) {
        LOG_INFO << std::dec << " from: " << GetOperandConvertType(from) << "  to: " << GetOperandConvertType(to);
        assert(0 && "convert  inst error type!!\r\n");
    }

    return data;
}

uint64_t Fadd(OPConvertType cvt, uint64_t data1, uint64_t data2, FloatRoundMode frm)
{
    float_status status = {frm, 0, floatx80_precision_x, false, false, false, false, false, false, false};
    switch (cvt) {
        case OPConvertType::OPCVT_FP64: {
            return static_cast<uint64_t>(float64_add(static_cast<float64>(data1), static_cast<float64>(data2),
                                                     &status));
        }
        case OPConvertType::OPCVT_FP32: {
            return static_cast<uint64_t>(float32_add(static_cast<float32>(data1), static_cast<float32>(data2),
                                                     &status));
        }
        case OPConvertType::OPCVT_FP16: {
            return static_cast<uint64_t>(float16_add(static_cast<float16>(data1), static_cast<float16>(data2),
                                                     &status));
        }
        case OPConvertType::OPCVT_BF16: {
            return static_cast<uint64_t>(bfloat16_add(static_cast<bfloat16>(data1), static_cast<bfloat16>(data2),
                                                      &status));
        }
        case OPConvertType::OPCVT_FP8: {
            return static_cast<uint64_t>(float8_add(static_cast<float8>(data1), static_cast<float8>(data2), &status));
        }
        case OPConvertType::OPCVT_FP8_1: {
            return static_cast<uint64_t>(float8_1_add(static_cast<float8_1>(data1), static_cast<float8_1>(data2),
                                                      &status));
        }
        default:
            ASSERT(0 && "Error: The data type is not matched.\n") << GetOperandConvertType(cvt);
            return 0;
    }
}

uint64_t Fsub(OPConvertType cvt, uint64_t data1, uint64_t data2, FloatRoundMode frm)
{
    float_status status = {frm, 0, floatx80_precision_x, false, false, false, false, false, false, false};
    switch (cvt) {
        case OPConvertType::OPCVT_FP64: {
            return static_cast<uint64_t>(float64_sub(static_cast<float64>(data1), static_cast<float64>(data2),
                                                     &status));
        }
        case OPConvertType::OPCVT_FP32: {
            return static_cast<uint64_t>(float32_sub(static_cast<float32>(data1), static_cast<float32>(data2),
                                                     &status));
        }
        case OPConvertType::OPCVT_FP16: {
            return static_cast<uint64_t>(float16_sub(static_cast<float16>(data1), static_cast<float16>(data2),
                                                     &status));
        }
        case OPConvertType::OPCVT_BF16: {
            return static_cast<uint64_t>(bfloat16_sub(static_cast<bfloat16>(data1), static_cast<bfloat16>(data2),
                                                      &status));
        }
        case OPConvertType::OPCVT_FP8: {
            return static_cast<uint64_t>(float8_sub(static_cast<float8>(data1), static_cast<float8>(data2),
                                                    &status));
        }
        case OPConvertType::OPCVT_FP8_1: {
            return static_cast<uint64_t>(float8_1_sub(static_cast<float8_1>(data1), static_cast<float8_1>(data2),
                                                      &status));
        }
        default:
            ASSERT(0 && "Error: The data type is not matched.\n") << GetOperandConvertType(cvt);
            return 0;
    }
}

uint64_t Fmul(OPConvertType cvt, uint64_t data1, uint64_t data2, FloatRoundMode frm)
{
    float_status status = {frm, 0, floatx80_precision_x, false, false, false, false, false, false, false};
    switch (cvt) {
        case OPConvertType::OPCVT_FP64: {
            return static_cast<uint64_t>(float64_mul(static_cast<float64>(data1), static_cast<float64>(data2),
                                                     &status));
        }
        case OPConvertType::OPCVT_FP32: {
            return static_cast<uint64_t>(float32_mul(static_cast<float32>(data1), static_cast<float32>(data2),
                                                     &status));
        }
        case OPConvertType::OPCVT_FP16: {
            return static_cast<uint64_t>(float16_mul(static_cast<float16>(data1), static_cast<float16>(data2),
                                                     &status));
        }
        case OPConvertType::OPCVT_BF16: {
            return static_cast<uint64_t>(bfloat16_mul(static_cast<bfloat16>(data1), static_cast<bfloat16>(data2),
                                                      &status));
        }
        case OPConvertType::OPCVT_FP8: {
            return static_cast<uint64_t>(float8_mul(static_cast<float8>(data1), static_cast<float8>(data2),
                                                    &status));
        }
        case OPConvertType::OPCVT_FP8_1: {
            return static_cast<uint64_t>(float8_1_mul(static_cast<float8_1>(data1), static_cast<float8_1>(data2),
                                                      &status));
        }
        default:
            ASSERT(0 && "Error: The data type is not matched.\n") << GetOperandConvertType(cvt);
            return 0;
    }
}

uint64_t Fdiv(OPConvertType cvt, uint64_t data1, uint64_t data2, FloatRoundMode frm)
{
    float_status status = {frm, 0, floatx80_precision_x, false, false, false, false, false, false, false};
    switch (cvt) {
        case OPConvertType::OPCVT_FP64: {
            return static_cast<uint64_t>(float64_div(static_cast<float64>(data1), static_cast<float64>(data2),
                                                     &status));
        }
        case OPConvertType::OPCVT_FP32: {
            return static_cast<uint64_t>(float32_div(static_cast<float32>(data1), static_cast<float32>(data2),
                                                     &status));
        }
        case OPConvertType::OPCVT_FP16: {
            return static_cast<uint64_t>(float16_div(static_cast<float16>(data1), static_cast<float16>(data2),
                                                     &status));
        }
        case OPConvertType::OPCVT_BF16: {
            return static_cast<uint64_t>(bfloat16_div(static_cast<bfloat16>(data1), static_cast<bfloat16>(data2),
                                                      &status));
        }
        case OPConvertType::OPCVT_FP8: {
            return static_cast<uint64_t>(float8_div(static_cast<float8>(data1), static_cast<float8>(data2),
                                                    &status));
        }
        case OPConvertType::OPCVT_FP8_1: {
            return static_cast<uint64_t>(float8_1_div(static_cast<float8_1>(data1), static_cast<float8_1>(data2),
                                                      &status));
        }
        default:
            ASSERT(0 && "Error: The data type is not matched.\n") << GetOperandConvertType(cvt);
            return 0;
    }
}

uint64_t Fmadd(OPConvertType cvt, uint64_t data1, uint64_t data2, uint64_t data3, FloatRoundMode frm)
{
    float_status status = {frm, 0, floatx80_precision_x, false, false, false, false, false, false, false};
    switch (cvt) {
        case OPConvertType::OPCVT_FP64: {
            return static_cast<uint64_t>(float64_muladd(static_cast<float64>(data1), static_cast<float64>(data2),
                                                        static_cast<float64>(data3), 0, &status));
        }
        case OPConvertType::OPCVT_FP32: {
            return static_cast<uint64_t>(float32_muladd(static_cast<float32>(data1), static_cast<float32>(data2),
                                                        static_cast<float32>(data3), 0, &status));
        }
        case OPConvertType::OPCVT_FP16: {
            return static_cast<uint64_t>(float16_muladd(static_cast<float16>(data1), static_cast<float16>(data2),
                                                        static_cast<float16>(data3), 0, &status));
        }
        case OPConvertType::OPCVT_BF16: {
            return static_cast<uint64_t>(bfloat16_muladd(static_cast<bfloat16>(data1), static_cast<bfloat16>(data2),
                                                         static_cast<bfloat16>(data3), 0, &status));
        }
        case OPConvertType::OPCVT_FP8: {
            return static_cast<uint64_t>(float8_muladd(static_cast<float8>(data1), static_cast<float8>(data2),
                                                       static_cast<float8>(data3), 0, &status));
        }
        case OPConvertType::OPCVT_FP8_1: {
            return static_cast<uint64_t>(float8_1_muladd(static_cast<float8_1>(data1), static_cast<float8_1>(data2),
                                                         static_cast<float8_1>(data3), 0, &status));
        }
        default:
            ASSERT(0 && "Error: The data type is not matched.\n") << GetOperandConvertType(cvt);
            return 0;
    }
}

uint64_t Fmsub(OPConvertType cvt, uint64_t data1, uint64_t data2, uint64_t data3, FloatRoundMode frm)
{
    float_status status = {frm, 0, floatx80_precision_x, false, false, false, false, false, false, false};
    switch (cvt) {
        case OPConvertType::OPCVT_FP64: {
            return static_cast<uint64_t>(float64_muladd(static_cast<float64>(data1), static_cast<float64>(data2),
                                                        static_cast<float64>(data3), float_muladd_negate_c, &status));
        }
        case OPConvertType::OPCVT_FP32: {
            return static_cast<uint64_t>(float32_muladd(static_cast<float32>(data1), static_cast<float32>(data2),
                                                        static_cast<float32>(data3), float_muladd_negate_c, &status));
        }
        case OPConvertType::OPCVT_FP16: {
            return static_cast<uint64_t>(float16_muladd(static_cast<float16>(data1), static_cast<float16>(data2),
                                                        static_cast<float16>(data3), float_muladd_negate_c, &status));
        }
        case OPConvertType::OPCVT_BF16: {
            return static_cast<uint64_t>(bfloat16_muladd(static_cast<bfloat16>(data1), static_cast<bfloat16>(data2),
                                                         static_cast<bfloat16>(data3), float_muladd_negate_c, &status));
        }
        case OPConvertType::OPCVT_FP8: {
            return static_cast<uint64_t>(float8_muladd(static_cast<float8>(data1), static_cast<float8>(data2),
                                                       static_cast<float8>(data3), float_muladd_negate_c, &status));
        }
        case OPConvertType::OPCVT_FP8_1: {
            return static_cast<uint64_t>(float8_1_muladd(static_cast<float8_1>(data1), static_cast<float8_1>(data2),
                                                         static_cast<float8_1>(data3), float_muladd_negate_c, &status));
        }
        default:
            ASSERT(0 && "Error: The data type is not matched.\n") << GetOperandConvertType(cvt);
            return 0;
    }
}

uint64_t Fnmadd(OPConvertType cvt, uint64_t data1, uint64_t data2, uint64_t data3, FloatRoundMode frm)
{
    float_status status = {frm, 0, floatx80_precision_x, false, false, false, false, false, false, false};
    switch (cvt) {
        case OPConvertType::OPCVT_FP64: {
            return static_cast<uint64_t>(float64_muladd(static_cast<float64>(data1), static_cast<float64>(data2),
                                                        static_cast<float64>(data3),
                                                        float_muladd_negate_c | float_muladd_negate_product, &status));
        }
        case OPConvertType::OPCVT_FP32: {
            return static_cast<uint64_t>(float32_muladd(static_cast<float32>(data1), static_cast<float32>(data2),
                                                        static_cast<float32>(data3),
                                                        float_muladd_negate_c | float_muladd_negate_product, &status));
        }
        case OPConvertType::OPCVT_FP16: {
            return static_cast<uint64_t>(float16_muladd(static_cast<float16>(data1), static_cast<float16>(data2),
                                                        static_cast<float16>(data3),
                                                        float_muladd_negate_c | float_muladd_negate_product, &status));
        }
        case OPConvertType::OPCVT_BF16: {
            return static_cast<uint64_t>(bfloat16_muladd(static_cast<bfloat16>(data1), static_cast<bfloat16>(data2),
                                                         static_cast<bfloat16>(data3),
                                                         float_muladd_negate_c | float_muladd_negate_product, &status));
        }
        case OPConvertType::OPCVT_FP8: {
            return static_cast<uint64_t>(float8_muladd(static_cast<float8>(data1), static_cast<float8>(data2),
                                                       static_cast<float8>(data3),
                                                       float_muladd_negate_c | float_muladd_negate_product, &status));
        }
        case OPConvertType::OPCVT_FP8_1: {
            return static_cast<uint64_t>(float8_1_muladd(static_cast<float8_1>(data1), static_cast<float8_1>(data2),
                                                         static_cast<float8_1>(data3),
                                                         float_muladd_negate_c | float_muladd_negate_product, &status));
        }
        default:
            ASSERT(0 && "Error: The data type is not matched.\n") << GetOperandConvertType(cvt);
            return 0;
    }
}

uint64_t Fnmsub(OPConvertType cvt, uint64_t data1, uint64_t data2, uint64_t data3, FloatRoundMode frm)
{
    float_status status = {frm, 0, floatx80_precision_x, false, false, false, false, false, false, false};
    switch (cvt) {
        case OPConvertType::OPCVT_FP64: {
            return static_cast<uint64_t>(float64_muladd(static_cast<float64>(data1), static_cast<float64>(data2),
                                                        static_cast<float64>(data3),
                                                        float_muladd_negate_product, &status));
        }
        case OPConvertType::OPCVT_FP32: {
            return static_cast<uint64_t>(float32_muladd(static_cast<float32>(data1), static_cast<float32>(data2),
                                                        static_cast<float32>(data3),
                                                        float_muladd_negate_product, &status));
        }
        case OPConvertType::OPCVT_FP16: {
            return static_cast<uint64_t>(float16_muladd(static_cast<float16>(data1), static_cast<float16>(data2),
                                                        static_cast<float16>(data3),
                                                        float_muladd_negate_product, &status));
        }
        case OPConvertType::OPCVT_BF16: {
            return static_cast<uint64_t>(bfloat16_muladd(static_cast<bfloat16>(data1), static_cast<bfloat16>(data2),
                                                         static_cast<bfloat16>(data3),
                                                         float_muladd_negate_product, &status));
        }
        case OPConvertType::OPCVT_FP8: {
            return static_cast<uint64_t>(float8_muladd(static_cast<float8>(data1), static_cast<float8>(data2),
                                                       static_cast<float8>(data3),
                                                       float_muladd_negate_product, &status));
        }
        case OPConvertType::OPCVT_FP8_1: {
            return static_cast<uint64_t>(float8_1_muladd(static_cast<float8_1>(data1), static_cast<float8_1>(data2),
                                                         static_cast<float8_1>(data3),
                                                         float_muladd_negate_product, &status));
        }
        default:
            ASSERT(0 && "Error: The data type is not matched.\n") << GetOperandConvertType(cvt);
            return 0;
    }
}

uint64_t Fabs(OPConvertType cvt, uint64_t data)
{
    switch (cvt) {
        case OPConvertType::OPCVT_FP64: {
            return static_cast<uint64_t>(float64_abs(static_cast<float64>(data)));
        }
        case OPConvertType::OPCVT_FP32: {
            return static_cast<uint64_t>(float32_abs(static_cast<float32>(data)));
        }
        case OPConvertType::OPCVT_FP16: {
            return static_cast<uint64_t>(float16_abs(static_cast<float16>(data)));
        }
        case OPConvertType::OPCVT_BF16: {
            return static_cast<uint64_t>(bfloat16_abs(static_cast<bfloat16>(data)));
        }
        case OPConvertType::OPCVT_FP8: {
            return static_cast<uint64_t>(float8_abs(static_cast<float8>(data)));
        }
        case OPConvertType::OPCVT_FP8_1: {
            return static_cast<uint64_t>(float8_1_abs(static_cast<float8_1>(data)));
        }
        default:
            ASSERT(0 && "Error: The data type is not matched.\n") << GetOperandConvertType(cvt);
            return 0;
    }
}

uint64_t Fsqrt(OPConvertType cvt, uint64_t data)
{
    float_status status = {float_round_nearest_even, 0, floatx80_precision_x, false, false, false, false, false, false,
                           false};
    switch (cvt) {
        case OPConvertType::OPCVT_FP64: {
            return static_cast<uint64_t>(float64_sqrt(static_cast<float64>(data), &status));
        }
        case OPConvertType::OPCVT_FP32: {
            return static_cast<uint64_t>(float32_sqrt(static_cast<float32>(data), &status));
        }
        case OPConvertType::OPCVT_FP16: {
            return static_cast<uint64_t>(float16_sqrt(static_cast<float16>(data), &status));
        }
        case OPConvertType::OPCVT_BF16: {
            return static_cast<uint64_t>(bfloat16_sqrt(static_cast<bfloat16>(data), &status));
        }
        case OPConvertType::OPCVT_FP8: {
            return static_cast<uint64_t>(float8_sqrt(static_cast<float8>(data), &status));
        }
        case OPConvertType::OPCVT_FP8_1: {
            return static_cast<uint64_t>(float8_1_sqrt(static_cast<float8_1>(data), &status));
        }
        default:
            ASSERT(0 && "Error: The data type is not matched.\n") << GetOperandConvertType(cvt);
            return 0;
    }
}

uint64_t Frecip(OPConvertType cvt, uint64_t data)
{
    if(IsNaN(cvt, data)) {
        ASSERT(0 && "instruction triggers an invalid operation NV exception.\n");
    }
    switch (cvt) {
        case OPConvertType::OPCVT_FP64: {
            double outcome = Frecip<double>(data);
            uint64_t* u64p = reinterpret_cast<uint64_t*>(&outcome);
            return *u64p;
        }
        case OPConvertType::OPCVT_FP32: {
            float outcome = Frecip<float>(data);
            uint32_t* u32p = reinterpret_cast<uint32_t*>(&outcome);
            return static_cast<uint64_t>(*u32p);
        }
        default:
            ASSERT(0 && "Error: The data type is not matched.\n") << GetOperandConvertType(cvt);
            return 0;
    }
}

uint64_t Fexp(OPConvertType cvt, uint64_t data, FloatRoundMode frm)
{
    switch (cvt) {
        case OPConvertType::OPCVT_FP64: {
            double outcome = Fexp<double>(data);
            uint64_t* u64p = reinterpret_cast<uint64_t*>(&outcome);
            return *u64p;
        }
        case OPConvertType::OPCVT_FP32: {
            float outcome = Fexp<float>(data);
            uint32_t* u32p = reinterpret_cast<uint32_t*>(&outcome);
            return static_cast<uint64_t>(*u32p);
        }
        case OPConvertType::OPCVT_FP16: {
            data = Calculate::ConvertAggre(data, OPConvertType::OPCVT_FP16, OPConvertType::OPCVT_FP32, frm);
            float outcome = Fexp<float>(data);
            uint32_t outcomeBits = 0;
            memcpy(&outcomeBits, &outcome, sizeof(float));
            data = Calculate::ConvertAggre(outcomeBits, OPConvertType::OPCVT_FP32, OPConvertType::OPCVT_FP16, frm);
            return data;
        }
        case OPConvertType::OPCVT_FP8: {
            data = Calculate::ConvertAggre(data, OPConvertType::OPCVT_FP8, OPConvertType::OPCVT_FP32, frm);
            float outcome = Fexp<float>(data);
            uint32_t outcomeBits = 0;
            memcpy(&outcomeBits, &outcome, sizeof(float));
            data = Calculate::ConvertAggre(outcomeBits, OPConvertType::OPCVT_FP32, OPConvertType::OPCVT_FP8, frm);
            return data;
        }
        case OPConvertType::OPCVT_BF16: {
            data = Calculate::ConvertAggre(data, OPConvertType::OPCVT_BF16, OPConvertType::OPCVT_FP32, frm);
            float outcome = Fexp<float>(data);
            uint32_t outcomeBits = 0;
            memcpy(&outcomeBits, &outcome, sizeof(float));
            data = Calculate::ConvertAggre(outcomeBits, OPConvertType::OPCVT_FP32, OPConvertType::OPCVT_BF16, frm);
            return data;
        }
        default:
            ASSERT(0 && "Error: The data type is not matched.\n") << GetOperandConvertType(cvt);
            return 0;
    }
}

uint64_t Fclass(OPConvertType cvt,uint64_t data)
{
    uint64_t sign = 0;
    uint64_t exponent = 0;
    uint64_t mantissa = 0;
    if(cvt == OPConvertType::OPCVT_FP64) {
        sign = ((data >> 63) & 0x1);
        exponent = ((data >> 52) & 0x7ff);
        mantissa = data & 0xfffffffffffff;
        uint64_t msbit = (mantissa >> 51) & 0x1;// Mantissa most significant bit

        if(IsNaN(OPConvertType::OPCVT_FP64, data)){// NaN
            if(msbit == 0){
                return static_cast<uint64_t>(0x1);// SNaN
            } else {
                return static_cast<uint64_t>(0x2);// QNaN
            }
        } else if(sign == 1){// negative
            if(exponent == 0x7ff && mantissa == 0x0){
                // inf
                return static_cast<uint64_t>(0x4);
            } else if(exponent == 0x0 && mantissa == 0x0){
                // 0
                return static_cast<uint64_t>(0x20);
            } else if(exponent == 0x0 && mantissa != 0x0){
                // subnormal
                return static_cast<uint64_t>(0x10);
            }else {
                // normal
                return static_cast<uint64_t>(0x8);
            }
        } else if(sign == 0){// positive
            if(exponent == 0x7ff && mantissa == 0x0){
                // inf
                return static_cast<uint64_t>(0x40);
            } else if(exponent == 0x0 && mantissa == 0x0){
                // 0
                return static_cast<uint64_t>(0x200);
            } else if(exponent == 0x0 && mantissa != 0x0){
                // subnormal
                return static_cast<uint64_t>(0x100);
            }else {
                // normal
                return static_cast<uint64_t>(0x80);
            }
        }
    } else if(cvt == OPConvertType::OPCVT_FP32){
        exponent = ((data >> 23) & 0xff);
        mantissa = data & 0x7fffff;
        uint64_t msbit = (mantissa >> 22) & 0x1;// Mantissa most significant bit

        if(IsNaN(OPConvertType::OPCVT_FP32, data)){// NaN
            if(msbit == 0){
                return static_cast<uint64_t>(0x1);// SNaN
            } else {
                return static_cast<uint64_t>(0x2);// QNaN
            }
        } else if(sign == 1){// negative
            if(exponent == 0xff && mantissa == 0x0){
                // inf
                return static_cast<uint64_t>(0x4);
            } else if(exponent == 0x0 && mantissa == 0x0){
                // 0
                return static_cast<uint64_t>(0x20);
            } else if(exponent == 0x0 && mantissa != 0x0){
                // subnormal
                return static_cast<uint64_t>(0x10);
            }else {
                // normal
                return static_cast<uint64_t>(0x8);
            }
        } else if(sign == 0){// positive
            if(exponent == 0xff && mantissa == 0x0){
                // inf
                return static_cast<uint64_t>(0x40);
            } else if(exponent == 0x0 && mantissa == 0x0){
                // 0
                return static_cast<uint64_t>(0x200);
            } else if(exponent == 0x0 && mantissa != 0x0){
                // subnormal
                return static_cast<uint64_t>(0x100);
            }else {
                // normal
                return static_cast<uint64_t>(0x80);
            }
        }
    } else if(cvt == OPConvertType::OPCVT_FP16){// 1 5 10
        exponent = ((data >> 10) & 0x1f);
        mantissa = data & 0x3ff;
        uint64_t msbit = (mantissa >> 9) & 0x1;// Mantissa most significant bit

        if(IsNaN(OPConvertType::OPCVT_FP16, data)){// NaN
            if(msbit == 0){
                return static_cast<uint64_t>(0x1);// SNaN
            } else {
                return static_cast<uint64_t>(0x2);// QNaN
            }
        } else if(sign == 1){// negative
            if(exponent == 0x1f && mantissa == 0x0){
                // inf
                return static_cast<uint64_t>(0x4);
            } else if(exponent == 0x0 && mantissa == 0x0){
                // 0
                return static_cast<uint64_t>(0x20);
            } else if(exponent == 0x0 && mantissa != 0x0){
                // subnormal
                return static_cast<uint64_t>(0x10);
            }else {
                // normal
                return static_cast<uint64_t>(0x8);
            }
        } else if(sign == 0){// positive
            if(exponent == 0x1f && mantissa == 0x0){
                // inf
                return static_cast<uint64_t>(0x40);
            } else if(exponent == 0x0 && mantissa == 0x0){
                // 0
                return static_cast<uint64_t>(0x200);
            } else if(exponent == 0x0 && mantissa != 0x0){
                // subnormal
                return static_cast<uint64_t>(0x100);
            }else {
                // normal
                return static_cast<uint64_t>(0x80);
            }
        }
    } else if(cvt == OPConvertType::OPCVT_FP8){// E4M3
        exponent = ((data >> 3) & 0xf);
        mantissa = data & 0x7;
        uint64_t msbit = (mantissa >> 2) & 0x1;// Mantissa most significant bit

        if(IsNaN(OPConvertType::OPCVT_FP8, data)){// NaN
            if(msbit == 0){
                return static_cast<uint64_t>(0x1);// SNaN
            } else {
                return static_cast<uint64_t>(0x2);// QNaN
            }
        } else if(sign == 1){// negative
            if(exponent == 0xf && mantissa == 0x0){
                // inf
                return static_cast<uint64_t>(0x4);
            } else if(exponent == 0x0 && mantissa == 0x0){
                // 0
                return static_cast<uint64_t>(0x20);
            } else if(exponent == 0x0 && mantissa != 0x0){
                // subnormal
                return static_cast<uint64_t>(0x10);
            }else {
                // normal
                return static_cast<uint64_t>(0x8);
            }
        } else if(sign == 0){// positive
            if(exponent == 0xf && mantissa == 0x0){
                // inf
                return static_cast<uint64_t>(0x40);
            } else if(exponent == 0x0 && mantissa == 0x0){
                // 0
                return static_cast<uint64_t>(0x200);
            } else if(exponent == 0x0 && mantissa != 0x0){
                // subnormal
                return static_cast<uint64_t>(0x100);
            }else {
                // normal
                return static_cast<uint64_t>(0x80);
            }
        }
    } else if(cvt == OPConvertType::OPCVT_FP8_1){// E5M2
        exponent = ((data >> 2) & 0x1f);
        mantissa = data & 0x3;
        uint64_t msbit = (mantissa >> 1) & 0x1;// Mantissa most significant bit

        if(IsNaN(OPConvertType::OPCVT_FP8_1, data)){// NaN
            if(msbit == 0){
                return static_cast<uint64_t>(0x1);// SNaN
            } else {
                return static_cast<uint64_t>(0x2);// QNaN
            }
        } else if(sign == 1){// negative
            if(exponent == 0x1f && mantissa == 0x0){
                // inf
                return static_cast<uint64_t>(0x4);
            } else if(exponent == 0x0 && mantissa == 0x0){
                // 0
                return static_cast<uint64_t>(0x20);
            } else if(exponent == 0x0 && mantissa != 0x0){
                // subnormal
                return static_cast<uint64_t>(0x10);
            }else {
                // normal
                return static_cast<uint64_t>(0x8);
            }
        } else if(sign == 0){// positive
            if(exponent == 0x1f && mantissa == 0x0){
                // inf
                return static_cast<uint64_t>(0x40);
            } else if(exponent == 0x0 && mantissa == 0x0){
                // 0
                return static_cast<uint64_t>(0x200);
            } else if(exponent == 0x0 && mantissa != 0x0){
                // subnormal
                return static_cast<uint64_t>(0x100);
            }else {
                // normal
                return static_cast<uint64_t>(0x80);
            }
        }
    } else if(cvt == OPConvertType::OPCVT_BF16){// 1 8 7
        exponent = ((data >> 7) & 0xff);
        mantissa = data & 0x7f;
        uint64_t msbit = (mantissa >> 6) & 0x1;// Mantissa most significant bit

        if(IsNaN(OPConvertType::OPCVT_BF16, data)){// NaN
            if(msbit == 0){
                return static_cast<uint64_t>(0x1);// SNaN
            } else {
                return static_cast<uint64_t>(0x2);// QNaN
            }
        } else if(sign == 1){// negative
            if(exponent == 0xff && mantissa == 0x0){
                // inf
                return static_cast<uint64_t>(0x4);
            } else if(exponent == 0x0 && mantissa == 0x0){
                // 0
                return static_cast<uint64_t>(0x20);
            } else if(exponent == 0x0 && mantissa != 0x0){
                // subnormal
                return static_cast<uint64_t>(0x10);
            }else {
                // normal
                return static_cast<uint64_t>(0x8);
            }
        } else if(sign == 0){// positive
            if(exponent == 0xff && mantissa == 0x0){
                // inf
                return static_cast<uint64_t>(0x40);
            } else if(exponent == 0x0 && mantissa == 0x0){
                // 0
                return static_cast<uint64_t>(0x200);
            } else if(exponent == 0x0 && mantissa != 0x0){
                // subnormal
                return static_cast<uint64_t>(0x100);
            }else {
                // normal
                return static_cast<uint64_t>(0x80);
            }
        }
    }
    ASSERT(0 && "The floating point number is of an invalid type.\n");
    return 0;
}

uint64_t Fmax(OPConvertType cvt, uint64_t data1, uint64_t data2, FloatRoundMode frm)
{
    float_status status = {frm, 0, floatx80_precision_x, false, false, false, false, false, false, false};
    switch (cvt) {
        case OPConvertType::OPCVT_FP64: {
            return static_cast<uint64_t>(float64_max(static_cast<float64>(data1), static_cast<float64>(data2),
                                                     &status));
        }
        case OPConvertType::OPCVT_FP32: {
            return static_cast<uint64_t>(float32_max(static_cast<float32>(data1), static_cast<float32>(data2),
                                                     &status));
        }
        case OPConvertType::OPCVT_FP16: {
            return static_cast<uint64_t>(float16_max(static_cast<float16>(data1), static_cast<float16>(data2),
                                                     &status));
        }
        case OPConvertType::OPCVT_BF16: {
            return static_cast<uint64_t>(bfloat16_max(static_cast<bfloat16>(data1), static_cast<bfloat16>(data2),
                                                      &status));
        }
        case OPConvertType::OPCVT_FP8: {
            return static_cast<uint64_t>(float8_max(static_cast<float8>(data1), static_cast<float8>(data2), &status));
        }
        case OPConvertType::OPCVT_FP8_1: {
            return static_cast<uint64_t>(float8_1_max(static_cast<float8_1>(data1), static_cast<float8_1>(data2),
                                                      &status));
        }
        default:
            ASSERT(0 && "Error: The data type is not matched.\n");
    }
    return 0;
}

uint64_t Fmin(OPConvertType cvt, uint64_t data1, uint64_t data2, FloatRoundMode frm)
{
    float_status status = {frm, 0, floatx80_precision_x, false, false, false, false, false, false, false};
    switch (cvt) {
        case OPConvertType::OPCVT_FP64: {
            return static_cast<uint64_t>(float64_min(static_cast<float64>(data1), static_cast<float64>(data2),
                                                     &status));
        }
        case OPConvertType::OPCVT_FP32: {
            return static_cast<uint64_t>(float32_min(static_cast<float32>(data1), static_cast<float32>(data2),
                                                     &status));
        }
        case OPConvertType::OPCVT_FP16: {
            return static_cast<uint64_t>(float16_min(static_cast<float16>(data1), static_cast<float16>(data2),
                                                     &status));
        }
        case OPConvertType::OPCVT_BF16: {
            return static_cast<uint64_t>(bfloat16_min(static_cast<bfloat16>(data1), static_cast<bfloat16>(data2),
                                                      &status));
        }
        case OPConvertType::OPCVT_FP8: {
            return static_cast<uint64_t>(float8_min(static_cast<float8>(data1), static_cast<float8>(data2), &status));
        }
        case OPConvertType::OPCVT_FP8_1: {
            return static_cast<uint64_t>(float8_1_min(static_cast<float8_1>(data1), static_cast<float8_1>(data2),
                                                      &status));
        }
        default:
            ASSERT(0 && "Error: The data type is not matched.\n");
    }
    return 0;
}

uint64_t Feq(OPConvertType cvt,uint64_t data1,uint64_t data2, FloatRoundMode frm)
{
    if(IsSNaN(cvt, data1) || IsSNaN(cvt, data2)) {
        ASSERT(0 && "instruction triggers an invalid operation NV exception.\n");
    }
    float_status status = {frm, 0, floatx80_precision_x, false, false, false, false, false, false, false};
    switch (cvt) {
        case OPConvertType::OPCVT_FP64: {
            return static_cast<uint64_t>(float64_eq(static_cast<float64>(data1), static_cast<float64>(data2), &status));
        }
        case OPConvertType::OPCVT_FP32: {
            return static_cast<uint64_t>(float32_eq(static_cast<float32>(data1), static_cast<float32>(data2), &status));
        }
        case OPConvertType::OPCVT_FP16: {
            return static_cast<uint64_t>(float16_eq(static_cast<float16>(data1), static_cast<float16>(data2), &status));
        }
        case OPConvertType::OPCVT_BF16: {
            return static_cast<uint64_t>(bfloat16_eq(static_cast<bfloat16>(data1), static_cast<bfloat16>(data2),
                                                     &status));
        }
        case OPConvertType::OPCVT_FP8: {
            return static_cast<uint64_t>(float8_eq(static_cast<float8>(data1), static_cast<float8>(data2), &status));
        }
        case OPConvertType::OPCVT_FP8_1: {
            return static_cast<uint64_t>(float8_1_eq(static_cast<float8_1>(data1), static_cast<float8_1>(data2),
                                                     &status));
        }
        default:
            ASSERT(0 && "Error: The data type is not matched.\n");
            return 0;
    }
}

uint64_t Feqs(OPConvertType cvt,uint64_t data1,uint64_t data2, FloatRoundMode frm)
{
    if(IsNaNOrSNan(cvt, data1) || IsNaNOrSNan(cvt, data2)) {
        ASSERT(0 && "instruction triggers an invalid operation NV exception.\n");
    }
    float_status status = {frm, 0, floatx80_precision_x, false, false, false, false, false, false, false};
    switch (cvt) {
        case OPConvertType::OPCVT_FP64: {
            return static_cast<uint64_t>(float64_eq(static_cast<float64>(data1), static_cast<float64>(data2), &status));
        }
        case OPConvertType::OPCVT_FP32: {
            return static_cast<uint64_t>(float32_eq(static_cast<float32>(data1), static_cast<float32>(data2), &status));
        }
        case OPConvertType::OPCVT_FP16: {
            return static_cast<uint64_t>(float16_eq(static_cast<float16>(data1), static_cast<float16>(data2), &status));
        }
        case OPConvertType::OPCVT_BF16: {
            return static_cast<uint64_t>(bfloat16_eq(static_cast<bfloat16>(data1), static_cast<bfloat16>(data2),
                                                     &status));
        }
        case OPConvertType::OPCVT_FP8: {
            return static_cast<uint64_t>(float8_eq(static_cast<float8>(data1), static_cast<float8>(data2), &status));
        }
        case OPConvertType::OPCVT_FP8_1: {
            return static_cast<uint64_t>(float8_1_eq(static_cast<float8_1>(data1), static_cast<float8_1>(data2),
                                                     &status));
        }
        default:
            ASSERT(0 && "Error: The data type is not matched.\n");
            return 0;
    }
}

uint64_t Flt(OPConvertType cvt,uint64_t data1,uint64_t data2, FloatRoundMode frm)
{
    if(IsSNaN(cvt, data1) || IsSNaN(cvt, data2)) {
        ASSERT(0 && "instruction triggers an invalid operation NV exception.\n");
    }
    float_status status = {frm, 0, floatx80_precision_x, false, false, false, false, false, false, false};
    switch (cvt) {
        case OPConvertType::OPCVT_FP64: {
            return static_cast<uint64_t>(float64_lt(static_cast<float64>(data1), static_cast<float64>(data2), &status));
        }
        case OPConvertType::OPCVT_FP32: {
            return static_cast<uint64_t>(float32_lt(static_cast<float32>(data1), static_cast<float32>(data2), &status));
        }
        case OPConvertType::OPCVT_FP16: {
            return static_cast<uint64_t>(float16_lt(static_cast<float16>(data1), static_cast<float16>(data2), &status));
        }
        case OPConvertType::OPCVT_BF16: {
            return static_cast<uint64_t>(bfloat16_lt(static_cast<bfloat16>(data1), static_cast<bfloat16>(data2),
                                                     &status));
        }
        case OPConvertType::OPCVT_FP8: {
            return static_cast<uint64_t>(float8_lt(static_cast<float8>(data1), static_cast<float8>(data2), &status));
        }
        case OPConvertType::OPCVT_FP8_1: {
            return static_cast<uint64_t>(float8_1_lt(static_cast<float8_1>(data1), static_cast<float8_1>(data2),
                                                     &status));
        }
        default:
            ASSERT(0 && "Error: The data type is not matched.\n");
            return 0;
    }
}

uint64_t Flts(OPConvertType cvt,uint64_t data1,uint64_t data2, FloatRoundMode frm)
{
    if(IsNaNOrSNan(cvt, data1) || IsNaNOrSNan(cvt, data2)) {
        ASSERT(0 && "instruction triggers an invalid operation NV exception.\n");
    }
    float_status status = {frm, 0, floatx80_precision_x, false, false, false, false, false, false, false};
    switch (cvt) {
        case OPConvertType::OPCVT_FP64: {
            return static_cast<uint64_t>(float64_lt(static_cast<float64>(data1), static_cast<float64>(data2), &status));
        }
        case OPConvertType::OPCVT_FP32: {
            return static_cast<uint64_t>(float32_lt(static_cast<float32>(data1), static_cast<float32>(data2), &status));
        }
        case OPConvertType::OPCVT_FP16: {
            return static_cast<uint64_t>(float16_lt(static_cast<float16>(data1), static_cast<float16>(data2), &status));
        }
        case OPConvertType::OPCVT_BF16: {
            return static_cast<uint64_t>(bfloat16_lt(static_cast<bfloat16>(data1), static_cast<bfloat16>(data2),
                                                     &status));
        }
        case OPConvertType::OPCVT_FP8: {
            return static_cast<uint64_t>(float8_lt(static_cast<float8>(data1), static_cast<float8>(data2), &status));
        }
        case OPConvertType::OPCVT_FP8_1: {
            return static_cast<uint64_t>(float8_1_lt(static_cast<float8_1>(data1), static_cast<float8_1>(data2),
                                                     &status));
        }
        default:
            ASSERT(0 && "Error: The data type is not matched.\n");
            return 0;
    }
}

} // namespace Calculate
} // namespace JCore