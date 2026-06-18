#include "PROBCommon.h"

namespace JCore {

void ROBState::Reset()
{
    for (uint32_t i = 0; i < entry.size(); i++) {
        entry[i].vld = false;
        entry[i].status = INST_FREE;
        entry[i].stack_complete = false;
        entry[i].check = false;
    }
    ROBID clr;
    clr.val = 0;
    clr.wrap = false;
    deallocPtr = clr;
    allocPtr = clr;
    commitPtr = clr;
    branchPtr = clr;
    branchVld = false;
    stackVld = true;
    size = 0;
    osdSize = 0;
    stall = false;
}

void ROBState::flush(FlushBus flushReq) {}

uint32_t ROBState::resolveData(MemReqBus const &mem, uint32_t lane, uint64_t mask)
{
    SimInst &dstInst = entry[mem.rid.val].inst;
    for (auto &pdst : dstInst->pdsts_) {
        pdst->dataVld = true;
        if ((mask & (1ULL << lane)) && pdst->vecDataVld) {
            pdst->vecData.Set(mem.data, lane);
        }
    }
    ++dstInst->retLane;
    return dstInst->retLane;
}

void RelateCmap::write(RelateInfo &info)
{
    if (info.vld) {
        entry.push_back(info);
    }
}

RelateInfo RelateCmap::front()
{
    if (entry.empty()) {
        RelateInfo info = RelateInfo();
        return info;
    }
    return entry.front();
}

RelateInfo RelateCmap::back()
{
    if (entry.empty()) {
        RelateInfo info = RelateInfo();
        return info;
    }
    return entry.back();
}

RelateInfo RelateCmap::read()
{
    RelateInfo info = RelateInfo();
    if (!entry.empty()) {
        info = entry.front();
        entry.pop_front();
    }
    return info;
}

RelateInfo RelateCmap::pop_back()
{
    RelateInfo info = RelateInfo();
    if (!entry.empty()) {
        info = entry.back();
        entry.pop_back();
    }
    return info;
}

uint32_t RelateCmap::size()
{
    return entry.size();
}

bool RelateCmap::empty()
{
    return (size() == 0);
}

void RelateCmap::Reset(OperandType rtype)
{
    entry.clear();
    type = rtype;
}
}