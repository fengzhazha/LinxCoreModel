#pragma once

#ifndef OTHERS_H
#define OTHERS_H

#include "../MInstCalculator.h"

namespace JCore {
namespace Calculate {
namespace Others {
Handler Lookup(Opcode op);
bool CalculateOthers(MInst &inst);
} // namespace Others
} // namespace Calculate
} // namespace JCore
#endif