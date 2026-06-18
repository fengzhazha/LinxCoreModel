#pragma once

#ifndef COMPOUND_H
#define COMPOUND_H

#include "../MInstCalculator.h"

namespace JCore {
namespace Calculate {
namespace Compound {
Handler Lookup(Opcode op);
bool CalculateCompound(MInst &inst);
} // namespace Compound
} // namespace Calculate
} // namespace JCore
#endif