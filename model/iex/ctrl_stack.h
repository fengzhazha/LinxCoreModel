#pragma once

#include "../configs/iex_config.h"
#include "iex/iex.h"
#include "ModelCommon/SimInstInfo.h"

namespace JCore {
const uint32_t CTRL_STACK_SIZE = 100;
struct BrInfo {
    uint64_t pc = 0;
    uint32_t mask = 0;
    uint32_t tSeq = 0;
    uint32_t uSeq = 0;
    bool seqVld = false;
    BrInfo() {}
    BrInfo& operator=(const BrInfo &cp)
    {
        pc = cp.pc;
        mask = cp.mask;
        tSeq = cp.tSeq;
        uSeq = cp.uSeq;
        seqVld = cp.seqVld;
        return *this;
    }
    BrInfo(const BrInfo &cp)
    {
        pc = cp.pc;
        mask = cp.mask;
        tSeq = cp.tSeq;
        uSeq = cp.uSeq;
        seqVld = cp.seqVld;
    }
};

class CtrlStack {
#define RPC_PTR (ptr - 2)
public:
    IEX                                             *iex;
    SimSys                                          *GetSim();

public:
    CtrlStack() : ptr(0) {}
    virtual ~CtrlStack() {}
    const                                           BrInfo& Top();

    void                                            Push(BrInfo &brinfo);
    void                                            Pop();
    bool                                            CheckRpc(uint64_t tpc);

public:
    void                                            Work();
    void                                            Build();
    void                                            Reset();
    void                                            flush(FlushBus& bus);
private:
    BrInfo                                          ctrlStack[CTRL_STACK_SIZE];
    uint32_t                                        ptr;
};

} // namespace JCore
