#pragma once

#ifndef STACK_RENAME_BUS
#define STACK_RENAME_BUS

#include <cstdint>

#include "../ROBID.h"
#include "../ModelEnumDefines.h"

namespace JCore {
/* Rename request bus format for stack load/store */
struct StackRenameBus {
    bool                    vld = false;
    bool                    ready = false;
    bool                    calculated = false;
    bool                    stall = false;
    bool                    set_sp = false;
    StackInstType           type = StackInstType::NORMAL;
    ROBID                   bid;
    ROBID                   rid;
    ROBID                   lsID;
    uint32_t                peID = 0;
    uint32_t                sptag = 0;
    uint32_t                stid = -1U;
    int64_t                 imm = 0;
    uint64_t                tpc = 0;
};

struct MoveOffset {
    uint32_t                srcAtag;
    int64_t                 imm = 0;
};
}
#endif