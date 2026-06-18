#ifndef  S_CONST_CONFIG_H
#define S_CONST_CONFIG_H

#include <cassert>
#include <iostream>

#include "Common.h"
#include "ISA.h"

namespace JCore {

constexpr uint64_t BITS_IN_BYTE = 8;
constexpr uint32_t MIN_LS_DATA_BYETS = 32;
constexpr uint32_t KILO = 1024;
constexpr uint32_t LOG_2 = 2;
constexpr uint32_t MAX_TILE_DATA_BYTE = 256;
constexpr uint32_t MAX_LANE_DATA_SIZE = 512; // sizeof(Uint64) * 64 = 512
const uint64_t INVALID_PC = 0xFFFFFFFFFFFFFFFFULL;

/* Construction of tid for communication with SOC */
constexpr uint32_t TID_TYPE_OFFSET = 24;
constexpr uint32_t PREFETCH_TID_TYPE = 3;
constexpr uint32_t PREFETCH_SIZE_BYTE = 256;
constexpr uint32_t PREFETCH_REQ_TYPE = 2;
} // namespace JCore
#endif