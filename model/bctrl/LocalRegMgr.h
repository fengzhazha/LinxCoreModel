#pragma once

#include <list>
#include <vector>

#include "core/Bus.h"
#define MAX_RELEASE_DISTENCE 4

namespace JCore {

class SimSys;

struct RegSeg {
    uint32_t segTag = 0;
    uint32_t segSize = 0;
};

struct RegStatus {
    uint32_t startTag = 0;
    // if any seg is true, then is busy, set free to false
    bool free = true;
    // 写入的时候使用
    uint32_t freePtr = 0;
    uint32_t usePtr = 0;
    std::vector<RegSeg> useSeg;
    std::vector<RegSeg> freeSeg;
};

struct RecordInfo {
    // rf 中的 索引
    uint64_t tag = 0;
    uint64_t addr = 0;
    uint32_t size = 1;
    ROBID prev;
    ROBID next;
    bool retired = false;
    bool vld = false;
    bool ready = false;
    bool idxVld = true;
    bool kill = false;
    bool markKill = false;
    OperandType type = OperandType::OPD_INVALID;
    // 在 freelist 中的位置
    uint32_t freePtr = 0;
    ROBID bid;
    ROBID rid;
    ROBID gid;
    // used for kill
    ROBID seq;
};

std::ostream &operator<<(std::ostream &os, const RecordInfo &info);

class LocalRegMgr {
private:
    /* \brief the size of physical reg */
    uint32_t                pSize = 0;
    /* \brief the allocated size of physical reg */
    std::vector<uint32_t>        usedPSize = std::vector<uint32_t>(1, 0);
    /* \brief the allocated size of mapQ */
    std::vector<uint32_t>        usedEntrySize = std::vector<uint32_t>(1, 0);
    /* \brief max size of mapQ */
    uint32_t                mapSize = 0;
   /* \brief the point to next alloc reg */
    /* \brief the point to next alloc reg */
    // 下一个 vttag(直接对应 rf 的索引)
    /* \brief the sequence of next alloc mapQ entry pointer */
    std::vector<ROBID>           mapQAllocPtr = std::vector<ROBID>(1, ROBID()); // tail->next
    /* \brief the sequence of dellocated mapQ entry pointer */
    std::vector<ROBID>           mapQDeallocPtr = std::vector<ROBID>(1, ROBID()); // head
    ROBID                   tail = ROBID();
   /* \brief the sequence of rename phsical pointer */
    std::vector<uint32_t>        allocPtr = std::vector<uint32_t>(1, 0);
    std::vector<uint32_t>        deallocPtr = std::vector<uint32_t>(1, 0);
    std::vector<uint32_t>        tailPtr = std::vector<uint32_t>(1, 0);
    /* \brief the map for rob-Seqition/reg-ptr */
    // tmap
    // use a link list to simulate the next/prev entry
    // for now, TODO: change to vector and use the prev and next se
    std::vector<RecordInfo> mapQ = std::vector<RecordInfo>(1, RecordInfo());
    std::vector<RegStatus>  regStatus;
    /* \brief T/U/M/N(debug) */
    OperandType            type = OperandType::OPD_TILE_TLINK;
    /* \brief Link the PE */
    uint32_t                peid = 0;
    constexpr static uint64_t NEED_TWO_REG = 64;
    uint32_t                tid = 0;
    // FIXME: Pred alloc size should not be 64, sth is wrong when rename.
    constexpr static uint64_t zeroPredSize = 64;

public:
    SimSys                  *sim;
    void                    Init(uint32_t gprCnt, uint32_t robSize, OperandType typ, uint32_t pe, uint32_t regWidth);
    void                    Init(uint32_t gprCnt, uint32_t pCnt, OperandType typ, uint32_t regWidth = 0);
    void                    InitPred(uint32_t gprCnt, uint32_t robSize, OperandType typ, uint32_t pe, uint32_t regWidth);
    void                    InitFreeList(uint64_t gprCnt, uint32_t regWidth);
    void                    InitSegment(uint32_t robSize);
    bool                    IsTileR();
    bool                    IsVecR();
    RecordInfo              Alloc(ROBID bid, uint32_t size, OperandType tType);
    uint32_t                Alloc(ROBID bid, ROBID rid, uint32_t size);
    uint32_t                Alloc(ROBID bid, ROBID rid, ROBID gid, uint32_t size);
    uint32_t                AllocVec(ROBID bid, ROBID rid, ROBID gid, uint32_t size);
    void                    UnsetRdy(uint32_t tag, ExecEngineTyp iexTyp);
    void                    Free(uint32_t eid);
    void                    ReportRetired(uint32_t seq, bool isDealloc);
    RecordInfo              Dispatch(int32_t offset, OperandType tType = OperandType::OPD_INVALID);
    void                    MarkNeedKill(int32_t offset);
    RecordInfo              DispatchVec(int32_t offset);
    RecordInfo              DispatchInVec(ROBID bid, ROBID gid, int32_t offset);
    RecordInfo              DispatchPred(ROBID bid, ROBID gid, int32_t offset,
                                         OperandType tTypeS = OperandType::OPD_INVALID);
    ROBID                   GetNextSeq();
    ROBID                   GetCurSeq();
    void                    SetCurSeq(ROBID &seq);
    bool                    CheckStall(uint32_t size, OperandType tType = OperandType::OPD_INVALID);
    void                    Check(uint32_t size, OperandType tType);
    void                    ReportMaskVLd(ROBID &seq, ROBID bid, ROBID rid);
    void                    flush(FlushBus &flushBus);
    void                    FlushVec(FlushBus &flushBus);
    void                    FlushTile(FlushBus &flushBus);
    void                    ReportKill(uint32_t seq);
    uint32_t                FindValidTag(uint32_t size, uint32_t &freeIdx);
    uint32_t                FindValidTwoTag(uint32_t size, uint32_t &freeIdx);
    void                    EarlyRelease();
    void                    InsertFree(uint32_t tag, uint32_t size, uint32_t freeIdx = 0);
    void                    InsertFreeVec(uint32_t tag, uint32_t size, uint32_t freeIdx);
    void                    InsertFreeVecTwoReg(uint32_t tag, uint32_t freeIdx);
    uint32_t                GetCount() {return pSize; };
    OperandType             GetType() {return type;};
    uint32_t                StoreToSRAM(RecordInfo info);
    ROBID                   GetNextFreeEntry();
    bool                    CheckLegal();
    void                    ReportBlockCommit(ROBID bid);
    void                    ReportBlockCommitVec(ROBID bid);
    void                    ReportGroupCommit(ROBID bid, ROBID gid);
    void                    MergeFreeList(std::vector<RegSeg>& seg);
    bool                    VecRegEnoughFor(uint32_t size);
    ROBID                   GetPrevROBID(ROBID& seq);
    void                    PrintInfo();
    void                    PrintFreeList();
    uint32_t                GetReleaseDistence();
    inline SimSys*          GetSim() { return sim; };
    void                    SetTid(uint32_t tid) { this->tid = tid; };
    void                    IncAllocPtr(OperandType tType);
    ROBID                   GetAllocEntryID(OperandType tType);
    uint32_t                GetMapGroupID(uint32_t eid);
    uint64_t                GetTileRenSize(OperandType tType);
    uint64_t                GetTileMapQSize(OperandType tType);
};
} // namespace JCore
