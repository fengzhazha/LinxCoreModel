#include "lsu/FakeLSU.h"

#include "core/Core.h"

namespace JCore {

void FakeLSU::Work()
{
    // From the oldest block, perform memory aaccelss
    ROBID oldestID = top->brob->getOldestBlockID(0);
    // Check whether block has been completed
    while (true) {
        BlockStatus status = top->brob->getBlockStatus(oldestID, 0);
        if (status == BlockStatus::COMPLETED) {
            if (fakeLSU[oldestID.val].memSet.size()) break;
            IncROBID(oldestID, core->configs.block_rob_depth);
        } else break;
    }

    if (top->brob->getBlockStatus(oldestID, 0) == BlockStatus::MISPRED) return;

    MemSet &memReqSet = fakeLSU[oldestID.val];

    if (!memReqSet.memSet.size()) return;

    // pick the youngest mem request
    if (memReqSet.memSet.find(memReqSet.oldest) == memReqSet.memSet.end()) return;
    MemReqBus memReq = memReqSet.memSet[memReqSet.oldest];
    IncROBID(memReqSet.oldest, LSU_ID_RANGE);
    memReqSet.memSet.erase(memReq.lsID);

    top->aaccelssSoftMemory(memReq);

    LOG_INFO_M(Unit::LSU, Stage::NA) << "[LSU Stage]: service request for Block " << oldestID.val
                                      << " LSID " << memReq.lsID << " " << memReq;

    // register cycles
    memReq.lsu_ret_cycle = top->GetSim()->getCycles();
    uint64_t latency = memReq.lsu_ret_cycle - memReq.lsuRecvCycle;
    stats->total_load_request ++;
    stats->total_load_latency += latency;

    // For load, simply load and return
    if (memReq.is_load) {
        // wakeup subsequent instructions
        pe_wakeup_q[memReq.peID].Write(memReq);
        // send data
        pe_ret_data_q[memReq.peID].Write(memReq);
    } else {
        // wakeup subsequent instructions
        pe_wakeup_q[memReq.peID].Write(memReq);
        // For store, need to resolve
        pe_resolve_q[memReq.peID].Write(memReq);
        // For store reserve,need to send data
        if (memReq.is_llsc)
            pe_ret_data_q[memReq.peID].Write(memReq);
    }


}

void FakeLSU::returnPE()
{
    for (uint32_t pe=0; pe< core->GetPECount(); pe++) {
        pe_wakeup_q[pe].Work();
        pe_resolve_q[pe].Work();
        pe_ret_data_q[pe].Work();
        if (!pe_wakeup_q[pe].Empty()) {
            MemReqBus memReq = pe_wakeup_q[pe].Read();
            core->iex[SCALAR_IEX]->setMemWakeup(memReq);
        }
        if (!pe_ret_data_q[pe].Empty()) {
            MemReqBus memReq = pe_ret_data_q[pe].Read();
            core->iex[SCALAR_IEX]->setMemData(memReq);
        }
        if (!pe_resolve_q[pe].Empty()) {
            MemReqBus memReq = pe_resolve_q[pe].Read();
            core->iex[SCALAR_IEX]->setMemResolve(memReq);
        }
    }
}

bool FakeLSU::mergeStore(MemReqBus &bus)
{
    if (bus.is_load || bus.type == ST_ALL) return true;
    bool r = false;
    MemSet &storeSet = fakeST[bus.bid.val];
    if (storeSet.memSet.count(bus.lsID) == 0) {
        storeSet.memSet.emplace(bus.lsID, bus);
    } else {
        MemReqBus &last = storeSet.memSet[bus.lsID];
        if (last.type == ST_ADDR) {
            // STA
            bus.addr = last.addr;
        } else {
            // STD
            bus.data = last.data;
        }
        r = true;
        LOG_INFO_M(Unit::LSU, Stage::NA) << "[LSU Stage]: merge store " << bus;
        ASSERT((last.type == ST_ADDR && bus.type == ST_DATA) || (last.type == ST_DATA && bus.type == ST_ADDR));
        storeSet.memSet.erase(bus.lsID);
    }
    return r;
}

} // namespace JCore