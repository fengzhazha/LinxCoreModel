#pragma once

#ifndef BLOCK_HINT_H
#define BLOCK_HINT_H

#include <cstdint>
#include <string>
#include <sstream>

namespace JCore {

enum class BranchPrompt {
    UNLIKELY,
    LIKELY,
    INVALID,
};

inline std::string GetBRPromptName(BranchPrompt br)
{
    switch (br) {
        case JCore::BranchPrompt::UNLIKELY:
            return "unlikely";
        case JCore::BranchPrompt::LIKELY:
            return "likely";
        default:
            return "invalid";
    }
}

enum class Temperature {
    NONE,
    COOL,
    WARM,
    HOT,
};

inline std::string GetTempName(Temperature temp)
{
    switch (temp) {
        case JCore::Temperature::NONE:
            return "none";
        case JCore::Temperature::COOL:
            return "cool";
        case JCore::Temperature::WARM:
            return "warm";
        case JCore::Temperature::HOT:
            return "hot";
    }
    return "invalid";
}

class BlockHintInfo {
public:
    bool                valid = false;
    BranchPrompt        br = BranchPrompt::INVALID;
    Temperature         temp = Temperature::NONE;
    uint64_t            presize = 0;

    bool                hintTrace = false;
    bool                traceBegin = false;

    BlockHintInfo() = default;
    ~BlockHintInfo() = default;
    void InitPref(uint64_t vld, uint64_t flag, uint64_t temperature, uint64_t size)
    {
        valid = (vld == 1);
        br = static_cast<BranchPrompt>(flag);
        temp = static_cast<Temperature>(temperature);
        presize = size;
    }

    void InitTrace(uint64_t beginOrEnd)
    {
        hintTrace = true;
        traceBegin = (beginOrEnd == 0);
    }

    std::string Dump()
    {
        std::stringstream oss;
        if (hintTrace) {
            oss << "TRACE.";
            if (traceBegin) {
                oss << "begin";
            } else {
                oss << "end";
            }
        } else {
            oss << "BR.";
            oss << GetBRPromptName(br);
            oss << ", TEMP." << GetTempName(temp);
            oss << ", " << std::dec << presize;
        }
        return oss.str();
    }
};
}
#endif