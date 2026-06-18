#pragma once

#include <cstdint>
#include <deque>
#include <iostream>
#include <vector>

#include "../configs/mlsu_config.h"
#include "core/Bus.h"
#include "core/Packet.h"
#include "mtccore/lsu/load_unit/ldq_cluster.h"
#include "mtccore/lsu/lsu_interface.h"
#include "mtccore/lsu/mtc_lsu_stats.h"
#include "ModelSpec.h"
#include "SimSys.h"

#define UNEXPECTED_DELAY 3
#define LDQ_STALL_THR 500
#define OLD_COMMIT_WAIT_CYC 3

namespace JCore {


class MtcLoadStoreUnit;

/* MtcPickEntry include n entry */
struct MtcPickEntry {
    std::vector<PMTCLUEntryInfo>               entryArr;
    void                                    Build(uint32_t depth);
    void                                    Reset(void);
};

/* MtcResolveQ include n load request */
struct MtcResolveQ {
    std::vector<MemReqBus>                  entryArr;
    uint32_t                                capcity = 0;
    uint32_t                                rslvNum = 0;

    void                                    Reset(void);
    bool                                    empty(void);
    bool                                    Full(void);
    bool                                    Full(MemReqBus &bus);
    void                                    insert(MemReqBus &bus);
    uint32_t                                retired(IDBus &commitBus, LSUType typeId);
    void                                    Build(uint32_t depth, uint32_t rslvNum);
    MemReqBus                               detect(MemReqBus &bus);
    void                                    flush(FlushBus &flushReq);
    MtcLoadStoreUnit                        *top = nullptr;
};

struct MtcCrossBuffer {
    std::deque<MemReqBus>                   crossQ;
    std::deque<MemReqBus>                   completed;

    void                                    Reset(void);
    void                                    flush(FlushBus &flushReq);
};

/* LDQ include n cluster */
class MtcLDQInfo : public SimObj {
public:
    MtcLDQInfo() {}
    virtual ~MtcLDQInfo() {}

    /* basic info */
    MtcLoadStoreUnit                         *top = nullptr;
    SimSys                                   *sim = NULL;
    mLSUConfig                               *pConfigs = NULL;
    bool                                     *pref_throw = NULL;

    /* from perfetcher */
    INPUT SimQueue<MemReqBus>*              pref_lu_q;
    /* to perfetcher */
    OUTPUT SimQueue<MemReqBus>*             lu_pref_q;
    OUTPUT SimQueue<PtrPrefFB>*             feedback_lu_pref_q;
    OUTPUT SimQueue<MemReqBus>*             atomic_lu_su_q;
    /* from SU */
    INPUT std::deque<MemReqBus>*            lookup_su_lu_q;
    // TODO: lookup stq for speculativly load pick
    INPUT std::deque<MemReqBus>*            wait_su_lu_q;
    INPUT std::deque<MemReqBus>*            detect_su_lu_q; // load/store conflict detect
    INPUT std::deque<MemReqBus>*            bypass_su_lu_q;
    INPUT std::deque<MemReqBus>*            wakeup_su_lu_q;
    /* to SU */
    OUTPUT std::deque<MemReqBus>*           lookup_lu_su_q;
    OUTPUT std::deque<MemReqBus>*           wait_lu_su_q;

    /* to MDB */
    OUTPUT std::deque<MDBBus>*              lookup_lu_mdb_q;
    OUTPUT std::deque<MDBBus>*              delete_lu_mdb_q;
    OUTPUT std::deque<MDBBus>*              record_lu_mdb_q;

    /* from L1 */
    INPUT std::deque<MemReqBus>*            tag_l1_lu_q;
    INPUT std::deque<MemReqBus>*            lookup_l1_lu_q;
    INPUT std::deque<MemReqBus>*            tag_scb_lu_q;
    INPUT std::deque<MemReqBus>*            lookup_scb_lu_q;
    INPUT std::deque<MDBBus>*               lookup_mdb_lu_q;
    INPUT SimQueue<MemReqBus>*              upgrade_l1_lu_q;
    INPUT SimQueue<PtrPacket>*              wakeup_l1_lu_q; // wakeup load and snoop
    INPUT SimQueue<MemReqBus>*              wakeup_scb_lu_q;
    // INPUT std::deque<MemReqBus>*         snoop_l1_lu_q;
    INPUT std::deque<MemReqBus>*            pref_l1_lu_q;
    INPUT std::deque<CommitInfo>*           scLdqOrder;
    /* to L1 */
    OUTPUT std::vector<std::deque<MemReqBus>*> pref_lu_l1_array;
    OUTPUT std::vector<std::deque<MemReqBus>*> tag_lu_l1_array;
    OUTPUT std::vector<std::deque<MemReqBus>*> lookup_lu_l1_array;
    OUTPUT std::vector<std::deque<MemReqBus>*> tag_lu_scb_array;
    OUTPUT std::vector<std::deque<MemReqBus>*> lookup_lu_scb_array;

    /* to L2 */
    SimQueue<PtrPacket>*                    snoop_lu_l2_q;
    SimQueue<PtrPacket>*                    lookup_lu_l2_q;

    // ------------------------------
    // LDQ Pipeline
    // ------------------------------
    /* E1 */
    void                                    queryByState(void);
    /* E2 */
    /* insert in LDQ when not pick */
    void                                    insert(void);
    /* Upgrade the tag in LDQ */
    void                                    mergeStateInfo(void);
    /* Pick the Oldest load request in new request and LDQ */
    void                                    pick(void);
    /* lookup in L1/L2/SCB/CB/STQ */
    void                                    lookup(void);

    /* E3 */
    /* merge data from L1/SCB/CB/STQ */
    void                                    receiveData(void);
    void                                    lsuExecEngine(MemReqBus &bus);
    /* check for cancel */
    void                                    checkCancel(void);
    /* E4, data in IEX */

    /* if the load is oldest, then delete from resolveQ */
    void                                    checkCommit(void); // getCommitFromPE
    void                                    prefetch(void);
    void                                    conflictDetect(void);
    /* use for load-to-use */
    void                                    statsMemBound(bool& anyload, bool& l1miss, bool& l2miss);

    void                                    handleStateQuery(MemReqBus &bus);
    void                                    handleInsert(MemReqBus &bus);
    bool                                    checkLdqHit(uint32_t cID, MemReqBus &bus);
    void                                    updateHitInfo(MemReqBus &bus);
    bool                                    checkDataPosionValid(uint64_t addr, uint64_t size, ReqData256 cData);
    void                                    updateSCBHitInfo(MemReqBus &bus);
    void                                    updateMDBInfo(MDBBus &bus);
    void                                    updateWaitInfo(MemReqBus &bus);
    void                                    pickL1(uint32_t cID);
    void                                    loadRepick(PMTCLUEntryInfo &e, uint32_t &idx);
    void                                    pickL2(uint32_t cID);
    void                                    handleL1Lookup(MemReqBus &bus);
    void                                    handleL2Lookup(MemReqBus &bus);
    void                                    handleL1Receive(MemReqBus &bus);
    void                                    handleL1Upgrade(MemReqBus &bus);
    void                                    handleSCBReceive(MemReqBus &bus);
    void                                    handleBypass(MemReqBus &bus);
    void                                    handleSTQReceive(MemReqBus &bus);
    void                                    handleMerge(MemReqBus &bus);
    bool                                    returnData(MemReqBus &bus, uint32_t iexIdx);
    void                                    processCrossRtn(MemReqBus &rtnBus);
    bool                                    sendCrossRtn(uint32_t pe, uint32_t iexIdx);
    void                                    handleCancel(MemReqBus &mem);
    void                                    sendMemPkt(PtrPacket &pkt, bool resp);
    void                                    sendMemReq(PtrPacket &pkt);
    uint64_t                                addr2LDQcID(uint64_t addr);
    void                                    handleDetect(MemReqBus &bus);
    void                                    handleSUWakeup(MemReqBus &bus);
    void                                    handleSCBWakeup(MemReqBus &bus);
    void                                    handleL1Wakeup(PtrPacket pkt);
    void                                    waitStore(MemReqBus &bus);
    void                                    checkLoadPending(void);
    void                                    LDQWakup(void);
    void                                    handleFlush(MemReqBus &confbus, MemReqBus &stBus);
    bool                                    checkStall();
    void                                    feedbackPref(PtrPacket pkt);
    void                                    checkDeadLock();
    bool                                    checkCltStall(uint64_t addr, uint32_t width);
    uint32_t                                retire(IDBus &commitBus);
    PMTCLUEntryInfo                            findOldestLoad(void);
    void                                    LUStatsTick(void);
    void                                    CheckMovRslvQ();
    bool                                    checkFlushStall(ROBID &oldestBid);
    bool                                    LDQRetired(IDBus &oldCmt, ROBID &oldestId, ROBID &oldestBID);
    bool                                    sendMissReqToL2(uint64_t tag);
    bool                                    PickPriority(PMTCLUEntryInfo &pickE, MTCLUEntryInfo &e, LSUType type);
    /* Basic Interface */
    void                                    Reset(void);
    void                                    Work(void);
    void                                    Xfer(void);
    void                                    Build(void);
    void                                    flush(FlushBus &flushReq);
    SimSys                                  *GetSim(void);
    void ReportStat() override {}
    void                                    LDQCheckDeadLockSendFlushReq(IDBus& oldRetire, bool isImmFlush);
private:
    std::vector<MtcClusterInfo>                clusters;

    /* Cross CacheLine(n PE) */
    std::vector<MtcCrossBuffer>                crossBuffer;
    MtcPickEntry                               pL1LookupEntries;
    MtcPickEntry                               pL2LookupEntries;
    /* data response but need to record, because maybe load/store conflict */
    MtcResolveQ                                rslvQ;
    /* when blocks has retired, then start to check commit */
    IDBus                                   lastId;
    IDBus                                   lastGid;

    /* Wait for L2 response */
    std::unordered_map<uint64_t, PtrPacket> missQ;
    std::set<uint64_t>                      prefSet;
    std::vector<PtrPacket>                  ldqPkts;
    bool                                    loadPending = false;
    uint64_t                                loadPendingCyc = 0;
    uint64_t                                ldqStallCyc = 0;
    uint32_t                                ldqCommitStallCyc = 0;
};

} // namespace JCore
