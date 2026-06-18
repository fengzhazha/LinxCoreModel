#pragma once
#include "core/Bus.h"
#include "ModelCommon/SimInstInfo.h"
#define IEXMDBSIZE 1024
#define MDBTABLESIZE 16
#define MDBTABLEMAXCYCLES 150
#define STINFOSIZE        16

namespace JCore {

class IEX;
class SimSys;

struct Info {
    bool vld;
    uint64_t tpc;
    int conf;
    uint32_t bid_off = 0;
    int lsid_off = 0;
    ROBID bid;
    ROBID lsid;
    bool rdy;
    bool isStWakeup = false;
    Info()
    {
        Reset();
    }
    void Reset() {
        vld = false;
        conf = 0;
        rdy = false;
        bid_off = 0;
        lsid_off = 0;
        isStWakeup = false;
    }
};

struct SSITEntry {
    bool vld;
    int stSize = 0;
    uint64_t ld_tpc;
    Info stInfo[STINFOSIZE];
    uint32_t cycles;
    int ld_conf = 0;
    bool intraConflict = false;
    SSITEntry()
    {
        Reset();
    }
    void Reset() {
        vld = false;
        cycles = 0;
        ld_conf = 0;
        intraConflict = false;
        for (int i = 0; i < STINFOSIZE; ++i) {
            stInfo[i].Reset();
        }
    }
    int lookup(uint64_t st_tpc) {
        for (int i = 0; i < STINFOSIZE; ++i) {
            if (stInfo[i].vld && st_tpc == stInfo[i].tpc) {
                return i;
            }
        }
        return -1;
    }
    void insert(uint64_t st_tpc, uint32_t bid_off, int lsid_off) {
        int idx = lookup(st_tpc);
        if (idx < 0) {
            for (int i = 0; i < STINFOSIZE; ++i) {
                if (!stInfo[i].vld) {
                    stInfo[i].vld = true;
                    stInfo[i].conf = 1;
                    stInfo[i].tpc = st_tpc;
                    stInfo[i].bid_off = bid_off;
                    stInfo[i].lsid_off = lsid_off;
                    stSize++;
                    break;
                }
            }
        } else {
            if (stInfo[idx].bid_off > bid_off || (stInfo[idx].bid_off == bid_off && stInfo[idx].lsid_off >= lsid_off)) {
                stInfo[idx].bid_off = bid_off;
                stInfo[idx].lsid_off = lsid_off;
            }
            stInfo[idx].conf++;
        }
        sort();
    }
    void sort() {
        int k = 0;
        for (int j = 1; j < stSize; j++) {
            if (!stInfo[j].vld) {
                break;
            }
            if (stInfo[j].conf > stInfo[k].conf || (stInfo[j].conf == stInfo[k].conf && stInfo[j].bid_off < stInfo[k].bid_off) ||
            (stInfo[j].conf == stInfo[k].conf && stInfo[j].bid_off == stInfo[k].bid_off && stInfo[j].lsid_off < stInfo[k].lsid_off)) {
                k = j;
            }
        }
        if (k == 0) {
            return;
        }
        Info temp= stInfo[k];
        stInfo[k] = stInfo[0];
        stInfo[0] = temp;
    }
};

struct MDBTable {
    bool vld = false;
    uint32_t cycles;
    Info stInfo;
    Info ldInfo;
    int ssitIdx = 0;
    int stSize = 1;
    bool intraConflict = false;
    MDBTable() {
        vld = false;
        cycles = 0;
        stSize = 1;
    }
    void Reset() {
        vld = false;
        cycles = 0;
        ldInfo.Reset();
        stSize = 1;
        stInfo.Reset();
    }
};
class IEXMDB : public SimObj {
private:
    // Store Set Id Table
    std::vector<SSITEntry> confEntry;
    std::vector<MDBTable> confTable;

    bool isFull() {
        return size >= IEXMDBSIZE;
    }
    bool isRegFull() {
        return regSize >= MDBTABLESIZE;
    }
    uint32_t size;
    uint32_t regSize;
    void flush();
    uint32_t replay();
    int mdb_confidence = 7;
public:
    bool mdb_enable = false;
    IEX *top;

    int ld_lookup(uint64_t tpc, bool isload = false);
    uint32_t lookupReg(int idx, uint64_t tpc, ROBID bid, ROBID lsid, uint32_t stid, bool isload = false);
    void insert(uint64_t ld_tpc, ROBID ld_bid, ROBID ld_lsid, uint64_t st_tpc,
        ROBID st_bid, ROBID st_lsid, bool intraConflict);
    bool release(uint32_t regidx = -1U);
    bool setStRdy(uint32_t regidx);
    bool setStRdy(ROBID bid, ROBID lsid);
    bool isStRdy(uint32_t regidx, bool& bo);
    void Reset();
    void Work();
    void Xfer();
    SimSys *GetSim();
    void ReportStat() override {}
    void Build(double conf, bool enable);
    void releaseConf(int idx, bool wakeuped);
    IDBus getOldestIDBus(uint32_t stid);
    int getCycles(uint32_t regidx) {
        return confTable[regidx].cycles;
    }
};

} // namespace JCore
