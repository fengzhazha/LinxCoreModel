#pragma once

#include <cstdint>
#include <deque>
#include <iostream>
#include <vector>

#include "core/Bus.h"
#include "core/Packet.h"
#include "lsu/lsu_stats.h"
#include "lsu/store_unit/stq.h"
#include "ModelSpec.h"
#include "SimSys.h"

#define STQ_STALL_THR 500
#define STQ_COMMIT_STALL 3

namespace JCore {

class LoadStoreUnit;

class StoreUnit : public SimObj {
private:
    STQ                                     stq;
    // Tile store resolve queue
    std::deque<MemReqBus>                        tsrq;
    /* when block commited, start to checkCommit */
    IDBus                                   lastId;
public:
    StoreUnit() {}
    virtual ~StoreUnit() {}

    LoadStoreUnit                           *top = NULL;
    SimSys                                  *sim = NULL;
    Core                                    *core = NULL;
    LSUConfig                               *pConfigs = NULL;
    uint64_t                                stq_stall_cyc = 0;
    uint32_t                                stid = -1U;

    INPUT std::vector<SimQueue<MemReqBus>*>     iexLsuStaArray;
    INPUT std::vector<SimQueue<MemReqBus>*>     iexLsuStdArray;
    /* from LU */
    INPUT std::deque<MemReqBus>*                 lookup_lu_su_q;
    INPUT std::deque<MemReqBus>*                 wait_lu_su_q;
    INPUT SimQueue<MemReqBus>*              atomic_lu_su_q;
    /* from BCC */
    INPUT SimQueue<BlockCommandPtr>*        bccTMABlockCmdQ;
    OUTPUT SimQueue<BlockCommandPtr>*        tmaBCCWakeupQ;
    INPUT SimQueue<IDBus>*                  bccLsuTstoreCommitQ;
    /* to LU */
    OUTPUT std::deque<MemReqBus>*                lookup_su_lu_q;
    OUTPUT std::deque<MemReqBus>*                wait_su_lu_q;
    OUTPUT std::deque<MemReqBus>*                bypass_su_lu_q;
    OUTPUT std::deque<MemReqBus>*                detect_su_lu_q; // load/store conflict detect
    OUTPUT std::deque<MemReqBus>*                wakeup_su_lu_q;
    OUTPUT std::deque<MDBBus>*                   lookup_mdb_su_q;
    /* to L1 */
    OUTPUT std::vector<SimQueue<MemReqBus>*>     commit_su_scb_array;
    OUTPUT std::vector<SimQueue<MemReqBus>*>    lsuBridgeTstoreArray;
    /* Singnal for store window slides */
    OUTPUT std::deque<uint64_t>                  lsu_iex_sid_q;

    //------------------------------
    // Store Pipeline
    //------------------------------
    void                                    store();
    bool                                    CheckFlushStall(ROBID &oldestBID);
    bool                                    checkCommit(IDBus &commitBus);
    void                                    commit(IDBus &commitBus);
    void                                    checkRetired(void); // retire_cmt_q
    void                                    checkConflict(void); //  lsConflictCheck
    void                                    handleLoadReq();
    void                                    GetCommitID();
    void                                    checkDeadLock();
    /* insert in LDQ when not pick */
    bool                                    insertStq(MemReqBus &req);
    void                                    mdbCheck(void);

    void                                    PickTStore();

    /* Check/Stats Interface */
    /* check if stall */
    bool                                    checkStall(); //  store_stall/update_state
    bool                                    checkStall(uint32_t size);

    /* Basic Interface */
    void                                    Reset(void);
    void                                    Work(void);
    void                                    Xfer(void);
    void                                    Build(void);
    void                                    flush(FlushBus flushReq);
    SimSys                                  *GetSim(void);
    void ReportStat() override {}
};

} // namespace JCore