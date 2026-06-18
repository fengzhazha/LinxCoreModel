#ifndef BLOCKISA_MODEL_PE_LGPRRF_H
#define BLOCKISA_MODEL_PE_LGPRRF_H

#include <vector>
#include <unordered_map>

#include "LocalFreeList.h"

namespace JCore {

class Core;
class LGPRRF : public SimObj {
public:
    LGPRRF();
    ~LGPRRF();
    void Work();
    void Xfer();
    void Build();

    bool CheckStallInput(uint32_t reserve = 1);
    bool CheckStallOutput(uint32_t reserve = 1);
    void Flush(FlushBus &flushReq);
    void SetSim(SimSys* sim) { this->sim = sim; }
    SimSys* GetSim() { return sim; }

    void Reset() {};
    void ReportStat() {};

    std::shared_ptr<DebugLog> debugLogger;
    LocalFreeList lFreeListInput;
    LocalFreeList lFreeListOutput;

private:
    SimSys *sim = nullptr;
    Core *core = nullptr;
};

}

#endif