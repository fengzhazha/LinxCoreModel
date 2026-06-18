#include "bctrl/BCtrl.h"

#include <cassert>
#include <cstring>

#include "core/Bus.h"
#include "core/Core.h"
#include "ISA.h"
#include "ISA.h"
#include "lsu/lsu.h"
#include "SimSys.h"

namespace JCore {

using namespace std;

void BCtrlUnit::Work() {
    // add stats
    if (!GetSim()->systemStatus.EcallRunning()) {
        stats->cycles++;
    }
    stats->slots_total += configs.bctrl_bandwidth;

    blockFetchUnit.Work();
    uint32_t stid = PickThread();
    RunFetchStage5(stid);
    blockIssueQueueUnit.Work();
    scalarPE->Work();
    blockROB.Work();
    bmdb.Work();
}

bool BCtrlUnit::CheckStall(uint32_t stid)
{
    if (core->flushUnit->needFlush()) {
        stats->slots_fe_bubble += configs.bctrl_bandwidth;
        stats->slots_flush_recover_bubble += configs.bctrl_bandwidth;
        return true;
    }

    if (blockROB.hasException(stid) || blockROB.getTerminateStatus(stid)) {
        return true;
    }

    return false;
}

bool BCtrlUnit::CheckBROBStall(uint64_t slot, uint32_t stid)
{
    if (blockROB.needStall(slot, stid) || blockROB.getSysBlockStatus(stid) || blockROB.GetBarrierBlockStatus(stid)){
        if (blockROB.needStall(slot, stid)) {
            stats->slots_brob_stall ++;
            stats->smtSlotsBrobStallArray[stid]++;
        }
        if (blockROB.getSysBlockStatus(stid)) {
            stats->slots_sysblock_stall += (configs.bctrl_bandwidth - slot);
            stats->smtSlotsSysblockStallArray[stid] += (configs.bctrl_bandwidth - slot);
        }
        return true;
    }

    return false;
}

bool BCtrlUnit::CheckSTDPEStall(uint64_t slot)
{
    // 后续需要确认
    (void)slot;
    return (scalarPE->dcTop.inst_top_dec_arr[0].size() > configs.bctrl_bandwidth);
}

static void SetBiqType(BHeader &fbh)
{
    fbh->biqType = GetBiqType(fbh->type);
    if (fbh->tileOp == TileOp::TMOV || fbh->tileOp == TileOp::MGATHER ||
        fbh->tileOp == TileOp::MSCATTER) {
        fbh->biqType = BIQType::TAU_IQ;
    }
    fbh->biqRealType = fbh->biqType;
}

// BHeader 后续会删除
void ConvertToBHeader(BHeader fbh, BlockCommandPtr cmd, SimInst inst) {
    fbh->machineInst = inst;
    fbh->bin = inst->binary;
    fbh->type = cmd->blockType;
    fbh->branchType = cmd->branchType;
    fbh->bpc = inst->pc;
    fbh->bstartBpc = inst->pc;
    fbh->size = GetEncodeLenVal(inst->codeLen);
    fbh->opcode = cmd->opcode;
    fbh->tileOp = cmd->tileOp;
    fbh->stid = cmd->stid;
    SetBiqType(fbh);
    cmd->biqType = fbh->biqType;
    fbh->isTemplate = cmd->bIsTemplate; // 两者template 的含义应该不同。后者包含TMA\CUBE
    // Block Resolve 需要使用这两个
    if (inst->bfuInfo && inst->bfuInfo->spInfo) {
        // FULL BP
        fbh->fallBPC = inst->pc + inst->bfuInfo->spInfo->hsize;
    } else {
        // PERFECT BP
        fbh->fallBPC = inst->pc + static_cast<uint64_t>(inst->codeLen);
    }
    fbh->divertBPC = cmd->barg.target;
}

uint32_t BCtrlUnit::PickThread() const
{
    static uint32_t stid = 0;
    stid = (stid + 1) % core->configs.scalar_smt_thread;
    return stid;
}

void BCtrlUnit::RunFetchStage5(uint32_t stid)
{
    // 由于flush\ECALL\BROB 已满导致\标量PE前端反压等导致的反压
    if (CheckStall(stid)) {
        return;
    }

    // BlockSplitCnt 类指令，需要分配BROB 表项，然后下发到标量PE 执行，其余指令直接下发到标量PE 执行。
    uint32_t blkSplitInstCnt = 0;
    for (uint32_t pos = 0; pos < configs.bctrl_bandwidth; pos++) {
        if (CheckSTDPEStall(pos)) {
            break;
        }
        SimInst inst = stallInst[stid] == nullptr ? blockFetchUnit.GetInst(stid) : stallInst[stid];
        if (inst == nullptr) {
            break;
        }
        LOG_INFO_M(Unit::BCC, Stage::F5) << "BCC Get Inst: " << inst->Dump();
        stallInst[stid] = nullptr;

        if ((inst->opcode == Opcode::OP_BSTOP ||
            !OpcodeInInstGroup(inst->opcode, InstGroup::BLOCK_SPLIT)) && currentBlkCmd[stid] == nullptr) {
            LOG_INFO_M(Unit::BCC, Stage::F5) << "Due to mismatch of block header, inst is skiped";
            continue;
        }

        if (inst->opcode != Opcode::OP_BSTOP && OpcodeInInstGroup(inst->opcode, InstGroup::BLOCK_SPLIT)) {
            if (CheckBROBStall(blkSplitInstCnt, stid)) {
                LOG_INFO_M(Unit::BCC, Stage::F5) << "STID: " << dec << stid << " stalled due to ROB Stall";
                stallInst[stid] = inst;
                return;
            }
            // Alloc BROB
            currentBHeader[stid] = std::make_shared<FullBlockHeader>();
            last_header[stid] = currentBHeader[stid];
            // 由于解码时，16bitBSTART 类opcode 已经有过修改，不能重复AccumulateBlockInfo
            ASSERT(inst->bcmd);
            currentBlkCmd[stid] = std::make_shared<BlockCommand>(*inst->bcmd);
            if (currentBlkCmd[stid] == nullptr) {
                currentBlkCmd[stid] = std::make_shared<BlockCommand>();
                currentBlkCmd[stid]->AccumulateBlockInfo(inst);
            }
            currentBlkCmd[stid]->machineInst = inst;
            currentBlkCmd[stid]->stid = stid;
            ConvertToBHeader(currentBHeader[stid], currentBlkCmd[stid], inst);
            ASSERT(stid == currentBHeader[stid]->stid);
            currentBID[stid] = blockROB.allocBlock(currentBHeader[stid], currentBlkCmd[stid], blkSplitInstCnt++);
            currentBHeader[stid]->renameBid = currentBID[stid];
            if (currentBlkCmd[stid]->bIsTemplate && currentBlkCmd[stid]->opcode != Opcode::OP_INVALID) {
                inst->isLastInBlock = false;
            }
            RslvDirectly(currentBHeader[stid]);
            LOG_INFO_M(Unit::BCC, Stage::F5) << "Alloc BROB Entry from:" << inst->Dump() << " to "
                << currentBlkCmd[stid]->Dump();
        }

        if (currentBlkCmd[stid]->opcode != Opcode::OP_INVALID && currentBlkCmd[stid]->hasSendBRenameLastFlag) {
            LOG_INFO_M(Unit::BCC, Stage::F5) << "BCC skip redundant inst: " << inst->Dump();
            continue;
        }

        // Send TO STD PE
        inst->bpc = currentBlkCmd[stid]->bpc;
        inst->bid = currentBlkCmd[stid]->bid;
        inst->biqType = currentBlkCmd[stid]->biqType;
        inst->peID = DispatchToPE();
        inst->stid = stid;
        scalarPE->dcTop.inst_top_dec_arr[inst->peID].emplace_back(inst);
        if (currentBlkCmd[stid]->opcode != Opcode::OP_INVALID) {
            scalarPE->dcTop.inst_top_ct_arr[inst->peID].emplace_back(inst);
            currentBlkCmd[stid]->hasSendBRenameLastFlag = true;
        }
        LOG_INFO_M(Unit::BCC, Stage::F5) << "BCC send Inst to STD PE " << inst->Dump();
        if (IsBlockTypeParallel(currentBlkCmd[stid]->blockType) &&
            inst->isLastInBlock && inst->opcode != Opcode::OP_BSTOP) {
            inst->isLastInBlock = false;
            stallInst[stid] = inst->GenNextBSTOP();
            stallInst[stid]->bfuInfo = std::make_shared<BFUMachineInfo>(GetSim()->getCycles());
        }
    }
}

uint64_t BCtrlUnit::DispatchToPE()
{
    // BCC PE 仅有一个，为适配已有arr 代码，仅返回0，后续删除
    return 0;
}

void BCtrlUnit::interrupt(CoreControlBus req, uint32_t stid) {
    if (req.vld) {
        LOG_DETAIL_M(Unit::BCC, Stage::NA) << "BCC Start to jump 0x" << hex << req.initPC;
        blockFetchUnit.jumpTo(req.initPC, stid);
    }
}

void BCtrlUnit::RecordBccEfficient() const
{
    if (!core->iex[SCALAR_IEX]->iq.CheckBCCBound()) {
        return;
    }

    stats->SetBccBoundEfficient();
}

void BCtrlUnit::RslvDirectly(BHeader &header) {
    if (header->isInst) {
        return;
    }

    // Resolve blocks if the target is absolute
    if (header->branchType == BranchType::BLK_BR_FALL || header->branchType == BranchType::BLK_BR_DIRECT ||
        header->branchType == BranchType::BLK_BR_CALL) {
        header->resolveMisPredict = false;
        header->resolveTaken = header->branchType != BranchType::BLK_BR_FALL;
        header->resolveTarget = header->branchType == BranchType::BLK_BR_FALL ? header->fallBPC : header->divertBPC;
        if (!core->flushUnit->needFlush(header->bSeq)) {
            blockFetchUnit.resolveBlock(header);
        }
    }
}

void BCtrlUnit::Build() {
    // override configurations
    configs.overrideDefaultConfig(GetSim()->getCfgs());
    cubeConfig.overrideDefaultConfig(GetSim()->getCfgs());
    configs.parallel_acc_chain_threshold = core->configs.block_rob_depth;
    stats = new BCtrlStats(GetSim()->getRpt());

    blockFetchUnit.Build();
    blockROB.top = this;
    blockROB.Build();
    flushRecovering.resize(core->configs.scalar_smt_thread, false);
    currentBID.resize(core->configs.scalar_smt_thread, ROBID());
    currentBHeader.resize(core->configs.scalar_smt_thread, nullptr);
    currentBlkCmd.resize(core->configs.scalar_smt_thread, nullptr);
    robStall.resize(core->configs.scalar_smt_thread, false);
    decodeStall.resize(core->configs.scalar_smt_thread, false);
    last_header.resize(core->configs.scalar_smt_thread, nullptr);
    curBid.resize(core->configs.scalar_smt_thread, 0);
    blockIssueQueueUnit.top = this;
    blockIssueQueueUnit.Build();
    bmdb.Build();
    BuildScalarPE();
    core->peArray.push_back(scalarPE);

    stallInst.resize(core->configs.scalar_smt_thread, nullptr);

    core->SetBccConfigs("inst_decode_bw", configs.GetCondfigValue("bctrl_bandwidth"));
    core->SetBccConfigs("block_rob_depth", core->configs.GetCondfigValue("block_rob_depth"));
    core->SetBccConfigs("BISQ:cube_isq_depth", configs.GetCondfigValue("cubeIssueQDepth"));
    core->SetBccConfigs("BISQ:vector_isq_depth", configs.GetCondfigValue("vecIssueQDepth"));
    core->SetBccConfigs("BISQ:mtc_isq_depth", configs.GetCondfigValue("mtcIssueQDepth"));
}

void BCtrlUnit::BuildScalarPE()
{
    scalarPE = std::make_shared<SPE>();
    scalarPE->sim = sim;
    scalarPE->core = core;
    scalarPE->peID = 0;
    scalarPE->Build();
}

void BCtrlUnit::Reset() {
    blockFetchUnit.Reset();
    blockROB.Reset();
    bmdb.Reset();
    for (size_t i = 0; i < decodeStall.size(); ++i) {
        decodeStall[i] = true;
    }

    // TODO: lane pointer and stall fixed
    current.dispatchAUXLane = 0;
    current.dispatchSTDLane = 0;
}

void BCtrlUnit::flush(FlushBus flushReq) {
    if (!flushReq.req.vld) return;
    uint32_t stid = flushReq.req.stid;
    flushRecovering[flushReq.req.stid] = true;
    blockFetchUnit.flush(flushReq);
    scalarPE->Flush(flushReq);
    blockIssueQueueUnit.Flush(flushReq);
    if (GetSim()->core->configs.bp_mode == static_cast<uint64_t>(BP_Mode::PERFECT_BP) ||
        GetSim()->core->configs.bp_mode == static_cast<uint64_t>(BP_Mode::NO_BP)) {
        // Rest offset GPR.
        GetSim()->core->disableOffGPR();
    }
    decodeStall[stid] = false;
    stallInst[stid] = nullptr;
    bool bid_match = currentBlkCmd[stid] && currentBlkCmd[stid]->bid == flushReq.req.bid;
    if (currentBlkCmd[stid] && (LessEqual(flushReq.req.bid, currentBlkCmd[stid]->bid))) {
        currentBlkCmd[stid] = nullptr;
        last_header[stid] = std::shared_ptr<FullBlockHeader>(nullptr);
        block_head_renamed = false;
        getMask = 0;
        setMask = 0;
        recover = false;
    }
    if (flushReq.req.fetchTPCVld) {
        last_header[stid] = blockROB.getBlockHeader(flushReq.req.bid, stid);
        currentBlkCmd[stid] = blockROB.GetBlockCMDPtr(flushReq.req.bid, stid);
        getMask = last_header[stid]->inputRegMask;
        setMask = last_header[stid]->outputRegMask;
        if (!bid_match) {
            recover = true;
        }
        curBid[stid] = last_header[stid]->curBid + 1;
        return;
    }
    BHeader theader = blockROB.getBlockHeader(flushReq.req.bid, stid);
    if (!flushReq.req.fetchTPCVld || theader == NULL) {
        ROBID bid = flushReq.req.bid;
        SubROBID(bid, 1, core->configs.block_rob_depth);
        theader = core->bctrl->blockROB.getBlockHeader(bid, stid);
        curBid[stid] = theader->curBid;
    }

    block_head_renamed = false;
}

void BCtrlUnit::replay(FlushBus flushReq) {
    if (!flushReq.req.vld) return;
}

void BCtrlUnit::Xfer() {
    current = next;
    blockFetchUnit.Xfer();
    blockROB.Xfer();
    blockIssueQueueUnit.Xfer();
    scalarPE->Xfer();
}


BCtrlUnit::BCtrlUnit() {
    blockFetchUnit.top = this;
    blockROB.top = this;
    blockIssueQueueUnit.top = this;
    bmdb.top = this;
}

void BCtrlUnit::RetireBlock(const ROBID& bid, uint32_t stid)
{
    if (last_header[stid] && last_header[stid]->bSeq == bid) {
        last_header[stid] = std::shared_ptr<FullBlockHeader>(nullptr);
        block_head_renamed = false;
    }
}

BCtrlUnit::~BCtrlUnit()
{
    DeletePtr(stats);
}

bool BCtrlUnit::StackRenameEnable(uint32_t peid)
{
    (void)peid;
    return configs.stack_rename_mode != 0;
}

void BCtrlUnit::ReportLocalRelease(OperandType typ, uint32_t seq, uint32_t tid, uint32_t peid)
{
    auto &sgprRenameUnit = this->scalarPE->d2Stage.sgprRenameUnit[peid][tid][SGPRType2Idx(typ)];
    sgprRenameUnit.ReportRetired(seq, true);
}

void BCtrlUnit::SMTTerminate(uint32_t stid)
{
    this->blockFetchUnit.TerminateFlush(stid);
}

} // namespace JCore
