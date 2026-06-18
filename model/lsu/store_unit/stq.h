#pragma once

#include <cstdint>
#include <deque>
#include <iostream>
#include <vector>

#include "../configs/lsu_config.h"
#include "core/Bus.h"
#include "lsu/lsu_interface.h"
#include "lsu/lsu_stats.h"
#include "ModelSpec.h"
#include "SimSys.h"

namespace JCore {

enum STQ_FSM {
    STQ_WAIT,
    STQ_COMMIT,
    STQ_MISS,
    STQ_L2_WAIT,
    STQ_IDLE,
    STQ_RESOLVED
};

class StoreUnit;

struct STQueueEntryInfo {
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
    uint32_t                                fsm = STQ_IDLE;
    /* \brief stq index */
    uint32_t                                StqIdx = 0;
    /* \brief stack mark */
    bool                                    stackVld = 0;
    bool                                    addrRdy = false;
    bool                                    dataRdy = false;

    void                                    Reset();
    void                                    init(MemReqBus &bus);
    bool                                    IsWorking() {
        return (fsm == STQ_WAIT);
    }
};

class STQ : public SimObj {
private:
    std::vector<STQueueEntryInfo>           stEntry;
    uint32_t                                size = 0;
    uint32_t                                osdSize = 0;
    std::deque<uint32_t>                    storeCommitQ;
public:
    STQ() {}
    virtual ~STQ() {}

    StoreUnit                               *top = NULL;
    SimSys                                  *sim = NULL;
    LSUConfig                               *pConfigs = NULL;

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
    bool                                    isStqCmtable(ROBID oldestBID, IDBus commitBus, STQueueEntryInfo &e);
    MemReqBus                               mdbCheck(MemReqBus &bus);
    void                                    SUStats_tick(void);
    MemReqBus                               checkDeadLock(IDBus &retireID);
    void                                    lookupForLoad(MemReqBus &ldReq, bool needData);
    MemReqBus                               PickTStore(uint32_t id);
    void                                    ResolveTstore(IDBus idBus);
    
    /* Basic Interface */
    void                                    Reset(void);
    void                                    Work(void);
    void                                    Xfer(void);
    void                                    Build(uint32_t clusterDepth);
    void                                    flush(FlushBus &flushReq);
    SimSys                                  *GetSim(void);
    void ReportStat() override {}
};

typedef std::shared_ptr<STQ> PSTQueueEntry;

} // namespace JCore