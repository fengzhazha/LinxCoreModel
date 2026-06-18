#pragma once

#ifndef CONVERT_H
#define CONVERT_H

#include "../MInstCalculator.h"

namespace JCore {
namespace Calculate {
namespace Convert {
Handler Lookup(Opcode op);
bool CalculateConvert(MInst &inst);
} // namespace Convert
} // namespace Calculate
} // namespace JCore
#endif