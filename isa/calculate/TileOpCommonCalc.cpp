#include <cassert>
#include <cmath>

#include "FloatPointUtils.h"
#include "TileOpCommonCalc.h"

namespace JCore {

uint64_t TileOpCommonCalc::FillEle(DataType dataType, PadValue paddingValue)
{
    uint64_t paddingData = 0;
    auto getFillDataMax = [](DataType dataType) -> uint64_t {
        switch (dataType) {
            case DataType::FP64:
                return 0x7FEFFFFFFFFFFFFF;
            case DataType::FP32:
                return 0x000000007F7FFFFF;
            case DataType::FP16:
                return 0x7BFF;
            case DataType::BF16:
                return 0xFE7F;
            case DataType::FP8:
                return 0x77;
            case DataType::FP8_1:
                return 0x7B;
            case DataType::FP4:
                return 0x7;
            case DataType::FP4_1:
                return 0x7;
            case DataType::INT64:
                return 0x7FFFFFFFFFFFFFFF;
            case DataType::INT32:
                return 0x7FFFFFFF;
            case DataType::INT16:
                return 0x7FFF;
            case DataType::INT8:
                return 0x7F;
            case DataType::UINT64:
                return 0xFFFFFFFFFFFFFFFF;
            case DataType::UINT32:
                return 0xFFFFFFFF;
            case DataType::UINT16:
                return 0xFFFF;
            case DataType::UINT8:
                return 0xFF;
            default:
                assert(0 && "ERROR: The current type of data filling is not supported yet");
                return 0;
        }
    };

    auto getFillDataMin = [](DataType dataType) -> uint64_t {
        switch (dataType) {
            case DataType::FP64:
                return 0xFEEFFFFFFFFFFFFF;
            case DataType::FP32:
                return 0xFF7FFFFF;
            case DataType::FP16:
                return 0xFBFF;
            case DataType::BF16:
                return 0xFE7F;
            case DataType::FP8:
                return 0xF7;
            case DataType::FP8_1:
                return 0xFB;
            case DataType::FP4:
                return 0xA;
            case DataType::FP4_1:
                return 0xC;
            case DataType::INT64:
                return 0x8000000000000000;
            case DataType::INT32:
                return 0xFFFFFFFF80000000;
            case DataType::INT16:
                return 0xFFFFFFFF8000;
            case DataType::INT8:
                return 0xFFFFFFFFFFFFFF80;
            case DataType::UINT64:
            case DataType::UINT32:
            case DataType::UINT16:
            case DataType::UINT8:
                return 0;
            default:
                assert(0 && "ERROR: The current type of data filling is not supported yet");
                return 0;
        }
    };

    switch (paddingValue) {
        case PadValue::ZERO:
            paddingData = 0;
            break;
        case PadValue::MAX:
            paddingData = getFillDataMax(dataType);
            break;
        case PadValue::MIN:
            paddingData = getFillDataMin(dataType);
            break;
        case PadValue::NONE:
            break;
        default:
            assert(0 && "ERROR: Wrong data fill type");
    }
    return paddingData;
}

// Floating-point numbers are implemented based on existing FEXP instruction logic
uint64_t TileOpCommonCalc::EleExp(uint64_t ele, DataType dataType, float_status status)
{
    uint64_t result = 0;
    switch (dataType) {
        case DataType::FP64: {
            uint64_t bits;
            double outcome = JCore::Calculate::Fexp<double>(ele);
            std::memcpy(&bits, &outcome, sizeof(bits));
            result = bits;
            break;
        }
        case DataType::FP32: {
            uint32_t bits = 0;
            float outcome = JCore::Calculate::Fexp<float>(ele);
            std::memcpy(&bits, &outcome, sizeof(bits));
            result = static_cast<uint64_t>(bits);
            break;
        }
        case DataType::FP16: {
            uint32_t bits = 0;
            ele = Calculate::ConvertAggre(ele,
                                          OPConvertType::OPCVT_FP16,
                                          OPConvertType::OPCVT_FP32,
                                          status.float_rounding_mode);
            float outcome = JCore::Calculate::Fexp<float>(ele);
            std::memcpy(&bits, &outcome, sizeof(float));
            ele = Calculate::ConvertAggre(bits,
                                          OPConvertType::OPCVT_FP32,
                                          OPConvertType::OPCVT_FP16,
                                          status.float_rounding_mode);
            result = ele;
            break;
        }
        case DataType::BF16: {
            uint32_t bits = 0;
            ele = Calculate::ConvertAggre(ele,
                                          OPConvertType::OPCVT_BF16,
                                          OPConvertType::OPCVT_FP32,
                                          status.float_rounding_mode);
            float outcome = JCore::Calculate::Fexp<float>(ele);
            std::memcpy(&bits, &outcome, sizeof(float));
            ele = Calculate::ConvertAggre(bits,
                                          OPConvertType::OPCVT_FP32,
                                          OPConvertType::OPCVT_BF16,
                                          status.float_rounding_mode);
            result = ele;
            break;
        }
        case DataType::FP8: {
            uint32_t bits = 0;
            ele = Calculate::ConvertAggre(ele,
                                          OPConvertType::OPCVT_FP8,
                                          OPConvertType::OPCVT_FP32,
                                          status.float_rounding_mode);
            float outcome = JCore::Calculate::Fexp<float>(ele);
            std::memcpy(&bits, &outcome, sizeof(float));
            ele = Calculate::ConvertAggre(bits,
                                          OPConvertType::OPCVT_FP32,
                                          OPConvertType::OPCVT_FP8,
                                          status.float_rounding_mode);
            result = ele;
            break;
        }
        case DataType::FP8_1: {
            uint32_t bits = 0;
            ele = Calculate::ConvertAggre(ele,
                                          OPConvertType::OPCVT_FP8_1,
                                          OPConvertType::OPCVT_FP32,
                                          status.float_rounding_mode);
            float outcome = JCore::Calculate::Fexp<float>(ele);
            std::memcpy(&bits, &outcome, sizeof(float));
            ele = Calculate::ConvertAggre(bits,
                                          OPConvertType::OPCVT_FP32,
                                          OPConvertType::OPCVT_FP8_1,
                                          status.float_rounding_mode);
            result = ele;
            break;
        }
        case DataType::FP4: {
            uint32_t bits = 0;
            ele = Calculate::ConvertAggre(ele,
                                          OPConvertType::OPCVT_FP4,
                                          OPConvertType::OPCVT_FP32,
                                          status.float_rounding_mode);
            float outcome = JCore::Calculate::Fexp<float>(ele);
            std::memcpy(&bits, &outcome, sizeof(float));
            ele = Calculate::ConvertAggre(bits,
                                          OPConvertType::OPCVT_FP32,
                                          OPConvertType::OPCVT_FP4,
                                          status.float_rounding_mode);
            result = ele;
            break;
        }
        case DataType::FP4_1: {
            uint32_t bits = 0;
            ele = Calculate::ConvertAggre(ele,
                                          OPConvertType::OPCVT_FP4_1,
                                          OPConvertType::OPCVT_FP32,
                                          status.float_rounding_mode);
            float outcome = JCore::Calculate::Fexp<float>(ele);
            std::memcpy(&bits, &outcome, sizeof(float));
            ele = Calculate::ConvertAggre(bits,
                                          OPConvertType::OPCVT_FP32,
                                          OPConvertType::OPCVT_FP4_1,
                                          status.float_rounding_mode);
            result = ele;
            break;
        }
        case DataType::INT64: {
            double result1 = std::exp(static_cast<double>(static_cast<int64_t>(ele)));
            uint64_t result2 = 0;
            std::memcpy(&result2, &result1, sizeof(result2));
            result = result2;
            break;
        }
        case DataType::INT32: {
            double result1 = std::exp(static_cast<double>(static_cast<int32_t>(ele)));
            uint64_t result2 = 0;
            std::memcpy(&result2, &result1, sizeof(result2));
            result = result2;
            break;
        }
        case DataType::INT16: {
            double result1 = std::exp(static_cast<double>(static_cast<int16_t>(ele)));
            uint64_t result2 = 0;
            std::memcpy(&result2, &result1, sizeof(result2));
            result = result2;
            break;
        }
        case DataType::INT8: {
            double result1 = std::exp(static_cast<double>(static_cast<int8_t>(ele)));
            uint64_t result2 = 0;
            std::memcpy(&result2, &result1, sizeof(result2));
            result = result2;
            break;
        }
        case DataType::UINT64: {
            double result1 = std::exp(static_cast<double>(static_cast<uint64_t>(ele)));
            uint64_t result2 = 0;
            std::memcpy(&result2, &result1, sizeof(result2));
            result = result2;
            break;
        }
        case DataType::UINT32: {
            double result1 = std::exp(static_cast<double>(static_cast<uint32_t>(ele)));
            uint64_t result2 = 0;
            std::memcpy(&result2, &result1, sizeof(result2));
            result = result2;
            break;
        }
        case DataType::UINT16: {
            double result1 = std::exp(static_cast<double>(static_cast<uint16_t>(ele)));
            uint64_t result2 = 0;
            std::memcpy(&result2, &result1, sizeof(result2));
            result = result2;
            break;
        }
        case DataType::UINT8: {
            double result1 = std::exp(static_cast<double>(static_cast<uint8_t>(ele)));
            uint64_t result2 = 0;
            std::memcpy(&result2, &result1, sizeof(result2));
            result = result2;
            break;
        }
        default:
            std::cerr << "Unsupported dataType: " << BriefDataType2String(dataType) << std::endl;
            assert(0);
    }
    return result;
}

// Floating-point numbers are implemented based on existing FRECIP instruction logic
uint64_t TileOpCommonCalc::EleRecip(uint64_t ele, DataType dataType)
{
    uint64_t result = 0;
    switch (dataType) {
        case DataType::FP64: {
            double result1 = JCore::Calculate::Frecip<double>(ele);
            uint64_t result2 = 0;
            std::memcpy(&result2, &result1, sizeof(result2));
            result = result2;
            break;
        }
        case DataType::FP32: {
            float result1 = JCore::Calculate::Frecip<float>(ele);
            uint32_t result2 = 0;
            std::memcpy(&result2, &result1, sizeof(result2));
            result = result2;
            break;
        }
        case DataType::INT64: {
            long double outcome = static_cast<long double>(static_cast<int64_t>(ele));
            double result1 = static_cast<double>(1.0L / outcome);
            uint64_t result2 = 0;
            std::memcpy(&result2, &result1, sizeof(result2));
            result = result2;
            break;
        }
        case DataType::INT32: {
            long double outcome = static_cast<long double>(static_cast<int32_t>(ele));
            double result1 = static_cast<double>(1.0L / outcome);
            uint64_t result2 = 0;
            std::memcpy(&result2, &result1, sizeof(result2));
            result = result2;
            break;
        }
        case DataType::INT16: {
            long double outcome = static_cast<long double>(static_cast<int16_t>(ele));
            double result1 = static_cast<double>(1.0L / outcome);
            uint64_t result2 = 0;
            std::memcpy(&result2, &result1, sizeof(result2));
            result = result2;
            break;
        }
        case DataType::INT8: {
            long double outcome = static_cast<long double>(static_cast<int8_t>(ele));
            double result1 = static_cast<double>(1.0L / outcome);
            uint64_t result2 = 0;
            std::memcpy(&result2, &result1, sizeof(result2));
            result = result2;
            break;
        }
        case DataType::UINT64: {
            long double outcome = static_cast<long double>(ele);
            double result1 = static_cast<double>(1.0L / outcome);
            uint64_t result2 = 0;
            std::memcpy(&result2, &result1, sizeof(result2));
            result = result2;
            break;
        }
        case DataType::UINT32: {
            long double outcome = static_cast<long double>(static_cast<uint32_t>(ele));
            double result1 = static_cast<double>(1.0L / outcome);
            uint64_t result2 = 0;
            std::memcpy(&result2, &result1, sizeof(result2));
            result = result2;
            break;
        }
        case DataType::UINT16: {
            long double outcome = static_cast<long double>(static_cast<uint16_t>(ele));
            double result1 = static_cast<double>(1.0L / outcome);
            uint64_t result2 = 0;
            std::memcpy(&result2, &result1, sizeof(result2));
            result = result2;
            break;
        }
        case DataType::UINT8: {
            long double outcome = static_cast<long double>(static_cast<uint8_t>(ele));
            double result1 = static_cast<double>(1.0L / outcome);
            uint64_t result2 = 0;
            std::memcpy(&result2, &result1, sizeof(result2));
            result = result2;
            break;
        }
        default:
            std::cerr << "Unsupported dataType: " << BriefDataType2String(dataType) << std::endl;
            assert(0);
    }
    return result;
}
}