#pragma once

#ifndef MOV_H
#define MOV_H

#include "../MInstCalculator.h"

namespace JCore {
namespace Calculate {
namespace Mov {
Handler Lookup(Opcode op);
bool CalculateMov(MInst &inst);
} // namespace Mov
} // namespace Calculate
} // namespace JCore
#endif
