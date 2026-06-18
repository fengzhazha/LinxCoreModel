#pragma once

#include <cstdint>
#include <deque>
#include <iostream>
#include <vector>

#include "core/Bus.h"
#include "core/Packet.h"
#include "mtccore/lsu/mtc_lsu_stats.h"
#include "mtccore/lsu/store_unit/MtcSTQ.h"
#include "ModelSpec.h"
#include "SimSys.h"

#define STQ_STALL_THR 500
#define MTC_STQ_COMMIT_STALL 3

namespace JCore {

class MtcLoadStoreUnit;

class MtcStoreUnit : public SimObj {
public:
    MtcStoreUnit() {}
    virtual ~MtcStoreUnit() {}

    MtcLoadStoreUnit                           *top = NULL;
    SimSys                                  *sim = NULL;
    Core                                    *core = NULL;
    mLSUConfig                               *pConfigs = NULL;
    uint64_t                                stq_stall_cyc = 0;

    /* from LU */
    INPUT std::deque<MemReqBus>*                 lookup_lu_su_q;
    INPUT std::deque<MemReqBus>*                 wait_lu_su_q;
    INPUT SimQueue<MemReqBus>*              atomic_lu_su_q;
    /* to LU */
    OUTPUT std::deque<MemReqBus>*                lookup_su_lu_q;
    OUTPUT std::deque<MemReqBus>*                wait_su_lu_q;
    OUTPUT std::deque<MemReqBus>*                bypass_su_lu_q;
    OUTPUT std::deque<MemReqBus>*                detect_su_lu_q; // load/store conflict detect
    OUTPUT std::deque<MemReqBus>*                wakeup_su_lu_q;
    OUTPUT std::deque<MDBBus>*                   lookup_mdb_su_q;

    /* to L1 */
    OUTPUT std::vector<SimQueue<MemReqBus>*>     commit_su_scb_array;

    /* Singnal for store window slides */
    std::deque<uint64_t>                         lsu_iex_sid_q;

    // ------------------------------
    // Store Pipeline
    // ------------------------------
    void                                    store(void);
    bool                                    CheckFlushStall(ROBID &oldestBID);
    bool                                    checkCommit(IDBus &commitBus);
    void                                    commit(IDBus &commitBus);
    void                                    checkRetired(void); // retire_cmt_q
    void                                    checkConflict(void); //  lsConflictCheck
    void                                    handleLoadReq();
    void                                    getCommitID();
    void                                    checkDeadLock();
    /* insert in LDQ when not pick */
    bool                                    insertStq(MemReqBus &req);
    void                                    mdbCheck(void);

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
private:
    MtcSTQ                                     stq;
    /* when block commited, start to checkCommit */
    IDBus                                   lastId;
};

} // namespace JCore