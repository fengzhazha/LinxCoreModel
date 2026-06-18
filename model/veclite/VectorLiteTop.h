#ifndef  BLOCKISA_MODEL_VECLITE_TOP_H
#define  BLOCKISA_MODEL_VECLITE_TOP_H
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "veclite/VectorLite.h"
#include "core/interface.h"
#include "SimSys.h"

namespace JCore {

class Core;
class IEX;
class OPE;
class VecPE;

class VectorLiteTop : public SimObj {
public:
    VectorLiteTop();
    ~VectorLiteTop() final;
    void Work() override;
    void Xfer() override;
    void Reset() override;
    SimSys *GetSim() override;
    void SetSim(SimSys *sm);
    Core *GetCore();
    void SetCore(Core *c);
    void Build();
    void BuildVecCore();
    uint32_t GetVecNum();
    void InitRing(std::shared_ptr<CrossStation> &station);
    void InitDualPERing(unsigned coreIdx, std::shared_ptr<CrossStation> &station);
    void PrintConfigs();
    void ReportStat() override;
    uint32_t BussyCount();
    void Dump();
    void Replay(const FlushBus &bus) const;
    void SetFlush(const FlushBus &bus);
    void ResetStats();
    uint64_t IexLdPipeCount(uint32_t coreId = 0);
    // TODO: For adapt current code.
    void ChangedSelectCore();
    uint32_t GetSelectCore() const;
    const VectorLiteCoreConfig& GetConfig() const;

    bool CheckLoadQEmpty(uint32_t coreId) const;
    uint32_t GetVecPEDecWidth() const;

private:
    Core *core = nullptr;
    SimSys *sim = nullptr;
    std::shared_ptr<VecPE> m_pe = nullptr;
    uint32_t selectCore = 0;
    std::vector<std::shared_ptr<VectorLite>>m_vecLiteCores;
};

} // namespace JCore

#endif
