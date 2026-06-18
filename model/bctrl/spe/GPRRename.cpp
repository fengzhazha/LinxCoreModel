#include "bctrl/spe/GPRRename.h"
#include "ModelCommon/SimInstInfo.h"
#include "ModelCommon/bus/FlushBus.h"
#include "ModelCommon/bus/MemReqBus.h"
#include "include/SimSys.h"
#include "core/Core.h"

namespace JCore {

void GPRRename::Work() {}
void GPRRename::Xfer() {}
void GPRRename::Reset() {}
void GPRRename::ReportStat() {}

SimSys* GPRRename::GetSim()
{
    return sim;
}

void GPRRename::Build()
{
    uint32_t brobDepth = sim->core->configs.block_rob_depth;
    uint32_t ptagNum = sim->core->configs.ggpr_count;
    uint32_t gprCount = static_cast<uint32_t>(GPR::GPR_COUNT);
    uint32_t mapQDepth = sim->core->configs.ggpr_mapq_depth;

    freeList = std::vector<bool>(ptagNum, true);
    freeSize = ptagNum;
    uint32_t smtCount = GetSim()->core->configs.scalar_smt_thread;
    smap = std::vector<std::vector<RenameMapInfo>>(smtCount, std::vector<RenameMapInfo>(gprCount, RenameMapInfo()));
    cmap = std::vector<std::vector<RenameMapInfo>>(smtCount, std::vector<RenameMapInfo>(gprCount, RenameMapInfo()));
    checkPoint = std::vector<std::vector<std::vector<RenameMapInfo>>>(smtCount,
            std::vector<std::vector<RenameMapInfo>>(brobDepth, std::vector<RenameMapInfo>(gprCount, RenameMapInfo())));
    checkPointVld = std::vector<std::vector<bool>>(smtCount, std::vector<bool>(brobDepth, false));
    robDepth = brobDepth;

    mapQ = std::vector<std::vector<RenameBus>>(smtCount, std::vector<RenameBus>(mapQDepth, RenameBus()));
    mapQFreeSize = std::vector<uint32_t>(smtCount, mapQDepth);

    for (uint32_t tid = 0; tid < smtCount; ++tid) {
        for (uint32_t i = 0; i < gprCount; ++i) {
            uint32_t realPtag = tid * gprCount + i;
            smap[tid][i].ptag = realPtag;
            cmap[tid][i].ptag = realPtag;
            freeList[realPtag] = false;
            --freeSize;
        }
    }
    renamePtr = std::vector<ROBID>(smtCount);
    retirePtr = std::vector<ROBID>(smtCount);
}

void GPRRename::RenameSrc(SimInst inst, POperandPtr psrc)
{
    if (psrc->type == OperandType::OPD_GREG && !psrc->renamed) {
        psrc->renamed = true;
        psrc->ptag = smap[inst->stid][psrc->value].ptag;
    }
}

void GPRRename::RenameDst(SimInst inst, POperandPtr pdst)
{
    if (pdst->type == OperandType::OPD_GREG && !pdst->renamed) {
        ASSERT(freeSize > 0);
        uint32_t ptag = AllocFreeTag();
        pdst->renamed = true;
        pdst->ptag = ptag;
        smap[inst->stid][pdst->value].ptag = ptag;
        Insert2MapQ(inst, pdst->value, ptag);
    }
}

void GPRRename::SetCheckPoint(ROBID bid, uint32_t stid)
{
    checkPoint[stid][bid.val] = smap[stid];
    checkPointVld[stid][bid.val] = true;
    renamePtr[stid] = bid;
}

bool GPRRename::CheckStall(uint32_t stid)
{
    return freeSize == 0 || mapQFreeSize[stid] == 0;
}

bool GPRRename::CheckStall(uint32_t num, uint32_t stid)
{
    return freeSize < num || mapQFreeSize[stid] < num;
}

uint32_t GPRRename::AllocFreeTag()
{
    uint32_t ptag = -1U;
    for (uint32_t i = 0; i < freeList.size(); ++i) {
        if (freeList[i]) {
            freeList[i] = false;
            ptag = i;
            freeSize--;
            break;
        }
    }
    ASSERT(ptag != -1U);
    return ptag;
}

void GPRRename::Insert2MapQ(SimInst inst, uint32_t atag, uint32_t ptag)
{
    uint32_t stid = inst->stid;
    for (uint32_t i = 0; i < mapQ[stid].size(); ++i) {
        auto &e = mapQ[stid][i];
        if (!e.vld) {
            e.vld = true;
            e.retired = false;
            e.peID = inst->peID;
            e.coreID = inst->coreID;
            e.tid = inst->stid;
            e.bid = inst->bid;
            e.rid = inst->rid;
            e.gid = inst->gid;
            e.type = OperandType::OPD_GREG;
            e.atag = atag;
            e.ptag = ptag;
            --mapQFreeSize[stid];
            break;
        }
    }
}

void GPRRename::RetireBlock(ROBID bid, uint32_t stid)
{
    retirePtr[stid] = bid;
    for (uint32_t i = 0; i < mapQ[stid].size(); ++i) {
        auto &e = mapQ[stid][i];
        if (e.vld && e.bid == bid) {
            FreePtag(cmap[stid][e.atag].ptag);
            rtable_release_q->Write(cmap[stid][e.atag].ptag);
            cmap[stid][e.atag].ptag = e.ptag;
            ReleaseMapQEntry(i, stid);
        }
    }
}

void GPRRename::FreePtag(uint32_t ptag)
{
    ASSERT(!freeList[ptag]);
    freeList[ptag] = true;
    ++freeSize;
}

void GPRRename::ReleaseMapQEntry(uint32_t idx, uint32_t stid)
{
    ASSERT(mapQ[stid][idx].vld);
    mapQ[stid][idx].Reset();
    ++mapQFreeSize[stid];
}

void GPRRename::Flush(FlushBus flushReq)
{
    ROBID youngerestBid = flushReq.req.bid;
    SubROBID(youngerestBid, 1, robDepth);
    uint32_t stid = flushReq.req.stid;
    if (LessEqual(youngerestBid, renamePtr[stid])) {
        renamePtr[stid] = youngerestBid;
        if (checkPointVld[stid][renamePtr[stid].val]) {
            smap[stid] = checkPoint[stid][renamePtr[stid].val];
        } else {
            smap[stid] = cmap[stid];
        }
    }
    for (uint32_t i = 0; i < mapQ[stid].size(); ++i) {
        auto &e = mapQ[stid][i];
        if (e.vld) {
            if ((flushReq.baseOnBid && LessEqual(flushReq.req.bid, e.bid))
                || (!flushReq.baseOnBid && LessEqual(flushReq.req.bid, flushReq.req.rid, e.bid, e.rid))) {
                FreePtag(e.ptag);
                ReleaseMapQEntry(i, stid);
            }
        }
        if (e.vld && !flushReq.baseOnBid && flushReq.req.bid == e.bid) {
            smap[stid][e.atag].ptag = e.ptag;
        }
    }
}

uint32_t GPRRename::GetCommitPTAG(uint32_t atag, uint32_t stid)
{
    return cmap[stid][atag].ptag;
}

}