#include "lsu/mdb/MDB.h"

#include "core/Core.h"

namespace JCore {

void MDB::Reset() {
    ssit.clear();
    lookup_lu_mdb_q->clear();
    record_lu_mdb_q->clear();
    lookup_mdb_lu_q->clear();
}

void MDB::Work() {
    if (LoggerManager::GetManager().level <= LoggerLevel::DETAIL) {
        for (auto it : ssit) {
            LOG_DETAIL_M(Unit::LSU, Stage::NA) << "mdb ssit " << it.first << " conf" << it.second.conf << " weight "
                << it.second.weight << " sttpc 0x" << std::hex << it.second.st_pc;
        }
    }
    lookupMDB();
    deleteMDB();
    recordMDB();
}

void MDB::Xfer() {

}

SimSys* MDB::GetSim() {
    return sim;
}

void MDB::Build() {
    verbose = pConfigs->verbose;
}

bool MDB::ld_lookup(uint64_t tpc, ROBID bid, uint64_t &st_pc, ROBID &st_bid) {
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

void MDB::insert(uint64_t ld_pc, ROBID ld_bid, ROBID ld_lsid, uint64_t st_pc, ROBID st_bid, ROBID st_lsid, int conf) {
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
    LOG_INFO_M(Unit::LSU, Stage::NA) << "MDB insert: ld_pc 0x" << std::hex << ld_pc << ", conf " << std::dec
        << ssit_e->conf << ", bid_off " << ssit_e->bid_off << ", st_pc 0x" << std::hex << ssit_e->st_pc;
}

void MDB::release(uint64_t load_pc) {
    if (ssit.count(load_pc) != 0) {
        LOG_INFO_M(Unit::LSU, Stage::NA) << "MDB release: load_pc 0x" << std::hex << load_pc << ", store_pc 0x"
            << std::hex << ssit[load_pc].st_pc << std::dec << ", bid_off " << ssit[load_pc].bid_off << ", conf "
            << ssit[load_pc].conf;
        ssit.erase(load_pc);
    }
}

bool MDB::dec(uint64_t &load_pc, uint64_t &store_pc) {
    if (ssit.count(load_pc) != 0 && ssit[load_pc].st_pc == store_pc) {
        if (ssit[load_pc].weight == 0) {
            release(load_pc);
            return true;
        }
        ssit[load_pc].weight = (ssit[load_pc].weight==0) ? 0 : (ssit[load_pc].weight - 1);
        if (!isStall(ssit[load_pc].weight)) {
            LOG_INFO_M(Unit::LSU, Stage::NA) << "MDB decrease to not stall: load_pc 0x" << std::hex << load_pc
                << ", store_pc 0x" << std::hex << ssit[load_pc].st_pc << std::dec << ", bid_off "
                << ssit[load_pc].bid_off << ", conf " << ssit[load_pc].conf;
            return true;
        }
    }
    return false;
}

void MDB::inc(uint64_t &load_pc, uint64_t &store_pc) {
    if (ssit.count(load_pc) != 0 && ssit[load_pc].st_pc == store_pc) {
        ssit[load_pc].weight = ((ssit[load_pc].weight + pConfigs->mdb_inc_step) > pConfigs->mdb_max_weight) ?
            pConfigs->mdb_max_weight : (ssit[load_pc].weight + pConfigs->mdb_inc_step);
    }
}

bool MDB::isStall(uint64_t &w) {
    uint64_t bond = (pConfigs->mdb_max_weight+1) * pConfigs->mdb_release_weight / 100 + 1;
    return (w >= bond);
}

void MDB::initWeight(uint64_t &w) {
    w = (pConfigs->mdb_max_weight+1) * pConfigs->mdb_release_weight / 100;
}

void MDB::lookupMDB(void) {
    for (uint32_t i = 0; i < pConfigs->lsu_width && !lookup_lu_mdb_q->empty(); ++i) {
        MDBBus bus = lookup_lu_mdb_q->front();
        lookup_lu_mdb_q->pop_front();
        handleMDBLookup(bus);
    }
}

void MDB::deleteMDB(void) {
    for (uint32_t i = 0; i < pConfigs->lsu_width && !delete_lu_mdb_q->empty(); ++i) {
        MDBBus bus = delete_lu_mdb_q->front();
        delete_lu_mdb_q->pop_front();
        handleMDBDelete(bus);
    }
}

void MDB::recordMDB(void) {
    for (uint32_t i = 0; i < pConfigs->lsu_width && !record_lu_mdb_q->empty(); ++i) {
        MDBBus bus = record_lu_mdb_q->front();
        record_lu_mdb_q->pop_front();
        handleMDBRecord(bus);
    }
}

void MDB::handleMDBLookup(MDBBus &bus) {
    bus.hit = ld_lookup(bus.ldInfo.tpc, bus.ldInfo.bid, bus.stInfo.tpc, bus.stInfo.bid);
    lookup_mdb_lu_q->push_back(bus);
    lookup_mdb_su_q->push_back(bus);
    LOG_INFO_M(Unit::LSU, Stage::NA) << "Lookup MDB " << bus;
}

void MDB::handleMDBRecord(MDBBus &bus) {
    insert(bus.ldInfo.tpc, bus.ldInfo.bid, bus.ldInfo.lsID, bus.stInfo.tpc, bus.stInfo.bid, bus.stInfo.lsID, bus.conf);
    core->bctrl->bmdb.reportConfilict(bus.ldInfo.bid, bus.stInfo.bid, bus.stInfo.stid);
    LOG_INFO_M(Unit::LSU, Stage::NA) << "Record MDB " << bus;
}

void MDB::handleMDBDelete(MDBBus &bus) {
    bool removed = dec(bus.ldInfo.tpc, bus.ldInfo.wait_tpc);
    if (removed) {
        LOG_INFO_M(Unit::LSU, Stage::NA) << "delete MDB " << bus;
    }
}

} // namespace JCore