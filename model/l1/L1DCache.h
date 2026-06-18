#pragma once

#include "../configs/l1_config.h"
#include "core/Packet.h"
#include "l1/L1.h"
#include "plat/DebugLog.h"

namespace JCore {

class L1Clusters;

class L1DCache : public L1Cache {
private:
    bool verbose;
    std::vector<int> bitList;

public:
    SimSys *sim = NULL;
    L1Config *pConfigs;
    LSUConfig  *lsuConfigs = nullptr;
    std::shared_ptr<DebugLog> debugLogger = nullptr;
    L1Clusters *cluster;
    
    L1DCache();
    ~L1DCache() override;

    bool      lookup(uint64_t va, bool write = false, bool *phit = nullptr);
    L1Entry   refill(uint64_t addr, uint64_t *data);
    L1Entry   refill(uint64_t addr, uint64_t *data, CacheState cs);
    L1Entry   refill(uint64_t addr, uint64_t *data, CacheState cs, PtrPacket pkt);
    uint512_t getData(uint64_t addr);
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
};

} // namespace JCore