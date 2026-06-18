#include "core/FlushControl.h"

#include <iomanip>

#include "core/Bus.h"
#include "core/SRename.h"
#include "ModelSpec.h"
#include "SimSys.h"
#include "pe/PEBase.h"
#include "mtccore/lsu/MtcLoadStoreUnit.h"

namespace JCore {

void FlushControl::Build() {
    flush_stats = new FlushStats(GetSim()->getRpt());
    peSignal = vector<FlushBus> (core->configs.stdPeCount + core->configs.simtPeCount + core->configs.memPeCount);
    peSignalEnabled = vector<bool> (core->configs.stdPeCount + core->configs.simtPeCount + core->configs.memPeCount);
    Reset();
}

void FlushControl::Reset() {
    flushSignalEnabled = false;
    flushSignal = FlushBus();
    replaySignal = FlushBus();

    for (uint32_t i = 0; i < peSignal.size(); ++i) {
        peSignal[i] = FlushBus();
        peSignalEnabled[i] = false;
    }
}

bool FlushControl::needFlush() {
    return flushSignal.req.vld;
}

bool FlushControl::needFlush(ROBID &bid) {
    if (!flushSignal.req.vld) {
        return false;
    }

    if (LessEqual(flushSignal.req.bid, bid)) {
        return true;
    }

    return false;
}

bool FlushControl::needFlush(ROBID &bid, ROBID &subID) {
    if (!flushSignal.req.vld) {
        return false;
    }

    if (flushSignal.baseOnBid && LessEqual(flushSignal.req.bid, bid)) {
        return true;
    }

    if (!flushSignal.baseOnBid && LessEqual(flushSignal.req.bid, flushSignal.req.lsID, bid, subID)) {
        return true;
    }

    return false;
}

bool isFlushType(FlushType type) {
    if (type == FlushType::MISS_PRED_FLUSH || type == FlushType::NUKE_FLUSH || type == FlushType::INNER_FLUSH ||
        type == FlushType::FAST_FLUSH) {
        return true;
    }

    return false;
}

bool isbaseOnBid(FlushReq &req) {
    if (req.type == FlushType::MISS_PRED_FLUSH || req.type == FlushType::NUKE_FLUSH ||
        req.type == FlushType::FAST_REPLAY || req.type == FlushType::FAST_FLUSH) {
        return true;
    }
    if (req.type == FlushType::PE_REPLAY && !req.fetchTPCVld) {
        return true;
    }

    return false;
}

bool isBasedOnPE(FlushReq &req) {
    if (req.type == FlushType::PE_REPLAY) {
        return true;
    }

    // TODO: Only support for simt mode for SIMT header
    if (req.iexTyp != SCALAR_IEX) {
        return true;
    }

    return false;
}

bool IsBasedOnThread(FlushReq &req)
{
    // TODO: Only support for simt mode for SIMT header
    if (req.iexTyp != SCALAR_IEX) {
        return true;
    }

    return false;
}

bool IsBasedOnGroup(FlushReq &req)
{
    // TODO: Only support for simt mode for SIMT header
    if (req.type == FlushType::SIMT_INNER_FLUSH) {
        return true;
    }

    return false;
}

bool IsNeedSimtReplay(FlushReq &req)
{
    if (req.iexTyp == SIMT_IEX || static_cast<uint32_t>(req.iexTyp) >= static_cast<uint32_t>(ExecEngineTyp::IEX_NUM)) {
        return true;
    }

    return false;
}

bool IsNeedMtcReplay(FlushReq &req)
{
    if (req.iexTyp == MEM_IEX) {
        return true;
    }

    return false;
}


bool FlushControl::CheckOlder(FlushBus &srcSignal, FlushBus &dstSignal) {
    if (srcSignal.req.stid != dstSignal.req.stid) {
        return false;
    }

    FlushType srcType = srcSignal.req.type;
    FlushType dstType = dstSignal.req.type;
    bool baseOnBid = srcSignal.baseOnBid || dstSignal.baseOnBid;

    // Check for conflicts. The same bid or rid.
    if (baseOnBid && srcSignal.req.bid == dstSignal.req.bid) {
        // miss predict > nuke flush > inner flush
        // fast replay > pe replay
        if (srcType == FlushType::MISS_PRED_FLUSH) {
            return true;
        }
        if (srcType == FlushType::NUKE_FLUSH && dstType == FlushType::INNER_FLUSH) {
            return true;
        }
        if (srcType == FlushType::NUKE_FLUSH && dstType == FlushType::PE_REPLAY) {
            return true;
        }
        if (srcType == FlushType::FAST_REPLAY && dstType == FlushType::PE_REPLAY) {
            return true;
        }
        return false;
    }
    if (!baseOnBid && srcSignal.req.bid == dstSignal.req.bid && srcSignal.req.rid == dstSignal.req.rid) {
        if (srcType == FlushType::INNER_FLUSH && dstType == FlushType::PE_REPLAY) {
            return true;
        }
        if (srcType == FlushType::PE_REPLAY && dstType == FlushType::INNER_FLUSH) {
            return true;
        }
        if (srcType == FlushType::INNER_FLUSH && dstType == FlushType::INNER_FLUSH) {
            return true;
        }
        return false;
    }
    // Check for the smae PE signals
    if (srcSignal.req.type == FlushType::PE_REPLAY) {
        if (dstType == FlushType::MISS_PRED_FLUSH) {
            return false;
        }
        if (dstType == FlushType::FAST_REPLAY) {
            return true;
        }
        if (dstType == FlushType::PE_REPLAY) {
            if (srcSignal.req.peID != dstSignal.req.peID) {
                return false;
            } else {
                return baseOnBid ? LessEqual(srcSignal.req.bid, dstSignal.req.bid) :
                        LessEqual(srcSignal.req.bid, srcSignal.req.rid, dstSignal.req.bid, dstSignal.req.rid);
            }
        }
    }
    // Check for older signals.
    ROBID oldestBid = core->bctrl->blockROB.getOldestBlockID(srcSignal.req.stid);
    if (baseOnBid && (LessEqual(srcSignal.req.bid, dstSignal.req.bid) || srcSignal.req.bid == oldestBid)) {
        return true;
    }
    if (!baseOnBid && LessEqual(srcSignal.req.bid, srcSignal.req.rid, dstSignal.req.bid, dstSignal.req.rid)) {
        return true;
    }

    return false;
}

void printThrow(FlushBus &olderSignal, FlushBus &throwSignal) {
    LOG_INFO_M(Unit::FLUSH_UNIT, Stage::NA) << " Older["  << olderSignal << "]. Throw away [" << throwSignal << "]";
}

void printSelect(FlushBus &cancelSignal, FlushBus &selectSignal) {
    if (cancelSignal.req.vld) {
        LOG_INFO_M(Unit::FLUSH_UNIT, Stage::NA) << "Cancel " << cancelSignal;
    }
    LOG_INFO_M(Unit::FLUSH_UNIT, Stage::NA) << "Select " << selectSignal;
}

FlushBus getSignal(FlushReq &req) {
    // Fill in the parameters
    FlushBus signal = FlushBus(req);
    signal.baseOnBid = isbaseOnBid(signal.req);
    signal.baseOnPE = isBasedOnPE(signal.req);
    signal.baseOnThread = IsBasedOnThread(signal.req);
    signal.baseOnGroup = IsBasedOnGroup(signal.req);
    signal.simtReplay = IsNeedSimtReplay(signal.req);
    signal.mtcReplay = IsNeedMtcReplay(signal.req);

    return signal;
}

void FlushControl::report(FlushReq &req) {
    if (!req.vld) {
        return;
    }

    // Ignore When exception block occur nuke/inner flush or fast/pe replay.
    if (req.type != FlushType::MISS_PRED_FLUSH &&
        core->bctrl->blockROB.getBlockStatus(req.bid, req.stid) == BlockStatus::EXCEPTION) {
        return;
    }

    FlushBus signal = getSignal(req);
    select(signal);
}

void FlushControl::select(FlushBus &signal) {
    // Flushing basing on PE
    if (signal.baseOnPE) {
        selectPESigal(signal);
        return;
    }

    if (signal.baseOnThread) {
        selectPESigal(signal);
        return;
    }

    // Replay
    if (!isFlushType(signal.req.type)) {
        selectReplaySigal(signal);
        return;
    }

    // Flush
    selectFlushSigal(signal);
}

bool FlushControl::mergeSignal(FlushBus &srcSignal, FlushBus &dstSignal)
{
    FlushType srcType = srcSignal.req.type;
    bool baseOnBid = srcSignal.baseOnBid | dstSignal.baseOnBid;
    bool lessEq = baseOnBid ? (LessEqual(dstSignal.req.bid, srcSignal.req.bid)) :
                    (LessEqual(dstSignal.req.bid, dstSignal.req.rid, srcSignal.req.bid, srcSignal.req.rid));
    if (!baseOnBid && srcSignal.req.peID == dstSignal.req.peID && lessEq) {
        if (srcType == FlushType::INNER_FLUSH) {
            LOG_INFO_M(Unit::FLUSH_UNIT, Stage::NA) << "Merge. Signal ["<<dstSignal<<"], inner flush ["<<srcSignal<<"]";
            if (DEBUG_VERBOSE_ON || core->configs.flushVerbose) {
                LOG_DEBUG_STRUCT(debugLogger, Unit::FLUSH_UNIT, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                                 dstSignal,
                                 "[FlushControl]: mergeSignal, srcType is FlushType::INNER_FLUSH, dstSignal: ");
                LOG_DEBUG_STRUCT(debugLogger, Unit::FLUSH_UNIT, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                                 srcSignal,
                                 "[FlushControl]: mergeSignal, srcType is FlushType::INNER_FLUSH, srcSignal: ");
            }
            srcSignal.req = dstSignal.req;
            srcSignal.req.type = FlushType::INNER_FLUSH;
            return true;
        }
    }

    if (baseOnBid && srcSignal.req.peID == dstSignal.req.peID && LessROBID(dstSignal.req.bid, srcSignal.req.bid)) {
        if (srcType == FlushType::NUKE_FLUSH) {
            LOG_INFO_M(Unit::FLUSH_UNIT, Stage::NA) << "Merge. Signal ["<<dstSignal<<"], Nuke flush ["<<srcSignal<<"].";
            if (DEBUG_VERBOSE_ON || core->configs.flushVerbose) {
                LOG_DEBUG_STRUCT(debugLogger, Unit::FLUSH_UNIT, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                                 dstSignal,
                                 "[FlushControl]: mergeSignal, srcType is FlushType::NUKE_FLUSH, dstSignal: ");
                LOG_DEBUG_STRUCT(debugLogger, Unit::FLUSH_UNIT, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                                 srcSignal,
                                 "[FlushControl]: mergeSignal, srcType is FlushType::NUKE_FLUSH, srcSignal: ");
            }
            srcSignal.req = dstSignal.req;
            srcSignal.req.type = FlushType::INNER_FLUSH;
            return true;
        }
    }
    return false;
}

void FlushControl::Work() {
}

void FlushControl::Xfer() {
    auto reportFlush = [this](SimQueue<FlushReq> *queue) {
        while (!queue->Empty()) {
            FlushReq req = queue->Read();
            this->report(req);
        }
    };

    reportFlush(bcc_flush_rpt_q);
    for (auto &rpt_q : iex_flush_rpt_q) {
        reportFlush(rpt_q);
    }

    for (auto simQ : pe_flush_rpt_array) {
        reportFlush(simQ);
    }
    for (auto &rptQ : lsu_flush_rpt_q) {
        reportFlush(rptQ);
    }

    flushSignalEnabled = CheckFlush(flushSignal);
    for (uint32_t i = 0; i < peSignal.size(); ++i) {
        peSignalEnabled[i] = CheckFlush(peSignal[i]);
    }

    flush();
    replay();
    FlushBaseOnPE();
}

bool FlushControl::CheckFlush(FlushBus &signal)
{
    if (!signal.req.vld) {
        return false;
    }

    // Flush condition: Miss prediction or oldest block.
    // Ready for flush.
    // TODO: Fix next alloc.
    if (signal.req.type == FlushType::MISS_PRED_FLUSH) {
        core->bctrl->blockROB.recoverBlock(signal);
        return true;
    }

    if (replaySignal.req.vld && LessEqual(replaySignal.req.bid, signal.req.bid)) {
        // Cancel nuke/inner flush.
        printThrow(replaySignal, signal);
        signal = FlushBus();
        return false;
    }

    ROBID oldestBid = core->bctrl->blockROB.getOldestBlockID(signal.req.stid);
    ROBID oldestGid = core->vectorTop->GetOldestGid(signal.req.stid);

    if (signal.req.iexTyp == MEM_IEX) {
        oldestGid = core->mtcCores[0]->GetOldestGid();
    }

    // TODO: need the tid
    IDBus commitRidBus = core->peArray[signal.req.peID]->GetRetireID();
    // Check if it is oldest block or oldest micro-inst.
    if (signal.req.iexTyp == SCALAR_IEX &&
        ((signal.baseOnBid && LessEqual(signal.req.bid, oldestBid)) ||(signal.req.immediateFlush) ||
        (!signal.baseOnBid && LessEqual(signal.req.bid, signal.req.rid, oldestBid, commitRidBus.rid)))) {
        core->bctrl->blockROB.setFlushed(signal);
        return true;
    }

    if ((core->IsVectorIex(signal.req.iexTyp) || signal.req.iexTyp == MEM_IEX) &&
        ((signal.baseOnBid && LessEqual(signal.req.bid, oldestBid)) ||(signal.req.immediateFlush) ||
        (!signal.baseOnBid && LessEqual(signal.req.bid, signal.req.gid,
            signal.req.rid, oldestBid, oldestGid, commitRidBus.rid)))) {
        return true;
    }

    return false;
}

void FlushControl::selectFlushSigal(FlushBus &signal) {
    // Check for older signals.
    if (flushSignal.req.vld && CheckOlder(flushSignal, signal)) {
        printThrow(flushSignal, signal);
        return;
    }

    if (signal.req.type != FlushType::MISS_PRED_FLUSH) {
        if (peSignal[signal.req.peID].req.vld && CheckOlder(peSignal[signal.req.peID], signal)) {
            printThrow(peSignal[signal.req.peID], signal);
            if (mergeSignal(signal, peSignal[signal.req.peID])) {
                FlushReq newReq = signal.req;
                peSignal[signal.req.peID] = FlushBus();
                peSignalEnabled[signal.req.peID] = false;
                this->report(newReq);
            }
            return;
        }
    }

    // Set the flush signal.
    printSelect(flushSignal, signal);
    if (signal.req.type != FlushType::MISS_PRED_FLUSH && signal.req.type != FlushType::SIMT_INNER_FLUSH) {
        core->bctrl->blockROB.setNeedFlush(signal.req.bid, signal.req.stid);
    }
    if (!signal.baseOnBid) {
        core->peArray[signal.req.peID]->SetNeedFlush(signal.req.bid, signal.req.rid, signal.req.lsID, signal.req.tid);
    }
    flushSignal = signal;

    // Clear pe signal.
    for (uint32_t i = 0; i < peSignal.size(); ++i) {
        auto &sgl = peSignal[i];
        if (sgl.req.vld && CheckOlder(flushSignal, sgl)) {
            LOG_INFO_M(Unit::FLUSH_UNIT, Stage::NA) << "Cancel "  << sgl;
            sgl = FlushBus();
            peSignalEnabled[i] = false;
        }
    }

    // Clear replay.
    if (replaySignal.req.vld && CheckOlder(flushSignal, replaySignal)) {
        LOG_INFO_M(Unit::FLUSH_UNIT, Stage::NA) << "Cancel "  << replaySignal;
        replaySignal = FlushBus();
    }
}

void FlushControl::selectReplaySigal(FlushBus &signal) {
    // If the oldest block is complete, then throw this replay.
    if (core->bctrl->blockROB.IsOldestBlkComplete(signal.req.stid)) {
        return;
    }

    // Check for older signals.
    // Nuke/Inner flush need to effect when the block become oldest, so it can not cancel the fast replay signal.
    if (flushSignal.req.vld && flushSignal.req.type == FlushType::MISS_PRED_FLUSH && CheckOlder(flushSignal, signal)) {
        printThrow(flushSignal, signal);
        return;
    }
    if (replaySignal.req.vld && CheckOlder(replaySignal, signal)) {
        printThrow(replaySignal, signal);
        return;
    }

    // Set the replay signal.
    printSelect(replaySignal, signal);
    if (!signal.baseOnBid) {
        core->peArray[signal.req.peID]->SetNeedFlush(signal.req.bid, signal.req.rid, signal.req.lsID, signal.req.tid);
    }
    replaySignal = signal;

    // Clear pe signal.
    for (uint32_t i = 0; i < peSignal.size(); ++i) {
        auto &sgl = peSignal[i];
        if (sgl.req.vld && CheckOlder(signal, sgl)) {
            LOG_INFO_M(Unit::FLUSH_UNIT, Stage::NA) << "Cancel " << sgl;
            sgl = FlushBus();
            peSignalEnabled[i] = false;
        }
    }
}

void FlushControl::selectPESigal(FlushBus &signal) {
    // Check for older signals.
    if (flushSignal.req.vld && CheckOlder(flushSignal, signal)) {
        printThrow(flushSignal, signal);
        if (mergeSignal(flushSignal, signal)) {
            FlushReq newReq = flushSignal.req;
            flushSignal = FlushBus();
            this->report(newReq);
        }
        return;
    }
    // Check the oldest flush signal of the same PE.
    // TODO: Delete after merge flushReqs.
    if (flushSignal.req.vld && signal.req.peID == flushSignal.req.peID && CheckOlder(signal, flushSignal)) {
        printThrow(signal, flushSignal);
        if (mergeSignal(flushSignal, signal)) {
            FlushReq newReq = flushSignal.req;
            flushSignal = FlushBus();
            this->report(newReq);
        }
        return;
    }
    if (replaySignal.req.vld && CheckOlder(replaySignal, signal)) {
        printThrow(replaySignal, signal);
        return;
    }

    FlushBus &sgl = peSignal[signal.req.peID];
    if (sgl.req.vld && CheckOlder(sgl, signal)) {
        printThrow(sgl, signal);
        return;
    }

    // Set the PE replay signal.
    printSelect(sgl, signal);
    if (signal.req.type != FlushType::MISS_PRED_FLUSH && signal.req.type != FlushType::SIMT_INNER_FLUSH) {
        core->bctrl->blockROB.setNeedFlush(signal.req.bid, signal.req.stid);
    }
    if (!signal.baseOnBid) {
        core->peArray[signal.req.peID]->SetNeedFlush(signal.req.bid, signal.req.rid, signal.req.lsID, signal.req.tid);
    }
    sgl = signal;
}

void FlushControl::flushRpt(FlushBus &signal) {
    auto match = [this, &signal] (FlushReq req) -> bool {
        FlushBus sgl = getSignal(req);
        return CheckOlder(signal, sgl);
    };

    bcc_flush_rpt_q->FlushIf(match);
    for (auto &rpt_q : iex_flush_rpt_q) {
        rpt_q->FlushIf(match);
    }
    for (auto simQ : pe_flush_rpt_array) {
        simQ->FlushIf(match);
    }

    for (auto &rptQ : lsu_flush_rpt_q) {
        rptQ->FlushIf(match);
    }
}

void FlushControl::rptStatistics(FlushBus flushReq) {
    if (!flushReq.req.vld) {
        return;
    }
    switch (flushReq.req.type) {
        case FlushType::MISS_PRED_FLUSH:
            flush_stats->efficientInerMisprediction++;
            break;
        case FlushType::NUKE_FLUSH:
            flush_stats->efficientInterConflict++;
            break;
        case FlushType::INNER_FLUSH:
            flush_stats->efficientIntraConflict++;
            break;
        case FlushType::PE_REPLAY:
            flush_stats->efficientIntraMisprediction++;
            break;
        case FlushType::FAST_REPLAY:
            flush_stats->efficientBlockReplay++;
            break;
        default:
            break;
    }
}

void FlushControl::flush() {
    if (flushSignalEnabled) {
        // get block cmd
        BlockCommandPtr cmd = core->bctrl->blockROB.GetBlockCMDPtr(flushSignal.req.bid, flushSignal.req.stid);
        if (cmd == nullptr) {
            cmd = core->bctrl->blockROB.GetLastBlockCMDPtr(flushSignal.req.bid, flushSignal.req.stid);
        }
        LOG_INFO_M(Unit::FLUSH_UNIT, Stage::NA) << "Generate[global flush] "<< flushSignal;

        flushRpt(flushSignal);
        core->bctrl->flush(flushSignal);
        core->sRenameUnit->flush(flushSignal);
        rptStatistics(flushSignal);

        // Conbine flush and replay signal.
        if (replaySignal.req.vld) {
            if (LessEqual(replaySignal.req.bid, flushSignal.req.bid)) {
                flushRpt(replaySignal);
                // Back-end flush base on oldest bid.
                core->bctrl->replay(replaySignal);
                core->sRenameUnit->replay(replaySignal);
                flushSignal = replaySignal;
                rptStatistics(replaySignal);

                LOG_INFO_M(Unit::FLUSH_UNIT, Stage::NA) << "Generate "<< replaySignal;
            }
            replaySignal = FlushBus();
        }

        flushBackend(flushSignal);
        flushSignalEnabled = false;
        flushSignal = FlushBus();
    }
}

void FlushControl::replay() {
    if (replaySignal.req.vld) {
        LOG_INFO_M(Unit::FLUSH_UNIT, Stage::NA) << "Generate[replay] "<< replaySignal;
        flushRpt(replaySignal);
        core->bctrl->replay(replaySignal);
        core->sRenameUnit->replay(replaySignal);
        flushBackend(replaySignal);
        rptStatistics(replaySignal);
        replaySignal = FlushBus();
    }
}

void FlushControl::FlushBaseOnPE() {
    for (uint32_t i = 0; i < peSignal.size(); ++i) {
        auto &sgl = peSignal[i];
        // Find the pe which is need to be replayed.
        if (peSignalEnabled[i] && sgl.req.vld) {
            LOG_INFO_M(Unit::FLUSH_UNIT, Stage::NA) << "Generate[pe flush] "<< sgl;

            core->bctrl->replay(sgl);
            core->sRenameUnit->replay(sgl);
            flushBackend(sgl);
            rptStatistics(sgl);
            core->bctrl->blockROB.SimtRecoverd(sgl);
            sgl = FlushBus();
            peSignalEnabled[i] = false;
        }
    }
}

void FlushControl::flushBackend(FlushBus &signal) {
    if (signal.simtReplay) {
        core->vectorTop->Replay(signal);
    } else {
        core->vectorTop->SetFlush(signal);
        core->vecliteTop->SetFlush(signal);
    }

    for (auto &cubeCore: core->cubeCores) {
        if (!cubeCore) {
            continue;
        }
        cubeCore->SetFlush(signal);
    }

    if (core->configs.mtc_separate_enable) {
        for (auto &vcore: core->mtcCores) {
            if (signal.simtReplay) {
                break;
            }
            if (!vcore) {
                continue;
            }
            if (signal.mtcReplay) {
                vcore->Replay(signal);
            } else {
                vcore->SetFlush(signal);
            }
        }
    }

    // flush all components
    if (signal.simtReplay || signal.mtcReplay || signal.baseOnPE) {
        core->peArray[signal.req.peID]->Flush(signal);
    } else {
        for (auto &pe: core->peArray) {
            pe->Flush(signal);
        }
    }

    if ((signal.simtReplay) && core->IsVectorIex(signal.req.iexTyp)) {
        uint32_t coreId = signal.req.peID - core->configs.stdPeCount;
        core->vectorTop->GetIex(coreId)->setFlush(signal);
        core->memIntf[static_cast<int>(LSUType::VECTOR_LSU)]->setFlush(signal);
    } else if ((signal.mtcReplay) && (signal.req.iexTyp == MEM_IEX)) {
        core->iex[signal.req.iexTyp]->setFlush(signal);
        core->MtcmemIntf[0]->SetFlush(signal);
    } else {
        /* The scalar flush also requires the vector IEX to be flushed. */
        uint32_t iexNum = static_cast<uint32_t>(ExecEngineTyp::IEX_NUM) + core->configs.simtPeCount;
        for (uint32_t i = 0; i < iexNum; ++i) {
            if (core->IsVectorIex(static_cast<ExecEngineTyp>(i))) {
                uint32_t coreId = (i == SIMT_IEX) ? 0 : i - static_cast<uint32_t>(ExecEngineTyp::IEX_NUM);
                core->vectorTop->GetIex(coreId)->setFlush(signal);
            } else {
                if (core->iex[i] == nullptr) {
                    continue;
                }
                core->iex[i]->setFlush(signal);
            }
        }
        for (auto &lsu: core->memIntf) {
            lsu->setFlush(signal);
        }

        for (auto &lsu: core->MtcmemIntf) {
            lsu->SetFlush(signal);
        }
    }

    core->tileReg->stashCtrlUnit->Flush(signal);

    if (core->configs.tileBridge) {
        for (auto& tileBridge : core->tileBridges) {
            tileBridge->Flush(signal);
        }
    }
}

SimSys *FlushControl::GetSim() {
    return sim;
}

FlushControl::~FlushControl()
{
    DeletePtr(flush_stats);
}

} // namespace JCore