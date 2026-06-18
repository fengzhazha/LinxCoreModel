#pragma once

#include <deque>
#include <list>

#include "core/Bus.h"
#include "mtccore/lsu/prefetch/PFCommon.h"

namespace JCore {

class MtcPrefetcher;

class MtcStream {
public:
    MtcPrefetcher *prefetcher;
    SimSys     *sim;
    std::deque<uint64_t> *pfl1_q;
    std::deque<uint64_t> *pfl2_q;

    void Reset(void);
    void Build(void);
    void train(MemReqBus &req);
    void Work(void);
    void Xfer(void);
    void feedback(PtrPrefFB);
    void L2Feedback(PtrPrefFB);
    MTCPFConfOP getPFConfOP(float accuracy, float lateness, float pollution, float miss);
    std::string getStr(MTCPFConfOP op);
    void setPFParam();
private:
    typedef struct aaccelss_bits_t {
        bool bits[32];
    } aaccelss_bits_t;

    struct L0Entry {
        uint64_t      region_addr;
        aaccelss_bits_t bits;
        uint32_t      aaccelss_cnt;
    };

    struct MTC_L1Entry {
        uint64_t      region_addr;
        aaccelss_bits_t bits;
        uint32_t      aaccelss_cnt;
        int           conf;
    };

    typedef std::shared_ptr<MTC_L1Entry> PtrMTC_L1Entry;
    typedef std::shared_ptr<L0Entry> PtrL0Entry;

    uint64_t region_size  = 0x200;
    uint64_t region_mask  = region_size - 1;
    uint64_t region_off_n = region_size >> 6;

    uint32_t filter_buf_depth = 4;
    uint32_t l0_table_size    = 8;
    uint32_t l1_table_size    = 8;

    uint32_t l1_trig_thr      = 4;
    uint32_t l0_upgrade_thr   = 6;
    uint32_t pfl1_depth       = 1;
    uint32_t pfl2_depth       = 2;

    std::list<PtrL0Entry> mru0_list;
    std::list<PtrMTC_L1Entry> mru1_list;

    std::deque<uint64_t> filter_buf;
    bool     pref_bad = false;
    int      pref_level = 1;
    uint64_t pfl2_lev_mole = 1;
    uint64_t pfl2_lev_deno = 1;

    uint64_t pfl1_count;
    uint64_t pfl1_bad_count;
    uint64_t pfl1_count_n;
    uint64_t pfl1_bad_count_n;
    bool     stream_disable;
    uint64_t interval_count;
    uint64_t interval_thr = 5000;
    float    ACCURACY_HIGH = 0.0;
    float    ACCURACY_LOW = 0.0;
    float    THRE_LATENESS = 0.0;
    float    THRE_POLLUTION = 0.0;
    float    THRE_MISS = 0.0;
    MTCPFConfLevel confLevel;
    std::vector<MtcStreamConf>  confArray;

    bool     filter_line   (uint64_t);
    void     train_l0      (uint64_t);
    bool     train_l1      (uint64_t);
    void     insert_l1     (PtrL0Entry);
    void     insert_l0     (uint64_t);
    uint64_t to_region_addr(uint64_t);
    uint32_t to_region_off (uint64_t);
    uint64_t region_p1     (uint64_t);
    uint64_t region_m1     (uint64_t);
    void outputq(uint32_t region_off_n, uint64_t pfl1_base, uint64_t pfl2_base);
};

} // namespace JCore