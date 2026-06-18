#pragma once

#ifndef ATOMIC_H
#define ATOMIC_H

#include "../MInstCalculator.h"

namespace JCore {
namespace Calculate {
namespace Atomic {
Handler Lookup(Opcode op);
bool CalculateAtomic(MInst &inst);
} // namespace Atomic
} // namespace Calculate
} // namespace JCore
#endif