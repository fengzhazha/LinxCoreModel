#pragma once

#include <deque>
#include <unordered_map>

#include "../configs/mlsu_config.h"
#include "core/Bus.h"
#include "SimSys.h"

namespace JCore {

class Core;
class MtcLoadStoreUnit;

class MtcMDB : public SimObj {
public:
    bool verbose;
    Core *core;
    SimSys *sim;
    MtcLoadStoreUnit *top = nullptr;
    mLSUConfig *pConfigs;

    /* from LU */
    INPUT std::deque<MDBBus>* lookup_lu_mdb_q;
    INPUT std::deque<MDBBus>* delete_lu_mdb_q;
    INPUT std::deque<MDBBus>* record_lu_mdb_q;
    /* to LU */
    OUTPUT std::deque<MDBBus>* lookup_mdb_lu_q;

    /* To SU */
    OUTPUT std::deque<MDBBus>* lookup_mdb_su_q;

    bool ld_lookup(uint64_t tpc, ROBID bid, uint64_t &st_pc, ROBID &st_bid);
    void insert(uint64_t load_pc, ROBID ld_bid, ROBID ld_lsid, uint64_t store_pc, ROBID st_bid, ROBID st_lsid,
                int conf);
    void release(uint64_t load_pc);
    bool dec(uint64_t &load_pc, uint64_t &store_pc);
    void inc(uint64_t &load_pc, uint64_t &store_pc);
    bool isStall(uint64_t &w);
    void initWeight(uint64_t &w);

    void lookupMDB(void);
    void deleteMDB(void);
    void recordMDB(void);
    void handleMDBLookup(MDBBus &bus);
    void handleMDBRecord(MDBBus &bus);
    void handleMDBDelete(MDBBus &bus);

    void Reset();
    void Work();
    void Xfer();
    SimSys *GetSim();
    void Build();
    void ReportStat() override {}
private:
    struct SSITEntry {
        int conf;
        uint64_t weight;
        uint32_t bid_off;
        uint32_t lsID_off;
        uint64_t st_pc;
        bool     nukeVld = false;
        ROBID    nukeBID;
    };

    // Store Set Id Table
    std::unordered_map<uint64_t, SSITEntry> ssit;

    uint32_t gen_ssid(uint64_t pc)
    {
        return pc ^ (pc >> 32);
    }
};

} // namespace JCore