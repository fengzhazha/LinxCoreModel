#pragma once

#ifndef PE_RESOLVE_BUS
#define PE_RESOLVE_BUS

#include <vector>
#include <string>

#include "ISA.h"
#include "../ROBID.h"
#include "../CycleInfo.h"

namespace JCore {
struct DataDstInfo {
    bool                    dataOut0_vld = false;
    uint64_t                dataOut0 = 0;
    bool                    dataOut1_vld = false;
    uint64_t                dataOut1 = 0;
    void Reset()
    {
        dataOut0_vld = false;
        dataOut0 = 0;
        dataOut1_vld = false;
        dataOut1 = 0;
    }
};

struct DIMResolveInfo {
    SystemReg               sysID = SystemReg::SYS_INVALID;
    uint64_t                sysData = 0;
    BIQType                 biqType = BIQType::NONE_IQ;
    bool                    force = false; // Use for SIMT loop reg upgrading
};

struct PEResolveBus {
    uint32_t                peid = -1U;
    uint32_t                coreId = 0;
    ROBID                   bid;
    ROBID                   rid;
    uint32_t                stid = -1U;
    ROBID                   gid;
    ROBID                   subrid;
    uint32_t                tid = 0;
    bool                    isPipeStore; // store complete in pipe
    bool                    isIssue = false;
    bool                    isComplete = false;
    bool                    isLDCancel = false; // for load cancel
    bool                    isSrcRead = false; // for VRF pre-release
    bool                    stack; // for stack load check
    CycleBus                pipe_cycle;
    std::string             iq_name; // used in pipeview
    std::vector<DataDstInfo> lanes;
    bool                    srcMVld = true;
    uint64_t                simtMask = 0;
    std::vector<POperandPtr> dsts;
    bool                    needSimtResolve = false;
    bool                    need2TileRename = false;
    DIMResolveInfo          dimRslvInfo;
    std::shared_ptr<AaccelssMemInfo> accMemInfo = nullptr;
    std::shared_ptr<AtomicInfo> atomicInfo = nullptr;
    std::shared_ptr<SetcInfo> setcInfo = nullptr;
    std::shared_ptr<SSRSetInfo> ssrInfo = nullptr;
    std::shared_ptr<ReduceInfo> reduceInfo = nullptr;
    std::shared_ptr<BranchInfo> brInfo = nullptr;

    void Reset() {
        peid = -1U;
        isPipeStore = false;
        isIssue = false;
        isComplete = false;
        stack = false;
        pipe_cycle = std::make_shared<CycleInfo>();
        for (uint32_t l = 0; l < lanes.size(); ++l) {
            Reset();
        }
    }
    PEResolveBus() { Reset(); }
};
}
#endif