#include "mtccore/lsu/mdb/MtcMDB.h"
#include "mtccore/lsu/MtcLoadStoreUnit.h"
#include "core/Core.h"

namespace JCore {

void MtcMDB::Reset()
{
    ssit.clear();
    lookup_lu_mdb_q->clear();
    record_lu_mdb_q->clear();
    lookup_mdb_lu_q->clear();
}

void MtcMDB::Work()
{
    if (verbose) {
        for (auto it : ssit) {
            cout<<"mdb ssit "<<it.first<<" conf"<<it.second.conf<<" weight "<<it.second.weight<<" sttpc 0x"
                <<hex<<it.second.st_pc<<endl;
        }
    }
    lookupMDB();
    deleteMDB();
    recordMDB();
}

void MtcMDB::Xfer()
{
}

SimSys* MtcMDB::GetSim()
{
    return sim;
}

void MtcMDB::Build()
{
    verbose = pConfigs->verbose;
}

bool MtcMDB::ld_lookup(uint64_t tpc, ROBID bid, uint64_t &st_pc, ROBID &st_bid)
{
    bool ssit_hit = false;
    bool first_after_nuke = false;
    SSITEntry *ssit_e;
    if (ssit.count(tpc) != 0) {
        ssit_hit = true;
        ssit_e = &ssit[tpc];
        st_pc  = ssit_e->st_pc;
        st_bid = bid;
        first_after_nuke = ssit_e->nukeVld && ssit_e->nukeBID == bid;
        ssit_e->nukeVld = false;
        SubROBID(st_bid, ssit_e->bid_off, core->configs.block_rob_depth);
    }
    return ssit_hit && !first_after_nuke && (ssit_e->conf > 0) && isStall(ssit_e->weight);
}

void MtcMDB::insert(uint64_t ld_pc, ROBID ld_bid, ROBID ld_lsid, uint64_t st_pc, ROBID st_bid, ROBID st_lsid, int conf)
{
    ASSERT(LessEqual(st_bid, ld_bid));
    uint32_t bid_off = DeltaBID(ld_bid, st_bid, core->configs.block_rob_depth);
    SSITEntry *ssit_e;
    if (ssit.count(ld_pc) == 0) {
        // Miss
        SSITEntry ent;
        ent.st_pc = st_pc;
        ent.bid_off = bid_off;
        ent.conf = conf;
        ent.nukeVld = true;
        ent.nukeBID = ld_bid;
        initWeight(ent.weight);
        ssit.emplace(ld_pc, ent);
        ssit_e = &ssit[ld_pc];
    } else {
        // hit
        ssit_e = &ssit[ld_pc];
        ssit_e->nukeVld = true;
        ssit_e->nukeBID = ld_bid;
        if (ssit_e->st_pc != st_pc) {
            uint32_t lsID_off = DeltaBID(ld_lsid, st_lsid, core->configs.block_rob_depth);
            if (ssit_e->conf < 1 || bid_off < ssit_e->bid_off || (bid_off == ssit_e->bid_off && lsID_off <
                ssit_e->lsID_off)) {
                ssit_e->st_pc   = st_pc;
                ssit_e->bid_off = bid_off;
                ssit_e->lsID_off = lsID_off;
                ssit_e->conf = conf;
                initWeight(ssit_e->weight);
            } else {
                ssit_e->conf--;
            }
        } else {
            ssit_e->conf = ((ssit_e->conf + 1) > 3) ? 3 : (ssit_e->conf + 1);
            if (bid_off < ssit_e->bid_off) {
                ssit_e->bid_off = bid_off;
            }
            inc(ld_pc, st_pc);
        }
    }
    if ((VERBOSE_ON && STAGE_ENABLED(StageID::LSU_ALL)) || verbose) {
        printf("MDB insert: ld_pc 0x%lx, conf %d, bid_off %u, st_pc 0x%lx\n", ld_pc, ssit_e->conf, ssit_e->bid_off,
               ssit_e->st_pc);
    }
    if (DEBUG_VERBOSE_ON || pConfigs->verbose) {
        LOG_DEBUG(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                  "MTCLSU: MDB insert: ld_pc 0x%lx, conf %d, bid_off %u, st_pc 0x%lx",
                  ld_pc, ssit_e->conf, ssit_e->bid_off, ssit_e->st_pc);
    }
}

void MtcMDB::release(uint64_t load_pc)
{
    if (ssit.count(load_pc) != 0) {
        if ((VERBOSE_ON && STAGE_ENABLED(StageID::LSU_ALL)) || verbose) {
            printf("MDB release: load_pc 0x%lx, store_pc 0x%lx, bid_off %u, conf %d\n", load_pc,
                ssit[load_pc].st_pc, ssit[load_pc].bid_off, ssit[load_pc].conf);
        }
        if (DEBUG_VERBOSE_ON || pConfigs->verbose) {
            LOG_DEBUG(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                      "MTCLSU: MDB release: load_pc 0x%lx, store_pc 0x%lx, bid_off %u, conf %d",
                      load_pc, ssit[load_pc].st_pc, ssit[load_pc].bid_off, ssit[load_pc].conf);
        }
        ssit.erase(load_pc);
    }
}

bool MtcMDB::dec(uint64_t &load_pc, uint64_t &store_pc)
{
    if (ssit.count(load_pc) != 0 && ssit[load_pc].st_pc == store_pc) {
        if (ssit[load_pc].weight == 0) {
            release(load_pc);
            return true;
        }
        ssit[load_pc].weight = (ssit[load_pc].weight==0) ? 0 : (ssit[load_pc].weight - 1);
        if (!isStall(ssit[load_pc].weight)) {
            if ((VERBOSE_ON && STAGE_ENABLED(StageID::LSU_ALL)) || verbose) {
                printf("MDB decrease to not stall: load_pc 0x%lx, store_pc 0x%lx, bid_off %u, conf %d\n", load_pc,
                       ssit[load_pc].st_pc, ssit[load_pc].bid_off, ssit[load_pc].conf);
            }
            if (DEBUG_VERBOSE_ON || pConfigs->verbose) {
                LOG_DEBUG(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                          "MTCLSU: MDB decrease to not stall: load_pc 0x%lx, store_pc 0x%lx, bid_off %u, conf %d",
                          load_pc, ssit[load_pc].st_pc, ssit[load_pc].bid_off, ssit[load_pc].conf);
            }
            return true;
        }
    }
    return false;
}

void MtcMDB::inc(uint64_t &load_pc, uint64_t &store_pc)
{
    if (ssit.count(load_pc) != 0 && ssit[load_pc].st_pc == store_pc) {
        ssit[load_pc].weight = ((ssit[load_pc].weight + pConfigs->mdb_inc_step) > pConfigs->mdb_max_weight) ?
                                pConfigs->mdb_max_weight : (ssit[load_pc].weight + pConfigs->mdb_inc_step);
    }
}

bool MtcMDB::isStall(uint64_t &w)
{
    uint64_t bond = (pConfigs->mdb_max_weight+1) * pConfigs->mdb_release_weight / 100 + 1;
    return (w >= bond);
}

void MtcMDB::initWeight(uint64_t &w)
{
    w = (pConfigs->mdb_max_weight+1) * pConfigs->mdb_release_weight / 100;
}

void MtcMDB::lookupMDB(void)
{
    for (uint32_t i = 0; i < pConfigs->lsu_width && !lookup_lu_mdb_q->empty(); ++i) {
        MDBBus bus = lookup_lu_mdb_q->front();
        lookup_lu_mdb_q->pop_front();
        handleMDBLookup(bus);
    }
}

void MtcMDB::deleteMDB(void)
{
    for (uint32_t i = 0; i < pConfigs->lsu_width && !delete_lu_mdb_q->empty(); ++i) {
        MDBBus bus = delete_lu_mdb_q->front();
        delete_lu_mdb_q->pop_front();
        handleMDBDelete(bus);
    }
}

void MtcMDB::recordMDB(void)
{
    for (uint32_t i = 0; i < pConfigs->lsu_width && !record_lu_mdb_q->empty(); ++i) {
        MDBBus bus = record_lu_mdb_q->front();
        record_lu_mdb_q->pop_front();
        handleMDBRecord(bus);
    }
}

void MtcMDB::handleMDBLookup(MDBBus &bus)
{
    bus.hit = ld_lookup(bus.ldInfo.tpc, bus.ldInfo.bid, bus.stInfo.tpc, bus.stInfo.bid);
    lookup_mdb_lu_q->push_back(bus);
    lookup_mdb_su_q->push_back(bus);
    if ((VERBOSE_ON && STAGE_ENABLED(StageID::LSU_ALL)) || verbose) {
        cout << "Lookup MDB " << bus << endl;
    }
    if (DEBUG_VERBOSE_ON || pConfigs->verbose) {
        LOG_DEBUG_STRUCT(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                         bus, "MTCLSU: Lookup MDB ");
    }
}

void MtcMDB::handleMDBRecord(MDBBus &bus)
{
    insert(bus.ldInfo.tpc, bus.ldInfo.bid, bus.ldInfo.lsID, bus.stInfo.tpc, bus.stInfo.bid, bus.stInfo.lsID, bus.conf);
    core->bctrl->bmdb.reportConfilict(bus.ldInfo.bid, bus.stInfo.bid, bus.stid);
    if ((VERBOSE_ON && STAGE_ENABLED(StageID::LSU_ALL)) || verbose) {
        cout << "Record MDB " << bus << endl;
    }
    if (DEBUG_VERBOSE_ON || pConfigs->verbose) {
        LOG_DEBUG_STRUCT(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                         bus, "MTCLSU: Record MDB ");
    }
}

void MtcMDB::handleMDBDelete(MDBBus &bus)
{
    bool removed = dec(bus.ldInfo.tpc, bus.ldInfo.wait_tpc);
    if (removed && ((VERBOSE_ON && STAGE_ENABLED(StageID::LSU_ALL)) || verbose)) {
        cout << "delete MDB " << bus << endl;
    }
    if (removed && (DEBUG_VERBOSE_ON || pConfigs->verbose)) {
        LOG_DEBUG_STRUCT(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                         bus, "MTCLSU: delete MDB ");
    }
}

} // namespace JCore