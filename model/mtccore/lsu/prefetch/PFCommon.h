#pragma once


#include "core/Bus.h"

namespace JCore {

enum MTCPFConfLevel {
    MTCVERY_MTCCONSERVATIVE,
    MTCCONSERVATIVE,
    MTCMIDDLE1,
    MTCMIDDLE2,
    MTCMIDDLE3,
    MTCAGGRESSIVE,
    MTCVERY_MTCAGGRESSIVE,
    MTCLVL_NUM
};

enum MTCPFParaState {
    MTCPARA_LOW,
    MTCPARA_MID,
    MTCPARA_HIGH
};

enum MTCPFConfOP {
    MTCDECREMENT,
    MTCNOT_CHANGE,
    MTCINCREMENT
};

struct MtcStreamConf {
    MTCPFConfLevel level;
    uint64_t    region_size;
    uint64_t    degree;
    uint64_t    pf_lev_mole;
    uint64_t    pf_lev_deno;
};

MTCPFConfLevel incPFLevel(MTCPFConfLevel level);

MTCPFConfLevel decPFLevel(MTCPFConfLevel level);

float mtccalRate(uint64_t total_mole, uint64_t total_deno, uint64_t interval_melo, uint64_t interval_deno);

} // namespace JCore