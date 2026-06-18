#pragma once
#ifndef MODEL_COMMON_H
#define MODEL_COMMON_H

#include "ModelEnumDefines.h"
#include "RingQueue.h"
#include "ROBID.h"
#include "SimQueue.h"

#define INPUT
#define OUTPUT
#define INNER

constexpr uint64_t NUM_256 = 256;
constexpr uint64_t NUM_1024 = 1024;
constexpr uint64_t NUM_100 = 100;

constexpr int MEM_REG_NUM = 3;
constexpr int MEM_BFI_NUM = 4;
constexpr int LOW_UIMM_LEN = 12;
constexpr int LOW_UIMM_MASK = 0xfff;
constexpr int HIGH_UIMM_MASK = 0x7;

#endif