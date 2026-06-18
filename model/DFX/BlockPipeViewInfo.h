#pragma once

#ifndef BLOCK_PIPE_VIEW_INFO
#define BLOCK_PIPE_VIEW_INFO

#include <cstdint>
#include <vector>
#include <deque>
#include <string>

#include "ModelCommon/CycleInfo.h"
#include "DataStruct/MachineId.h"

namespace JCore {
class InstPipeViewInfo {
public:
    uint64_t bid = 0;
    uint64_t rid = 0;
    uint64_t tid = 0;
    uint64_t tpc = 0;
    uint64_t gid = 0;
    uint64_t logicGid = 0;
    std::string label = "";
    bool isVecLite = false;
    CycleBus cycleInfo = nullptr;
};
using InstPipeViewPtr = std::shared_ptr<InstPipeViewInfo>;

class GroupPipeViewInfo {
public:
    bool isCompleted = false;
    uint64_t bid = 0;
    uint64_t tpc = 0;
    uint64_t gid = 0;
    uint64_t logicGid = 0;
    uint64_t tid = 0;
    std::string label = "";
    CycleBus cycleInfo = nullptr;
};
using GroupPipeViewPtr = std::shared_ptr<GroupPipeViewInfo>;

enum class PipeViewFlushType {
    FLUSH_MISBP = 0,
    FLUSH_NUKE,
    FLUSH_NONE,
};

class BlockPipeViewInfo {
public:
    uint64_t bid = 0;
    uint64_t bpc = 0;
    uint64_t machineId = 0;
    uint64_t tid = 0;

    bool modelCompleted = false;
    std::string label = "";
    CycleBus cycleInfo = nullptr;

    std::vector<InstPipeViewPtr> instPipeViewList;

    bool isParBlock = false;
    BIQType biqType = BIQType::NONE_IQ;
    uint64_t groupNum = 0;
    uint64_t completedGroupNum = 0;
    uint64_t groupPrintPtr = 0;
    std::vector<GroupPipeViewPtr> groupStatus;
    std::vector<std::deque<InstPipeViewPtr>> groupInstPipeViewList; // for smit minst

    std::vector<PipeViewFlushType> flushTypes;
    std::vector<std::shared_ptr<BlockPipeViewInfo>> flushInfo;

    void Reset(uint64_t blockId, uint64_t blockStartPC)
    {
        bid = blockId;
        bpc = blockStartPC;
        modelCompleted = false;
        label = "";
        cycleInfo = nullptr;
        instPipeViewList.clear();

        isParBlock = false;
        groupNum = 0;
        completedGroupNum = 0;
        groupPrintPtr = 0;
        groupStatus.clear();
        groupInstPipeViewList.clear();
    }

    void FullReset(uint64_t blockId, uint64_t blockStartPC)
    {
        Reset(blockId, blockStartPC);
        flushTypes.clear();
        flushInfo.clear();
    }

    void SetFlush(PipeViewFlushType fType)
    {
        flushTypes.push_back(fType);
        flushInfo.push_back(DeepCopy());
        Reset(bid, 0);
    }

    std::shared_ptr<BlockPipeViewInfo> DeepCopy()
    {
        std::shared_ptr<BlockPipeViewInfo> info = std::make_shared<BlockPipeViewInfo>();
        info->bid = bid;
        info->bpc = bpc;
        info->modelCompleted = modelCompleted;
        info->label = label;
        info->cycleInfo = cycleInfo;
        info->isParBlock = isParBlock;
        info->biqType = biqType;
        info->groupNum = groupNum;
        info->completedGroupNum = completedGroupNum;
        info->groupPrintPtr = groupPrintPtr;
        for (auto &inst : instPipeViewList) {
            info->instPipeViewList.push_back(inst);
        }
        for (auto &group : groupStatus) {
            info->groupStatus.push_back(group);
        }
        info->groupInstPipeViewList.clear();
        info->groupInstPipeViewList.resize(groupInstPipeViewList.size());
        for (uint64_t i = 0; i < groupInstPipeViewList.size(); i++) {
            for (auto &inst : groupInstPipeViewList[i]) {
                info->groupInstPipeViewList[i].push_back(inst);
            }
        }
        return info;
    }
};
using BlockPipeViewPtr = std::shared_ptr<BlockPipeViewInfo>;
}
#endif
