#include "LocalFreeList.h"
#include "core/Bus.h"
#include "core/Core.h"
#include "DataStruct/MachineId.h"
#include "utils/LogInfo.h"

namespace JCore {

LocalFreeList::LocalFreeList() {}
LocalFreeList::~LocalFreeList() {}

void LocalFreeList::Work()
{
    Alloc();
    Release();
}

void LocalFreeList::Alloc()
{
    auto processQueue = [this](SimQueue<LocalFreeInfo>* queue,
                             SimQueue<LocalFreeInfo>* responseQueue) {
        if (!queue || queue->Empty()) {
            return;
        }
        while (!queue->Empty()) {
            LocalFreeInfo info = queue->Front();
            ASSERT(info.stid != -1U);
            queue->Pop();

            if (!info.last) {
                info.ptag = AllocPtag(info);
            }

            if (responseQueue) {
                responseQueue->Write(info);
            }
        }
    };

    processQueue(gsRreqVecSrfQ, gsRrspSrfVecQ);
    processQueue(gsWreqVecSrfQ, gsWrspSrfVecQ);
    processQueue(rreq_vec_srf_q, rrsp_srf_vec_q);
    processQueue(wreq_vec_srf_q, wrsp_srf_vec_q);
    processQueue(mtc_rreq_vec_srf_q, mtc_rrsp_srf_vec_q);
    processQueue(mtc_wreq_vec_srf_q, mtc_wrsp_srf_vec_q);
}

void LocalFreeList::Release()
{
}

void LocalFreeList::Xfer()
{
}

void LocalFreeList::Build()
{
    depth = GetSim()->core->configs.vec_lpreg_count;
    fList.resize(depth);
    blockFormalMap.resize(GetSim()->core->configs.scalar_smt_thread);
}

uint32_t LocalFreeList::AllocPtag(LocalFreeInfo &info)
{
    bool found = false;
    uint32_t freePtag = 0;
    for (uint32_t ptag = 0; ptag < fList.size(); ++ptag) {
        if (!fList[ptag].vld) {
            found = true;
            freePtag = ptag;
            break;
        }
    }
    if (!found) {
        ASSERT(0 && "no avail ptag");
        return UINT32_MAX;
    }
    fList[freePtag] = info;
    fList[freePtag].vld = true;
    ++size;
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "Local ptag freelist Alloc ptag. ptag=" << freePtag
        << ", bid=" << info.bid << ", offset=" << info.offset;
    return freePtag;
}

uint32_t LocalFreeList::LookupPtag(ROBID &bid, uint32_t offset, uint32_t stid)
{
    auto &entry = blockFormalMap[stid][bid.val];
    ASSERT(entry.vld && "Unalloc formal parameter");
    ASSERT(offset < entry.ptagList.size());
    return entry.ptagList[offset];
}

uint32_t LocalFreeList::DispatchPtag(ROBID &bid, uint32_t offset, uint32_t stid)
{
    for (uint32_t ptag = 0; ptag < fList.size(); ++ptag) {
        if (fList[ptag].vld && fList[ptag].bid == bid && fList[ptag].offset == offset && fList[ptag].stid == stid) {
            return ptag;
        }
    }

    LOG_ERROR << "DispatchPtag error, bid " << bid << " offset: " << dec << offset;
    ASSERT(0 && "not found");
    return 0;
}

uint32_t LocalFreeList::GetPtagData(ROBID &bid, uint32_t offset, uint32_t stid)
{
    for (uint32_t ptag = 0; ptag < fList.size(); ++ptag) {
        if (fList[ptag].vld && fList[ptag].bid == bid && fList[ptag].offset == offset && fList[ptag].stid == stid) {
            return fList[ptag].data;
        }
    }

    LOG_ERROR << "GetPtagData error, bid " << bid << " offset: " << dec << offset;
    ASSERT(0 && "not found");
    return 0;
}

void LocalFreeList::ReleasePtag(uint32_t ptag)
{
    fList[ptag].vld = false;
    ASSERT(size > 0);
    --size;
}

void LocalFreeList::ReleaseBlock(ROBID &bid, uint32_t stid)
{
    for (uint32_t ptag = 0; ptag < fList.size(); ++ptag) {
        if (fList[ptag].vld && fList[ptag].bid == bid && fList[ptag].stid == stid) {
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << "Local ptag freelist release ptag. bid " << fList[ptag].bid
                << " offset " << fList[ptag].offset;
            ReleasePtag(ptag);
        }
    }
}

bool LocalFreeList::CheckStall(uint32_t reserve)
{
    return size + reserve >= depth;
}

void LocalFreeList::Flush(FlushBus &flushReq)
{
    for (uint32_t ptag = 0; ptag < fList.size(); ++ptag) {
        if (fList[ptag].vld && fList[ptag].stid == flushReq.req.stid && LessEqual(flushReq.req.bid, fList[ptag].bid)) {
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << "Local ptag freelist flush ptag. bid " << fList[ptag].bid
                << " offset " << fList[ptag].offset;
            ReleasePtag(ptag);
        }
    }
}

}
