#pragma once

#include "../configs/mtc_l1_config.h"
#include "core/Packet.h"
#include "mtccore/l1/MTC_L1Cache.h"
#include "plat/DebugLog.h"

namespace JCore {

class MTC_L1Clusters;

class MTC_L1DCache : public MTC_L1Cache {
public:
    SimSys *sim = NULL;
    MTC_L1Config *pConfigs;
    mLSUConfig  *lsuConfigs = nullptr;
    std::shared_ptr<DebugLog> debugLogger = nullptr;
    MTC_L1Clusters *cluster;
    
    MTC_L1DCache();
    ~MTC_L1DCache() override;

    bool      lookup(uint64_t va, bool write = false, bool *phit = nullptr);
    MTC_L1Entry   refill(uint64_t addr, uint64_t *data);
    MTC_L1Entry   refill(uint64_t addr, uint64_t *data, MTC_CacheState cs);
    MTC_L1Entry   refill(uint64_t addr, uint64_t *data, MTC_CacheState cs, PtrPacket pkt);
    uint2048_t getData(uint64_t addr);
    void      dataUpdate(uint64_t addr, uint64_t data, uint32_t width);
    void      upgradeBroadcast(uint64_t addr);
    uint32_t  calcSetIdx(uint64_t va) override;
    bool      queryByTag(uint64_t tag);

    void   Reset();
    void   Work();
    void   Xfer();
    SimSys *GetSim();
    void   Build(void);
    void ReportStat() override {}
private:
    bool verbose;
    std::vector<int> bitList;
};

} // namespace JCore