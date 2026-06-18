#pragma once

#include <cstdint>
#include <deque>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "../configs/mlsu_config.h"
#include "ModelCommon/SimInstInfo.h"
#include "core/Bus.h"
#include "core/Packet.h"
#include "mtccore/lsu/load_unit/ldq_cluster.h"
#include "mtccore/lsu/lsu_interface.h"
#include "lsu/lsu_stats.h"
#include "ModelSpec.h"
#include "SimSys.h"


namespace JCore {

class MtcLoadStoreUnit;

enum class MTC_SCPQ_INST_FSM {
    MTC_INST_IDLE = 0,
    MTC_INST_SRC_READY,
    MTC_INST_SRC_INSERT,
    MTC_INST_L1_HIT,
    MTC_INST_SENT,
};

enum class MTC_SCPQ_FSM {
    MTC_SCPQ_IDLE = 0,
    MTC_SCPQ_SENT_L1,
    MTC_SCPQ_L1_MISS,
    MTC_SCPQ_L1_HIT,
    MTC_SCPQ_WAIT_MEM,
};

struct BusInfo {
    uint64_t hitcount = 0;
    MTC_SCPQ_FSM fsm = MTC_SCPQ_FSM::MTC_SCPQ_IDLE;
};

struct ScEntry {
    MTC_SCPQ_INST_FSM fsm = MTC_SCPQ_INST_FSM::MTC_INST_IDLE;
    SimInst inst = nullptr;
};

class Scalper : public SimObj {
public:
    Scalper() {}
    virtual ~Scalper() {}
    /* basic info */
    MtcLoadStoreUnit                                  *top = NULL;
    SimSys                                            *sim = NULL;
    mLSUConfig                                        *pConfigs = NULL;
    std::deque<MemReqBus>                             ReqList;
    std::deque<ScEntry>                               entryList;
    std::unordered_map<uint64_t, BusInfo>             ReqQ;
    std::unordered_map<CommitInfo, vector<uint64_t>, CommitInfoHash>       tagInfo;
     // input from rename
    INPUT SimQueue<SimInst>*                           pe_iex_sc_lda_array;
    // output to Dispatch
    OUTPUT SimQueue<SimInst>*                          pe_iex_lda_array;
    /* from L1 */
    INPUT std::deque<MemReqBus>*                     tag_l1_scalper_q;
       /* to L1 */
    OUTPUT std::vector<std::deque<MemReqBus>*>       tag_scalper_l1_array;
     // from IEX
    INPUT std::deque<CommitInfo>*                    pe_scalper_commit_q;
    /* from L1 */
    INPUT SimQueue<MemReqBus>*                       wakeup_l1_scalper_q;
    INPUT std::deque<CommitInfo>*                    scLdqOrder;
    INPUT std::deque<SimInst>*                         iexScalperInstQ;
    uint32_t                                         sendNum;
    uint32_t                                         sentNum;
    uint32_t                                         receiveNum;
    uint32_t                                         waitTime;
    uint32_t                                         GetScalperSize();
    // 遍历ReqList并删除tag为0的元素
    void removeElements();
    vector<MemReqBus> GetMemReqs(ScEntry entry);
    vector<MemReqBus> GetTlsMemReqs(SimInst inst);
    void checkSrcReady();
    void UpdataSendStatus();
    void sendToL1();
    void updateHitInfo(MemReqBus &bus);
    void mergeStateInfo(void);
    void sendMemReq(MemReqBus &bus);
    void handleL1miss();
    void sendOutInst();
    void insertReqest();
    void insertInst();
    void handleL1Wakeup();
    void handleCommit();
    SimInst FindInst(MemReqBus &bus);
    void Work();
    void Reset(void);
    void Xfer(void);
    void Build(void);
    SimSys *GetSim(void);
    void ReportStat() override {}
    };
}
