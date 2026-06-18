#include "iex/iex_mdb.h"

#include "core/Core.h"
#include "iex/iex.h"
#include "ModelSpec.h"
#include "SimSys.h"

namespace JCore {


void IEXMDB::Build(double conf, bool enable)
{
    confEntry = std::vector<SSITEntry>(IEXMDBSIZE);
    confTable = std::vector<MDBTable>(MDBTABLESIZE);
    size = 0;
    regSize = 0;
    mdb_confidence = conf;
    mdb_enable = enable;
}

void IEXMDB::Reset()
{
    confEntry.clear();
    confTable.clear();
}

void IEXMDB::Work() {
    for (uint32_t i = 0; i < IEXMDBSIZE; ++i) {
        if (!confEntry[i].vld) {
            continue;
        }
        ++confEntry[i].cycles;
    }
    for (uint32_t i = 0; i < MDBTABLESIZE; ++i) {
        if (!confTable[i].vld) {
            continue;
        }
        ++confTable[i].cycles;
    }
}

void IEXMDB::Xfer()
{
    for (uint32_t i = 0; i < MDBTABLESIZE; ++i) {
        if (!confTable[i].vld) {
            continue;
        }
        if (confTable[i].cycles > MDBTABLEMAXCYCLES) {
            confTable[i].Reset();
        }
    }

    for (uint32_t stid = 0; stid < top->GetSim()->core->configs.scalar_smt_thread; ++stid) {
        ROBID oldestBID = top->core->bctrl->blockROB.getOldestBlockID(stid);
        for (uint32_t i = 0; i < MDBTABLESIZE; ++i) {
            if (!confTable[i].vld) {
                continue;
            }
            if (!confTable[i].intraConflict && confTable[i].ldInfo.vld && confTable[i].ldInfo.bid == oldestBID) {
                confTable[i].Reset();
            }
        }
    }
}

SimSys* IEXMDB::GetSim()
{
    return top->GetSim();
}

int IEXMDB::ld_lookup(uint64_t tpc, bool isload)
{
    if (!mdb_enable) {
        return -1;
    }
    for (uint32_t i = 0; i < IEXMDBSIZE; ++i) {
        if (confEntry[i].vld) {
            if (isload && tpc == confEntry[i].ld_tpc) {
                return i;
            } else if (!isload) {
                if (tpc == confEntry[i].stInfo[0].tpc) {
                    return i;
                }
            }
        }
    }
    return -1;
}

IDBus IEXMDB::getOldestIDBus(uint32_t stid) {
    ROBID oldestBID = top->core->bctrl->blockROB.getOldestBlockID(stid);
    IDBus old_commit, commitBus;
    old_commit.vld = false;
    for (uint32_t pe = 0; pe < top->core->peArray.size(); pe++) {
        commitBus = top->core->peArray[pe]->GetRetireID();
        if (!commitBus.vld || (commitBus.bid != oldestBID)) {
            continue;
        }
        if (!old_commit.vld || LessEqual(commitBus.bid, commitBus.lsID, old_commit.bid, old_commit.lsID)) {
            old_commit = commitBus;
        }
    }
    commitBus   = old_commit;
    return commitBus;
}

uint32_t IEXMDB::lookupReg(int idx, uint64_t tpc, ROBID bid, ROBID lsid, uint32_t stid, bool isload)
{
    if (!mdb_enable || idx == -1) {
        return -1U;
    }
    SSITEntry &entry = confEntry[idx];
    if (entry.ld_conf < mdb_confidence) {
        return -1U;
    }
    auto copyInfo = [](Info &info, uint64_t tpc, ROBID bid, ROBID lsid, bool vld) {
        info.vld = vld;
        info.tpc = tpc;
        info.bid = bid;
        info.lsid = lsid;
    };

    for (uint32_t i = 0; i < MDBTABLESIZE; ++i) {
        if (confTable[i].vld && confTable[i].ssitIdx == idx) {
            Info &stinfo = confTable[i].stInfo;
            if (isload && stinfo.vld && LessEqual(stinfo.bid, stinfo.lsid, bid, lsid)) {
                if (!confTable[i].ldInfo.vld || (confTable[i].ldInfo.vld &&
                    LessEqual(bid, lsid, confTable[i].ldInfo.bid, confTable[i].ldInfo.lsid))) {
                    copyInfo(confTable[i].ldInfo, tpc, bid, lsid, true);
                }
                return i;
            } else if (!isload && !stinfo.vld && confTable[i].ldInfo.vld) {
                Info &ldinfo = confTable[i].ldInfo;
                if (LessEqual(bid, lsid, ldinfo.bid, ldinfo.lsid)) {
                    copyInfo(confTable[i].stInfo, tpc, bid, lsid, true);
                    return i;
                }
            }
        }
    }
    for (uint32_t i = 0; i < MDBTABLESIZE; ++i) {
        if (!confTable[i].vld) {
            if (isload) {
                IDBus bus = getOldestIDBus(stid);
                if (bid == bus.bid && lsid == bus.lsID) {
                    return -1U;
                }
                copyInfo(confTable[i].ldInfo, tpc, bid, lsid, true);
            } else {
                copyInfo(confTable[i].stInfo, tpc, bid, lsid, true);
            }
            confTable[i].ssitIdx = idx;
            confTable[i].vld = true;
            confTable[i].intraConflict = entry.intraConflict;
            regSize++;
            return i;
        }
    }
    return -1U;

}

void IEXMDB::insert(uint64_t ld_tpc, ROBID ld_bid, ROBID ld_lsid,uint64_t st_tpc,
    ROBID st_bid, ROBID st_lsid, bool intraConflict)
{
    if (!mdb_enable) {
        return;
    }
    ASSERT(LessEqual(st_bid, ld_bid));
    uint32_t depth = top->core->configs.block_rob_depth;
    uint32_t bid_off = DeltaBID(ld_bid, st_bid, depth);
    int lsID_off = DeltaBID(ld_lsid, st_lsid, 1024);
    SSITEntry *ssit_e = NULL;
    top->stats->memoryAaccelssConflict++;
    if (isFull()) {
        uint32_t idx = replay();
        ssit_e = &confEntry[idx];
        ssit_e->vld = true;
        ssit_e->insert(st_tpc, bid_off, lsID_off);
        ssit_e->ld_tpc = ld_tpc;
        ssit_e->intraConflict = intraConflict;
        return;
    }
    int idx = ld_lookup(ld_tpc, true);
    if (idx < 0) {
        // Miss
        for (uint32_t i = 0; i < IEXMDBSIZE; ++i) {
            if (!confEntry[i].vld) {
                ssit_e = &confEntry[i];
                ssit_e->vld = true;
                ssit_e->insert(st_tpc, bid_off, lsID_off);
                ssit_e->ld_tpc = ld_tpc;
                size++;
                break;
            }
        }
    } else {
        // hit
        ssit_e = &confEntry[idx];
        ssit_e->cycles = 0;
        ++ssit_e->ld_conf;
        ssit_e->vld = true;
        ssit_e->insert(st_tpc, bid_off, lsID_off);
        ssit_e->ld_tpc = ld_tpc;
        size++;
    }
    if (intraConflict) {
        ssit_e->intraConflict = intraConflict;
    }
    if (ssit_e) {
        LOG_DEBUG_M(top->machineType, Stage::NA) << "IEXMDB insert: ld_pc 0x" << std::hex << ld_tpc << ", st_pc 0x"
            << st_tpc << ", conf" << std::dec << ssit_e->ld_conf << ", bid_off" << bid_off << ", lsid_off 0x"
            << std::hex << lsID_off;
    }
}

bool IEXMDB::setStRdy(uint32_t regidx)
{
    if (!mdb_enable || regidx == -1U) {
        return true;
    }
    if (!confTable[regidx].vld) {
        return false;
    }
    Info &it = confTable[regidx].stInfo;
    it.rdy = true;
    it.isStWakeup = true;
    return true;
}

bool IEXMDB::isStRdy(uint32_t regidx, bool& bo)
{
    if (!mdb_enable || regidx == -1U ) {
        return true;
    }
    if (!confTable[regidx].vld) {
        return true;
    }
    if (confTable[regidx].cycles > MDBTABLEMAXCYCLES) {
        return true;
    }
    Info &it = confTable[regidx].stInfo;
    bo = it.isStWakeup;
    return it.rdy;
}

bool IEXMDB::release(uint32_t regidx)
{
    confTable[regidx].Reset();
    --regSize;
    return true;
}

void IEXMDB::releaseConf(int idx, bool wakeuped) {
    if (idx == -1) {
        return;
    }
    int val = 1;
    if (!wakeuped) {
        val = 2;
    }
    confEntry[idx].ld_conf -= val;
    if (wakeuped) {
        top->stats->efficient_intercept++;
    }
}

uint32_t IEXMDB::replay() {
    uint32_t idx = 0;
    for (uint32_t i = 1; i < IEXMDBSIZE; ++i) {
        if (confEntry[i].cycles > confEntry[idx].cycles) {
            idx = i;
        }
    }
    confEntry[idx].Reset();
    return idx;
}

} // namespace JCore
