#include "VecPEDecode.h"

#include "VecPE.h"
#include "ModelSpec.h"
#include "core/Core.h"

namespace JCore {

void VecPEDecode::Work()
{
    stall = false;
    // Rename stall
    if (dec_ren_q->getStall()) {
        stall = true;
        if (vpeTop->machineType == MachineType::VPE) {
            LOG_DEBUG_M(Unit::VECTOR, Stage::D2) <<", Rename stall Decode";
        } else if (vpeTop->machineType == MachineType::MPE) {
            LOG_DEBUG_M(Unit::MTC, Stage::D2) <<", Rename stall Decode";
        }
        return;
    }

    PickThread();
    GenVectorMemInst();
    DecodeInstBundle();
    RecordDecodeStats();
}

void VecPEDecode::Xfer()
{
    ifu_dec_q->Work();
    for (auto &q : ctReqQueue) {
        q->Work();
    }
    for (auto &q : ctGenInstQueue) {
        q->Work();
    }
}

void VecPEDecode::Reset()
{
    contextSwitch = false;
}

void VecPEDecode::ReportStat()
{

}

SimSys* VecPEDecode::GetSim()
{
    return vpeTop->sim;
}

void VecPEDecode::Build()
{
    uint32_t threadNum = vpeTop->configs.threadNum;
    ifu_dec_q = std::make_shared<SimQueue<DecodeBundle>>();
    ifu_dec_q->InitMaxSize(vpeTop->configs.threadNum);
    thEntryOcc.resize(vpeTop->configs.threadNum, 0);
    vcSid.resize(vpeTop->configs.threadNum, ROBID());
    for (size_t i = 0; i < threadNum; i++) {
        ctReqQueue.emplace_back(std::make_shared<SimQueue<CopyCtReqBus>>());
        ctGenInstQueue.emplace_back(std::make_shared<SimQueue<SimInst>>());
    }
}

void VecPEDecode::PickThread()
{
    PickIfuThread();
    PickCTThread();
}

void VecPEDecode::PickIfuThread()
{
    if (ifu_dec_q->Full()) {
        return;
    }
    const uint32_t threadCnt = vpeTop->configs.threadNum;
    uint32_t pickIdx = threadPtr;
    uint32_t robStallTh = 0;
    bool found = false;

    for (uint32_t i = 0; i < threadCnt; ++i) {
        uint32_t idx = (threadPtr + i) % threadCnt;
        if (ifuDecThdQ->Empty(idx)) {
            continue;
        }
        DecodeBundle&& checkBundle = ifuDecThdQ->Front(idx);
        ASSERT(checkBundle.mask != -1U);
        if (!vpeTop->prob[idx]->GetRobPreAllocStall(thEntryOcc[idx], checkBundle.mask)) {
            pickIdx = idx;
            threadPtr = (idx + 1) % threadCnt;  // Round-Robin Selection, point to next thread
            found = true;
            vpeTop->stats->threadPickCnt[idx]++;
            break;
        }

        ASSERT(idx < 4); // cout array size is 4
        vpeTop->stats->threadRobStall[idx]++;
        vpeTop->stats->logicROBStall[idx]++;
        robStallTh++;
        if (vpeTop->machineType == MachineType::VECTOR) {
            LOG_DEBUG_M(Unit::VECTOR, Stage::D0) << "pick thread, tid=" << idx << ", rob stall";
        } else if (vpeTop->machineType == MachineType::MTC) {
            LOG_DEBUG_M(Unit::MTC, Stage::D0) << "pick thread, tid=" << idx << ", rob stall";
        }
    }

    if (robStallTh == threadCnt) {
        vpeTop->stats->robStall++;
        ReportVecRobStall(vpeTop->configs.decodeWidth);
    }

    // UnStallIfu(); // 暂时没有看到对StallIFU 的调用，此处unstall 全部的IFU 是否也不合理。
    if (!found) {
        return;
    }
    DecodeBundle&& bundle = ifuDecThdQ->Read(pickIdx);  // 选择当前线程
    bundle.pipe_cycle->d0Cycle = GetSim()->getCycles();
    // In the decoding phase, only one thread is processed.
    ifu_dec_q->Write(bundle);
    ASSERT(bundle.tid == pickIdx);
    thEntryOcc[pickIdx]++;
    if (vpeTop->machineType == MachineType::VECTOR) {
        LOG_DEBUG_M(Unit::VECTOR, Stage::D0) << "write dec queue, tid=" << pickIdx << ", (Block:" << bundle.bid
            << ", Group:" << bundle.gid <<  ", fid:" << bundle.fid << "), rob occ:" << vpeTop->prob[pickIdx]->getRobSize()
            << ", thread_num:" << thEntryOcc[pickIdx];
    } else if (vpeTop->machineType == MachineType::MTC) {
        LOG_DEBUG_M(Unit::MTC, Stage::D0) << "write dec queue, tid=" << pickIdx << ", (Block:" << bundle.bid
            << ", Group " << bundle.gid <<  ", fid " << bundle.fid << "), rob occ:" << vpeTop->prob[pickIdx]->getRobSize()
            << ", thread_num:" << thEntryOcc[pickIdx];
    }
}

void VecPEDecode::PickCTThread()
{
    bool found = false;
    const uint32_t threadCnt = vpeTop->configs.threadNum;
    for (uint32_t i = 0; i < threadCnt; ++i) {
        uint32_t ptr = (ctThreadPtr + i) % threadCnt;
        if (CTPickMatch(ptr)) {
            found = true;
            ctThreadPtr = ptr;
            break;
        }
    }
    if (!found) {
        ctThreadPtr = (ctThreadPtr + 1) % threadCnt;
    }
}

bool VecPEDecode::CTPickMatch(uint32_t threadID)
{
    if (!ctGenInstQueue[threadID]->Empty()) {
        return true;
    }
    if (!ctReqQueue[threadID]->Empty() && !workThdQ[threadID]->Empty()) {
        const auto& curBlock = workThdQ[threadID]->Front();
        const auto& req = ctReqQueue[threadID]->Front();
        if (curBlock.bid == req.bid && req.gid == curBlock.gid) {
            return true;
        }
    }
    return false;
}

bool VecPEDecode::DecodeBinStall(uint32_t threadID, ROBID bid, ROBID gid)
{
    if (!ctGenInstQueue[threadID]->WRBothEmpty()) {
        return true;
    }
    for (auto it = ctCopyinReqQueue.cbegin(); it != ctCopyinReqQueue.cend(); ++it) {
        if (it->bid == bid && it->gid == gid) {
            return true;
        }
    }
    if (!ctReqQueue[threadID]->WRBothEmpty() && !workThdQ[threadID]->Empty()) {
        const auto& curBlock = workThdQ[threadID]->Front();
        auto idMatch = [&curBlock](CopyCtReqBus req) -> bool {
            return (curBlock.bid == req.bid && curBlock.gid == req.gid);
        };
        if (ctReqQueue[threadID]->MatchIf(idMatch)) {
            return true;
        }
    }
    return false;
}

void VecPEDecode::DecodeInstBundle()
{
    // In the decoding phase, only one thread is processed.
    if (!ctGenInstQueue[ctThreadPtr]->Empty() && workThdQ[ctThreadPtr]->Empty()) {
        ASSERT(false && "Empty vpe workloadQ at MCALL template") << " tid" << ctThreadPtr;
    }
    uint32_t decodeCnt = 0;
    uint64_t peDecWidth = vpeTop->configs.decodeWidth;
    // cppyout/copyin inst decode
    while (decodeCnt < peDecWidth && !ctGenInstQueue[ctThreadPtr]->Empty()) {
        if (vpeTop->prob[ctThreadPtr]->GetRobRealAllocStall()) {
            ReportVecRobStall(peDecWidth - decodeCnt);
            break;
        }
        SimInst inst = ctGenInstQueue[ctThreadPtr]->Read();
        DecodeCTInst(inst);
        if (OpcodeIsStore(inst->opcode)) {
            UpdateVcSid(inst->tid);
            UpdateInstSid(inst);
        }
        inst->lsID = vcSid[inst->tid];
        inst->rid = vpeTop->prob[inst->tid]->getAllocPtr();
        vpeTop->prob[inst->tid]->allocROB(inst);
        // use ld/st to differentiate copyin/copyout
        if (OpcodeIsStore(inst->opcode)) {
            vpeTop->vectorCore->m_vectorGS->DecodeCopyoutInst(inst->bid, inst->gid);
        }
        dec_ren_q->Write(inst);
        ++decodeCnt;
        if (inst->threadSwitchEnd) {
            contextSwitch = false;
            workThdQ[ctThreadPtr]->Read();
            if (!workThdQ[ctThreadPtr]->Empty() && workThdQ[ctThreadPtr]->Front().lastFlag) {
                workThdQ[ctThreadPtr]->Read();
            }
        }
        LOG_INFO_M(Unit::VECTOR, Stage::D1) << " from mcall template " << inst->Dump();
    }

    if (ifu_dec_q->Empty()) {
        return;
    }
    // In the decoding phase, only one thread is processed.
    auto bundle = ifu_dec_q->Front();
    uint32_t tid = bundle.tid;
    if (DecodeBinStall(tid, bundle.bid, bundle.gid)) {
        return;
    }
    if (workThdQ[tid]->Empty()) {
        ASSERT(false && "Empty vpe workloadQ") << " tid" << tid << " bundle bid" << bundle.Dump();
    }
    if (!CheckFetchBundle(bundle)) {
        return;
    }

    while (decodeCnt < peDecWidth && !ifu_dec_q->Empty() && !contextSwitch) {
        auto bundle = ifu_dec_q->Front();
        uint32_t tid = bundle.tid;
        // 此函数内使用next 状态，后续修正为current状态，参考BCtrl 的CheckBROBStall(blkSplitInstCnt);
        if (vpeTop->prob[tid]->GetRobRealAllocStall()) {
            ReportVecRobStall(peDecWidth - decodeCnt);
            break;
        }
        SimInst inst = DecodeInst(bundle, binPtr);
        if (!inst) {
            break;
        }

        // TODO: 这里识别到是global load，Context switch 开始拆指令
        if (inst->IsMcallLoadGlobal()) {
            contextSwitch = true;
            // m_workLoadQ flush
            // m_workLoadQ实际是指向Vec中的workThdQ
            // 因为已经对IFU Flush了，所以不需要ifu stall

            // CopyOut插指令
            CopyCtReqBus req;
            req.tid = inst->tid;
            req.tpc = inst->pc;
            req.groupSlotId = vpeTop->vectorCore->m_vectorGS->TriggerCopyout(inst->bid, inst->gid, inst->GetNextPC());
            req.transactionId = vpeTop->vectorCore->m_vectorGS->GetTransactionId(req.groupSlotId);
            req.isCopyOut = true;
            req.copyType = inst->pdsts_[DST0_IDX]->type;
            req.bid = inst->bid;
            req.gid = inst->gid;
            ASSERT(inst->stid != -1U);
            req.stid = inst->stid;
            req.shapelpinfo = inst->shapelpinfo;
            req.addrDataType = inst->psrcs_[SRC0_IDX]->dataType;  // 保存LOAD指令的地址数据类型
            ctReqQueue[inst->tid]->Write(std::move(req));
            ConvertLoadToAdd(inst);
            LOG_INFO_M(Unit::VECTOR, Stage::D1) << " mcall context switch " << inst->Dump();
        }

        if (OpcodeManager::Inst().GetOpcodeGroup(inst->opcode) == InstGroup::SPECIAL) {
            inst->opcode = Opcode::OP_ADDI;

            POperandPtr operand = std::make_shared<PhysicalOperand>(OperandType::OPD_SIMM, 0);
            operand->cvt = OPConvertType::OPCVT_S64;
            operand->width = OperandWidth::OPDW_D;
            operand->data = static_cast<int64_t>(0);
            inst->srcs.push_back(operand);
        }

        if (OpcodeIsStore(inst->opcode)) {
            UpdateVcSid(inst->tid);
            UpdateInstSid(inst);

            // 为VSCATTER指令设置groupSlotId和transactionId
            // 判断条件：accMemInfo存在 && 非local访问
            if (inst->accMemInfo && !inst->accMemInfo->local) {
                uint64_t slotId = vpeTop->vectorCore->m_vectorGS->GetSlotsID(inst->bid, inst->gid);
                inst->groupSlotId = slotId;
                inst->transactionId = vpeTop->vectorCore->m_vectorGS->GetTransactionId(slotId);
                LOG_DEBUG_M(Unit::VECTOR, Stage::D1) << "VSCATTER inst: Set groupSlotId="
                    << inst->groupSlotId
                    << ", transactionId=0x" << std::hex << inst->transactionId << std::dec
                    << " for " << inst->Dump();
            }
        }
        inst->lsID = vcSid[inst->tid];

        inst->rid = vpeTop->prob[inst->tid]->getAllocPtr();
        vpeTop->prob[tid]->allocROB(inst);
        ++binPtr;
        HandleLast(bundle);
        // temporal fix for scalar split blk
        if (!inst->isLastInBlock) {
            // not BSTOP
            dec_ren_q->Write(inst);
            LOG_INFO_M(Unit::VECTOR, Stage::D1) << " from ifu fetch " << inst->Dump() << " in " << bundle.Dump();
        } else {
            PEResolveBus peResolve = inst->GenRslvBus();
            vpeTop->iex_pe_rslv_array[inst->tid]->Write(peResolve);
        }
        ++decodeCnt;

        GetSim()->core->vectorTop->SetDecIns(vpeTop->vecCoreId); // 原有代码用的就是coreID，应该使用解码的数量？
        // MCALL Context switch flush
        if (contextSwitch) {
            auto match = [&inst](const DecodeBundle d) -> bool { return d.bid == inst->bid && d.gid == inst->gid; };
            for (uint32_t t = 0; t < thEntryOcc.size(); ++t) {
                auto matchIf = [&match, &t](const DecodeBundle d) -> bool {
                    return d.tid == t && match(d);
                };
                thEntryOcc[t] -= ifu_dec_q->MatchCount(matchIf);
            }
            ifu_dec_q->FlushIf(match);
            binPtr = 0;
            // IFU flush
            JCore::PE_IFU::FlushFrontInfo fInfo;
            fInfo.tid = inst->tid;
            fInfo.bid = inst->bid;
            fInfo.gid = inst->gid;
            fInfo.fid = bundle.fid;
            ASSERT(bundle.stid != -1U);
            fInfo.stid = bundle.stid;
            fInfo.tpc = bundle.tpc;
            fInfo.flush = false;
            fInfo.flushf3 = true;
            vpeTop->ifu.flushByLoad(fInfo);
            break;
        }
    }
}

void VecPEDecode::ConvertLoadToAdd(SimInst const& inst) const
{
    inst->opcode = Opcode::OP_ADD;
}

void VecPEDecode::HandleLast(const DecodeBundle &bundle)
{
    if (binPtr == bundle.mask) {
        ifu_dec_q->Read();
        binPtr = 0;
        thEntryOcc[bundle.tid]--;

        if (bundle.last) {
            uint32_t tid = bundle.tid;
            vpeTop->prob[tid]->setLastBlockEnd();

            auto it = workThdQ[tid]->Read();
            if (!workThdQ[tid]->Empty() && workThdQ[tid]->Front().lastFlag) {
                auto it = workThdQ[tid]->Read();
            }
        }
    }
}

void VecPEDecode::UpdateVcSid(uint32_t tid)
{
    IncROBID(vcSid[tid], vpeTop->configs.vcSidMax);
}

void VecPEDecode::UpdateInstSid(SimInst &dinst)
{
    dinst->vcSid = vcSid[dinst->tid];
}

void VecPEDecode::Dump()
{
    cout << "CT workThdQ Dump ++++++++++++++++++++" << endl;
    for (uint64_t i = 0; i < workThdQ.size(); i++) {
        RingQueue<BlockRunInfo>* workThd = workThdQ[i];
        for (uint64_t j = 0; j < workThd->Size(); j++) {
            BlockRunInfo& thd = (*workThd)[j];
            cout << "CT workThdQ[" << i << " tid][" << j << "].bid:" << thd.bid << endl;
            cout << "CT workThdQ[" << i << " tid][" << j << "].gid:" << thd.gid << endl;
        }
    }
    cout << "CT ifu_dec_q Dump ++++++++++++++++++++" << endl;
    if (!ifu_dec_q->Empty()) {
        cout << "CT ifu_dec_q.front.gid:" << ifu_dec_q->Front().gid << endl;
    } else {
        cout << "CT ifu_dec_q: Enmpty" << endl;
    }

    cout << "CT ifuDecThdQ Dump ++++++++++++++++++++" << endl;
    for (uint64_t i = 0; i < 4; i++) {
        auto queue = ifuDecThdQ->GetRawEntity(i);
        cout << "CT ifuDecThdQ[" << i << " tid].size" << queue.Size() << endl;
        for (uint64_t j = 0; j < queue.Size(); j++) {
            auto& item = queue.GetRawReadData()[j];
            cout << "CT ifuDecThd[" << i << " tid][" << j << "]" << endl;
            cout << "bid" << item.bid << endl;
            cout << "gid" << item.gid << endl;
            cout << "tid" << item.tid << endl;
        }
    }
}

SimInst VecPEDecode::DecodeInst(const DecodeBundle &bundle, uint32_t binIdx)
{
    SimInst inst = std::make_shared<SimInstInfo>(bundle.pipe_cycle);
    uint64_t bin = bundle.entry[binIdx];
    uint64_t tpc = bundle.tpcArr[binIdx];
    uint64_t iSize = bundle.isize[binIdx];
    bool is64Dst = false;
    inst->peID = vpeTop->peID;
    inst->coreID = vpeTop->vecCoreId;
    inst->bpc = bundle.bpc;
    inst->tid = bundle.tid;
    inst->gid = bundle.gid;
    inst->stid = bundle.stid;
    inst->lanes = bundle.shapelpinfo.validLaneNum;
    inst->Decode(tpc, bin, iSize, is64Dst, inst->lanes);
    inst->dwDstType = is64Dst;
    inst->predMask = bundle.shapelpinfo.GetSIMTMask();
    inst->logicalGID = bundle.gid.val;
    inst->shapelpinfo = bundle.shapelpinfo;
    inst->biqType = bundle.biqType;
    inst->first = bundle.first && (binIdx == 0); // 暂时想不到用处。
    inst->storeSplit = false;
    if (vpeTop->machineType == MachineType::VECTOR) {
        inst->iexType = ExecEngineTyp::SIMT_IEX;
    } else if (vpeTop->machineType == MachineType::MTC) {
        inst->iexType = ExecEngineTyp::MEM_IEX;
    }
    inst->bid = bundle.bid;
    // update pipecycle
    inst->pipeCycle->decodeCycle = GetSim()->getCycles();
    return inst;
}

bool VecPEDecode::CheckFetchBundle(DecodeBundle &bundle)
{
    if (!bundle.vld) {
        vpeTop->stats->slots_fe_bubble++;
        return false;
    }
    if (workThdQ[bundle.tid]->Front().bid != bundle.bid) {
        LOG_ERROR_M(vpeTop->machineType, Stage::D1) << "Fetch Bundle bid != workloadQ bid";
        ASSERT(false);
    }
    return true;
}

void VecPEDecode::RecordDecodeStats()
{
    if (allocROBCnt == 0) {
        return;
    }
    uint64_t decodeWidth = vpeTop->configs.decodeWidth;
    // generated instructions is less than the decoded width
    vpeTop->stats->slots_fe_bubble += (decodeWidth - allocROBCnt);
    vpeTop->stats->decode_cnt += allocROBCnt;
    ++vpeTop->stats->decodeCycle;
    // decoder and ifu working but no instruction generation
    if (allocROBCnt == 0 && !vpeTop->ifu.IsIfuIdle() && ifu_dec_q->Empty()) {
        vpeTop->stats->slots_idle += decodeWidth;
        return;
    }
}

void VecPEDecode::Flush(const FlushBus &flushReq)
{
    contextSwitch = false;
    if (stallInst && LessEqual(flushReq.req.bid, stallInst->bid)) {
        stallInst = std::shared_ptr<SimInstInfo>(nullptr);
    }

    auto flushIfu = [&, this](const DecodeBundle dec) -> bool {
        if (flushReq.baseOnThread) {
            if (dec.tid != flushReq.req.tid) {
                return false;
            }
        }
        return LessEqual(flushReq.req.bid, dec.bid);
    };
    for (uint32_t tid = 0.; tid < thEntryOcc.size(); ++tid) {
        auto matchIf = [&flushIfu, &tid](const DecodeBundle dec) -> bool {
            return flushIfu(dec) && dec.tid == tid;
        };
        thEntryOcc[tid] -= ifu_dec_q->MatchCount(matchIf);
    }
    if (!ifu_dec_q->Empty()) {
        auto bundle = ifu_dec_q->Front();
        if (flushIfu(bundle)) {
            binPtr = 0;
        }
    }
    ifu_dec_q->FlushIf(flushIfu);
    auto flushInst = [&flushReq](SimInst inst) -> bool {
        if (flushReq.baseOnThread) {
            if (inst->tid != flushReq.req.tid) {
                return false;
            }
        }
        return LessEqual(flushReq.req.bid, inst->bid);
    };

    for (auto &q : ctGenInstQueue) {
        q->FlushIf(flushInst);
    }
    auto flushCTCopy = [&flushReq](CopyCtReqBus req) -> bool {
        if (flushReq.baseOnThread) {
            if (req.tid != flushReq.req.tid) {
                return false;
            }
        }
        return LessEqual(flushReq.req.bid, req.bid);
    };
    for (auto &q : ctReqQueue) {
        q->FlushIf(flushCTCopy);
    }
    auto it = ctCopyinReqQueue.cbegin();
    while (it != ctCopyinReqQueue.cend()) {
        if (flushCTCopy(*it)) {
            it = ctCopyinReqQueue.erase(it);
        } else {
            ++it;
        }
    }
}

void VecPEDecode::ReportTagStall(uint32_t leftSlots)
{
    if (vpeTop->machineType != MachineType::VECTOR) {
        return;
    }

    if (!GetSim()->core->vectorTop->CheckLoadQEmpty(vpeTop->vecCoreId)) {
        GetSim()->core->vectorTop->SetPeMemoryBound(vpeTop->vecCoreId, leftSlots);
        return;
    }
    GetSim()->core->vectorTop->SetPeRegTagStall(vpeTop->vecCoreId, leftSlots);
}

void VecPEDecode::ReportVecRobStall(uint32_t leftSlots)
{
    if (vpeTop->machineType != MachineType::VECTOR) {
        return;
    }

    if (!GetSim()->core->vectorTop->CheckLoadQEmpty(vpeTop->vecCoreId)) {
        GetSim()->core->vectorTop->SetPeMemoryBound(vpeTop->vecCoreId, leftSlots);
        return;
    }
    GetSim()->core->vectorTop->SetPeRobStall(vpeTop->vecCoreId, leftSlots);
}

void VecPEDecode::DecodeCTInst(SimInst const& inst) const
{
    inst->peID = vpeTop->peID;
    inst->coreID = vpeTop->vecCoreId;
    inst->logicalGID = inst->gid.val;
    inst->first = false;
    inst->iexType = ExecEngineTyp::SIMT_IEX;
}

SimInst VecPEDecode::GenInitSimInst() const
{
    SimInst inst = std::make_shared<SimInstInfo>();
    return inst;
}


void VecPEDecode::GenStoreInstSrc(SimInst const& inst, const OperandType& type,
    const uint64_t& value, const uint32_t& srcIdx) const
{
    POperandPtr operand = std::make_shared<PhysicalOperand>(type, value);
    operand->isDst = false;
    if (srcIdx == 0) {
        operand->cvt = OPConvertType::OPCVT_S32;
        operand->width = OperandWidth::OPDW_W;
    } else if (srcIdx == 1 || srcIdx == 3) {
        operand->cvt = OPConvertType::OPCVT_S64;
        operand->width = OperandWidth::OPDW_D;
        operand->data = value;
    } else if (srcIdx == 2) {
        operand->cvt = OPConvertType::OPCVT_NO_CVT;
        operand->width = OperandWidth::OPDW_D;
        operand->shamt = 2;
    }
    inst->srcs.push_back(operand);
}

void VecPEDecode::GenLoadInstSrc(SimInst const& inst, const OperandType& type,
    const uint64_t& value, const uint32_t& srcIdx) const
{
    POperandPtr operand = std::make_shared<PhysicalOperand>(type, value);
    operand->isDst = false;
    if (srcIdx == 0) {
        operand->cvt = OPConvertType::OPCVT_S64;
        operand->width = OperandWidth::OPDW_D;
    } else if (srcIdx == 2) {
        operand->cvt = OPConvertType::OPCVT_S64;
        operand->width = OperandWidth::OPDW_D;
        operand->data = static_cast<int64_t>(value);
    } else if (srcIdx == 1) {
        operand->cvt = OPConvertType::OPCVT_NO_CVT;
        operand->width = OperandWidth::OPDW_D;
        operand->shamt = 2;
    }
    inst->srcs.push_back(operand);
}

void VecPEDecode::GenLoadInstDst(SimInst const& inst, const OperandType& type,
    const int64_t& value) const
{
    POperandPtr operand = std::make_shared<PhysicalOperand>(type, static_cast<uint64_t>(value));
    operand->cvt = OPConvertType::OPCVT_U32;
    operand->width = OperandWidth::OPDW_W;
    operand->isDst = true;
    inst->dsts.push_back(operand);
}

void VecPEDecode::RecordGenCycle(SimInst const& inst)
{
    const uint64_t currentCycle = GetSim()->getCycles();
    inst->pipeCycle->createCycle = currentCycle;
    inst->pipeCycle->f0Cycle = currentCycle;
    inst->pipeCycle->f1Cycle = currentCycle;
    inst->pipeCycle->f2Cycle = currentCycle;
    inst->pipeCycle->f3Cycle = currentCycle;
    inst->pipeCycle->f4Cycle = currentCycle;
    inst->pipeCycle->f5Cycle = currentCycle;
    inst->pipeCycle->decodeCycle = currentCycle;
}

void VecPEDecode::GenVectorMemInst()
{
    // 在PickThread的时候，ifuDecThdQ对应线程如果是空的，则会不会选择当前线程，在Context切换时，
    // 已经将对应线程的ifuDecThdQ给flush了，导致这一线程永远取不到
    // 识别是否需要插入copyin
    for (uint32_t threadID = 0; threadID < workThdQ.size() && !ctCopyinReqQueue.empty(); ++threadID) {
        for (auto it = ctCopyinReqQueue.begin(); it != ctCopyinReqQueue.end() && !workThdQ[threadID]->Empty(); ++it) {
            auto req = *it;
            ASSERT(req.needCopyIn);
            // copyout not done yet
            // wait copyout inst
            if (!ctGenInstQueue[req.tid]->Empty() && ctGenInstQueue[req.tid]->Front()->bid == req.bid
                && ctGenInstQueue[req.tid]->Front()->gid == req.gid) {
                continue;
            }
            // current thread occupied by other warp
            if (!ctGenInstQueue[threadID]->Empty()) { // wait all copyout inst drain empty
                continue;
            }
            // current group dispatch contiously
            const auto& workThd = workThdQ[threadID]->Front();
            if (workThd.bid != req.bid || req.gid != workThd.gid) {
                continue;
            }
            bool isFirst = vpeTop->vectorCore->m_vectorGS->CheckFirstDispatch(workThd.bid, workThd.gid);
            if (!isFirst) {
                CopyCtReqBus reqnew = CopyCtReqBus();
                reqnew = req;
                reqnew.tid = threadID;
                vpeTop->vectorCore->m_vectorGS->TriggerCopyin(workThd.bid, workThd.gid);
                reqnew.isCopyOut = false;
                reqnew.needCopyIn = false;
                reqnew.isCopyIn = true;
                reqnew.bid = workThd.bid;
                reqnew.gid = workThd.gid;
                reqnew.shapelpinfo = workThd.shapelpinfo;
                ctReqQueue[threadID]->Write(std::move(reqnew));
                it = ctCopyinReqQueue.erase(it);
                LOG_INFO_M(Unit::VECTOR, Stage::D1) << "Pick copyin to code generator B"
                    << dec << AsWrap(reqnew.bid) << ")-" << "G" << dec << reqnew.gid << "-" << "T" << dec << reqnew.tid;
                break;
            }
        }
    }

    uint32_t thread = ctThreadPtr;
    if (ctReqQueue[thread]->Empty()) {
        return;
    }
    static const uint32_t REG_GROUPS = 6;      // vt, vu, vm, vn, t, u
    // 寄存器类型映射
    OperandType regTypes[REG_GROUPS] = {
        OperandType::OPD_VTLINK,  // vt
        OperandType::OPD_VULINK,  // vu
        OperandType::OPD_VMLINK,  // vm
        OperandType::OPD_VNLINK,   // vn
        OperandType::OPD_TLINK,   // u
        OperandType::OPD_ULINK,   // t
    };
    const auto& req = ctReqQueue[thread]->Front();
    if (req.isCopyOut) {
        vpeTop->vectorCore->m_vectorGS->SetVgatherCounter(req.groupSlotId, 0);
        vpeTop->vectorCore->m_vectorGS->SetCopyoutCounter(req.groupSlotId, 24);
        ASSERT(req.tid == thread);
        GenMcallCopyOutCT(req, regTypes);
        CopyCtReqBus reqnew = req;
        ctReqQueue[thread]->Read();
        reqnew.isCopyOut = false;
        reqnew.needCopyIn = true;
        ctCopyinReqQueue.push_back(reqnew);
    } else if (req.isCopyIn) {
        ASSERT(req.tid == thread);
        GenMcallCopyInCT(req, regTypes);
        ctReqQueue[thread]->Read();
    }
}

void VecPEDecode::GenMcallCopyOutCT(const CopyCtReqBus& req, const OperandType* regTypes)
{
    static const uint32_t VREG_GROUPS = 4;      // vt, vu, vm, vn
    static const uint32_t REG_GROUPS = 6;      // vt, vu, vm, vn
    static const uint32_t NUMS_PER_GROUP = 4;     // 每组4个寄存器
    static const uint32_t ADDR_STRIDE = 256;           // 步长256字节 4 * 64 = 256
    static const uint32_t GROUP_SIZE = 1024;      // 每组大小：4 * 256 = 1024
    static const uint32_t SGPR_STREID = 8;
    static const uint32_t SGPR_GROUP_SIZE = 32;
    uint64_t ctPC = 0;
    uint32_t sgprBaseAddr = VREG_GROUPS * GROUP_SIZE;
    for (uint32_t i = 0; i < REG_GROUPS; i++) {
        for (uint32_t j = 0; j < NUMS_PER_GROUP; j++) {
            SimInst inst = GenInitSimInst();
            inst->biqType = BIQType::MCALL_IQ;
            inst->bpc = 0;
            inst->pc = ctPC;
            ctPC += GetEncodeLenVal(EncodeLen::ENL_L);
            inst->bid = req.bid;
            inst->gid = req.gid;
            inst->tid = req.tid;
            inst->stid = req.stid;
            inst->transactionId = req.transactionId;
            inst->groupSlotId = req.groupSlotId;
            inst->shapelpinfo = req.shapelpinfo;
            inst->storeSplit = false;
            inst->accMemInfo = std::make_shared<AaccelssMemInfo>();
            inst->accMemInfo->local = true;
            inst->accMemInfo->continuous = true;
            inst->autogen = true;
            if (j == 3 && regTypes[i] == req.copyType) {
                inst->isVgather = true;
            }
            // v.swi.local vt#1, [ta, lc0<<2, 0]
            // simm，ct 时直接赋值，不再需要移位。
            uint32_t offset = 0;
            if (OperandTypeIsSGPR(regTypes[i])) {
                inst->opcode = Opcode::OP_SDI;
                inst->lanes = 1;
                inst->codeLen = EncodeLen::ENL_L;
                inst->predMask = 1;
                offset = sgprBaseAddr + (i - VREG_GROUPS) * SGPR_GROUP_SIZE + j * SGPR_STREID;
            } else {
                inst->opcode = Opcode::OP_SWI;
                inst->lanes = req.shapelpinfo.validLaneNum;
                inst->codeLen = EncodeLen::ENL_V;
                inst->predMask = req.shapelpinfo.GetSIMTMask();
                offset = i * GROUP_SIZE + j * ADDR_STRIDE;
            }

            GenStoreInstSrc(inst, regTypes[i], NUMS_PER_GROUP - 1 - j, 0);  // vt#j
            GenStoreInstSrc(inst, OperandType::OPD_TO_GPR_REG, req.gprAddr, 1);
            if (!OperandTypeIsSGPR(regTypes[i])) {
                GenStoreInstSrc(inst, OperandType::OPD_LC0, 0, 2);  // LC0
            }
            GenStoreInstSrc(inst, OperandType::OPD_SIMM, offset, 3);  // imm
            inst->ConvertPOperand(inst->lanes);
            // VGATHER: 设置SRC0(GM地址)的dataType
            if (inst->isVgather) {
                inst->psrcs_[SRC0_IDX]->dataType = req.addrDataType;
            }
            RecordGenCycle(inst);
            if (i == REG_GROUPS - 1 && j == NUMS_PER_GROUP - 1) {
                inst->threadSwitchEnd = true;
            }
            ASSERT(inst->stid != -1U);
            ctGenInstQueue[req.tid]->Write(inst);
            LOG_INFO_M(Unit::VECTOR, Stage::D1) << "Generate mcall copyout template " << inst->Dump();
        }
    }
}

void VecPEDecode::GenMcallCopyInCT(const CopyCtReqBus& req, const OperandType* regTypes)
{
    static const uint32_t VREG_GROUPS = 4;     // vt, vu, vm, vn
    static const uint32_t REG_GROUPS = 6;      // vt, vu, vm, vn t u
    static const uint32_t NUMS_PER_GROUP = 4;  // 每组4个寄存器
    static const uint32_t ADDR_STRIDE = 256;   // 步长256字节 4 * 64 = 256
    static const uint32_t GROUP_SIZE = 1024;   // 每组大小：4 * 256 = 1024
    static const uint32_t SGPR_STREID = 8;
    static const uint32_t SGPR_GROUP_SIZE = 32;
    uint64_t ctPC = 0;
    uint32_t sgprBaseAddr = VREG_GROUPS * GROUP_SIZE;
    for (uint32_t i = 0; i < REG_GROUPS; i++) {
        for (uint32_t j = 0; j < NUMS_PER_GROUP; j++) {
            SimInst inst = GenInitSimInst();
            inst->biqType = BIQType::MCALL_IQ;
            inst->bpc = 0;
            inst->pc = ctPC;
            ctPC += GetEncodeLenVal(EncodeLen::ENL_L);
            inst->bid = req.bid;
            inst->gid = req.gid;
            inst->tid = req.tid;
            inst->stid = req.stid;
            inst->transactionId = req.transactionId;
            inst->groupSlotId = req.groupSlotId;
            inst->shapelpinfo = req.shapelpinfo;
            inst->lanes = OperandTypeIsSGPR(regTypes[i]) ? 1 : req.shapelpinfo.validLaneNum;
            inst->codeLen = EncodeLen::ENL_V;
            inst->accMemInfo = std::make_shared<AaccelssMemInfo>();
            inst->accMemInfo->continuous = true;
            inst->accMemInfo->local = true;
            inst->autogen = true;

            // simm，ct 时直接赋值，不再需要移位。
            uint32_t loadOffset = 0;
            if (OperandTypeIsSGPR(regTypes[i])) {
                inst->opcode =  Opcode::OP_LDI;
                inst->lanes = 1;
                inst->codeLen = EncodeLen::ENL_L;
                inst->predMask = 1;
                loadOffset = sgprBaseAddr + (i - VREG_GROUPS) * SGPR_GROUP_SIZE + j * SGPR_STREID;
            } else {
                inst->opcode =  Opcode::OP_LWI;
                inst->lanes = req.shapelpinfo.validLaneNum;
                inst->codeLen = EncodeLen::ENL_V;
                inst->predMask = req.shapelpinfo.GetSIMTMask();
                loadOffset = i * GROUP_SIZE + j * ADDR_STRIDE;
            }
            GenLoadInstSrc(inst, OperandType::OPD_TO_GPR_REG, req.gprAddr, 0);
            if (!OperandTypeIsSGPR(regTypes[i])) {
                GenLoadInstSrc(inst, OperandType::OPD_LC0, 0, 1);  // LC0
            }
            GenLoadInstSrc(inst, OperandType::OPD_SIMM, loadOffset, 2);  // imm
            // GenTemplateInstSrc(inst, OperandType::OPD_GREG, baseAddr);  // 基址
            // GenTemplateInstSrc(inst, OperandType::OPD_UIMM, loadOffset); // 偏移
            GenLoadInstDst(inst, regTypes[i], 0); // 目标寄存器
            inst->ConvertPOperand(inst->lanes);

            RecordGenCycle(inst);
            ASSERT(inst->stid != -1U);
            ctGenInstQueue[req.tid]->Write(inst);
            LOG_INFO_M(Unit::VECTOR, Stage::D1) << "Generate mcall copyin template " << inst->Dump();
        }
    }
}

void VecPEDecode::SetScalarLaneMask(SimInst const& inst, OperandType type) const
{
    if (OperandTypeIsSGPR(type)) {
        inst->predMask = 1;
    }
}
} // namespace JCore
