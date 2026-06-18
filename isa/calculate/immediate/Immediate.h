#pragma once

#ifndef IMMEDIATE_H
#define IMMEDIATE_H

#include "../MInstCalculator.h"

namespace JCore {
namespace Calculate {
namespace Immediate {
Handler Lookup(Opcode op);
bool CalculateImmediate(MInst &inst);
} // namespace Immediate
} // namespace Calculate
} // namespace JCore
#endif