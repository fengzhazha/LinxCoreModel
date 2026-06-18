#pragma once

#ifndef LPV_INFO_H
#define LPV_INFO_H

#include <cstdint>
#include <memory>
#include <cstring>

namespace JCore {
constexpr uint32_t LOAD_VECTOR = 4;
constexpr uint32_t CANCEL_VECTOR = 1;
constexpr uint32_t RELEASE_VECTOR = 0;
constexpr uint32_t LPV_SIZE = 8;
struct LpvInfo {
    /* TO DO: To solve the bug of invalid pointer, change vector to array */
    uint32_t lpv[LPV_SIZE] = {0};
    bool notEmpty = false;

    void LoadInit(uint32_t pipe) {
        lpv[pipe] = LOAD_VECTOR;
        notEmpty = true;
    }
    void Reset() {
        memset(lpv, 0, sizeof(lpv));
        notEmpty = false;
    }
    void Move() {
        if (!notEmpty) {
            return;
        }
        notEmpty = false;
        for (uint32_t i = 0; i < LPV_SIZE; ++i) {
            if (lpv[i]) {
                lpv[i] >>= 1;
                notEmpty |= (lpv[i] != 0);
            }
        }
    }
    void Set(const LpvInfo& info) {
        for (uint32_t i = 0; i < LPV_SIZE; ++i) {
            lpv[i] |= info.lpv[i];
            notEmpty |= (lpv[i] != 0);
        }
    }
    bool CheckCancel(uint32_t pipe) { return (lpv[pipe] & CANCEL_VECTOR) != 0; }
    bool CheckRelease() {
        for (uint32_t i = 0; i < LPV_SIZE; ++i) {
            if (lpv[i] != RELEASE_VECTOR) {
                return false;
            }
        }
        return true;
    }
};

using PLpvInfo = std::shared_ptr<LpvInfo>;
void SetLpv(PLpvInfo src, PLpvInfo &dst, bool force = false);
}
#endif