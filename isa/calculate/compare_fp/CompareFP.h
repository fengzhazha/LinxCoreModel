#pragma once

#ifndef COMPAREFP_H
#define COMPAREFP_H

#include "../MInstCalculator.h"

namespace JCore {
namespace Calculate {
namespace CompareFP {
Handler Lookup(Opcode op);
bool CalculateCompareFP(MInst &inst);
} // namespace CompareFP
} // namespace Calculate
} // namespace JCore
#endif