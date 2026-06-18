#pragma once

#include <cstdint>
#include <iostream>
#include <vector>

#include "bctrl/bctrl_stats.h"
#include "bctrl/LocalRegMgr.h"
#include "core/Bus.h"
#include "ModelSpec.h"
#include "ModelCommon/SimInstInfo.h"
#include "pe/PECommon/DecodeState.h"

namespace JCore {


class DCTop;
class SimSys;
class Decoder : public SimObj {
private:
    std::vector<DecodeState>        decState;
    uint32_t                        peid = 0;

public:
    SimSys                          *sim;
    DCTop                           *top;

    /* INPUT */
    std::deque<SimInst>             *pinst_top_dec_q;
    /* OUTPUT */
    std::vector<std::deque<SimInst>*>             pblock_split_dc_top_q;
    std::deque<SimInst>             *pinst_dc_top_q;
    bool                            PushBlockSplitInst(SimInst &inst, std::stringstream &oss);
    bool                            DecodeInst(SimInst &simInst, std::stringstream &oss);

    void                            Reset();
    void                            Work();
    void                            Xfer();
    void                            Build(uint32_t pe);
    SimSys                          *GetSim();
    void ReportStat() override {}
    void                            flush(FlushBus &flushReq);
    bool                            checkStall(uint32_t inputCnt=1);
    bool                            FitMovrOpt(SimInst &inst);
};


} // namespace JCore
