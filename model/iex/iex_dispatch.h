#pragma once

#include "../configs/iex_config.h"
#include "core/Bus.h"
#include "iex/iex_iq.h"
#include "iex/iex_stat.h"
#include "ModelCommon/SimInstInfo.h"

namespace JCore {

class IEX;

struct DispatchInfo {
    // Dispatch ID of different issue queue
    uint32_t                        dispatchID = 0;
    // Count of instrutions stay in IQ for each PE
    vector<uint32_t>                count;
    // Maximum count of instrutions stay in IQ for each PE
    uint32_t                        maxCount;
};

class DispatchUnit : public SimObj {
private:
    IEXConfig                       configs;
public:
    DispatchUnit() {};
    virtual ~DispatchUnit() {};
    /* \brief Input cmd instructions from each PE rename stage to issue queue */
    std::vector<SimQueue<SimInst>*>   pe_iex_cmd_array;   // 4 PE
    /* \brief Input alu instructions from each PE rename stage to issue queue */
    std::vector<SimQueue<SimInst>*>   pe_iex_alu_array;   // 4 PE
    /* \brief Input lda instructions from each PE rename stage to issue queue */
    std::vector<SimQueue<SimInst>*>   pe_iex_lda_array;
    std::vector<SimQueue<SimInst>*>   pe_iex_sc_lda_array;
    /* \brief Input sta instructions from each PE rename stage to issue queue */
    std::vector<SimQueue<SimInst>*>   pe_iex_sta_array;
    /* \brief Input std instructions from each PE rename stage to issue queue */
    std::vector<SimQueue<SimInst>*>   pe_iex_std_array;
    /* \brief Input bru instructions from each PE rename stage to issue queue */
    std::vector<SimQueue<SimInst>*>   pe_iex_bru_array;
    std::vector<SimQueue<SimInst>*>   pe_iex_vec_agu_array;
    std::vector<SimQueue<SimInst>*>   pe_iex_vec_scalar_array;

    // Information of diffrrent issue queue
    DispatchInfo                    cmdInfo;
    DispatchInfo                    aluInfo;
    DispatchInfo                    aguInfo;
    DispatchInfo                    ldaInfo;
    DispatchInfo                    ldascInfo;
    DispatchInfo                    staInfo;
    DispatchInfo                    stdInfo;
    DispatchInfo                    bruInfo;
    DispatchInfo                    scaInfo;

    queue<uint32_t>                 peid_q;

    uint32_t                        reservedNum = 0;

    IEX                             *top;
    SimSys                          *sim;
    SimSys                          *GetSim() { return sim; }
    void ReportStat() override {}
    void                            Work();
    void                            Xfer();
    void                            Build(uint32_t peCount);
    void                            Reset();
    void                            flush(const FlushBus& flushReq);
    /* \brief Dispatch for PE-IEX queue */
    void                            dispatch();
    void                            setCancel(uint32_t pipe);
    /* \brief Deque instructions from pe-iex queue */
    void                            ReleaseEntry(ROBID bid, ROBID rid, uint32_t stid);
    /* \brief Report Statistics for dispatch */
    void                            rptDispatchStats();
    void                            SelectMinPEIDToQueue(vector<SimQueue<SimInst>*> &array);
    void    GenFlushRequest(ROBID bid, uint32_t fbid, uint64_t tpc, uint32_t peid, uint32_t stid) const;
};

} // namespace JCore
