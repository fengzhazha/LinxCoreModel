#pragma once

#ifndef CACHE_MAINTAIN_H
#define CACHE_MAINTAIN_H

#include "../MInstCalculator.h"

namespace JCore {
namespace Calculate {
namespace CacheMaintain {
Handler Lookup(Opcode op);
bool CalculateCacheMaintain(MInst &inst);
} // namespace CacheMaintain
} // namespace Calculate
} // namespace JCore
#endif