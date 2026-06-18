#pragma once

#include "../configs/iex_config.h"
#include "ModelCommon/SimInstInfo.h"
#include "core/Bus.h"

namespace JCore {


class IEX;

struct ReduceRegInfo {
    bool vld = false;
    uint64_t data = 0;

    void Reset()
    {
        vld = false;
        data = 0;
    }

    void Set(uint64_t d)
    {
        vld = true;
        data = d;
    }

    uint64_t Get() { return data; }

    bool IsValid() { return vld; }
};

class RdUnit {
public:
    IEX                                             *top;
    SimSys                                          *GetSim();

    INPUT std::vector<SimQueue<SimInst>*>           pe_iex_rd_array;
    OUTPUT RFReqBus                                 rf_wr_req;

    void                                            setReg(Opcode opcode, uint32_t ptag, uint64_t data,
                                                           OperandWidth width, OPConvertType cvt);
    void                                            setReg(Opcode opcode, ReduceRegInfo& reg, uint64_t data,
                                                           OperandWidth width, OPConvertType cvt);
    void                                            Reduce(SimInst &inst);
    void                                            Work();
    void                                            Build();
    void                                            Reset();
    void                                            flush(FlushBus &bus);
    void                                            setRegFileWrite(POperandPtr& dst, const POperandPtr &instDst);
    RdUnit() {}
    virtual ~RdUnit() {}
private:
    // 0-23 GPR
    std::vector<ReduceRegInfo>                      rdReg;
    // SIMT Local Scalar Register
    ReduceRegInfo                                   tReg;
    ReduceRegInfo                                   uReg;
};


} // namespace JCore
