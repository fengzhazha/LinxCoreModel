#pragma once

#include <cstdint>
#include <deque>
#include <iostream>
#include <vector>

#include "core/Bus.h"
#include "lsu/lsu_stats.h"
#include "ModelSpec.h"
#include "SimSys.h"

namespace JCore {

enum LDQ_FSM {
    LDQ_IDLE,
    LDQ_WAIT,
    LDQ_REPICK,
    LDQ_MULTI_HIT,
    LDQ_L1_DC_MISS,
    LDQ_L2_WAIT,
    LDQ_RESOLVED
};

struct LUEntryInfo {
    MemReqBus                               memReq;
    /* Attribute Array */
    /* \brief LDQ Data Hit */
    bool                                    ldqHit = false;
    /* \brief Data Cache Hit */
    bool                                    l1Hit = false;
    bool                                    storeBypass = false;
    bool                                    ldqRnt = false;
    bool                                    l1Rnt = false;
    bool                                    scbRnt = false;
    bool                                    stqRnt = false;
    /* \brief MDB Hit */
    bool                                    mdbHit = false;
    /* \brief Load Queue FSM */
    uint32_t                                fsm = LDQ_IDLE;

    /* \brief waiting in LDQ */
    /* \brief wait for store bid */
    ROBID                                   waitBid;
    bool                                    waitStore = false;
    bool                                    mdbStall = false;
    uint64_t                                waitStoreTpc = 0;
    uint64_t                                stallCycle = 0;

    /* \brief Use for cross Cacheline */
    bool                                    crossLine = false;
    int                                     hitCnt = 0;

    /* \brief ldq index(debug) */
    uint32_t                                cID = 0;
    uint32_t                                eID = 0;

    /* \brief ldq stall */
    uint64_t                                waitingStoreCycle = 0;
    uint64_t                                delayCnt = 0;

    void                                    insert(MemReqBus &bus);
    void                                    Reset(void);
    void                                    rewait(void);
    bool                                    IsWorking(void);
    bool                                    IsIdle();
    bool                                    dataHit();
};
typedef LUEntryInfo *PLUEntryInfo;

struct DataField {
    uint512_t                               data;
    uint64_t                                tag = 0;
    bool                                    vld = false;

    void                                    Reset();
};

class ClusterInfo;
struct clusterData {
    ClusterInfo                             *top;
    std::vector<DataField>                  dField;
    uint32_t                                replaceIdx = 0;

    void                                    replace(uint64_t t, ReqData &d);
    void                                    merge(uint64_t t, ReqData &d);
    uint512_t                               getData(uint64_t t);
    bool                                    lookup(uint64_t t);
    void                                    Reset(void);
    void                                    Build(uint32_t bufferSize);
};

/* Cluster include n entry */
struct ClusterInfo {
    std::vector<LUEntryInfo>                entryArr;
    bool                                    vld = false;
    uint32_t                                size = 0;
    clusterData                             cData;

    void                                    Reset(void);
    void                                    Build(uint32_t clusterDepth, uint32_t cID, uint32_t dataSize);
    void                                    setLdqHitInfo(uint64_t tag, bool hit);
    bool                                    checkHit(uint64_t tag);
    bool                                    IsIteratorEnding(uint32_t it);
};

} // namespace JCore