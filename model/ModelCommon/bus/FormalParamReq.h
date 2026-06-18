#pragma once

#ifndef FORMAL_PARAM_H
#define FORMAL_PARAM_H

#include <cstdint>

#include "../ROBID.h"

namespace JCore {
struct LocalFreeInfo {
    bool vld = false;
    bool last = false;
    bool write = false;
    ROBID bid;
    uint32_t offset = 0;
    uint32_t ptag = 0;
    uint64_t data = 0;
    uint32_t stid = -1U;
};

struct LocalFreeWriteInfo {
    bool vld = false;
    bool last = false;
    ROBID bid;
    uint32_t ptag = 0;
    uint64_t data = 0;
};
}
#endif