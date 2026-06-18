#pragma once

#ifndef PE_ROB_STATUS_H
#define PE_ROB_STATUS_H

#include <array>

#include "core/Bus.h"
#include "ModelCommon/ROBID.h"
#include "ModelCommon/SimInstInfo.h"

namespace JCore {
/* PE Reorder Buffer Status */
enum PROBStatus {
    INST_FREE = 0,
    INST_ALLOCATED,
    INST_RENAMED,
    INST_ISSUED,
    INST_COMPLETED,
    INST_RETIRED,
    INST_FAULT,
    INST_NEEDFLUSH,
    INST_STATUS_NUM
};

inline const std::array<std::string, static_cast<std::size_t>(PROBStatus::INST_STATUS_NUM)>& PROBStatusNames()
{
    static const std::array<std::string, static_cast<std::size_t>(PROBStatus::INST_STATUS_NUM)> PSTATUS_NAMES = {{
        "free",         // INST_FREE = 0,
        "alloc",        // INST_ALLOCATED,
        "renamed",      // INST_RENAMED,
        "issued",       // INST_ISSUED,
        "completed",    // INST_COMPLETED,
        "retired",      // INST_RETIRED,
        "fault",        // INST_FAULT,
        "need_flush"    // INST_NEEDFLUSH,
    }};
    return PSTATUS_NAMES;
}

inline const std::string& GetPROBStatus(PROBStatus type)
{
    const std::size_t idx = static_cast<std::size_t>(type);
    const auto& names = PROBStatusNames();
    static const std::string INVALID_PSTATUS_TYPE = "invalid_prob_status";
    if (idx >= names.size()) {
        return INVALID_PSTATUS_TYPE;
    }
    return names[idx];
}

/* PE Reorder Buffer Entry */
struct PROBEntry {
    bool                    vld = false;
    PROBStatus              status = INST_FREE;
    bool                    stack_complete = false;
    uint64_t                tpc = 0;
    ROBID                   bid;
    ROBID                   gid;
    ROBID                   rid;
    bool                    last = false;
    bool                    check = false;
    SimInst                 inst;       // debug only
    uint32_t                getMask = 0;
    uint32_t                setMask = 0;
    bool                    isMCALL = false;

    std::string Dump()
    {
        std::stringstream oss;
        oss << inst->Dump();
        oss << " "<< GetPROBStatus(status);
        return oss.str();
    }
};

}
#endif