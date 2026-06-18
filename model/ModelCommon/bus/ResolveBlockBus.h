#pragma once

#ifndef RESOVLE_BLOCK_BUS_H
#define RESOVLE_BLOCK_BUS_H

#include <cstdint>

#include "../ROBID.h"
#include "../ModelEnumDefines.h"

namespace JCore {

struct ROBIDBus {
    bool vld = false;
    ROBID id;
};

struct IDBus {
    bool                    vld = false;
    bool                    isLoadStore;
    bool                    isLoad;
    bool                    isTld;
    bool                    first = false;
    ExecEngineTyp           iexTyp = SCALAR_IEX;
    ROBID                   bid;
    ROBID                   nonFlushBid;
    ROBID                   gid;
    ROBID                   rid;
    uint32_t                tid = -1U;
    uint32_t                stid = -1U;
    ROBID                   lsID;
    uint64_t                lid = 0;
    uint64_t                sid = 0;
    uint32_t                peID = 0;
    uint32_t                coreId = 0;
    uint32_t                pipeID = -1U;
    uint64_t                tpc = 0;
    uint32_t                fbid = 0;
    uint32_t                fbid_local = 0;
    uint32_t                busId = 0;
    ROBID                   tSeq;
    ROBID                   uSeq;
    ROBID                   predSeq;
};

/* bus from IEX to BlockEOB */
struct ResolveBlockBus {
    IDBus idBus;
    bool misPred;
    bool resolveTaken;
    uint64_t resolveTarget;
};

}
#endif