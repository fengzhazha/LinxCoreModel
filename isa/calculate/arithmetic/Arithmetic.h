#pragma once

#ifndef ARITHMETIC_H
#define ARITHMETIC_H

#include "../MInstCalculator.h"

namespace JCore {
namespace Calculate {
namespace Arithmetic {
Handler Lookup(Opcode op);
bool CalculateArithmetic(MInst &inst);
} // namespace Arithmetic
} // namespace Calculate
} // namespace JCore
#endif