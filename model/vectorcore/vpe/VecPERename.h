#pragma once

#ifndef VEC_PE_RENAME_H
#define VEC_PE_RENAME_H

#include "core/Bus.h"
#include "ModelSpec.h"
#include "ModelCommon/SimInstInfo.h"
#include "ModelCommon/ModelCommon.h"
#include "bctrl/LocalRegMgr.h"
#include "vectorcore/vpe/LGPR/LGPRRF.h"
#include "vectorcore/vpe/vrf/VRFRename.h"

namespace JCore {
class VecPE;
class VecPERename : SimObj {
public:
    VecPERename() = default;
    ~VecPERename() = default;
    VecPE *vpeTop;
    SimSys *sim;

    INPUT SimQueue<SimInst>* dec_ren_q;
    INNER std::deque<SimInst> d2_s1_q;

    std::shared_ptr<LGPRRF> lgprRF;
    std::shared_ptr<VRFRename> vrfRename;
    // 寻址方式 tid->handtype
    std::vector<std::vector<LocalRegMgr>> sgprRenameUnit;
    // 寻址方式 tid
    std::vector<LocalRegMgr> predMaskRenameUnit;

    void Work() override;
    void Xfer() override;
    void Reset() override;
    void ReportStat() override;
    SimSys *GetSim() override;

    void Build();
    void Rename();
    bool CheckStall(SimInst const& inst);
    void SetLOOPRegPOperandVld(SimInst &inst, POperandPtr &operand);
    bool RenameSrcPOperand(SimInst &inst, POperandPtr &operand);
    bool RenameDstPOperand(SimInst &inst, POperandPtr &operand);
    void ToDispatch();
    bool DispatchToVIexQ(SimInst &inst);
    bool InsertToVecScalarPipe(const SimInst &inst);
    bool InsertToVecAluPipe(const SimInst &inst);
    bool InsertToVecAguPipe(const SimInst &inst);
    bool InsertToVecAguStdPipes(const SimInst &inst);
    void Flush(FlushBus flushReq);
    void RepLocalRetired(OperandType type, ROBID &seq, bool isDealloc, uint32_t tid);
    void ReportLocalKill(OperandType type, uint32_t seq, uint32_t tid);
};
}

#endif
