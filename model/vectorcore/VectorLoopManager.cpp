#include "vectorcore/VectorLoopManager.h"

#include "core/Core.h"
#include "vectorcore/GROB.h"

namespace JCore {

void VectorLoopManager::Build()
{
    m_lc0 = 0;
    m_lc1 = 0;
    m_lc2 = 0;
    m_lb0 = 0;
    m_lb1 = 0;
    m_lb2 = 0;
}

void VectorLoopManager::Work()
{
    Dispatch();
    HandleInputReg();
    HandleOutputReg();
    HandleRegList();
    ReceiveRequest();
}

void VectorLoopManager::ReqQueryInfo()
{
    uint32_t i = 0;
    while (!handleInputRegisterQ.empty() && i < top->core->configs.input_reg_num) {
        LocalFreeInfo info = handleInputRegisterQ.front();
        handleInputRegisterQ.pop();
        rreq_vec_srf_q->Write(info);
        ++i;

        LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << top->m_coreId << "] Loop Manager]: Query info. bid "
            << info.bid << " offset " << info.offset;
    }
}

void VectorLoopManager::RespQueryInfo()
{
    while (!rrsp_srf_vec_q->Empty()) {
        LocalFreeInfo ptagInfo = rrsp_srf_vec_q->Read();

        LocalFreeInfo info = ptagInfo;
        data_vec_viex_q->Write(info);
        LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << top->m_coreId
            << "] Loop Manager]: Response info and write to vector iex. bid "
            << ptagInfo.bid << " offset " << ptagInfo.offset << " ptag " << dec << ptagInfo.ptag
            << " data 0x" << ptagInfo.data;
    }
}

void VectorLoopManager::RespViex()
{
    while (!data_viex_vec_q->Empty()) {
        LocalFreeInfo info = data_viex_vec_q->Read();
        lastInQ.push(info.bid);
        LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << top->m_coreId
            << "] Loop Manager]: Response last from iex. bid " << info.bid;
    }
}

void VectorLoopManager::HandleInputReg()
{
    RespViex();
    RespQueryInfo();
    ReqQueryInfo();
}

void VectorLoopManager::ReqPtagFromOutFreeList()
{
    while (!handleOutputRegisterQ.empty()) {
        LocalFreeInfo info = handleOutputRegisterQ.front();
        handleOutputRegisterQ.pop();
        wreq_vec_srf_q->Write(info);
        LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << top->m_coreId
            << "] Loop Manager]: Get ptag from freelist. bid " << info.bid << " offset " << info.offset;
    }
}

void VectorLoopManager::RespPtagFromOutFreeList()
{
    while (!wrsp_srf_vec_q->Empty()) {
        LocalFreeInfo info = wrsp_srf_vec_q->Read();
        if (!info.last) {
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << top->m_coreId << "] Loop Manager: RO Response from "
                << "freelist. bid " << info.bid << " offset" << info.offset << " ptag" << info.ptag;
        } else {
            lastOutQ.push(info.bid);
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << top->m_coreId << "] Loop Manager: RO Response from "
                << "freelist. bid " << info.bid << " last";
        }
    }
}

void VectorLoopManager::HandleOutputReg()
{
    RespPtagFromOutFreeList();
    ReqPtagFromOutFreeList();
}

void VectorLoopManager::HandleRegList()
{
    while (!handleRegisterCMDQ.empty()) {
        BlockCommandPtr cmd = handleRegisterCMDQ.front();
        handleRegisterCMDQ.pop_front();

        // Input reg list
        for (uint32_t i = 0; i < cmd->srcPtag.size(); i++) {
            LocalFreeInfo info;
            info.vld = true;
            info.bid = cmd->bid;
            info.stid = cmd->stid;
            info.ptag = cmd->srcPtag[i];
            info.data = cmd->srcData[i];
            info.offset = i;
            handleInputRegisterQ.push(info);
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << top->m_coreId
                << "] Loop Manager]: Handle input list. bid " << info.bid << " offset " << info.offset;
        }
        LocalFreeInfo in;
        in.vld = true;
        in.bid = cmd->bid;
        in.stid = cmd->stid;
        in.last = true;
        handleInputRegisterQ.push(in);

        // Output reg list
        for (uint32_t i = 0; i < cmd->dstPtag.size(); i++) {
            LocalFreeInfo info;
            info.vld = true;
            info.bid = cmd->bid;
            info.stid = cmd->stid;
            info.ptag = cmd->dstPtag[i];
            info.offset = i;
            handleOutputRegisterQ.push(info);
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << top->m_coreId
                << "] Loop Manager]: Handle output list. Bid " << info.bid << " offset " << info.offset;
        }
        LocalFreeInfo info;
        info.vld = true;
        info.bid = cmd->bid;
        info.stid = cmd->stid;
        info.last = true;
        handleOutputRegisterQ.push(info);

        workingCMDQ.push_back(cmd);
    }

    HandleInputReg();
    HandleOutputReg();
}

void VectorLoopManager::ReceiveRequest()
{
    if (top->GetSelectCore() != top->m_coreId) {
        return;
    }
    SendStashReq();
    RecvStashWayId();
    RecvStashRslvWakeup();
    IssueBlockcmdToExe();
}

void VectorLoopManager::HeadPrefetch(BlockCommandPtr cmd) const
{
    for (auto &src: cmd->srcs) {
        if (src->vld) {
            BlockCommandPtr bcmd = make_shared<BlockCommand>();
            bcmd->bid = cmd->bid;
            bcmd->stid = cmd->stid;
            bcmd->srcs.emplace_back(src);
            bcmd->dsts.emplace_back(src);
            bcmd->dsts[0]->dstNodeId = top->stations[0]->GetId();
            uint32_t base = src->baseAddr;
            uint32_t size = bcmd->dsts[0]->size;
            top->RegisterTileAddr(cmd->bid, cmd->stid, base, size);
            if (top->GetConfig().head_prefetch_en && top->pTileUtils->IsRingMode(false)) {
                uint32_t trainAddr = (base + top->GetConfig().head_prefetch_stride * 256) &
                    (static_cast<uint64_t>(~(255ULL)));
                top->m_vecta->TrainHPf(trainAddr, top->GetConfig().head_prefetch_size, bcmd->bid, bcmd->stid);
            }
        }
    }
}

void VectorLoopManager::SendStashReq()
{
    // Send stash request to stash control unit
    if (!bccVecBlockCmdQ->Empty()) {
        BlockCommandPtr cmd = bccVecBlockCmdQ->Read();
        cmd->coreId = top->m_coreId;
        if (top->GetConfig().stash_mode) {
            for (auto &src : cmd->srcs) {
                if (src->vld) {
                    BlockCommandPtr bcmd = make_shared<BlockCommand>();
                    bcmd->bid = cmd->bid;
                    bcmd->blockStartPC = cmd->blockStartPC;
                    bcmd->coreId = top->m_coreId;
                    bcmd->tileOp = TileOp::TSTASH_TA_INPUT;
                    bcmd->srcs.emplace_back(src);
                    bcmd->srcs[0]->dstNodeId = top->stations[0]->GetId();
                    stashCmdQ->Write(bcmd);
                    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "Src To stash Unit " << bcmd->Dump();
                }
            }
            for (auto &dst : cmd->dsts) {
                if (dst->vld && top->GetConfig().stash_to_enable) {
                    BlockCommandPtr bcmd = make_shared<BlockCommand>();
                    bcmd->bid = cmd->bid;
                    bcmd->blockStartPC = cmd->blockStartPC;
                    bcmd->coreId = top->m_coreId;
                    bcmd->tileOp = TileOp::TSTASH_TO_OUTPUT;
                    bcmd->dsts.emplace_back(dst);
                    bcmd->dsts[0]->dstNodeId = top->stations[0]->GetId();
                    stashCmdQ->Write(bcmd);
                    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "Dst To stash Unit " << bcmd->Dump();
                }
            }
        }
        cmdMonitor.push_back(cmd);
    }
}

void VectorLoopManager::RecvStashWayId()
{
    // Stash allocated wayid return
    auto setWayId = [](TileOperandPtr &operand, BlockCommandPtr &cmd) {
        for (auto &src : cmd->srcs) {
            if (src->vld && src->handType == operand->handType && src->tileTag == operand->tileTag) {
                src->wayId = operand->wayId;
                src->wayAllocated = true;
            }
        }
        for (auto &dst : cmd->dsts) {
            if (dst->vld && dst->handType == operand->handType && dst->tileTag == operand->tileTag) {
                dst->wayId = operand->wayId;
                dst->stashReady = true;
                dst->wayAllocated = true;
            }
        }
    };
    while (!stashAllocDoneQ->Empty()) {
        BlockCommandPtr bcmd = stashAllocDoneQ->Read();
        for (auto &cmd : cmdMonitor) {
            if (bcmd->bid != cmd->bid) {
                continue;
            }
            if (!bcmd->srcs.empty()) {
                setWayId(bcmd->srcs[0], cmd);
            }
            if (!bcmd->dsts.empty()) {
                setWayId(bcmd->dsts[0], cmd);
            }
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << "Way id return and set " << cmd->Dump();
        }
    }
}

void VectorLoopManager::RecvStashRslvWakeup()
{
    // stash resolve done, wakeup src ready
    while (!stashRslvQ->Empty()) {
        BlockCommandPtr bcmd = stashRslvQ->Read();
        TileOperandPtr &operand = bcmd->dsts[0];
        for (auto &cmd : cmdMonitor) {
            for (auto &src : cmd->srcs) {
                if (src->vld && src->handType == operand->handType && src->wayId == operand->wayId
                    && src->tileTag == operand->tileTag && src->wayAllocated) {
                    src->stashReady = true;
                    LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << top->m_coreId
                                                        << " Stash resolve return " << bcmd->Dump()
                                                        << " wakeup way: " << operand->wayId << " " << cmd->Dump();
                }
            }
        }
    }
}

void VectorLoopManager::IssueBlockcmdToExe()
{
    if (top->m_blockCmdBufferIDPool.empty() || cmdMonitor.empty()) {
        return;
    }

    for (auto it = cmdMonitor.cbegin(); it != cmdMonitor.cend(); ++it) {
        BlockCommandPtr cmd = *it;
        if ((top->GetConfig().stash_mode && cmd->StashReady(top->GetConfig().stash_to_enable)) ||
            !top->GetConfig().stash_mode) {
            LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[Vector Loop Manager]: recv block cmd:" << cmd->Dump();
            cmd->execMachineId = top->machineId;
            HeadPrefetch(cmd);
            for (auto &src : cmd->srcs) {
                if (src->vld) {
                    uint32_t base = src->baseAddr;
                    uint32_t size = src->size;
                    top->RegisterTileAddr(cmd->bid, cmd->stid, base, size);
                }
            }
            const uint32_t maxReadLGrp = 12;
            const uint32_t maxWrtiteLGrp = 4;
            if (top->m_lgprRF->CheckStallInput(maxReadLGrp)) {
                LOG_INFO_M(Unit::VECTOR, Stage::NA) << "lgprRF input stall";
                return;
            }
            if (top->m_lgprRF->CheckStallOutput(maxWrtiteLGrp)) {
                LOG_INFO_M(Unit::VECTOR, Stage::NA) << "lgprRF output stall";
                return;
            }
            top->InsertBlockCmdBuffer(cmd);
            handleRegisterCMDQ.push_back(cmd);
            cmdMonitor.erase(it);
            break;
        }
    }
}

void VectorLoopManager::Dump()
{
    cout << "===== vec LM Dump start: =====" << endl;
    for (auto it = cmdMonitor.cbegin(); it != cmdMonitor.cend(); ++it) {
        cout << (*it)->Dump() << endl;
    }
    cout << endl << "m_blockCmdBufferIDPool is empty: " << top->m_blockCmdBufferIDPool.empty() << endl;
    cout << "===== vec LM Dump end: =====" << endl;
}

bool VectorLoopManager::CheckDispEnable()
{
    return !lastInQ.empty() && !lastOutQ.empty() && lastInQ.front() == lastOutQ.front();
}

// loop Ctrl 分发
void VectorLoopManager::Dispatch()
{
    if (!workingCMDQ.empty()) {
        if (!lastInQ.empty()) {
        }
        if (!lastOutQ.empty()) {
        }
    }
    if (workingCMDQ.empty() || !CheckDispEnable()) {
        return;
    }

    BlockCommandPtr cmd = workingCMDQ.front();
    if (top->m_grob->CheckStall(1, cmd->stid)) {
        return;
    }

    lastInQ.pop();
    lastOutQ.pop();
    workingCMDQ.pop_front();
    uint32_t lb0 = cmd->lb0 == 0 ? 1 : cmd->lb0;
    uint32_t lb1 = cmd->lb1 == 0 ? 1 : cmd->lb1;
    uint32_t lb2 = cmd->lb2 == 0 ? 1 : cmd->lb2;
    uint64_t shapeSize = lb0 * lb1 * lb2;
    uint64_t groupNum = cmd->GetGroupNum(m_lanes);
    bool dimReduction = cmd->IsReduceDimension();
    uint64_t avgGroupNum = groupNum / m_thdN;
    if (avgGroupNum == 0) {
        avgGroupNum = 1;
    }
    uint64_t lastGroupNum = shapeSize % m_lanes;
    if (lastGroupNum == 0) {
        lastGroupNum = m_lanes;
    }

    GetSim()->GetVerifyManager(cmd->stid)->InitPARGroup(cmd->bid.val, groupNum);
    GetSim()->GetViewManager(cmd->stid)->InitBIQType(cmd->bid.val, BIQType::VEC_IQ);
    GetSim()->GetViewManager(cmd->stid)->InitPARGroup(cmd->bid.val,  groupNum);

    LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << top->m_coreId << "] Loop Manager]:  LM handle:"
        << cmd->Dump();
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << top->m_coreId << "] Loop Manager]:  generate group num: "
        << std::dec << groupNum;

    GroupAllocInfo grobAllocInfo;
    grobAllocInfo.bid = cmd->bid;
    grobAllocInfo.tpc = cmd->bTextPC;

    grobAllocInfo.groupNum = groupNum;
    if (cmd->dsts.size() > 0) {
        grobAllocInfo.tag = cmd->dsts[0]->tileTag;
        grobAllocInfo.dest = cmd->dsts[0]->baseAddr;
        grobAllocInfo.destVld = true;
    } else {
        grobAllocInfo.tag = 0;
        grobAllocInfo.dest = 0;
        grobAllocInfo.destVld = false;
    }
    grobAllocInfo.blockCmd = cmd;
    grobAllocInfo.tileId = cmd->bid.val;
    grobAllocInfo.createCycle = GetSim()->getCycles();
    grobAllocInfo.stid = cmd->stid;
    m_lm2GROB->Write(grobAllocInfo);

    VCore::GBufferAllocReq gBufferReq;
    ShapeLoopInfo shapeInfo = ShapeLoopInfo(lb0, lb1, lb2, groupNum, avgGroupNum, lastGroupNum, m_lanes);
    shapeInfo.dimReduction = dimReduction;
    gBufferReq.vld = true;
    gBufferReq.blockCmd = cmd;
    gBufferReq.shapelpinfo = shapeInfo;
    m_lm2GBufferQ->Write(gBufferReq);
    for (auto &src : cmd->srcs) {
        if (src->vld) {
            uint32_t base = src->baseAddr;
            uint32_t size = src->size;
            top->RegisterTileAddr(cmd->bid, cmd->stid, base, size);
        }
    }
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << top->m_coreId << "] Loop Manager]:  Dispatch cmd:"
        << cmd->Dump() << " to group buffer";
}

void VectorLoopManager::ReportCommitGroup(const ROBID &bid, const ROBID &gid, uint32_t stid)
{
    ASSERT(top->m_grob->Find(bid, gid, stid));
    top->m_grob->ReportCommitGroup(bid, gid, stid);
}

SimSys* VectorLoopManager::GetSim()
{
    return m_sim;
}

void VectorLoopManager::SetSim(SimSys *sim)
{
    m_sim = sim;
}

void VectorLoopManager::SetFlush(FlushBus &bus)
{
    LOG_INFO_M(Unit::VECTOR, Stage::NA) << " [VECTOR " << top->m_coreId << "] Loop Manager]:  Recv Flush " << bus;
    top->m_grob->SetFlush(bus);
    auto match = [&bus](LocalFreeInfo &info) {
        if (info.stid != bus.req.stid) {
            return false;
        }
        return LessEqual(bus.req.bid, info.bid);
    };

    rreq_vec_srf_q->FlushIf(match);
    rrsp_srf_vec_q->FlushIf(match);
    wreq_vec_srf_q->FlushIf(match);
    wrsp_srf_vec_q->FlushIf(match);
    req_vec_siex_q->FlushIf(match);
    rsp_siex_vec_q->FlushIf(match);
    data_vec_viex_q->FlushIf(match);
    data_viex_vec_q->FlushIf(match);

    for (auto it = cmdMonitor.begin(); it != cmdMonitor.end();) {
        if (LessEqual(bus.req.bid, (*it)->bid) && (*it)->stid == bus.req.stid) {
            it = cmdMonitor.erase(it);
        } else {
            ++it;
        }
    }

    /* flush BlockCommandPtr Queue */
    auto flushBlockCommandPtrQ = [&bus](std::deque<BlockCommandPtr>& q) {
        for (auto it = q.cbegin(); it != q.cend();) {
            if (LessEqual(bus.req.bid, (*it)->bid) && (*it)->stid == bus.req.stid) {
                it = q.erase(it);
            } else {
                it++;
            }
        }
    };

    flushBlockCommandPtrQ(workingCMDQ);
    flushBlockCommandPtrQ(handleRegisterCMDQ);
}

} // namespace JCore
