#pragma once

#include <set>
#include <vector>
#include "../configs/vectorcore_config.h"
#include "../configs/iex_config.h"
#include "iex/iex_stat.h"
#include "iex/pipe/iex_pipe.h"
#include "iex/iex_vab.h"
#include "core/Bus.h"
#include "ModelCommon/SimInstInfo.h"

namespace JCore {

using IssueChkFn = std::function<bool(const SimInst&, IssueBlockReason&)>;

/* PE Issue Queue State */
class IssueQueue;

enum class IsqOrder {
    OOO,                // default isq out of order
    INORDER,            // ISSUE inorder(within thread)
    STRICTLY_INORDER,   // Keep order of different threads(Make isq a fifo)
    COUNT
};

struct AgeQueue {
    IEXConfig                   *configs;
    IEX                         *top;
    std::vector<SimInst>          entry;
    uint32_t                    size = 0;
    bool                        update = false; // Optimization for executing.
    uint32_t                    storeCnt = 0;

    void                        Build(uint32_t size);
    void                        insert(SimInst &inst);
    bool                        loadLimit(SimInst &inst);
    void                        issued(SimInst &inst, bool state);
    void                        Release(ROBID bid, ROBID rid, uint32_t stid);
    void                        sortByAge();
    void                        Reset();
};

struct WakeupInfo {
    OperandType type = OperandType::OPD_INVALID;
    uint32_t ptag = 0;
    bool recycled = false;

    WakeupInfo(OperandType typeVal, uint32_t ptagVal, bool recycledVal);
    ~WakeupInfo();
};

struct IssueState {
    IssueQueue                  *top;
    std::vector<SimInst>          entry;
    bool                        insert_stall = false; // For print only
    bool                        sliding_window_vld = false;
    uint64_t                    sliding_window_sid = 0;
    uint64_t                    sliding_window_load_id = 0;
    std::string                 name; // used in pipeview
    uint32_t                    size = 0;
    uint32_t                    sizeNotIssued = 0; // Use for statistics
    uint64_t                    reserveSize = 0;
    AgeQueue                    *pAgeQueue = NULL;
    bool                        insertEle = false; // Optimization for executing.
    uint32_t                    selected_biot_id = 0;
    ROBID                       insertId;
    uint32_t                    ReadyInstCount();
    SimInst                     Select(IssueState* next, bool& ldqLimit, uint32_t oldestOption,
                                       const PipeStallFn_t& pipeStallFn, const std::vector<IssueChkFn>& cantIssue,
                                       const std::vector<IssueChkFn>& cancelIssue,
                                       std::vector<IssueBlockReason>& reasons);
    SimInst                     Select_SC(IssueState *next, bool &ldqLimit, const PipeStallFn_t& pipeStallFn,
                                          const IssueChkFn& cantIssue, const std::vector<IssueChkFn>& cancelIssue);

    bool                        ssrsetCheck(SimInst inst);
    bool                        CheckSysStateStall(const SimInst inst);
    void                        insert(SimInst inst);
    bool                        Wakeup(const WakeupInfo& wakeInfo, PLpvInfo lpvInfo = nullptr,
                                       uint32_t peID = 0, uint32_t tid = 0, uint32_t stid = 0);
    void                        WakeupTmpSrcD(uint32_t dTag, uint32_t peid, PLpvInfo &lpvInfo, uint32_t tid);
    void                        wakeupLda(uint64_t id);
    void                        windowSlides(uint64_t distance, bool isLoad = false);
    bool                        wakeupGLReg(uint32_t ptag, PLpvInfo &lpvInfo);
    void                        wakeupAddrSrc();
    void                        wakeupAddrSrc(uint32_t stid);
    void                        deliveryData(uint32_t ptag, uint64_t data);
    void                        wakeupStackLoad(ROBID bid, ROBID rid, PLpvInfo &lpvInfo);
    void                        Reset();
    void                        Build(uint32_t size);
    void                        flush(FlushBus flushReq);
    void                        moveLpv();
    void                        setCancel(uint32_t pipe);
    void                        setCancel(const SimInst &ins, uint32_t pip);
    void                        ReleaseEntry(ROBID bid, ROBID rid, uint32_t stid);
    void                        Xfer();
    bool                        full();
    bool                        reservefull();
    bool                        storeIDMatch(SimInst &inst);
    bool                        IsInWindow(SimInst inst);
    bool                        IsInLdWindow(SimInst &inst);
    bool                        IsInStWindow(SimInst &inst);

    IssueState& operator=(const IssueState &rhs);
};

struct IssueQ {
    std::vector<IssueState>         cmdIQ;  // 1 CMDQ
    std::vector<IssueState>         aluIQ;  // 6 ALUQ
    std::vector<IssueState>         ldaIQ;  // scalar agu 4 AGUQ
    std::vector<IssueState>         ldaScIQ;  // scalper scalar agu 4 AGUQ
    std::vector<IssueState>         aguIQ;  // vector agu
    std::vector<IssueState>         staIQ;  // 4 STAQ
    std::vector<IssueState>         stdIQ;  // 4 STDQ
    std::vector<IssueState>         bruIQ;  // 4 BRUQ
    std::vector<IssueState>         scaIQ;  // vector scalar pipe

    // Load and store instructions sort by age.
    AgeQueue                        aguAgeQueue;
    AgeQueue                        aguSCAgeQueue;
    bool                            isEmpty();
};

enum IssueType {
    CMD,
    ALU,
    AGU,
    LDA,
    STA,
    STD,
    BRU,
    SCA, // Vector Scalar Pipe
};

class IEX;
class VAB;

struct IssueStats {
    uint32_t selectCnt = 0;
    uint64_t vcvt = 0;
    uint64_t vfp = 0;
    uint64_t shfl = 0;
    uint64_t divSqrt = 0;
    uint64_t vint = 0;
    uint32_t readyNeedPickCnt = 0;
};

struct RdPortControl {
    enum class PipeId {
        ALU0 = 0,
        ALU1,
        ALU2,
        AGU0,
        AGU1,
        NUM,
        UNDEF = 0xff
    };

    enum class PipeSrcId {
        ALU0_SRC0 = 0,
        ALU0_SRC1,
        ALU0_SRC2,
        ALU1_SRC0,
        ALU1_SRC1,
        ALU1_SRC2,
        ALU2_SRC0,
        ALU2_SRC1,
        ALU2_SRC2,
        AGU0_SRC0,
        AGU0_SRC1, // should not exist
        AGU0_SRC2, // should not exist
        AGU1_SRC0, // should not exist
        AGU1_SRC1, // should not exist
        AGU1_SRC2, // should not exist
        NUM,
        UNDEF = 0xff
    };

    static std::string GetPipeSrcIdStr(PipeSrcId pipeSrcId);

    explicit RdPortControl(IEX* top);
    ~RdPortControl();

    void Build();
    void Reset();
    bool IssueAndUpdate(PipeId pipe, const SimInst& inst, bool update);
    bool CanIssue(PipeId pipe, const SimInst& inst);
    void PostIssueUpdate(PipeId pipe, const SimInst& inst);

    IEX* mTop { nullptr };
    std::vector<std::vector<uint32_t>> mVrfPipeSrcRdPorts; // PipeSrcId, ReadPort
    std::vector<std::set<uint32_t>> mVrfRdPortBanksInit; // ReadPort, Bank
    std::vector<std::set<uint32_t>> mVrfRdPortBanksAvailable; // ReadPort, Bank
};

class IssueQueue {
private:
    /* \brief Issue Stage */
    void                            issueCMD();
    SimInst                         IssueALUIqPick(uint32_t iqId, uint32_t pickId,
                                                   const std::vector<IssueChkFn>& cantIssueFn,
                                                   const std::vector<IssueChkFn>& cancelIssueFnVec,
                                                   std::vector<IssueBlockReason>& blockReasons);
    void                            IssueALUIqUpdateStats(uint32_t iqId, uint32_t pickId, const SimInst& issueInst,
                                                          std::vector<uint32_t>& iqExecIssueLimit,
                                                          IssueStats& issStats);
    void                            IssueALUIqGetCheckFn(uint32_t pickId, uint32_t nonHeterPickCount,
                                                         const std::vector<uint32_t>& iqExecIssueLimit,
                                                         std::vector<IssueChkFn>& cantIssueFn,
                                                         std::vector<IssueChkFn>& cancelIssueFnVec);
    void                            IssueALUIq(uint32_t iqId, IssueStats& issStats);
    void                            issueALU();
    void                            issueAGU();
    void                            issueGatherAGU();
    void                            issueSCAGU();
    void                            issueSTA();
    void                            issueSTD();
    void                            issueBRU();
    void                            issueVecAGU();
    void                            issueVecSca();
    void                            SetCorePEBound(BIQType biqType, uint64_t cycle) const;
    void                            SetCorePEBoundBrobStall(BIQType biqType, uint64_t cycle) const;
    void                            SetCorePEBoundBiqStall(BIQType biqType, uint64_t cycle) const ;
    void                            SetCorePEBoundTileTagStall(BIQType biqType, uint64_t cycle) const;
    void                            SetCoreTopDownBound(bool efficient, SimQueue<SimInst> &isq, uint64_t cycle = 1) const;
    void                            RecordVecIssueBlockReasons(std::vector<IssueBlockReason> blockReasons) const;
    void                            RecordVecAluPickerIssue(uint32_t pickId, const SimInst& inst) const;

public:
    IssueQueue() {};
    ~IssueQueue() {};
    IEXConfig                       configs;
    IsqOrder                        order = IsqOrder::OOO;
    IEX                             *top;
    IssueQ                          current;
    IssueQ                          next;
    SimQueue<SimInst>                 cmdIQ;

    /* \brief Output selected cmd instructions into RF stage */
    std::vector<SimInst>              selectCmdInst;
    /* \brief Output selected alu instructions into RF stage */
    std::vector<SimInst>              selectAluInst;
    /* \brief Output selected agu instructions into RF stage */
    std::vector<SimInst>              selectAguInst;
    /* \brief Output selected lda instructions into RF stage */
    std::vector<SimInst>              selectLdaInst;
    /* \brief Output selected lda instructions into RF stage for scalper */
    std::vector<SimInst>              selectSCLdaInst;
    /* \brief Output selected sta instructions into RF stage */
    std::vector<SimInst>              selectStaInst;  // size=1
    /* \brief Output selected std instructions into RF stage */
    std::vector<SimInst>              selectStdInst;
    /* \brief Output selected bru instructions into RF stage */
    std::vector<SimInst>              selectBruInst;
    std::vector<SimInst>              selectScaInst;

    bool                            cmdIQStall;
    bool                            aluIQStall;
    bool                            aguIQStall;
    bool                            ldaIQStall;
    bool                            ldaSCIQStall;
    bool                            staIQStall;
    bool                            stdIQStall;
    bool                            bruIQStall;
    bool                            scaIQStall;

    std::vector<PipeStallFn_t>      aluPipeStallFn;
    std::vector<PipeStallFn_t>      scaPipeStallFn;

    //----------------------------------------
    //------Interface between stages----------
    //----------------------------------------
    /* \brief Input cmd instructions from each PE rename stage to issue queue (array Build in Core) */
    INPUT std::vector<SimQueue<SimInst>*>   pe_iex_cmd_array;
    /* \brief Input alu instructions from each PE rename stage to issue queue (array Build in Core) */
    INPUT std::vector<SimQueue<SimInst>*>   pe_iex_alu_array;   // 4 PE
    /* \brief Input agu instructions from each PE rename stage to issue queue (array Build in Core) */
    INPUT std::vector<SimQueue<SimInst>*>   pe_iex_lda_array;
    /* for scalper */
    INPUT std::vector<SimQueue<SimInst>*>   pe_iex_sc_lda_array;
    /* \brief Input sta instructions from each PE rename stage to issue queue (array Build in Core) */
    INPUT std::vector<SimQueue<SimInst>*>   pe_iex_sta_array;
    /* \brief Input std instructions from each PE rename stage to issue queue (array Build in Core) */
    INPUT std::vector<SimQueue<SimInst>*>   pe_iex_std_array;
    /* \brief Input std instructions from each PE rename stage to issue queue (array Build in Core) */
    INPUT std::vector<SimQueue<SimInst>*>   pe_iex_bru_array;
    INPUT std::vector<SimQueue<SimInst>*>   pe_iex_vec_agu_array;
    INPUT std::vector<SimQueue<SimInst>*>   pe_iex_vec_scalar_array;

    INPUT SimQueue<SimInst>                 isq_wake_q;
    std::shared_ptr<VAB>                  vab;

    std::vector<std::function<bool(const SimInst&, IssueBlockReason&)>> vecAluIqPickerCantIssueFn; // per picker
    std::vector<uint32_t> vecAluIqTotalIssueLimit; // per type

    uint32_t                              aluIqPickerRRIndex = 0;

    std::shared_ptr<RdPortControl>        rdPortCtrl { nullptr };
    //----------------------------------------
    void                            Build();
    void                            Reset();
    void                            Work();
    void                            Xfer();
    SimSys                          *GetSim();
    void                            Dump();

    //----------------------------------------
    //------Externally called functions-------
    //----------------------------------------
    /* \brief wakeup by register ready */
    void                            WakeupIQTag(const WakeupInfo& wakeInfo, PLpvInfo lpvInfo = nullptr,
                                                uint32_t peID = 0, uint32_t tid = 0, uint32_t stid = 0);
    /* \brief Inner wakeup by T register due to part sequential ready */
    // void                            WakeupDummy(uint32_t peID, uint32_t dTag, PLpvInfo &lpvInfo, uint32_t tid);
    /* \brief Inner wakeup by sta ready on perfect simulation mode only */
    void                            wakeupLda(uint64_t id);
    /* \brief Store window slides */
    void                            windowSlides(uint64_t distance, bool isLoad = false);
    /* \brief External wakeup by outer global stack ptag ready */
    void                            wakeupSptag(uint32_t sptag, bool recycled, PLpvInfo &lpvInfo);
    /* \brief  delete inst from issue queue by rob id */
    void                            ReleaseEntry(ROBID bid, ROBID rid, uint32_t stid);
    /* \brief Release stack-load-check by stack-get*/
    void                            wakeupStackLoad(ROBID bid, ROBID rid, PLpvInfo &lpvInfo);
    void                            flush(FlushBus flushReq);
    void                            deliveryData(uint32_t ptag, uint64_t data);

    void                            setIQCancel(uint32_t pipe);
    void                            releaseIQEntry(ROBID bid, ROBID rid, uint32_t stid);
    void                            releaseCMDEntry(ROBID bid, ROBID rid, uint32_t stid);
    void                            releaseALUEntry(ROBID bid, ROBID rid, uint32_t stid);
    void                            releaseAGUEntry(ROBID bid, ROBID rid, uint32_t stid);
    void                            ReleaseLDAEntry(ROBID bid, ROBID rid, uint32_t stid);
    void                            ReleaseLDASCEntry(ROBID bid, ROBID rid, uint32_t stid);
    void                            releaseSTAEntry(ROBID bid, ROBID rid, uint32_t stid);
    void                            releaseSTDEntry(ROBID bid, ROBID rid, uint32_t stid);
    void                            releaseBRUEntry(ROBID bid, ROBID rid, uint32_t stid);
    void                            ReleaseVecScaEntry(ROBID bid, ROBID rid, uint32_t stid);
    void                            recieveWakeReq();

    void                            setCancel(const SimInst &inst, uint32_t pip);
    void                            RptIQStats(const SimInst &inst, IssueState &iq, IQStats &iq_stat, uint32_t id);
    void                            CoutStat(uint32_t readyCycle);
    bool                            CheckBCCBound();

    void                            InitVecALUHelperFnAndStruct();
    IexExecUnit                     GetIssueExecUnit(Opcode opcode) const;
};

} // namespace JCore
