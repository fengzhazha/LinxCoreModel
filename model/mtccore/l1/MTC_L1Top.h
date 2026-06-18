#pragma once

#include <cstdint>
#include <deque>
#include <iostream>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../configs/mlsu_config.h"
#include "../emulator/Memory.h"
#include "core/Bus.h"
#include "core/Packet.h"
#include "mtccore/l1/MTC_L1Clusters.h"
#include "mtccore/l1/MTC_L1DCache.h"
#include "mtccore/l1/MTC_SCBuffer.h"
#include "mtccore/lsu/mtc_lsu_stats.h"
#include "ModelSpec.h"
#include "SimSys.h"
#include "plat/DebugLog.h"

namespace JCore {

class Core;
class MtcLoadStoreUnit;

class MTC_L1Top : public SimObj {
public:
    MtcLoadStoreUnit                        *top = nullptr;
    /* basic info */
    uint32_t                                memID_s = 0;
    Core                                    *core = NULL;
    SimSys                                  *sim = NULL;
    MTC_L1Config                                configs;
    std::shared_ptr<DebugLog>               debugLogger = nullptr;
    mLSUConfig                               *lsuConfigs = nullptr;

    /* from LU */
    INPUT std::vector<std::deque<MemReqBus>*> pref_lu_l1_array;
    INPUT std::vector<std::deque<MemReqBus>*> tag_lu_l1_array;
    INPUT std::vector<std::deque<MemReqBus>*>  tag_scalper_l1_array;
    INPUT std::vector<std::deque<MemReqBus>*> lookup_lu_l1_array;
    INPUT std::vector<std::deque<MemReqBus>*> tag_lu_scb_array;
    INPUT std::vector<std::deque<MemReqBus>*> lookup_lu_scb_array;

    // for TSTORE
    INPUT std::deque<MemReqBus>*                 peScbWrQ;

    /* to LU */
    OUTPUT std::deque<MemReqBus>*                tag_l1_lu_q;
    OUTPUT std::deque<MemReqBus>*                tag_l1_scalper_q;
    OUTPUT std::deque<MemReqBus>*                pref_l1_lu_q;
    OUTPUT std::deque<MemReqBus>*                lookup_l1_lu_q;
    OUTPUT std::deque<MemReqBus>*                tag_scb_lu_q;
    OUTPUT std::deque<MemReqBus>*                lookup_scb_lu_q;
    OUTPUT SimQueue<PtrPacket>*             wakeup_l1_lu_q;
    OUTPUT SimQueue<MemReqBus>*             wakeup_l1_scalper_q;
    OUTPUT SimQueue<MemReqBus>*             upgrade_l1_lu_q;
    OUTPUT SimQueue<MemReqBus>*             wakeup_scb_lu_q;

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

private:
    std::vector<MTC_L1Clusters>                     clusters;
};

} // namespace JCore