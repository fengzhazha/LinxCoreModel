#pragma once

#include <cstdint>
#include <unordered_map>
#include "ModelCommon/SimInstInfo.h"

namespace JCore {

enum class IexExecUnit {
    IALU = 0,
    FCVT,
    FDIV,
    FMLA,
    IMAC,
    PERMUTE,
    UNIT_NUM,
    UNDEF = 0xff
};

struct IexLatencyInfo {
    uint64_t latency;
    IexExecUnit unit;
};

struct IexLatency {
    static const std::unordered_map<Opcode, IexLatencyInfo> VEC_LATENCY;
    static const std::unordered_map<IexExecUnit, uint32_t> VEC_ITER_CYCLES;

    static bool GetInstExLatency(const SimInst& inst, uint32_t& latency);
    static std::string GetExecUnitString(IexExecUnit unit);
};

} // namespace JCore
