#pragma once

#ifndef STORE_H
#define STORE_H

#include "../MInstCalculator.h"

namespace JCore {
namespace Calculate {
namespace Store {
Handler Lookup(Opcode op);
bool CalculateStore(MInst &inst);
} // namespace Store
} // namespace Calculate
} // namespace JCore
#endif