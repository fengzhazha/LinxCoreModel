#pragma once

#include <deque>
#include <unordered_map>

#include "core/Bus.h"
#include "interface/InterfaceCommon.h"
#include "ModelSpec.h"
#include "SimSys.h"

namespace JCore {

struct TileSCBEntry {
    int id;
    bool valid;
    bool waitRsp;
    uint64_t addr;
    ROBID bid;
    uint32_t writeReqId;
    uint32_t allocCycle;
    uint32_t evictCycle;
    bool isFromLastInst = false;
    std::vector<uint8_t> data;
    DataMask mask = DataMask(1);

    explicit TileSCBEntry(int id):id(id), valid(false), waitRsp(false), allocCycle(0){};
    void Reset()
    {
        valid = false;
        waitRsp = false;
        addr = 0;
        bid = ROBID();
        writeReqId = UINT32_MAX;
        allocCycle = 0;
        evictCycle = 0;
        isFromLastInst = false;
        data.clear();
        mask.Reset();
    }
    // 是否同地址
    bool IsSameEntry(uint32_t check_addr) const
    {
        return (addr == check_addr);
    }

    bool IsFromLastInst() const
    {
        return isFromLastInst;
    }

    void SetFromLastInst()
    {
        isFromLastInst = true;
    }

    void MergeData(uint8_t* merge_data, DataMask req_mask)
    {
        mask.Merge(req_mask);
        for (size_t i = 0; i < MAX_TILE_DATA_BYTE; i++) {
            if (req_mask.IsValid(i) && i < data.size()) {
                data[i] = merge_data[i];
            }
        }
    }

    bool IsFullyCovered() const
    {
        return mask.IsFull();
    }

    void Print() const
    {
        std::cout << "Entry " << id << ": valid=" << valid
                  << ", addr=0x" << std::hex << addr
                  << ", cycle=" << std::dec << allocCycle
                  << ", fully_covered=" << IsFullyCovered() << std::endl;
        std::cout << "  Mask: ";
    }
};

class MemoryCoreTA;
class TileSCB : public SimObj {
public:
    TileSCB();
    ~TileSCB();
    void ReportStat() override {}

    std::vector<VecTileRegStReq> vecSCBStReqQ;

    OUTPUT SimQueue<VecTileRegStReq>        *scbStReqQ;
    OUTPUT SimQueue<TileRegVecLdRes>        *scbStResQ;
    OUTPUT std::unordered_set<uint32_t> scbCommitQ;
    bool isInDrainMode = false;

    int FindFreeEntry();
    bool IsFull() const;
    bool IsEmpty() const;
    bool CannotMerge(uint64_t addr);
    unsigned GetOcc() const;
    int SelectEvictEntry() const;
    void Evict();
    void DrainAllEntry();
    void DrainEntry(ROBID &bid);
    int FindEntryByAddress(uint64_t aligned_addr);
    void HandleStore();
    void HandleTileRegRsp(uint32_t reqId);
    void MergeEntry(int entryIdx, uint64_t addr, uint8_t* data, DataMask reqMask, bool isLast = false);
    void AllocateEntry(uint64_t addr, const ROBID &bid, uint8_t* data, DataMask reqMask, bool isLast = false);
    void Work();
    void Xfer();
    void Reset();
    SimSys* GetSim();
    void SetSim(SimSys *sim);
    void Build(MemoryCoreTA *top, uint32_t entry_num);
    void DumpScb();
    void ProcessTStqReq();
    bool IsHalfFull() const;
    bool IsDrainAllScbEntry(uint32_t bid, int id) const;
    bool IsLastInstGetRsp(uint32_t bid) const;
    void LastInstGetRsp(uint32_t bid);
    void RemoveFromRspSet(uint32_t bid);

private:
    static constexpr uint64_t BLOCK_MASK = ~0xFFULL;
    static constexpr uint64_t OFFSET_MASK = 0xFF;
    uint32_t entryNum;
    ROBID drainBid;
    std::deque<TileSCBEntry*> free_list;
    std::vector<TileSCBEntry> Entries;
    MemoryCoreTA* vecLsu;
    SimSys *m_sim;
    std::unordered_set<uint32_t> receiveLastRsp;
};

} // namespace JCore