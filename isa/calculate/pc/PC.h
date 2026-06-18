#pragma once

#ifndef PC_H
#define PC_H

#include "../MInstCalculator.h"

namespace JCore {
namespace Calculate {
namespace PC {
Handler Lookup(Opcode op);
bool CalculatePC(MInst &inst);
} // namespace PC
} // namespace Calculate
} // namespace JCore
#endif