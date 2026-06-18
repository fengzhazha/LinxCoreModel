#pragma once

#include <deque>
#include <unordered_map>

#include "core/Bus.h"
#include "interface/InterfaceCommon.h"
#include "ModelSpec.h"
#include "SimSys.h"

namespace JCore {

enum class EvictMode {
    PER_CYCLE = 0,   // evict one entry per cycle
    FULL,            // evict only when full
    COUNT,
};

enum class Status {
    INVALID = 0,   // intial
    SHARED,        // has send write req to tilereg
    MODIFIED,
};

struct TOEntry {
    int id;
    bool isVgather = false;
    uint32_t groupSlotId = 0;
    bool valid;
    bool waitRsp;
    bool reissue;
    uint32_t addr;
    uint32_t tag;
    ROBID bid;
    ROBID gid;
    bool fromCTInst = false;
    Status status = Status::INVALID;
    uint32_t writeReqId;
    uint32_t allocCycle;
    uint32_t evictCycle;
    std::vector<uint8_t> data;
    DataMask mask = DataMask(1);
    uint32_t stid = -1U;

    explicit TOEntry(int id):id(id), valid(false), waitRsp(false), reissue(false), allocCycle(0){};
    void Reset()
    {
        isVgather = false;
        groupSlotId = 0;
        valid = false;
        waitRsp = false;
        reissue = false;
        addr = 0;
        tag = 0;
        bid = ROBID();
        gid = ROBID();
        fromCTInst = false;
        status = Status::INVALID;
        writeReqId = UINT32_MAX;
        allocCycle = 0;
        evictCycle = 0;
        data.clear();
        mask.Reset();
    }

    void SetStatus(bool flag)
    {
        status = flag ? Status::MODIFIED : Status::SHARED;
    }

    bool IsDirty() const
    {
        return (valid && status == Status::MODIFIED);
    }

    bool IsNeedEvict() const
    {
        return (valid && status == Status::MODIFIED && !waitRsp);
    }

    // 是否同地址
    bool IsSameEntry(uint32_t check_addr) const
    {
        return (addr == check_addr);
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

class TOWayInfo {
public:
    TOWayInfo(uint32_t bidVal, uint32_t stidVal, unsigned way, uint32_t size, uint32_t address)
        : bid(bidVal), stid(stidVal), way(way), size(size), address(address), endAddress(address + size)
    {
        if (size > 65536) {
            ASSERT(0 && "Warning: size should <= 64KB");
        }
        if (address > std::numeric_limits<uint32_t>::max() - size) {
            ASSERT(0 && "wrong address range");
        }
    }

    bool FindWayByAddress(uint32_t targetAddress, unsigned* resultWay = nullptr) const
    {
        // availAddress范围：address <= availAddress < address + size
        if (targetAddress >= address && targetAddress < endAddress) {
            if (resultWay) {
                *resultWay = way;
            }
            return true;
        }
        return false;
    }

    bool FindWayByAddress(uint32_t targetAddress, unsigned& resultWay) const
    {
        if (targetAddress >= address && targetAddress < endAddress) {
            resultWay = way;
            return true;
        }
        return false;
    }

    std::pair<bool, unsigned> FindWay(uint32_t targetAddress) const
    {
        if (targetAddress >= address && targetAddress < endAddress) {
            return {true, way};
        }
        return {false, 0};
    }

    uint32_t GetBid() const { return bid; }
    uint32_t GetStid() const { return stid; }
    unsigned GetWay() const { return way; }
    uint32_t GetSize() const { return size; }
    uint32_t GetAddress() const { return address; }
    uint32_t GetEndAddress() const { return endAddress; }
    bool IsInRange(uint32_t targetAddress) const
    {
        return targetAddress >= address && targetAddress < endAddress;
    }

    void DumpInfo() const
    {
        std::cout << "BID: " << bid
                  << ", Way: " << way
                  << ", Size: " << size
                  << ", Address: 0x" << std::hex << address << std::dec
                  << ", Range: [0x" << std::hex << address << ", 0x"
                  << endAddress << ")" << std::dec << std::endl;
    }
private:
    uint32_t bid;
    uint32_t stid = -1U;
    unsigned way;
    uint32_t size;
    uint32_t address;
    uint32_t endAddress;
};

class VectorCoreTA;
class VectorCore;

class VecSCB : public SimObj {
public:
    VecSCB();
    ~VecSCB();
    void ReportStat() override {}

    std::vector<VecTileRegStReq> vecStReqQ;

    OUTPUT SimQueue<VecTileRegStReq>        *SCBTileRegStReqQ;
    OUTPUT SimQueue<TileRegVecLdRes>        *SCBvecWrResQ;
    std::vector<bool> isInDrainMode;

    int FindFreeEntry();
    bool IsFull(bool updatePMU = true) const;
    bool IsEmpty(ROBID bid, uint32_t stid) const;
    bool IsWayEmpty(uint32_t way) const;
    bool IsEmpty() const;
    bool CannotMerge(uint32_t addr);
    unsigned GetOcc() const;
    int SelectEvictEntry() const;
    void Evict();
    void DrainAllEntry(uint32_t stid);
    void DrainOutCacheEntry(ROBID bid, uint32_t stid);
    void DrainEntry(ROBID &bid, uint32_t stid);
    int FindEntryByAddress(uint32_t aligned_addr);
    void HandleStore();
    void HandleTileRegRsp(uint32_t reqId);
    void HandleTileRspTO(uint32_t reqId);
    void MergeEntry(int entryIdx, uint32_t addr, VecTileRegStReq& req, uint8_t* data, DataMask reqMask);
    void AllocateEntry(uint32_t addr, VecTileRegStReq& req, uint8_t* data, DataMask reqMask, uint32_t stid);
    std::vector<uint32_t>  GetAvailWay(ROBID bid, uint32_t stid) const;
    void WriteSCB(VecTileRegStReq& req);
    void DecodeAddress(uint32_t address, uint32_t& tag, uint32_t& index) const;
    void WriteOutCache(VecTileRegStReq& req);
    void MergeOutCache(TOEntry& entry, uint32_t addr, VecTileRegStReq& req, uint8_t* data, DataMask reqMask);
    void AllocOutCache(TOEntry& entry, uint32_t addr, VecTileRegStReq& req, const ROBID &bid, uint8_t* data,
                       DataMask reqMask, uint32_t stid);
    void EvictOutCache();
    uint64_t GetTOWay(uint32_t address);
    void MergeTOData(uint32_t address, unsigned way, unsigned bytes, std::vector<uint8_t>& result);
    void SetTOWay(unsigned blockId, unsigned way);
    void RecvStashClear();
    void Work();
    void Xfer();
    void Reset();
    SimSys* GetSim();
    void SetSim(SimSys *sim);
    uint64_t GetTOWayByAddr(uint64_t addr, ROBID bid, uint32_t stid);
    void Flush(FlushBus &bus);

    void Build(VectorCoreTA *top, VectorCore *vec, uint32_t entry_num);
    void DumpScb();
    bool IsEvictPerCycle() const;
    void ClearTOWay(uint32_t bid, uint32_t stid);
    void ClearWay(ROBID bid, uint32_t stid)
    {
        std::vector<uint32_t> ways = GetAvailWay(bid, stid);
        for (auto way : ways) {
            activeOutWay.erase(way);
        }
        ClearTOWay(bid.val, stid);
    }

private:
    static constexpr uint64_t BLOCK_MASK = ~0xFFULL;
    static constexpr uint64_t OFFSET_MASK = 0xFF;
    uint32_t entryNum;
    std::vector<ROBID> drainBid;
    std::vector<std::map<ROBID, std::vector<uint32_t>>> drainWays;
    std::deque<TOEntry*> free_list;
    std::vector<TOEntry> Entries;
    std::vector<std::vector<TOEntry>> outputCache;
    VectorCoreTA* vecLsu;
    VectorCore *vtop;
    SimSys *m_sim;
    EvictMode evictMode = EvictMode::COUNT;
    uint64_t nToSets = 0;
    uint64_t nToWays = 0;
    std::map<uint64_t, bool> activeOutWay;
    std::map<unsigned, unsigned> wayMapByBcc_;
    std::vector<TOWayInfo> toWays;
};

} // namespace JCore