#include "bctrl/BIFU.h"

#include <cstring>
#include <iostream>
#include <memory>
#include <set>

#include "../emulator/SoftCore.h"
#include "bctrl/BCtrl.h"
#include "bctrl/bfu/common/MachineInst.h"
#include "core/Core.h"
#include "GFUSim.h"
#include "ISA.h"
#include "ModelCommon/SimInstInfo.h"

namespace JCore {

using namespace std;

BlockIFU::BlockIFU()
{
}

void BlockIFU::Work()
{
    if (GetSim()->core->IsFullBP()) {
        // push the oldest misprediction into resolveQ
        if (oldestMispred) {
            NS_CORE::PtrMachineInst machineInst = oldestMispred->machineInst;
            auto func = [this, machineInst](NS_CORE::PtrMachineInst h) -> bool {
                if (h->bfuInfo->fbid > machineInst->bfuInfo->fbid) {
                    return true;
                }
                if (h->bfuInfo->hid > machineInst->bfuInfo->hid) {
                    return true;
                }
                return false;
            };
            be_bfu_rslv_q[machineInst->stid].FlushIf(func);
            be_bfu_rslv_q[machineInst->stid].Write(oldestMispred->machineInst);
            auto match = [this] (const NS_CORE::PtrMachineInst& machineInst) -> bool {
                // return (!machineInst->hdr.isInst || machineInst->hid != oldestMispred->machineInst->hid);
                return (machineInst->bfuInfo->hid != oldestMispred->machineInst->bfuInfo->hid) &&
                        machineInst->stid == oldestMispred->stid;
            };
            for (auto &q: bfu_be_q) {
                q.FlushIf(match);
            }
            oldestMispred = shared_ptr<FullBlockHeader>(nullptr);
        }
        for (uint32_t i = 0; i < top->core->configs.threadCount; ++i) {
            be_bfu_rslv_q[i].Work();
            be_bfu_cmt_q[i].Work();
        }
        be_bfu_nuke_info_q.Work();
        // drive bfu engine
        bfu->Work();
        bfuIntf.be_bfu_nuke = nullptr;
        // drive bfu-be queue
        for (auto &q : bfu_be_q) {
            q.Work();
        }
    }
}

void BlockIFU::Xfer() {
}

void BlockIFU::Reset() {
    if (GetSim()->core->IsFullBP()) {
        bfu->Reset();
    } else {
        // 重置bfuPerfectTrace 相关指针。
    }
    bfuIntf.be_bfu_nuke = nullptr;
    oldestMispred = shared_ptr<FullBlockHeader>(nullptr);
}

SimSys *BlockIFU::GetSim() {
    return top->sim;
}

void BlockIFU::jumpTo(uint64_t bpc, uint32_t stid) {
    if (!GetSim()->core->IsFullBP()) {
        return;
    }
    bfu->CreateNewInfoToFetchQ(bpc, 0, 0, stid);
}

SimInst BlockIFU::GetInst(uint32_t stid)
{
    if (GetSim()->core->IsFullBP()) {
        return GetInstFromBFU(stid);
    } else if (GetSim()->core->IsPerfectBP()) {
        return GetInstFromTrace(stid);
    } else {
        ASSERT(false && "bp_mode 2 is not yet supported");
        SimInst inst = std::make_shared<SimInstInfo>();
        return inst;
    }
}

SimInst BlockIFU::GetInstFromTrace(uint32_t stid)
{
    SimInst inst = std::make_shared<SimInstInfo>();
    inst->stid = stid;
    bool firstInBlock = false;
    bool lastInBlock = false;
    uint64_t pc = 0;
    bool fromFetchQ = false;
    if (!flushedPCQ.empty()) {
        PerfectFetchInfo info = flushedPCQ.front();
        firstInBlock = info.first;
        lastInBlock = info.last;
        pc = info.pc;
        flushedPCQ.pop_front();
        LOG_INFO_M(Unit::BFU, Stage::NA) << "Get PC: 0x" << hex << pc << " from flushedQ";
        fromFetchQ = true;
    } else {
        firstInBlock = GetSim()->GetVerifyManager(stid)->CheckBFUFrontIsFirst();
        lastInBlock = GetSim()->GetVerifyManager(stid)->CheckBFUFrontLast();
        pc = GetSim()->GetVerifyManager(stid)->GetBFUFrontPC();
    }
    if (pc == 0) {
        LOG_INFO_M(Unit::BFU, Stage::NA) << "the fetch address is 0 in perfect BP mode";
        return nullptr;
    }
    uint32_t size = 0;
    uint64_t bin = 0;

    // PreCheck inst Size
    // TODO: sim the latency of i mem.
    bin = GetSim()->memory.Load(pc, WIDTH_16, false);
    size = CheckMInstSize(bin);
    bin = GetSim()->memory.Load(pc, size, false);

    inst->DecodeBin(bin, size);
    inst->pc = pc;
    inst->isLastInBlock = lastInBlock;
    InstGroup grp = OpcodeManager::Inst().GetOpcodeGroup(inst->opcode);
    if (!fromFetchQ) {
        PerfectFetchInfo info;
        info.isInst = (grp != InstGroup::BLOCK_SPLIT);
        info.pc = pc;
        info.last = lastInBlock;
        info.first = firstInBlock;
        info.canSkip = !((!(lastInBlock && !firstInBlock) && inst->opcode != Opcode::OP_BSTOP));
        LOG_INFO_M(Unit::BFU, Stage::NA) << "BFU Record pc: 0x" << hex << pc;
        fetchedQ.push_back(info);
    }
    switch (grp) {
        case InstGroup::BLOCK_SPLIT:
            if (lastInBlock && !firstInBlock) {
                inst->opcode = Opcode::OP_BSTOP;
            }
            break;
        case InstGroup::CONVERT:
            inst->InitFRM(GetSim()->GetFRM());
            break;
        default:
            break;
    }
    inst->bfuInfo = std::make_shared<BFUMachineInfo>(GetSim()->getCycles());

    if (OpcodeInInstGroup(inst->opcode, InstGroup::BLOCK_SPLIT) && inst->opcode != Opcode::OP_BSTOP) {
        BlockCommandPtr cmd = std::make_shared<BlockCommand>();
        cmd->AccumulateBlockInfo(inst);
        inst->bcmd = cmd;
    }
    return inst;
}

SimInst BlockIFU::GetInstFromBFU(uint32_t stid)
{
    SimInst inst = bfu_be_q[stid].Empty() ? nullptr : bfu_be_q[stid].Read();
    if (inst) {
        inst->stid = stid;
        InstGroup grp = OpcodeManager::Inst().GetOpcodeGroup(inst->opcode);
        switch (grp) {
            case InstGroup::CONVERT:
                inst->InitFRM(GetSim()->GetFRM());
                break;
            default:
                break;
        }
    }
    return inst;
}

void BlockIFU::resolveBlock(BHeader &header) {
    auto resolveMispredictHeader = [this](BHeader& oldestHeader, BHeader &header) {
        oldestHeader->machineInst->bfuInfo->resolved = true;
        oldestHeader->machineInst->bfuInfo->resolve_time = GetSim()->getCycles();
        oldestHeader->machineInst->bfuInfo->committed = false;
        oldestHeader->machineInst->bfuInfo->resolve_taken = header->resolveTaken;
        oldestHeader->machineInst->bfuInfo->resolve_mispredict = header->resolveMisPredict;
        oldestHeader->machineInst->bfuInfo->resolve_tgt = header->resolveTarget;
    };
    if (GetSim()->core->IsFullBP()) {
        // In full BP mode, PE may resolve branch multiple times due to nuke flush or speculative execution
        // of a non-existing block. In the latter case, the following code may modify an machineInst
        // that is already observed by BFU, and trigger BRU flush that should happen in the future.
        if (header->machineInst->bfuInfo->resolved) {
            return;
        }

        resolveMispredictHeader(header, header);
        if (header->machineInst->bfuInfo->resolve_mispredict) {
            if (!oldestMispred || (oldestMispred && LessEqual(header->bSeq, oldestMispred->bSeq))) {
                oldestMispred = header;
                oldestMispred->vld = true;
                resolveMispredictHeader(oldestMispred, header);
            }
        } else {
            be_bfu_rslv_q[header->machineInst->stid].Write(header->machineInst);
        }

        if (header->machineInst->bfuInfo->resolve_mispredict) {
            LOG_INFO_M(Unit::BCC, Stage::F0) << "Misprediction at block " << std::dec << header->bSeq.val
                << " BPC 0x" << std::hex << header->bpc << " Predict BPC 0x" << std::hex << header->predictTarget
                << " Target BPC 0x" << header->resolveTarget << " Predict Taken " << std::dec
                << header->predictTaken << " Target taken " << header->resolveTaken
                << ", fbid=" << header->machineInst->bfuInfo->fbid
                << ", fbid_local=" << header->machineInst->bfuInfo->fbid_local
                << ", hid=" << header->machineInst->bfuInfo->hid
                << std::hex << ", pc=0x" << header->machineInst->pc
                << ", bundlePC=0x" << header->machineInst->GetBundlePosPC();
        }
    }
}

void BlockIFU::retireBlock(BHeader &header) {
    lastRetireBPC = header->bpc;
    if (GetSim()->core->IsFullBP()) {
        header->machineInst->bfuInfo->commit_time = GetSim()->getCycles();
        header->machineInst->bfuInfo->committed = true;
        be_bfu_cmt_q[header->machineInst->stid].Write(header->machineInst);
    }

    if (GetSim()->core->IsPerfectBP()) {
        // 当前实现，默认perfectBP 不会产生flush，因此读完pc 之后即释放perfect信息。
        // 后续需要适配可能产生的nukeflush。
        // 1. 增加retireBlockPtr, fetchBlockPtr等，
        // 2. 适配flush
        // 3. 在此处retire 时释放bfuPerfectTrace;ifuPerfectTrace 保存的队头信息。
        ASSERT(!fetchedQ.empty());
        auto info = fetchedQ.front();
        ASSERT(info.pc == lastRetireBPC);
        LOG_INFO_M(Unit::BFU, Stage::NA) << "INFO.PC=0x" << hex << info.pc << ", retiredBPC=0x" << hex << lastRetireBPC;
        fetchedQ.pop_front();
        if (fetchedQ.empty()) {
            return;
        }
        info = fetchedQ.front();
        for (; info.isInst || info.canSkip; info = fetchedQ.front()) {
            fetchedQ.pop_front();
            if (fetchedQ.empty()) {
                return;
            }
        }
    }
}

void BlockIFU::Build() {
    bfu = std::make_shared<NS_CORE::BFU>(GetSim(), top->stats, bfuIntf);
    bfu->Build();
    bfu_be_q.resize(top->core->configs.threadCount);
    be_bfu_rslv_q.resize(top->core->configs.threadCount);
    be_bfu_cmt_q.resize(top->core->configs.threadCount);
    bfuIntf.bfu_be_q.resize(top->core->configs.threadCount);
    bfuIntf.be_bfu_rslv_q.resize(top->core->configs.threadCount);
    bfuIntf.be_bfu_cmt_q.resize(top->core->configs.threadCount);
    for (uint32_t i = 0; i < bfuIntf.bfu_be_q.size(); ++i) {
        bfuIntf.bfu_be_q[i] = &bfu_be_q[i];
        bfuIntf.be_bfu_rslv_q[i] = &be_bfu_rslv_q[i];
        bfuIntf.be_bfu_cmt_q[i] = &be_bfu_cmt_q[i];
    }
    bfuIntf.be_bfu_nuke_info_q = &be_bfu_nuke_info_q;

    oldestMispred = make_shared<FullBlockHeader>();
}

void BlockIFU::flush(FlushBus flushReq) {
    if (GetSim()->core->IsFullBP()) {
        FlushFullBP(flushReq);
    } else {
        FlushNoBFU(flushReq);
    }
}

void BlockIFU::FlushFullBP(FlushBus flushReq)
{
    // For full BP mode
    if (flushReq.req.type == FlushType::MISS_PRED_FLUSH) {
        bfuIntf.be_bfu_nuke = nullptr;
        ROBID bid = flushReq.req.bid;
        SubROBID(bid, 1, top->core->configs.block_rob_depth);
        BlockStatus stat = top->blockROB.getBlockStatus(bid, flushReq.req.stid);
        BHeader header = top->blockROB.getBlockHeader(bid, flushReq.req.stid);
        if (stat == BlockStatus::FREE || stat == BlockStatus::FLUSHED || !header || !header->machineInst ||
            header->isTemplate) {
            bfu_be_q[flushReq.req.stid].Reset();
        } else {
            auto h = header->machineInst;
            auto match = [&h] (const NS_CORE::PtrMachineInst& machineInst) -> bool {
                // return (!machineInst->hdr.isInst || machineInst->hid != h->hid);
                return (machineInst->bfuInfo->hid != h->bfuInfo->hid);
            };
            bfu_be_q[flushReq.req.stid].FlushIf(match);
        }
    } else if (flushReq.req.iexTyp == SCALAR_IEX) {
        bfuIntf.be_bfu_nuke = top->core->nukeMHdr;
        bfuIntf.be_bfu_rslv_q[flushReq.req.stid]->Reset();
        bfu_be_q[flushReq.req.stid].Reset();
    }

    top->stats->bhq_clear ++;

    if (oldestMispred && LessEqual(flushReq.req.bid, oldestMispred->bSeq)) {
        oldestMispred = shared_ptr<FullBlockHeader>(nullptr);
    }
}

void BlockIFU::FlushNoBFU(FlushBus flushReq)
{
    // PerfectBP 模式下仍可能有 NUKE Flush(例如 Load-Store 冲突)，所以仍然需要保留 flush 的逻辑
    LOG_INFO_M(Unit::BFU, Stage::NA) << "BFU Perfect BP Recv flush: " << flushReq << ", baseOnBID: " << boolalpha
                                     << flushReq.baseOnBid;
    // 拿到需要刷的块 BID
    auto cmd = GetSim()->core->bctrl->blockROB.GetBlockCMDPtr(flushReq.req.bid, flushReq.req.stid);
    uint64_t startPC = 0;
    if (flushReq.baseOnBid) {
        if (flushReq.req.fetchTPCVld) {
            // NUKE
            LOG_INFO_M(Unit::BCC, Stage::NA) << "pc=0x" << hex << cmd->machineInst->bpc
                                             << ", codelen=" << dec << static_cast<uint64_t>(cmd->machineInst->codeLen);
            startPC = cmd->machineInst->pc + static_cast<uint64_t>(cmd->machineInst->codeLen);
        } else {
            startPC = cmd->machineInst->pc;
        }
    } else {
        // 块内 INNER flush
        ASSERT(flushReq.req.fetchTPCVld);
        startPC = flushReq.req.fetchTPC;
    }
    LOG_INFO_M(Unit::BFU, Stage::NA) << "START PC=0x" << hex << startPC;
    auto it = fetchedQ.begin();
    // 因为 fetchedQ 相当于保存了所有的取值历史，所以从 fetchedQ 恢复的时候，需要先把
    // flushedPCQ 清空，避免从 flush 发生取指后再次发生 flush 的场景
    flushedPCQ.clear();
    for (it = fetchedQ.begin(); it != fetchedQ.end(); ++it) {
        if (it->pc == startPC) {
            break;
        }
    }
    for (; it != fetchedQ.end(); ++it) {
        LOG_INFO_M(Unit::BFU, Stage::NA) << "Repush PC: 0x" << hex << it->pc;
        flushedPCQ.push_back(*it);
    }
    LOG_INFO_M(Unit::BCC, Stage::NA) << "FlushedPCQ.size() = " << dec << flushedPCQ.size();
}

void BlockIFU::TerminateFlush(uint32_t stid)
{
    bfu->TerminateFlush(stid);
}

} // namespace JCore
