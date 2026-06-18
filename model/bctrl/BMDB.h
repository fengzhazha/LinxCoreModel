#pragma once

#include <unordered_map>

#include "core/Bus.h"

namespace JCore {

using namespace std;

class BCtrlUnit;

class BMDB : public SimObj {
private:
    struct StoreEntry {
        int conf = 0;
        uint32_t bid_off = 0;
        ROBID st_bid;
        uint64_t st_pc = 0;
        uint32_t pe_id = 0;
        StoreEntry() {conf = 0;}
    };

    struct Confpkt {
        uint64_t ld_pc;
        StoreEntry conf_st;
    };

    std::unordered_map<uint64_t, StoreEntry> confTable;

    SimQueue<Confpkt> ls_conf_q;
public:
    BCtrlUnit *top;

    bool verbose;
    void Work();
    void Xfer();
    void Build();
    void Reset();
    void ReportStat() override {}
    SimSys *GetSim();
    void reportConfilict(ROBID ld_bid, ROBID st_bid, uint32_t stid);
    void insert(Confpkt pkt);
    bool lookup(uint64_t ld_pc, ROBID ld_bid);
    uint32_t getPEID(uint64_t ld_pc);
    void updatePEID(uint64_t st_pc, uint32_t pe, ROBID st_bid);
};

} // namespace JCore