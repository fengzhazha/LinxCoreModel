#pragma once

#include <cstdint>
#include <deque>
#include <iostream>
#include <vector>

#include "core/Bus.h"
#include "mtccore/lsu/mtc_lsu_stats.h"
#include "ModelSpec.h"
#include "SimSys.h"

namespace JCore {

enum MTCLDQ_FSM {
    MTC_LDQ_IDLE,
    MTC_LDQ_WAIT,
    MTC_LDQ_REPICK,
    MTC_LDQ_MULTI_HIT,
    MTC_LDQ_L1_DC_MISS,
    MTC_LDQ_L2_WAIT,
    MTC_LDQ_RESOLVED
};

struct MTCLUEntryInfo {
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
    uint32_t                                fsm = MTC_LDQ_IDLE;

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
typedef MTCLUEntryInfo *PMTCLUEntryInfo;

struct MtcDataField {
    uint2048_t                               data;
    uint64_t                                tag = 0;
    bool                                    vld = false;

    void                                    Reset();
};

class MtcClusterInfo;
struct MtcClusterData {
    MtcClusterInfo                             *top;
    std::vector<MtcDataField>                  dField;
    uint32_t                                replaceIdx = 0;

    void                                    replace(uint64_t t, ReqData256 &d);
    void                                    merge(uint64_t t, ReqData256 &d);
    uint2048_t                               getData(uint64_t t);
    bool                                    lookup(uint64_t t);
    void                                    Reset(void);
    void                                    Build(uint32_t bufferSize);
};

/* Cluster include n entry */
struct MtcClusterInfo {
    std::vector<MTCLUEntryInfo>                entryArr;
    bool                                    vld = false;
    uint32_t                                size = 0;
    MtcClusterData                             cData;

    void                                    Reset(void);
    void                                    Build(uint32_t clusterDepth, uint32_t cID, uint32_t dataSize);
    void                                    setLdqHitInfo(uint64_t tag, bool hit);
    bool                                    checkHit(uint64_t tag);
    bool                                    IsIteratorEnding(uint32_t it);
};

} // namespace JCore