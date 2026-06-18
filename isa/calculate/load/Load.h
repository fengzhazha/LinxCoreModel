#pragma once

#ifndef LOAD_H
#define LOAD_H

#include "../MInstCalculator.h"

namespace JCore {
namespace Calculate {
namespace Load {
Handler Lookup(Opcode op);
bool CalculateLoad(MInst &inst);
} // namespace Load
} // namespace Calculate
} // namespace JCore
#endif