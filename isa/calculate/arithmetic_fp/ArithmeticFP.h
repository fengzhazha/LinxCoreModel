#pragma once

#ifndef ARITHMETIC_FP_H
#define ARITHMETIC_FP_H

#include "../MInstCalculator.h"

namespace JCore {
namespace Calculate {
namespace ArithmeticFP {
Handler Lookup(Opcode op);
bool CalculateArithmeticFP(MInst &inst);
} // namespace ArithmeticFP
} // namespace Calculate
} // namespace JCore
#endif