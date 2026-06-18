#pragma once

#include <cstdint>
#include <deque>
#include <iostream>
#include <vector>

#include "../configs/mlsu_config.h"
#include "core/Bus.h"
#include "mtccore/lsu/lsu_interface.h"
#include "mtccore/lsu/mtc_lsu_stats.h"
#include "ModelSpec.h"
#include "SimSys.h"

namespace JCore {

enum MTC_STQ_FSM {
    MTC_STQ_WAIT,
    MTC_STQ_COMMIT,
    MTC_STQ_MISS,
    MTC_STQ_L2_WAIT,
    MTC_STQ_IDLE,
    MTC_STQ_RESOLVED
};

class MtcStoreUnit;

struct MtcSTQueueEntryInfo {
    /* \brief Is the STQ Entry Valid */
    bool                                    vld = false;
    /* Attribute Array */
    MemReqBus                               memStReq;
    
    /* \brief Block ID of the LDQ Entry */
    ROBID                                   bid;
    /* \brief ROB ID of the LDQ Entry */
    ROBID                                   rid;
    /* \brief PE ID of the LDQ Entry */
    uint32_t                                peid = 0;
    /* \brief VA Array */
    /* \brief Data Size */
    uint32_t                                size = 0;
    /* \brief Virtual Address */
    uint64_t                                addr = 0;
    /* \brief Response data */
    uint64_t                                data = 0;
    /* \brief Status Array */
    bool                                    miss = false;
    /* \brief Store Queue FSM */
    uint32_t                                fsm = MTC_STQ_IDLE;
    /* \brief stq index */
    uint32_t                                StqIdx = 0;
    /* \brief stack mark */
    bool                                    stackVld = 0;
    bool                                    addrRdy = false;
    bool                                    dataRdy = false;

    void                                    Reset();
    void                                    init(MemReqBus &bus);
    bool                                    IsWorking()
    {
        return (fsm == MTC_STQ_WAIT);
    }
};

class MtcSTQ : public SimObj {
public:
    MtcSTQ() {}
    virtual ~MtcSTQ() {}

    MtcStoreUnit                               *top = NULL;
    SimSys                                  *sim = NULL;
    mLSUConfig                               *pConfigs = NULL;

    /* Singnal for store window slides */
    std::deque<uint64_t>                         *lsu_iex_sid_q;

    void                                    insert(MemReqBus &bus);
    void                                    free(uint32_t index);
    void                                    retire(IDBus &commitBus);
    void                                    commit();
    void                                    getData(MemReqBus &ldReq);
    void                                    checkWait(MemReqBus &ldReq);
    bool                                    full();
    bool                                    full(uint32_t sz);
    bool                                    stall();
    bool                                    Empty();
    bool                                    mergeStore(MemReqBus &bus);
    bool                                    isStqCmtable(ROBID oldestBID, IDBus commitBus, MtcSTQueueEntryInfo &e);
    MemReqBus                               mdbCheck(MemReqBus &bus);
    void                                    SUStats_tick(void);
    MemReqBus                               checkDeadLock(IDBus &retireID);
    void                                    lookupForLoad(MemReqBus &ldReq, bool needData);
    
    /* Basic Interface */
    void                                    Reset(void);
    void                                    Work(void);
    void                                    Xfer(void);
    void                                    Build(uint32_t clusterDepth);
    void                                    flush(FlushBus &flushReq);
    SimSys                                  *GetSim(void);
    void ReportStat() override {}

private:
    std::vector<MtcSTQueueEntryInfo>           stEntry;
    uint32_t                                size = 0;
    uint32_t                                osdSize = 0;
    std::deque<uint32_t>                    storeCommitQ;
};

typedef std::shared_ptr<MtcSTQ> MtcPSTQueueEntry;

} // namespace JCore