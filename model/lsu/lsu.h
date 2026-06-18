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
#include "l1/clusters_top.h"
#include "l1/SCB.h"
#include "l2/L2Cache.h"
#include "load_unit/ldq.h"
#include "lsu/FakeLSU.h"
#include "lsu/lsu_interface.h"
#include "lsu/lsu_stats.h"
#include "lsu/mdb/MDB.h"
#include "lsu/prefetch/Prefetcher.h"
#include "ModelSpec.h"
#include "SimSys.h"
#include "store_unit/store_unit.h"

#if (defined GENERIC_SOC_NEW || defined GENERIC_SOC)
    #include "generic_soc/soc_wrapper.h"
#else
    #include "soc/soc_wrapper.h"
#endif

namespace JCore {

class Core;

class LoadStoreUnit : public SimObj {
private:
    std::vector<LDQInfo>                        lu;
    std::vector<StoreUnit>                      su;
    MDB                                         mdb;
    Prefetcher                                  prefetcher;
    FakeLSU                                     fakeLSU;
    bool                                        verbose = false;

public:
    LoadStoreUnit() {}
    virtual ~LoadStoreUnit();

    Core                                        *core = NULL;
    SimSys                                      *sim = NULL;
    LSUConfig                                   configs;
    LSUInterface                                intf;
    LSUType                                     typeId = LSUType::SCALAR_LSU;

    std::shared_ptr<L1Top>                      l1Cache = nullptr;
    std::shared_ptr<L2Cache>                    l2Cache = nullptr;

    uint32_t                                    memID_s;
    std::shared_ptr<LSUStats>                   stats;
    std::shared_ptr<DebugLog>                   debugLogger = nullptr;
    BlockROB                                    *brob; // Use for FakeLSU

    /* from IEX */
    INPUT std::vector<std::vector<SimQueue<MemReqBus>*>>     iex_lsu_lda_array;
    INPUT std::vector<std::vector<SimQueue<MemReqBus>*>>     iex_lsu_sta_array;
    INPUT std::vector<std::vector<SimQueue<MemReqBus>*>>     iex_lsu_std_array;
    /* LSU inner connections */
    INNER std::vector<SimQueue<MemReqBus>>      ldaArray;
    INNER std::vector<SimQueue<MemReqBus>>      staArray;
    INNER std::vector<SimQueue<MemReqBus>>      stdArray;

    /* to IEX */
    OUTPUT std::vector<std::vector<SimQueue<MemReqBus>*>>    lsu_iex_lret_array;
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

    // Memory Interface
    SimQueue<GFUMemReq>                         *pkt_in_q;
    SimQueue<GFUMemReq>                         *pkt_out_q;

    // Send to specific L1 cluster
    void                                        sendL1(std::vector<std::deque<MemReqBus>*> &busArray, MemReqBus &bus);
    bool                                        sendSimL1(std::vector<SimQueue<MemReqBus>*> &busArray, MemReqBus &bus);
    bool                                    checkSimStall(std::vector<SimQueue<MemReqBus>*> &busArray, MemReqBus &bus);

    bool                                        l1Allow();
    bool                                        l2Allow();
    void                                        aaccelssSoftMemory(MemReqBus &memReq);

    /* Check/Stats Interface */
    bool                                        CheckLoadStall(uint32_t stid);
    bool                                        CheckLoadCltStall(uint64_t addr, uint32_t width, uint32_t stid);
    bool                                        checkStoreStall(uint32_t stid);
    bool                                        CheckStoreStall(uint32_t size, uint32_t stid);
    void                                        resetStats(void);
    void                                        StatsMemBound(bool& anyload, bool& l1miss, bool& l2miss,
                                                    bool& stqfull, uint32_t stid);
    /* flush when oldest request cannot arrive for long time */
    void                                        genDeadLockFlush(MemReqBus &mem);

    /* Work for PMU */
    std::unordered_map<uint64_t, uint64_t>      addrCounter;
    void                                        countAddr(uint64_t addr);

    /* Basic Interface */
    void                                        Reset();
    void                                        Work();
    void                                        Xfer();
    void                                        Build();
    void                                        buildInterface();
    void                                        setFlush(FlushBus flushReq);
    bool                                        getVerbose();
    SimSys                                      *GetSim();
    void                                        ReportStat();
    std::vector<SimQueue<MemReqBus>*>           &GetIEXQueue(vector<std::vector<SimQueue<MemReqBus>*>> &lsuQueue);
    bool                                        CheckLoadQEmpty(uint32_t stid);
    void                                        LSURoute();
};

} // namespace JCore