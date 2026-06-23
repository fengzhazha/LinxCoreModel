#pragma once

#include <unordered_set>
#include <vector>

#include "../configs/bfu_config.h"
#include "bctrl/bctrl_stats.h"
#include "bctrl/bfu/bfu_bhc.h"
#include "bctrl/bfu/bfu_bim.h"
#include "bctrl/bfu/bfu_brq.h"
#include "bctrl/bfu/bfu_btlb.h"
#include "bctrl/bfu/bfu_ghrq.h"
#include "bctrl/bfu/bfu_hwpf.h"
#include "bctrl/bfu/bfu_ibtb.h"
#include "bctrl/bfu/bfu_interface.h"
#include "bctrl/bfu/bfu_logger.h"
#include "bctrl/bfu/bfu_loop_buffer.h"
#include "bctrl/bfu/bfu_loop_pred.h"
#include "bctrl/bfu/bfu_pbtb.h"
#include "bctrl/bfu/bfu_ras.h"
#include "bctrl/bfu/bfu_sp.h"
#include "bctrl/bfu/bfu_tage.h"
#include "bctrl/bfu/bfu_ubtb.h"
#include "bctrl/bfu/bfu_utils.h"
#include "SimSys.h"
#include "ModelCommon/DelayQueue.h"

#define LOCAL_PIPE_FB_NUM 1

namespace JCore {


namespace NS_CORE {
class BFU : public SimObj {
    friend class BFUComponent;
protected:
    struct SelectPipeInfo {
        bool vld = false;
        bool stall = false;
        uint32_t pipe_id = 0;
        void Reset() { vld = false; stall = false; }
    };

    struct DeliverFBInfo {
        bool vld = false;
        bool global = false;
        uint32_t pipe_id = 0;
        uint32_t idx = 0;
        seq_t fbid = 0;
        seq_t fbid_local = 0;
    };

    SimSys* sim;
    // common components
    BFULogger logger;
    BFUUtils  utils;
    BCtrlStats* stats;
    // functional components
    UBTB ubtb;
    PBTB pbtb;
    BHC bhc;
    BTLB btlb;
    GHRQ ghrq[BFU_TAGE_NPAGE];
    std::vector<RAS> ras;
    IBTB ibtb;
    BIM bim;
    TAGE tage[BFU_TAGE_NPAGE];
    BRQ brq;
    StaticPredictor sp;
    HWPF hwpf;
    LoopPred lp;
    LoopBuffer lb;
    // pipe
    Pipe pipe[NSTAGE];
    vector<LocalPipe> local_fu;
    // instruction footprint
    std::unordered_set<addr_t> hdr_fp_cmt;
    std::unordered_set<addr_t> hdr_fp_spec;
    std::unordered_set<addr_t> hdr_fp_pref;
    // used by fake core
    int commit_cnt {0};
    // Selelcted local pipe for F2 fetch
    SelectPipeInfo select_info;

public:
    BFUInterface& intf;
    BFU(SimSys* sim, BCtrlStats* stats, BFUInterface& intf): sim(sim), stats(stats), intf(intf) {}
    virtual ~BFU();
    void Work() override;
    void Dump();
    void Reset() override;
    void Xfer() override;
    BFUConfig cfg;
    SimSys* GetSim() { return sim; }

    void Build();

    void CreateNewInfoToFetchQ(addr_t va, addr_t va_prev, addr_t tag, uint32_t stid, bool first_after_redirect = false);
    void CreateNewF0(PtrFB &fb);
    uint32_t CreateLocalF0(addr_t va_prev, PtrFB fb_prev, pos_t pos_end);
    virtual bool L2CAvailable();

    void RequestL2C(addr_t pa_cl, BFUReqType req_type);
    void DeliverInst(PtrMachineInst const& h);
    void UpdateUBTB(PtrFB const& fb, pos_t pos);
    void UpdatePredictors(PtrFB const& fb);
    void updatePosbase(PtrFB const& fb, pos_t pos);
    void ReportStat() final;
    void OnWarmupFinish();
    bool PipeIdle();
    // TODO: Just a hack for smt, needs to check needs in the future
    void TerminateFlush(uint32_t stid);
    // Used by fake core
    int GetCommitCnt() { return commit_cnt; }
private:
    deque<DeliverFBInfo> deliver_q;
    ThdRingQ<PtrFB> fetchThQ;
    // TODO: Winding fbid
    seq_t fbid_global = 0;
    // update for tage
    PtrFB rslvNtFB[BFU_TAGE_NPAGE] = {nullptr};
    // global fb return from l2
    bool globalL2Reurn = false;
    /* \breif Pipe start, handle request */
    void RunAtPre();
    /* \brief F4 stage: competitive prediction */
    void RunF4();
    /* \brief F3 stage: predecode and static prediction */
    void RunF3();
    /* \brief F2 stage: fetch stage */
    void RunF2();
    /* \brief F1 stage: 0 cycle prediction */
    void RunF1();
    /* \brief F0 stage: create new fetch bundle */
    void RunF0();
    uint32_t PickThread();
    /* \breif use loop-buffer */
    void RunLoopBuffer();
    /* \brief control pipe, stall and flush */
    void PipeCtrl();
    /* \brief move pipe */
    void PipeMove();
    /* \brief move pipe from src to dst */
    void MoveOneStage(Pipe& src, Pipe& dst);
    /* \brief main predictor */
    void DoMainPrediction(PtrFB &fb, pos_t pos);
    /* \brief main prediction with loop-buffer */
    void DoMainPredictionLB(PtrFB &fb);
    /* \brief handle message from back end */
    void HandleBEMsg();
    /* \brief handle l2 return */
    void HandleL2Pkt();
    /* \brief Hand l2 single paket */
    void HandleL2SinglePkt(PtrPacket const& pkt);
    /* \brief print pipe state */
    char PipeState2Char(PipeState s);
    void PrintPipeState();
    /* \brief N taken at F1, create the second fetch bundle at current cycle */
    void CreateFBContinue(size_t idx);
    /* \brief 0 cycle prediction at F1 stage */
    void PredF1(PtrFB &fb);
    /* \brief prediction at F4 stage */
    bool PredF4(PtrFB &fb);
    /* \brief handling nuke flush */
    bool NukeHandling();
    /* \brief handling misprediction */
    void RedirectHandling();
    /* \brief handling intra flush at F4 stage */
    void FlushForF4();
    /* \brief flush for local fetch bundle */
    void FlushByLocal();
    void FlushForEnd(uint32_t pipe_id);
    void FlushForCall(uint32_t pipe_id);
    void FlushForIncond(uint32_t pipe_id);
    void ReleaseEmptyLocalPipes();
    /* \brief handling pipe stall or move */
    void PipeStall();
    /* \brief stall for delievering instructions */
    void DeliverStall(uint32_t stid);
    /* \brief stall for cache miss */
    void FetchStall();
    /* \brief select and stall for local pipe */
    void LocalFetchStall();
    /* \brief tlb stall */
    void TlbStall();
    /* \brief create new fetch bundle for local pipe */
    void CreateNewFB();
    /* \brief ras stall */
    void RasStall(uint32_t stid);
    /* \brief if the fetch bundle taken in itself */
    bool CheckForwardMatch(PtrFB const& fb);
    /* \brief fetch bundle taken in itself at F1 stage */
    void ShortForwardF1(PtrFB &fb);
    /* \brief fetch bundle taken in itself at F4 stage */
    bool ShortForwardF4(PtrFB &fb, pos_t& pos);
    /* \brief get free pipe id of local pipe */
    uint32_t GetFreeLocalPipeID();
    /* \brief get oldedst local pipe id */
    uint32_t GetOldestLocalPipeID();
    /* \brief delivery stall for local pipe */
    bool LocalPipeStall(uint32_t const& reserve);
    /* \brief delivery fetch bundle to back-end */
    void DeliverFB();
    /* \brief check if it is the oldest global fetch bundle */
    bool IsOLdestGlobalFB(PtrFB const& fb);
    /* \brief check if it is the oldest local fetch bundle */
    bool IsOldestLocalFB(PtrFB const& fb);
    /* \brief get local pipe id by fbid */
    uint32_t GetLocalPipeID(seq_t const& fbid);
    /* \brief global fetch bundle wakeup appurtenant local pipe */
    void WakeupLocalPipe(PtrFB const& fb);
    /* \brief check intra misprediction */
    bool MisAtF4(const PtrFB &fb);
    /* \brief get global Fb by fbid */
    PtrFB GetStartFBByFbidHid(seq_t fbid, seq_t hid, uint32_t pipe_id, uint32_t stid);
    /* \brief check for the oldest fetch bundle */
    bool CheckOldest(PtrFB const& fb, bool *global_select, bool *local_select);
    /* \brief decrise fetch size in local pipe */
    void DecLocalPipeFetchSize(uint32_t pipe_id, PtrFB const& fb);
    /* \brief set fetch end for local fetch bundle */
    void SetLocalPipeFetchSize(uint32_t pipe_id, addr_t end_pc);
    /* \brief check if the call branch is mispredicted */
    bool CheckRasMis(PtrFB const& fb_start, pos_t const& pos, PtrFB const& fb_addpc);
    /* \brief arbitrate global fetch bundle in main predictor */
    void ArbitratePrediction(PtrFB &fb, pos_t pos);
    /* \brief arbitrate predictors with static predictor */
    void ArbitrateWithSP(PtrFB &fb, pos_t pos_s);
    /* \breif arbitrate for local fetch bundle */
    void ArbitrateForLocalFB(PtrFB &fb);
    /* \brief check misprediction in local fetc bundle */
    bool MisInLocalFB(PtrFB& fb, LocalPipe& local);
    /* \brief report statistics for local fetch bundle */
    void RptLocalStat();
    /* \brief get valid fetch bundle in global pipe from F1~F3 */
    int GetValidFB(size_t idx);
    /* \brief Arbitrate tage */
    void ArbitrateTAGE(PtrFB &fb);
    uint32_t SelectTageID(PtrFB &fb);
    /* \brief train tage by resolve */
    void TrainTAGE(PtrFB const& fb, pos_t pos);
    void DumpPipeStatus();
};

}


} // namespace JCore
