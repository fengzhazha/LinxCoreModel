#pragma once

#include <deque>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <unordered_set>
#include <vector>

#include "../configs/l2_config.h"
#include "core/Bus.h"
#include "GFUSim.h"
#include "l2/l2_stats.h"
#include "l2/L2types.h"
#include "ModelSpec.h"

namespace JCore {

class L2Mem;
class L2Cache;
struct L2Entry;

class MissQueue : public SimObj {
public:
    enum MissQState {
        S_EMPTY, S_LOOKUP,
        S_WBACK,
        S_MISS, S_REPLACE,
        S_SNP, S_EVICT, S_RESP
    };

    static const char* state_str(MissQState e) {
        static const char* str[] = {"S_EMPTY", "S_LOOKUP", "S_WBACK", "S_MISS", "S_REPLACE", "S_SNP", "S_EVICT", "S_RESP"};
        return str[e];
    }

    struct MissQEntry {
        MissQState state = MissQState::S_EMPTY;
        uint64_t addr = 0;
        uint512_t data;
        int src = 0;
        bool dirty = false;
        bool valid = false;
        bool wait = false;
        bool waitL1Pref = false;
        bool waitL2Pref = false;
        bool write = false;
        bool hit = false;
        bool excl = false;
        bool update = false;
        bool upgrade = false;
        bool share = false;
        bool lookuped = false;
        bool dup = false;
        bool inst = false;
        bool pref = false;
        bool checkPref = false;
        uint32_t tid = 0;
        int idx = 0;
        uint64_t alloc_cycle = 0;
        PtrPacket req_pkt;
        std::unordered_set<int> dst_set;
        bool checkseq = true;
        bool transferReq = false;
        bool vCoreReq = false;
        uint32_t coreId = 0;
        uint32_t reqId = 0;
        uint32_t peId = 0;
        uint32_t stid = -1U;
        LSUType lsuTypeId = LSUType::SCALAR_LSU;
    };

private:

    friend class L2Cache;
    bool verbose;
    /* \Prefetch count in look up queue */
    uint64_t    cur_pref_count = 0;
    /* \Prefetch count in l1pkt_q */
    uint64_t    cur_pref_count_in_l1q = 0;

    // Current state
    std::vector<MissQEntry> queue;
    std::deque<MissQEntry*> free_list;
    std::deque<MissQEntry*> resp_list;
    std::deque<MissQEntry*> lookup_list;
    std::deque<MissQEntry*> replace_list;
    std::deque<MissQEntry*> snp_list;
    std::deque<MissQEntry*> evict_list;
    std::deque<MissQEntry*> wback_list;
    std::map<uint64_t, MissQEntry*> comb_map;
    std::map<uint64_t, MissQEntry*> gets_map;
    std::map<uint64_t, std::deque<MissQEntry*>> miss_filter;
    std::map<uint64_t, MissQEntry*> evict_map;
    std::list<MissQEntry*> pending_list;
    /* The pollution filter to count the L2Cache miss caused by prefetch. */
    std::vector<bool>   poll_filter;
    uint64_t            low_mask;
    uint64_t            high_mask;

    std::deque<MissQEntry*> picked_lookup;
    std::deque<MissQEntry*> picked_replace;
    std::deque<MissQEntry*> picked_wback;
    std::deque<MissQEntry*> picked_resp;
    std::deque<MissQEntry*> picked_inv;
    std::deque<MissQEntry*> picked_evict;

    std::deque<L2Entry> evict_q;
    std::deque<PtrPacket> l1pkt_q;
    std::deque<PtrPacket> l2pkt_q;
    std::deque<PtrPacket> snp_resp_q;

    std::set<uint64_t> pref_set;

    SimQueue<GFUMemReq> *l2_mem_q;
    SimQueue<GFUMemReq> *mem_l2_q;
    L2Mem*    l2m;
    std::map<uint64_t, uint64_t> prefQ;

    void update_state(MissQEntry *e, MissQState s, bool tolist = true);
    bool full(void);
    bool pfl2Full(void);
    bool stall(void);
    bool prefStall(void);
    bool normalReqFull(void);
    bool normalStall(void);
    uint64_t checkPrefReserve(void);
    void handleL1Pkt(void);
    void handleLookup(void);
    void handleWBack(void);
    void handleReplace(void);
    void handleMemResp(void);
    void HandleL2Pkt(void);
    MissQEntry* allocEntry(void);
    void doWBCombine(uint64_t addr, int src, bool dirty, uint512_t *pdata, bool snpResp);
    void checkSeq(MissQEntry* e, uint64_t addr);
    void freeSeq(MissQEntry *e);
    bool checkCombine(uint64_t addr);
    void sendMemReq(int id, uint64_t addr, bool write, bool pref, bool inst, uint512_t *pdata, uint32_t peid);
    void stats_tick(void);
    bool replacePref();
    void throwPref();
    uint64_t getPollFilterIndex(uint64_t addr);
    void setPollFilter(uint64_t addr, bool dst);
    bool lookupPollFilter(uint64_t addr);
    uint32_t getLsuTypeIndex(LSUType lsuTypeId);

    template<typename... ARGS>
    void info(ARGS... args) {
        printf("[L2] MissQueue. ");
        printf(args...);
    }

public:
    std::shared_ptr<L2Config> config { nullptr };
    SimSys *sim;
    std::shared_ptr<L2Stats> stats;
    L2Cache *top;
    bool m_transferReq = false;
    bool m_vCoreReq = false;
    LSUType lsuTypeId = LSUType::SCALAR_LSU;

    void Work() override;
    void Reset() override;
    void Xfer() override;
    SimSys *GetSim() override;
    void ReportStat() override {}

    void Build();
    void pushL1Pkt(PtrPacket pkt);
    bool getL2Pkt(PtrPacket &pkt);
    void pushRespPkt(PtrPacket resp);
};

} // namespace JCore
