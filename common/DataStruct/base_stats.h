#pragma once

#include <cstring>  // uint64_t

#include "utils/reporter.h"

namespace JCore {
namespace NS_PLAT {

class BaseStats {
public:
    Reporter *rpt;
    BaseStats() : rpt(nullptr){};
    virtual ~BaseStats() = default;
    explicit BaseStats(Reporter *r) : rpt(r){};
    virtual void Reset(size_t size) { memset(reinterpret_cast<uint8_t*>(this) + 2 * sizeof(rpt), 0, size - 2 * sizeof(rpt)); }
    virtual void report() {};
};

}

} // namespace JCore