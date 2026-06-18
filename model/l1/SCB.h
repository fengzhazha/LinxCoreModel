#pragma once

#include <deque>
#include <unordered_map>

#include "core/Bus.h"
#include "core/Packet.h"
#include "l1/L1DCache.h"
#include "lsu/lsu_stats.h"
#include "ModelSpec.h"
#include "SimSys.h"

namespace JCore {

class L1Clusters;

class SCBuffer : public SimObj {
private:
    enum SCBState {
        S_EMPTY, S_VALID, S_LOOKUP, S_MISS
    };

    struct SCBEntry {
        uint64_t addr;
        uint8_t data[64];
        bool valid[64];
        bool full;
        SCBState state;
        int id;
    };

    struct InsertReq {
        uint64_t addr;
        int      size;
        uint64_t data;
    };

private:
    uint32_t N_SCB;
    SCBEntry *array;
    std::deque<SCBEntry*> free_list;
    std::deque<SCBEntry*> lookup_list;
    std::deque<SCBEntry*> resp_list;
    std::unordered_map<uint64_t, SCBEntry*> comb_map;

    void free_entry(SCBEntry *e);
    void handleInsert(void);
    void handleFull(void);
    void handleLookup(void);
    void handleMemResp(void);
    void sendMemReq(SCBEntry *e, bool upgrade);
    void sendLUwakeup(uint64_t addr, bool *valid);
    void checkEntryFull(SCBEntry *e);
    void stats_tick();

    InsertReq cur_req;
    SCBEntry *picked_lookup;

    std::deque<InsertReq> insert_q;
    SimQueue<uint32_t> mem_resp_q;

public:
    bool                       verbose;
    L1DCache                   *dcache;
    L1Config                   *configs;
    L1Clusters                  *cluster;
    L1Clusters                  *top = nullptr;
    SimSys                     *sim;
    LSUConfig                  *lsuConfigs = nullptr;
    std::shared_ptr<DebugLog>  debugLogger = nullptr;
    bool                       dcache_allow = true;
    int                        memID_s;
    int                        scbID;
    std::shared_ptr<LSUStats>  stats;
    bool                       l2_data_allow = true;
    uint32_t                   n_store_in;

    OUTPUT std::vector<SimQueue<MemReqBus>*> wakeup_scb_lu_q;
    OUTPUT std::vector<SimQueue<PtrPacket>*> pkt_out_q;

    SCBuffer();
    virtual ~SCBuffer();

    void Work() override;
    void Xfer() override;
    void Reset() override;
    SimSys* GetSim() override;
    void Build(void);
    void ReportStat() override {}

    bool full(void);
    void insert(uint64_t addr, int size, uint64_t data);
    bool lookup(uint64_t addr, uint8_t **d_array, bool **v_array);
    void setMemResp(PtrPacket pkt);
    bool hasDCacheReq(void);
};

} // namespace JCore