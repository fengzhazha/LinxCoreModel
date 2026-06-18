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
class CodeTemplate : public SimObj {
private:
    BlockCommandPtr             ctBlock = nullptr;
    DecodeState                 decState;
    uint32_t                    peid = 0;

    uint32_t                    localGetMask = 0;
    uint32_t                    localSetMask = 0;
    uint32_t                    localRegCnt = 0;

public:
    CodeTemplate() {};
    virtual ~CodeTemplate() {};
    DCTop                       *top;
    SimSys                      *sim;

    /* INPUT */
    std::deque<SimInst>         *pinst_top_ct_q;
    SimQueue<SimInst>             *rf_ct_q;
    /* OUTPUT */
    std::deque<SimInst>         *pinst_ct_top_q;
    SimQueue<SimInst>           extendInstQ;
    SimQueue<SimInst>           alignInstQ;
    SimQueue<SimInst>           endingInstQ;
    uint32_t                    genCntAtCyc = 0;
    uint32_t                    totalInstCnt = 0;
    uint32_t                    genInstCnt = 0;
    uint32_t                    genNoFixInstCnt = 0;
    uint32_t                    instWdith = 0;
    void                        genTemplate();

    GenMemStage                 curStage = GenMemStage::START;
    GenMemStage                 nextStage = GenMemStage::START;
    GenMemCopyStage             curMemcopyStage = GenMemCopyStage::GEN_ALIGN;
    GenMemCopyStage             nextMemcopyStage = GenMemCopyStage::GEN_ALIGN;
    std::vector<uint64_t>            curRegData;
    std::vector<uint64_t>            nextRegData;
    std::vector<uint64_t>            reg;
    uint32_t                    addrOffset = 0;
    uint32_t                    srcOffset = 0;
    uint32_t                    dstOffset = 0;
    uint32_t                    srcTotalOffset = 0;
    uint32_t                    ct_stCnt = 0;
    uint32_t                    ct_ldCnt = 0;
    bool                        ct_vld = false;
    MemCopyStatus               followStatus = MemCopyStatus::FOLLOW_LD;
    bool                        isGenDczvaInst = false;
    // SimQueue<SimInst>*            dec_ren_q;
    bool                        GenSameTemplate(SimInst &inst, const char* funcName);
    void                        RecordGenCycle(SimInst &inst);
    SimInst                       FindInstWithRegId(std::vector<SimInst> &insts, uint32_t regId);
    void                        ReadRf(BlockCommandPtr &cmd);
    bool                        genMcopyRequstToRegData(BlockCommandPtr cmd);
    bool                        genMsetRequstToRegData(BlockCommandPtr cmd);
    bool                        GenMemSetInst();
    bool                        GenMemCopyInst();
    bool                        GenMemcopyAlignInst();
    bool                        GenMemcopyFollowInst();
    bool                        GenMemcopyEndingInst();
    void                        InsertAlignInstToQ();
    void                        InsertEndingInstToQ();
    void                        GenbfiInstMset(uint32_t stid = 0);
    void                        GenExtendInst();
    void                        GetLast(SimInst &inst, BlockCommandPtr cmd);
    /* \brief CALL/INDCALL block prologue genaration */
    void                        genCallPrologue();
    /* \brief Set global epilogue genaration */
    void                        GenSetEpilogue();
    /* \brief MEMSET block template genaration */
    bool                        GenMemSetTemplate();
    /* \brief MEMCOPY block template genaration */
    bool                        GenMemCopyTemplate();
    /* \brief MEMPUSH block template genaration */
    void                        genMemPushTemplate();
    /* \brief MEMPOP block template genaration */
    void                        genMemPopTemplate();
    /* \brief f.exit block template genaration */
    bool                        GenFExitTemplate();
    /* \brief f.entry block template genaration */
    bool                        GenFEntryTemplate();
    /* \brief f.texit block template genaration */
    bool                        GenFRetTemplate();
    bool                        GenEcall();
    void                        Build(uint32_t pe);
    void                        Xfer();
    uint32_t                    calculte_bits(uint32_t mask);
    /* 从低位到高位检查，返回输入掩码中第n个置位为1的bit位下标。 */
    uint32_t                    obtainSetBitID(uint32_t mask, uint32_t n);
    SimInst                     GenTemplateInitSimInst(uint32_t stid = 0);
    void                        GenTemplateInstSrc(SimInst &inst, OperandType type, uint64_t value);
    void                        GenTemplateInstDst(SimInst &inst, OperandType type, uint64_t value);
    void                        GenBLbarTemplate();
    void                        resetGen();

    void                        Reset();
    void                        Work();
    SimSys                      *GetSim();
    void ReportStat() override {}
    void                        flush(FlushBus &flushReq);
    bool                        checkStall(uint32_t inputCnt=1);
};


} // namespace JCore
