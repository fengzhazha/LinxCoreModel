#include "core/Bus.h"
#include "mtccore/l1/MTC_L1Clusters.h"
#include "mtccore/l1/MTC_L1Top.h"

namespace JCore {
using namespace std;
void MTC_L1Clusters::Reset(void)
{
    dcache.Reset();
    scb.Reset();

    tag_lu_l1_q->clear();
    lookup_lu_l1_q->clear();
    tag_lu_scb_q->clear();
    lookup_lu_scb_q->clear();
    resp_l2_l1_q->clear();
    wakeup_scb_lu_q->Reset();
    top->tag_l1_lu_q->clear();
    top->tag_l1_scalper_q->clear();
    top->lookup_l1_lu_q->clear();
    top->tag_scb_lu_q->clear();
    top->lookup_scb_lu_q->clear();
    top->wakeup_l1_scalper_q->Reset();
}

void MTC_L1Clusters::Work(void)
{
    scb.Work();
    queryByTag();
    lookupCache();
    refillCache();
    lookupSCB();
    commitStore();
}

void MTC_L1Clusters::Xfer(void)
{
    scb.Xfer();
    allocCachePort();
}

void MTC_L1Clusters::Build(void)
{
    stats = std::make_shared<MtcLSUStats>();

    dcache.cluster = this;
    dcache.pConfigs = pConfigs;
    dcache.sim = GetSim();
    dcache.lsuConfigs = top->lsuConfigs;
    dcache.debugLogger = top->debugLogger;
    dcache.Build();

    scb.cluster = this;
    scb.scbID = cID;
    scb.memID_s = memID_s;
    scb.dcache = &dcache;
    scb.configs = &top->configs;
    scb.stats = stats;
    scb.sim = GetSim();
    scb.lsuConfigs = top->lsuConfigs;
    scb.debugLogger = top->debugLogger;
    scb.pkt_out_q = pkt_out_q;
    scb.wakeup_scb_lu_q = wakeup_scb_lu_q;
    scb.top = this;
    scb.Build();
}

SimSys *MTC_L1Clusters::GetSim(void)
{
    return sim;
}

bool MTC_L1Clusters::hasDCacheReq(void)
{
    return !lookup_lu_l1_q->empty();
}

void MTC_L1Clusters::allocCachePort(void)
{
    // Priorty: refill > lookup
    if (!resp_l2_l1_q->empty()) {
        dcacheAllow = false;
        return;
    }
    dcacheAllow = true;
}

void MTC_L1Clusters::queryByTag(void)
{
    if (!dcacheAllow) {
        return;
    }

    for (uint32_t i = 0; i < pConfigs->cluster_width && !tag_lu_l1_q->empty(); ++i) {
        MemReqBus bus = tag_lu_l1_q->front();
        tag_lu_l1_q->pop_front();
        handleTagQuery(bus);
    }

    for (uint32_t i = 0; i < pConfigs->cluster_width && !tag_scalper_l1_q->empty(); ++i) {
        MemReqBus bus = tag_scalper_l1_q->front();
        tag_scalper_l1_q->pop_front();
        handleTagQuery(bus);
    }

    for (uint32_t i = 0; i < pConfigs->cluster_width && !pref_lu_l1_q->empty(); ++i) {
        MemReqBus bus = pref_lu_l1_q->front();
        pref_lu_l1_q->pop_front();
        handleTagQuery(bus);
    }
}

void MTC_L1Clusters::lookupCache(void)
{
    if (!dcacheAllow) {
        return;
    }

    for (uint32_t i = 0; i < pConfigs->cluster_width && !lookup_lu_l1_q->empty(); ++i) {
        MemReqBus bus = lookup_lu_l1_q->front();
        lookup_lu_l1_q->pop_front();
        handleDCacheLookup(bus);
    }
}

void MTC_L1Clusters::lookupSCB(void)
{
    for (uint32_t i = 0; i < pConfigs->cluster_width && !tag_lu_scb_q->empty(); ++i) {
        MemReqBus bus = tag_lu_scb_q->front();
        tag_lu_scb_q->pop_front();
        handleSCBLookup(bus, true);
    }

    for (uint32_t i = 0; i < pConfigs->cluster_width && !lookup_lu_scb_q->empty(); ++i) {
        MemReqBus bus = lookup_lu_scb_q->front();
        lookup_lu_scb_q->pop_front();
        handleSCBLookup(bus, false);
    }
}

void MTC_L1Clusters::refillCache(void)
{
    for (uint32_t i = 0; i < pConfigs->cluster_width && !resp_l2_l1_q->empty(); ++i) {
        if (top->wakeup_l1_lu_q->toBeOverflow(1)) {
            break;
        }
        PtrPacket pkt = resp_l2_l1_q->front();
        resp_l2_l1_q->pop_front();
        handleRefill(pkt);
    }
}

void MTC_L1Clusters::commitStore(void)
{
    for (uint32_t i = 0; i < pConfigs->cluster_width && !commit_su_scb_q->Empty() && !scb.full(); ++i) {
        MemReqBus bus = commit_su_scb_q->Read();
        scb.insert(bus.addr, bus.size, bus.data);
        if ((VERBOSE_ON && STAGE_ENABLED(StageID::LSU_ALL)) || pConfigs->verbose) {
            printf("[CLT%u]: store ", scb.scbID);
            cout << bus;
            printf(" is written to SCB%d. tag 0x%lx, addr 0x%lx, size 0x%u, data 0x%lx\n", scb.scbID, bus.addr&~0x3f,
                bus.addr, bus.size, bus.data);
        }
        if (DEBUG_VERBOSE_ON || top->lsuConfigs->verbose) {
            LOG_DEBUG_STRUCT(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                             bus, "L1 [CLT%u]: store is written to SCB%d. tag 0x%lx, addr 0x%lx, size 0x%u, "
                             "data 0x%lx. Req: ", scb.scbID, scb.scbID, bus.addr&~0x3f, bus.addr, bus.size, bus.data);
        }
    }
    if (scb.full() || !commit_su_scb_q->Empty()) {
        commit_su_scb_q->setStall();
    } else {
        commit_su_scb_q->unsetStall();
    }
}

void MTC_L1Clusters::handleTagQuery(MemReqBus &bus)
{
    bus.l1_miss = !dcache.queryByTag(bus.tag);
    if ((VERBOSE_ON && STAGE_ENABLED(StageID::LSU_ALL)) || pConfigs->verbose) {
        cout << "[CLT" << cID << "] query tag " << bus << " miss: " << bus.l1_miss <<  endl;
    }
    if (DEBUG_VERBOSE_ON || top->lsuConfigs->verbose) {
        LOG_DEBUG_STRUCT(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                         bus, "L1 [CLT%u]: query tag, miss:%d, MemReqBus: ", cID, bus.l1_miss);
    }

    if (bus.fromScalper) {
      //  cout << "send to scalper " << bus.tag << endl;
        top->tag_l1_scalper_q->push_back(bus);
        return;
    }

    if (bus.prefetch) {
        top->pref_l1_lu_q->push_back(bus);
        return;
    }

    top->tag_l1_lu_q->push_back(bus);
}

void MTC_L1Clusters::handleDCacheLookup(MemReqBus &bus)
{
    if (!dcache.lookup(bus.tag)) {
        bus.data_vld = false;
    } else {
        uint2048_t data = dcache.getData(bus.tag);
        bus.mtc_reqData.insertCacheData(data);
        bus.data_vld = true;
    }
    top->lookup_l1_lu_q->push_back(bus);

    if ((VERBOSE_ON && STAGE_ENABLED(StageID::LSU_ALL)) || pConfigs->verbose) {
        cout << "[CLT" << cID << "] lookup L1 D-Cache " << bus << " ";
        if (!bus.data_vld)
            cout << " miss" << endl;
        else
            cout << bus.mtc_reqData.data.toStr()<< endl;
    }
    if (DEBUG_VERBOSE_ON || top->lsuConfigs->verbose) {
        std::string info;
        if (!bus.data_vld) {
            info += " miss";
        } else {
            info += bus.mtc_reqData.data.toStr();
        }
        LOG_DEBUG_STRUCT(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                         bus, "L1 [CLT%u]: lookup L1 D-Cache, %s, ", cID, info.c_str());
    }
}

void MTC_L1Clusters::handleSCBLookup(MemReqBus &bus, bool checkOnly)
{
    // SCB
    bool hit = false;
    uint8_t *scbData;
    bool *scbValid;
    bus.mtc_reqData.Reset();
    if (scb.lookup(bus.tag, &scbData, &scbValid)) {
        UpdateData(bus.mtc_reqData.data, scbData, scbValid);
        for (uint32_t i = 0; i < pConfigs->dcache_way_size; i++) {
            bus.mtc_reqData.positionVld[i] = scbValid[i];
        }
        hit = true;
    }

    auto commit_read_q = commit_su_scb_q->GetRawReadData();
    for (auto &st : commit_read_q) {
        // Iterate from old to young
        if (AddrOverlap(st.addr, st.size, bus.addr, bus.size)) {
            if (OpcodeManager::Inst().GetOpcodeGroup(st.opcode) == InstGroup::CACHE_MAINTAIN) {
                bus.mtc_reqData.zero();
                hit = true;
                continue;
            }
            UpdateData(bus.mtc_reqData.data, st.addr, st.size, st.data, bus.tag);
            MTCUpdateSTValid(bus.mtc_reqData.positionVld, st.addr, st.size, true, bus.tag);
            hit = true;
        }
    }
    auto commit_write_q = commit_su_scb_q->GetRawWriteData();
    for (auto &st : commit_write_q) {
        // Iterate from old to young
        if (AddrOverlap(st.addr, st.size, bus.addr, bus.size)) {
            if (OpcodeManager::Inst().GetOpcodeGroup(st.opcode) == InstGroup::CACHE_MAINTAIN) {
                bus.mtc_reqData.zero();
                hit = true;
                continue;
            }

            UpdateData(bus.mtc_reqData.data, st.addr, st.size, st.data, bus.tag);
            MTCUpdateSTValid(bus.mtc_reqData.positionVld, st.addr, st.size, true, bus.tag);
            hit = true;
        }
    }
    bus.data_vld = hit;

    if (checkOnly) {
        top->tag_scb_lu_q->push_back(bus);
    } else {
        top->lookup_scb_lu_q->push_back(bus);
    }
    if ((VERBOSE_ON && STAGE_ENABLED(StageID::LSU_ALL)) || pConfigs->verbose) {
        cout << "[CLT" << cID << "] lookup SCB " << bus << " ";
        if (!bus.data_vld)
            cout << " miss" << endl;
        else
            cout << bus.mtc_reqData.data.toStr()<< endl;
    }
    if (DEBUG_VERBOSE_ON || top->lsuConfigs->verbose) {
        std::string info;
        if (!bus.data_vld) {
            info += " miss";
        } else {
            info += bus.mtc_reqData.data.toStr();
        }
        LOG_DEBUG_STRUCT(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                         bus, "L1 [CLT%u]: lookup SCB, %s, ", cID, info.c_str());
    }
}

void MTC_L1Clusters::handleRefill(PtrPacket &pkt)
{
    pkt->tid = pkt->tid >> 2;
    bool duplicate = dcache.lookup(pkt->addr);
    pkt->mtc_data = duplicate ? dcache.getData(pkt->addr) : pkt->mtc_data;
    top->wakeup_l1_lu_q->Write(pkt);

    // send to scalper, wakeup L1 miss
    MemReqBus bus;
    bus.tag = pkt->addr;
    if (pkt->isRead()) {
        top->wakeup_l1_scalper_q->Write(bus);
    }
    
    if (pkt->isDup()) {
        if ((VERBOSE_ON && STAGE_ENABLED(StageID::LSU_ALL)) || pConfigs->verbose) {
            printf("[CLT%u]: pkt 0x%lx is duplicated in L1\n", cID, pkt->addr);
        }
        if (DEBUG_VERBOSE_ON || top->lsuConfigs->verbose) {
            LOG_DEBUG(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                      "L1 [CLT%u]: pkt 0x%lx is duplicated in L1", cID, pkt->addr);
        }
        return;
    }

    if ((VERBOSE_ON && STAGE_ENABLED(StageID::LSU_ALL)) || pConfigs->verbose) {
        cout << "[CLT" << cID << "]: packet " << *pkt << " is refilled\n";
    }

    if (DEBUG_VERBOSE_ON || top->lsuConfigs->verbose) {
        LOG_DEBUG_STRUCT(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                         *pkt, "L1 [CLT%u]: packet is refilled, packet: ", cID);
    }

    (void)dcache.refill(pkt->addr, pkt->mtc_data.bits);
    stats->dcache_refill_count++;
}

void MTC_L1Clusters::broadcastUpgrade(uint64_t addr, uint2048_t &data, bool *p)
{
 // when SCB upgrade
    MemReqBus bus;
    bus.vld = true;
    bus.toMtcLsu = true;
    bus.addr = (addr & ~0xff);
    bus.tag = (addr & ~0xff);
    bus.size = pConfigs->dcache_way_size;
    bus.data_vld = true;
    bus.mtc_reqData.insertValidCacheData(data, p);
    top->upgrade_l1_lu_q->Write(bus);
}

bool MTC_L1Clusters::dataAllow()
{
    return dcacheAllow;
}

} // namespace JCore