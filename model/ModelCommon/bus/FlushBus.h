#pragma once

#ifndef FLUSH_BUS_H
#define FLUSH_BUS_H

#include <cstdint>

#include "../ROBID.h"
#include "../ModelEnumDefines.h"
#include "../ModelCommon/SimInstInfo.h"
#include "../ModelCommon/bus/MemReqBus.h"

namespace JCore {
/* Flush request */
struct FlushReq {
    bool                    vld = false;
    FlushType               type;
    /* for LinxCore, multi PEs, for Lightcore multi lanes */
    uint32_t                peID = 0;
    uint32_t                coreId = 0;
    /* Thread ID */
    uint32_t                tid = 0;
    uint32_t                stid = -1U;
    /* Block ROB ID */
    ROBID                   bid;
    ROBID                   gid;
    /* UInst ROB ID */
    ROBID                   rid;

    ROBID                   lsID;
    ExecEngineTyp           iexTyp = SCALAR_IEX;
    ShapeLoopInfo               shapelpinfo;
    uint32_t                    logicalGID = 0;
    bool                    fetchBpcVld;
    uint64_t                fetchBpc;
    /* \brief next fetch TPC of flush/replay */
    bool                    fetchTPCVld = false; // Switch of fetchTPC
    uint64_t                fetchTPC = 0;
    /* for nuke-flush in lsu */
    uint64_t                memId = 0;
    /* \brief storeId,用于恢复storeId(LSU检测load store乱序就靠sid+ldid) */
    uint64_t                sid = 0;
    /* \brief loadId,用于恢复loadId(LSU检测load store乱序就靠sid+ldid) */
    uint64_t                ldid = 0;
    /* 精准回退用的,每条指令需要恢复当前块的setMask使用情况,为Codetemplate补齐做准备 */
    uint16_t                reservedSetMask = 0;
    /* \Determine whether it is the first instruction */
    bool                    firstInst = false;
    uint32_t                fbid = 0;
    uint32_t                fbid_local = 0;
    bool                    noSplitBlk = true;
    bool                    immediateFlush = false;
    bool                    maskVld = false;
    uint64_t                simtMask = 0;
    ROBID                   tSeq;
    ROBID                   uSeq;
    ROBID                   predSeq;
    FlushReq()
    {
        vld = false;
        type = FlushType::MISS_PRED_FLUSH;
        peID = -1;
        tid = -1U;
        bid = ROBID();
        gid = ROBID();
        rid = ROBID();
        lsID = ROBID();
        fetchBpcVld = false;
        fetchBpc = true;
        fetchTPCVld = false;
        fetchTPC = 0;
    }
};

/* Flush request bus */
struct FlushBus {
    struct FlushReq         req;
    bool                    baseOnBid = false;
    bool                    baseOnGroup = false;
    // Flush/replay base on peID.
    bool                    baseOnPE = false;
    bool                    baseOnThread = false;
    bool                    simtReplay = false;
    bool                   mtcReplay = false;
    // The end position of flushing(DFX).
    uint64_t                dTag = -1U;
    ROBID                   old_alloc;

    FlushBus() { req = FlushReq(); baseOnBid = false; baseOnPE = false; }
    FlushBus(FlushReq flushReq) { FlushBus(); req = flushReq; }

    bool match(MemReqBus bus) {
        if (bus.stid != this->req.stid) {
            return false;
        }
        if (this->baseOnPE && this->req.peID != bus.peID) {
            return false;
        }
        if (this->baseOnThread && this->req.tid != bus.tid) {
            return false;
        }
        if (this->baseOnBid) {
            return LessEqual(this->req.bid, bus.bid);
        }
        if (this->baseOnGroup) {
            if (LessEqual(this->req.bid, bus.bid)) {
                return true;
            }
            return LessEqual(this->req.bid, this->req.gid, this->req.lsID, bus.bid, bus.gid, bus.lsID);
        }
        return LessEqual(this->req.bid, this->req.lsID, bus.bid, bus.lsID);
    }
};
}

#endif