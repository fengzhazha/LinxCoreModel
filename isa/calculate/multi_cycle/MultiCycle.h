#pragma once

#ifndef MULTI_CYCLE_H
#define MULTI_CYCLE_H

#include "../MInstCalculator.h"

namespace JCore {
namespace Calculate {
namespace MultiCycle {
Handler Lookup(Opcode op);
bool CalculateMultiCycle(MInst &inst);
} // namespace MultiCycle
} // namespace Calculate
} // namespace JCore
#endif