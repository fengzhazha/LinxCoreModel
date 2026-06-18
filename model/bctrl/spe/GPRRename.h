#pragma once

#ifndef GPR_RENAME_H
#define GPE_RENAME_H

#include "ModelSpec.h"
#include "ModelCommon/bus/RenameBus.h"
#include "ModelCommon/ModelCommon.h"
#include "ModelCommon/bus/FlushBus.h"
#include "ModelCommon/SimInstInfo.h"

namespace JCore {

class GPRRename : SimObj {
public:
    GPRRename() = default;
    ~GPRRename() = default;

    SimSys *sim;
    /* \brief Ptag release request from block rename and PE to iex-ptag-ready */
    OUTPUT SimQueue<uint32_t> *rtable_release_q;

    void Work() override;
    void Xfer() override;
    void Reset() override;
    void ReportStat() override;
    SimSys *GetSim() override;
    void Build();

    void RenameSrc(SimInst inst, POperandPtr psrc);
    void RenameDst(SimInst inst, POperandPtr pdst);
    void SetCheckPoint(ROBID bid, uint32_t stid);
    bool CheckStall(uint32_t stid);
    bool CheckStall(uint32_t num, uint32_t stid);
    void RetireBlock(ROBID bid, uint32_t stid);
    void Flush(FlushBus flushReq);
    uint32_t GetCommitPTAG(uint32_t atag, uint32_t stid);
private:
    std::vector<bool> freeList;
    uint32_t freeSize = 0;

    std::vector<std::vector<RenameMapInfo>> smap;
    std::vector<std::vector<RenameMapInfo>> cmap;
    std::vector<std::vector<std::vector<RenameMapInfo>>> checkPoint;
    std::vector<std::vector<bool>> checkPointVld;
    std::vector<ROBID> renamePtr;
    std::vector<ROBID> retirePtr;
    uint32_t robDepth = 0;

    std::vector<std::vector<RenameBus>> mapQ;
    std::vector<uint32_t> mapQFreeSize;

    uint32_t AllocFreeTag();
    void FreePtag(uint32_t ptag);
    void Insert2MapQ(SimInst inst, uint32_t atag, uint32_t ptag);
    void ReleaseMapQEntry(uint32_t idx, uint32_t stid);
};

}

#endif