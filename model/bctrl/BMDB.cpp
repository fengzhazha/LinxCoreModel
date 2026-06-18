#include "bctrl/BMDB.h"

#include "bctrl/BCtrl.h"
#include "core/Bus.h"
#include "core/Core.h"

namespace JCore {

void BMDB::Work()
{
    ls_conf_q.Work();
    while (!ls_conf_q.Empty()) {
        auto pkt = ls_conf_q.Read();
        insert(pkt);
    }
}

void BMDB::insert(Confpkt pkt)
{
    if (confTable.count(pkt.ld_pc) == 0 || confTable[pkt.ld_pc].conf < 1) {
        confTable[pkt.ld_pc] = pkt.conf_st;
    } else {
        if (pkt.conf_st.st_pc != confTable[pkt.ld_pc].st_pc) {
            if (pkt.conf_st.bid_off < confTable[pkt.ld_pc].bid_off) {
                confTable[pkt.ld_pc] = pkt.conf_st;
            } else {
                confTable[pkt.ld_pc].conf--;
            }
        } else {
            confTable[pkt.ld_pc].conf = ((confTable[pkt.ld_pc].conf + 1) > 3) ? 3 : (confTable[pkt.ld_pc].conf + 1);
            if (pkt.conf_st.bid_off < confTable[pkt.ld_pc].bid_off) {
                confTable[pkt.ld_pc].bid_off = pkt.conf_st.bid_off;
                confTable[pkt.ld_pc].pe_id = uint32_t(-1);
            }
        }
    }
}

bool BMDB::lookup(uint64_t ld_pc, ROBID ld_bid)
{
    if (confTable.count(ld_pc) == 0) {
        return false;
    }

    uint32_t bid_off = DeltaBID(ld_bid, confTable[ld_pc].st_bid, top->core->configs.block_rob_depth);
    if (confTable[ld_pc].conf < 1 || bid_off != confTable[ld_pc].bid_off || confTable[ld_pc].pe_id == uint32_t(-1)) {
        return false;
    }

    if (ld_pc == confTable[ld_pc].st_pc) {
        return false;
    }

    return true;
}

uint32_t BMDB::getPEID(uint64_t ld_pc)
{
    return confTable[ld_pc].pe_id;
}

void BMDB::updatePEID(uint64_t st_pc, uint32_t pe, ROBID st_bid)
{
    for (auto &it : confTable) {
        if (it.second.st_pc == st_pc) {
            it.second.pe_id = pe;
            it.second.st_bid = st_bid;
        }
    }
}

void BMDB::Xfer()
{

}

void BMDB::Build()
{

}

void BMDB::Reset()
{
    ls_conf_q.Reset();
    confTable.clear(); 
}

SimSys* BMDB::GetSim()
{
    return nullptr;
}

void BMDB::reportConfilict(ROBID ld_bid, ROBID st_bid, uint32_t stid)
{
    BlockCommandPtr ldCmd = top->blockROB.GetBlockCMDPtr(ld_bid, stid);
    BlockCommandPtr stCmd = top->blockROB.GetBlockCMDPtr(st_bid, stid);
    uint64_t ld_pc = ldCmd->bpc;
    uint64_t st_pc = stCmd->bpc;
    uint32_t bid_off = DeltaBID(ld_bid, st_bid, top->core->configs.block_rob_depth);
    StoreEntry e;
    e.conf = 1;
    e.st_pc = st_pc;
    e.bid_off = bid_off;
    e.st_bid = st_bid;
    e.pe_id = uint32_t(-1);
    Confpkt pkt;
    pkt.ld_pc = ld_pc;
    pkt.conf_st = e;
    // Record in queue, handle in Work()
    ls_conf_q.Write(pkt);
}

} // namespace JCore