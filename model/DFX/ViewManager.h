#pragma once

#ifndef VIEW_MANAGER
#define VIEW_MANAGER

#include <unordered_set>

#include "BlockPipeViewInfo.h"
#include "PipeViewOut.h"
#include "../configs/dfx_config.h"

namespace JCore {
class SimSys;
class ViewManager {
public:
    SimSys *sim;
    DFXConfig config;
    ViewManager() = default;
    ~ViewManager() = default;
    void InitViewManager(uint64_t bRobDepth);
    void AddBlockSrc(uint64_t curBlockId, uint64_t srcBlockId);
    /* Swimlane depency */
    std::vector<uint64_t>& GetBlockSrcVec(uint64_t blockId);
    void ClearBlockSrcVec(uint64_t blockId);

    /* pipe view */
    void ResetBlockDepInfo(uint64_t blockId);
    void ResetBlockPipeInfo(uint64_t blockId, uint64_t bpc);
    void FlushBlockInfo(uint64_t blockId, PipeViewFlushType fType);
    void RecordBlockCompleted(BlockPipeViewPtr blockInfo);
    void RecordMinst(InstPipeViewPtr instInfo);
    void InitBIQType(uint64_t blockId, BIQType biqType);
    void InitPARGroup(uint64_t blockId, uint64_t groupNum);
    void RecordPARGroupCompleted(GroupPipeViewPtr groupInfo);
    void RecordPARMinst(InstPipeViewPtr instInfo);

    void PrintPipeView(uint64_t width);
    std::string GetBID() const;
    void PrintBlockInfo(uint64_t blockId);
    void PrintBlockInsts(uint64_t blockId) const;
    void PrintParBlockGroups(uint64_t blockId) const;
    void PrintParBlockInsts(uint64_t blockId) const;
private:
    std::unordered_set<MachineType> filterSet;

    uint64_t blockRobDepth = 0;
    std::vector<std::vector<uint64_t>> blockDependency; // Index: blockId, Value: eventId of src blockIds

    uint64_t currentBlockId = 0;
    std::vector<BlockPipeViewPtr> blockPipeViewList;

    PipeViewOutPtr pipeViewOut = nullptr;
};

} // namespace JCore
#endif