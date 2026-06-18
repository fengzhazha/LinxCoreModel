#pragma once

#ifndef RF_REQ_BUS
#define RF_REQ_BUS

#include "../SimInstInfo.h"
namespace JCore
{
struct RFReqBus {
    // 3 read, 2 write
    uint32_t                pipeid = -1U;
    uint32_t                peid = -1U;
    uint32_t                coreId = 0;
    ROBID                   bid;
    ROBID                   gid;
    ROBID                   rid;
    bool                    vld = false;
    uint64_t                sysID = -1UL;               // system register ID

    std::vector<POperandPtr> srcs;
    std::vector<POperandPtr> dsts;

    /* Thread id, default 0 */
    uint32_t                tid = 0;
    uint32_t                stid = -1U;

    RFReqBus() {}
    void Reset() {
        vld = false;
        srcs.clear();
        dsts.clear();
    }

    POperandPtr GetSrcOperandInfo(uint64_t idx)
    {
        if (idx >= srcs.size()) {
            return nullptr;
        }
        return srcs[idx];
    }

    POperandPtr GetDstOperandInfo(uint64_t idx)
    {
        if (idx >= dsts.size()) {
            return nullptr;
        }
        return dsts[idx];
    }
};
}
#endif