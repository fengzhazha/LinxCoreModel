#ifndef INST_TRACER
#define INST_TRACER

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <iostream>
#include <vector>
#include <set>
#include <string>
#include <map>
#include "ModelCommon/CycleInfo.h"
#include "ModelSpec.h"
#include "ModelCommon/ROBID.h"
#include "ModelCommon/BlockCommand.h"

namespace JCore {

class Core;

struct SimInstInfo;
using SimInst = std::shared_ptr<SimInstInfo>;

enum class BlockEvent {
    ALLOC = 0,
    BROB_ISSUE,
    MISPRED,
    COMMIT,
    UNDEF = 0xff
};

enum class InstrEvent {
    MISPRED = 0,
    COMMIT,
    SPLIT,
    FIFO,
    REQ,
    ALLOC,
    READY,
    DONE,
    UNDEF = 0xff
};

enum class MemEvent {
    RECV = 0,
    DONE,
    UNDEF = 0xff
};

struct BlockDumpInfo {
    BlockCommandPtr blockCmd {nullptr};
    uint64_t allocCycle { 0 };
    uint64_t brobIssueCycle { 0 }; // issue from BROB
    uint64_t flushCycle { 0 };
    uint64_t commitCycle { 0 };

    BlockDumpInfo() {}
    explicit BlockDumpInfo(BlockCommandPtr cmd) : blockCmd(cmd) {}
    ~BlockDumpInfo() {}

    BlockDumpInfo& operator|=(const BlockDumpInfo& other);
    friend std::ostream& operator<<(std::ostream& os, const BlockDumpInfo& obj);
};
using PBlockDumpInfo = std::shared_ptr<BlockDumpInfo>;

struct InstDumpInfo {
    SimInst inst;
    std::string peName;
    uint64_t flushCycle { 0 };
    uint64_t commitCycle { 0 };

    explicit InstDumpInfo(const SimInst& ins) : inst(ins) {}
    ~InstDumpInfo() {}

    friend std::ostream& operator<<(std::ostream& os, const InstDumpInfo& obj);
};
using PInstDumpInfo = std::shared_ptr<InstDumpInfo>;

class CubeUop;
using PCubeUop = std::shared_ptr<CubeUop>;

struct CubeInstDumpInfo {
    PCubeUop inst;
    std::string peName;
    uint64_t splitCycle { 0 };
    uint64_t fifoCycle { 0 };
    uint64_t reqCycle { 0 };
    uint64_t readyCycle { 0 };
    uint64_t doneCycle { 0 };

    explicit CubeInstDumpInfo(const PCubeUop& ins) : inst(ins) {}
    ~CubeInstDumpInfo() {}

    friend std::ostream& operator<<(std::ostream& os, const CubeInstDumpInfo& obj);
};
using PCubeInstDumpInfo = std::shared_ptr<CubeInstDumpInfo>;

struct MemDumpInfo {
    SimInst inst;
    bool isLoad { false };
    uint64_t recvCycle { 0 };
    uint64_t doneCycle { 0 };

    explicit MemDumpInfo(const SimInst& ins) : inst(ins) {}
    ~MemDumpInfo() {}
    friend std::ostream& operator<<(std::ostream& os, const MemDumpInfo& obj);
};
using PMemDumpInfo = std::shared_ptr<MemDumpInfo>;

class InstTracer {
public:
    InstTracer(SimSys* sim, Core* core, std::ostream* stream);
    ~InstTracer();

    void SetFunctionalState(bool enable);
    bool IsEnabled() const;

    void Build() const;
    void Xfer();
    void Reset() const;

    void Push(ROBID bid, const PBlockDumpInfo& info);
    void Push(ROBID bid, const PInstDumpInfo& info);
    void Push(ROBID bid, const PCubeInstDumpInfo& info);
    void Push(uint64_t instUid, const PMemDumpInfo& info);

    void PushBlockEvent(ROBID bid, BlockEvent event);
    void PushInstrEvent(ROBID bid, const SimInst& inst, InstrEvent event);
    void PushInstrEvent(ROBID bid, const PCubeUop& inst, InstrEvent event);
    void PushMemEvent(uint64_t instUid, MemEvent event) const;

    SimSys* GetSim();
    static uint64_t GetNextUid();

protected:
    SimSys *m_sim { nullptr };
    Core* m_core { nullptr };
    std::ostream* m_stream { nullptr };
    bool m_enable { false };

    std::set<ROBID> m_completedBlocks;

    std::map<ROBID, PBlockDumpInfo> m_blockInfo;
    std::map<ROBID, std::vector<PInstDumpInfo>> m_instInfo;
    std::map<ROBID, std::vector<PCubeInstDumpInfo>> m_cubeInstInfo;
    std::map<uint64_t, std::vector<PMemDumpInfo>> m_memInfo;

    static std::atomic<uint64_t> nextUid;
};
using PInstTracer = std::shared_ptr<InstTracer>;

uint64_t GetInstNextUid();

} // namespace JCore
#endif // INST_TRACER
