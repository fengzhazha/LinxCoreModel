#pragma once

#include <deque>
#include <map>
#include <set>
#include <vector>

#include "l1/L1.h"
#include "pe/ifu/common/ifu_component.h"

namespace JCore {


namespace PE_IFU {

class IFUICache : public L1Cache, public IFUComponent {
public:
    IFUICache();
    ~IFUICache();

    void                            Reset();
    void                            Work();
    void                            Xfer();
    void                            Build();
    SimSys                          *GetSim();
    void ReportStat() override {}
    bool                            verbose;
    std::deque<MissQEntry>          missQ;

    FetchReqPtr                     stallPtr;

    /* \brief Get whether look up ICache is stall. */
    bool                            LookupStall();
    /* \brief Get whether get data from ICache is stall. */
    bool                            GetDataStall();
    /* \brief Get whether ICache can send miss request. */
    bool                            CanSendReq();
    /* \brief Get whether ICache can send prefetch miss request. */
    bool                            CanSendPref();
    /* \brief virtual address hit in tag array */
    bool                            lookupTag(uint64_t virAddr);
    /* \brief get cache data with PE fetch request */
    void                            getCacheData(FetchReqPtr const& fetchReq, uint32_t fetchWidth);
    /* \brief refill cache data by PLRU */
    L1Entry                         refill(uint64_t addr, uint64_t *data);
    /* \brief Look up miss queue. */
    bool                            lookUpMissQ(uint64_t tpc);
    /* \brief Insert miss address to miss queue. */
    bool                            insertMissQ(uint64_t tpc, bool pref);
    /* \brief Send miss requests to L2. */
    void                            SendMissReq();
    /* \brief Release entry in miss queue. */
    void                            releaseMissQ(uint64_t addr, bool pref);

private:
};
}

} // namespace JCore
