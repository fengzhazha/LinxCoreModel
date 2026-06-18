#pragma once

#ifndef BIT_H
#define BIT_H

#include "../MInstCalculator.h"

namespace JCore {
namespace Calculate {
namespace Bit {
Handler Lookup(Opcode op);
bool CalculateBit(MInst &inst);
} // namespace Bit
} // namespace Calculate
} // namespace JCore
#endif