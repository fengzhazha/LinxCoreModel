#pragma once

#include <cstdint>
#include <deque>
#include <iostream>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../configs/l1_config.h"
#include "../emulator/Memory.h"
#include "core/Bus.h"
#include "core/Packet.h"
#include "l1/L1DCache.h"
#include "l1/L1DCache.h"
#include "l1/SCB.h"
#include "l1/SCB.h"
#include "lsu/lsu_stats.h"
#include "ModelSpec.h"
#include "SimSys.h"

#define WAIT_TIME 3

namespace JCore {

class L1Top;

class L1Clusters : public SimObj {
private:
    L1DCache                                dcache;
    SCBuffer                                scb;
    uint32_t                                wait = 0; // Check for locking by refilling

    /* if not coming and snooping, allow for LDQ issue */
    bool                                    dcacheAllow = true;
    /* if not snooping, allow for snooping */
    bool                                    snoopAllow = true;

public:
    /* basic info */
    uint32_t                                memID_s = 0;
    uint32_t                                cID = 0;
    L1Top                                   *top = NULL;
    SimSys                                  *sim = NULL;
    L1Config                                *pConfigs;
    std::shared_ptr<LSUStats>               stats;

    /* from LU */
    INPUT std::deque<MemReqBus>                  *pref_lu_l1_q;
    INPUT std::deque<MemReqBus>                  *tag_lu_l1_q;
    INPUT std::deque<MemReqBus>                  *lookup_lu_l1_q;
    INPUT std::deque<MemReqBus>                  *tag_lu_scb_q;
    INPUT std::deque<MemReqBus>                  *lookup_lu_scb_q;
    /* from SU */
    INPUT SimQueue<MemReqBus>               *commit_su_scb_q;
    /* from L2 */
    INPUT std::deque<PtrPacket>                  *resp_l2_l1_q;
    /* to l2 */
    OUTPUT std::vector<SimQueue<PtrPacket>*>             pkt_out_q;
    /* to LU */
    OUTPUT std::vector<SimQueue<MemReqBus>*>             wakeup_scb_lu_q;

    /* L1 cluster pipeline */
    void                                    queryByTag(void);
    void                                    lookupCache(void);
    void                                    lookupSCB(void);
    void                                    lookupCB(void);
    void                                    refillCache(void);
    void                                    snoop2L2(void);
    void                                    commitStore(void);

    /* auxiliary */
    void                                    allocCachePort(void);
    bool                                    hasDCacheReq(void);
    bool                                    dataAllow(void);
    void                                    handleTagQuery(MemReqBus &bus);
    void                                    handleDCacheLookup(MemReqBus &bus);
    void                                    handleSCBLookup(MemReqBus &bus, bool checkOnly);
    void                                    handleCBLookup(MemReqBus &bus);
    void                                    handleRefill(PtrPacket &pkt);
    void                                    handleSnoop(PtrPacket &pkt);
    void                                    broadcastUpgrade(uint64_t addr, uint512_t &data, bool *p);
    void                                    handlePreftch(MemReqBus&);

   /* Basic Interface */
    void                                    Reset(void);
    void                                    Work(void);
    void                                    Xfer(void);
    void                                    Build(void);
    SimSys                                  *GetSim(void);
    void ReportStat() override {}
};

} // namespace JCore