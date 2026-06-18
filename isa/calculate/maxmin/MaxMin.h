#pragma once

#ifndef MAXMIN_H
#define MAXMIN_H

#include "../MInstCalculator.h"

namespace JCore {
namespace Calculate {
namespace MaxMin {
Handler Lookup(Opcode op);
bool CalculateMaxMin(MInst &inst);
} // namespace MaxMin
} // namespace Calculate
} // namespace JCore
#endif