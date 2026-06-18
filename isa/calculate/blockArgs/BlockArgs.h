#pragma once

#ifndef BLOCK_ARGS_H
#define BLOCK_ARGS_H

#include "../MInstCalculator.h"

namespace JCore {
namespace Calculate {
namespace BlockArgs {
Handler Lookup(Opcode op);
bool CalculateBlockArgs(MInst &inst);
} // namespace BlockArgs
} // namespace Calculate
} // namespace JCore
#endif