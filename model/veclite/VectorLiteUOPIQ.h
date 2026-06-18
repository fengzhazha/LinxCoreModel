#ifndef BLOCKISA_MODEL_VECLITE_UOP_IQ_H
#define BLOCKISA_MODEL_VECLITE_UOP_IQ_H
#include <vector>
#include <map>

#include "core/Bus.h"
#include "veclite/VectorLiteUop.h"
#include "veclite/VectorLiteUopInfo.h"
#include "veclite/VectorLiteScheduleIQ.h"

namespace JCore {

class VectorLite;

class VectorLiteUOPIQ {
public:
    enum class IssueOrder {
        OOO = 0,
        INORDER,
        COUNT,
        UNDEF = 0xff
    };

    enum class IqStage {
        I = 0,
        S,
        NUM,
        UNDEF = 0xff
    };

    struct StageInfo {
        IqStage stage { IqStage::I };
        uint32_t iCnt { 0 };
        uint32_t readRegLatency { 0 };
        uint64_t rdyCycle { 0 };
        explicit StageInfo(uint32_t readRegLatencyVal)
            : readRegLatency(readRegLatencyVal)
        {}
        ~StageInfo() = default;
    };
    using IqEntry = std::pair<VectorLiteUopT, std::shared_ptr<StageInfo>>;

    struct IqPipe {
        VectorLiteUOPIQ*                top { nullptr };
        uint32_t                        pipeId { 0 };
        uint32_t                        depth { 0 };
        uint32_t                        stallMinEntryNum { 0 };
        std::deque<IqEntry>             entries;
        uint64_t                        pipeLatency = 0;

        IqPipe(VectorLiteUOPIQ* topVal, uint32_t pipeIdVal, uint32_t depthVal, uint32_t stallMinEntryNumVal);
        ~IqPipe();

        void Work();

        bool CheckStall(uint32_t pendingCnt) const;
        bool Alloc(const VectorLiteUopT &uop);
        std::deque<IqEntry>& Entries();
        void Flush(const FlushBus &bus);
    };
    using IqPipeT = std::shared_ptr<IqPipe>;

    VectorLiteUOPIQ();
    ~VectorLiteUOPIQ();
    VectorLite*                         top = nullptr;
    void                                Xfer() const;
    void                                Work();
    void                                Build();
    bool                                CheckStall(const VectorLiteUopT &uop) const;
    SimSys*                             GetSim();
    void                                SetSim(SimSys *sim);

    void                                SetFlush(const FlushBus &bus);
    bool                                IsIdle();
    void                                Stats() const;

    INPUT SimQueue<VectorLiteUopT>      *m_siqUiqInQ { nullptr };
    OUTPUT SimQueue<VectorLiteUopT>     *m_uiqEXEInQ { nullptr };

private:
    SimSys*                             m_sim { nullptr };
    uint32_t                            m_reqLimit { 0 };
    uint32_t                            m_iqDepth { 0 };
    uint32_t                            m_pipeNum { 0 };
    std::map<VectorLiteUopInfo::ExePipe, uint32_t> m_pipeId;

    std::vector<IqPipeT>                m_pipes;
    // Scoreboard相关成员
    uint32_t m_minTColSumIssueInterval { 3 };
    uint64_t m_lastTColSumIssueCycle { 0 };  // 上次发射TCOLSUM的周期. 初始化为0，表示没有发射过，使得第一个TCOLSUM可以立即发射
    bool m_tcolsumPending { false };         // 是否有TCOLSUM在等待发射

    void                                ReceivedUInfo();
    bool                                AllocEntry(const VectorLiteUopT &uop);
    void                                Issue();
};
} // namespace JCore

#endif
