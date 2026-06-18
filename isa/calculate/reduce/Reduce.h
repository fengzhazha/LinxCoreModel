#pragma once

#ifndef REDUCE_H
#define REDUCE_H

#include "../MInstCalculator.h"

namespace JCore {
namespace Calculate {
namespace Reduce {
Handler Lookup(Opcode op);
bool CalculateReduce(MInst &inst);
} // namespace Reduce
} // namespace Calculate
} // namespace JCore
#endif