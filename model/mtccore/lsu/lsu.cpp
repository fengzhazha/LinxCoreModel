#include "mtccore/lsu/MtcLoadStoreUnit.h"
#include "mtccore/l1/MTC_L1Top.h"

namespace JCore {

void MtcLoadStoreUnit::Reset()
{
    lu.Reset();
    su.Reset();
    ResetStats();
}


void MtcLoadStoreUnit::Build()
{
    configs.overrideDefaultConfig(GetSim()->getCfgs());
    stats = std::make_shared<MtcLSUStats>(GetSim()->getRpt());
    MTC_L1Cache = std::make_shared<MTC_L1Top>();
    l2Cache = std::make_shared<L2Cache>();
    BuildInterface();

    su.pConfigs = &configs;
    su.core = core;
    su.top = this;
    su.sim = sim;
    su.Build();

    lu.sim = sim;
    lu.top = this;
    lu.pConfigs = &configs;
    lu.sim = sim;
    lu.Build();

    if (typeId == LSUType::MEMORY_LSU) {
        configs.pref_enable = false;
    }

    sc.sim = sim;
    sc.top = this;
    sc.pConfigs = &configs;
    sc.sim = sim;
    sc.Reset();

    prefetcher.configs = &configs;
    prefetcher.sim = GetSim();
    prefetcher.top = this;
    prefetcher.Build();

    mdb.sim = sim;
    mdb.core = core;
    mdb.pConfigs = &configs;
    mdb.top = this;
    mdb.Build();

    MTC_L1Cache->sim = GetSim();
    MTC_L1Cache->top = this;
    MTC_L1Cache->lsuConfigs = &configs;
    MTC_L1Cache->debugLogger = debugLogger;
    MTC_L1Cache->memID_s = memIdS;
    MTC_L1Cache->Build();

    l2Cache->sim = GetSim();
    l2Cache->debugLogger = debugLogger;
    l2Cache->memID_s = memIdS;
    l2Cache->l2_mem_q = pkt_out_q;
    l2Cache->mem_l2_q = pkt_in_q;
    l2Cache->inst_l2_q = inst_l2_q;
    l2Cache->hpref_l2_q = hpref_l2_q;
    l2Cache->l2_inst_q = l2_inst_q;
    l2Cache->snp_l2_q = snp_l2_q;
    l2Cache->Build();
    fakeLSU.pe_ret_data_q.resize(core->GetPECount());
    fakeLSU.pe_resolve_q.resize(core->GetPECount());
    fakeLSU.pe_wakeup_q.resize(core->GetPECount());

    lu.Reset();
    su.Reset();
    mdb.Reset();
    prefetcher.Reset();
    MTC_L1Cache->Reset();
    l2Cache->Reset();

    lu.pref_throw = &l2Cache->pref_throw;
}

void MtcLoadStoreUnit::BuildInterface()
{
    // TODO: init for L1 cluster
    uint32_t cltCnt = configs.lsu_width;
    intf.pref_lu_l1_array.resize(cltCnt);
    for (auto &pref_lu_l1_q : intf.pref_lu_l1_array) {
        lu.pref_lu_l1_array.emplace_back(&pref_lu_l1_q);
        MTC_L1Cache->pref_lu_l1_array.emplace_back(&pref_lu_l1_q);
    }
    intf.tag_lu_l1_array.resize(cltCnt);
    for (auto &tag_lu_l1_q : intf.tag_lu_l1_array) {
        lu.tag_lu_l1_array.emplace_back(&tag_lu_l1_q);
        MTC_L1Cache->tag_lu_l1_array.emplace_back(&tag_lu_l1_q);
    }
    intf.tag_scalper_l1_array.resize(cltCnt);
    for (auto &tag_scalper_l1_q : intf.tag_scalper_l1_array) {
        sc.tag_scalper_l1_array.emplace_back(&tag_scalper_l1_q);
        MTC_L1Cache->tag_scalper_l1_array.emplace_back(&tag_scalper_l1_q);
    }
    intf.lookup_lu_l1_array.resize(cltCnt);
    for (auto &lookup_lu_l1_q : intf.lookup_lu_l1_array) {
        lu.lookup_lu_l1_array.emplace_back(&lookup_lu_l1_q);
        MTC_L1Cache->lookup_lu_l1_array.emplace_back(&lookup_lu_l1_q);
    }
    intf.lookup_lu_scb_array.resize(cltCnt);
    for (auto &lookup_lu_scb_q : intf.lookup_lu_scb_array) {
        lu.lookup_lu_scb_array.emplace_back(&lookup_lu_scb_q);
        MTC_L1Cache->lookup_lu_scb_array.emplace_back(&lookup_lu_scb_q);
    }
    intf.tag_lu_scb_array.resize(cltCnt);
    for (auto &tag_lu_scb_q : intf.tag_lu_scb_array) {
        lu.tag_lu_scb_array.emplace_back(&tag_lu_scb_q);
        MTC_L1Cache->tag_lu_scb_array.emplace_back(&tag_lu_scb_q);
    }
    intf.commit_su_scb_array.resize(cltCnt);
    for (auto &commit_su_scb_q : intf.commit_su_scb_array) {
        su.commit_su_scb_array.emplace_back(&commit_su_scb_q);
        MTC_L1Cache->commit_su_scb_array.emplace_back(&commit_su_scb_q);
    }

    su.lookup_lu_su_q = &intf.lookup_lu_su_q;
    su.wait_lu_su_q = &intf.wait_lu_su_q;
    su.lookup_su_lu_q = &intf.lookup_su_lu_q;
    su.wait_su_lu_q = &intf.wait_su_lu_q;
    su.bypass_su_lu_q = &intf.bypass_su_lu_q;
    su.detect_su_lu_q = &intf.detect_su_lu_q;
    su.wakeup_su_lu_q = &intf.wakeup_su_lu_q;
    su.lookup_mdb_su_q = &intf.lookup_mdb_su_q;
    su.atomic_lu_su_q = &intf.atomic_lu_su_q;
    ASSERT(su.atomic_lu_su_q);

    lu.lookup_lu_su_q = &intf.lookup_lu_su_q;
    lu.wait_lu_su_q = &intf.wait_lu_su_q;
    lu.lookup_su_lu_q = &intf.lookup_su_lu_q;
    lu.wait_su_lu_q = &intf.wait_su_lu_q;
    lu.bypass_su_lu_q = &intf.bypass_su_lu_q;
    lu.detect_su_lu_q = &intf.detect_su_lu_q;
    lu.wakeup_su_lu_q = &intf.wakeup_su_lu_q;
    lu.pref_lu_q = intf.pref_lu_q;
    lu.lu_pref_q = intf.lu_pref_q;
    lu.lookup_lu_mdb_q = &intf.lookup_lu_mdb_q;
    lu.delete_lu_mdb_q = &intf.delete_lu_mdb_q;
    lu.record_lu_mdb_q = &intf.record_lu_mdb_q;
    lu.tag_l1_lu_q = &intf.tag_l1_lu_q;
    sc.tag_l1_scalper_q = &intf.tag_l1_scalper_q;
    sc.pe_scalper_commit_q = intf.pe_scalper_commit_q;
    lu.lookup_l1_lu_q = &intf.lookup_l1_lu_q;
    lu.tag_scb_lu_q = &intf.tag_scb_lu_q;
    lu.lookup_scb_lu_q = &intf.lookup_scb_lu_q;
    lu.wakeup_scb_lu_q = &intf.wakeup_scb_lu_q;
    lu.lookup_mdb_lu_q = &intf.lookup_mdb_lu_q;
    lu.upgrade_l1_lu_q = &intf.upgrade_l1_lu_q;
    lu.atomic_lu_su_q = &intf.atomic_lu_su_q;
    lu.wakeup_l1_lu_q = &intf.wakeup_l1_lu_q;
    sc.wakeup_l1_scalper_q = &intf.wakeup_l1_scalper_q;
    sc.scLdqOrder = &intf.scLdqOrder;
    lu.scLdqOrder = &intf.scLdqOrder;
    sc.iexScalperInstQ = intf.iexScalperInstQ;
    sc.pe_iex_sc_lda_array = intf.pe_iex_sc_lda_array;
    sc.pe_iex_lda_array = intf.pe_iex_lda_array;
    ASSERT(lu.atomic_lu_su_q);
    // lu.snoop_l1_lu_q = &intf.snoop_l1_lu_q;
    lu.pref_l1_lu_q = &intf.pref_l1_lu_q;
    lu.snoop_lu_l2_q = snp_l2_q; // TODO: snoop interface is in core
    lu.lookup_lu_l2_q = intf.lookup_lu_l2_q;

    mdb.lookup_lu_mdb_q = &intf.lookup_lu_mdb_q;
    mdb.delete_lu_mdb_q = &intf.delete_lu_mdb_q;
    mdb.record_lu_mdb_q = &intf.record_lu_mdb_q;
    mdb.lookup_mdb_lu_q = &intf.lookup_mdb_lu_q;
    mdb.lookup_mdb_su_q = &intf.lookup_mdb_su_q;

    MTC_L1Cache->tag_l1_lu_q = &intf.tag_l1_lu_q;
    MTC_L1Cache->tag_l1_scalper_q = &intf.tag_l1_scalper_q;
    MTC_L1Cache->pref_l1_lu_q = &intf.pref_l1_lu_q;
    MTC_L1Cache->lookup_l1_lu_q = &intf.lookup_l1_lu_q;
    MTC_L1Cache->tag_scb_lu_q = &intf.tag_scb_lu_q;
    MTC_L1Cache->lookup_scb_lu_q = &intf.lookup_scb_lu_q;
    MTC_L1Cache->wakeup_scb_lu_q = &intf.wakeup_scb_lu_q;
    MTC_L1Cache->wakeup_l1_lu_q = &intf.wakeup_l1_lu_q;
    MTC_L1Cache->upgrade_l1_lu_q = &intf.upgrade_l1_lu_q;
    MTC_L1Cache->lookup_l2_l1_q = intf.lookup_l2_l1_q;
    MTC_L1Cache->wakeup_l1_scalper_q = &intf.wakeup_l1_scalper_q;
    MTC_L1Cache->peScbWrQ = intf.peScbWrQ;

    prefetcher.pref_l1_q = intf.pref_lu_q;
    prefetcher.pref_l2_q = intf.pref_l2_q;
    prefetcher.lu_pref_q = intf.lu_pref_q;
    prefetcher.l1_pref_q = intf.feedback_lu_pref_q;
    prefetcher.l2_pref_q = intf.l2_pref_q;

    l2Cache->l1_l2_q = intf.lookup_lu_l2_q;
    l2Cache->l2_l1_q = intf.lookup_l2_l1_q;
    l2Cache->pref_l2_q = intf.pref_l2_q;
    l2Cache->l2_pref_q = intf.l2_pref_q;

    lsu_flush_rpt_q = new SimQueue<FlushReq>();
}

bool MtcLoadStoreUnit::CheckMemoryReq(uint64_t tag)
{
    return lu.sendMissReqToL2(tag);
}

void MtcLoadStoreUnit::Work()
{
    ASSERT(typeId == LSUType::MEMORY_LSU);
    if (configs.seq_mode) {
        // increase frequentcy for fast simulation
        fakeLSU.Work();
        fakeLSU.Work();
        fakeLSU.Work();
        fakeLSU.Work();
        fakeLSU.returnPE();
        return;
    }
    prefetcher.l2_data_allow = l2Cache->pfl2_allow() && !intf.pref_l2_q->getStall();

    lu.Work();
    sc.Work();
    su.Work();
    mdb.Work();
    MTC_L1Cache->Work();
    l2Cache->Work();
    prefetcher.Work();

    if (core->configs.soc_enable) {
        // L2-Interaction and SoC Interface: Receive GFU Memory (L2) response from SoC --> Collect L1
        // Miss request --> Send Missed ld/st request --> SoC Work() and Xfer()
        // soc_enable need to call soc->Work();
    }
    stats->cycles++;
}

void MtcLoadStoreUnit::Xfer()
{
    lu.Xfer();
    su.Xfer();
    mdb.Xfer();
    intf.Work();
    MTC_L1Cache->Xfer();
    l2Cache->Xfer();
    prefetcher.Xfer();
}

void MtcLoadStoreUnit::SetFlush(FlushBus flushReq)
{
    if (!flushReq.req.vld) {
        return;
    }

    if (configs.seq_mode) {
        ROBID ptr = flushReq.req.bid;
        for (uint32_t i = 0; i < fakeLSU.fakeLSU.size(); i++) {
            fakeLSU.fakeLSU[ptr.val].memSet.clear();
            fakeLSU.fakeLSU[ptr.val].oldest.val = 0;
            fakeLSU.fakeLSU[ptr.val].oldest.wrap = false;
            IncROBID(ptr, fakeLSU.fakeLSU.size());
            if (ptr.val == flushReq.old_alloc.val) break;
        }
        return;
    }

    lu.flush(flushReq);
    su.flush(flushReq);
}

bool MtcLoadStoreUnit::L1Allow()
{
    return MTC_L1Cache->dataAllow();
}

bool MtcLoadStoreUnit::L2Allow()
{
    return l2Cache->data_allow();
}

bool MtcLoadStoreUnit::GetVerbose()
{
    return verbose;
}

SimSys *MtcLoadStoreUnit::GetSim()
{
    return sim;
}

void MtcLoadStoreUnit::SendL1(std::vector<std::deque<MemReqBus>*> &busArray, MemReqBus &bus)
{
    uint32_t l1cID = MTC_L1Cache->addr2L1cID(bus.addr);
    busArray[l1cID]->push_back(bus);
}

bool MtcLoadStoreUnit::SendSimL1(std::vector<SimQueue<MemReqBus>*> &busArray, MemReqBus &bus)
{
    uint32_t l1cID = MTC_L1Cache->addr2L1cID(bus.addr);
    if (busArray[l1cID]->getStall()) {
        return false;
    }

    busArray[l1cID]->Write(bus);
    return true;
}

bool MtcLoadStoreUnit::CheckSimStall(std::vector<SimQueue<MemReqBus>*> &busArray, MemReqBus &bus)
{
    uint32_t l1cID = MTC_L1Cache->addr2L1cID(bus.addr);
    return busArray[l1cID]->getStall();
}

void MtcLoadStoreUnit::StatsMemBound(bool& anyload, bool& l1miss, bool& l2miss, bool& stqfull)
{
    // memstall_anyload: anyload is inflight
    // memstall_l1miss: any inflight load has missed L1
    // memstall_l2miss: any inflight load has missed L2
    // memstall_store: no more stores can be issued (?)
    if (configs.seq_mode) {
        return; // no LSU stall in seq mode
    }

    lu.statsMemBound(anyload, l1miss, l2miss);
    stqfull = su.checkStall();
}

void MtcLoadStoreUnit::Report(void)
{
    if (configs.seq_mode) {
        fakeLSU.stats->report("Fake LSU");
        return;
    }
    stats->report("MTC - Memory LSU");
    l2Cache->ReportStat();
}

void MtcLoadStoreUnit::ResetStats(void)
{
    if (configs.seq_mode) {
        fakeLSU.stats->Reset();
        return;
    }
    if (stats != nullptr) {
        stats->Reset();
    }
    if ((l2Cache != nullptr) && (l2Cache->stats != nullptr)) {
        l2Cache->stats->Reset();
    }
}

void MtcLoadStoreUnit::AaccelssSoftMemory(MemReqBus &memReq)
{
    if (!memReq.vld) return;
    switch (memReq.opcode) {
        case Opcode::OP_LDI:
        case Opcode::OP_LR_D:
        case Opcode::OP_LD_PCR:
            memReq.data = GetSim()->loadData(memReq.addr, 8, false);
            break;
        case Opcode::OP_LWI:
        case Opcode::OP_LR_W:
        case Opcode::OP_LW_PCR:
            memReq.data = GetSim()->loadData(memReq.addr, 4, true);
            break;
        case Opcode::OP_LHI:
        case Opcode::OP_LH_PCR:
            memReq.data = GetSim()->loadData(memReq.addr, 2, true);
            break;
        case Opcode::OP_LBI:
        case Opcode::OP_LB_PCR:
            memReq.data = GetSim()->loadData(memReq.addr, 1, true);
            break;
        case Opcode::OP_LWUI:
        case Opcode::OP_LWU_PCR:
            memReq.data = GetSim()->loadData(memReq.addr, 4, false);
            break;
        case Opcode::OP_LHUI:
        case Opcode::OP_LHU_PCR:
            memReq.data = GetSim()->loadData(memReq.addr, 2, false);
            break;
        case Opcode::OP_LBUI:
        case Opcode::OP_LBU_PCR:
            memReq.data = GetSim()->loadData(memReq.addr, 1, false);
            break;
        case Opcode::OP_SDI:
        case Opcode::OP_SC_D:
        case Opcode::OP_SD_PCR:
            GetSim()->storeData(memReq.addr, memReq.data, 8);
            break;
        case Opcode::OP_SWI:
        case Opcode::OP_SC_W:
        case Opcode::OP_SW_PCR:
            GetSim()->storeData(memReq.addr, memReq.data, 4);
            break;
        case Opcode::OP_SHI:
        case Opcode::OP_SH_PCR:
            GetSim()->storeData(memReq.addr, memReq.data, 2);
            break;
        case Opcode::OP_SBI:
        case Opcode::OP_SB_PCR:
            GetSim()->storeData(memReq.addr, memReq.data, 1);
            break;
        default:
            ASSERT(0 && "Unrecognized opcode in LSU");
            break;
    }
}

bool MtcLoadStoreUnit::CheckLoadStall()
{
    return lu.checkStall();
}

bool MtcLoadStoreUnit::CheckLoadCltStall(uint64_t addr, uint32_t width)
{
    return lu.checkCltStall(addr, width);
}

bool MtcLoadStoreUnit::CheckStoreStall()
{
    return su.checkStall();
}

uint32_t MtcLoadStoreUnit::GetScalpersize()
{
    return sc.GetScalperSize();
}

bool MtcLoadStoreUnit::CheckStoreStall(uint32_t size)
{
    return su.checkStall(size);
}

void MtcLoadStoreUnit::CountAddr(uint64_t addr)
{
    if (!configs.addr_cnt_enable) {
        return;
    }

    auto it = addrCounter.find(addr);
    if (it != addrCounter.end()) {
        it->second++;
    } else {
        stats->addrCount++;
        addrCounter.insert(std::pair<uint64_t, uint64_t>(addr, 0));
    }
}

std::vector<SimQueue<MemReqBus>*> &MtcLoadStoreUnit::GetIEXQueue(vector<std::vector<SimQueue<MemReqBus>*>> &lsuQueue)
{
    if (typeId == LSUType::SCALAR_LSU) {
        return lsuQueue[SCALAR_IEX];
    }

    assert(typeId == LSUType::MEMORY_LSU);
    return lsuQueue[MEM_IEX];
}

MtcLoadStoreUnit::~MtcLoadStoreUnit()
{
    DeletePtr(lsu_flush_rpt_q);
}

} // namespace JCore