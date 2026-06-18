#pragma once

#include <cstdint>
#include <deque>
#include <iostream>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../emulator/Memory.h"
#include "core/Bus.h"
#include "core/Core.h"
#include "core/Packet.h"
#include "interface/MemTTransLdRes.h"
#include "interface/MemTTransStRes.h"
#include "interface/TTransMemLdReq.h"
#include "interface/TTransMemStReq.h"
#include "mtccore/l1/MTC_L1Clusters.h"
#include "mtccore/l1/MTC_SCBuffer.h"
#include "l2/L2Cache.h"
#include "mtccore/lsu/load_unit/MtcLDQInfo.h"
#include "lsu/FakeLSU.h"
#include "mtccore/lsu/lsu_interface.h"
#include "mtccore/lsu/mtc_lsu_stats.h"
#include "mtccore/lsu/mdb/MtcMDB.h"
#include "mtccore/lsu/load_unit/scalper.h"
#include "mtccore/lsu/prefetch/MtcPrefetcher.h"
#include "ModelSpec.h"
#include "SimSys.h"
#include "store_unit/MtcStoreUnit.h"
#if defined GENERIC_SOC || defined GENERIC_SOC_NEW
    #include "generic_soc/soc_wrapper.h"
#else
    #include "soc/soc_wrapper.h"
#endif

namespace JCore {

class Core;

class MtcLoadStoreUnit : public SimObj {
public:
    MtcLoadStoreUnit() = default;
    ~MtcLoadStoreUnit() override;

    Core                                        *core = nullptr;
    SimSys                                      *sim = nullptr;
    mLSUConfig                                   configs;
    MtcLSUInterface                              intf;
    LSUType                                      typeId = LSUType::MEMORY_LSU;

    std::shared_ptr<MTC_L1Top>                    MTC_L1Cache = nullptr;
    std::shared_ptr<L2Cache>                      l2Cache = nullptr;

    /* \brief Pointer to System-on-Chip */
#if defined GENERIC_SOC || defined GENERIC_SOC_NEW
    std::shared_ptr<SocGeneric>                   soc;
#else
    std::shared_ptr<SOC>                        soc;
#endif
    uint32_t                                    memIdS;
    std::shared_ptr<MtcLSUStats>                   stats;
    std::shared_ptr<DebugLog>                   debugLogger = nullptr;
    BlockROB                                    *brob; // Use for FakeLSU

    /* from IEX */
    INPUT std::vector<SimQueue<MemReqBus>*>     iex_lsu_lda_array;
    INPUT std::vector<SimQueue<MemReqBus>*>     iex_lsu_sta_array;
    INPUT std::vector<SimQueue<MemReqBus>*>     iex_lsu_std_array;
    /* to IEX */
    OUTPUT std::vector<SimQueue<MemReqBus>*>    lsu_iex_lret_array;
    /* to PE */
    OUTPUT std::vector<SimQueue<MemReqBus>*>    lsu_pe_rslv_array;
    /* from tile trans */
    std::vector<SimQueue<TTransMemLdReq>*>      tTransMemLdReqArray;
    std::vector<SimQueue<TTransMemStReq>*>      tTransMemStReqArray;
    /* to tile trans */
    std::vector<SimQueue<MemTTransLdRes>*>      tTransMemLdResArray;
    std::vector<SimQueue<MemTTransStRes>*>      tTransMemStResArray;

    /* to flushcontrol */
    OUTPUT SimQueue<FlushReq>                   *lsu_flush_rpt_q = nullptr;
    // Instruction Interface
    SimQueue<PtrPacket>                         *inst_l2_q;
    SimQueue<PtrPacket>                         *hpref_l2_q;
    SimQueue<PtrPacket>                         *l2_inst_q;
    SimQueue<PtrPacket>                         *l2_bfu_q;
    SimQueue<PtrPacket>                         *snp_l2_q;
    SimQueue<VecTileRegStReq>                   *mtciexScbReqQ;

    // Memory Interface
    SimQueue<GFUMemReq>                         *pkt_in_q;
    SimQueue<GFUMemReq>                         *pkt_out_q;

    // Send to specific L1 cluster
    void                                        SendL1(std::vector<std::deque<MemReqBus>*> &busArray, MemReqBus &bus);
    bool                                        SendSimL1(std::vector<SimQueue<MemReqBus>*> &busArray, MemReqBus &bus);
    bool                                    CheckSimStall(std::vector<SimQueue<MemReqBus>*> &busArray, MemReqBus &bus);

    bool                                        L1Allow();
    bool                                        L2Allow();
    void                                        AaccelssSoftMemory(MemReqBus &memReq);
    bool                                        CheckMemoryReq(uint64_t tag);
    uint32_t                                    GetScalpersize();
    /* Check/Stats Interface */
    bool                                        CheckLoadStall();
    bool                                        CheckLoadCltStall(uint64_t addr, uint32_t width);
    bool                                        CheckStoreStall();
    bool                                        CheckStoreStall(uint32_t size);
    void                                        ResetStats(void);
    void                                        StatsMemBound(bool& anyload, bool& l1miss, bool& l2miss,
                                                    bool& stqfull);
    /* flush when oldest request cannot arrive for long time */
    void                                        GenDeadLockFlush(MemReqBus &mem);

    /* Work for PMU */
    std::unordered_map<uint64_t, uint64_t>      addrCounter;
    void                                        CountAddr(uint64_t addr);

    /* Basic Interface */
    void                                        Reset() override;
    void                                        Work() override;
    void                                        Xfer() override;
    void                                        Build();
    void                                        BuildInterface();
    void                                        SetFlush(FlushBus flushReq);
    bool                                        GetVerbose();
    SimSys                                      *GetSim() override;
    void                                        Report();
    void ReportStat() override {}
    std::vector<SimQueue<MemReqBus>*>           &GetIEXQueue(vector<std::vector<SimQueue<MemReqBus>*>> &lsuQueue);
    Scalper                                     sc;
private:
    MtcLDQInfo                                     lu;
    MtcStoreUnit                                   su;
    MtcMDB                                         mdb;
    MtcPrefetcher                                  prefetcher;
    FakeLSU                                        fakeLSU;
    bool                                        verbose = false;
};

} // namespace JCore