#include "ViewManager.h"

#include "include/SimSys.h"
namespace JCore {

void ViewManager::InitViewManager(uint64_t bRobDepth)
{
    config.overrideDefaultConfig(sim->getCfgs());
    if (config.pipeViewFilterEnable) {
        for (auto &str : config.pipeViewFilterSet) {
            filterSet.insert(StrToMachineType(str));
        }
    }
    blockDependency.clear();
    blockDependency.resize(bRobDepth);

    blockPipeViewList.clear();
    blockPipeViewList.resize(bRobDepth);
    for (uint64_t i = 0; i < bRobDepth; i++) {
        blockPipeViewList[i] = std::make_shared<BlockPipeViewInfo>();
        blockPipeViewList[i]->Reset(i, 0);
    }
    blockRobDepth = bRobDepth;
    if (config.printPipeView) {
        pipeViewOut = std::make_shared<PipeViewOut>();
        pipeViewOut->sim = sim;
        pipeViewOut->Init(config.pipeViewOutFile + ".out");
    }
}

void ViewManager::AddBlockSrc(uint64_t curBlockId, uint64_t srcBlockId)
{
    if (!config.recordDepency) {
        return;
    }
    blockDependency[curBlockId].push_back(srcBlockId);
}

std::vector<uint64_t>& ViewManager::GetBlockSrcVec(uint64_t blockId)
{
    return blockDependency[blockId];
}

void ViewManager::ClearBlockSrcVec(uint64_t blockId)
{
    blockDependency[blockId].clear();
}

void ViewManager::ResetBlockDepInfo(uint64_t blockId)
{
    ClearBlockSrcVec(blockId);
}

void ViewManager::ResetBlockPipeInfo(uint64_t blockId, uint64_t bpc)
{
    blockPipeViewList[blockId]->FullReset(blockId, bpc);
    LOG_INFO_M(Unit::DFX, Stage::NA) << "Reset view manager ptr" << std::dec << blockId << " bpc:0x" << std::hex << bpc;
}

void ViewManager::FlushBlockInfo(uint64_t blockId, PipeViewFlushType fType)
{
    (void)fType;
    blockPipeViewList[blockId]->Reset(blockId, 0);
}

void ViewManager::RecordBlockCompleted(BlockPipeViewPtr blockInfo)
{
    if (!config.printPipeView) {
        return;
    }
    blockPipeViewList[blockInfo->bid]->label = blockInfo->label;
    blockPipeViewList[blockInfo->bid]->cycleInfo = blockInfo->cycleInfo;
    blockPipeViewList[blockInfo->bid]->modelCompleted = true;
    blockPipeViewList[blockInfo->bid]->machineId = blockInfo->machineId;
    LOG_INFO_M(Unit::DFX, Stage::NA) << "View manager ptr" << std::dec << blockInfo->bid << " completed";
}

void ViewManager::RecordMinst(InstPipeViewPtr instInfo)
{
    if (!config.printPipeView) {
        return;
    }
    blockPipeViewList[instInfo->bid]->instPipeViewList.push_back(instInfo);
}

void ViewManager::InitBIQType(uint64_t blockId, BIQType biqType)
{
    if (!config.printPipeView) {
        return;
    }
    blockPipeViewList[blockId]->biqType = biqType;
    LOG_INFO_M(Unit::DFX, Stage::NA) << "View manager Init " << std::dec << blockId << " biqType:" <<
        BIQTypeName(biqType);
}

void ViewManager::InitPARGroup(uint64_t blockId, uint64_t groupNum)
{
    if (!config.printPipeView) {
        return;
    }
    blockPipeViewList[blockId]->isParBlock = true;
    blockPipeViewList[blockId]->groupNum = groupNum;
    blockPipeViewList[blockId]->groupStatus.resize(groupNum);
    blockPipeViewList[blockId]->groupInstPipeViewList.resize(groupNum);
    LOG_INFO_M(Unit::DFX, Stage::NA) << "View manager Init " << std::dec << blockId << " groupNum:" << groupNum;
}

void ViewManager::RecordPARGroupCompleted(GroupPipeViewPtr groupInfo)
{
    if (!config.printPipeView) {
        return;
    }
    ASSERT(blockPipeViewList[groupInfo->bid]->groupStatus.size() > groupInfo->logicGid);
    blockPipeViewList[groupInfo->bid]->completedGroupNum++;
    blockPipeViewList[groupInfo->bid]->groupStatus[groupInfo->logicGid] = groupInfo;
    blockPipeViewList[groupInfo->bid]->groupStatus[groupInfo->logicGid]->isCompleted = true;
}

void ViewManager::RecordPARMinst(InstPipeViewPtr instInfo)
{
    if (!config.printPipeView) {
        return;
    }
    ASSERT(blockPipeViewList[instInfo->bid]->groupInstPipeViewList.size() > instInfo->logicGid);
    blockPipeViewList[instInfo->bid]->groupInstPipeViewList[instInfo->logicGid].push_back(instInfo);
}

void ViewManager::PrintPipeView(uint64_t width)
{
    if (!config.printPipeView) {
        return;
    }
    for (uint64_t i = 0; i < width; i++) {
        uint64_t ptr = currentBlockId % blockRobDepth;
        if (!blockPipeViewList[ptr]->modelCompleted) {
            break;
        }
        PrintBlockInfo(ptr);
        currentBlockId++;
        blockPipeViewList[ptr]->FullReset(ptr, 0);
        LOG_INFO_M(Unit::DFX, Stage::NA) << "Print Block PipeView" << ptr << " bpc:0x" << std::hex
            << blockPipeViewList[ptr]->bpc << " total Print Block:" << std::dec << currentBlockId;
        blockPipeViewList[ptr]->FullReset(ptr, 0);
    }
}

std::string ViewManager::GetBID() const
{
    return "[B" + std::to_string(currentBlockId) + "]";
}

void ViewManager::PrintBlockInfo(uint64_t blockId)
{
    BlockPipeViewPtr blockInfo = blockPipeViewList[blockId];
    blockInfo->label = GetBID() + blockInfo->label;
    auto type = GetMachineType(blockInfo->machineId);
    pipeViewOut->PrintBlockStatus(blockInfo);
    bool biqEnable = !blockInfo->isParBlock || !config.pipeViewFilterEnable ||
        (config.pipeViewFilterEnable && filterSet.find(type) != filterSet.end());

    if (biqEnable) {
        for (InstPipeViewPtr &instInfo : blockInfo->instPipeViewList) {
            pipeViewOut->PrintUinst(instInfo);
        }
    }
    if (blockInfo->isParBlock && biqEnable) {
        for (uint64_t i = 0; i < blockInfo->groupNum; i++) {
            pipeViewOut->PrintGroup(blockInfo->groupStatus[i]);
            for (InstPipeViewPtr &instInfo : blockInfo->groupInstPipeViewList[i]) {
                pipeViewOut->PrintUinst(instInfo);
            }
        }
    }
}

void ViewManager::PrintBlockInsts(uint64_t blockId) const
{
    (void)blockId;
}

void ViewManager::PrintParBlockGroups(uint64_t blockId) const
{
    (void)blockId;
}

void ViewManager::PrintParBlockInsts(uint64_t blockId) const
{
    (void)blockId;
}
}
