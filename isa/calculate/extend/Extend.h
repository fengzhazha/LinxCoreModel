#pragma once

#ifndef EXTEND_H
#define EXTEND_H

#include "../MInstCalculator.h"

namespace JCore {
namespace Calculate {
namespace Extend {
Handler Lookup(Opcode op);
bool CalculateExtend(MInst &inst);
} // namespace Extend
} // namespace Calculate
} // namespace JCore
#endif