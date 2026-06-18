#pragma once

#include <set>

#include "../configs/mlsu_config.h"
#include "core/Bus.h"
#include "core/Packet.h"
#include "mtccore/lsu/prefetch/PFCommon.h"
#include "mtccore/lsu/prefetch/MtcStream.h"
#include "mtccore/lsu/prefetch/MtcStride.h"

namespace JCore {

class MtcLoadStoreUnit;
class MtcPrefetcher {
public:
    struct PFScnt {
        uint64_t maxThr;
        uint64_t takeThr;
        uint64_t cnt = 0;
        PFScnt() {};
        PFScnt(uint64_t max, uint64_t take)
        {
            maxThr = max;
            takeThr = take;
            cnt = 0;
        }
        void inc() {cnt = cnt == maxThr ? cnt : cnt + 1;}
        void dec() {cnt = cnt == 0 ? cnt : cnt - 1;}
        bool isTaken() {return cnt > takeThr;}
        void Reset() {cnt = 0;}
    };
public:
    ~MtcPrefetcher();
    SimQueue<MemReqBus> *pref_l1_q;
    SimQueue<MemReqBus> *lu_pref_q;
    SimQueue<PtrPacket> *pref_l2_q;
    SimQueue<PtrPrefFB> *l1_pref_q;
    SimQueue<PtrPrefFB> *l2_pref_q;
    mLSUConfig           *configs;
    bool                 verbose;
    SimSys               *sim;
    MtcLoadStoreUnit     *top;
    bool                 l2_data_allow = true;

    void                insert(MemReqBus &bus);
    void                pfl1(uint64_t addr, uint32_t type);
    void                pfl2(uint64_t addr, uint32_t type);

    void                Reset();
    void                Work();
    void                Xfer();
    SimSys              *GetSim();
    void                Build();

private:
    MtcStride              stride;
    MtcStream              stream;
    std::deque<uint64_t> stride_pfl1_q;
    std::deque<uint64_t> stride_pfl2_q;
    std::deque<uint64_t> stream_pfl1_q;
    std::deque<uint64_t> stream_pfl2_q;
    uint64_t            pfl1_count;

    struct PrefetchBuffer {
        uint32_t depth;
        uint32_t count = 0;
        std::set<uint64_t> pfl1_set;
        std::set<uint64_t> pfl2_set;
        std::set<uint64_t> addr_set;
        std::deque<uint64_t> addr_fifo;
        PrefetchBuffer(uint32_t depth_) { depth = depth_; }
        bool insert_pfl1(uint64_t addr);
        bool insert_pfl2(uint64_t addr);
        void insert_addr(uint64_t addr);
    };
    
    PrefetchBuffer *pref_buf = nullptr;
};

} // namespace JCore