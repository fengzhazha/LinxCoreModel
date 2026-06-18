#pragma once

#include <sstream>
#include <vector>

#include "../configs/core_config.h"
#include "../configs/iex_config.h"
#include "ModelCommon/SimInstInfo.h"
#include "ModelCommon/ModelCommon.h"
#include "ModelCommon/bus/FlushBus.h"
#include "ModelCommon/bus/ResolveBlockBus.h"

namespace JCore {

struct TableInfo {
    bool                                        ready;
    bool                                        global;
    bool                                        retired;
    bool                                        inst_retired;
    bool                                        data_vld;
    ROBID                                       bid;
    ROBID                                       rid;
    PLpvInfo                                    lpvInfo;
    uint32_t                                    peid;
    uint64_t                                    data;
    TableInfo() { ready = false; global = true; retired = false; inst_retired = false; data_vld = false; }
    void Reset() { TableInfo(); }
};

using PTInfo = std::shared_ptr<TableInfo>;

struct ReadyTable {
    /* \brief T register ready table */
    std::vector<std::vector<std::vector<PTInfo>>>   tRegReady;
    /* \brief U register ready table */
    std::vector<std::vector<std::vector<PTInfo>>>   uRegReady;
    /* \brief VT/VU/VM register ready table */
    std::vector<PTInfo>                             vrfReady;
    /* \brief Predicate register ready table */
    std::vector<std::vector<std::vector<PTInfo>>>   simtMaskRegReady;
    /* brief local ptag ready table */
    std::vector<PTInfo>                      *ptagReady;
    std::vector<PTInfo>                      lptagReady;
    /* \brief stack ready table */
    std::vector<PTInfo>                      sptagReady; // TODO : move stack ready table to here
    /* \brief ready table for split instruction itself */
    std::vector<std::vector<PTInfo>>         robReady;
    SimSys*                                  sim = nullptr;

    void                                        Build(CoreConfig const &configs, uint32_t peCount, ExecEngineTyp type,
                                                    uint32_t robDepth);
    void                                        Reset();
    void                                        MoveLpv();
    ReadyTable() {}
    ReadyTable(const ReadyTable &copy)
    {
        tRegReady = copy.tRegReady;
        uRegReady = copy.uRegReady;
        vrfReady = copy.vrfReady;
        simtMaskRegReady = copy.simtMaskRegReady;
        *ptagReady = *copy.ptagReady;
        lptagReady = copy.lptagReady;
        sptagReady = copy.sptagReady;
        robReady = copy.robReady;
    }
    void operator=(const ReadyTable &copy)
    {
        tRegReady = copy.tRegReady;
        uRegReady = copy.uRegReady;
        vrfReady = copy.vrfReady;
        simtMaskRegReady = copy.simtMaskRegReady;
        *ptagReady = *copy.ptagReady;
        lptagReady = copy.lptagReady;
        sptagReady = copy.sptagReady;
        robReady = copy.robReady;
    }
};

class IEX;

class ReadyState : public SimObj {
private:
    ReadyTable                                  current;
    ReadyTable                                  next;
    std::deque<SimInst>                         wr_inst_q;
public:
    ReadyState() {};
    virtual ~ReadyState() {};
    IEX                                         *top;
    SimSys                                      *sim;
    INPUT SimQueue<uint32_t>                    *vrfRtableTagFreeQ;
    INPUT SimQueue<uint32_t>                    *vrfRtableTagRetireQ;
    /* \brief Ptag release request from block rename and PE to iex-ptag-ready */
    INPUT SimQueue<uint32_t>                    *rtable_release_q;
    /* \brief Ptag retire request from BROB to iex-ptag-ready */
    INPUT SimQueue<ROBID>                       *block_rtable_retire_q;
    /* \brief WF request from IEX */
    INPUT SimQueue<RFReqBus>                    *iex_rt_wr_q;
    /* \brief WF request from ready table to regfile */
    OUTPUT SimQueue<RFReqBus>                   *rf_wr_q;
    /* \brief Wakeup request from ready-table to isq */
    OUTPUT SimQueue<SimInst>                      *isq_wake_q;
    /* \brief Resolve at s1 stage */
    OUTPUT std::vector<std::vector<SimQueue<PEResolveBus>*>>    rslv_array;
    /* \brief Template block rf */
    OUTPUT std::vector<SimQueue<SimInst>*>        rf_ct_q;
    void                                        Work();
    void                                        Xfer();
    void                                        flush(FlushBus flushReq);
    void                                        Build(uint32_t peCount);
    void                                        Reset();
    void ReportStat() override {}
    /* \brief Check from ready table */
    void                                        checkReady(SimInst &inst);
    /* \brief Check ready */
    void                                        CheckReadySrc(SimInst &inst, size_t idx, std::string &dispInfo);
    /* \brief information of id update for ready table */
    void                                        CheckDst(SimInst &inst, uint32_t idx);
    /* \brief Set/Unset t register ready table */
    void                                        SetRegReadyTable(OperandType type, uint32_t ptag, bool ready,
                                                                PLpvInfo lpvInfo = nullptr, uint32_t peID = 0, uint32_t tid = 0);
    void                                        setROBReadyTable(uint32_t peid, uint32_t rid, PLpvInfo &lpvInfo, bool ready);
    /* \brief Set/Unset stack ready table */
    void                                        setSptagReady(uint32_t sptag, bool ready);

    SimSys                                      *GetSim() { return sim; }
    /* \brief mark ptag retired for the whole block */
    void                                        setBlockRetire(ROBID bid);
    /* \brief mark ptag retired for the ptag */
    void                                        setPtagRetire(uint32_t ptag);
    /* \brief check ptag ready */
    bool                                        checkPtagReady(uint32_t ptag);
    PLpvInfo                                    checkPtagLpvInfo(uint32_t ptag);
    /* \brief update ROBDID for ready table for flush/retire */
    void                                        resetPtagID(uint32_t ptag, IDBus bus, bool global);
    /* \brief change status for ready table */
    void                                        ReleasePtag(uint32_t ptag, PLpvInfo &lpvInfo);
    void                                        setState();
    /* \brief Check load ready of refcore perfect load-store mode */
    void                                        checkLoadReady(SimInst &inst, std::string &dispInfo);
    /* \brief Resolve instructions at S1 stage */
    void                                        checkRslv(SimInst &inst, std::string &dispInfo);
    /* \brief data delivery bypass */
    void                                        dataBypass();
    /* \brief Check data ready from ready table data-buffer */
    void                                        GetSrcData(POperandPtr src, bool scalar);
    /* \brief Recive request from IEX */
    void                                        dataWrite();
    /* \brief Init GPR Rtable */
    void                                        InitGGPRRtable(uint64_t ptag, uint64_t data);
};

} // namespace JCore
