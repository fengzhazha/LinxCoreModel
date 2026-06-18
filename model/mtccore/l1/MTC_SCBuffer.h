#pragma once

#include <deque>
#include <unordered_map>

#include "core/Bus.h"
#include "core/Packet.h"
#include "mtccore/l1/MTC_L1DCache.h"
#include "mtccore/lsu/mtc_lsu_stats.h"
#include "ModelSpec.h"
#include "SimSys.h"

namespace JCore {

class MTC_L1Clusters;

class MTC_SCBuffer : public SimObj {
public:
    bool                       verbose;
    MTC_L1DCache                   *dcache;
    MTC_L1Config                   *configs;
    MTC_L1Clusters                  *cluster;
    MTC_L1Clusters                  *top = nullptr;
    SimSys                     *sim;
    mLSUConfig                  *lsuConfigs = nullptr;
    std::shared_ptr<DebugLog>  debugLogger = nullptr;
    bool                       dcache_allow = true;
    int                        memID_s;
    int                        scbID;
    std::shared_ptr<MtcLSUStats>  stats;
    bool                       l2_data_allow = true;
    uint32_t                   n_store_in;

    OUTPUT SimQueue<MemReqBus> *wakeup_scb_lu_q;
    OUTPUT SimQueue<PtrPacket> *pkt_out_q;

    MTC_SCBuffer();
    virtual ~MTC_SCBuffer();

    void Work() override;
    void Xfer() override;
    void Reset() override;
    SimSys* GetSim() override;
    void Build(void);

    bool full(void);
    void ReportStat() override {}
    void insert(uint64_t addr, int size, uint64_t data);
    bool lookup(uint64_t addr, uint8_t **d_array, bool **v_array);
    void setMemResp(PtrPacket pkt);
    bool hasDCacheReq(void);
private:
    enum MTC_SCBState {
        MTC_S_EMPTY, MTC_S_VALID, MTC_S_LOOKUP, // MTC_S_MISS
    };

    struct MTC_SCBEntry {
        uint64_t addr;
        uint8_t data[256];
        bool valid[256];
        bool full;
        MTC_SCBState state;
        int id;
    };

    struct InsertReq {
        uint64_t addr;
        int      size;
        uint64_t data;
        Opcode   opcode = Opcode::OP_INVALID;
    };
    uint32_t N_SCB;
    MTC_SCBEntry *array;
    std::deque<MTC_SCBEntry*> free_list;
    std::deque<MTC_SCBEntry*> lookup_list;
    std::deque<MTC_SCBEntry*> resp_list;
    std::unordered_map<uint64_t, MTC_SCBEntry*> comb_map;

    void free_entry(MTC_SCBEntry *e);
    void handleInsert(void);
    void handleFull(void);
    void handleLookup(void);
    void handleMemResp(void);
    void sendMemReq(MTC_SCBEntry *e, bool upgrade);
    void sendMemReq(MTC_SCBEntry *e);
    void sendLUwakeup(uint64_t addr, bool *valid);
    void checkEntryFull(MTC_SCBEntry *e);
    void stats_tick();

    InsertReq cur_req;
    MTC_SCBEntry *picked_lookup;

    std::deque<InsertReq> insert_q;
    SimQueue<uint32_t> mem_resp_q;
};

} // namespace JCore