#pragma once

#ifndef CORE_CONTROL_BUS_H
#define CORE_CONTROL_BUS_H

#include <cstdint>

namespace JCore {
struct CoreControlBus {
    bool                    vld;
    bool                    interrupt;
    uint64_t                initPC;
    bool                    halt;
};
}
#endif