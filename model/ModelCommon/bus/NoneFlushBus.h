#pragma once

#ifndef NONE_FLUSH_BUS_H
#define NONE_FLUSH_BUS_H

#include <cstdint>

#include "../ROBID.h"

namespace JCore {
struct LdNonFlushBus {
    uint32_t                peid = -1U;
    uint32_t                tid = 0;
    ROBID                   bid;
    ROBID                   gid;
    ROBID                   rid;
    ROBID                   lsID;
};
struct StNonFlushBus {
    uint32_t                peid = -1U;
    uint32_t                tid = 0;
    ROBID                   bid;
    ROBID                   gid;
    ROBID                   rid;
    ROBID                   lsID;
};
}
#endif