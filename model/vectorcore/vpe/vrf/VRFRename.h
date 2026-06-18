#ifndef VRF_RENAME_H
#define VRF_RENAME_H

#include <vector>
#include <unordered_map>

#include "core/Bus.h"
#include "../common/Common.h"

namespace JCore {

class VecPEDecode;
class VectorCore;

constexpr uint32_t VRF_RELATIVE_DISTACE = 4;
constexpr uint32_t HAND_NUM = 4;
constexpr uint32_t MAX_REG_SIZE = 256; // 256B
const std::unordered_map<OperandType, uint32_t> REG_TYPE_MAP = {
    {OperandType::OPD_VTLINK, 0},
    {OperandType::OPD_VULINK, 1},
    {OperandType::OPD_VMLINK, 2},
    {OperandType::OPD_VNLINK, 3},
};

class SimSys;

struct VRFMapEntry {
    bool vld = false;
    bool pair = false;
    bool kill = false;
    bool markNeedKill = false;
    bool done = false;
    bool retired = false;
    bool recycled = false;
    OperandType rType = OperandType::OPD_VTLINK;
    uint32_t tag = 0;
    uint32_t tid = 0;
    uint32_t stid = 0;
    ROBID bid;
    ROBID rid;
    ROBID gid;
    bool CheckIDMatch(ROBID ibid, ROBID igid)
    {
        return (vld && ibid == bid && igid == gid);
    }
};

struct VRFPmuEntry {
    uint64_t allocCycle = 0;
    uint64_t deallocCycle = 0;
    uint64_t useTime = 0;
    uint64_t reuseTime = 0;
    bool vec = false;
    bool mtc = false;
    OperandType type = OperandType::OPD_VTLINK;
};

std::ostream& operator<<(std::ostream& os, VRFMapEntry const &e);

class VRFRename {
public:
    VRFRename();
    ~VRFRename();
    OUTPUT SimQueue<uint32_t> vrfRtableTagFreeQ;
    OUTPUT SimQueue<uint32_t> vrfRtableTagRetireQ;
    OUTPUT SimQueue<uint32_t> vrfMtcRtableTagFreeQ;
    OUTPUT SimQueue<uint32_t> vrfMtcRtableTagRetireQ;
    void Build(uint32_t threadNum, uint32_t mapSize, uint32_t pSize);
    bool Stall(const SimInst& inst, uint32_t tid, uint32_t stid, uint32_t size);
    bool MultiStall(const SimInst& inst, const std::vector<uint8_t>& vrfReqNum);
    bool MapQStall(uint32_t tid, uint32_t stid, uint32_t cnt = 1);
    VRFMapEntry Dispatch(uint32_t tid, uint32_t stid, const SimInst& inst, OperandType rType, uint32_t distance);
    void MarkNeedKill(uint32_t tid, uint32_t stid, OperandType rType, uint32_t distance);
    bool IsVecCore(uint32_t coreThread);
    bool FindReusablePtag(const POperandPtr& src, bool pair, uint32_t& tag);
    bool SrcPtagReusable(uint32_t tid, uint32_t stid, const POperandPtr& src, bool pair);
    void Alloc(VRFMapEntry& info, const SimInst& inst);
    void Retire(uint32_t tid, uint32_t stid, const SimInst& inst, uint32_t tag, bool recycled, bool end);
    bool Kill(uint32_t tid, uint32_t stid, const SimInst& inst, OperandType rType, uint32_t tag, bool recycled);
    bool PreRelease(uint32_t tid, uint32_t stid, const SimInst& inst, OperandType rType, uint32_t tag, bool recycled);
    void ReleaseWholeThread(uint32_t tid, uint32_t stid, const SimInst& inst);
    void Flush(FlushBus flushReq);
    void FreeEntry(uint32_t tid, uint32_t stid, uint32_t eid, const SimInst& inst);
    void FreeTag(uint32_t tid, uint32_t stid, uint32_t tag, const SimInst& inst);
    void InitVecPMU();
    void InitMtcPMU();
    uint64_t GetTagUsedSize()
    {
        return freeList.size() - freeSize;
    }
    uint64_t GetMapQSize()
    {
        uint64_t totalSize = 0;
        for (size_t tid = 0; tid < mapQ.size(); tid++) {
            for (uint32_t stid = 0; stid < mapQ[tid].size(); stid++) {
                totalSize += (mapQ[tid][stid].size() - mapFreeSize[tid][stid]);
            }
        }
        return totalSize;
    }
    VectorCore* top = nullptr;
    SimSys *sim = nullptr;
    VecPEDecode *vecPE = nullptr;
    // PEDecodeUnit *mtcPE = nullptr;

private:
    std::vector<std::vector<std::vector<VRFMapEntry>>> mapQ;
    std::vector<std::vector<uint32_t>> mapDeallocPtr;
    std::vector<std::vector<uint32_t>> mapAllocPtr;
    std::vector<std::vector<uint32_t>> mapFreeSize;
    std::vector<std::vector<std::vector<std::vector<VRFMapEntry>>>> smap;
    std::vector<std::vector<std::vector<uint32_t>>> smapPtr;
    std::vector<std::vector<std::vector<std::vector<VRFMapEntry>>>> cmap;
    std::vector<std::vector<std::vector<uint32_t>>> cmapPtr;
    std::vector<bool> freeList;
    std::vector<bool> recycleList;
    std::vector<uint32_t> regUsingCnt;
    std::vector<uint32_t> regReadCnt;
    std::vector<VRFPmuEntry> pmu;
    uint32_t freeSize = 0;
    uint32_t GetFreeTag();
    uint32_t GetFreePairTag();
    uint32_t GetEntryID(uint32_t tid, uint32_t stid, uint32_t tag);
    void RetiredEntry(uint32_t tid, uint32_t stid);
    void FreeCMapEntry(VRFMapEntry& e, const SimInst& inst, bool flush);
    void FlushSingleMap(uint32_t tid, uint32_t stid, FlushBus flushReq);
    void UpdatePEPmu(VRFPmuEntry& pmu);
    void UpdatePmuIf(const VRFMapEntry& e);
    SimSys* GetSim() { return sim; }
};

}

#endif
