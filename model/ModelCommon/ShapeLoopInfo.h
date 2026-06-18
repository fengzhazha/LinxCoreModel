#pragma once

#ifndef SHAPE_LOOP_INFO
#define SHAPE_LOOP_INFO

#include <cstdint>

namespace JCore {
struct ShapeLoopInfo {
    uint64_t lb0 = 0;
    uint64_t lb1 = 0;
    uint64_t lb2 = 0;
    uint32_t groupNum = 0;
    uint32_t avgGroupNum = 0;
    uint32_t lastGroupNum = 0;
    uint32_t validLaneNum = 64;
    bool     dimReduction = true;
    uint32_t m_lanes = 0;

    ShapeLoopInfo() = default;
    ShapeLoopInfo(uint64_t m_lb0, uint64_t m_lb1, uint64_t m_lb2, uint32_t gnum, uint32_t aNum,
        uint32_t lNum, uint32_t mlane)
        : lb0(m_lb0), lb1(m_lb1), lb2(m_lb2), groupNum(gnum), avgGroupNum(aNum),
          lastGroupNum(lNum), m_lanes(mlane) {}

    uint64_t GetSIMTMask() const {
        constexpr uint64_t MAX_LANES_SUPPORT = 64;
        assert(validLaneNum <= MAX_LANES_SUPPORT && "not support");
        if (validLaneNum == 64) {
            return ~0ULL;
        }
        /* FIXME: maybe use vector<bool> or bitset to instead uint64_t */
        return (1ULL << validLaneNum) - 1;
    }
};
}
#endif