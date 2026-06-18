#pragma once

#include <map>
#include <stdexcept>
#include <string>
#include <iostream>

namespace JCore {

enum class DataType {
    FP64     = 0,
    FP32     = 1,
    TF32     = 2,
    HF32     = 3,
    FP16     = 4,
    BF16     = 5, // e8m7x2
    HIF8     = 6,
    FP8      = 7, // e4m3
    FP8_1    = 8, // e5m2
    FP6      = 9, // e3m2
    FP6_1    = 10, // e2m3
    FP4      = 11, // e2m1x2
    FP4_1    = 12, // e1m2x2
    SF8      = 13, // e8m0
    HIF4     = 14, // HiF4x2
    INT64    = 16, // S64
    INT32    = 17, // S32
    INT16    = 18, // S16
    INT8     = 19, // S8
    INT4     = 20, // S4x2
    UINT64   = 24, // U64
    UINT32   = 25, // U32
    UINT16   = 26, // U16
    UINT8    = 27, // U8
    UINT4    = 28, // U4*2
    INVALID  = 31,
};

inline std::ostream& operator<<(std::ostream& os, DataType dataType)
{
    switch (dataType) {
        case DataType::FP64:
            return os << "FP64";
        case DataType::FP32:
            return os <<"FP32";
        case DataType::TF32:
            return os <<"TF32";
        case DataType::HF32:
            return os <<"HF32";
        case DataType::FP16:
            return os <<"FP16";
        case DataType::BF16:
            return os <<"BF16";
        case DataType::HIF8:
            return os <<"HIF8";
        case DataType::FP8:
            return os <<"FP8";
        case DataType::FP8_1:
            return os <<"FP8_1";
        case DataType::FP6:
            return os <<"FP6";
        case DataType::FP6_1:
            return os <<"FP6_1";
        case DataType::FP4:
            return os <<"FP4";
        case DataType::FP4_1:
            return os <<"FP4_1";
        case DataType::SF8:
            return os <<"SF8";
        case DataType::HIF4:
            return os <<"HIF4";
        case DataType::INT64:
            return os <<"INT64";
        case DataType::INT32:
            return os <<"INT32";
        case DataType::INT16:
            return os <<"INT16";
        case DataType::INT8:
            return os <<"INT8";
        case DataType::INT4:
            return os <<"INT4";
        case DataType::UINT64:
            return os <<"UINT64";
        case DataType::UINT32:
            return os <<"UINT32";
        case DataType::UINT16:
            return os <<"UINT16";
        case DataType::UINT8:
            return os <<"UINT8";
        case DataType::UINT4:
            return os <<"UINT4";
        case DataType::INVALID:
            return os <<"INVALID";
        default:
            return os <<"Unknown DataType";
    }
}

inline std::string BriefDataType2String(DataType t)
{
    switch (t) {
        case DataType::FP64: return "FP64";
        case DataType::FP32: return "FP32";
        case DataType::TF32: return "TF32";
        case DataType::HF32: return "HF32";
        case DataType::FP16: return "FP16";
        case DataType::BF16: return "BF16";
        case DataType::HIF8: return "HIF8";
        case DataType::FP8: return "E4M3";
        case DataType::FP8_1: return "E5M2";
        case DataType::FP6: return "E3M2";
        case DataType::FP6_1: return "E2M3";
        case DataType::FP4: return "E2M1x2";
        case DataType::FP4_1: return "E1M2x2";
        case DataType::SF8: return "SF8";
        case DataType::HIF4: return "HIF4x2";
        case DataType::INT64: return "INT64";
        case DataType::INT32: return "INT32";
        case DataType::INT16: return "INT16";
        case DataType::INT8: return "INT8";
        case DataType::INT4: return "INT4x2";
        case DataType::UINT64: return "UINT64";
        case DataType::UINT32: return "UINT32";
        case DataType::UINT16: return "UINT16";
        case DataType::UINT8: return "UINT8";
        case DataType::UINT4: return "UINT4x2";
        case DataType::INVALID: return "INVALID";
        default: return "Unknown DataType";
    }
}

const std::map<std::string, DataType> STR_DATA_TYPE_MAP = {
    {"FP64", DataType::FP64},
    {"FP32", DataType::FP32},
    {"TF32", DataType::TF32},
    {"HF32", DataType::HF32},
    {"FP16", DataType::FP16},
    {"BF16", DataType::BF16},
    {"HIF8", DataType::HIF8},
    {"E4M3", DataType::FP8},
    {"E5M2", DataType::FP8_1},
    {"E3M2", DataType::FP6},
    {"E2M3", DataType::FP6_1},
    {"E2M1x2", DataType::FP4},
    {"E1M2x2", DataType::FP4_1},
    {"SF8", DataType::SF8},
    {"HIF4x2", DataType::HIF4},
    {"INT64", DataType::INT64},
    {"INT32", DataType::INT32},
    {"INT16", DataType::INT16},
    {"INT8", DataType::INT8},
    {"INT4x2", DataType::INT4},
    {"UINT64", DataType::UINT64},
    {"UINT32", DataType::UINT32},
    {"UINT16", DataType::UINT16},
    {"UINT8", DataType::UINT8},
    {"UINT4x2", DataType::UINT4},
};
 
inline DataType String2BriefDataType(std::string s)
{
    if (STR_DATA_TYPE_MAP.find(s) == STR_DATA_TYPE_MAP.end()) {
        return DataType::INVALID;
    }
    return STR_DATA_TYPE_MAP.at(s);
}

constexpr size_t HF4_DATA_WIDTH = 1;
constexpr size_t HF8_DATA_WIDTH = 1;
constexpr size_t BF16_DATA_WIDTH = 2;
constexpr size_t FP32_DATA_WIDTH = 4;
constexpr size_t INT64_DATA_WIDTH = 8;

inline size_t BytesOf(DataType t) {
    switch (t) {
        case DataType::INT4:
        case DataType::HIF4: return HF4_DATA_WIDTH; // 4bits still need 1 byte
        case DataType::INT8:
        case DataType::FP8:
        case DataType::FP8_1:
        case DataType::HIF8: return HF8_DATA_WIDTH;
        case DataType::INT16:
        case DataType::FP16:
        case DataType::BF16: return BF16_DATA_WIDTH;
        case DataType::INT32:
        case DataType::FP32: return FP32_DATA_WIDTH;
        case DataType::INT64: return INT64_DATA_WIDTH;
        default:
            // std::cerr << "data type :" << (uint32_t)t << std::endl;
            // throw std::invalid_argument("Unknown DataType");
            return HF8_DATA_WIDTH;
    }
}

inline JCore::DataType ConvertTo4ByteType(JCore::DataType dstType)
{
    JCore::DataType retType = dstType;
    switch (dstType) {
        case JCore::DataType::FP64:
        case JCore::DataType::FP32:
        case JCore::DataType::TF32:
        case JCore::DataType::HF32:
        case JCore::DataType::FP16:
        case JCore::DataType::BF16:
        case JCore::DataType::HIF8:
        case JCore::DataType::FP8:
        case JCore::DataType::FP8_1:
        case JCore::DataType::FP6:
        case JCore::DataType::FP6_1:
        case JCore::DataType::FP4:
        case JCore::DataType::FP4_1:
        case JCore::DataType::SF8:
        case JCore::DataType::HIF4:
            retType = JCore::DataType::FP32;
            break;
        case JCore::DataType::INT64:
        case JCore::DataType::INT32:
        case JCore::DataType::INT16:
        case JCore::DataType::INT8:
        case JCore::DataType::INT4:
            retType = JCore::DataType::INT32;
            break;
        case JCore::DataType::UINT64:
        case JCore::DataType::UINT32:
        case JCore::DataType::UINT16:
        case JCore::DataType::UINT8:
        case JCore::DataType::UINT4:
            retType = JCore::DataType::UINT32;
            break;
        default:
            break;
    }
        return retType;
}
} // namespace JCore