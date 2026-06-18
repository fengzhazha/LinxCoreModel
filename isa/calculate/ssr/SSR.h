#pragma once

#ifndef SSR_H
#define SSR_H

#include "../MInstCalculator.h"

namespace JCore {
namespace Calculate {
namespace SSR {
Handler Lookup(Opcode op);
bool CalculateSSR(MInst &inst);
} // namespace SSR
} // namespace Calculate
} // namespace JCore
#endif