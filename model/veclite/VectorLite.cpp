#include "veclite/VectorLite.h"
#include "veclite/VectorLiteTop.h"

#include "core/Core.h"

namespace JCore {

VectorLite::VectorLite(uint32_t coreId) : m_coreId(coreId) {}
VectorLite::~VectorLite() {}

void VectorLite::SetVerboseOn()
{
    verbose = true;
}

void VectorLite::Work()
{
    if (!core->GetConfig().singleTierMode) {
        return;
    }
    WriteTileReg();
    exeUnit.Work();
    uiq.Work();
    siq.Work();
    rob.Work();
    stashUnit.Work();
    tileBuffer.Work();
    ReceivedCmd();
    Flush();
    m_stats.m_totalCycle++;
    if (!rob.IsIdle()) {
        m_stats.m_workingCycle++;
    }
}

void VectorLite::Xfer()
{
    if (!core->GetConfig().singleTierMode) {
        return;
    }
    exeUnit.Xfer();
    uiq.Xfer();
    siq.Xfer();
    rob.Xfer();
    stashUnit.Xfer();
    tileBuffer.Xfer();
    Flush();
    CheckBusy();

    m_fetchStashInQ.Work();
    m_fetchRobInQ.Work();
    m_fetchSiqInQ.Work();
    m_stashSiqWakeuQ.Work();
    m_siqUiqInQ.Work();
    m_siqRobDispQ.Work();
    m_siqRobDispLastQ.Work();
    drainScbDataQ.Work();
    drainScbRespQ.Work();
    m_uiqEXEInQ.Work();
    m_exeRobRslvQ.Work();
}

void VectorLite::Build()
{
    sim = top->GetSim();
    core = top->GetCore();
    config.overrideDefaultConfig(GetSim()->getCfgs());
    ubConfig.overrideDefaultConfig(GetSim()->getCfgs());

    m_stats.m_coreId = m_coreId;
    m_stats.rpt = GetSim()->getRpt();
    m_stats.Reset();

    BuildInterface();
    stashUnit.top = this;
    stashUnit.Build();
    rob.top = this;
    siq.top = this;
    uiq.top = this;
    exeUnit.top = this;
    tileBuffer.top = this;
    rob.Build();
    siq.Build();
    uiq.Build();
    exeUnit.Build();
    tileBuffer.Build();
}

void VectorLite::BuildInterface()
{
    bccVecliteFlushQ = &this->core->coreInterface.bCCVecliteFlushArray[m_coreId];
    bccVecliteBlockCmdQ = &this->core->coreInterface.bccVecliteBlockCmdQ;
    rob.m_fetchRobInQ = &m_fetchRobInQ;

    siq.m_fetchSiqInQ = &m_fetchSiqInQ;

    tileBuffer.tileRegLdReqQ = this->m_vecTileRegLdReqQ;
    tileBuffer.tileRegStReqQ = this->m_vecTileRegStReqQ;
    tileBuffer.tileRegLdResQ = this->m_tileRegVecLdResQ;
    tileBuffer.tileRegWrRspQ = this->m_tileRegVecWrResQ;
    tileBuffer.drainScbDataQ = &drainScbDataQ;
    rob.drainScbDataQ = &drainScbDataQ;
    tileBuffer.drainScbRespQ = &drainScbRespQ;
    rob.drainScbRespQ = &drainScbRespQ;

    stashUnit.m_fetchStashInQ = &m_fetchStashInQ;
    stashUnit.vecliteStashCmdQ = &this->core->coreInterface.vecliteStashCmdQ;
    stashUnit.stashAllocDoneQ = &this->core->coreInterface.stashVecliteAllocDoneArray[m_coreId];
    stashUnit.stashRslvQ = &this->core->coreInterface.stashVecliteRslvArray[m_coreId];

    stashUnit.m_stashSiqWakeuQ = &m_stashSiqWakeuQ;
    siq.m_stashSiqWakeuQ = &m_stashSiqWakeuQ;

    siq.m_siqUiqInQ = &m_siqUiqInQ;
    uiq.m_siqUiqInQ = &m_siqUiqInQ;
    siq.m_uiqStallFn = bind(&VectorLiteUOPIQ::CheckStall, &uiq, placeholders::_1);

    siq.m_siqRobDispQ = &m_siqRobDispQ;
    rob.m_siqRobDispQ = &m_siqRobDispQ;

    siq.m_siqRobDispLastQ = &m_siqRobDispLastQ;
    rob.m_siqRobDispLastQ = &m_siqRobDispLastQ;

    uiq.m_uiqEXEInQ = &m_uiqEXEInQ;
    exeUnit.m_uiqEXEInQ = &m_uiqEXEInQ;

    exeUnit.m_exeRobRslvQ = &m_exeRobRslvQ;
    rob.m_exeRobRslvQ = &m_exeRobRslvQ;
    rob.vecliteStashCompQ = &this->core->coreInterface.vecliteStashCompQ;
    rob.vecliteBccWakeupQ = &this->core->coreInterface.vecliteBccWakeupQ;
}

void VectorLite::ReceivedCmd()
{
    while (!bccVecliteBlockCmdQ->Empty()) {
        bool stashStall = stashUnit.CheckStall();
        bool robStall = rob.CheckStall();
        bool siqStall = siq.CheckStall();
        if (!stashStall && !robStall && !siqStall) {
            BlockCommandPtr cmd = bccVecliteBlockCmdQ->Read();
            cmd->reqCycle = GetSim()->getCycles();
            cmd->execMachineId = machineId;
            LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << GetCoreId() << "] "
                << "Fetch, receive: " << cmd->Dump();
            m_fetchStashInQ.Write(cmd);
            m_fetchRobInQ.Write(cmd);
            m_fetchSiqInQ.Write(cmd);
            continue;
        }
        if (stashStall) {
            m_stats.m_stashStallCycle += 1;
        }
        if (robStall) {
            m_stats.m_robStallCycle += 1;
        }
        if (siqStall) {
            m_stats.m_siqStallCycle += 1;
        }
        break;
    }
}

bool VectorLite::IsVecBusy()
{
    return !rob.IsIdle();
}

void VectorLite::CheckBusy()
{
    bool currentBusy = IsVecBusy();
    if (currentBusy && !lastCycleVectorBusy) {
        // free to busy
        vectorBusyStartCycle = GetSim()->getCycles();
    }
    if (!currentBusy && lastCycleVectorBusy) {
        // busy to free
        SwimLogData logData;
        logData.name = "Veclite busy";
        logData.pid = machineId;
        logData.tid = CORE_INTER_VIEW_TID;
        logData.sTime = vectorBusyStartCycle;
        logData.eTime = GetSim()->getCycles();
        logData.eventId = GetSim()->GetEventId();
        logData.hint = "Veclite busy";
        GetSim()->AddDuration(logData);
    }
    lastCycleVectorBusy = currentBusy;
}

bool VectorLite::HasReadPort(bool colSum, ROBID bid, uint32_t uopCnt, uint32_t latency)
{
    if (core->GetConfig().perfectAaccelssRF) {
        return true;
    }
    bool tileBankArbSuaccelss = core->CanReadTileReg(MachineType::VECLITE);
    if (!tileBankArbSuaccelss) {
        m_stats.m_tileRegRdStallCycle += 1;
        return false;
    }
    if (AllocWrBufferEntry(colSum, bid, uopCnt)) {
        core->AcquireReadTileReg(MachineType::VECLITE, latency);
        m_stats.m_tileRegRdCount += 1;
        return true;
    }
    return false;
}

/**
 * 分配写缓冲entry
 * @param bid block id
 * @param uopCnt 该块包含的uop数量
 * @return true 分配成功; false 空间不足（反压）
 */
bool VectorLite::AllocWrBufferEntry(bool colSum, ROBID bid, uint32_t uopCnt)
{
    uint32_t requiredEntries = colSum ? 1 :
        (uopCnt + config.tileWrBufferEntryUopNum - 1) / config.tileWrBufferEntryUopNum;
    if (static_cast<uint32_t>(tileRegWrBuffer.size()) + requiredEntries > config.tileWrBufferDepth) {
        return false;
    }

    for (uint32_t i = 0; i < requiredEntries; ++i) {
        tileRegWrBuffer.emplace_back(WrRegReq(bid));
    }

    return true;
}

void VectorLite::ResolveUop(ROBID bid)
{
    core->vectorRealWriteRFReqCnt++;
    if (core->GetConfig().perfectAaccelssRF) {
        return;
    }
    bool foundPendingEntry = false;

    // 遍历所有条目，找到对应 bid 且未 ready
    for (auto& entry : tileRegWrBuffer) {
        if (entry.bid == bid && !entry.dataReady) {
            foundPendingEntry = true;
            entry.resolveCnt++;
            if (entry.resolveCnt >= config.tileWrBufferEntryUopNum) {
                entry.dataReady = true;
            }
            break;
        }
    }

    if (!foundPendingEntry) {
        std::cout << "resolve error: no available entry for bid:" << bid.val
                  << " (all entries are dataReady)" << std::endl;
        ASSERT(false && "All entries for this bid are already resolved/ready");
    }
}

bool VectorLite::HasWrCredit() const
{
    return static_cast<uint32_t>(tileRegWrBuffer.size()) < config.tileWrBufferDepth;
}

bool VectorLite::ResolveTile(ROBID bid)
{
    if (core->GetConfig().perfectAaccelssRF) {
        return true;
    }
    bool found = false;
    // 将所有匹配 bid 且未 ready 的条目标记为 ready
    for (auto& entry : tileRegWrBuffer) {
        if (entry.bid == bid && !entry.dataReady) {
            entry.dataReady = true;
            found = true;
        }
    }

    return found;
}

void VectorLite::WriteTileReg()
{
    if (tileRegWrBuffer.empty()) {
        return;
    }
    bool tileBankArbSuaccelss = core->CanWriteTileReg(MachineType::VECLITE);
    if (!tileBankArbSuaccelss) {
        m_stats.m_tileRegWrStallCycle += 1;
        return;
    }
    // 查找第一个 dataReady 的entry
    auto it = std::find_if(tileRegWrBuffer.begin(), tileRegWrBuffer.end(),
                           [](const WrRegReq& entry) { return entry.dataReady; });
    if (it != tileRegWrBuffer.end()) {
        tileRegWrBuffer.erase(it);
        core->AcquireWriteTileReg(MachineType::VECLITE, core->GetConfig().vecWriteOccTileregLat);
        m_stats.m_tileRegWrCount += 1;
    } else {
        m_stats.m_tileRegWrDataNotRdyCycle += 1;
    }
}

// 获取dataReady的entry数量
uint32_t VectorLite::GetReadyCount() const
{
    return std::count_if(tileRegWrBuffer.begin(), tileRegWrBuffer.end(),
                         [](const WrRegReq& entry) { return entry.dataReady; });
}

// 打印buffer状态
void VectorLite::PrintStatus() const
{
    std::cout << "Vector Lite Core " << m_coreId << " Status:" << std::endl;
    std::cout << "  Used: " << tileRegWrBuffer.size() << "/" << config.tileWrBufferDepth << std::endl;
    std::cout << "  Ready: " << GetReadyCount() << std::endl;
}

// 打印buffer内容
void VectorLite::PrintBuffer() const
{
    std::cout << "\n========== Vector Lite Core " << m_coreId << " Content ==========" << std::endl;

    if (tileRegWrBuffer.empty()) {
        std::cout << "Buffer is empty" << std::endl;
    } else {
        for (size_t i = 0; i < tileRegWrBuffer.size(); ++i) {
            const WrRegReq& entry = tileRegWrBuffer[i];
            std::cout << "Entry[" << i << "]: BID=" << entry.bid
                      << ", DataReady=" << (entry.dataReady ? "YES" : "NO");
            if (entry.dataReady) {
                std::cout << " <-- Ready to write";
            }
            std::cout << std::endl;
        }
    }
    std::cout << "==============================================\n" << std::endl;
}

void VectorLite::InsertBlockCmdBuffer(const BlockCommandPtr &cmd) const
{
    (void)cmd;
}

void VectorLite::DeallocBlockCmdBuffer(const BlockCommandPtr &grobCmd) const
{
    (void)grobCmd;
}

void VectorLite::FlushCmdBuffer(const FlushBus& bus) const
{
    (void)bus;
}

void VectorLite::ReportStat()
{
    m_stats.Report();
}

void VectorLite::Reset()
{
}

SimSys* VectorLite::GetSim()
{
    return sim;
}

const VectorLiteCoreConfig& VectorLite::GetConfig() const
{
    return config;
}

bool VectorLite::IsSrcReady(uint32_t tagId)
{
    return (GetConfig().tileBufferEnable ? tileBuffer.IsSrcReady(tagId) : true);
}

void VectorLite::FreeSrcTileTag(uint32_t tagId)
{
    tileBuffer.ClearEntry(tagId);
}

bool VectorLite::AllocSrcCache(uint32_t tagId, OperandType handType, ROBID bid, uint32_t tileSize, uint64_t address)
{
    return tileBuffer.AllocateEntry(tagId, handType, bid, tileSize, address);
}

void VectorLite::Flush()
{
    if (bccVecliteFlushQ->Empty()) {
        return;
    }

    bccVecliteFlushQ->Read();
    Reset();
}

void VectorLite::DoCommonFlush(const FlushBus& bus)
{
    auto matchCmd = [&bus](const BlockCommandPtr& cmd) -> bool {
        return LessEqual(bus.req.bid, cmd->bid);
    };
    m_fetchStashInQ.FlushIf(matchCmd);
    m_fetchRobInQ.FlushIf(matchCmd);
    m_fetchSiqInQ.FlushIf(matchCmd);

    auto matchROBID = [&bus](ROBID& bid) -> bool {
        return LessEqual(bus.req.bid, bid);
    };
    m_stashSiqWakeuQ.FlushIf(matchROBID);

    auto matchUopInfo = [&bus](const VectorLiteUopT& uop) -> bool {
        return LessEqual(bus.req.bid, uop->cmd->bid);
    };
    m_siqUiqInQ.FlushIf(matchUopInfo);
    m_exeRobRslvQ.FlushIf(matchUopInfo);
    auto matchSIQInfo = [&bus](const SIQInfoPtr& info) -> bool {
        return LessEqual(bus.req.bid, info->cmd->bid);
    };
    m_siqRobDispQ.FlushIf(matchSIQInfo);
    m_siqRobDispLastQ.FlushIf(matchSIQInfo);

    auto matchDrain = [&bus](const ScbDrainBus& info) -> bool {
        return LessEqual(bus.req.bid, info.bid);
    };
    drainScbDataQ.FlushIf(matchDrain);
    drainScbRespQ.FlushIf(matchDrain);

    auto matchUOPInfo = [&bus](VectorLiteUopT& info) -> bool {
        return LessEqual(bus.req.bid, info->cmd->bid);
    };
    m_uiqEXEInQ.FlushIf(matchUOPInfo);

    FlushCmdBuffer(bus);
}

void VectorLite::SetFlush(const FlushBus &bus)
{
    LOG_INFO_M(Unit::VECLITE, Stage::NA) << " [VECTOR " << GetCoreId() << "] "
        << "VecTop, Recv Flush " << bus;
    DoCommonFlush(bus);
    exeUnit.SetFlush(bus);
    uiq.SetFlush(bus);
    siq.SetFlush(bus);
    rob.SetFlush(bus);
    stashUnit.SetFlush(bus);
    tileBuffer.SetFlush(bus);
}

ROBID VectorLite::GetOldestBid() const
{
    ROBID bid;
    return bid;
}

uint32_t VectorLite::GetCoreId() const
{
    return m_coreId;
}

void VectorLite::Dump()
{
    rob.Dump();
}

void VectorLite::PrintCfg() const
{
}

void VectorLite::ResetStats() const
{
}

uint32_t VectorLite::GetSelectCore() const
{
    return top->GetSelectCore();
}

uint32_t VectorLite::GetIdleSpace() const
{
    // TODO
    return 0;
}

} // namespace JCore
