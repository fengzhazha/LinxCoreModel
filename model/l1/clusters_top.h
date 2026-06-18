#pragma once

#include <cstdint>
#include <deque>
#include <iostream>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../configs/lsu_config.h"
#include "../emulator/Memory.h"
#include "core/Bus.h"
#include "core/Packet.h"
#include "l1/cluster.h"
#include "l1/L1DCache.h"
#include "l1/SCB.h"
#include "lsu/lsu_stats.h"
#include "ModelSpec.h"
#include "SimSys.h"
#include "plat/DebugLog.h"

namespace JCore {

class Core;
class LoadStoreUnit;
class MtcLoadStoreUnit;

class L1Top : public SimObj {
private:
    std::vector<L1Clusters>                     clusters;

public:
    LoadStoreUnit                           *top = nullptr;
    MtcLoadStoreUnit                        *mtctop = nullptr;
    /* basic info */
    uint32_t                                memID_s = 0;
    Core                                    *core = NULL;
    SimSys                                  *sim = NULL;
    L1Config                                configs;
    std::shared_ptr<DebugLog>               debugLogger = nullptr;
    LSUConfig                               *lsuConfigs = nullptr;

    /* from LU */
    INPUT std::vector<std::deque<MemReqBus>*> pref_lu_l1_array;
    INPUT std::vector<std::deque<MemReqBus>*> tag_lu_l1_array;
    INPUT std::vector<std::deque<MemReqBus>*> lookup_lu_l1_array;
    INPUT std::vector<std::deque<MemReqBus>*> tag_lu_scb_array;
    INPUT std::vector<std::deque<MemReqBus>*> lookup_lu_scb_array;

    /* to LU */
    OUTPUT std::vector<std::deque<MemReqBus>*>                tag_l1_lu_q;
    OUTPUT std::vector<std::deque<MemReqBus>*>                pref_l1_lu_q;
    OUTPUT std::vector<std::deque<MemReqBus>*>                lookup_l1_lu_q;
    OUTPUT std::vector<std::deque<MemReqBus>*>                tag_scb_lu_q;
    OUTPUT std::vector<std::deque<MemReqBus>*>                lookup_scb_lu_q;
    OUTPUT std::vector<SimQueue<PtrPacket>*>             wakeup_l1_lu_q;
    OUTPUT std::vector<SimQueue<MemReqBus>*>             upgrade_l1_lu_q;
    OUTPUT std::vector<SimQueue<MemReqBus>*>             wakeup_scb_lu_q;

    /* from SU */
    INPUT std::vector<SimQueue<MemReqBus>*>      commit_su_scb_array;

    /* from L2 */
    INPUT SimQueue<PtrPacket>*              lookup_l2_l1_q;
    INPUT std::vector<std::deque<PtrPacket>>resp_l2_l1_array;

    /* sim queue to cluster queue */
    uint64_t                                addr2L1cID(uint64_t addr);
    bool                                    dataAllow(void);
    void                                    dispL2Resp(void);

   /* Basic Interface */
    void                                    Reset(void);
    void                                    Work(void);
    void                                    Xfer(void);
    void                                    Build(void);
    SimSys                                  *GetSim(void);
    void ReportStat() override {}
    uint32_t                                getCltNum() { return configs.l1_clts_num; }
};

} // namespace JCore