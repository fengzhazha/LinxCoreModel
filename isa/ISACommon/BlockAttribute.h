#pragma once

#ifndef BLOCK_ATTRIBUTE_H
#define BLOCK_ATTRIBUTE_H

#include <cstdint>
#include <sstream>
#include <vector>

#include "DataType.h"
#include "FRM.h"
#include "LayOut.h"

namespace JCore {

enum class PadValue {
    ZERO = 0,
    MAX,
    MIN,
    NONE,
    INVALID,
};

enum class CMode {
    EQ = 0,
    NE,
    LT,
    GT,
    LE,
    GE,
    INVALID,
};

inline std::string GetPadValueName(PadValue pad)
{
    switch (pad) {
        case PadValue::ZERO:
            return "zero";
        case PadValue::MAX:
            return "max";
        case PadValue::MIN:
            return "min";
        case PadValue::NONE:
            return "none";
        default:
            return "INVALID";
    }
}

class BlockAttribute {
public:
    bool                                    trap = false;
    bool                                    atomic = false;
    bool                                    aq = false;
    bool                                    rl = false;
    bool                                    far = false;
    LayOut                                  layout = LayOut::NORM;
    bool                                    canon = false; // false is normal
    bool                                    dimReduction = true; // default: reduce dimension
    bool                                    saturation = false;
    PadValue                                padValue = PadValue::NONE;
    CMode                                   cMode = CMode::INVALID;
    FRMMode                                 rMode = FRMMode::FRM_INV;
    DataType                                dataType = DataType::INVALID;
    std::string Dump()
    {
        std::vector<std::string> parts;
        auto addCondText = [&parts](bool cond, const std::string& text) {
            if (cond) {
                parts.push_back(text);
            }
        };
        addCondText(trap, "trap");
        addCondText(atomic, "atomic");
        addCondText(aq, "aq");
        addCondText(rl, "rl");
        addCondText(far, "far");
        addCondText(dimReduction, "DR");
        parts.emplace_back(GetLayOutName(layout) + "." + (canon ? "canon" : "normal"));
        parts.emplace_back(BriefDataType2String(dataType));
        parts.emplace_back(GetPadValueName(padValue));

        std::ostringstream oss;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) {
                oss << ", ";
            }
            oss << parts[i];
        }
        return oss.str();
    }
};
}
#endif
