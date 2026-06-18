#pragma once

#ifndef BRANCH_H
#define BRANCH_H

#include "../MInstCalculator.h"

namespace JCore {
namespace Calculate {
namespace Branch {
Handler Lookup(Opcode op);
bool CalculateBranch(MInst &inst);
} // namespace Branch
} // namespace Calculate
} // namespace JCore
#endif