#ifndef  BLOCKISA_MODEL_BISSUE_H
#define  BLOCKISA_MODEL_BISSUE_H

#pragma once

#include <array>
#include <deque>
#include <set>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <utility>

#include "core/Bus.h"
#include "ModelCommon/SimInstInfo.h"
#include "interface/InterfaceCommon.h"
#include "DataStruct/MachineId.h"
#include "ModelCommon/ROBID.h"

namespace JCore {

class BCtrlUnit;
class BlockIssueQueueUnit;

constexpr size_t RELATIVE_DISTANCE = 16;

class CubeCredit {
public:
    uint32_t peId = 0;
    uint64_t issuedCubeCmd = 0;
};
class CHandIDAllocator {
public:
    // key:accChainId, value:{startId, endId}
    std::map<uint64_t, std::map<uint32_t, std::map<uint64_t, CHandAllocInfoPtr>>> allocated;
    // key:accChainId, value:{startId, endId}
    std::map<uint64_t, std::map<uint32_t, std::map<uint64_t, CHandAllocInfoPtr>>> toFree;
    std::map<uint64_t, std::map<uint32_t, std::map<uint64_t, uint64_t>>> chainAllocEntryNum;
    std::map<uint32_t, std::map<uint64_t, uint64_t>> totalSize;
    std::map<uint32_t, std::map<uint64_t, uint64_t>> usingSize;
    std::map<uint32_t, std::map<uint64_t, uint64_t>> allocPtr;
    std::map<uint32_t, std::map<uint64_t, uint64_t>> deallocPtr;
    std::vector<CubeCredit> cubeCreditVec;
    std::map<uint64_t, uint64_t> accChainSizeMap;

    explicit CHandIDAllocator() = default;
    ~CHandIDAllocator() = default;
    bool Alloc(uint64_t num, uint64_t accChainId, CHandAllocInfoPtr& info, uint64_t peId, uint32_t stid);
    bool TryDealloc(CHandAllocInfoPtr info, uint64_t peId, uint32_t stid);
    void Free(CHandAllocInfoPtr info, uint64_t peId, uint32_t stid);
    CHandAllocInfoPtr LookUp(uint64_t accChainId, uint64_t peId, uint32_t stid);
    bool Full(uint64_t num);
    void Build(uint32_t scalarTreads) const
    {
        (void)scalarTreads;
    }
};

using CAllocatorPtr = std::shared_ptr<CHandIDAllocator>;

class TileStatus {
public:
    bool ready = false;
    bool canDealloc = false;
    bool allocated = false;
    bool isZero = false;
    OperandType handType = OperandType::OPD_INVALID;
    uint64_t tag = -1U;
    uint64_t size = -1U;
    uint64_t addr = -1U;

    TileInfoPtr tileInfo = nullptr;

    BlockCommandPtr producerCmd = nullptr;
    TileOperandPtr producer = nullptr;
    explicit TileStatus() = default;
    explicit TileStatus(OperandType hType, uint64_t t) : handType(hType), tag(t) {}
    void Reset();
    std::string Dump() const
    {
        std::stringstream oss;
        oss << GetTileTypeStr(handType) << " tag:" << std::dec << tag << ", alloc:" << allocated << ", dealloc:"
            << canDealloc << ", rdy" << ready << ", size:" << size << ", addr:0x" << std::hex << addr;
        if (producerCmd != nullptr) {
            oss << " Cmd:" << producerCmd->Dump();
        }
        return oss.str();
    }
};

class TileStatusGroup {
public:
    BlockIssueQueueUnit* top;
    std::vector<TileStatus> entry;
    uint64_t entryNum = 0;
    uint64_t usingTagNum = 0;
    OperandType handType = OperandType::OPD_INVALID;
    uint64_t segStartAddr = 0;
    uint64_t segEndAddr = 0;
    uint64_t currAllocAddr = 0;
    uint64_t currSize = 0;
    uint64_t threshold = 0; // size threshold; addr alloc range: [segStartAddr,(segStartAddr + threshold))
    uint64_t deallocPtr = 0; // dealloc entry index
    uint64_t retiredPtr = 0; // retire entry index
    uint64_t lastAllocPtr = 0;
    uint64_t allocPtr = 0;
    std::deque<uint64_t> retiredTagQ;
    uint32_t relativeDistance = RELATIVE_DISTANCE;
    explicit TileStatusGroup() = default;
    explicit TileStatusGroup(OperandType hType, uint64_t depth, uint64_t segAddr, uint64_t thres, uint32_t distance)
        : handType(hType), segStartAddr(segAddr), threshold(thres), relativeDistance(distance)
    {
        currAllocAddr = segAddr;
        segEndAddr = segAddr + thres;
        entryNum = depth;
        for (uint64_t i = 0; i < depth; i++) {
            entry.push_back(TileStatus(hType, i));
        }
    }
    bool TryAllocateTileAddr(uint64_t size);
    bool AllocateTileAddr(uint64_t &tag, uint64_t size, bool isZero);
    bool AllocateZeroTileAddr(uint64_t &tag);
    TileStatus& GetTileStatus(uint64_t tag);
    void Wakeup(TileOperandPtr tileOpd);
    void TryDealloc();
    uint64_t GetProducerTag(uint32_t link);
    void CheckRelativeIndex();
    void ReportTileNotReuse(TileOperandPtr opd);
    void ReportDstRetired(TileOperandPtr opd);
    void GatherStats();
    void FlushTileAddr(FlushBus &flushReq);
};

class SharedFreeListStatus {
public:
    bool free = true;
    bool ready = false;
    uint32_t producerCnt = 0;
    uint32_t occupiedSize = 1;
    SharedFreeListStatus() {}

    void Reset();
    std::string Dump() const;
};

struct TcopyTagInfo {
    bool vld = false;
    OperandType handType = OperandType::OPD_INVALID;
    uint32_t tag = 0;
};

class SharedMapQStatus {
public:
    bool vld = false;
    bool kill = false;
    bool retired = false;
    bool ready = false;
    bool isZero = false;
    uint64_t addr = 0;
    uint64_t size = 0;
    uint32_t vtag = 0;
    uint32_t ptag = 0;
    uint32_t occupiedTagNum = 0;
    OperandType handType = OperandType::OPD_INVALID;
    TileOperandPtr producer = nullptr;
    BlockCommandPtr producerCmd = nullptr;
    // tcopy dst tag recorded in producer
    std::deque<TcopyTagInfo> moveInfoQ;
    // tcopy src tag recorded in tcopy
    TcopyTagInfo copyInfo = TcopyTagInfo();

    bool lastReuse = true;
    uint32_t usingConsumCnt = 0;
    ROBID younerestConsumID = ROBID();
    std::unordered_map<ROBID, bool, ROBIDKeyHash> consumList;
    TileInfoPtr tileInfo = nullptr;

    bool PreRelease(ROBID nonFlushPtr);

    SharedMapQStatus() {}

    void Reset();
    std::string Dump() const;
};

class PartitionMapQ {
public:
    std::vector<SharedMapQStatus> entry;
    uint32_t allocPtr = 0;
    void Build(uint32_t size);

    PartitionMapQ() {}
};

class SharedTileStatus {
public:
    BlockIssueQueueUnit* top;
    uint64_t elementSize = 0;

    std::vector<SharedFreeListStatus> freeListEntry;
    std::map<OperandType, uint32_t> partitionSPtr;
    std::map<OperandType, uint32_t> partitionEPtr;
    std::map<OperandType, uint32_t> maxFreeTagNum;
    std::map<OperandType, uint32_t> freeTagNum;

    std::map<OperandType, std::vector<SharedMapQStatus>> mapQEntry;
    std::map<OperandType, uint32_t> freeMapQEntryNum;
    std::map<OperandType, uint32_t> deallocPtr;
    std::map<OperandType, uint32_t> retirePtr;
    std::map<OperandType, uint32_t> lastAllocPtr;
    std::map<OperandType, uint32_t> allocPtr;

    std::map<OperandType, std::deque<uint64_t>> retiredTagQMap;

    // statistics
    std::map<OperandType, uint64_t> occupiedSize;

    SharedTileStatus() {}

    void Reset();
    void BuildCapacity(uint64_t totalSize, uint64_t elem);
    void BuildTilePartion(std::map<OperandType, uint32_t> tileHandlePerMap, uint32_t mapQSize);
    void Wakeup(OperandType handType, uint32_t tag);
    bool CheckStall(OperandType handType, uint32_t size);
    bool Allocate(OperandType handType, uint64_t &vtag, uint32_t size, bool isZero);
    uint32_t GetFreeTileVtag(OperandType handType);
    bool AllocateZero(OperandType handType, uint64_t &vtag, uint32_t size);
    bool MoveTileTag(POperandPtr const& src, POperandPtr const& dst);
    uint32_t AllocateFreeEntry(OperandType handType, uint32_t size);
    void OccupyMultiFreeEntry(uint32_t tag, uint32_t size);
    void OccupySingleFreeEntry(uint32_t tag);
    void IncFreeEntryDstUseCnt(uint32_t tag, uint32_t size);
    void FreeMultiFreeEntry(uint32_t tag, uint32_t size);
    void FreeSingleFreeEntry(uint32_t tag);
    void ReportKill(TileOperandPtr src);
    void ReportRetire(TileOperandPtr dst);
    void FreeMapQEntry(OperandType handType, uint32_t vtag);
    void Flush(FlushBus &flushReq);
    bool CheckPartitionFree(OperandType handType, uint32_t needTagNum);
    bool CheckSharedFree(uint32_t needTagNum);
    uint32_t AllocatePartionTag(OperandType handType, uint32_t needTagNum);
    uint32_t AllocateSharedTag(uint32_t needTagNum);
    uint32_t GetProducerTag(OperandType handType, uint32_t link);
    void PrevTileRetire(ROBID nonFlushPtr);
    void FlushConsumerInfo(FlushBus &flushReq, OperandType handType, uint32_t ptr);
    void GatherStats();
    void Dump();
    uint64_t GetTotal() const;
};

struct MCallRunningStatus {
    bool vld = false;
    bool stall = false;
    BlockCommandPtr cmd = nullptr;
    uint32_t totalThread = 0;
    uint32_t renameGidPtr = 0;
    uint32_t retiredGroupCnt = 0;
    void Reset()
    {
        vld = false;
        stall = false;
        cmd = nullptr;
        totalThread = 0;
        renameGidPtr = 0;
        retiredGroupCnt = 0;
    }
};

struct MCallFallStatus {
    MCallRunningStatus curMCall = MCallRunningStatus();
    uint32_t maxGroupSchedulerCreit = 0;
    uint32_t groupSchedulerCreit = 0;
    void Reset()
    {
        curMCall.Reset();
        groupSchedulerCreit = maxGroupSchedulerCreit;
    }
    bool Stall() const
    {
        return groupSchedulerCreit == 0;
    }
};

class BIssueQ {
public:
    BCtrlUnit* top;
    BIQType type;
    std::vector<BlockCommandPtr> entry;
    std::vector<std::unordered_map<uint32_t, BlockCommandPtr>> blockMap;
    uint64_t size = 0;

    std::unordered_set<uint32_t> accChainIssue;

    explicit BIssueQ() = default;
    explicit BIssueQ(BIQType t, uint64_t depth, uint32_t scalarThreads);
    ~BIssueQ() = default;
    void Wakeup(BlockCommandPtr cmd, TileOperandPtr tileOpd);
    void FlagDoubleOutput(TileOperandPtr tileOpd);
    bool GetCHandId(BlockCommandPtr cmd);
    bool GetCHandIdAlloc(BlockCommandPtr cmd);
    bool GetCHandIdDep(BlockCommandPtr cmd);
    BlockCommandPtr CubePick();
    BlockCommandPtr MCallPick();
    BlockCommandPtr Pick();
    void Release(ROBID bid, uint32_t stid);
    uint64_t Size();
    bool Full();
    void Insert(BlockCommandPtr blockCmd);
    BlockCommandPtr Get(ROBID bid, uint32_t stid);
    void Flush(FlushBus &flushReq);

    std::unordered_map<uint64_t, bool> accChainIdFilter;
    void SetAccChainFilter(uint64_t accChainThres);
    CAllocatorPtr cHandIDAlloc = nullptr; // CUBE_IQ
};

class BlockIssueQueueUnit : public SimObj {
public:
    BCtrlUnit                                   *top;

    /* \brief Block requests array from BCC to other modoules */
    OUTPUT std::vector<SimQueue<BCCCubeFlush>*>      m_bccCubeFlushArr;
    OUTPUT std::vector<SimQueue<BCCTTransFlush>*>    m_bccTransFlushArr;
    OUTPUT std::vector<SimQueue<BCCVecFlush>*>       m_bccVecFlushArr;
    OUTPUT std::vector<SimQueue<BCCVecFlush>*>       m_bCCVecliteFlushArray;
    OUTPUT std::vector<SimQueue<BCCVecFlush>*>       m_bccMtcFlushArr;
    OUTPUT std::unordered_map<BIQType, std::vector<SimQueue<BlockCommandPtr>*>> blockDispatchQ;
    // OUTPUT std::unordered_map<BIQType, std::vector<std::vector<SimQueue<BlockCommandPtr>*>>> blockDispatchQ;
    OUTPUT SimQueue<TileOperandPtr> *stashFreeQ;
    OUTPUT SimQueue<TileOperandPtr> *stashRetireQ;
    OUTPUT SimQueue<BlockCommandPtr> *schedulerBCCRslvQ;
    INPUT std::unordered_map<BIQType, SimQueue<BlockCommandPtr>*> blockWakeupQ;
    INPUT SimQueue<SimInst>                       *cmdIQBISQ;
    BlockIssueQueueUnit() {}
    ~BlockIssueQueueUnit() {}

    void                                Work();
    void                                Xfer();
    void                                Reset();
    void                                Build();
    SimSys                              *GetSim();
    void ReportStat() override {}
    /* \brief Load/Store window slides */
    void                                WindowSlides(uint64_t distance, BIQType iq, bool isLoad);
    void                                Flush(FlushBus &flushReq);

    uint64_t                            lastBlockIdToCube = 0;
    std::vector<uint64_t>               accPtr; // For add acc dependency
    std::vector<uint64_t>               accChainId;

    std::vector<BIssueQ> bIssueQ;

    std::map<OperandType, TileStatusGroup> tileStatus;
    std::vector<SharedTileStatus> sharedTileStatus;
    std::map<OperandType, uint64_t> tileSwimIdMap = {
        {OperandType::OPD_TILE_TLINK, 100},
        {OperandType::OPD_TILE_ULINK, 101},
        {OperandType::OPD_TILE_MLINK, 102},
        {OperandType::OPD_TILE_NLINK, 103},
        {OperandType::OPD_TILE_STACK, 104},
        {OperandType::OPD_TILE_MCALL_GPR, 105},
        {OperandType::OPD_TILE_MCALL_STK, 106},
    };
    uint64_t totalTileSwimId = 107;
    bool lastTileRegFull = false;

    MCallFallStatus mcallStatus;

    bool CheckBlockCmdStall(BIQType biqType, SimInst inst);
    bool CheckBIQStall(BIQType biqType, SimInst inst);
    bool CheckTileRegStall(SimInst inst);
    bool GetTileRegInfo(BlockCommandPtr cmd, SimInst inst);
    void AllocACCDep(BlockCommandPtr cmd);
    bool BIQFull(SimInst inst);
    void FlagDoubleOutput(std::vector<TileOperandPtr> &srcs);
    void InsertBlockCmd();
    uint32_t GetCoreNum() const;
    void DispatchBlockCmd();
    void WakeupBIssueQEntry(BlockCommandPtr cmd);
    void WakeupBIssueQEntry(TileOperandPtr dst, uint32_t stid, BlockCommandPtr cmd = nullptr);
    void WakeupBlockCmd();
    void GatherStats(uint32_t stid);
    void WakeupMoveDependency(SharedMapQStatus e, uint32_t stid);
    void WakeupTile(TileOperandPtr dst, uint32_t stid);
    void ReportBlockRetired(BlockCommandPtr cmd);
    bool RenameSingleTileDst(BlockCommandPtr cmd, SimInst inst, POperandPtr dst, uint32_t dstLocInBlock);
    void IncLastAllocPtr(uint32_t stid);
    void SetConsumerInfo(POperandPtr const& src, ROBID bid, uint32_t stid);
    void SetTileSrcRslv(BlockCommandPtr const& cmd);
    void PrevTileRelease(uint32_t stid);
    void MCallTileRelese(TileOperandPtr const& dst, uint32_t stid);
    void SetMCallStatus(SimInst const& inst);
    void ReleaseMCallStatus(ROBID bid);
    void RetireMCallGroup();
    BlockCommandPtr TryMCallAlloc(BlockCommandPtr const& cmd);
    void RollBackLCubeCredit(BlockCommandPtr cmd);
};

} // namespace JCore

#endif
