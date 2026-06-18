#ifndef  BLOCKISA_MODEL_VECTORTOP_GROB_H
#define  BLOCKISA_MODEL_VECTORTOP_GROB_H
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "vectorcore/VectorCore.h"
#include "core/interface.h"
#include "SimSys.h"

namespace JCore {

class Core;
class IEX;

class VectorTop : public SimObj {
public:
    VectorTop();
    ~VectorTop() final;
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
    void ReportIEXStats();
    void ReportTileLoadLatDis();
    void ReportVectorKeyStat();
    void ReportVectorTopDown();
    void ReportStat() override;
    uint32_t BussyCount();
    ROBID GetOldestGid(uint32_t stid);
    void Dump();
    void Replay(FlushBus &bus);
    void SetFlush(FlushBus &bus);
    uint32_t GetLdqCredit(uint32_t coreId);
    void SubLdqCredit(uint32_t coreId, uint32_t num);
    uint32_t GetStqCredit(uint32_t coreId, uint32_t tid);
    void SubStqCredit(uint32_t coreId, uint32_t tid, uint32_t num);
    uint64_t GetTileLdqSize(uint32_t coreId = 0);
    uint64_t GetTileStqSize(uint32_t coreId = 0);
    void ResetStats();
    uint64_t IexLdPipeCount(uint32_t coreId = 0);
    // TODO: For adapt current code.
    std::shared_ptr<IEX> GetIex(uint32_t coreId);
    std::shared_ptr<VecPE> GetPE(uint32_t coreId);
    void SetPE(std::shared_ptr<VecPE> pe, uint32_t coreId);
    void ChangedSelectCore();
    uint32_t GetSelectCore() const;
    const VectorCoreConfig& GetConfig() const;

    void SetPeBussy(uint32_t coreId, uint64_t cycle = 1);
    void SetPeRegTagStall(uint32_t coreId, uint64_t cycle = 1);
    void SetPeRobStall(uint32_t coreId, uint64_t cycle = 1);
    void SetPeMemoryBound(uint32_t coreId, uint64_t cycle = 1);
    void SetDecIns(uint32_t coreId, uint64_t insts = 1);
    void ReduceDecIns(uint32_t coreId, uint64_t insts = 1);
    void SetPeIqStall(uint32_t coreId, uint64_t cycle = 1);
    void SetPeRetireIns(uint32_t coreId, uint64_t insts);
    bool CheckLoadQEmpty(uint32_t coreId);

private:
    Core *core = nullptr;
    SimSys *sim = nullptr;
    uint32_t selectCore = 0;
    std::vector<std::shared_ptr<VectorCore>>m_vecCores;
};

} // namespace JCore

#endif