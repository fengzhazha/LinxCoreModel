#pragma once

#ifndef SETC_H
#define SETC_H

#include "../MInstCalculator.h"

namespace JCore {
namespace Calculate {
namespace Setc {
Handler Lookup(Opcode op);
bool CalculateSetc(MInst &inst);
} // namespace Setc
} // namespace Calculate
} // namespace JCore
#endif