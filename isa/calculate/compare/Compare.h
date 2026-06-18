#pragma once

#ifndef COMPARE_H
#define COMPARE_H

#include "../MInstCalculator.h"

namespace JCore {
namespace Calculate {
namespace Compare {
Handler Lookup(Opcode op);
bool CalculateCompare(MInst &inst);
} // namespace Compare
} // namespace Calculate
} // namespace JCore
#endif