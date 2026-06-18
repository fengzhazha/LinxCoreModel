#pragma once

#include "core/Bus.h"
#include "core/Packet.h"
#include "l2/l2_stats.h"
#include "l2/L2Mem.h"
#include "l2/L2types.h"
#include "l2/MissQueue.h"
#include "plat/DebugLog.h"

namespace JCore {

class L2Cache : public SimObj {
private:
    MissQueue missq;
    L2Mem l2mem;
    bool verbose;

public:
    SimSys *sim;
    std::shared_ptr<L2Config> config { nullptr };
    std::shared_ptr<L2Stats> stats;
    uint32_t memID_s = 0;
    bool updateL2Prefetcher = false;
    std::shared_ptr<DebugLog> debugLogger = nullptr;
    LSUType lsuTypeId = LSUType::SCALAR_LSU;

    // Interface
    SimQueue<PtrPacket> *l1_l2_q;
    SimQueue<PtrPacket> *l2_l1_q;
    SimQueue<PtrPacket> *inst_l2_q;
    SimQueue<PtrPacket> *l2_inst_q;
    SimQueue<GFUMemReq> *l2_mem_q;
    SimQueue<GFUMemReq> *mem_l2_q;
    SimQueue<PtrPacket> *pref_l2_q;
    SimQueue<PtrPacket> *hpref_l2_q;
    SimQueue<PtrPacket> *snp_l2_q;
    SimQueue<PtrPrefFB> *l2_pref_q;
    bool                pref_throw = false;
    SimQueue<PtrPacket> *lsuL2ReqQ;
    SimQueue<PtrPacket> *l2LsuRspQ;
    SimQueue<GFUMemReq> *l2SocReqQ;
    SimQueue<GFUMemReq> *socL2RspQ;
    std::vector<SimQueue<PtrPacket>*> vecCoreL2ReqArray;
    std::vector<SimQueue<PtrPacket>*> l2VecCoreRspArray;

    void Work() override;
    void Reset() override;
    void Xfer() override;
    SimSys *GetSim() override;
    void ReportStat();

    void Build();
    bool data_allow(void);
    bool inst_allow(void);
    bool pfl2_allow(void);
    void feedbackToPrefetcher();
};

} // namespace JCore
