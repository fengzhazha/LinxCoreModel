#pragma once

#ifndef GROUP_INFO_H
#define GROUP_INFO_H

#include <cstdint>

#include "../ROBID.h"
#include "../BlockCommand.h"

namespace JCore {
struct GroupAllocInfo {
    ROBID bid;
    uint32_t stid = 0;
    uint64_t bpc = 0;
    uint64_t tpc = 0;
    uint64_t groupNum = 0;
    uint64_t dest = 0;
    uint64_t tileId = 0;
    uint32_t tag = 0;
    bool destVld = false;
    uint64_t createCycle = 0;
    BlockCommandPtr blockCmd = nullptr;
};

struct GroupIssueInfo {
    ROBID bid;
    ROBID gid;
    uint32_t stid = 0;
    uint32_t tid = 0;
    uint64_t p1Cycle = 0;
};
}
#endif