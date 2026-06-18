#ifndef BLOCKISA_MODEL_VECLITE_EXE_H
#define BLOCKISA_MODEL_VECLITE_EXE_H
#include <atomic>
#include <vector>
#include <map>
#include <memory>

#include "core/Bus.h"
#include "veclite/VectorLiteUOPIQ.h"

namespace JCore {

class VectorLite;

class VectorLiteExE {
public:
    enum class ExeStage {
        EX = 0,
        W1,
        W2,
        RESOLVED,
        NUM,
        UNDEF = 0xff
    };
    struct StageInfo {
        ExeStage stage { ExeStage::EX };
        uint32_t latency { 0 };
        uint32_t exStage { 0 };
        explicit StageInfo(uint32_t latencyVal)
            : latency(latencyVal)
        {}
        ~StageInfo() = default;
    };
    using ExeEntry = std::pair<VectorLiteUopT, std::shared_ptr<StageInfo>>;

    VectorLiteExE();
    ~VectorLiteExE();
    VectorLite*                         top = nullptr;
    void                                Work();
    void                                Xfer() const;
    void                                Build();
    bool                                CheckStall(TileOp tileOp) const;
    SimSys*                             GetSim() const;
    void                                SetSim(SimSys *sim);

    void                                SetFlush(const FlushBus &bus);
    bool                                IsIdle();
    void                                Stats() const;

    INPUT SimQueue<VectorLiteUopT>      *m_uiqEXEInQ;
    OUTPUT SimQueue<VectorLiteUopT>     *m_exeRobRslvQ;

private:
    SimSys*                             m_sim { nullptr };
    uint32_t                            m_exeMaxLatency { 5 };
    std::vector<ExeEntry>               m_entries;

    void RunEX(const ExeEntry& entry) const;
    void RunW1(const ExeEntry& entry) const;
    void RunW2(const ExeEntry& entry) const;

    void                                ReceiveUOP();
    bool                                Execute();

    void                                Resolve(const VectorLiteUopT &uop) const;
};

} // namespace JCore

#endif
