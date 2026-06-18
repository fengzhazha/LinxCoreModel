#include "pe/ifu/bside/ifu_SP.h"

#include "pe/ifu/iside/pe_ifu.h"

namespace JCore {


using namespace std;

namespace PE_IFU {

SP::SP(PEIFU* p) {ifu = p;}
SP::~SP() {}

void SP::Build()
{
    logLabel = "[SP" + std::to_string(id);
}

void SP::Work(FetchReqPtr &fetchReq)
{
    InnerBrType innerBrType = NOBRANCH;
    uint32_t totalWith = 0;
    uint64_t directJumpTpc = 0;
    for (uint32_t i = 0; i < fetchReq->instCnt; ++i) {
        if (totalWith >= fetchReq->mask) {
            break;
        }
        assert(i < fetchReq->data.size());
        assert(i < fetchReq->isize.size());
        innerBrType = ifu->preDecodor.preDecode(fetchReq->data[i], fetchReq->isize[i], fetchReq->btype);
        // TODO: For debug, do not delete.
        // cout << "IFU block " << fetchReq->bid << " Group " << fetchReq->gid << " fid: " << fetchReq->fid
        //     << " STID: " << dec << fetchReq->stid
        //     << " start TPC: 0x" << hex << fetchReq->tpc << " current tpc: 0x" << fetchReq->tpcArr[i]
        //     << " bin: 0x" << fetchReq->data[i]
        //     << " isize: " << dec << fetchReq->isize[i] << " curWidth: " << (totalWith + fetchReq->isize[i])
        //     << " innerBrType: " << (uint32_t)innerBrType << " inst count: " << fetchReq->instCnt << endl;
        switch (innerBrType) {
            case BEND:
                HandleBEnd(fetchReq, i, true, totalWith);
                fetchReq->instCnt = i + 1;
                return;
            case INNER_COND:
            case INNER_INDIRECT:
                fetchReq->blockNoBranch = false;
                fetchReq->instCnt = i + 1;
                HandleJump(fetchReq, i, false, totalWith);
                ifu->stats->cond++;
                ifu->stats->branch++;
                return;
            case INNER_DIRECT:
                fetchReq->blockNoBranch = false;
                fetchReq->instCnt = i + 1;
                HandleJump(fetchReq, i, true, totalWith);
                directJumpTpc = ifu->preDecodor.preDecodeDirectJump(fetchReq->data[i],
                    fetchReq->isize[i], fetchReq->btype, fetchReq->tpcArr[i]);
                ifu->ContinueFetch(fetchReq->m_threadId,  directJumpTpc,  fetchReq->bid, true, fetchReq->stid);
                ifu->stats->direct++;
                ifu->stats->branch++;
                return;
            default:
                if (fetchReq->endTPC == INST_MAX_PC) {
                    HandleBEnd(fetchReq, i, false, totalWith);
                }
                break;
        }
        totalWith += fetchReq->isize[i];
    }
    return;
}

// TODO: Add stid
void SP::handleBEnd(FetchReqPtr const& fetchReq, uint32_t idx, bool isBEnd)
{
    // pred last inst(lastfetchreq && mask last)  last inst ----------correct
    // pred not last(!lastfetchreq || !mask last)  last  inst --------miss
    // pred not last(!lastfetchreq)                not last ----------correct
    // pred last                                   not last ----------miss
    uint32_t instWidth = fetchReq->isize[idx];
    if (!isBEnd) {
        if (fetchReq->last && ((idx + 1) * instWidth == fetchReq->mask)) {
            // Flush, reGenerate Fetch
            fetchReq->last = false;
            return;
        } else {
            // correct;
            return;
        }
    }

    uint64_t tpc = fetchReq->tpc + idx * instWidth;
    ifu->ibp.update(tpc, INST_MAX_PC, fetchReq->isize[idx]);
    LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel << " BEND. bid " << dec << fetchReq->bid << " BPC 0x"
        << hex << fetchReq->bpc << " TPC 0x" << (fetchReq->tpc + idx * instWidth) << " bin " << fetchReq->data[idx];
    if (fetchReq->last) {
        if (fetchReq->endTPC == INST_MAX_PC) {
            ifu->releaseFeCtrlQ(fetchReq->bid, fetchReq->m_threadId, fetchReq->stid);
        }
        ASSERT((idx + 1) * instWidth <= fetchReq->mask);
        if ((idx + 1) * instWidth == fetchReq->mask) {
            // correct
            LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel << " Pred: bend inst, Real: bend inst. (correct)";
        } else {
            // fetchReq correct, mask error
            fetchReq->mask = (idx + 1) * instWidth;
            LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel
                << " Pred: not last inst, Real: bend inst. (correct fetchReq miss mask)";
        }
    } else {
        // flush
        LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel << " Pred: not last inst, Real: bend inst. (miss fetchReq)";
        if (fetchReq->endTPC == INST_MAX_PC) {
            if (!fetchReq->bundleNoBranch && (idx + 1) * instWidth < fetchReq->mask) {
                fetchReq->bundleNoBranch = true;
            }
            ifu->flushFront(fetchReq->bid, true, fetchReq->tpc, fetchReq->m_threadId);
        } else {
            ifu->flushFront(fetchReq->bid, false, fetchReq->tpc, fetchReq->m_threadId);
        }
        fetchReq->last = true;
        fetchReq->mask = (idx + 1) * instWidth;
    }
}

void SP::HandleBEnd(FetchReqPtr const& fetchReq, uint32_t idx, bool isBEnd, uint32_t totalWith)
{
    // pred last inst(lastfetchreq && mask last)  last inst ----------correct
    // pred not last(!lastfetchreq || !mask last)  last  inst --------miss
    // pred not last(!lastfetchreq)                not last ----------correct
    // pred last                                   not last ----------miss
    uint32_t instWidth = fetchReq->isize[idx];
    if (!isBEnd) {
        if (fetchReq->last && (totalWith == fetchReq->mask)) {
            // Flush, reGenerate Fetch
            fetchReq->last = false;
            return;
        }

        // correct;
        return;
    }

    uint64_t tpc = fetchReq->tpc + totalWith;
    ifu->ibp.update(tpc, INST_MAX_PC, instWidth);
    LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel << "BEND. bid " << fetchReq->bid <<" BPC 0x" << hex
        << fetchReq->bpc << " TPC 0x" << tpc << " bin " << fetchReq->data[idx];
    if (fetchReq->last) {
        if (fetchReq->endTPC == INST_MAX_PC) {
            ifu->releaseFeCtrlQ(fetchReq->bid, fetchReq->m_threadId, fetchReq->stid);
        }
        ASSERT(totalWith <= fetchReq->mask);
        if (totalWith == fetchReq->mask) {
            // correct
            LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel << "Pred: bend inst, Real: bend inst. (correct)";
        } else {
            // fetchReq correct, mask error
            fetchReq->mask = totalWith;
            LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel
                << "Pred: not last inst, Real: bend inst. (correct fetchReq miss mask)";
        }
    } else {
        // flush
        LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel << "Pred: not last inst, Real: bend inst. (miss fetchReq)";
        FlushFrontInfo fInfo;
        fInfo.tid = fetchReq->m_threadId;
        fInfo.fid = fetchReq->fid;
        fInfo.bid = fetchReq->bid;
        fInfo.gid = fetchReq->gid;
        fInfo.tpc = fetchReq->tpcArr[idx];
        fInfo.flush = true;
        fInfo.stid = fetchReq->stid;
        if (fetchReq->endTPC == INST_MAX_PC) {
            if (!fetchReq->bundleNoBranch && totalWith < fetchReq->mask) {
                fetchReq->bundleNoBranch = true;
            }
            ifu->FlushGidFront(fInfo);
        } else {
            fInfo.flush = true;
            ifu->FlushGidFront(fInfo);
        }
        fetchReq->last = true;
        fetchReq->mask = totalWith;
    }
}


void SP::HandleJump(FetchReqPtr const& fetchReq, uint32_t idx, bool IsDirect, uint32_t totalWith)
{
    uint32_t instWidth = fetchReq->isize[idx];
    uint32_t tid = fetchReq->m_threadId;
    LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel << "Set flush by inner branch. bid " << dec << fetchReq->bid
        << " BPC 0x" << hex << fetchReq->bpc << " TPC 0x" << (fetchReq->tpc + idx * instWidth) << " bin "
        << fetchReq->data[idx];

    FlushFrontInfo fInfo;
    fInfo.tid = fetchReq->m_threadId;
    fInfo.fid = fetchReq->fid;
    fInfo.bid = fetchReq->bid;
    fInfo.gid = fetchReq->gid;
    fInfo.tpc = fetchReq->tpcArr[idx];
    fInfo.flush = false;
    fInfo.stid = fetchReq->stid;
    ifu->FlushGidFront(fInfo);
    fetchReq->bundleNoBranch = false;
    fetchReq->mask = totalWith;
    if (!IsDirect) {
        ifu->WaitForFetch(tid);
    }
}

void SP::handleBR(FetchReqPtr const& fetchReq, uint32_t idx)
{
    fetchReq->bundleNoBranch = false;
}

void SP::StallStaticPredict()
{
    stall = true;
}
void SP::UnStallStaticPredict()
{
    stall = false;
}
bool SP::NeedStallStaticPredict()
{
    return stall;
}

}

} // namespace JCore
