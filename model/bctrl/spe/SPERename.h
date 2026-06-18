#pragma once

#ifndef SPE_RENAME_H
#define SPE_RENAME_H

#include "ModelSpec.h"
#include "ModelCommon/SimInstInfo.h"
#include "ModelCommon/ModelCommon.h"
#include "bctrl/spe/GPRRename.h"
#include "bctrl/LocalRegMgr.h"

namespace JCore {
class SPE;
class SPERename : SimObj {
public:
    SPERename() = default;
    ~SPERename() = default;
    SPE *top;
    SimSys *sim;

    INPUT SimQueue<SimInst>* dec_ren_q;
    INNER std::deque<SimInst> d2_s1_q;

    GPRRename ggprRenameUnit;
    // 寻址方式：pe->tid->hand
    std::vector<std::vector<std::vector<LocalRegMgr>>> sgprRenameUnit;

    void Work() override;
    void Xfer() override;
    void Reset() override;
    void ReportStat() override;
    SimSys *GetSim() override;

    void Build();
    void Rename();
    bool RenameSrcPOperand(SimInst &inst, POperandPtr &operand);
    bool CheckRenameStall(SimInst inst);
    bool RenameDstPOperand(SimInst &inst, POperandPtr &operand);
    void ToDispatch();
    bool InsertToSIEXQ(SimInst &inst);
    bool InsertToCmdIEX(SimInst inst);
    bool InsertToAluIEX(SimInst inst);
    bool InsertToStoreIEX(SimInst inst);
    bool InsertToLoadIEX(SimInst inst);
    bool InsertToBruIEX(SimInst inst);
    bool StackRename(SimInst inst);
    void Flush(FlushBus flushReq);
    void ReportSGPRBlockCommit(ROBID bid, uint32_t stid);
    void RepLocalRetired(OperandType type, uint32_t peid, ROBID &seq, bool isDealloc, uint32_t tid = 0);
};
}
#endif