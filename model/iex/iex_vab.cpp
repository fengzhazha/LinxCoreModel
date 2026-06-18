#include "iex/iex_vab.h"

#include "core/Core.h"
#include "iex/iex.h"

namespace JCore {
void VAB::Build(uint32_t size)
{
    originInst = std::make_shared<SimInstInfo>();
    entry.resize(size);
    entry_valid.resize(size);
    for (uint32_t i = 0; i < entry.size(); i++) {
        entry[i] = new MemRequest();
        entry_valid[i] = false;
    }
    reqSize = size;
    Reset();
}

void VAB::Reset()
{
    originInst.reset();
    for (uint32_t index = 0; index < reqSize; ++index) {
        entry_valid[index] = false;
        if (entry[index] != nullptr) {
            delete entry[index];
        }
    }
    reqSize = 0;
    curSize = 0;
    Stall = false;
    Pending = false;
}

void VAB::split(uint64_t addr)
{
    uint64_t align_addr = addr & static_cast<uint64_t>(~(255ULL));

    bool miss = true;
    uint32_t index = 0;
    for (index = 0; index < reqSize; ++index) {
        if (entry[index]->addrs.Get(0, static_cast<uint32_t>(OperandWidth::OPDW_D)) == align_addr) {
            miss = false;
            break;
        }
    }

    if (miss) {
        VecData addrs;
        addrs.Init(64, 64);
        for (int lane = 0; lane < 64; ++lane) {
            addrs.Set(align_addr + originInst->pdsts_[DST0_IDX]->vecData.width/8*lane, lane, static_cast<uint32_t>(OperandWidth::OPDW_D));
        }
        entry[reqSize] = new MemRequest();
        entry[reqSize]->tpc = originInst->pc;
        entry[reqSize]->peID = originInst->peID;
        entry[reqSize]->stid = originInst->stid;
        entry[reqSize]->opcode = originInst->opcode;
        entry[reqSize]->laneSize = OpcodeManager::Inst().GetOpcodeAccMemBaseInfo(originInst->opcode)->bytes;
        entry[reqSize]->realReqCnt = originInst->realReqCnt;
        entry[reqSize]->addrs = addrs;
        entry[reqSize]->data.width = originInst->pdsts_[DST0_IDX]->vecData.width;
        entry[reqSize]->data.size = 64;
        entry[reqSize]->data.data.resize(64);
        entry[reqSize]->mask = originInst->GetSrcMSIMTMask();
        entry[reqSize]->lanes = entry.size();
        entry[reqSize]->thread = originInst->peID;
        ASSERT(GetSim()->core->IsVecPe(originInst->peID));
        entry[reqSize]->bid = originInst->bid;
        entry[reqSize]->rid = originInst->rid;
        entry[reqSize]->gid = originInst->gid;
        entry[reqSize]->tid = originInst->tid;
        entry[reqSize]->lsID = originInst->lsID;
        entry[reqSize]->width = OpcodeManager::Inst().GetOpcodeAccMemBaseInfo(originInst->opcode)->bytes;
        entry[reqSize]->isLoad = true;
        entry[reqSize]->start = 0;
        entry[reqSize]->toMemory = false;
        entry[reqSize]->uinst = originInst;
        entry[reqSize]->gatherLd = originInst->gatherLd;
        reqSize++;
    }
}

MemRequest* VAB::selectOne()
{
    MemRequest* memReq = nullptr;
    // VAB空或VAB请求已经发完
    if (reqSize == 0 || curSize >= reqSize|| curSize >= entry.size()) {
        return memReq;
    }

    memReq = entry[curSize];
    curSize++;
    if (curSize == reqSize || curSize == entry.size()) {
        originInst->gatherInfo.gather_issue_cycle_end = GetSim()->getCycles();
        iex_iq_top->top->stats->tile_gather_issue_cycle += \
            originInst->gatherInfo.gather_issue_cycle_end - originInst->gatherInfo.gather_issue_cycle_begin;
        Pending = true;
    }
    return memReq;
}

void VAB::dumpVab()
{
    std::cout << "VAB::dumpVab" << std::endl;
    for (uint32_t index = 0; index < reqSize; ++index) {
        if (entry[index]) {
            std::cout << "index" << index << " addrs" << entry[index]->addrs.Get(0, static_cast<uint32_t>(OperandWidth::OPDW_D)) << std::endl;
        } else {
            std::cout << "index" << index << " 空" << std::endl;
        }
    }
}

void VAB::pushVab(SimInst inst)
{
    inst->gatherLd = true;
    originInst = inst;
    originInst->gatherInfo.gather_issue_cycle_begin = GetSim()->getCycles();
    Stall = true;
}

SimInst VAB::popVab()
{
    return originInst;
}

bool VAB::isGather(MemRequest bus)
{
    uint32_t peID = bus.thread;
    auto &rob_next = *iex_iq_top->top->nextROBs[peID][bus.tid];
    SimInst &inst = rob_next[bus.rid.val].inst;
    if (inst->gatherLd) {
        return false;
    } else {
        return true;
    }
}

bool VAB::resp2vab(MemRequest req)
{
    for (uint32_t tmp = 0; tmp < reqSize; ++tmp) {
        if (entry[tmp]->addrs.Get(0, static_cast<uint32_t>(OperandWidth::OPDW_D)) == req.addrs.Get(0, static_cast<uint32_t>(OperandWidth::OPDW_D))) {
            entry[tmp]->data = req.data;
            entry_valid[tmp] = true;
        }
    }
    bool full = true;
    for (uint32_t tmp = 0; tmp < curSize; ++tmp) {
        if (!entry_valid[tmp]) {
            full = false;
            break;
        }
    }
    if (full) {
        Resolve(req);
        Reset();
    }
    return full;
}

void VAB::Resolve(MemRequest req)
{
    uint32_t peID = req.thread;
    auto &rob_next = *iex_iq_top->top->nextROBs[peID][req.tid];
    SimInst &inst = rob_next[req.rid.val].inst;
    inst->gatherInfo.gather_issue_cycle_Retire = GetSim()->getCycles();
    iex_iq_top->top->stats->tile_gather_complete_cycle += inst->gatherInfo.gather_issue_cycle_Retire -
                                                          inst->gatherInfo.gather_stall_cycle_begin;
    // 不清楚功能，暂时注释
    // for (uint32_t lane = 0; lane < inst->lanes; ++lane) {
    //     for (uint32_t tmp = 0; tmp < reqSize; ++tmp) {
    //         uint64_t addr = inst->dst0.vec_data.Get(lane, static_cast<uint32_t>(OperandWidth::OPDW_D));
    //         uint64_t align_addr = addr & static_cast<uint64_t>(~(255ULL));
    //         if (align_addr == entry[tmp]->addrs.Get(0, static_cast<uint32_t>(OperandWidth::OPDW_D))) {
    //             inst->dst0.vec_data.Set(entry[tmp]->data.Get((addr-align_addr)/entry[tmp]->data.width),
    //                 lane, static_cast<uint32_t>(OperandWidth::OPDW_D));
    //         }
    //     }
    // }

    // inst->dst0.dataOut_vld = true;
    // inst->loadWakeuped = true;
    // PLpvInfo lpvInfo = std::make_shared<LpvInfo>();
    // iex_iq_top->wakeupReg(peID, inst->dst0, lpvInfo, inst->tid);
    // iex_iq_top->wakeupReg(peID, inst->dst1, lpvInfo, inst->tid);
    // iex_iq_top->top->agupip_resolve(inst);
}

using namespace std;

} // namespace JCore
