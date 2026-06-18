#include "bctrl/BROB.h"

#include <cstring>
#include <iomanip>
#include <iostream>

#include "bctrl/BCtrl.h"
#include "core/Core.h"
#include "lsu/lsu.h"
#include "utils/obj_print.h"
#include "DFX/InstTracer.h"

namespace JCore {

using namespace std;

ROBID BlockROB::allocBlock(BHeader &header, BlockCommandPtr cmd, uint32_t pos) {
    uint32_t stid = cmd->stid;
    IncROBID(next[stid].allocPtr, next[stid].entry.size());
    next[stid].size += 1;
    if (next[stid].size > top->core->configs.block_rob_depth) {
        cerr<<"Cycle "<<dec<<top->GetSim()->getCycles()<<": BROB size = "<<next[stid].size<<" (buffer size = "<<top->core->configs.block_rob_depth<<")"<<endl;
        ASSERT(0 && "Block Alloc: BROB Overflow!");
    }
    // register header entry
    ROBID current_pos = current[stid].allocPtr;
    for(uint32_t i = 0; i <pos; i++){
        IncROBID(current_pos, next[stid].entry.size());
    }
    header->bSeq = current_pos;
    header->bid = current_pos;
    // For pre-peDecode
    next[stid].entry[current_pos.val].header = header;
    cmd->bid = current_pos;
    cmd->execMachineId = top->machineId;
    GetSim()->GetViewManager(stid)->ResetBlockDepInfo(cmd->bid.val);
    GetSim()->GetViewManager(stid)->ResetBlockPipeInfo(cmd->bid.val, cmd->bpc);
    GetSim()->GetVerifyManager(stid)->ResetBlockInfo(cmd->bid.val, cmd->bpc);
    next[stid].entry[current_pos.val].blockCMD = cmd;
    next[stid].entry[current_pos.val].peID = header->peid;
    if (cmd->blockType == BlockType::BLK_TYPE_SYS) {
        setSysBlockStatus(stid, true);
    }
    if (next[stid].entry[current_pos.val].status == BlockStatus::PAR_RUNNING) {
        next[stid].size--;
    }
    if (next[stid].entry[current_pos.val].status != BlockStatus::RUNNING) {
        next[stid].entry[current_pos.val].status = BlockStatus::ALLOCATED;
    }

    next[stid].entry[current_pos.val].id = current_pos;

    top->stats->slotsBrobAllocated++;
    // for (size_t i = 0; i < top->GetSim()->core->peArray.size(); i++) {
    //     shared_ptr<OPE> ope = dynamic_pointer_cast<OPE>(top->GetSim()->core->peArray[i]);
    //     ope->ResetRetInstCnt(current_pos.val);
    // }

    if (top->core->pTracer->IsEnabled()) {
        top->core->pTracer->Push(current_pos, make_shared<BlockDumpInfo>(cmd));
        top->core->pTracer->PushBlockEvent(current_pos, BlockEvent::ALLOC);
    }
    return current_pos;
}

void reportBlkMinstsByType(BCtrlStats *stats, const BHeader &header) {
    if (header->biqType == BIQType::CUBE_IQ) {
        ++stats->rtiredCubeBlockCnt;
    } else if (header->biqRealType == BIQType::VEC_IQ) {
        ++stats->rtiredVcallBlockCnt;
    } else if (header->biqRealType == BIQType::VET_IQ) {
        ++stats->rtiredTmplBlockCnt;
    } else if (header->biqRealType == BIQType::MTC_IQ) {
        ++stats->rtiredMcallBlockCnt;
    } else if (header->biqRealType == BIQType::TMA_IQ || header->biqRealType == BIQType::TAU_IQ) {
        ++stats->rtiredTMABlockCnt;
    }
    switch (header->type) {
        case BlockType::BLK_TYPE_STD:
            ++stats->rtiredHdrStd;
            stats->rtiredHdrStdMinsts += header->realSize;
            break;
        case BlockType::BLK_TYPE_SYS:
            ++stats->rtiredHdrSys;
            stats->rtiredHdrSysMinsts += header->realSize;
            break;
        case BlockType::BLK_TYPE_FP:
            ++stats->rtiredHdrFp;
            stats->rtiredHdrFpMinsts += header->realSize;
            break;
        case BlockType::BLK_TYPE_MPAR:
        case BlockType::BLK_TYPE_MSEQ:
        case BlockType::BLK_TYPE_VPAR:
        case BlockType::BLK_TYPE_VSEQ:
        case BlockType::BLK_TYPE_TEPL:
        case BlockType::BLK_TYPE_TMA:
        case BlockType::BLK_TYPE_CUBE:
            ++stats->rtiredHdrSimt;
            stats->rtiredHdrSimtMinsts += header->realSize;
            break;
        case BlockType::BLK_TYPE_INVAL:
            ++stats->rtiredHdrOther;
            stats->rtiredHdrOtherMinsts += header->realSize;
            break;
        default:
            cout << "Header type is " << static_cast<int>(header->type) << endl;
            ASSERT(0 && "not support!");
    }
}

void reportTemplateBlkBinstsByType(BCtrlStats *stats, const BHeader &header) {
    if (header->opcode == Opcode::OP_INVALID) {
        return;
    }
    switch (header->opcode) {
        case Opcode::OP_MSET:
            ++stats->rtiredHdrCtMset;
            stats->rtiredHdrCtMsetMinsts += header->realSize;
            break;
        case Opcode::OP_MCOPY:
            ++stats->rtiredHdrCtMcopy;
            stats->rtiredHdrCtMcopyMinsts += header->realSize;
            break;
        case Opcode::OP_FENTRY:
            ++stats->rtiredHdrCtFentry;
            stats->rtiredHdrCtFentryMinsts += header->realSize;
            break;
        case Opcode::OP_FEXIT:
            ++stats->rtiredHdrCtFexit;
            stats->rtiredHdrCtFexitMinsts += header->realSize;
            break;
        case Opcode::OP_FRET_RA:
        case Opcode::OP_FRET_STK:
            ++stats->rtiredHdrCtFret;
            stats->rtiredHdrCtFretMinsts += header->realSize;
            break;
        default:
            cout << "Block opcode is " << static_cast<int>(header->opcode) << endl;
            ASSERT(0 && "Not support");
    }
}

void reportBlkMinstsByBranch(BCtrlStats *stats, const BHeader &header) {
     switch (header->branchType) {
        case BranchType::BLK_BR_FALL:
            stats->rtiredHdrFallMinst += header->realSize;
            break;
        case BranchType::BLK_BR_COND:
            stats->rtiredHdrCondMinst += header->realSize;
            break;
        case BranchType::BLK_BR_DIRECT:
            stats->rtiredHdrDirMinst += header->realSize;
            break;
        case BranchType::BLK_BR_IND:
            stats->rtiredHdrInDirMinst += header->realSize;
            break;
        case BranchType::BLK_BR_CALL:
            stats->rtiredHdrCallMinst += header->realSize;
            break;
        case BranchType::BLK_BR_ICALL:
            stats->rtiredHdrInCallMinst += header->realSize;
            break;
        case BranchType::BLK_BR_RET:
            stats->rtiredHdrRetMinst += header->realSize;
            break;
        default:
            cout << "Branch Type is " << static_cast<int>(header->branchType) << endl;
            ASSERT(0 && "Not support");
    }
}

static bool BlockNoNeedMInst(const BHeader& header)
{
    return header->tileOp == TileOp::TMOV ||
           header->biqType == BIQType::TAU_IQ ||
           header->biqType == BIQType::TMA_IQ ||
           header->biqType == BIQType::CUBE_IQ ||
           header->size == 0;
}

static void ReportBlkMinstsCnt(BCtrlStats *stats, const BHeader &header)
{
    if (!header) {
        return;
    }
    stats->moveInsts += header->moveCnt;

    // Blocks that bypass PE execution won't have instCommitStats
    // This includes: TMOV, skip_* modes, and size-0 dst tiles
    if (header->instStats.instCommitStats == nullptr) {
        ASSERT(BlockNoNeedMInst(header));
        return;
    }

    for (auto &it : header->instStats.instCommitStats->instGroupCommitInstNum) {
        stats->groupInstMap[static_cast<uint64_t>(it.first)] += it.second;
    }
}

static void ReportBlkLocalRegStats(BCtrlStats *stats, const BHeader &header)
{
    if (!header) {
        return;
    }
    const uint64_t linkMaxSize = stats->tRegSrcCnt.size();
    auto &regStats = header->instStats.instRelativeRegStats;
    for (uint64_t i = 0; i < linkMaxSize; ++i) {
        stats->tRegSrcCnt[i] += regStats->regRelativeDist[OperandType::OPD_TLINK][i];
        stats->uRegSrcCnt[i] += regStats->regRelativeDist[OperandType::OPD_ULINK][i];
        stats->vtRegSrcCnt[i] += regStats->regRelativeDist[OperandType::OPD_VTLINK][i];
        stats->vuRegSrcCnt[i] += regStats->regRelativeDist[OperandType::OPD_VULINK][i];
        stats->vmRegSrcCnt[i] += regStats->regRelativeDist[OperandType::OPD_VMLINK][i];
        stats->vnRegSrcCnt[i] += regStats->regRelativeDist[OperandType::OPD_VNLINK][i];
        stats->tRegSrcRobDist[i] += regStats->regRobDist[OperandType::OPD_TLINK][i];
        stats->uRegSrcRobDist[i] += regStats->regRobDist[OperandType::OPD_ULINK][i];
        stats->vtRegSrcRobDist[i] += regStats->regRobDist[OperandType::OPD_VTLINK][i];
        stats->vuRegSrcRobDist[i] += regStats->regRobDist[OperandType::OPD_VULINK][i];
        stats->vmRegSrcRobDist[i] += regStats->regRobDist[OperandType::OPD_VMLINK][i];
        stats->vnRegSrcRobDist[i] += regStats->regRobDist[OperandType::OPD_VNLINK][i];
    }
}

static void GetCyclesInfo(BlockPipeViewPtr blockInfo, const FullBlockHeader &header)
{
    blockInfo->cycleInfo = std::make_shared<CycleInfo>();
    blockInfo->cycleInfo->createCycle = header.createCycle;
    if (header.createCycle == 0 && header.f1Cycle > 0) {
        blockInfo->cycleInfo->createCycle = header.f1Cycle - 1;
    }
    blockInfo->cycleInfo->f1Cycle = header.f1Cycle;
    if (header.f1Cycle == 0 && header.createCycle > 0) {
        blockInfo->cycleInfo->f1Cycle = header.createCycle + 1;
    }
    blockInfo->cycleInfo->fetchCycle = header.fetchCycle;
    blockInfo->cycleInfo->fetchReturnCycle = header.fetchReturnCycle;
    blockInfo->cycleInfo->f3Cycle = header.f3Cycle;
    blockInfo->cycleInfo->decodeCycle = header.decodeCycle;
    blockInfo->cycleInfo->renameCycle = header.renameCycle;
    blockInfo->cycleInfo->renamedCycle = header.renamedCycle;
    blockInfo->cycleInfo->dispatchCycle = header.dispatchCycle;
    blockInfo->cycleInfo->iqCycle = header.iqCycle;
    blockInfo->cycleInfo->p1Cycle = header.issue_cycle;
    blockInfo->cycleInfo->e1Cycle = header.exe_cycle;
    blockInfo->cycleInfo->completeCycle = header.completeCycle;
}

bool BlockROB::BlockCOMPLETED(BROBEntry &nEntry, BROBEntry &bentry, ROBID &cPtr, uint32_t stid)
{
    //FIXME::check fakeLSU
    if (nEntry.header->type == BlockType::BLK_TYPE_XB && nEntry.header->branchType == BranchType::BLK_BR_CALL) {
        GetSim()->refInfo.retireEcall(nEntry.header->bpc);
        GetSim()->systemStatus.ecallStatus = EcallStatus::COMMITTED;
    }
    // 由于stackrename的优化，load指令不会去lsu，所以他在这里滑动load窗口
    if (bentry.header->opcode == Opcode::OP_FEXIT || bentry.header->opcode == Opcode::OP_FRET_RA ||
        bentry.header->opcode == Opcode::OP_FRET_STK) {
        top->core->iex[SCALAR_IEX]->iq.windowSlides(bentry.header->stackGetCnt, true);
    }
    bentry.header->stackGetCnt = 0;
    if (IsBlockTypeParallel(bentry.header->type)) {
        top->blockIssueQueueUnit.ReportBlockRetired(bentry.blockCMD);
    }

    GetSim()->GetVerifyManager(bentry.blockCMD->stid)->RecordBlockCompleted(nEntry.id.val);
    LOG_INFO_M(Unit::BCC, Stage::RE) << "Block " << dec << cPtr << " retires. Remain rob size "
                                     << dec << (next[stid].size - 1);
    IncROBID(next[stid].commitPtr, next[stid].entry.size());
    ASSERT(next[stid].size>0);
    next[stid].size -= 1;
    if (next[stid].size>=top->core->configs.block_rob_depth) {
        ASSERT(0 && "Block Reorder Buffer Overflow!");
    }
    nEntry.status = BlockStatus::FREE;
    nEntry.blockCMD = nullptr;
    nEntry.header->vld = false;
    nEntry.header->resolveTargetVld = false;
    if (nEntry.header->type == BlockType::BLK_TYPE_SYS) {
        setSysBlockStatus(stid, false);
    }
    //add to stats
    top->stats->binsts++;
    // slot_minst_retired equals to minst retired, except CT insts and bstart
    top->stats->slot_minst_retired += (nEntry.header->realSize + nEntry.header->auxHdrCnt - 1);

    if (IsBlockTypeParallel(nEntry.header->type)) {
        ++top->stats->retired_tileop;
        switch (nEntry.header->biqType) {
            case BIQType::VEC_IQ:
                ++top->stats->retired_tileop_vec;
                top->core->vectorTop->SetPeRetireIns(nEntry.header->coreId, nEntry.header->realSize);
                break;
            case BIQType::VET_IQ:
                ++top->stats->retired_tileop_vec;
                break;
            case BIQType::CUBE_IQ:
                ++top->stats->retired_tileop_cube;
                break;
            case BIQType::MTC_IQ:
                ++top->stats->retired_tileop_mtc;
                break;
            case BIQType::TMA_IQ:
            case BIQType::TAU_IQ:
                ++top->stats->retired_tileop_tma;
                break;
            default:
                break;
        }
    }

    // 放到各自PE/core 自己统计。SPE/VPE 行为不同；
    // top->core->peArray[nEntry.peID]->stats->binsts_commit++;
    if (nEntry.header->resolveTaken) {
        // [Optimization] When it is taken, fetch a header as bstop. So add 1
        ++nEntry.header->realSize;
    }
    top->stats->minsts += nEntry.header->realSize;
    // [Optimization] bstart is not send to PE, so add 1.
    if (!IsBlockTypeParallel(bentry.header->type)) {
        if (nEntry.header->isSplitBlk) {
            // for split block
            ++top->stats->minsts;
        }
        top->stats->minsts += nEntry.header->auxHdrCnt;
    } else if (bentry.header->isLastHeader) {
        top->stats->minsts += nEntry.header->auxSimtHdrCnt + nEntry.header->auxHdrCnt;
    }
    if (top->core->configs.hint_warmup_enable && nEntry.header->warmupHint) {
        top->core->OnWarmupFinish();
    }
    // 重置当前块，数据统计信息
    top->core->peArray[nEntry.peID]->ResetBlockInstStats(nEntry.header->bid.val);
    reportBlkMinstsByType(top->stats, nEntry.header);
    reportBlkMinstsByBranch(top->stats, nEntry.header);
    reportTemplateBlkBinstsByType(top->stats, nEntry.header);
    ReportBlkMinstsCnt(top->stats, nEntry.header);

    if (!IsBlockTypeParallel(bentry.header->type) || bentry.header->isLastHeader) {
        top->RetireBlock(bentry.header->renameBid, bentry.header->stid);
    }
    top->blockFetchUnit.retireBlock(bentry.header);
    if (bentry.header->biqType == BIQType::VEC_IQ || bentry.header->biqType == BIQType::MTC_IQ) {
        ReportBlkLocalRegStats(top->stats, nEntry.header);
    }

    //Retire rename ptags
    top->scalarPE->d2Stage.ggprRenameUnit.RetireBlock(cPtr, nEntry.header->stid);
    block_rtable_retire_q->Write(cPtr);
    // if (!(bentry.header->size == 0 && !bentry.header->link && bentry.header->type != BlockType::TEMPLATE)
    if (!(bentry.header->size == 0 && !bentry.header->link)
        && top->core->configs.ca_light_core && !top->core->configs.parallel_decode) {
        IDBus bus = IDBus();
        bus.bid = cPtr;
        blcok_lane_retire_q->Write(bus);
    }
    top->core->sRenameUnit->setBlockRetire(cPtr);
    if (GetSim()->GetViewManager(stid)->config.printPipeView) {
        BlockPipeViewPtr blockInfo = std::make_shared<BlockPipeViewInfo>();
        blockInfo->bid = cPtr.val;
        blockInfo->machineId = current[stid].entry[cPtr.val].blockCMD->execMachineId;
        GetCyclesInfo(blockInfo, *current[stid].entry[cPtr.val].header);
        blockInfo->cycleInfo->retireCycle = GetSim()->getCycles() + 1;
        blockInfo->label = current[stid].entry[cPtr.val].DumpPipeViewInfo();
        GetSim()->GetViewManager(stid)->RecordBlockCompleted(blockInfo);
    }

    if (top->core->pTracer->IsEnabled()) {
        top->core->pTracer->PushBlockEvent(cPtr, BlockEvent::COMMIT);
    }

    IncROBID(cPtr, current[stid].entry.size());

    //reset stats if warmup block number is reached
    if (!top->core->warmupFinished && (top->stats->binsts >= top->core->warmupBlockNum &&
        top->stats->minsts >= top->core->warmupMinstNum)) {
        top->core->warmupFinished = true;
        top->core->OnWarmupFinish();
        return false;
    }

    if (bentry.header->resolveMisPredict) {
        return false;
    }
    return true;
}

void BlockROB::ReportLocalRegBlockCommit(ROBID bid, uint32_t stid)
{
    top->scalarPE->d2Stage.ReportSGPRBlockCommit(bid, stid);
}

void BlockROB::commitBlock(uint32_t stid) {
    ROBID cPtr = current[stid].commitPtr;
    ++top->stats->retired_tileop_cycles;
    for (uint32_t i = 0; i < top->configs.bctrl_bandwidth && i < current[stid].size; ++i) {
        BROBEntry &bentry = current[stid].entry[cPtr.val];
        BROBEntry &nEntry = next[stid].entry[cPtr.val];
        if (bentry.header->type == BlockType::BLK_TYPE_XB && bentry.header->branchType == BranchType::BLK_BR_CALL &&
            GetSim()->systemStatus.ecallStatus == EcallStatus::DECODE) {
            GetSim()->systemStatus.ecallStatus = EcallStatus::EXECUTE;
        }
        if (GetSim()->correctBCount + i >= GetSim()->maxBCount) {
            nEntry.status = BlockStatus::TERMINATE;
            break;
        }

        if (nEntry.header && IsBlockTypeParallel(nEntry.header->type)) {
            switch (nEntry.header->biqType) {
                case BIQType::VEC_IQ:
                    ++top->stats->retired_tileop_vec_cycles;
                    break;
                case BIQType::VET_IQ:
                    ++top->stats->retired_tileop_vec_cycles;
                    break;
                case BIQType::CUBE_IQ:
                    ++top->stats->retired_tileop_cube_cycles;
                    break;
                case BIQType::MTC_IQ:
                    ++top->stats->retired_tileop_mtc_cycles;
                    break;
                case BIQType::TMA_IQ:
                case BIQType::TAU_IQ:
                    ++top->stats->retired_tileop_tma_cycles;
                    break;
                default:
                    break;
            }
        }

        if (bentry.status == BlockStatus::TERMINATE) {
            // TODO: gfsim terminate(bsize=0) and system terminate(minst trap)
            GetSim()->terminate[stid] = true;
            setTerminateStatus(stid, false);
            top->SMTTerminate(stid);
            LOG_INFO_M(Unit::BCC, Stage::CT) << "whole program terminates, STID: " << dec << stid;
            break;
        } else if (bentry.status == BlockStatus::EXCEPTION) {
            cerr << "Fault at cycle " << dec << GetSim()->getCycles() << ", correctBCount "<<GetSim()->correctBCount;
            cerr << " bid " << dec << cPtr.val;
            if (bentry.header) {
                cerr << " " << bentry.header;
            }
            if (!bentry.exceptionInfo.empty()) {
                cerr << "exception info: " << bentry.exceptionInfo << endl;
            }
            cerr << endl;
            ASSERT(0 && "Block Commit Exception!");
        } else if (bentry.status == BlockStatus::COMPLETED) {
            if (!BlockCOMPLETED(nEntry, bentry, cPtr, stid)) {
                break;
            }
            LOG_INFO_M(Unit::BCC, Stage::CT) << "BID " << bentry.id << " commited. stid " << stid ;
            continue;
        }
        break;
    }
}

void BlockROB::setNeedFlush(ROBID &bid, uint32_t stid)
{
    if (next[stid].entry[bid.val].status == BlockStatus::EXCEPTION) {
        return;
    }
    next[stid].entry[bid.val].status = BlockStatus::NEEDFLUSH;
}

static bool IsNeedRecover(BlockStatus status)
{
    return status == BlockStatus::COMPLETED || status == BlockStatus::NEEDFLUSH || status == BlockStatus::FLUSHED;
}

void BlockROB::setFlushed(FlushBus &flushReq) {
    uint32_t stid = flushReq.req.stid;
    BROBEntry &bentry = next[stid].entry[flushReq.req.bid.val];
    // TODO: Fix next alloc
    flushReq.old_alloc = next[stid].allocPtr;
    if (next[stid].entry[next[stid].allocPtr.val].status == BlockStatus::PAR_RUNNING) {
        IncROBID(flushReq.old_alloc, next[stid].entry.size());
    }
    if (getOldestBlockID(stid) == flushReq.req.bid || flushReq.req.immediateFlush) {
        next[stid].entry[next[stid].commitPtr.val].status = BlockStatus::RUNNING;
    }

    // Reset the status
    ROBID ptr = next[stid].commitPtr;
    // TODO: Only support simt mode, need to support for vector/loop mode.
    if (top->core->IsVectorIex(flushReq.req.iexTyp) || (flushReq.req.iexTyp == MEM_IEX)) {
        ptr = next[stid].commitPtr;
        IncROBID(ptr, next[stid].entry.size());
        for (uint32_t i = 1; i < next[stid].size; ++i) {
            if (!next[stid].entry[ptr.val].header) {
                break;
            }
            if (flushReq.baseOnPE && flushReq.req.peID != next[stid].entry[ptr.val].header->peid) {
                IncROBID(ptr, next[stid].entry.size());
                continue;
            }

            if (LessEqual(flushReq.req.bid, ptr) && IsNeedRecover(next[stid].entry[ptr.val].status)) {
                next[stid].entry[ptr.val].status = BlockStatus::RUNNING;
            }
            IncROBID(ptr, next[stid].entry.size());
        }
        return;
    }

    bentry.header->stackGetCnt = 0;

    //perform global nuke flush
    ptr = next[stid].commitPtr;
    IncROBID(ptr, next[stid].entry.size());
    ROBID nextPtr = ptr;    // commitPtr + 1
    // Flush if not baseOnBid
    GetSim()->GetVerifyManager(stid)->Flush(flushReq, this->getOldestBlockID(stid).GetVal());
    for (uint32_t i = 0; i < next[stid].entry.size() - 1; ++i) {
        if (!next[stid].entry[ptr.val].header) {
            continue;
        }
        if (flushReq.baseOnPE && flushReq.req.peID != next[stid].entry[ptr.val].header->peid) {
            IncROBID(ptr, next[stid].entry.size());
            continue;
        }
        if (LessEqual(flushReq.req.bid, ptr)) {
            if (next[stid].entry[ptr.val].header && next[stid].entry[ptr.val].header->type == BlockType::BLK_TYPE_SYS) {
                setSysBlockStatus(stid, false);
            }
            if (next[stid].entry[ptr.val].status == BlockStatus::EXCEPTION) {
                setExcpStatus(stid, false);
            }
            if (next[stid].entry[ptr.val].status == BlockStatus::TERMINATE) {
                setTerminateStatus(stid, false);
            }

            if (next[stid].entry[ptr.val].status == BlockStatus::ALLOCATED ||
                next[stid].entry[ptr.val].status == BlockStatus::DISPATCHED ||
                next[stid].entry[ptr.val].status == BlockStatus::RENAMING ||
                next[stid].entry[ptr.val].status == BlockStatus::RENAMED ||
                next[stid].entry[ptr.val].status == BlockStatus::ISSUED ||
                next[stid].entry[ptr.val].status == BlockStatus::RUNNING ||
                next[stid].entry[ptr.val].status == BlockStatus::PAR_RUNNING ||
                next[stid].entry[ptr.val].status == BlockStatus::COMPLETED ||
                next[stid].entry[ptr.val].status == BlockStatus::TERMINATE ||
                next[stid].entry[ptr.val].status == BlockStatus::MISPRED ||
                next[stid].entry[ptr.val].status == BlockStatus::NEEDFLUSH ||
                next[stid].entry[ptr.val].status == BlockStatus::EXCEPTION) {

                ASSERT(next[stid].size>0);
                --next[stid].size;
            }

            if (GetSim()->perfectGetSet) {
                GetSim()->refInfo.refTrace.entry[ptr.val].resetGSRPtr();
            }
            if (GetSim()->perfectLoadStore) {
                GetSim()->refInfo.refTrace.entry[ptr.val].resetLSRPtr();
                GetSim()->recoverBlockRefStq(ptr.val);
            }

            next[stid].entry[ptr.val].status = BlockStatus::FLUSHED;
            GetSim()->GetViewManager(stid)->ResetBlockPipeInfo(ptr.val, 0);
            if (flushReq.baseOnBid) {
                GetSim()->GetVerifyManager(stid)->ResetBlockInfo(ptr.val, 0);
            }
            if (next[stid].entry[ptr.val].header && next[stid].entry[ptr.val].header->type == BlockType::BLK_TYPE_XB &&
                next[stid].entry[ptr.val].header->branchType == BranchType::BLK_BR_CALL) {
                GetSim()->systemStatus.ecallStatus = EcallStatus::NONE;
            }
            if (LessROBID(ptr, next[stid].sbarPtr) && next[stid].entry[ptr.val].header &&
                top->core->configs.BSB_enable && next[stid].entry[ptr.val].header->sbarCertain) {
                ASSERT(next[stid].sbarID>=next[stid].entry[ptr.val].header->storeCount);
                next[stid].sbarID -= next[stid].entry[ptr.val].header->storeCount;
            }
        }
        IncROBID(ptr, next[stid].entry.size());
    }

    if (getOldestBlockID(stid) == flushReq.req.bid || flushReq.req.immediateFlush) {
        // Optimization for nuke flush
        // set current block running
        next[stid].entry[next[stid].commitPtr.val].header->getBitMask =
                                                    next[stid].entry[next[stid].commitPtr.val].header->inputRegMask;
        next[stid].entry[next[stid].commitPtr.val].header->setBitMask =
                                                    next[stid].entry[next[stid].commitPtr.val].header->outputRegMask;
        //reajust alloc/rename ptr
        if (flushReq.req.iexTyp == SCALAR_IEX) {
            next[stid].allocPtr = nextPtr;
            if (flushReq.req.immediateFlush) {
                next[stid].entry[flushReq.req.bid.val].status = BlockStatus::RUNNING;
                next[stid].entry[flushReq.req.bid.val].header->getBitMask = next[stid].entry[flushReq.req.bid.val].header->inputRegMask;
                next[stid].entry[flushReq.req.bid.val].header->setBitMask = next[stid].entry[flushReq.req.bid.val].header->outputRegMask;
                //reajust alloc/rename ptr
                next[stid].allocPtr = flushReq.req.bid;
                IncROBID(next[stid].allocPtr, next[stid].entry.size());
                ++next[stid].size;
            }
        }

        if (GetSim()->perfectGetSet && flushReq.baseOnBid) {
            GetSim()->refInfo.refTrace.entry[getOldestBlockID(stid).val].resetGSRPtr();
        }

        if (GetSim()->perfectLoadStore && flushReq.baseOnBid) {
            GetSim()->refInfo.refTrace.entry[getOldestBlockID(stid).val].resetLSRPtr();
            GetSim()->recoverBlockRefStq(getOldestBlockID(stid).val);
        }

        // To set BPC from nuke flush signal.(next)
        if (!flushReq.baseOnBid && bentry.header->resolveTargetVld && bentry.header->machineInst) {
            bentry.header->machineInst->bfuInfo->resolved = true;
            bentry.header->machineInst->bfuInfo->resolve_tgt = bentry.header->resolveTarget;
            bentry.header->machineInst->bfuInfo->resolve_taken = bentry.header->resolveTaken;
            bentry.header->machineInst->bfuInfo->resolve_mispredict = bentry.header->resolveMisPredict;
            bentry.header->machineInst->bfuInfo->predict_taken = bentry.header->predictTaken;
            bentry.header->machineInst->bfuInfo->predict_tgt = bentry.header->predictTarget;
        }

        if (flushReq.req.noSplitBlk && !flushReq.baseOnBid) {
            NS_CORE::NukeInfo nukeInfo;
            nukeInfo.fbid = flushReq.req.fbid;
            nukeInfo.fbid_local = flushReq.req.fbid_local;
            nukeInfo.pc = flushReq.req.fetchTPC;
            nukeInfo.stid = flushReq.req.stid;
            if (top->core->configs.bp_mode==0) {
                top->blockFetchUnit.be_bfu_nuke_info_q.Write(nukeInfo);
            }
            if (LessROBID(flushReq.req.bid, next[stid].sbarPtr)) {
                next[stid].sbarPtr = flushReq.req.bid;
                IncROBID(next[stid].sbarPtr, next[stid].entry.size());
            }
            return;
        }
        if (flushReq.req.iexTyp == SCALAR_IEX) {
            top->core->nukeMHdr = bentry.header->machineInst;
        }
    } else {
        // reajust alloc/rename ptr
        ROBID id = flushReq.req.bid;

        // To set BPC from nuke flush signal.(current)
        SubROBID(id, 1, top->core->configs.block_rob_depth);
        BROBEntry &entry = next[stid].entry[id.val];
        if (entry.header->resolveTargetVld && entry.header->machineInst) {
            entry.header->machineInst->bfuInfo->resolved = true;
            entry.header->machineInst->bfuInfo->resolve_tgt = entry.header->resolveTarget;
            entry.header->machineInst->bfuInfo->resolve_taken = entry.header->resolveTaken;
            entry.header->machineInst->bfuInfo->resolve_mispredict = entry.header->resolveMisPredict;
            entry.header->machineInst->bfuInfo->predict_taken = entry.header->predictTaken;
            entry.header->machineInst->bfuInfo->predict_tgt = entry.header->predictTarget;
        }
        if (flushReq.req.iexTyp == SCALAR_IEX) {
            next[stid].allocPtr = flushReq.req.bid;
            top->core->nukeMHdr = entry.header->machineInst;
        }
    }
    if (LessROBID(flushReq.req.bid, next[stid].sbarPtr) && flushReq.req.iexTyp == SCALAR_IEX) {
        next[stid].sbarPtr = flushReq.req.bid;
        IncROBID(next[stid].sbarPtr, next[stid].entry.size());
    }
}

void BlockROB::deliveryStoreID(uint32_t stid) {
    if (!top->core->configs.BSB_enable) {
        return;
    }
    ROBID sbarPtr = current[stid].sbarPtr;
    uint64_t sbarID = current[stid].sbarID;
    for (uint32_t i = 0; i < next[stid].entry.size(); i++) {
        if (next[stid].entry[sbarPtr.val].id != sbarPtr) {
            break;
        }
        BROBEntry &bentry = next[stid].entry[sbarPtr.val];
        if (bentry.status == BlockStatus::FREE || bentry.status == BlockStatus::FLUSHED ||
            bentry.status == BlockStatus::MISPRED || bentry.status == BlockStatus::EXCEPTION) {
            break;
        }
        bentry.header->storeStart = true;
        bentry.header->startSID = sbarID;
        if (!bentry.header->sbarCertain) {
            break;
        }
        // cout<<"debug: "<<dec<<sbarPtr.val<<" "<<bentry.header->startSID<<" "<<bentry.header->storeCount<<endl;
        sbarID += bentry.header->storeCount;
        IncROBID(sbarPtr, next[stid].entry.size());
    }
    next[stid].sbarPtr = sbarPtr;
    next[stid].sbarID = sbarID;
}

void BlockROB::setStoreCount(ROBID bid, uint64_t storeCount, uint32_t stid) {
    if (!next[stid].entry[bid.val].header || next[stid].entry[bid.val].header->sbarCertain) {
        return;
    }
    next[stid].entry[bid.val].header->sbarCertain = true;
    next[stid].entry[bid.val].header->storeCount = storeCount;
}

void BlockROB::exeBlock(ROBID bid, BlockStatus status, uint32_t stid) {
    if (next[stid].entry[bid.val].status == BlockStatus::EXCEPTION ||
        next[stid].entry[bid.val].status == BlockStatus::NEEDFLUSH ||
        next[stid].entry[bid.val].status == BlockStatus::FLUSHED ||
        next[stid].entry[bid.val].status == BlockStatus::MISPRED ||
        next[stid].entry[bid.val].status == BlockStatus::FREE) {
        return;
    }

    next[stid].entry[bid.val].header->exe_cycle = GetSim()->getCycles();
    next[stid].entry[bid.val].status = status;
}

void BlockROB::resolveBlock() {
    for (auto &it: iex_brob_rslvblk) {
        while (!it->Empty()) {
            const ResolveBlockBus &bus = it->Read();
            resolveBlock(bus.idBus, bus.misPred, bus.resolveTaken, bus.resolveTarget, bus.idBus.stid);
        }
    }
}

void BlockROB::resolveBlock(const IDBus &idBus, bool misPred, bool resolveTaken, uint64_t resolveTarget, uint32_t stid)
{
    if (next[stid].entry[idBus.bid.val].status == BlockStatus::FLUSHED) {
        return;  // block flushed
    }
    if (next[stid].entry[idBus.bid.val].status == BlockStatus::EXCEPTION) {
        return;
    }
    if (next[stid].entry[(idBus.bid.val+1) % next[stid].entry.size()].status == BlockStatus::MISPRED) {
        return;
    }

    LOG_INFO_M(Unit::BCC, Stage::D1) << "BROB resolve block " << dec << idBus.bid.val;
    ASSERT(next[stid].entry[idBus.bid.val].header);
    next[stid].entry[idBus.bid.val].header->resolveTargetVld = true;
    next[stid].entry[idBus.bid.val].header->resolveMisPredict = misPred;
    next[stid].entry[idBus.bid.val].header->resolveTaken = resolveTaken;
    next[stid].entry[idBus.bid.val].header->resolveTarget = resolveTarget;
    // update predictTarget to avoid multi-misprediction at one block
    next[stid].entry[idBus.bid.val].header->predictTaken = resolveTaken;
    next[stid].entry[idBus.bid.val].header->predictTarget = resolveTarget;
    if (!misPred) {
        top->blockFetchUnit.resolveBlock(next[stid].entry[idBus.bid.val].header);
    }

    if (top->core->configs.bp_mode==1 || top->core->configs.bp_mode==2) {
        return;
    }

    // Block misprediction, mark next block as mis-predicted
    // This performs a merge if there are multiple mispredicted blocks
    if (misPred) {
        ROBID mBID = idBus.bid;
        IncROBID(mBID, current[stid].entry.size());
        if (mBID.val != current[stid].commitPtr.val) {
            if (next[stid].entry[mBID.val].header &&
                next[stid].entry[mBID.val].header->type == BlockType::BLK_TYPE_SYS) {
                setSysBlockStatus(stid, false);
            }
            if (next[stid].entry[mBID.val].status == BlockStatus::EXCEPTION) {
                setExcpStatus(stid, false);
            }
            if (next[stid].entry[mBID.val].status == BlockStatus::TERMINATE) {
                setTerminateStatus(stid, false);
            }

            if (current[stid].entry[mBID.val].status == BlockStatus::DISPATCHED ||
                current[stid].entry[mBID.val].status == BlockStatus::RENAMING ||
                current[stid].entry[mBID.val].status == BlockStatus::RENAMED ||
                current[stid].entry[mBID.val].status == BlockStatus::ISSUED ||
                current[stid].entry[mBID.val].status == BlockStatus::RUNNING ||
                current[stid].entry[mBID.val].status == BlockStatus::PAR_RUNNING ||
                current[stid].entry[mBID.val].status == BlockStatus::ALLOCATED ||
                current[stid].entry[mBID.val].status == BlockStatus::COMPLETED ||
                current[stid].entry[mBID.val].status == BlockStatus::TERMINATE ||
                current[stid].entry[mBID.val].status == BlockStatus::NEEDFLUSH ||
                current[stid].entry[mBID.val].status == BlockStatus::EXCEPTION) {
                next[stid].entry[mBID.val].status = BlockStatus::MISPRED;
            }
        }
        // reajust alloc ptr
        FlushReq req;
        // generate flush request
        req.vld = true;
        req.bid = mBID;
        req.rid.val = 0;
        req.stid = idBus.stid;
        req.iexTyp = idBus.iexTyp;
        /* Considering this is a block misprediction, it is independent of the thread. */
        req.tid = 0;
        req.type = FlushType::MISS_PRED_FLUSH;
        top->core->flushUnit->flush_stats->InterBlockMisprediction++;
        top->core->flushUnit->flush_stats->smtInterBlockMispredictionArray[stid]++;
        bcc_flush_rpt_q->Write(req);

        if (top->core->pTracer->IsEnabled()) {
            top->core->pTracer->PushBlockEvent(mBID, BlockEvent::MISPRED);
        }
        LOG_INFO_M(Unit::BCC, Stage::D1) << "Generate branch mispred flush at Block " << dec << mBID
                                         << ", stid=" << dec << stid;
    }
}

void BlockROB::SetBlockCommand(ROBID bid, BlockCommandPtr cmd)
{
    next[cmd->stid].entry[bid.val].blockCMD = cmd;
}

void BlockROB::completeBlock(ROBID bid, bool empty, uint32_t stid) {
    (void)empty;
    if (next[stid].entry[bid.val].status == BlockStatus::EXCEPTION ||
        next[stid].entry[bid.val].status == BlockStatus::NEEDFLUSH ||
        next[stid].entry[bid.val].status == BlockStatus::FLUSHED ||
        next[stid].entry[bid.val].status == BlockStatus::MISPRED ||
        next[stid].entry[bid.val].status == BlockStatus::FREE ||
        next[stid].entry[bid.val].status == BlockStatus::TERMINATE) {
        return;
    }
    if (next[stid].entry[bid.val].status == BlockStatus::COMPLETED) {
        cout << "wrong complete status, bid:" << dec << bid << endl;
    }
    ASSERT(next[stid].entry[bid.val].status != BlockStatus::COMPLETED);
    next[stid].entry[bid.val].status = BlockStatus::COMPLETED;
    top->blockIssueQueueUnit.SetTileSrcRslv(GetBlockCMDPtr(bid, stid));
    next[stid].entry[bid.val].header->completeCycle = GetSim()->getCycles() + 1;

    BROBEntry &bentry =  next[stid].entry[bid.val];
    bool check = (bentry.header->tileOp != TileOp::MGATHER);
    if (bentry.blockCMD != nullptr && !bentry.blockCMD->dsts.empty() &&
        GetSim()->GetVerifyManager(stid)->config.verifyParBlockTileReg &&
        bentry.blockCMD->dsts[0]->size > 0 && check) {
        for (uint32_t i = 0; i < bentry.blockCMD->dsts.size(); i++) {
            std::shared_ptr<TileRegVerifyData> tileRegData = std::make_shared<TileRegVerifyData>();
            tileRegData->shapeM = bentry.blockCMD->lb0;
            tileRegData->shapeN = bentry.blockCMD->lb1;
            tileRegData->data.resize(bentry.blockCMD->dsts[i]->size, 0);
            top->core->tileReg->GetTileRegData(bentry.blockCMD->dsts[i], tileRegData->data, bentry.blockCMD->stid);
            GetSim()->GetVerifyManager(stid)->RecordBlockTileRegData(bentry.header->bid.val, tileRegData);
        }
    }

    decStatsIssuedBlocks(bid, stid);

    LOG_INFO_M(Unit::BCC, Stage::CT) << "Block B" << dec << bid << "[ST" << stid << "]" << " completed";
}

void BlockROB::SetACC(ROBID bid, uint64_t prevChainID, uint64_t prevAccPtr,
                      uint64_t lastChainID, uint64_t lastAccPtr, uint32_t stid)
{
    ASSERT(lastSetACCBlock[stid] < bid);
    IncROBID(lastSetACCBlock[stid], next[stid].entry.size());
    while (lastSetACCBlock[stid] < bid) {
        next[stid].entry[lastSetACCBlock[stid].val].accChainID = prevChainID;
        next[stid].entry[lastSetACCBlock[stid].val].accTagPtr = prevAccPtr;
        IncROBID(lastSetACCBlock[stid], next[stid].entry.size());
    }
    next[stid].entry[bid.val].accChainID = lastChainID;
    next[stid].entry[bid.val].accTagPtr = lastAccPtr;
    lastSetACCBlock[stid] = bid;
}

uint64_t BlockROB::GetACCChainID(ROBID flushBid, uint32_t stid)
{
    if (flushBid < lastSetACCBlock[stid]) {
        lastSetACCBlock[stid] = flushBid;
    }
    return next[stid].entry[lastSetACCBlock[stid].val].accChainID;
}

uint64_t BlockROB::GetACCTagPtr(ROBID flushBid, uint32_t stid)
{
    if (flushBid < lastSetACCBlock[stid]) {
        lastSetACCBlock[stid] = flushBid;
    }
    return next[stid].entry[lastSetACCBlock[stid].val].accTagPtr;
}

void BlockROB::reportBlockRenaming(ROBID bid, uint32_t stid) {
    if (next[stid].entry[bid.val].status == BlockStatus::EXCEPTION ||
        next[stid].entry[bid.val].status == BlockStatus::NEEDFLUSH ||
        next[stid].entry[bid.val].status == BlockStatus::FLUSHED ||
        next[stid].entry[bid.val].status == BlockStatus::MISPRED ||
        next[stid].entry[bid.val].status == BlockStatus::COMPLETED) {
        return;
    }
    if (next[stid].entry[bid.val].status != BlockStatus::RUNNING &&
        next[stid].entry[bid.val].status != BlockStatus::PAR_RUNNING) {
        next[stid].entry[bid.val].status = BlockStatus::RENAMING;
    }
}

void BlockROB::reportBlockRenamed(ROBID bid, uint32_t stid) {
    if (next[stid].entry[bid.val].status == BlockStatus::EXCEPTION ||
        next[stid].entry[bid.val].status == BlockStatus::NEEDFLUSH ||
        next[stid].entry[bid.val].status == BlockStatus::FLUSHED ||
        next[stid].entry[bid.val].status == BlockStatus::MISPRED ||
        next[stid].entry[bid.val].status == BlockStatus::COMPLETED ||
        next[stid].entry[bid.val].status == BlockStatus::EXCEPTION ||
        next[stid].entry[bid.val].status == BlockStatus::TERMINATE) {
        return;
    }

    if (next[stid].entry[bid.val].status != BlockStatus::RUNNING &&
        next[stid].entry[bid.val].status != BlockStatus::PAR_RUNNING) {
        next[stid].entry[bid.val].status = BlockStatus::RENAMED;
        next[stid].entry[bid.val].header->renamedCycle = GetSim()->getCycles();
    }
    if (next[stid].entry[bid.val].status == BlockStatus::PAR_RUNNING) {
        next[stid].entry[bid.val].status = BlockStatus::RUNNING;
    }
    next[stid].entry[bid.val].header->renameCycle = GetSim()->getCycles();
}

void BlockROB::SimtRecoverd(const FlushBus &flushReq)
{
    if (!top->core->IsVectorIex(flushReq.req.iexTyp) && (flushReq.req.iexTyp != MEM_IEX)) {
        return;
    }

    // Simt inner flush no need to flush any blocks.
    if (flushReq.req.type == FlushType::SIMT_INNER_FLUSH) {
        return;
    }

    uint32_t stid = flushReq.req.stid;
    ROBID ptr = next[stid].commitPtr;
    ROBID recoverBid = flushReq.req.bid;
    IncROBID(recoverBid, next[stid].entry.size());
    for (uint32_t i = 0; i < next[stid].entry.size() && i < next[stid].size; ++i) {
        if (!((LessEqual(recoverBid, ptr) && next[stid].entry[ptr.val].header->peid == flushReq.req.peID) &&
            (next[stid].entry[ptr.val].status == BlockStatus::ALLOCATED ||
            next[stid].entry[ptr.val].status == BlockStatus::DISPATCHED ||
            next[stid].entry[ptr.val].status == BlockStatus::RENAMING ||
            next[stid].entry[ptr.val].status == BlockStatus::RENAMED ||
            next[stid].entry[ptr.val].status == BlockStatus::ISSUED ||
            next[stid].entry[ptr.val].status == BlockStatus::RUNNING ||
            next[stid].entry[ptr.val].status == BlockStatus::PAR_RUNNING ||
            next[stid].entry[ptr.val].status == BlockStatus::COMPLETED ||
            next[stid].entry[ptr.val].status == BlockStatus::TERMINATE ||
            next[stid].entry[ptr.val].status == BlockStatus::MISPRED ||
            next[stid].entry[ptr.val].status == BlockStatus::EXCEPTION))) {
            IncROBID(ptr, next[stid].entry.size());
            continue;
        }

        if (next[stid].entry[ptr.val].status == BlockStatus::NEEDFLUSH) {
            break;
        }

        // Alloc BIQ
        PEControlBus peReq;
        peReq.vld = true;
        peReq.bid = next[stid].entry[ptr.val].header->bSeq;
        peReq.bpc = next[stid].entry[ptr.val].header->bpc;
        peReq.tpc = next[stid].entry[ptr.val].header->payloadPC;
        peReq.fetchTpc = next[stid].entry[ptr.val].header->payloadPC;
        peReq.hsize = next[stid].entry[ptr.val].header->hsize;
        peReq.bsize = next[stid].entry[ptr.val].header->size;
        peReq.isize = WIDTH_32;
        peReq.empty = true;
        peReq.noBranch = !next[stid].entry[ptr.val].header->hyper;
        peReq.blkSrcType = BlkSrcType::BLK_ISSUEQ;
        peReq.getMask = next[stid].entry[ptr.val].header->getBitMask;
        peReq.setMask = next[stid].entry[ptr.val].header->setBitMask;
        peReq.btype = next[stid].entry[ptr.val].header->type;
        peReq.templateIDSet.reset();
        peReq.noSplitBlk = next[stid].entry[ptr.val].header->isInline;
        // top->bcc_pe_req_array[flushReq.req.peID]->Write(peReq);
        next[stid].entry[ptr.val].status = BlockStatus::RUNNING;
        // the inst waitting for recovering cannot be retired because there cannot be the oldest inst in this block.
        ASSERT(next[stid].entry[ptr.val].status != BlockStatus::COMPLETED);
        ASSERT(next[stid].entry[ptr.val].status != BlockStatus::FREE);
        IncROBID(ptr, next[stid].entry.size());
    }
}

bool isNeedReplay(FlushBus &flushReq, BROBEntry &entry)
{
    if (entry.status == BlockStatus::FLUSHED||entry.status == BlockStatus::ALLOCATED ||
        entry.status == BlockStatus::RENAMING || entry.status == BlockStatus::RENAMED ||
        entry.status == BlockStatus::ISSUED) {
        return false;
    }

    if (flushReq.baseOnPE && flushReq.req.peID == entry.peID) {
        bool lessEq = flushReq.baseOnBid ? LessEqual(flushReq.req.bid, entry.id) :
                        LessROBID(flushReq.req.bid, entry.id);
        return lessEq;
    } else if (flushReq.baseOnPE) {
        return false;
    }

    if (LessEqual(flushReq.req.bid, entry.id)) {
        return true;
    }

    return false;
}

void BlockROB::replay(FlushBus &flushReq)
{
    LOG_DEBUG_M(Unit::BCC, Stage::BROB) << "Replay block ";
    ROBID replayBid = flushReq.req.bid;
    std::vector<uint32_t> replayBidVec;
    uint32_t stid = flushReq.req.stid;
    for (uint32_t i = 0; i < next[stid].entry.size(); ++i) {
        uint32_t ptr = replayBid.val;
        IncROBID(replayBid, next[stid].entry.size());
        if (next[stid].entry[ptr].status == BlockStatus::FREE) {
            break;
        }
        if (isNeedReplay(flushReq, next[stid].entry[ptr]) && next[stid].entry[ptr].status != BlockStatus::TERMINATE) {
            if (next[stid].entry[ptr].status == BlockStatus::EXCEPTION) {
                setExcpStatus(stid, false);
            }
            if (next[stid].entry[ptr].status == BlockStatus::PAR_RUNNING) {
                next[stid].entry[ptr].status = BlockStatus::FREE;
                if (next[stid].entry[ptr].header->type == BlockType::BLK_TYPE_XB &&
                    next[stid].entry[ptr].header->branchType == BranchType::BLK_BR_CALL) {
                    GetSim()->systemStatus.ecallStatus = EcallStatus::NONE;
                }
                next[stid].size--;
                continue;
            }
            next[stid].entry[ptr].status = BlockStatus::ISSUED;
            // TODO:: Empty block do not need to replay
            if (next[stid].entry[ptr].header->machineInst &&
                next[stid].entry[ptr].header->branchType != BranchType::BLK_BR_FALL) {
                next[stid].entry[ptr].header->machineInst->bfuInfo->resolved = false;
            }
            if (flushReq.baseOnPE) {
                if (ptr != flushReq.req.bid.val) {
                    next[stid].entry[ptr].header->getBitMask = current[stid].entry[ptr].header->inputRegMask;
                    next[stid].entry[ptr].header->setBitMask = current[stid].entry[ptr].header->outputRegMask;
                }
            } else {
                next[stid].entry[ptr].header->getBitMask = current[stid].entry[ptr].header->inputRegMask;
                next[stid].entry[ptr].header->setBitMask = current[stid].entry[ptr].header->outputRegMask;
            }
            if (GetSim()->perfectGetSet) {
                GetSim()->refInfo.refTrace.entry[ptr].resetGSRPtr();
            }
            if (GetSim()->perfectLoadStore) {
                GetSim()->refInfo.refTrace.entry[ptr].resetLSRPtr();
                GetSim()->recoverBlockRefStq(ptr);
            }
            LOG_DEBUG_M(Unit::BCC, Stage::BROB) << next[stid].entry[ptr].id << " | ";
            replayBidVec.push_back(next[stid].entry[ptr].id.val);
        }
    }

    LOG_DEBUG_M(Unit::BCC, Stage::BROB);
    if (DEBUG_VERBOSE_ON || top->configs.verbose) {
        std::string replayBidStr;
        for (uint32_t bid : replayBidVec) {
            replayBidStr += std::to_string(bid);
            replayBidStr += " | ";
        }
        LOG_DEBUG(top->debugLogger, Unit::BCC, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::LOW,
                  "Replay block %s", replayBidStr.c_str());
    }
}

void BlockROB::IssueBlock(ROBID bid, uint32_t peID, uint32_t stid) {
    if (next[stid].entry[bid.val].status != BlockStatus::RUNNING &&
        next[stid].entry[bid.val].status != BlockStatus::PAR_RUNNING) {
        next[stid].entry[bid.val].status = BlockStatus::ISSUED;
        incStatsIssuedBlocks(bid, stid);
        if (top->core->pTracer->IsEnabled()) {
            top->core->pTracer->PushBlockEvent(bid, BlockEvent::BROB_ISSUE);
        }
    }
    next[stid].entry[bid.val].peID = peID;
    next[stid].entry[bid.val].header->dispatchCycle = GetSim()->getCycles(); // S1
    next[stid].entry[bid.val].header->iqCycle = GetSim()->getCycles() + 1; // IQ
}

std::string BlockROB::PrintLastStatusAndReturnOldest(uint32_t stid)
{
    PrintStatus(stid);
    std::string ret = current[stid].entry[current[stid].commitPtr.val].Dump();
    return ret;
}

void BlockROB::GatherStats()
{
    ++top->stats->issued_cube_blocks_histogram[issued_cube_blocks];
    ++top->stats->issued_vector_blocks_histogram[issued_vector_blocks];
    ++top->stats->issued_tload_histogram[issued_tload];
    ++top->stats->issued_tstore_histogram[issued_tstore];
    top->stats->total_ostd_cube_blocks += issued_cube_blocks;
    top->stats->total_ostd_vector_blocks += issued_vector_blocks;
    top->stats->total_ostd_tload += issued_tload;
    top->stats->total_ostd_tstore += issued_tstore;
    top->stats->total_ostd_blocks = top->stats->total_ostd_cube_blocks +
                                    top->stats->total_ostd_vector_blocks +
                                    top->stats->total_ostd_tload +
                                    top->stats->total_ostd_tstore;

    for (size_t stid = 0; stid < top->core->configs.scalar_smt_thread; stid++) {
        top->stats->smtOstdCubeBlocksArray[stid] += smt_issued_cube_blocks_array[stid];
        top->stats->smtOstdVectorBlocksArray[stid] += smt_issued_vector_blocks_array[stid];
        top->stats->smtOstdTloadArray[stid] += smt_issued_tload_array[stid];
        top->stats->smtOstdTstoreArray[stid] += smt_issued_tstore_array[stid];
        top->stats->smtOstdBlocksArray[stid] = top->stats->smtOstdCubeBlocksArray[stid] +
                                        top->stats->smtOstdVectorBlocksArray[stid] +
                                        top->stats->smtOstdTloadArray[stid] +
                                        top->stats->smtOstdTstoreArray[stid];
    }
}

void BlockROB::incStatsIssuedBlocks(ROBID bid, uint32_t stid)
{
    if (next[stid].entry[bid.val].header->biqType == BIQType::CUBE_IQ) {
        ++issued_cube_blocks;
        ++smt_issued_cube_blocks_array[stid];
    } else if (next[stid].entry[bid.val].header->biqType == BIQType::VEC_IQ) {
        ++issued_vector_blocks;
        ++smt_issued_vector_blocks_array[stid];
    } else if (next[stid].entry[bid.val].header->biqType == BIQType::VET_IQ) {
        ++issuedTempleteBlocks;
    } else if (next[stid].entry[bid.val].header->biqType == BIQType::TMA_IQ) {
        if (next[stid].entry[bid.val].header->tileOp == TileOp::TLOAD) {
            ++issued_tload;
            ++smt_issued_tload_array[stid];
        } else if (next[stid].entry[bid.val].header->tileOp == TileOp::TSTORE) {
            ++issued_tstore;
            ++smt_issued_tstore_array[stid];
        }
    }
}

void BlockROB::decStatsIssuedBlocks(ROBID bid, uint32_t stid)
{
    if (next[stid].entry[bid.val].header->biqType == BIQType::CUBE_IQ) {
        ASSERT(issued_cube_blocks > 0);
        --issued_cube_blocks;
        --smt_issued_cube_blocks_array[stid];
    } else if (next[stid].entry[bid.val].header->biqType == BIQType::VEC_IQ) {
        ASSERT(issued_vector_blocks > 0);
        --issued_vector_blocks;
        --smt_issued_vector_blocks_array[stid];
    } else if (next[stid].entry[bid.val].header->biqType == BIQType::VET_IQ) {
        ASSERT(issuedTempleteBlocks > 0);
        --issuedTempleteBlocks;
    } else if (next[stid].entry[bid.val].header->biqType == BIQType::TMA_IQ) {
        if (next[stid].entry[bid.val].header->tileOp == TileOp::TLOAD) {
            ASSERT(issued_tload > 0);
            --issued_tload;
            --smt_issued_tload_array[stid];
        } else if (next[stid].entry[bid.val].header->tileOp == TileOp::TSTORE) {
            ASSERT(issued_tstore > 0);
            --issued_tstore;
            --smt_issued_tstore_array[stid];
        }
    }
}

void BlockROB::PrintStatus(uint32_t stid, bool enableFlag)
{
    if (!enableFlag && LoggerManager::GetManager().level > LoggerLevel::DETAIL) {
        return;
    }
    if (current[stid].size == 0) {
        return;
    }
    LOG_DETAIL_M(Unit::BCC, Stage::BROB) << "==================================================";
    LOG_DETAIL_M(Unit::BCC, Stage::BROB) << "size: " << dec << current[stid].size;
    for (uint32_t i = 0; i <current[stid].entry.size(); i++) {
        uint32_t ptr = (current[stid].commitPtr.val+i)%current[stid].entry.size();
        if (current[stid].entry[ptr].status == BlockStatus::FREE ||
            current[stid].entry[ptr].status == BlockStatus::FLUSHED) {
            continue;
        }
        std::stringstream oss;
        oss << "\t|--" << current[stid].entry[ptr].Dump();
        if (current[stid].commitPtr.val == ptr) {
            oss << "<- oldest";
        }
        LOG_DETAIL_M(Unit::BCC, Stage::BROB) << "\t|--" << current[stid].entry[ptr].Dump()
            << (current[stid].commitPtr.val == ptr ? "<- oldest" : "");
    }
    LOG_DETAIL_M(Unit::BCC, Stage::BROB) << "==================================================";
}

void BlockROB::Work() {
    for (uint32_t i = 0; i < top->core->configs.scalar_smt_thread; ++i) {
        Work(i);
    }
    GatherStats();
}

void BlockROB::Work(uint32_t stid)
{
    top->stats->total_brob_size += current[stid].size;
    top->stats->smtBrobSizeArray[stid] += current[stid].size;
    deliveryStoreID(stid);
    commitBlock(stid);
    next[stid].CheckNonFlushBid();
    PrintStatus(stid);
}

void BlockROB::recoverBlock(FlushBus &flushReq) {
    uint32_t stid = flushReq.req.stid;
    if (next[stid].entry[next[stid].commitPtr.val].status == BlockStatus::NEEDFLUSH) {
        return;
    }

    // TODO: fix next alloc ptr
    flushReq.old_alloc = next[stid].allocPtr;
    if (next[stid].entry[next[stid].allocPtr.val].status == BlockStatus::PAR_RUNNING) {
        IncROBID(flushReq.old_alloc, next[stid].entry.size());
    }
    next[stid].allocPtr = flushReq.req.bid;
    ROBID resovleBid = flushReq.req.bid;
    SubROBID(resovleBid, 1, next[stid].entry.size());

    // Record the bpc that mis-predicted.
    uint64_t predictBpc = next[stid].entry[resovleBid.val].header->resolveTarget;
    if (top->stats->misBpcCntMap.count(predictBpc) != 0) {
        ++top->stats->misBpcCntMap[predictBpc];
    } else {
        top->stats->misBpcCntMap.insert(make_pair(predictBpc, 1));
    }
    uint64_t brBpc = next[stid].entry[resovleBid.val].header->bpc;
    if (top->stats->misBrBpcCntMap.count(brBpc) != 0) {
        ++top->stats->misBrBpcCntMap[brBpc];
    } else {
        top->stats->misBrBpcCntMap.insert(make_pair(brBpc, 1));
    }
    ++top->stats->bpc_mispredict_count;
    top->stats->second_taken_mis_pred += next[stid].entry[resovleBid.val].header->machineInst->bfuInfo->predict_at_once;
    top->stats->first_taken_mis_pred += !next[stid].entry[resovleBid.val].header->machineInst->bfuInfo->predict_at_once;
    top->stats->forward_mis_pred += next[stid].entry[resovleBid.val].header->machineInst->bfuInfo->predict_forward;

    // Set flag of reolving after reporting to BIFU
    ASSERT(next[stid].entry[resovleBid.val].header);
    next[stid].entry[resovleBid.val].header->resolveTargetVld = true;

    LOG_DEBUG_M(Unit::BCC, Stage::BROB) << "Recover from missBP bid " << std::dec << resovleBid.val
        << " redirect BPC 0x" << std::hex << next[stid].entry[resovleBid.val].header->resolveTarget;
    if (DEBUG_VERBOSE_ON || top->configs.verbose) {
        LOG_DEBUG(top->debugLogger, Unit::BCC, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::LOW,
                  "Recover from missBP bid %u, redirect BPC 0x%x",
                  resovleBid.val, next[stid].entry[resovleBid.val].header->resolveTarget);
    }
    top->blockFetchUnit.resolveBlock(next[stid].entry[resovleBid.val].header);

    ROBID bptr = next[stid].commitPtr;
    for (uint32_t i = 0; i < next[stid].entry.size(); ++i) {
        if (LessEqual(flushReq.req.bid, bptr)) {
            if (next[stid].entry[bptr.val].status == BlockStatus::EXCEPTION) {
                setExcpStatus(stid, false);
            }
            if (next[stid].entry[bptr.val].status == BlockStatus::TERMINATE) {
                setTerminateStatus(stid, false);
            }
            if (next[stid].entry[bptr.val].header &&
                next[stid].entry[bptr.val].header->type == BlockType::BLK_TYPE_SYS) {
                setSysBlockStatus(stid, false);
            }

            if (next[stid].entry[bptr.val].status == BlockStatus::ALLOCATED ||
                next[stid].entry[bptr.val].status == BlockStatus::DISPATCHED ||
                next[stid].entry[bptr.val].status == BlockStatus::RENAMING ||
                next[stid].entry[bptr.val].status == BlockStatus::RENAMED ||
                next[stid].entry[bptr.val].status == BlockStatus::ISSUED ||
                next[stid].entry[bptr.val].status == BlockStatus::RUNNING ||
                next[stid].entry[bptr.val].status == BlockStatus::PAR_RUNNING ||
                next[stid].entry[bptr.val].status == BlockStatus::COMPLETED ||
                next[stid].entry[bptr.val].status == BlockStatus::TERMINATE ||
                next[stid].entry[bptr.val].status == BlockStatus::MISPRED ||
                next[stid].entry[bptr.val].status==BlockStatus::NEEDFLUSH ||
                next[stid].entry[bptr.val].status == BlockStatus::EXCEPTION) {
                ASSERT(next[stid].size>0);
                next[stid].size--;

                if (next[stid].size > top->core->configs.block_rob_depth) {
                    cerr<<"Cycle "<<dec<<top->GetSim()->getCycles()<<": BROB size = "<<next[stid].size<<" (buffer size = "<<top->core->configs.block_rob_depth<<")"<<endl;
                    cerr<<"Block Entry "<<dec<<bptr.val<<endl;
                    ASSERT(0 && "Block Recover: BROB Overflow!");
                }

                if (next[stid].entry[bptr.val].header &&
                    next[stid].entry[bptr.val].header->type == BlockType::BLK_TYPE_XB &&
                    next[stid].entry[bptr.val].header->branchType == BranchType::BLK_BR_CALL) {
                    GetSim()->systemStatus.ecallStatus = EcallStatus::NONE;
                }
            }
            if (!(bptr == flushReq.req.bid && next[stid].entry[bptr.val].status == BlockStatus::PAR_RUNNING)) {
                if (GetSim()->perfectGetSet) {
                    GetSim()->refInfo.refTrace.entry[bptr.val].resetGSRPtr();
                }
                if (GetSim()->perfectLoadStore) {
                    GetSim()->refInfo.refTrace.entry[bptr.val].resetLSRPtr();
                    GetSim()->recoverBlockRefStq(bptr.val);
                }
                next[stid].entry[bptr.val].status = BlockStatus::FLUSHED;
            }
            if (LessROBID(bptr, next[stid].sbarPtr) && next[stid].entry[bptr.val].header &&
                top->core->configs.BSB_enable && next[stid].entry[bptr.val].header->sbarCertain) {
                ASSERT(next[stid].sbarID>=next[stid].entry[bptr.val].header->storeCount);
                next[stid].sbarID -= next[stid].entry[bptr.val].header->storeCount;
            }
        }
        IncROBID(bptr, next[stid].entry.size());
    }

    if (next[stid].size == 0) {
        next[stid].sbarPtr = next[stid].allocPtr;
    } else if (LessROBID(flushReq.req.bid, next[stid].sbarPtr)) {
        next[stid].sbarPtr = flushReq.req.bid;
    }
}

bool BlockROB::needStall(uint32_t pos, uint32_t stid) {
    // Check if ROB is full or not
    return (current[stid].size + pos >= current[stid].entry.size());
}

void BlockROB::Xfer() {
    if (GetSim()->systemStatus.ecallStatus == EcallStatus::COMMITTED &&
        GetSim()->stqFree && GetSim()->l2Free) {
        GetSim()->systemStatus.ecallStatus = EcallStatus::NONE;
    }

    for (auto &it : iex_brob_rslvblk) {
        it->Work();
    }
    resolveBlock();
    current = next;
}

void BlockROB::Reset() {
    for (uint32_t i = 0; i < top->core->configs.scalar_smt_thread; ++i) {
        Reset(i);
    }
}

void BlockROB::Reset(uint32_t stid) {
    for (uint32_t i = 0; i <current[stid].entry.size(); i++) {
        current[stid].entry[i].Reset();
        next[stid].entry[i].Reset();
    }

    for (uint32_t i = 0; i <peCount; i++) {
        current[stid].osdBSize[i] = 0;
        next[stid].osdBSize[i] = 0;
        current[stid].ipcPE[i] = 0;
        next[stid].ipcPE[i] = 0;
    }

    current[stid].size = 0;
    next[stid].size = 0;

    next[stid].sbarID = 0;
    current[stid].sbarID = 0;
}

void BlockROB::Build() {
    current.resize(top->core->configs.scalar_smt_thread);
    next.resize(top->core->configs.scalar_smt_thread);
    lastSetACCBlock.resize(top->core->configs.scalar_smt_thread);
    for (uint32_t i = 0; i < top->core->configs.scalar_smt_thread; ++i) {
        Build(i);
    }

    top->stats->issued_cube_blocks_histogram.resize(top->core->configs.block_rob_depth + 1, 0);
    top->stats->issued_vector_blocks_histogram.resize(top->core->configs.block_rob_depth + 1, 0);
    top->stats->issued_tload_histogram.resize(top->core->configs.block_rob_depth + 1, 0);
    top->stats->issued_tstore_histogram.resize(top->core->configs.block_rob_depth + 1, 0);
    runBarrierBlock.resize(top->core->configs.scalar_smt_thread, false);
    excpStall.resize(top->core->configs.scalar_smt_thread, false);
    terminate.resize(top->core->configs.scalar_smt_thread, false);
    runSysBlock.resize(top->core->configs.scalar_smt_thread, false);

    /* smt */
    top->stats->smtBrobSizeArray.resize(top->core->configs.scalar_smt_thread, 0);
    top->stats->smtOstdBlocksArray.resize(top->core->configs.scalar_smt_thread, 0);
    top->stats->smtOstdCubeBlocksArray.resize(top->core->configs.scalar_smt_thread, 0);
    top->stats->smtOstdVectorBlocksArray.resize(top->core->configs.scalar_smt_thread, 0);
    top->stats->smtOstdTloadArray.resize(top->core->configs.scalar_smt_thread, 0);
    top->stats->smtOstdTstoreArray.resize(top->core->configs.scalar_smt_thread, 0);

    smt_issued_cube_blocks_array.resize(top->core->configs.scalar_smt_thread, 0);
    smt_issued_vector_blocks_array.resize(top->core->configs.scalar_smt_thread, 0);
    smt_issued_tload_array.resize(top->core->configs.scalar_smt_thread, 0);
    smt_issued_tstore_array.resize(top->core->configs.scalar_smt_thread, 0);

    top->stats->smtSlotsDecodeStallArray.resize(top->core->configs.scalar_smt_thread, 0);
    top->stats->smtSlotsBrenameStallArray.resize(top->core->configs.scalar_smt_thread, 0);
    top->stats->smtSlotsBrobStallArray.resize(top->core->configs.scalar_smt_thread, 0);
    top->stats->smtSlotsSysblockStallArray.resize(top->core->configs.scalar_smt_thread, 0);
    top->stats->smtBrenameSetStallArray.resize(top->core->configs.scalar_smt_thread, 0);
    top->stats->smtBrenameGetStallArray.resize(top->core->configs.scalar_smt_thread, 0);
    top->stats->smtBisqFullStallArray.resize(top->core->configs.scalar_smt_thread, 0);
    top->stats->smtTileregSizeFullStallArray.resize(top->core->configs.scalar_smt_thread, 0);
}

void BlockROB::Build(uint32_t stid)
{
    for (uint32_t i = 0; i < top->core->configs.block_rob_depth; i++) {
        current[stid].entry.push_back(BROBEntry());
        next[stid].entry.push_back(BROBEntry());
    }

    peCount = top->core->GetPECount();
    for (uint32_t i = 0; i < peCount; i++) {
        current[stid].osdBSize.push_back(0);
        next[stid].osdBSize.push_back(0);
        current[stid].ipcPE.push_back(0);
        next[stid].ipcPE.push_back(0);
    }
    Reset(stid);
}

void BROBEntry::Reset() {
    status = BlockStatus::FREE;
    header = shared_ptr<FullBlockHeader>(nullptr);
    perf.Reset();
}

std::string BROBEntry::DumpPipeViewInfo()
{
    std::stringstream oss;
    oss << "(PC:0x" << std::hex << header->bpc << ") Block." << GetBlockTypeName(header->type);
    oss << " " << BIQTypeName(header->biqType);
    oss << std::dec << header->peid << " " << GetBlockBranchTypeName(header->branchType);
    if (blockCMD->tileOp != TileOp::TINVALID) {
        oss << blockCMD->Dump();
    }
    return oss.str();
}

std::string BROBEntry::Dump()
{
    if (blockCMD == nullptr) {
        return "EMPTY BROB ENTRY";
    }
    std::stringstream oss;
    oss << blockCMD->Dump();
    oss << " " << BIQTypeName(blockCMD->biqType);
    oss << " " << GetBROBStatus(status);
    return oss.str();
}

void BlockStatusEntry::Reset() {
    running = false;
    stallOnLoad = false;
    stallOnGet = false;
    stall = false;
    forwardSet = false;
    ipc = 0;
}

SimSys *BlockROB::GetSim() {
    return top->sim;
}

BHeader BlockROB::getBlockHeader(ROBID bid, uint32_t stid) {
    return next[stid].entry[bid.val].header;
}

BlockCommandPtr BlockROB::GetLastBlockCMDPtr(ROBID bid, uint32_t stid)
{
    ROBID ptr = bid;
    SubROBID(ptr, 1, next[stid].entry.size());
    return next[stid].entry[ptr.val].blockCMD;
}

BlockCommandPtr BlockROB::GetBlockCMDPtr(ROBID bid, uint32_t stid)
{
    return current[stid].entry[bid.val].blockCMD;
}

BlockCommandPtr BlockROB::GetNextBlockCMDPtr(ROBID bid, uint32_t stid)
{
    return next[stid].entry[bid.val].blockCMD;
}

void BlockROB::setBlockHeaderGetMask(ROBID bid, uint32_t getBitMask, uint32_t stid) {
    next[stid].entry[bid.val].header->getBitMask |= getBitMask;
}

void BlockROB::setBlockHeaderSetMask(ROBID bid, uint32_t setBitMask, uint32_t stid) {
    next[stid].entry[bid.val].header->setBitMask |= setBitMask;
}

void BlockROB::setBlockHeaderSetRegMask(ROBID bid, uint32_t setRegMask, uint32_t stid) {
    next[stid].entry[bid.val].header->setRegMask = setRegMask;
}

void BlockROB::setBlockHeaderInputMask(ROBID bid, uint32_t getMask, uint32_t stid) {
    next[stid].entry[bid.val].header->inputRegMask |= getMask;
}

void BlockROB::setBlockHeaderOutputMask(ROBID bid, uint32_t setMask, uint32_t stid) {
    next[stid].entry[bid.val].header->outputRegMask |= setMask;
}

std::ostream& operator<<(std::ostream& out, BROBEntry const &entry) {
    out<<"BSeq "<<dec<<entry.id<<" "<<entry.header<<" Mapping ";
    return out;
}

void BlockROB::reportRunning(ROBID bid, Opcode op, uint32_t pe, uint32_t stid) {
    next[stid].entry[bid.val].perf.running= true;
    next[stid].entry[bid.val].perf.ipc++;
    next[stid].ipcPE[pe]++;
}

void BlockROB::reportStall(ROBID bid, bool load, bool get, uint32_t pe, uint32_t stid)
{
    (void)pe;
    next[stid].entry[bid.val].perf.stall = true;
    next[stid].entry[bid.val].perf.stallOnLoad = load;
    next[stid].entry[bid.val].perf.stallOnGet = get;
}

void BlockROB::reportException(ROBID bid, uint32_t stid, std::string info) {
    if (next[stid].entry[bid.val].status != BlockStatus::FLUSHED
        && next[stid].entry[bid.val].status != BlockStatus::MISPRED
        && next[stid].entry[bid.val].status != BlockStatus::NEEDFLUSH) {
        next[stid].entry[bid.val].status = BlockStatus::EXCEPTION;
        next[stid].entry[bid.val].exceptionInfo = info;
        LOG_DEBUG_M(Unit::BCC, Stage::BROB) << "report exception at bid " << std::dec << bid.val;
        if (!info.empty()) {
            LOG_DEBUG_M(Unit::BCC, Stage::BROB) << "Exception Info: " << info;
        }
        if (DEBUG_VERBOSE_ON || top->configs.verbose) {
            LOG_DEBUG(top->debugLogger, Unit::BCC, Stage::NA, bid.val, UINT64_MAX, LogLevel::HIGH,
                      "Report exception at bid %u", bid.val);
            if (!info.empty()) {
                LOG_DEBUG(top->debugLogger, Unit::BCC, Stage::NA, bid.val, UINT64_MAX, LogLevel::HIGH,
                          "Exception Info: %s", info);
            }
        }
        setExcpStatus(stid, true);
    }
}

void BlockROB::reportTerminate(ROBID bid, uint32_t stid) {
    if (next[stid].entry[bid.val].status != BlockStatus::FLUSHED
        && next[stid].entry[bid.val].status != BlockStatus::MISPRED
        && next[stid].entry[bid.val].status != BlockStatus::NEEDFLUSH) {
        next[stid].entry[bid.val].status =  BlockStatus::TERMINATE;
        setTerminateStatus(stid, true);
    }
}

void BlockROB::reportRealBsize(const uint32_t pos, OPEState *opeState, uint32_t stid)
{
    if (!next[stid].entry[pos].header) {
        return;
    }

    BSizeBus bus;
    bus.instCommitStats = opeState->instCommitStats[pos];
    bus.instRelativeRegStats = opeState->instRelativeRegStats[pos];
    ASSERT(bus.instCommitStats != nullptr);

    next[stid].entry[pos].header->realSize = bus.bSize;
    next[stid].entry[pos].header->instStats = bus;
}

bool BlockROB::CheckRename(ROBID &bid, uint32_t stid) {
    BlockStatus status = current[stid].entry[bid.val].status;
    return status == BlockStatus::RENAMED || status == BlockStatus::DISPATCHED || status == BlockStatus::RUNNING;
}

BHeader BlockROB::getNextBlockHeader(ROBID bid, uint32_t stid) {
    return next[stid].entry[bid.val].header;
}

uint32_t BlockROB::GetRetiredInstNum()
{
    return retire_total_inst;
}

void BlockROB::SetRetiredInstNum(uint32_t num)
{
    retire_total_inst += num;
}

static bool CheckIsScalar(BHeader &header)
{
    return !IsBlockTypeParallel(header->type);
}

static bool CheckIsBranchHeader(BHeader &header)
{
    return header->branchType == BranchType::BLK_BR_DIRECT || header->branchType == BranchType::BLK_BR_CALL ||
        header->branchType == BranchType::BLK_BR_COND || header->branchType == BranchType::BLK_BR_IND ||
        header->branchType == BranchType::BLK_BR_ICALL || header->branchType == BranchType::BLK_BR_RET;
}

static bool CheckHasLoadStore(BHeader &header)
{
    return header->loadCount != 0 || header->storeCount != 0;
}

static bool CheckIsTLoadTStore(BHeader &header)
{
    return (header->tileOp == TileOp::TLOAD || header->tileOp == TileOp::TSTORE);
}

static bool CheckNonFlushOldestBid(BROBEntry &bentry)
{
    // 1. block completed
    // 2. no branch
    // 3. no scalar block
    // 4. Tload/Tstore had been issued
    if (bentry.status == BlockStatus::COMPLETED) {
        return true;
    }

    if (CheckIsScalar(bentry.header)) {
        bool brachRslv = (CheckIsBranchHeader(bentry.header) && bentry.header->machineInst && bentry.header->machineInst->bfuInfo->resolved)
                         || !CheckIsBranchHeader(bentry.header);
        return brachRslv && !CheckHasLoadStore(bentry.header);
    }

    if (CheckIsTLoadTStore(bentry.header)) {
        return bentry.status == BlockStatus::ISSUED;
    }

    return true;
}

void BROBState::CheckNonFlushBid()
{
    nonFlushBid = commitPtr;
    for (uint32_t i = 0; i < size; ++i) {
        BROBEntry &bentry = entry[nonFlushBid.val];
        if (!CheckNonFlushOldestBid(bentry) || i == size - 1) {
            break;
        }
        IncROBID(nonFlushBid, entry.size());
    }
}

ROBID BlockROB::GetNonFlushOldestBid(uint32_t stid)
{
    return current[stid].nonFlushBid;
}

} // namespace JCore
