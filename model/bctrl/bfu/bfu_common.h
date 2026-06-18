#pragma once

#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>

#include "bctrl/bctrl_stats.h"
#include "ISA.h"
#include "bctrl/bfu/common/MachineInst.h"
#include "SimSys.h"
#include "bctrl/bfu/common/SPMinst.h"

namespace JCore {


namespace NS_CORE {
constexpr int HEADER_SIZE_LOG2 = 1;
constexpr int MIN_BUNDLE_SIZE = 1 << HEADER_SIZE_LOG2; // Bytes
int constexpr BFU_BANDWIDTH = 0x40 / MIN_BUNDLE_SIZE;
int constexpr BFU_WAY_SIZE = 8;
int constexpr INLINE_HEADER_SIZE = 2;
int constexpr POS_WIDTH = BFU_BANDWIDTH * BFU_WAY_SIZE / INLINE_HEADER_SIZE;
int constexpr BFU_ALIGNMENT = 2;
int constexpr BFU_CROSS_CL_MAX = 2;
int constexpr BFU_GHR_LEN = 192;
static_assert(BFU_GHR_LEN % 64 == 0 && "BFU_GHR_LEN must be a multiple of 64");
int constexpr BFU_GHR_NGROUP = BFU_GHR_LEN / 64;
int constexpr BFU_CL_NBYTE = 64;  // TODO: add a core level parameter for cacheline and page size
int constexpr BFU_PG_NBYTE = 4096;
int constexpr BFU_TAGE_NPAGE = 2;

using BrType = BranchType;
using ghr_t = uint64_t;

enum BFUStage {
    F0=0, F1, F2, F3, F4, NSTAGE, NIL
};

enum PipeFBIndex {
    FIR = 0, SEC = 1
};

enum BFUReqType {
    BHC_DMD=0, BHC_PF, BTLB_DMD, BTLB_PF
};

class UCnt {
    uint8_t cnt = 0;
public:
    void inc() { cnt = cnt==3? cnt : cnt+1; }
    void dec() { cnt = cnt==0? cnt : cnt-1; }
    bool isZero() { return cnt==0; }
    void Reset() { cnt = 1; }
};

class SatCnt1b {
    uint8_t cnt = 1;
public:
    void inc() { cnt = cnt==1? cnt : cnt+1; }
    void dec() { cnt = cnt==0? cnt : cnt-1; }
    bool isTaken() { return cnt>0; }
    void Reset(bool taken) { cnt = taken? 1 : 0; }
};

class SatCnt2b {
public:
    uint8_t cnt = 1;
    void inc() { cnt = cnt==3? cnt : cnt+1; }
    void dec() { cnt = cnt==0? cnt : cnt-1; }
    bool isTaken() { return cnt>1; }
    void Reset(bool taken) { cnt = taken? 2 : 1; }
};

class SatCnt3b {
    uint8_t cnt = 3;
public:
    void inc() { cnt = cnt==7? cnt : cnt+1; }
    void dec() { cnt = cnt==0? cnt : cnt-1; }
    bool isTaken() { return cnt>3; }
    void Reset(bool taken) { cnt = taken? 4 : 3; }
};

struct BranchInfo {
    bool        vld = false;
    addr_t      tgt = -1U;
    addr_t      pc = -1U; // tag
    addr_t      end_pc = -1U;
    BrType      attr = BranchType::BLK_BR_INVAL;
    SatCnt2b    scnt;
    UCnt        ucnt;
    void Reset() {vld = false; attr = BranchType::BLK_BR_INVAL; scnt.Reset(false); ucnt.Reset(); }
};

struct UBTBEntry {
    set_t set_idx = 0;
    way_t way_idx = 0;
    addr_t   tag = -1;
    uint32_t size = 0;
    std::vector<BranchInfo> br;

    UBTBEntry() { Reset(); }
    void Build (uint32_t nsize) { br.resize(nsize); Reset(); }
    void Reset() {
        for (uint32_t i = 0; i < br.size(); i++) {
            br[i].Reset();
        }
        size = 0;
    }
    bool full() { return size >= br.size(); }
    bool isTaken(uint32_t i) {
        const BrType branchTaken[] { BranchType::BLK_BR_IND, BranchType::BLK_BR_ICALL, BranchType::BLK_BR_RET, BranchType::BLK_BR_DIRECT,
                                     BranchType::BLK_BR_CALL};
        bool taken = (std::find(branchTaken, std::end(branchTaken), br[i].attr) != std::end(branchTaken));
        return (br[i].vld && ((br[i].attr == BranchType::BLK_BR_COND && br[i].scnt.isTaken()) || taken));
    }
    uint32_t getEntryIdx(uint32_t pc) {
        uint32_t idx = -1U;
        for (uint32_t i = 0; i < br.size(); i++) {
            if (br[i].vld && br[i].pc == pc) {
                idx = i;
                break;
            }
        }
        return idx;
    }
    void evit(uint32_t idx, uint32_t replace_idx) {
        if (idx < replace_idx) {
            for (uint32_t i = replace_idx - 1; i >= idx; i--) {
                br[i+1] = br[i];
                if (i == 0) {
                    break;
                }
            }
        } else if (idx > replace_idx) {
            for (uint32_t i = replace_idx; i < idx; i++) {
                br[i] = br[i+1];
            }
        }
    }
    void insert(uint32_t idx, BrType branch, addr_t target, addr_t taken_pc, addr_t end_pc) {
        if (!(br[idx].vld && br[idx].pc == taken_pc &&
            branch == BranchType::BLK_BR_DIRECT && br[idx].attr != BranchType::BLK_BR_CALL)) {
            br[idx].attr = branch;
        }
        br[idx].vld = true;
        br[idx].pc = taken_pc;
        br[idx].tgt = target;
        br[idx].end_pc = end_pc;
    }
    uint32_t update(BrType branch, addr_t target, addr_t taken_pc, addr_t end_pc) {
        uint32_t idx = getEntryIdx(taken_pc);
        // hit
        if (idx != -1U) {
            insert(idx, branch, target, taken_pc, end_pc);
            br[idx].ucnt.inc();
            return idx;
        }
        // replace
        if (full()) {
            uint32_t replace_idx = -1U;
            for (uint32_t i = 0; i < br.size(); i++) {
                // recently not use and no taken
                if (!br[i].scnt.isTaken() && br[i].ucnt.isZero()) {
                    replace_idx = -1U;
                }
                // insert in fetch order
                if ((br[i].pc > taken_pc || i == br.size()-1) && idx != -1U) {
                    idx = i;
                }
            }
            if (replace_idx == -1U) {
                // decrease use count
                for (uint32_t i = 0; i < br.size(); i++) {
                    br[i].ucnt.dec();
                }
                idx = -1U;
            } else {
                // insert to idx, evit replace_idx
                evit(idx, replace_idx);
                br[idx].Reset();
                insert(idx, branch, target, taken_pc, end_pc);
            }
            return idx;
        }
        // insert in pc order
        for (uint32_t i = 0; i < br.size(); i++) {
            if (!br[i].vld || (br[i].vld && br[i].pc > taken_pc)) {
                idx = i;
                evit(idx, size);
                break;
            }
        }
        size ++;
        br[idx].Reset();
        insert(idx, branch, target, taken_pc, end_pc);
        return idx;
    }
};

struct GHRInfo {
    ghr_t ghr[BFU_GHR_NGROUP+1] = {0}; // 0 for LSB (most recent hist), add one for protection
    std::vector<idx_t> wptr;
};

struct PathHist {
    addr_t pc = -1U;
    addr_t tgt = -1U;
    BrType type = BranchType::BLK_BR_INVAL;
    bool taken = false;
    PathHist(addr_t pc, addr_t tgt, BrType(type), bool taken)
     : pc(pc), tgt(tgt), type(type), taken(taken) {};
};

struct UBTBInfo {
    bool hit = false;
    set_t set_idx = -1U;
    way_t way_idx = -1U;

    bool taken = false;
    addr_t taken_pc = -1U;
    addr_t tgt = -1U;
    addr_t end_pc = -1U;
    BrType taken_type = BranchType::BLK_BR_INVAL;

    std::vector<PathHist> path_hist;
    UBTBInfo& operator=(UBTBInfo &oldInfo);
    void Reset() {
        hit = false; set_idx = -1U; way_idx = -1U; taken = false; tgt = -1U;
        end_pc = -1U; taken_type = BranchType::BLK_BR_INVAL; path_hist.clear();
    }
};

struct PBTBInfo {
    bool hit = false;
    set_t set_idx = -1U;
    way_t way_idx = -1U;
    UBTBEntry* hit_entry = nullptr;
    void Reset() { hit = false; set_idx = -1; way_idx = -1; hit_entry = nullptr; }
};

struct BHCInfo {
    msize_t num_cl = 0;
    addr_t va_cl[BFU_CROSS_CL_MAX] = {0};
    addr_t pa_cl[BFU_CROSS_CL_MAX] = {0};
    bool hit[BFU_CROSS_CL_MAX] = {0};
    set_t set_idx[BFU_CROSS_CL_MAX] = {0};
    way_t way_idx[BFU_CROSS_CL_MAX] = {0};
    bool allLinesHit() {
        bool all_hit = true;
        for (pos_t n=0; n<num_cl; n++) all_hit &= hit[n];
        return all_hit;
    }
};

struct IBTBInfo {
    struct LookupInfo {
        set_t set_idx;
        way_t way_idx;
        bool hit;
    };
    idx_t table_idx = -1U;
    addr_t tgt = -1U;
    std::vector<LookupInfo> lkup_info;
    void Reset() {lkup_info.clear(); table_idx = -1U; tgt = -1U; }
    IBTBInfo& operator=(IBTBInfo &oldInfo);
};

struct RASInfo {
    idx_t spec_tos = -1U;
    idx_t spec_wptr = -1U;
    idx_t cmt_alloc_ptr = -1U;
    void Reset() { spec_tos = -1U; spec_wptr = -1U; cmt_alloc_ptr = -1U; }
};

struct SPInfo {
    // header targets
    addr_t tgt[BFU_BANDWIDTH] = {-1U};
    // header attributes
    BrType attr[BFU_BANDWIDTH] = {BranchType::BLK_BR_INVAL};
    // position of addpc
    bool add_pc_vld = false;
    pos_t pos = -1U;
    addr_t return_tgt = -1U;
    std::vector<PathHist> path_hist;
};

struct BIMInfo {
    bool taken = false;
    set_t set_idx = -1U;
    BrType taken_type = BranchType::BLK_BR_INVAL;
    addr_t taken_pc = -1U;
    addr_t tgt = -1U;
    addr_t end_pc = -1U;
    void Reset() { taken = false; set_idx = -1U; tgt = -1U; end_pc = -1U; }
};

struct TAGEInfo {
    struct LookupInfo {
        bool hit = false;
        bool taken = false;
        BrType attr = BranchType::BLK_BR_INVAL;
        set_t set_idx = -1U;
        way_t way_idx = -1U;
        addr_t pc = -1U;
        addr_t tgt = -1U;
        addr_t end_pc = -1U;
        SatCnt3b w;
    };
    // TAGE Lookup info
    std::vector<LookupInfo> lkup_info;
    // TAGE final prediction
    idx_t prim_idx = -1U;
    idx_t altn_idx = -1U;
    bool taken = false;
    BrType taken_type = BranchType::BLK_BR_INVAL;
    addr_t taken_pc = -1U;
    addr_t tgt = -1U;
    addr_t end_pc = -1U;
    SatCnt3b w;
    void Reset() { lkup_info.clear(); prim_idx = -1U; altn_idx = -1U; taken = false; tgt = -1U; end_pc = -1U; }
    TAGEInfo& operator=(TAGEInfo &oldInfo);
};

struct CPHTInfo {
    bool hit = false;
    SatCnt3b w;
    uint32_t selectID = 0;
};

struct LoopInfo {
    bool hit = false;
    bool pred_taken = false;
    bool backup_active = false;
    addr_t backup_pc = -1U;
    uint32_t backup_curr_cnt = -1U;
};

struct PreDcodeInfo {
    bool global = false;
    bool taken = false;
    bool afterBranch = false;
    uint64_t preBin = 0;
    uint64_t binWidth = 0;
    uint64_t preVa = 0;
    seq_t fbid_global = 0;
    seq_t fbid_local = 0;
    PtrMachineInst machineInst = nullptr;

    void Reset() {
        taken = false;
        afterBranch = false;
        preBin = 0;
        binWidth = 0;
        preVa = 0;
        machineInst = nullptr;
    }

    void Reset(uint32_t stid)
    {
        if (machineInst && machineInst->stid != stid) {
            return;
        }

        Reset();
    }

    void Flush(seq_t fbid_g, seq_t fbid_l, bool globalFlush, bool localFlush) {
        if ((fbid_g == fbid_global && fbid_l < fbid_local && localFlush) ||
            fbid_g < fbid_global || (fbid_g == fbid_global && global)) {
            Reset();
        }
    }

    void Flush(seq_t fbid_g, seq_t fbid_l, bool globalFlush, bool localFlush, uint32_t stid)
    {
        if (machineInst && machineInst->stid != stid) {
            return;
        }

        Flush(fbid_g, fbid_l, globalFlush, localFlush);
    }
};

struct MainInfo {
    bool vld = false;
    bool taken = false;
    bool end_pc_vld = false;
    BrType taken_type = BranchType::BLK_BR_INVAL;
    pos_t pos = -1U;
    pos_t end_pos = -1U;
    addr_t taken_pc = -1U;
    addr_t tgt = -1U;
    addr_t end_pc = -1U;

    std::vector<PathHist> path_hist;

    bool is_from_sp = false;
    bool is_from_bim = false;
    bool is_from_tage = false;
    bool is_from_ibtb = false;
    bool is_from_ras = false;
    bool is_from_loop = false;
    bool is_from_ubtb = false;
    bool is_from_lb = false;
    MainInfo& operator=(MainInfo &oldInfo);
    void Reset() { vld = false; taken = false; pos = -1U; end_pos = -1U; tgt = -1U;
                   taken_type = BranchType::BLK_BR_INVAL; path_hist.clear(); is_from_sp = false;
                   is_from_bim = false; is_from_tage = false; is_from_ibtb = false;
                   is_from_ras = false; is_from_loop = false; is_from_ubtb = false; }
};

struct LoopBufferInfo {
    bool front = false;
    bool hit = false;
    uint32_t lbIdx = -1;
    uint32_t curRepCnt = 0;
    bool miss = false;
    bool taken = false;
    bool checkBP = false;
    pos_t pos = -1;
    addr_t tgt = -1;
};

struct ResolveInfo {
    bool mispredict = false;
    bool taken = false;
    pos_t pos = -1U;
    addr_t tgt = -1U;
    PtrMachineInst mispred_mhdr = nullptr;
    ResolveInfo() { mispredict = false; taken = false; pos = -1U; tgt = -1U; mispred_mhdr = nullptr; }
};

struct RedirectInfo {
    bool resolve_taken = false;
    pos_t pos = -1U;
    addr_t tgt = -1U;
    std::vector<PathHist> path_hist;
    RedirectInfo() { resolve_taken = false; pos = -1; tgt = -1U; path_hist.clear(); }
};

struct ShortForwardInfo {
    bool vld = false;
    bool mispred = false;
    addr_t va = -1;
    addr_t va_prev = -1;
    pos_t tgt_pos = -1;
    pos_t pre_pos = -1;
    GHRInfo ghr_info[BFU_TAGE_NPAGE];
    UBTBInfo ubtb_info;
    PBTBInfo pbtb_info;
    IBTBInfo ibtb_info;
    TAGEInfo tage_info[BFU_TAGE_NPAGE];
    BIMInfo bim_info;
    RASInfo ras_info;
    MainInfo main_info;
};

struct SFInfo {
    bool found = false;
    pos_t pos = 0;
};

struct BinInfo {
    bool vld = false;
    uint16_t bin = 0;
};

struct FetchBundle {
    bool global {true};
    bool end {false};
    seq_t fbid {0};
    seq_t fbid_local {0};
    seq_t hid {0};
    uint32_t pipe_id {-1U};
    addr_t va {0};
    addr_t tag {0};
    addr_t recent_va {0};
    addr_t pa = -1;
    addr_t va_prev = -1;
    addr_t end_va = -1;
    pos_t cmPos = -1U;
    pos_t flushPos = -1U;
    uint32_t stid = -1U;

    bool resolved = false;
    bool committed = false;
    bool updated = false;

    bool first_after_redirect = false;
    bool first_after_nuke = false;
    bool predict_at_once = false;

    time_t create_time {0};
    time_t f1_time {0};
    time_t bhc_fetch_time {0};
    time_t ib_time {0};

    PtrMachineInst machineInst[BFU_BANDWIDTH];
    BinInfo bin[BFU_BANDWIDTH]; // in accordance with decode interface

    GHRInfo ghr_info[BFU_TAGE_NPAGE];
    UBTBInfo ubtb_info;
    PBTBInfo pbtb_info;
    IBTBInfo ibtb_info;
    BHCInfo bhc_info;
    CPHTInfo cPHTInfo;
    TAGEInfo tage_info[BFU_TAGE_NPAGE];
    SPInfo sp_info;
    BIMInfo bim_info;
    RASInfo ras_info;
    LoopInfo loop_info[BFU_BANDWIDTH];
    MainInfo main_info;
    LoopBufferInfo lb_info;
    ResolveInfo rslv_info;
    RedirectInfo redir_info;
    RedirectInfo replay_info;
    ShortForwardInfo sfb_info[BFU_BANDWIDTH];
    PreDcodeInfo dec_info;
    FetchBundle(addr_t va, addr_t va_prev, bool first_after_redirect, seq_t fbid, seq_t loc_fbid, time_t t) :
        fbid(fbid), fbid_local(loc_fbid), va(va), tag(va), recent_va(va), pa(va), va_prev(va_prev), first_after_redirect(first_after_redirect), create_time(t) {
    }
    void recoverFB(pos_t pos);
    SFInfo findShortForward(pos_t pos);
    bool isFallThrough();
    void setInvalid(pos_t pos);
};

using PtrFB = std::shared_ptr<FetchBundle>;

struct HWPFInfo {
    addr_t va = 0;
    addr_t pa = 0;
    uint32_t stid = -1U;

    HWPFInfo(uint32_t va, uint32_t pa, uint32_t stid) : va(va), pa(pa), stid(stid) {}
};

using PtrHWPFInfo = std::shared_ptr<HWPFInfo>;

typedef struct LookupLBInfo {
    bool                        valid = false;
    bool                        trust = false;
    bool                        cross = false;
    uint64_t                    lbIndex;
    uint64_t                    lastEntryIndex;
    uint64_t                    matchLen;
    LookupLBInfo() : valid(false), trust(false), cross(false), lbIndex(0), lastEntryIndex(0), matchLen(0) {}
} LookupLBInfo;

enum class PipeState {
    INVALID, VALID, STALL
};

enum class StallState {
    FB1_STALL, FB2_STALL
};

struct Pipe {
    PipeState state;
    size_t stallIdx = 0;
    std::vector<PtrFB> fb;
    bool Empty();
    void Flush(uint32_t stid);
    void Flush();
    void Reset();
    void FlushByFbidGlobal(seq_t fbid, bool localFlush, uint32_t stid);
    void FlushByFbidAndHid(seq_t fbid, seq_t hid, uint32_t stid);
    uint32_t GetFirstValidFBIdx();
    void Build(size_t size);
    Pipe() : state(NS_CORE::PipeState::INVALID), stallIdx(0), fb({nullptr}) {}
};

struct LocalPipe {
    bool occupied = false;
    bool ready = false; // local decode behind global
    bool sizeVld = false;
    bool sizeGet = false;
    seq_t occupied_fbid;
    uint32_t occupiedStid = -1U;
    BrType taken_type = BranchType::BLK_BR_INVAL;
    addr_t taken_pc = -1U;
    addr_t end_pc = -1U;
    addr_t tgt = 0;
    Pipe pipe[NSTAGE];
    void FreePipe();
    bool FreePipeBycheckPipe();
    void Flush(seq_t fbid, bool localFlush, uint32_t stid);
};

}

} // namespace JCore
