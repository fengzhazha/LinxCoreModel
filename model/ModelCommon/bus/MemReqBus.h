#pragma once

#ifndef MEM_REQ_BUS_H
#define MEM_REQ_BUS_H

#include <cstdint>
#include <set>

#include "../ROBID.h"
#include "../ModelEnumDefines.h"
#include "ISA.h"
#include "../BlockCommand.h"
#include "TileBridgeBus.h"
#include "core/Packet.h"

namespace JCore {

/* Data bus format for memory request */
struct MemReqBus {
    bool                    vld = false;
    bool                    is_load = false;
    bool                    toMtcLsu = false;
    uint32_t                peID = 0;
    uint32_t                coreId = 0;
    Opcode                  opcode = Opcode::OP_INVALID;
    uint32_t                size = 0; // GM
    ROBID                   bid;
    ROBID                   gid;
    ROBID                   rid;
    uint64_t                instUid { -1ULL };
    ROBID                  subrid;
    uint32_t                tid = -1U;
    uint32_t                fbid = 0;
    uint32_t                fbid_local = 0;
    uint32_t                stid = -1U;
    uint64_t                addr = 0;
    bool                    data_vld = false;
    bool                    srcMVld = true;
    uint64_t                simtMask = 0;
    uint64_t                data = 0;
    ExecEngineTyp           iexTyp = SCALAR_IEX;
    ROBID                   lsID;       // debugging
    uint32_t                pipeID = 0; // Use for load-to-use.
    uint64_t                tpc = 0;        // debugging
    uint64_t                lsuRecvCycle = 0;   // PMU
    uint64_t                lsu_ret_cycle = 0;   // PMU
    uint64_t                sid = 0;              // BranchType::BSB real store id
    uint64_t                load_id = 0;
    uint64_t                ref_id = -1U;         // Store id of perfect load-store mode
    bool                    is_llsc = false;
    uint64_t                llsc = 0;
    bool                    prefetch = false;
    bool                    isCrossCacheLine = false;
    bool                    specWakeup = false;
    bool                    stack_vld = false;  // stack get or set
    uint32_t                delay = 0; // Use For load-to-use
    TileBridgeBus           tile;
    BlockCommandPtr         blockCmd = nullptr;

    uint64_t                ldqPickCycle = 0;       // debugging
    uint64_t                ldqIssueCycle = 0;      // debugging
    uint64_t                l1MissCycle = 0;        // debugging
    uint64_t                l2MissCycle = 0;        // debugging
    uint64_t                memRntCycle = 0;        // debugging
    uint64_t                l2RntCycle = 0;         // debugging
    uint64_t                l1RntCycle = 0;         // debugging
    uint64_t                bridgeRecvCycle = 0;
    uint64_t                bridgeReadTileCycle = 0;
    uint64_t                bridgeWriteTileCycle = 0;
    uint64_t                bridgeResolveCycle = 0;
    uint64_t                sendToScalperCycle = 0;
    uint64_t                genPrefetchCycle = 0;
    uint64_t                genLoadReadReqCycle = 0;
    uint64_t                loadDataReturnCycle = 0;
    uint64_t                prefetchDataRetCycle = 0;
    uint64_t                sendFromScalperCycle = 0;
    uint64_t                sendMemoryReqCycle = 0;
    std::string             iq_name;                  // debugging
    bool                    wait_store = false;
    uint64_t                wait_tpc = 0;
    ROBID                   wait_bid;
    ROBID                   wait_rid;
    bool                    ldq_miss = false;
    bool                    l1_miss = false;
    bool                    l2_miss = false;
    bool                    scb_miss = false;
    bool                    cb_miss = false;
    uint64_t                tag = 0;
    uint32_t                simtLane = 0;
    ReqData                 reqData;
    ReqData256              mtc_reqData;
    uint32_t                cID = 0;
    uint32_t                eID = 0;
    ROBID                   tSeq;
    ROBID                   uSeq;
    ROBID                   predSeq;

    bool                    intercept = false;
    bool                    noSplitBlk = true;
    uint32_t                realReqCnt = 0;
    std::set<uint32_t>      laneSet;
    /*
    *  store    | prefetch
    * 0: ST_ALL | 1 PFTYPE_STRIDE
    * 1: ST_ADDR| 2 PFTYPE_STREAM
    * 2: ST_DATA| 3 PFType_SW_L1I
    *             4 PFType_SW_L1D
    *             5 PFType_SW_L2
    * */
    uint32_t                type = 0;
    /* for load_xx atomic instructions */

    POperandPtr             src1;
    /* stores the registers to be woken up. */
    POperandPtr             dst1;
    bool                    tTransReq = false;
    uint32_t                tTransId = 0;
    uint8_t                 memData[256];
    bool                    IsTileLS()
    {
        return blockCmd != nullptr;
    }
    void                    CalcGMSize()
    {
        if (blockCmd != nullptr && blockCmd->IsTloadOrTstore()) {
            size = blockCmd->lb0 * blockCmd->lb1 * BytesOf(blockCmd->dataType);
        }
    }

    bool                    fromScalper = false;
    uint32_t                scalper_hit_num = 0;
};

struct ScbDrainBus {
    int                     tid;
    ROBID                   bid;
    uint32_t                stid;
    ROBID                   gid;
};

struct MDBBus {
    MemReqBus ldInfo;
    MemReqBus stInfo;
    int conf = false;
    bool hit = false; // hit in MDB
    bool vld = false;
    uint32_t stid = -1U;
};
}
#endif