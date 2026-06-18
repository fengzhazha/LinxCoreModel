#pragma once


#include "core/Bus.h"

namespace JCore {

enum PFConfLevel {
    VERY_CONSERVATIVE,
    CONSERVATIVE,
    MIDDLE1,
    MIDDLE2,
    MIDDLE3,
    AGGRESSIVE,
    VERY_AGGRESSIVE,
    LVL_NUM
};

enum PFParaState {
    PARA_LOW,
    PARA_MID,
    PARA_HIGH
};

enum PFConfOP {
    DECREMENT,
    NOT_CHANGE,
    INCREMENT
};

struct StreamConf {
    PFConfLevel level;
    uint64_t    region_size;
    uint64_t    degree;
    uint64_t    pf_lev_mole;
    uint64_t    pf_lev_deno;
};

PFConfLevel incPFLevel(PFConfLevel level);

PFConfLevel decPFLevel(PFConfLevel level);

float calRate(uint64_t total_mole, uint64_t total_deno, uint64_t interval_melo, uint64_t interval_deno);

} // namespace JCore