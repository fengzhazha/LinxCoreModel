#pragma once

#ifndef CALCULATE_MINST_H
#define CALCULATE_MINST_H

#include <unordered_map>
#include "../MInst.h"
#include "ISACommon/InstGroup.h"

namespace JCore {
namespace Calculate {

using Handler = bool (*)(MInst& inst);

class MInstCalculator {
public:
    static MInstCalculator Inst();

    ~MInstCalculator() = default;
    bool CalculateMinst(MInst& inst) const;

private:
    MInstCalculator();
    Handler GetHandler(InstGroup grp, MInst& inst) const;

    std::unordered_map<InstGroup, Handler> m_handlers;
};

} // namespace Calculate
} // namespace JCore
#endif
