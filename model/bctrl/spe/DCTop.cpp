#include "DCTop.h"

#include "bctrl/BCtrl.h"
#include "bctrl/spe/Decoder.h"
#include "core/Core.h"
#include "include/SimSys.h"
#include "SPE.h"

namespace JCore {

DCTop::~DCTop()
{
}

void DCTop::RecordRegStall(uint32_t peid, uint32_t width, uint32_t stid)
{
    if (!GetSim()->core->iex[SCALAR_IEX]->iq.CheckBCCBound()) {
        return;
    }

    if (top->prob[0]->getROBStall()) {
        return;
    }

    if (!GetSim()->core->memIntf[static_cast<int>(LSUType::VECTOR_LSU)]->CheckLoadQEmpty(stid)) {
        GetSim()->core->bctrl->stats->SetBccBoundBackendMemBound(width);
        return;
    }

    GetSim()->core->bctrl->stats->SetBccBoundBackendRegStall(width);
}

void DCTop::RecordRobStall(uint32_t width, uint32_t stid)
{
    if (!GetSim()->core->iex[SCALAR_IEX]->iq.CheckBCCBound()) {
        return;
    }

    if (!GetSim()->core->memIntf[static_cast<int>(LSUType::VECTOR_LSU)]->CheckLoadQEmpty(stid)) {
        GetSim()->core->bctrl->stats->SetBccBoundBackendMemBound(width);
        return;
    }

    GetSim()->core->bctrl->stats->SetBccBoundBackendRobStall(width);
}

void DCTop::Work()
{
    for (auto &dc : decoderDev) {
        dc.Work();
    }
    for (auto &ct : templateDev) {
        ct.Work();
    }
    auto processInvalid = [this](std::set<uint32_t>& invalidStid, uint32_t pe,
                                 uint32_t& stid, uint32_t peDecWidth, SimInst& inst) -> bool {
        if (inst) {
            return false;
        }
        statsInfo[pe].supplyBubble = peDecWidth - statsInfo[pe].allocROBCnt;
        invalidStid.insert(stid);
        if (invalidStid.size() == GetSim()->core->configs.scalar_smt_thread) {
            // break path
            return true;
        }
        stid = (stid + 1) % GetSim()->core->configs.scalar_smt_thread;
        return false;
    };

    for (uint32_t pe = 0; pe < GetSim()->core->configs.stdPeCount; ++pe) {
        // resets some statistics for the current cycle
        statsInfo[pe].Reset();

        uint64_t peDecWidth = top->configs.decodeWidth;
        uint32_t stid = 0;
        std::set<uint32_t> invalidStid;
        for (uint32_t i = 0; i < peDecWidth && !dec_ren_q->getStall();) {
            SimInst inst = SelectInst(pe, stid);
            bool shouldBreak = processInvalid(invalidStid, pe, stid, peDecWidth, inst);
            if (shouldBreak) {
                break;
            } else if (!inst) {
                continue;
            }

            inst->rid = top->prob[inst->stid]->getAllocPtr();
            top->prob[inst->stid]->allocROB(inst);
            statsInfo[0].allocROBCnt++;
            statsInfo[0].allocROBTotalCnt++;

            // 暂不支持
            // getRefInfo(dinst, true);
            SetLoadStoreID(inst);
            // Rename
            ASSERT(inst->stid != -1U);
            dec_ren_q->Write(inst);
            GetSim()->core->bctrl->RecordBccEfficient();
            stallInstArr[pe][stid] = std::shared_ptr<SimInstInfo>(nullptr);
            stid = (stid + 1) % GetSim()->core->configs.scalar_smt_thread;
            ++i;
        }
    }
    RecordDecoderStats();
}

SimInst DCTop::SelectbyInstQ(uint32_t peid, std::deque<SimInst> &instQ, std::deque<SimInst>::const_iterator it,
    ROBID &oldestBid, BlockCommandPtr &cmd)
{
    if (instQ.empty() || it == instQ.cend()) {
        return nullptr;
    }

    SimInst inst = (*it);
    uint32_t stid = inst->stid;
    ROBID instBid = inst->bid;

    if (instBid != oldestBid) {
        return nullptr;
    }

    instQ.erase(it);
    // 普通标量块，以dinst->last 为结尾。last 指令执行完成块提交。
    // 并行块，标量指令，仅表示块完整，可以发给对应异构core执行，BSTOP 有其他获取信息的作用，因此自动产生BSTOP。
    if (inst->isLastInBlock || (IsBlockTypeParallel(cmd->blockType) && inst->opcode == Opcode::OP_BSTOP)) {
        blockSplitDCTopArr[peid][stid].pop_front();
    }
    return inst;
}

SimInst DCTop::SelectByStid(uint32_t peid, uint32_t stid, ROBID &oldestBid, BlockCommandPtr &cmd)
{
    if (CheckPEStall(peid, stid)) {
        return nullptr;
    }

    std::deque<SimInst> &instQ = inst_dc_top_arr[peid];
    std::deque<SimInst> &ctQ = inst_ct_top_arr[peid];

    auto getIt = [stid](const std::deque<SimInst> &iQ) {
        std::deque<SimInst>::const_iterator it = iQ.cend();
        if (!iQ.empty()) {
            for (it = iQ.cbegin(); it != iQ.cend(); ++it) {
                if ((*it)->stid == stid) {
                    return it;
                }
            }
        }
        return it;
    };

    SimInst inst = SelectbyInstQ(peid, instQ, getIt(instQ), oldestBid, cmd);
    if (inst) {
        return inst;
    }

    inst = SelectbyInstQ(peid, ctQ, getIt(ctQ), oldestBid, cmd);
    if (inst) {
        return inst;
    }

    return nullptr;
}

SimInst DCTop::SelectInst(uint32_t peid, uint32_t stid)
{
    ASSERT(stid != -1U);
    if (blockSplitDCTopArr[peid][stid].empty()) {
        return nullptr;
    }

    if (stallInstArr[peid][stid]) {
        return stallInstArr[peid][stid];
    }

    ROBID oldestBid = blockSplitDCTopArr[peid][stid].front()->bid;
    BlockCommandPtr cmd = GetSim()->core->bctrl->blockROB.GetNextBlockCMDPtr(oldestBid, stid);
    return SelectByStid(peid, stid, oldestBid, cmd);
}

void DCTop::SetLoadStoreID(SimInst &inst)
{
    BlockCommandPtr cmd = GetSim()->core->bctrl->blockROB.GetNextBlockCMDPtr(inst->bid, inst->stid);
    InstGroup grp = OpcodeManager::Inst().GetOpcodeGroup(inst->opcode);
    uint32_t stid = inst->stid;
    switch (grp) {
        case InstGroup::BLOCK_SPLIT:
            if (inst->opcode == Opcode::OP_BSTOP) {
                break;
            }
            cmd->startSID = storeCount[stid];
            cmd->startLoadID = loadCount[stid];
            if (cmd->tileOp == TileOp::TLOAD) {
                loadCount[stid]++;
            }
            if (cmd->tileOp == TileOp::TSTORE) {
                storeCount[stid]++;
            }
            break;
        case InstGroup::LOAD:
            inst->load_id = loadCount[stid];
            cmd->loadInstCount++;
            loadCount[stid]++;
            break;
        case InstGroup::STORE:
            inst->sid = storeCount[stid];
            cmd->storeInstCount++;
            storeCount[stid]++;
            break;
        default:
            break;
    }
}

bool DCTop::checkDCInStall(uint32_t peID, uint32_t inputCnt)
{
    return inst_top_dec_arr[peID].size() + inputCnt >= MAX_BF_SIZE;
}

bool DCTop::checkCTInStall(uint32_t peID, uint32_t inputCnt)
{
    return inst_top_ct_arr[peID].size() + inputCnt >= MAX_BF_SIZE;
}

bool DCTop::checkCTStall(uint32_t stid)
{
    // if (lastHeader && (lastHeader->opcode == BlockOpcode::MEM_COPY || lastHeader->opcode == BlockOpcode::MEM_SET)) {
    if (lastHeader[stid]) {
        if (waitMcopy[stid]) {
            LOG_INFO_M(Unit::BCC, Stage::D2) << "Wait for reg-data "<< lastHeader[stid];
            if (DEBUG_VERBOSE_ON || GetSim()->core->bctrl->configs.verbose) {
                LOG_DEBUG_STRUCT(GetSim()->core->bctrl->debugLogger, Unit::BCC, Stage::D1, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                                 lastHeader[stid], "Wait for reg-data, lastHeader:");
            }
            return true;
        }

        if (readFile[stid]) {
            LOG_INFO_M(Unit::BCC, Stage::D2) << "Reg-data return " << lastHeader[stid];
            if (DEBUG_VERBOSE_ON || GetSim()->core->bctrl->configs.verbose) {
                LOG_DEBUG_STRUCT(GetSim()->core->bctrl->debugLogger, Unit::BCC, Stage::D1, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                                 lastHeader[stid], "Reg-data return, lastHeader:");
            }
            calcLSCnt(lastHeader[stid]);
        }
    }

    return false;
}

bool DCTop::checkPEStall(uint32_t pe)
{
    bool robStall = false;
    for (uint32_t stid = 0; stid < top->prob.size(); ++stid) {
        robStall = top->prob[stid]->getROBStall();
        uint64_t peDecWidth = top->configs.decodeWidth;
        uint32_t instCnt = inst_dc_top_arr[pe].size() + inst_ct_top_arr[pe].size();
        uint32_t width = instCnt > peDecWidth ? peDecWidth : instCnt;
        if (robStall) {
            RecordRobStall(width, stid);
            top->stats->slotsRobStall += (peDecWidth - statsInfo[pe].allocROBCnt);
            LOG_INFO_M(Unit::BCC, Stage::D2) << "Decode Stall. PE " << pe << " list stall " << ", rob stall "
                    << robStall;
            return true;
        }
    }

    return false;
}

bool DCTop::CheckPEStall(uint32_t pe, uint32_t stid)
{
    bool robStall = false;
    robStall = top->prob[stid]->getROBStall();
    uint64_t peDecWidth = top->configs.decodeWidth;
    uint32_t instCnt = inst_dc_top_arr[pe].size() + inst_ct_top_arr[pe].size();
    uint32_t width = instCnt > peDecWidth ? peDecWidth : instCnt;
    if (robStall) {
        RecordRobStall(width, stid);
        top->stats->slotsRobStall += (peDecWidth - statsInfo[pe].allocROBCnt);
        LOG_INFO_M(Unit::BCC, Stage::D2) << "Decode Stall. PE " << pe << " list stall " << ", rob stall "
                << robStall << " stid " << stid;
        return true;
    }

    return false;
}

SimSys *DCTop::GetSim()
{
    return sim;
}

void DCTop::Reset()
{

}

void DCTop::Xfer()
{
    for (uint32_t i = 0; i < decoderDev.size(); ++i) {
        decoderDev[i].Xfer();
        templateDev[i].Xfer();
    }
}

void DCTop::Build()
{
    uint32_t peCount = GetSim()->core->configs.stdPeCount;
    inst_top_dec_arr.resize(peCount);
    inst_dc_top_arr.resize(peCount);
    blockSplitDCTopArr.resize(peCount);
    for (uint32_t i = 0; i < peCount; ++i) {
        blockSplitDCTopArr[i].resize(GetSim()->core->configs.scalar_smt_thread);
    }
    decoderDev.resize(peCount);
    for (uint32_t i = 0; i < inst_top_dec_arr.size(); ++i) {
        decoderDev[i].sim = sim;
        decoderDev[i].top = this;
        decoderDev[i].pinst_top_dec_q = &inst_top_dec_arr[i];
        decoderDev[i].pinst_dc_top_q = &inst_dc_top_arr[i];
        decoderDev[i].pblock_split_dc_top_q.resize(GetSim()->core->configs.scalar_smt_thread);
        for (size_t stid = 0; stid < GetSim()->core->configs.scalar_smt_thread; ++stid) {
            decoderDev[i].pblock_split_dc_top_q[stid] = &blockSplitDCTopArr[i][stid];
        }
        decoderDev[i].Build(i);
    }

    inst_top_ct_arr.resize(peCount);
    inst_ct_top_arr.resize(peCount);
    templateDev.resize(peCount);
    for (uint32_t i = 0; i < inst_top_ct_arr.size(); ++i) {
        templateDev[i].sim = sim;
        templateDev[i].top = this;
        templateDev[i].pinst_top_ct_q = &inst_top_ct_arr[i];
        templateDev[i].pinst_ct_top_q = &inst_ct_top_arr[i];
        templateDev[i].Build(i);
    }

    stallInstArr.resize(peCount);
    for (uint32_t i = 0; i < peCount; ++i) {
        stallInstArr[i].resize(GetSim()->core->configs.scalar_smt_thread);
    }

    statsInfo.resize(peCount + GetSim()->core->configs.simtPeCount + GetSim()->core->configs.memPeCount);
    uint32_t smtCount = GetSim()->core->configs.scalar_smt_thread;
    lastHeader.resize(smtCount, nullptr);
    readFile.resize(smtCount, false);
    waitMcopy.resize(smtCount, false);
    loadCount.resize(smtCount, 0);
    storeCount.resize(smtCount, 0);
    mtcLoadCount.resize(smtCount, 0);
    mtcStoreCount.resize(smtCount, 0);
    regData.resize(smtCount, std::vector<uint64_t>());
}

void DCTop::flush(FlushBus &flushReq)
{
    if (GetSim()->core->IsVectorIex(flushReq.req.iexTyp) || ((flushReq.req.iexTyp == MEM_IEX))) {
        return;
    }
    uint32_t stid = flushReq.req.stid;
    for (auto &dev : decoderDev)
        dev.flush(flushReq);

    for (size_t i  = 0; i < GetSim()->core->configs.stdPeCount; i++)
        templateDev[i].flush(flushReq);

    for (auto &arr : stallInstArr) {
        for (auto &stallInst : arr) {
            if (stallInst && stallInst->stid == flushReq.req.stid && LessEqual(flushReq.req.bid, stallInst->bid)) {
                stallInst = std::shared_ptr<SimInstInfo>(nullptr);
            }
        }
    }

    if (lastHeader[stid] && LessEqual(flushReq.req.bid, lastHeader[stid]->bid)) {
        lastHeader[stid] = std::shared_ptr<FullBlockHeader>(nullptr);
    }

    BlockCommandPtr cmd = GetSim()->core->bctrl->blockROB.GetBlockCMDPtr(flushReq.req.bid, flushReq.req.stid);
    /* Nuke flush 的时候，会走这个分支，此时会从块头往下的一条指令取值，所以需要把块头给恢复到对应队列中 */
    if (flushReq.req.fetchTPCVld) {
        blockSplitDCTopArr[flushReq.req.peID][flushReq.req.stid].push_back(cmd->machineInst);
        if (cmd->opcode != Opcode::OP_INVALID) {
            LOG_DEBUG_M(Unit::BCC, Stage::NA) << "Push a new header: " << cmd->machineInst->Dump();
            inst_top_ct_arr[flushReq.req.peID].push_back(cmd->machineInst);
        }
    }
    if (!flushReq.req.fetchTPCVld || cmd == nullptr) {
        ROBID bid = flushReq.req.bid;
        SubROBID(bid, 1, GetSim()->core->configs.block_rob_depth);
        BlockCommandPtr lastBlkCmd = GetSim()->core->bctrl->blockROB.GetBlockCMDPtr(bid, stid);
        loadCount[stid] = lastBlkCmd->startLoadID + lastBlkCmd->loadInstCount;
        storeCount[stid] = lastBlkCmd->startSID + lastBlkCmd->storeInstCount;
        return;
    }
    loadCount[stid] = cmd->startLoadID;
    storeCount[stid] = cmd->startSID;

    waitMcopy[stid] = false;
    readFile[stid] = false;
    if (cmd->opcode == Opcode::OP_FENTRY) {
        storeCount[stid] += cmd->storeInstCount;
    } else if (cmd->opcode == Opcode::OP_FEXIT || cmd->opcode == Opcode::OP_FRET_RA) {
        loadCount[stid] += cmd->loadInstCount;
    } else if (cmd->opcode == Opcode::OP_MCOPY || cmd->opcode == Opcode::OP_MSET) {
        waitMcopy[stid] = true;
    }
}

uint64_t calculte_bits(uint32_t mask) {
    uint64_t num = 0;
    while(mask != 0) {
        if (mask & 1) {
            ++num;
        }
        mask >>= 1;
    }
    return num;
}

uint64_t CountOfMcopy(uint64_t dstAddr, uint64_t srcAddr, uint64_t ldCnt, uint64_t& stNum) {
    uint64_t num = 0;
    uint64_t idx = 8;
    uint64_t stCnt = 0;
    dstAddr &= 0x7;
    srcAddr &= 0x7;
    if (ldCnt == 0) {
        return 0;
    }
    if (srcAddr == dstAddr) {
        if (dstAddr != 0) {
            num += 2;
            stCnt += min(8 - srcAddr, ldCnt);
            ldCnt -= stCnt;
            stNum++;
            stCnt = 0;
        }
    } else if (srcAddr > dstAddr) {
        if (dstAddr != 0) {
            num += 2;
            stCnt += min(idx - srcAddr, ldCnt);
            ldCnt -= stCnt;
            if (ldCnt > 0) {
                num += 1;
                stCnt += min(idx, ldCnt);
                ldCnt -= min(idx, ldCnt);
                stNum++;
                stCnt -= min(idx - dstAddr, stCnt);
            } else {
                stNum++;
                stCnt -= min(8 - dstAddr, stCnt);
            }
        } else {
            num += 1;
            stCnt += min(idx - srcAddr, ldCnt);
            ldCnt -= stCnt;
        }
    } else {
        num += 2;
        stCnt += min(idx - srcAddr, ldCnt);
        ldCnt -= stCnt;
        stNum++;
        stCnt -= min(idx - dstAddr, stCnt);
    }
    uint64_t temp = ldCnt / idx;
    num += temp;
    stNum += temp;
    stCnt += (ldCnt % idx);
    if ((ldCnt % idx) > 0) {
        num++;
    }
    if (stCnt >= idx) {
        stNum++;
        stCnt -= idx;
    }
    if (stCnt > 0) {
        switch (stCnt) {
            case 1:
            case 2:
            case 4:
                stNum ++;
                break;
            case 3:
            case 5:
            case 6:
                stNum += 2;
                break;
            case 7:
                stNum += 3;
                break;
            default: break;
        }
    }
    return num;
}

uint64_t storeCountOfMset(uint64_t addr, uint64_t data, int64_t count) {
    uint64_t num = 0;
    while (count > 0) {
        int64_t off = 0;
        if ((data & 0xff) == 0 && (addr & 0x3f) == 0 && count >= 64) {
            off = 64;
        } else if ((addr & 0x7) == 0 && count >= 8) {
            off = 8;
        } else if ((addr & 0x3) == 0 && count >= 4) {
            off = 4;
        } else if ((addr & 0x1) == 0 && count >= 2) {
            off = 2;
        } else {
            off = 1;
        }
        num++;
        count -= off;
        addr += off;
        if (num >= NUM65580) {  // Exit directly when the number of sd exceeds the upper limit
            return num;
        }
    }
    return num;
}

void DCTop::calcLSCnt(BHeader &header)
{
    header->loadCount = 0;
    header->storeCount = 0;
    uint32_t stid = header->stid;

    // Inline-inst
    if (header->isInst && (lastHeader[stid] && !lastHeader[stid]->isTemplate)) {
        Opcode opcode = preDecode(header);
        if (OpcodeIsLoad(opcode)) {
            lastHeader[stid]->loadCount++;
        } else if (OpcodeIsStore(opcode)) {
            lastHeader[stid]->storeCount++;
        }
    }

    // New header or last inst
    if (lastHeader[stid] && (!header->isInst || header->isLast)) {
        // Record load/store cnt in a new header.
        loadCount[header->stid] = lastHeader[stid]->loadCount + lastHeader[stid]->startLoadID;
        storeCount[header->stid] = lastHeader[stid]->storeCount + lastHeader[stid]->startSID;
    }

    // New header, read the record info
    if (!header->isInst) {
        header->startLoadID = loadCount[header->stid];
        header->startSID = storeCount[header->stid];
    }

    // Tempate calculation
    if (header->opcode == Opcode::OP_FEXIT || header->opcode == Opcode::OP_FRET_RA || header->opcode == Opcode::OP_FRET_STK) {
        header->loadCount = calculte_bits(header->setBitMask) - 1;
        return;
    }

    if (header->opcode == Opcode::OP_FENTRY) {
        int m = max(2, (int)header->RegSrc0);
        int n = min(23, (int)header->RegSrc1);
        if (m > n) {
            header->storeCount = 24 + n - m - 1;
        } else {
            header->storeCount = n - m + 1;
        }
        return;
    }

    if (header->opcode == Opcode::OP_MCOPY && readFile[stid]) {
        header->loadCount = CountOfMcopy(regData[stid][0], regData[stid][1], regData[stid][2], header->storeCount);
        readFile[stid] = false;
        return;
    }

    if (header->opcode == Opcode::OP_MSET && readFile[stid]) {
        header->storeCount = storeCountOfMset(regData[stid][0], regData[stid][1], (int64_t)regData[stid][2]);
        if (header->storeCount >= NUM65580) {
            GetSim()->core->bctrl->blockROB.reportException(header->bid, header->stid,
                "Exceptional store count in MEM_SET");
        }
        readFile[stid] = false;
        return;
    }

    // BranchType::ECALL
    if (header->type == BlockType::BLK_TYPE_XB && header->branchType == BranchType::BLK_BR_CALL) {
        ReferenceInfo &ref = GetSim()->refInfo;
        uint32_t totalInstCnt = ref.getEcallInstCount(header->bpc);
        const uint32_t loopLen = 3;
        header->storeCount = (totalInstCnt - static_cast<uint32_t>(GPR::GPR_COUNT)) / loopLen;
    }

    if (header->tileOp == TileOp::TLOAD) {
        if (GetSim()->core->configs.mtc_tls_enable) {
            header->load_id = mtcLoadCount[stid];
            ++mtcLoadCount[stid];
        } else {
            header->loadCount = 1;
            ++loadCount[header->stid];
        }
    }
    if (header->tileOp == TileOp::TSTORE) {
        if (GetSim()->core->configs.mtc_tls_enable) {
            header->sid = mtcStoreCount[stid];
            ++mtcStoreCount[stid];
        } else {
            header->storeCount = 1;
            ++storeCount[header->stid];
        }
    }
}

Opcode DCTop::preDecode(BHeader &header)
{
    MInst inst = MInst();
    uint32_t width = IsBlockTypeNeedVReg(header->type) ? WIDTH_64 : ((header->inlineBin & 0x1)==0 ? WIDTH_16 : WIDTH_32);
    inst.DecodeBin(header->inlineBin, width);
    inst.pc = header->bpc;
    return inst.opcode;
}

void DCTop::getRefInfo(SimInst &inst, bool needCheck) {
    (void)inst;
    (void)needCheck;
    // if (!GetSim()->perfectSimON || !needCheck) return;
    // if (GetSim()->perfectLoadStore) {
    //     BHeader header = GetSim()->core->bctrl->blockROB.getBlockHeader(inst->bid);
    //     uint32_t curid = header->curBid % GetSim()->core->configs.block_rob_depth;
    //     if (GetSim()->core->configs.bp_mode == 0) {
    //         curid = inst->bid.val;
    //     }
    //     uint64_t rptr = GetSim()->refInfo.refTrace.entry[inst->bid.val].lsRPtr;
    //     uint64_t bpc = GetSim()->refInfo.refTrace.entry[curid].bpc;
    //     uint64_t q_size = GetSim()->refInfo.refTrace.entry[curid].lsInfoQ.size();
    //     rptr = inst->lsID.val;
    //     std::string ldStInfo;
    //     ldStInfo += (" lsinfo ptr:" + std::to_string(rptr));
    //     if (OpcodeIsStore(inst->opcode)) {
    //         ldStInfo += " store id:";
    //         ldStInfo += std::to_string(inst->ref.lsInfo.id);
    //     } else {
    //         ldStInfo += " load depend id:";
    //         if (inst->ref.lsInfo.depend_id.empty()) {
    //             ldStInfo += "none";
    //         }
    //         for (auto id : inst->ref.lsInfo.depend_id) {
    //             ldStInfo += "-";
    //             ldStInfo += std::to_string(id);
    //         }
    //     }
    //     if ((OpcodeIsLoad(inst->opcode) || OpcodeIsStore(inst->opcode)) &&
    //         inst->bpc == bpc && rptr < q_size) {
    //         inst->ref.lsInfo = GetSim()->refInfo.refTrace.entry[curid].lsInfoQ[rptr];
    //         if (DEBUG_VERBOSE_ON || GetSim()->core->bctrl->configs.verbose) {
    //             LOG_DEBUG(GetSim()->core->bctrl->debugLogger, Unit::BCC, Stage::D1, UINT32_MAX, UINT64_MAX, LogLevel::LOW,
    //                       "getRefInfo in perfectLS mode, %s", ldStInfo.c_str());
    //         }
    //         LOG_INFO_M(Unit::BCC, Stage::D1) << "getRefInfo in perfectLS mode, " << ldStInfo;
    //         GetSim()->refInfo.refTrace.entry[inst->bid.val].incLSRPtr();
    //     }
    // }

    // if(GetSim()->perfectGetSet && inst->bpc == GetSim()->refInfo.refTrace.entry[inst->bid.val].bpc) {
    //     uint64_t rptr = GetSim()->refInfo.refTrace.entry[inst->bid.val].gsRPtr;
    //     std::string ldStInfo;
    //     if (rptr >= GetSim()->refInfo.refTrace.entry[inst->bid.val].gsInfoQ.size()) {
    //         return;
    //     }
    //     inst->ref.gsInfo = GetSim()->refInfo.refTrace.entry[inst->bid.val].gsInfoQ[rptr];
    //     ldStInfo += " GS info ptr:";
    //     ldStInfo += std::to_string(rptr);
    //     if (DEBUG_VERBOSE_ON || GetSim()->core->bctrl->configs.verbose) {
    //         LOG_DEBUG(GetSim()->core->bctrl->debugLogger, Unit::BCC, Stage::D1, UINT32_MAX, UINT64_MAX, LogLevel::LOW,
    //                   "getRefInfo in perfectLS mode, %s", ldStInfo.c_str());
    //     }
    //     LOG_INFO_M(Unit::BCC, Stage::D1) << "getRefInfo in perfectLS mode, " << ldStInfo;
    //     GetSim()->refInfo.refTrace.entry[inst->bid.val].incGSRPtr();
    // }
}

void DCTop::RecordDecoderStats()
{
    for (uint32_t pe = 0; pe < GetSim()->core->configs.stdPeCount; ++pe) {
        uint64_t peDecWidth = top->configs.decodeWidth;
        bool isPEIdle = top->IsPEIdle();
        StatsInfo info = statsInfo[pe];

        statsInfo[pe].slotsTotal += peDecWidth;
        top->stats->decodeCycle++;
        if (info.allocROBCnt != 0) {
            top->stats->alloc_cycle++;
        }
        top->stats->slotsReceived = info.receiveInstCnt + info.receiveHdrCnt + info.receiveCTHdrCnt;
        top->stats->slotsCtGen += info.genCTInstCnt;
        top->stats->slotsDecoderSend += info.decodeInstCnt;
        top->stats->slots_rob_allocated += info.allocROBCnt;
        if (!isPEIdle) {
            top->stats->slots_fe_bubble += (peDecWidth - info.allocROBCnt);
            top->stats->slots_supply_bubble += info.supplyBubble;
            // TODO: stid
            if (GetSim()->core->bctrl->flushRecovering[0]) {
                top->stats->slots_flush_recover_bubble += (peDecWidth - info.allocROBCnt);
            }
        }
        top->stats->slotsRenameStall += info.renameBubble;
        if (isPEIdle) {
            top->stats->slots_idle += (peDecWidth - info.allocROBCnt);
            // TODO: stid
            if (GetSim()->core->bctrl->flushRecovering[0]) {
                top->stats->slots_flush_recover_idle += (peDecWidth - info.allocROBCnt);
            }
        }
        if (info.decoderStall || info.codeTemplateStall) {
            top->stats->slots_decode_stall += (info.genCTInstCnt + info.decodeInstCnt < peDecWidth) ?
                                                                peDecWidth - info.genCTInstCnt - info.decodeInstCnt : 0;
        }
        info.genCTInstCnt = info.decodeInstCnt = 0;
        info.decoderStall = info.codeTemplateStall = false;
    }
}

void DCTop::SetLastHeader(BHeader &header)
{
    uint32_t stid = header->stid;
    lastHeader[stid] = header;
}
} // namespace JCore
