#pragma once

#include <chrono>
#include <map>
#include <vector>

#include "../configs/core_config.h"
#include "bctrl/BCtrl.h"
#include "bctrl/bfu/bfu.h"
#include "core/Bus.h"
#include "core/flush_stats.h"
#include "core/Packet.h"
#include "core/SRename.h"
#include "l2/L2Cache.h"
#include "lsu/lsu.h"
#include "ModelSpec.h"
#include "pe/PEBase.h"

namespace JCore {

class FlushControl : public SimObj {
private:
    // TODO: Support multi-thread
    /* \brief Enable for flushSignal */
    bool                            flushSignalEnabled;
    /* \brief Global flush */
    FlushBus                        flushSignal;
    /* \brief Global replay */
    FlushBus                        replaySignal;
    /* \brief Flush/replay based on PE */
    std::vector<FlushBus>           peSignal;
    /* \brief Flush/replay based on PE */
    std::vector<bool>               peSignalEnabled;

public:
    Core                            *core;
    SimSys                          *sim;
    std::shared_ptr<DebugLog>       debugLogger = nullptr;
    FlushStats                      *flush_stats;
    SimQueue<FlushReq>              *bcc_flush_rpt_q;
    vector<SimQueue<FlushReq>*>      iex_flush_rpt_q;
    std::vector<SimQueue<FlushReq>*> pe_flush_rpt_array;
    std::vector<SimQueue<FlushReq>*>  lsu_flush_rpt_q;

    FlushControl() {};
    virtual ~FlushControl();

    SimSys                          *GetSim();
    /* \brief Initial */
    void                            Build();
    /* \brief Reset the flush uint */
    void                            Reset();
    /* \brief Work in current cycle */
    void                            Work();
    /* \brief Change next state to current */
    void                            Xfer();
    void ReportStat() override {}
    /* \brief Report signal for flush or replay */
    void                            report(FlushReq &signal);
    /* \brief Check older flush signal */
    bool                            CheckOlder(FlushBus &srcSignal, FlushBus &dstSignal);
    /* \brief Select signal */
    void                            select(FlushBus &signal);
    /* \brief Merge flush and replay signal. */
    bool                            mergeSignal(FlushBus &srcSignal, FlushBus &dstSignal);
    /* \brief Check if it is effect. */
    bool                            CheckFlush(FlushBus &signal);
    /* \brief Select flush signal */
    void                            selectFlushSigal(FlushBus &signal);
    /* \brief Select replay signal */
    void                            selectReplaySigal(FlushBus &signal);
    /* \brief Select PE replay signal */
    void                            selectPESigal(FlushBus &signal);
    /* \brief Do for Flush */
    void                            flush();
    /* \brief Do for replay */
    void                            replay();
    /* \brief Do for pe replay */
    void                            FlushBaseOnPE();
    /* \brief Flush for back-end */
    void                            flushBackend(FlushBus &signal);
    /* \brief Check if it is needed to flush */
    bool                            needFlush();
    bool                            needFlush(ROBID &bid);
    bool                            needFlush(ROBID &bid, ROBID &subID);
    /* \brief Clean in report queue(Occur flush while reporting the younger signal to the queue.) */
    void                            flushRpt(FlushBus &signal);
    /* \brief Record efficient flush for statistics */
    void                            rptStatistics(FlushBus flushReq);
    /* \brief PrintLog while select FlushSigal */
    void                            PrintLogThrow(FlushBus &olderSignal, FlushBus &throwSignal);
    void                            PrintLogSelect(FlushBus &cancelSignal, FlushBus &selectSignal);
};

} // namespace JCore