#pragma once

#ifndef PE_ROB_COMMON_H
#define PE_ROB_COMMON_H

#include "PROBStatus.h"

namespace JCore {
/* PE Reorder Buffer State */
struct ROBState {
    std::vector<PROBEntry>          entry;
    /* \brief Dealloc pointer */
    ROBID                           deallocPtr;
    /* \brief Alloc pointer */
    ROBID                           allocPtr;
    /* \brief Commit pointer */
    ROBID                           commitPtr;
    ROBID                           commitBid;
    /* \brief Last branch minst pointer */
    ROBID                           branchPtr;
    bool                            branchVld = false;
    bool                            stackVld = false;
    uint32_t                        size = 0;
    uint32_t                        osdSize = 0;
    bool                            stall = false;

    void                            Reset();
    void                            flush(FlushBus flushReq);
    uint32_t                        resolveData(MemReqBus const &mem, uint32_t lane, uint64_t mask);

    inline PROBEntry& operator[](size_t index) { return entry[index]; }
    inline PROBEntry const& operator[](size_t index) const { return entry[index]; }
};

struct RelateInfo {
    bool                            vld = false;
    bool                            logic_long = false;
    bool                            kill = false;
    bool                            recycled = false;
    uint32_t                        peid;
    ROBID                           bid;
    ROBID                           gid;
    ROBID                           rid;
    uint32_t                        tag;
    ROBID                           seq;
    uint32_t                        tid = 0;
};

struct RelateCmap {
    std::deque<RelateInfo>          entry;
    OperandType                     type;
    RelateInfo                      front();
    RelateInfo                      back();
    RelateInfo                      read();
    RelateInfo                      pop_back();
    void                            write(RelateInfo &info);
    uint32_t                        size();
    bool                            empty();
    void                            Reset(OperandType rtype);
};
}
#endif