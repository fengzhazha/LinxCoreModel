#include "TileBridge.h"
#include <cmath>
#include <bitset>
#include <algorithm>
#include <climits>
#include "core/Core.h"
#include "../configs/core_config.h"
#include "interface/ConstConfig.h"

namespace JCore {
namespace GFSIM {
namespace TileBridge {

constexpr uint32_t MAX_LANE_BIT = 6;
// Align: Read-256B, Write-128B
constexpr uint64_t SOC_READ_CACHELINE_ALIGN_MASK = 256;
static constexpr uint64_t TILE_ALIGN_MASK = ~(static_cast<uint64_t>(MAX_TILE_DATA_BYTE) - 1);

// Helper function to check if DataType is 64-bit
inline bool Is64BitType(DataType t)
{
    return (t == DataType::INT64 || t == DataType::UINT64 || t == DataType::FP64);
}

/* ========== Bridge Table ========== */

TileBridge::TileBridge(SimSys *sim, TileUtils *util, uint32_t coreId)
{
    m_config.overrideDefaultConfig(sim->getCfgs());
    id = coreId;
    machineType = MachineType::TMA;
    machineId = GetProcessID(MachineType::TMA, id, m_config.BPQ_SIZE);
    m_sim = sim;
    pTileUtils = util;

    m_bpqNRFBQ = std::make_shared<SimQueue<RFBEntry>>();
    m_bpqNWCBQ = std::make_shared<SimQueue<WCBEntry>>();
    m_bpqSRFBQ = std::make_shared<SimQueue<RFBEntry>>();
    m_bpqSWCBQ = std::make_shared<SimQueue<WCBEntry>>();
    m_nRFBBpqQ = std::make_shared<SimQueue<RFBEntry>>();
    m_nWCBBpqQ = std::make_shared<SimQueue<WCBEntry>>();
    m_sRFBBpqQ = std::make_shared<SimQueue<RFBEntry>>();
    m_sWCBBpqQ = std::make_shared<SimQueue<WCBEntry>>();
    m_bpqTagReadReqQ = std::make_shared<SimQueue<TagReq>>();
    m_cacheAaccelssQ = std::make_shared<SimQueue<CacheAaccelssOp>>();
    m_switchNetwork = std::make_shared<DelayQueue<SwitchInfo>>();
    m_cacheAaccelssQ->InitMaxSize(m_config.CAQ_SIZE);

    m_bdb = std::make_shared<BridgeDataBuffer>(m_config.BRIDGE_DATA_BUFFER_TLOAD_ONLY_USE_SIZE,
                                               m_config.BRIDGE_DATA_BUFFER_DATA_SIZE);

    m_tsBDB = std::make_shared<BridgeDataBuffer>(m_config.BRIDGE_DATA_BUFFER_TSTORE_ONLY_USE_SIZE,
                                                 m_config.BRIDGE_DATA_BUFFER_DATA_SIZE);

    ReadFillBufferConfig nrfbConfig = {
        .sim = sim,
        .readGranularity = m_config.NRFB_READ_BANDWIDTH,
        .bdbDataSize = m_config.BRIDGE_DATA_BUFFER_DATA_SIZE,
        .maxSendReadNum = static_cast<uint32_t>(m_config.NRFB_SEND_READ_NUM),
        .bdb = m_tsBDB,
        .memUnit = MemoryUnit::TR,
        .util = util,
        .reqNum = m_config.NRFB_OUTSTANDING_REQ_NUM,
        .id = id,
        .pmuData = m_pmuData,
    };
    m_nrfb = std::make_shared<ReadFillBuffer>(nrfbConfig);

    ReadFillBufferConfig srfbConfig = {
        .sim = sim,
        .readGranularity = m_config.SRFB_READ_BANDWIDTH,
        .bdbDataSize = m_config.BRIDGE_DATA_BUFFER_DATA_SIZE,
        .maxSendReadNum = static_cast<uint32_t>(m_config.SRFB_SEND_READ_NUM),
        .bdb = m_bdb,
        .tsBDB = m_tsBDB,
        .memUnit = MemoryUnit::GM,
        .util = util,
        .reqNum = m_config.SRFB_OUTSTANDING_REQ_NUM,
        .id = id,
        .pmuData = m_pmuData,
    };
    m_srfb = std::make_shared<ReadFillBuffer>(srfbConfig);

    WriteCoalesceBufferConfig nwcbConfig = {
        .sim = sim,
        .bdb = m_bdb,
        .memUnit = MemoryUnit::TR,
        .util = util,
        .reqNum = m_config.NWCB_OUTSTANDING_REQ_NUM,
        .bdbDataSize = m_config.BRIDGE_DATA_BUFFER_DATA_SIZE,
        .id = id,
        .pmuData = m_pmuData,
    };
    m_nwcb = std::make_shared<WriteCoalesceBuffer>(nwcbConfig);

    WriteCoalesceBufferConfig swcbConfig = {
        .sim = sim,
        .bdb = m_tsBDB,
        .memUnit = MemoryUnit::GM,
        .util = util,
        .reqNum = m_config.SWCB_OUTSTANDING_REQ_NUM,
        .bdbDataSize = m_config.BRIDGE_DATA_BUFFER_DATA_SIZE,
        .id = id,
        .pmuData = m_pmuData,
    };
    m_swcb = std::make_shared<WriteCoalesceBuffer>(swcbConfig);

    BridgePairQConfig bpqConfig = {
        .sim = sim,
        .util = util,
        .id = id,
        .config = m_config,
        .bdb = m_bdb,
        .tsBDB = m_tsBDB,
        .m_nrfb = m_nrfb,
        .m_srfb = m_srfb,
        .m_nwcb = m_nwcb,
        .m_swcb = m_swcb,
        .pmuData = m_pmuData,
    };
    m_bpq = std::make_shared<BridgePairQ>(bpqConfig);
    m_bpq->machineId = machineId;

    TagConfig tagConfig = {
        .sim = sim,
        .config = m_config,
        .bdb = m_bdb,
        .srfb = m_srfb,
        .bpq = m_bpq,
        .pmuData = m_pmuData,
    };
    m_tag = std::make_shared<Tag>(tagConfig);

    m_memoryPreBuffer = std::make_shared<MemoryPreBuffer>(sim, m_bpq, m_bdb, m_srfb, m_config);
}

void TileBridge::Build()
{
    m_bpq->m_lsuBridgeTloadQ = m_lsuBridgeTloadQ;
    m_bpq->m_lsuBridgeTstoreQ = m_lsuBridgeTstoreQ;
    m_bpq->tmaBCCWakeupQ = tmaBCCWakeupQ;
    m_bpq->tauBCCWakeupQ = tauBCCWakeupQ;
    m_bpq->bccTAUBlockCmdQ = bccTAUBlockCmdQ;
    m_bpq->m_bpqNRFBQ = m_bpqNRFBQ;
    m_bpq->m_bpqNWCBQ = m_bpqNWCBQ;
    m_bpq->m_bpqSRFBQ = m_bpqSRFBQ;
    m_bpq->m_bpqSWCBQ = m_bpqSWCBQ;
    m_bpq->m_nRFBBpqQ = m_nRFBBpqQ;
    m_bpq->m_nWCBBpqQ = m_nWCBBpqQ;
    m_bpq->m_sRFBBpqQ = m_sRFBBpqQ;
    m_bpq->m_sWCBBpqQ = m_sWCBBpqQ;
    m_bpq->m_tileRegBPQLdResQ = m_tileRegBridgeLdResQ;
    m_bpq->m_bpqTileRegLdReqQ = m_bridgeTileRegLdReqQ;

    m_bpq->m_vectorBridgeBPQReqQ = m_vectorBridgeReqQ;
    m_bpq->m_vectorBridgeBPQRspQ = m_vectorBridgeRspQ;
    m_bpq->m_bpqTagReadReqQ = m_bpqTagReadReqQ;
    m_bpq->m_cacheAaccelssQ = m_cacheAaccelssQ;
    m_bpq->m_switchNetwork = m_switchNetwork;
    m_bpq->m_tag = m_tag;
    m_bpq->stationReadRange = stationReadRange;
    for (uint32_t i = 0; i < stationReadRange; ++i) {
        m_bpq->AddNode(stations[i]);
    }

    m_tag->m_bpqTagReadReqQ = m_bpqTagReadReqQ;
    m_tag->m_tagSRFBQ = m_bpqSRFBQ;
    m_tag->m_cacheAaccelssQ = m_cacheAaccelssQ;

    m_nrfb->m_bpqRFBQ = m_bpqNRFBQ;
    m_nrfb->m_rfbBpqQ = m_nRFBBpqQ;
    m_nrfb->m_tileRegRFBLdResQ = m_tileRegBridgeLdResQ;
    m_nrfb->m_rfbTileRegLdReqQ = m_bridgeTileRegLdReqQ;
    m_nrfb->pTileUtils = pTileUtils;
    m_nrfb->stationReadRange = stationReadRange;
    for (uint32_t i = 0; i < stationReadRange; ++i) {
        m_nrfb->AddNode(stations[i]);
    }

    m_srfb->m_bpqRFBQ = m_bpqSRFBQ;
    m_srfb->m_rfbBpqQ = m_sRFBBpqQ;
    m_srfb->m_memRFBRspQ = m_memBridgeRspQ;
    m_srfb->m_rfbMemReqQ = m_bridgeMemReqQ;
    m_srfb->m_memoryPreBuffer = m_memoryPreBuffer;
    m_srfb->m_bpq = m_bpq;

    m_nwcb->m_bpqWCBAllocQ = m_bpqNWCBQ;
    m_nwcb->m_wcbBpqQ = m_nWCBBpqQ;
    m_nwcb->m_wcbTileRegStReqQ = m_bridgeTileRegStReqQ;
    m_nwcb->m_tileRegWCBWrResQ = m_tileRegBridgeWrResQ;
    m_nwcb->m_switchNetwork = m_switchNetwork;
    m_nwcb->pTileUtils = pTileUtils;
    m_nwcb->stationReadRange = 0;
    m_nwcb->m_bpq = m_bpq;
    for (uint32_t i = stationReadRange; i < stations.size(); ++i) {
        m_nwcb->AddNode(stations[i]);
    }

    m_swcb->m_bpqWCBAllocQ = m_bpqSWCBQ;
    m_swcb->m_wcbBpqQ = m_sWCBBpqQ;
    m_swcb->m_wcbMemReqQ = m_bridgeMemReqQ;
    m_swcb->m_wcbMemRspQ = m_memBridgeRspQ;
    m_swcb->m_bpq = m_bpq;

    m_memoryPreBuffer->m_switchNetwork = m_switchNetwork;

    m_bpq->fakeChannelNode.SetBridgeMemReqQ(m_bpq.get(), m_bridgeMemReqQ, m_memBridgeRspQ);
    m_bpq->fakeChannelNode.pTileUtils = pTileUtils;
    m_bpq->fakeChannelNode.bridgeTileRegStReqQ = m_bridgeTileRegStReqQ;
    GetSim()->core->SetTmaConfigs("bpq_size", std::to_string(m_config.BPQ_SIZE));
    GetSim()->core->SetTmaConfigs("bdb_size", std::to_string(m_config.BRIDGE_DATA_BUFFER_SIZE));
    GetSim()->core->SetTmaConfigs("srfb_size", std::to_string(m_config.SRFB_OUTSTANDING_REQ_NUM));
    GetSim()->core->SetTmaConfigs("swcb_size", std::to_string(m_config.SWCB_OUTSTANDING_REQ_NUM));
    GetSim()->core->SetTmaConfigs("nrfb_size", std::to_string(m_config.NRFB_OUTSTANDING_REQ_NUM));
    GetSim()->core->SetTmaConfigs("nwcb_size", std::to_string(m_config.NWCB_OUTSTANDING_REQ_NUM));
}

void TileBridge::Work()
{
    m_bpq->Work();
    m_nrfb->Work();
    m_srfb->Work();
    m_nwcb->Work();
    m_swcb->Work();
    m_tag->Work();
    AaccelssBDB();
    RecordPMUData();

    PrintStatus();
}

void TileBridge::Xfer()
{
    m_bpq->Xfer();
    m_nrfb->Xfer();
    m_srfb->Xfer();
    m_nwcb->Xfer();
    m_swcb->Xfer();

    m_bpqNRFBQ->Work();
    m_bpqNWCBQ->Work();
    m_bpqSRFBQ->Work();
    m_bpqSWCBQ->Work();
    m_nRFBBpqQ->Work();
    m_nWCBBpqQ->Work();
    m_sRFBBpqQ->Work();
    m_sWCBBpqQ->Work();
    m_bpqTagReadReqQ->Work();
    m_cacheAaccelssQ->Work();
    m_switchNetwork->Xfer();
    m_memoryPreBuffer->Xfer();
    CheckBusy();
}

void TileBridge::Reset()
{
}

SimSys* TileBridge::GetSim()
{
    return m_sim;
}

void TileBridge::Flush(FlushBus& bus)
{
    m_bpq->Flush(bus);
    m_nrfb->Flush(bus);
    m_srfb->Flush(bus);
    m_nwcb->Flush(bus);
    m_swcb->Flush(bus);

    auto matchMemReq = [&bus](MemReqBus &req) {
        return req.stid == bus.req.stid && LessEqual(bus.req.bid, req.bid);
    };

    m_lsuBridgeTloadQ->FlushIf(matchMemReq);
    m_lsuBridgeTstoreQ->FlushIf(matchMemReq);
}

void TileBridge::ReportStat()
{
}

void TileBridge::PrintStatus()
{
    if (!IsBusy()) {
        return;
    }

    LOG_DEBUG_M(Unit::TMA, Stage::NA) << "[Bridge Table" << std::dec << id << "]:"
        << "TL BDB Use Size:" << m_bdb->GetAliveEntryCounter()
        << ", TS BDB Use Size:" << m_tsBDB->GetAliveEntryCounter();
    LOG_DEBUG_M(Unit::TMA, Stage::CAQ) << "CAQ Size:" << m_cacheAaccelssQ->Size() + m_cacheAaccelssQ->SizeW();
    m_bpq->PrintStatus();
    m_nrfb->PrintStatus();
    m_srfb->PrintStatus();
    m_nwcb->PrintStatus();
    m_swcb->PrintStatus();
}

void TileBridge::RecordPMUData()
{
    if (m_config.TOP_DOWN_PMU_VERBOSE) {
        RecordTloadTopDownPMUData();
    }
    if (!IsBusy() || !m_config.OCCUPANCY_VERBOSE) {
        return;
    }

    // TMA Key Stats --- bpq, rfb, wcb occupancy
    if (m_bpq->GetBPQSize() > 0) {
        m_pmuData.bpqNonEmptryCycles++;
    }
    const uint32_t threshold75pBPQ = m_config.BPQ_SIZE * 3 / 4;
    if (m_bpq->GetBPQSize() >= threshold75pBPQ) {
        m_pmuData.bpq75pCycles++;
    }
    if (m_bpq->GetBPQSize() == m_config.BPQ_SIZE) {
        m_pmuData.bpqFullCycles++;
    }

    if (m_bdb->GetAliveEntryCounter() > 0) {
        m_pmuData.tlbdbNonEmptyCycles++;
    }
    const uint32_t threshold75pTLBdb = m_config.BRIDGE_DATA_BUFFER_TLOAD_ONLY_USE_SIZE * 3 / 4;
    if (m_bdb->GetAliveEntryCounter() >= threshold75pTLBdb) {
        m_pmuData.tlbdb75pCycles++;
    }
    if (m_bdb->GetAliveEntryCounter() == m_config.BRIDGE_DATA_BUFFER_TLOAD_ONLY_USE_SIZE) {
        m_pmuData.tlbdbFullCycles++;
    }
    if (m_tsBDB->GetAliveEntryCounter() > 0) {
        m_pmuData.tsbdbNonEmptyCycles++;
    }
    const uint32_t threshold75pTSBdb = m_config.BRIDGE_DATA_BUFFER_TSTORE_ONLY_USE_SIZE * 3 / 4;
    if (m_tsBDB->GetAliveEntryCounter() >= threshold75pTSBdb) {
        m_pmuData.tsbdb75pCycles++;
    }
    if (m_tsBDB->GetAliveEntryCounter() == m_config.BRIDGE_DATA_BUFFER_TSTORE_ONLY_USE_SIZE) {
        m_pmuData.tsbdbFullCycles++;
    }

    if (m_srfb->GetAliveEntryCounter() > 0) {
        m_pmuData.srfbNonEmptyCycles++;
    }
    const uint32_t threshold75pSrfb = m_config.SRFB_OUTSTANDING_REQ_NUM * 3 / 4;
    if (m_srfb->GetAliveEntryCounter() >= threshold75pSrfb) {
        m_pmuData.srfb75pCycles++;
    }
    if (m_srfb->GetAliveEntryCounter() == m_config.SRFB_OUTSTANDING_REQ_NUM) {
        m_pmuData.srfbFullCycles++;
    }

    if (m_nwcb->GetAliveEntryCounter() > 0) {
        m_pmuData.nwcbNonEmptyCycles++;
    }
    const uint32_t threshold75pNwcb = m_config.NWCB_OUTSTANDING_REQ_NUM * 3 / 4;
    if (m_nwcb->GetAliveEntryCounter() >= threshold75pNwcb) {
        m_pmuData.nwcb75pCycles++;
    }
    if (m_nwcb->GetAliveEntryCounter() == m_config.NWCB_OUTSTANDING_REQ_NUM) {
        m_pmuData.nwcbFullCycles++;
    }

    if (m_swcb->GetAliveEntryCounter() > 0) {
        m_pmuData.swcbNonEmptyCycles++;
    }
    const uint32_t threshold75pSwcb = m_config.SWCB_OUTSTANDING_REQ_NUM * 3 / 4;
    if (m_swcb->GetAliveEntryCounter() >= threshold75pSwcb) {
        m_pmuData.swcb75pCycles++;
    }
    if (m_swcb->GetAliveEntryCounter() == m_config.SWCB_OUTSTANDING_REQ_NUM) {
        m_pmuData.swcbFullCycles++;
    }
}

void TileBridge::RecordTloadTopDownPMUData()
{
    auto doStat = [this](uint32_t stid) {
        BPQEntryPtr entry = GetOldestBpqPtr(stid);
        if (!entry) {
            return;
        }
        // TODO: FIX PMU for smt
        switch (entry->pairStatus) {
            case ExecStatus::NO_START:
                m_pmuData.startingCycles++;
                break;
            case ExecStatus::SEND_WRITE:
                m_pmuData.integCycles++;
                break;
            default:
                RecordTloadTopDownFetching(entry);
        }
    };
    if (!IsTloadBusy()) {
        m_pmuData.idleCycles++;
    } else {
        if (m_nwcb->IsWriteReady()) {
            if (m_nwcb->IsWriteRingBlocked()) {
                m_pmuData.egressBlockCycles++;
            } else {
                m_pmuData.efficientCycles++;
            }
        } else { // unavailableCycles
            for (uint32_t stid = 0; stid < GetSim()->core->configs.scalar_smt_thread; ++stid) {
                doStat(stid);
            }
        }
    }
    return;
}

void TileBridge::RecordTloadTopDownFetching(BPQEntryPtr bpqEntry)
{
    uint32_t threshold94pSrfb = bpqEntry->readReqs.size() * 15 / 16;
    const uint32_t threshold100pBdb = m_config.BRIDGE_DATA_BUFFER_SIZE
                                    - m_config.BRIDGE_DATA_BUFFER_TSTORE_ONLY_USE_SIZE;
    const uint32_t threshold75pBdb = threshold100pBdb * 3 / 4;
    if (bpqEntry->resolveReadCounter <= threshold94pSrfb) {
        if (m_bdb->GetAliveEntryCounter() <= threshold75pBdb) {
            m_pmuData.latBoundCycles++;
        } else {
            m_pmuData.ostBoundCycles++;
        }
    } else {
        if (m_bdb->GetAliveEntryCounter() <= threshold75pBdb) {
            m_pmuData.varBoundCycles++;
        } else {
            m_pmuData.varOstBoundCycles++;
        }
    }
    return;
}

BPQEntryPtr TileBridge::GetOldestBpqPtr(uint32_t stid)
{
    return m_bpq->GetOldestBpqPtr(stid);
}

PMUData& TileBridge::GetPMUData()
{
    for (auto kv : m_bpq->m_mgatherReadTagDataUsage) {
        m_pmuData.mgatherReadTagValidDataSize += kv.second.count();
    }
    return m_pmuData;
}

static void FillDataBySwitchMask(std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> switchMask,
                                 std::vector<uint8_t>& srcData, std::vector<uint8_t>& dstData)
{
    for (auto it : switchMask) {
        uint32_t dstOffset = it.first;
        uint32_t srcOffset = it.second.first;
        uint32_t dataSize = it.second.second;
        auto srcDataBegin = srcData.begin() + srcOffset;
        auto dstDataBegin = dstData.begin() + dstOffset;

        auto freeMemSize = dstData.capacity() - dstOffset;
        ASSERT(dataSize <= freeMemSize && "Insufficient free memory!");
        std::copy(srcDataBegin, srcDataBegin + dataSize, dstDataBegin);
    }
}

static bool IsVscatterRingReq(std::shared_ptr<Request> pkt)
{
    return (pkt->GetChannel() == ChannelType::DATA && pkt->GetFlit().cmd == CHICommand::CompDBIDResp);
}

void TileBridge::AaccelssBDB()
{
    auto handleCAQ = [this]() {
        if (m_cacheAaccelssQ->Empty()) {
            return;
        }

        CacheAaccelssOp&& cao = m_cacheAaccelssQ->Front();
        BPQEntryPtr bpqEntry = m_bpq->GetBpqPtr(cao.bpqId);
        std::vector<uint8_t> data;
        for (auto bdbId : cao.bdbIds) {
            DataBlock dataBlock = m_bdb->ReadDataBlock(bdbId);
            data.insert(data.end(), dataBlock.data.begin(), dataBlock.data.end());
        }
        if (cao.type == CacheAaccelssType::READ_TO_NWCB) {
            /* Read data from bdb to NWCB */
            std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> switchMask =
                m_bpq->GetSwitchMask(cao.bpqId, cao.readAddr);

            SwitchInfo switchInfo;
            switchInfo.bpqId = cao.bpqId;
            switchInfo.data = data;
            switchInfo.switchMask = switchMask;
            m_switchNetwork->write(switchInfo);
            bpqEntry->readReqTagPipeViews[cao.readReqId].bdbReadCycle = GetSim()->getCycles();
            bpqEntry->readReqTagPipeViews[cao.readReqId].retireCycle = GetSim()->getCycles() + 1;
            LOG_DEBUG_M(MachineType::TMA, Stage::CAQ) << "CAQ read data from bdb to NWCB, bpq id:" << std::dec
                << cao.bpqId << ", addr:0x" << std::hex << cao.readAddr;
        } else if (cao.type == CacheAaccelssType::READ_TO_SWCB) {
            if (m_bpqSRFBQ->SizeW() != 0) {
                return;
            }
            /* Read victim from bdb to SWCB */
            WriteReq writeReq;
            writeReq.writeAddr = cao.writeAddr;
            writeReq.writeSize = m_config.GLOBAL_MEMORY_CACHELINE_SIZE;
            WCBEntry wcbEntry;
            wcbEntry.writeReq = writeReq;
            wcbEntry.bpqId = cao.bpqId;
            wcbEntry.data = data;
            wcbEntry.stid = bpqEntry->tileArg.stid;
            wcbEntry.status = WCBEntryStatus::DATA_READY;
            wcbEntry.writeMask = std::vector<bool>(m_config.GLOBAL_MEMORY_CACHELINE_SIZE, true);
            wcbEntry.op = bpqEntry->tileArg.op;
            m_bpqSWCBQ->Write(wcbEntry);

            /* SRFB */
            RFBEntry rfbEntry;
            rfbEntry.addr = cao.readAddr;
            rfbEntry.size = m_config.GLOBAL_MEMORY_CACHELINE_SIZE;
            rfbEntry.bpqId = cao.bpqId;
            rfbEntry.bid = bpqEntry->bid;
            rfbEntry.memDir = bpqEntry->direction;
            rfbEntry.op = bpqEntry->tileArg.op;
            rfbEntry.stid = bpqEntry->tileArg.stid;
            rfbEntry.bdbId = cao.bdbIds;
            m_bpqSRFBQ->Write(rfbEntry);
            bpqEntry->readReqTagPipeViews[cao.readReqId].bdbReadCycle = GetSim()->getCycles();
            bpqEntry->readReqTagPipeViews[cao.readReqId].rfbCycle = GetSim()->getCycles() + 1;
            bpqEntry->readReqTagPipeViews[cao.readReqId].retireCycle = GetSim()->getCycles() + 2;
            LOG_DEBUG_M(MachineType::TMA, Stage::CAQ) << "CAQ read victim from bdb to SWCB, bpq id:" << std::dec
                << cao.bpqId << ", addr:0x" << std::hex << cao.readAddr;
        } else if (cao.type == CacheAaccelssType::WRITE_TO_BDB) {
            /* Write data from ts bdb to tl bdb */
            std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> switchMask =
                m_bpq->GetSwitchMask(cao.bpqId, cao.readAddr);

            std::vector<uint8_t> tsBDBData;
            for (auto bdbId : bpqEntry->tsBDBIds) {
                DataBlock dataBlock = m_tsBDB->ReadDataBlock(bdbId);
                tsBDBData.insert(tsBDBData.end(), dataBlock.data.begin(), dataBlock.data.end());
            }

            /* Fill the data in the TL BDB based on the switchMask. */
            FillDataBySwitchMask(switchMask, tsBDBData, data);

            /* Write back data to TL BDB. */
            uint8_t *dataPtr = data.data();
            for (auto bdbId : cao.bdbIds) {
                /*
                 * Currently, the offset between the BDB and baseAddr is checked during BDB write.
                 * As a result, before the current request is processed by the CAQ, another request
                 * may have swapped out the BDB of the current request in the tag.
                 * Therefore, m_bdb->WriteData cannot be directly used here.
                 */
                DataBlock& dataBlock = m_bdb->ReadDataBlock(bdbId);
                std::copy(dataPtr, dataPtr + m_config.BRIDGE_DATA_BUFFER_DATA_SIZE, dataBlock.data.begin());
                dataPtr += m_config.BRIDGE_DATA_BUFFER_DATA_SIZE;
            }

            bpqEntry->readReqTagPipeViews[cao.readReqId].bdbWriteCycle = GetSim()->getCycles();
            bpqEntry->readReqTagPipeViews[cao.readReqId].retireCycle = GetSim()->getCycles() + 1;

            ++bpqEntry->resolveReadCounter;
            if (bpqEntry->resolveReadCounter == bpqEntry->readReqs.size()) {
                bpqEntry->pairStatus = ExecStatus::COMPLETE;
                bpqEntry->completeCycle = GetSim()->getCycles();
            }
        }
        m_cacheAaccelssQ->Pop();
        m_bpq->ReleaseCAQAvailCnt();
    };

    if (!m_memoryPreBuffer->Empty()) {
        m_memoryPreBuffer->FillWCBAndBDB();
    } else {
        handleCAQ();
    }
}

std::ostream& operator<<(std::ostream& os, ExecStatus status)
{
    switch (status) {
        case ExecStatus::NO_START:
            os << "NO_START";
            break;
        case ExecStatus::WAIT_OTHER_BPQ:
            os << "WAIT_OTHER_BPQ";
            break;
    // VSCATTER states
        case ExecStatus::SEND_DBID_RSP:
            os << "SEND_DBID_RSP";
            break;
        case ExecStatus::WAIT_D_A_VEC:
            os << "WAIT_D_A_VEC";
            break;
        case ExecStatus::GET_ADDR_TILE:
            os << "GET_ADDR_TILE";
            break;
        case ExecStatus::WAIT_ADDR_TILE:
            os << "WAIT_ADDR_TILE";
            break;
        case ExecStatus::SEND_READ:
            os << "SEND_READ";
            break;
        case ExecStatus::WAIT_READ_DATA:
            os << "WAIT_READ_DATA";
            break;
        case ExecStatus::SEND_WRITE:
            os << "SEND_WRITE";
            break;
        case ExecStatus::WAIT_WRITE_RESOLVE:
            os << "WAIT_WRITE_RESOLVE";
            break;
        case ExecStatus::COMPLETE:
            os << "COMPLETE";
            break;
        default:
            os << "UNKNOWN";
            break;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, WCBEntryStatus status)
{
    switch (status) {
        case WCBEntryStatus::NO_START:
            os << "NO_START";
            break;
        case WCBEntryStatus::WAIT_READ:
            os << "WAIT_READ";
            break;
        case WCBEntryStatus::INTERGRATING:
            os << "INTERGRATING";
            break;
        case WCBEntryStatus::DATA_READY:
            os << "DATA_READY";
            break;
        case WCBEntryStatus::TRANSMITED:
            os << "TRANSMITED";
            break;
        case WCBEntryStatus::FLUSHED:
            os << "FLUSHED";
            break;
        case WCBEntryStatus::COMPLETE:
            os << "COMPLETE";
            break;
        default:
            os << "UNKNOWN";
            break;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, RFBEntryStatus status)
{
    switch (status) {
        case RFBEntryStatus::WAIT:
            os << "WAIT";
            break;
        case RFBEntryStatus::TRANSMITED:
            os << "TRANSMITED";
            break;
        case RFBEntryStatus::FLUSHED:
            os << "FLUSHED";
            break;
        case RFBEntryStatus::RETURNED:
            os << "RETURNED";
            break;
        default:
            os << "UNKNOWN";
            break;
    }
    return os;
}

bool TileBridge::IsBusy()
{
    return m_bpq->IsBusy();
}

void TileBridge::CheckBusy()
{
    bool currentBusy = IsBusy();
    if (currentBusy && !lastCycleTMABusy) {
        // free to busy
        tmaBusyStartCycle = GetSim()->getCycles();
    }
    if (!currentBusy && lastCycleTMABusy) {
        // busy to free
        SwimLogData logData;
        logData.name = "TMA busy";
        logData.pid = machineId;
        logData.tid = CORE_INTER_VIEW_TID;
        logData.sTime = tmaBusyStartCycle;
        logData.eTime = GetSim()->getCycles();
        logData.eventId = GetSim()->GetEventId();
        logData.hint = "TMA busy";
        GetSim()->AddDuration(logData);
    }
    lastCycleTMABusy = currentBusy;
}

bool TileBridge::IsTloadBusy()
{
    return m_bpq->IsTloadBusy();
}

bool TileBridge::IsWriteRingBlocked() const
{
    return m_nwcb->IsWriteRingBlocked();
}

bool TileBridge::IsTstoreBusy()
{
    return m_bpq->IsTstoreBusy();
}

bool TileBridge::IsTmovBusy()
{
    return m_bpq->IsTmovBusy();
}

bool TileBridge::IsMgatherBusy()
{
    return m_bpq->IsMgatherBusy();
}

bool TileBridge::IsMscatterBusy()
{
    return m_bpq->IsMscatterBusy();
}

bool TileBridge::IsVgatherBusy()
{
    return m_bpq->IsVgatherBusy();
}

bool TileBridge::IsVscatterBusy()
{
    return m_bpq->IsVscatterBusy();
}

uint64_t TileBridge::GetBPQSize()
{
    return m_bpq->GetBPQSize();
}

static bool DataTypeIsFP4(DataType t)
{
    return (t == DataType::FP4 || t == DataType::FP4_1 || t == DataType::HIF4);
}

static bool DoubleEqual(double x, double y)
{
    return std::fabs(x - y) < std::numeric_limits<double>::epsilon();
}

static bool IsInteger(double x)
{
    return DoubleEqual(x, std::round(x));
}

static bool IsDivisible(double x, double y)
{
    return std::fmod(x, y) < std::numeric_limits<double>::epsilon();
}

static constexpr double HALF_BYTE_DATA_WIDTH = 0.5F;
static constexpr double ONE_BYTE_DATA_WIDTH = 1.0F;
static constexpr double TWO_BYTE_DATA_WIDTH = 2.0F;
static constexpr double FOUR_BYTE_DATA_WIDTH = 4.0F;
static constexpr double EIGHT_BYTE_DATA_WIDTH = 8.0F;

static inline double GetDataSize(DataType t)
{
    switch (t) {
        case DataType::INT4:
        case DataType::UINT4:
        case DataType::FP4:
        case DataType::FP4_1:
        case DataType::UINT8:
        case DataType::INT8:
        case DataType::SF8:
        case DataType::FP8:
        case DataType::FP8_1:
        case DataType::HIF4:
        case DataType::HIF8:
            return ONE_BYTE_DATA_WIDTH;
        case DataType::INT16:
        case DataType::UINT16:
        case DataType::FP16:
        case DataType::BF16:
            return TWO_BYTE_DATA_WIDTH;
        case DataType::INT32:
        case DataType::UINT32:
        case DataType::TF32:
        case DataType::HF32:
        case DataType::FP32:
            return FOUR_BYTE_DATA_WIDTH;
        case DataType::UINT64:
        case DataType::INT64:
        case DataType::FP64:
            return EIGHT_BYTE_DATA_WIDTH;
        default:
            std::cerr << "TileBridge unknown datatype: " << static_cast<uint64_t>(t) << std::endl;
            throw std::invalid_argument("Unknown DataType");
    }
}

static bool IsMemoryModeReq(TileOp op)
{
    return (op == TileOp::MGATHER || op == TileOp::MSCATTER || op == TileOp::VGATHER || op == TileOp::VSCATTER);
}

static bool IsScatterReq(TileOp op)
{
    return (op == TileOp::MSCATTER || op == TileOp::VSCATTER);
}

static bool IsGatherReq(TileOp op)
{
    return (op == TileOp::MGATHER || op == TileOp::VGATHER);
}

static bool IsVectorReq(TileOp op)
{
    return (op == TileOp::VGATHER || op == TileOp::VSCATTER);
}

/* Source queue identifiers for ReceiveMemReq bid-based arbitration */
enum class MemReqSrc {
    TLOAD = 0,
    TSTORE,
    TMOV,
    MGATHER,
    MSCATTER,
    VECMSG,
};

struct MemReqCandidate {
    MemReqSrc src;
    ROBID bid;
};

/* ========== Bridge Pair Queue ========= */
void TileBridgeArg::InitByMemReqBus(BlockCommandPtr cmd)
{
    op = cmd->tileOp;
    srcType = cmd->dataType;
    stid = cmd->stid;
    if (cmd->blockAttr != nullptr) {
        dstType = cmd->tileOp == TileOp::TMOV ? srcType : cmd->blockAttr->dataType; // TMOV不做数据格式转换，srcType和dstType相同
        layout = cmd->blockAttr->layout;
    }
    if (cmd->tileOp == TileOp::TLOAD || cmd->tileOp == TileOp::TSTORE) {
        ASSERT(cmd->srcData.size() >= 1);
        if (cmd->srcData.size() == 1) {
            double tmpStride = cmd->lb0 * GetDataSize(srcType);
            ASSERT(IsInteger(tmpStride));
            strideGM = static_cast<uint32_t>(tmpStride);
        } else {
            strideGM = cmd->srcData[1];
        }
        baseAddrGM = cmd->srcData[0];
    }
    switch (op) {
        case TileOp::TLOAD:
            if (!cmd->dsts.empty()) {
                baseAddrTR0 = cmd->dsts[0]->baseAddr;
            }
            break;
        case TileOp::TSTORE:
            if (!cmd->srcs.empty()) {
                baseAddrTR0 = cmd->srcs[0]->baseAddr;
            }
            break;
        case TileOp::TMOV:
            if (!cmd->srcs.empty()) {
                baseAddrTR0 = cmd->srcs[0]->baseAddr;
                baseAddrDstTR = cmd->dsts[0]->baseAddr;
            }
            break;
        case TileOp::MGATHER:
            srcType = cmd->srcs.at(0)->tileInfo->dataType;
            dstType = cmd->dataType;
            baseAddrGM = cmd->srcData[0];
            baseAddrTR0 = cmd->srcs.at(0)->baseAddr;
            baseAddrDstTR = cmd->dsts.at(0)->baseAddr;
            break;
        case TileOp::MSCATTER:
            srcType = cmd->srcs.at(1)->tileInfo->dataType;  // data type of offset tile
            dstType = cmd->dataType;                        // data type of data tile
            baseAddrGM = cmd->srcData.at(0);
            baseAddrTR0 = cmd->srcs.at(0)->baseAddr;    // data   tile
            baseAddrTR1 = cmd->srcs.at(1)->baseAddr;    // offset tile
            break;
        default:
            ASSERT(false && "Unsupport TILEOP") << cmd->Dump();
    }
    if (op == TileOp::TMOV) {
        switch (cmd->blockAttr->layout) {
            case LayOut::NORM:
            case LayOut::ND2NZ:
            case LayOut::ND2ZN:
                d1TR = cmd->lb0;
                d2TR = cmd->lb1;
                break;
            case LayOut::DN2NZ:
            case LayOut::DN2ZN:
                d1TR = cmd->lb1;
                d2TR = cmd->lb0;
                break;
            default:
                ASSERT(false && "Unsupport layout") << cmd->Dump();
                break;
        }
        originalD2TR = d2TR;
        return;
    }
    if (op == TileOp::MGATHER || op == TileOp::MSCATTER) {
        d1TR = cmd->lb0;
        d2TR = cmd->lb1;
        return;
    }
    TileOperandPtr opdPtr = (op == TileOp::TLOAD) ? cmd->dsts[0] : cmd->srcs[0];
    switch (cmd->blockAttr->layout) {
        case LayOut::NORM:
        case LayOut::ND2NZ:
        case LayOut::ND2ZN:
            d1GM = cmd->lb0;
            d2GM = cmd->lb1;
            d1TR = cmd->lb2;
            ASSERT(IsDivisible(opdPtr->size, (d1TR * GetDataSize(srcType))));
            d2TR = opdPtr->size / (d1TR * GetDataSize(srcType));
            break;
        case LayOut::DN2NZ:
        case LayOut::DN2ZN:
            d2GM = cmd->lb0;
            d1GM = cmd->lb1;
            d2TR = cmd->lb2;
            ASSERT(IsDivisible(opdPtr->size, (d2TR * GetDataSize(srcType))));
            d1TR = opdPtr->size / (d2TR * GetDataSize(srcType));
            break;
        default:
            ASSERT(false && "Unsupport layout") << cmd->Dump();
            break;
    }
    originalD1GM = d1GM;
    originalD2TR = d2TR;
}

BridgePairQ::BridgePairQ(BridgePairQConfig& config) : m_pmuData(config.pmuData)
{
    m_sim = config.sim;
    pTileUtils = config.util;
    id = config.id;
    m_config = config.config;
    m_bdb = config.bdb;
    m_tsBDB = config.tsBDB;
    m_nrfb = config.m_nrfb;
    m_srfb = config.m_srfb;
    m_nwcb = config.m_nwcb;
    m_swcb = config.m_swcb;
    m_caqAvailCnt = config.config.CAQ_SIZE;
    m_srfbAvailCnt = config.config.SRFB_OUTSTANDING_REQ_NUM;
    m_swcbAvailCnt = config.config.SWCB_OUTSTANDING_REQ_NUM;
    m_mpbAvailCnt = config.config.MPB_SIZE;
    m_tloadBidOrderList.resize(config.sim->core->configs.scalar_smt_thread);

    for (uint64_t i = 0; i < m_config.BPQ_SIZE; i++) {
        m_tidPool.push_back(i);
        m_bpqIDPool.emplace(i);
    }
}

void BridgePairQ::Work()
{
    ResolveBlock();
    ReceiveWCBWriteRslv();
    ReceiveRFBReadRslv();
    /* vgather 和 vscatter消息处理 */
    HandleVscatterTileTrans();
    HandleGatherReadSrc();
    SendReadWriteReq();
    ReceiveMemReq();
    /* Placed under ReceiveMemReq to facilitate the first BPQ being pushed into the queue. */
    InsertSplitedBPQ();

    if (m_config.fake_vgather_enable) {
        SendFackChannelReq();
        fakeChannelNode.Work();
    }

    if (!m_bpq.empty()) {
        std::sort(m_tidPool.begin(), m_tidPool.end());
    }
}

void BridgePairQ::Xfer()
{
    StubProcessVecBridgeMsg();
}

void BridgePairQ::StubProcessVecBridgeMsg()
{
    static int cycle = 0;
    static int countRsp = 0;
    static int countReq = 0;
    // 容量测试8个, 不超过
    const static int MAX_MSG_NUM = 0;
    /* vgather 和 vscatter桩消息处理 */
    if ((countRsp < MAX_MSG_NUM) && ((cycle % 39) == 0) && (m_config.debug_vec_bridge_vgather_enable)) {
        if (!m_vectorBridgeBPQRspQ->Empty()) {
            m_vectorBridgeBPQRspQ->Pop();
            countRsp++;
        }
    }

    if (((countReq < MAX_MSG_NUM) && ((cycle + 30) % m_config.debug_stub_vgather_freq) == 0)
        && (m_config.debug_vec_bridge_vgather_enable)) {
        StubVectorSendVgather();
        countReq++;
    }
    cycle++;
}


void BridgePairQ::Reset()
{
}

SimSys* BridgePairQ::GetSim()
{
    return m_sim;
}

void BridgePairQ::ReportStat()
{
}

void BridgePairQ::Flush(FlushBus &bus)
{
    auto bpqEntryNeedFlush = [&bus](BPQEntryPtr& entry) {
        return LessEqual(bus.req.bid, entry->bid) && entry->tileArg.stid == bus.req.stid;
    };
    uint32_t stid = bus.req.stid;

    for (auto it = m_bpq.begin(); it != m_bpq.end();) {
        BPQEntryPtr entry = *it;
        uint64_t bpqId = entry->bpqID;

        auto rfbEntryMatchFlush = [&bpqId](RFBEntry& entry)->bool {
            return entry.bpqId == bpqId;
        };

        auto wcbEntryMatchFlush = [&bpqId](WCBEntry& entry)->bool {
            return entry.bpqId == bpqId;
        };

        auto tagReqMatchFlush = [&bpqId](TagReq& tagReq)->bool {
            return tagReq.entry->bpqID == bpqId;
        };

        if (bpqEntryNeedFlush(entry)) {
            ASSERT(!IsVectorReq(entry->tileArg.op) && entry->tileArg.op != TileOp::MSCATTER);
            m_bdb->ReleaseBuffer(bpqId);
            m_tsBDB->ReleaseBuffer(bpqId);
            m_bpqNWCBQ->FlushIf(wcbEntryMatchFlush);
            m_bpqSWCBQ->FlushIf(wcbEntryMatchFlush);
            m_nWCBBpqQ->FlushIf(wcbEntryMatchFlush);
            m_sWCBBpqQ->FlushIf(wcbEntryMatchFlush);
            m_bpqNRFBQ->FlushIf(rfbEntryMatchFlush);
            m_bpqSRFBQ->FlushIf(rfbEntryMatchFlush);
            m_nRFBBpqQ->FlushIf(rfbEntryMatchFlush);
            m_sRFBBpqQ->FlushIf(rfbEntryMatchFlush);
            m_bpqTagReadReqQ->FlushIf(tagReqMatchFlush);

            if (entry->genReadCycle != 0 || entry->genWriteCycle != 0) {
                DecOutstandingTileOpNum(entry->tileArg.op);
            }

            if (entry->tileArg.op == TileOp::TLOAD) {
                --m_tloadCnt;
            } else if (entry->tileArg.op == TileOp::TSTORE) {
                --m_tstoreCnt;
            } else if (entry->tileArg.op == TileOp::TMOV) {
                --m_tmovCnt;
            } else if (entry->tileArg.op == TileOp::MGATHER) {
                --m_mgatherCnt;
            } else {
                LOG_ERROR_M(Unit::TMA, Stage::NA) << "[Bridge Table]: Unsupport tileop for tile bridge";
                abort();
            }

            m_bpqIDPool.emplace(entry->bpqID);
            it = m_bpq.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_splitedBPQList.begin(); it != m_splitedBPQList.end();) {
        if (bpqEntryNeedFlush(*it)) {
            it = m_splitedBPQList.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_tloadBidOrderList[stid].cbegin(); it != m_tloadBidOrderList[stid].cend();) {
        if (LessEqual(bus.req.bid, *it)) {
            it = m_tloadBidOrderList[stid].erase(it);
        } else {
            ++it;
        }
    }
}

void BridgePairQ::PrintStatus()
{
    LOG_DEBUG_M(Unit::TMA, Stage::BPQ) << "BPQ size:" << std::dec << m_bpq.size();
    for (auto entry: m_bpq) {
        std::stringstream tmp;
        tmp << "    |--" << "BPQ" << std::dec << entry->bpqID << "-B" << entry->bid << " "
            << GetTileOpName(entry->tileArg.op) << " " << entry->pairStatus;
        if (IsVectorReq(entry->tileArg.op)) {
            tmp << " tid:" << entry->tileArg.tid << " group slot id:" << entry->tileArg.groupSlotId;
        }
        if (entry->rowLeadBPQPtr) {
            tmp << " row lead:" << entry->rowLeadBPQPtr->bpqID;
        }
        if (entry->instLeadBPQPtr) {
            tmp << " inst lead:" << entry->instLeadBPQPtr->bpqID;
        }

        LOG_DEBUG_M(Unit::TMA, Stage::BPQ) << tmp.str();
    }
}

static void SplitGMReadAddrs(std::vector<ReadReq>& readReqs, TileBridgeArg& tileArg, double dataSize,
                             uint64_t maxCacheLineSize, uint64_t bdbDataSize)
{
    uint64_t minCacheLineSize = bdbDataSize;
    ASSERT((maxCacheLineSize % minCacheLineSize) == 0 &&
    "SplitGMReadAddrs: maxCacheLineSize must be an integer multiple of bdbDataSize");
    auto calReadSize = [maxCacheLineSize, minCacheLineSize](uint64_t readAddr, uint64_t endAddr) {
        return (endAddr - readAddr) > minCacheLineSize ? maxCacheLineSize : minCacheLineSize;
    }; // TODO: for CL256/BDB64, can support 128 soc req in the future

    std::unordered_map<uint64_t, uint64_t> uniqueCacheLines;
    double tmpSize = std::round(tileArg.originalD1GM * dataSize);
    uint64_t effectiveSize = static_cast<uint64_t>(tmpSize);
    for (uint32_t i = 0; i < tileArg.d2GM; ++i) {
        uint64_t startAddr = tileArg.baseAddrGM + i * tileArg.strideGM;
        uint64_t endAddr = startAddr + effectiveSize;
        uint64_t readAddr = startAddr & (~(minCacheLineSize - 1));
        bool alignedToMaxCacheline = true;
        uint64_t stepCacheLineSize = 0;
        for (; readAddr < endAddr; readAddr += stepCacheLineSize) {
            if (uniqueCacheLines.count(readAddr) == 0) {
                ReadReq req;
                req.readAddr = readAddr;
                alignedToMaxCacheline = readAddr % maxCacheLineSize == 0;
                stepCacheLineSize = !alignedToMaxCacheline ? minCacheLineSize : calReadSize(readAddr, endAddr);
                req.readSize = stepCacheLineSize;
                readReqs.push_back(req);
                uniqueCacheLines.insert(std::make_pair(readAddr, stepCacheLineSize));
            } else {
                stepCacheLineSize = uniqueCacheLines[readAddr];
            }
        }
    }
}

/* TODO: Subsequent optimization involves writing at the granularity of cache lines. */
static void SplitNormTRWriteAddrs(std::vector<WriteReq>& writeReqs, TileBridgeArg& tileArg, double dataSize,
                                  uint64_t cacheLineSize)
{
    uint64_t validRowSize = static_cast<uint64_t>(std::round(tileArg.d1GM * dataSize));

    for (uint32_t d2TRIdx = 0; d2TRIdx < tileArg.d2TR; ++d2TRIdx) {
        uint64_t rowStartAddr = tileArg.baseAddrTR0 + static_cast<uint64_t>(d2TRIdx) * tileArg.d1TR * dataSize;
        uint64_t rowEndAddr = rowStartAddr + validRowSize;
        uint64_t writeAddr = rowStartAddr;

        while (writeAddr < rowEndAddr) {
            uint64_t alignWriteAddr = writeAddr & ~(cacheLineSize - 1);
            uint32_t writeSize = cacheLineSize - (writeAddr & (cacheLineSize - 1));
            if (writeAddr + writeSize > rowEndAddr) {
                writeSize = rowEndAddr - writeAddr;
            }

            uint32_t d1TRIdx = (writeAddr - rowStartAddr) / dataSize;

            WriteReq req;
            req.writeAddr = alignWriteAddr;
            req.writeSize = cacheLineSize;
            req.validWriteSize = writeSize;
            req.baseAddrTR0 = tileArg.baseAddrTR0;
            req.baseAddrGM = tileArg.baseAddrGM;
            req.d1TRIdx = d1TRIdx;
            req.d2TRIdx = d2TRIdx;
            req.d1TR = tileArg.d1TR;
            req.d2TR = tileArg.d2TR;
            req.d1GM = tileArg.d1GM;
            req.d2GM = tileArg.d2GM;
            req.strideGM = tileArg.strideGM;
            req.dataSize = dataSize;
            req.srcDataSize = dataSize;
            req.writeMask = std::vector<bool>(cacheLineSize, false);
            for (uint32_t j = 0; j < writeSize; ++j) {
                req.writeMask.at((writeAddr - alignWriteAddr) + j) = true;
            }
            writeReqs.push_back(req);

            writeAddr += writeSize;
        }
    }
}

/* ND2NZ & DN2ZN */
static void SplitGatherLayoutTRWriteAddrs(std::vector<WriteReq>& writeReqs, TileBridgeArg tileArg,
                                          uint32_t ringTransD2Num, uint32_t ringTransD1Num)
{
    constexpr uint32_t fractalSize = FRACTAL_ROW_BYTES * FRACTAL_ROW_NUM;
    constexpr uint32_t ringTransSize = fractalSize / 2;
    constexpr uint32_t halfFractalRowNum = FRACTAL_ROW_NUM / 2;
    for (uint32_t i = 0; i < ringTransD2Num; ++i) {
        for (uint32_t j = 0; j < ringTransD1Num; ++j) {
            WriteReq req;
            req.readAddr = tileArg.baseAddrGM + i * (halfFractalRowNum * tileArg.strideGM) + j * FRACTAL_ROW_BYTES;
            req.writeAddr = tileArg.baseAddrTR0 + i * ringTransSize + j * (tileArg.originalD2TR * FRACTAL_ROW_BYTES);
            req.writeSize = ringTransSize;
            req.strideGM = tileArg.strideGM;
            /* FIXME: How to set the padding to be determined. */
            req.validWriteSize = ringTransSize;
            writeReqs.push_back(req);
        }
    }
}

/* ND2ZN & DN2NZ */
static void SplitScatterLayoutTRWriteAddrs(std::vector<WriteReq>& writeReqs, TileBridgeArg tileArg,
                                           uint32_t ringTransD2Num, uint32_t ringTransD1Num, double dataSize)
{
    constexpr uint32_t fractalSize = FRACTAL_ROW_BYTES * FRACTAL_ROW_NUM;
    constexpr uint32_t ringTransSize = fractalSize / 2;
    constexpr uint32_t halfFractalRowNum = FRACTAL_ROW_NUM / 2;
    uint32_t fractalColNum = FRACTAL_ROW_BYTES / dataSize;
    for (uint32_t i = 0; i < ringTransD2Num; ++i) {
        for (uint32_t j = 0; j < ringTransD1Num; ++j) {
            WriteReq req;
            req.readAddr = tileArg.baseAddrGM + i * (fractalColNum * tileArg.strideGM) +
                           static_cast<uint64_t>(j * (halfFractalRowNum * dataSize));
            req.writeAddr = tileArg.baseAddrTR0 + j * (ringTransSize) + i * (tileArg.d1TR * FRACTAL_ROW_BYTES);
            req.writeSize = ringTransSize;
            req.strideGM = tileArg.strideGM;
            req.dataSize = dataSize;
            req.srcDataSize = dataSize;
            /* FIXME: How to set the padding to be determined. */
            req.validWriteSize = ringTransSize;
            writeReqs.push_back(req);
        }
    }
}

void BridgePairQ::InsertSplitedBPQ()
{
    auto getNextBPQEntry = [this]() -> BPQEntryPtr {
        BPQEntryPtr entry = nullptr;
        if (RemainBPQSize(false) > 0) {
            BPQEntryPtr otherEntry = m_splitedBPQList.empty() ? nullptr : m_splitedBPQList.front();
            BPQEntryPtr tstoreEntry = m_splitedTStoreBPQList.empty() ? nullptr : m_splitedTStoreBPQList.front();
            if (otherEntry != nullptr && tstoreEntry != nullptr) {
                if (LessEqual(otherEntry->bid, tstoreEntry->bid)) {
                    entry = otherEntry;
                    m_splitedBPQList.pop_front();
                } else {
                    entry = tstoreEntry;
                    m_splitedTStoreBPQList.pop_front();
                }
            } else if (otherEntry != nullptr && tstoreEntry == nullptr) {
                entry = otherEntry;
                m_splitedBPQList.pop_front();
            } else if (otherEntry == nullptr && tstoreEntry != nullptr) {
                entry = tstoreEntry;
                m_splitedTStoreBPQList.pop_front();
            } else {
                return nullptr;
            }
        } else if (RemainBPQSize(true) > 0) {
            m_lsuBridgeTloadQ->setStall();
            bccTAUBlockCmdQ->setStall();
            if (m_splitedTStoreBPQList.empty()) {
                return nullptr;
            }
            entry = m_splitedTStoreBPQList.front();
            m_splitedTStoreBPQList.pop_front();
        } else {
            m_lsuBridgeTloadQ->setStall();
            m_lsuBridgeTstoreQ->setStall();
            bccTAUBlockCmdQ->setStall();
        }

        return entry;
    };

    if (!m_splitedBPQList.empty() || !m_splitedTStoreBPQList.empty()) {
        BPQEntryPtr entry = getNextBPQEntry();
        if (entry == nullptr) {
            return;
        }

        if (entry->tileArg.op == TileOp::TLOAD) {
            ++m_tloadCnt;
            HandleTLoadBPQ(entry);
        } else if (entry->tileArg.op == TileOp::TMOV) {
            ++m_tmovCnt;
            HandleTMovBPQ(entry);
        } else if (entry->tileArg.op == TileOp::TSTORE) {
            ++m_tstoreCnt;
            HandleTStoreBPQ(entry);
        } else if (entry->tileArg.op == TileOp::MGATHER) {
            ++m_mgatherCnt;
            HandleGatherBPQ(entry);
        } else if (entry->tileArg.op == TileOp::MSCATTER) {
            ++m_mscatterCnt;
            HandleMScatterBPQ(entry);
        } else if (entry->tileArg.op == TileOp::VGATHER) {
            ++m_vgatherCnt;
            HandleGatherBPQ(entry);
        } else if (entry->tileArg.op == TileOp::VSCATTER) {
            ++m_vscatterCnt;
            HandleVScatterBPQ(entry);
        } else {
            ASSERT(false && "Unsupport tile op");
        }
    }

    if (m_bpq.size() == m_config.BPQ_SIZE - 1) {
        m_lsuBridgeTloadQ->setStall();
        m_lsuBridgeTstoreQ->setStall();
        bccTAUBlockCmdQ->setStall();
    }
}

void BridgePairQ::SplitTloadAndTstoreSubRowBPQ(std::vector<BPQEntryPtr>& subInstBPQArray, const uint64_t splitD2TR)
{
    uint32_t splitBPQNum = 0;
    if (subInstBPQArray[0]->tileArg.op == TileOp::TLOAD) {
        splitBPQNum = (subInstBPQArray[0]->tileArg.d2TR + splitD2TR - 1) / splitD2TR;
    } else if (subInstBPQArray[0]->tileArg.op == TileOp::TSTORE) {
        /*
         * This is an optimization for the case where padding exists in tstore. When the number of remaining
         * rows that need to be written to the GM is 0, this sub-BPQ does not need to be written or read, and
         * therefore does not need to be generated.
         */
        splitBPQNum = (subInstBPQArray[0]->tileArg.d2GM + splitD2TR - 1) / splitD2TR;
    }
    std::vector<uint64_t> baseAddrGMArr;
    std::vector<uint64_t> baseAddrTRArr;
    std::vector<uint64_t> remainD2TRArr;
    std::vector<uint64_t> remainD2GMArr;

    for (BPQEntryPtr entry : subInstBPQArray) {
        baseAddrGMArr.push_back(entry->tileArg.baseAddrGM);
        baseAddrTRArr.push_back(entry->tileArg.baseAddrTR0);
        remainD2TRArr.push_back(entry->tileArg.d2TR);
        remainD2GMArr.push_back(entry->tileArg.d2GM);
    }

    double dataSize = GetDataSize(subInstBPQArray[0]->blockCmd->dataType);
    std::vector<BPQEntryPtr> d2HeadEntryArr;
    for (uint32_t i = 0; i < splitBPQNum; ++i) {
        BPQEntryPtr d1HeadEntry = nullptr;
        for (uint32_t j = 0; j < subInstBPQArray.size(); ++j) {
            BPQEntryPtr subInstBPQ = subInstBPQArray[j];
            BPQEntryPtr entry = std::make_shared<BPQEntry>(*subInstBPQ);
            if (j == 0) {
                d1HeadEntry = entry;
                entry->bdbRelatedBPQNum = subInstBPQArray.size() - 1;
            } else {
                entry->rowLeadBPQPtr = d1HeadEntry;
            }
            if (i == 0) {
                entry->attr = (j == 0) ? BPQAttr::SUB_INST_LEAD : BPQAttr::SUB_INST;
                entry->activeInstBPQNum = (j == 0) ? subInstBPQ->activeInstBPQNum : 0;
                entry->activeRowBPQNum = splitBPQNum - 1;
                d2HeadEntryArr.push_back(entry);
            } else {
                entry->attr = (j == 0) ? BPQAttr::SUB_ROW_LEAD : BPQAttr::SUB_ROW;
                entry->activeInstBPQNum = 0;
                entry->activeRowBPQNum = 0;
                entry->instLeadBPQPtr = d2HeadEntryArr[j];
            }
            entry->tileArg.baseAddrGM = baseAddrGMArr[j];
            entry->tileArg.d2GM = std::min(splitD2TR, remainD2GMArr[j]);
            entry->tileArg.baseAddrTR0 = baseAddrTRArr[j];
            entry->tileArg.d2TR = std::min(splitD2TR, remainD2TRArr[j]);
            baseAddrGMArr[j] += entry->tileArg.d2TR * entry->tileArg.strideGM;
            if (entry->tileArg.layout == LayOut::ND2NZ || entry->tileArg.layout == LayOut::DN2ZN) {
                baseAddrTRArr[j] += entry->tileArg.d2TR * m_config.FRACTAL_ROW_BYTES;
            } else if (entry->tileArg.layout == LayOut::ND2ZN || entry->tileArg.layout == LayOut::DN2NZ ||
                       entry->tileArg.layout == LayOut::NORM) {
                baseAddrTRArr[j] += entry->tileArg.d2TR * entry->tileArg.d1TR * dataSize;
            } else {
                ASSERT(false && "Unsupport layout");
            }
            remainD2TRArr[j] -= entry->tileArg.d2TR;
            remainD2GMArr[j] = (remainD2GMArr[j] <= entry->tileArg.d2GM) ? 0 : remainD2GMArr[j] - entry->tileArg.d2GM;

            if (entry->tileArg.op == TileOp::TLOAD) {
                m_splitedBPQList.emplace_back(entry);
            } else if (entry->tileArg.op == TileOp::TSTORE) {
                m_splitedTStoreBPQList.emplace_back(entry);
            }
        }
    }
}

std::shared_ptr<std::vector<BPQEntryPtr>> BridgePairQ::SplitTLoadSubInstBPQ(MemReqBus& memReq) const
{
    uint32_t dstCount = memReq.blockCmd->dsts.size();
    BlockCommandPtr cmd = memReq.blockCmd;
    uint64_t tileSize = 0;
    for (auto& dst : cmd->dsts) {
        tileSize += dst->size;
    }

    double dataSize = GetDataSize(cmd->dataType);
    uint64_t col = cmd->lb2;
    ASSERT(IsDivisible(tileSize, (col * dataSize)));
    uint64_t row = tileSize / (col * dataSize);
    if (cmd->blockAttr->layout == LayOut::ND2NZ || cmd->blockAttr->layout == LayOut::ND2ZN) {
        ASSERT(col % dstCount == 0);
    } else if (cmd->blockAttr->layout == LayOut::DN2NZ || cmd->blockAttr->layout == LayOut::DN2ZN) {
        ASSERT(row % dstCount == 0);
    }
    if (cmd->blockAttr->layout == LayOut::ND2NZ || cmd->blockAttr->layout == LayOut::ND2ZN) {
        ASSERT(col % dstCount == 0);
    } else if (cmd->blockAttr->layout == LayOut::DN2NZ || cmd->blockAttr->layout == LayOut::DN2ZN) {
        ASSERT(row % dstCount == 0);
    }

    uint64_t originalD1TR = 0;
    uint64_t originalD2TR = 0;
    uint64_t originalD1GM = 0;
    uint64_t originalD2GM = 0;
    switch (cmd->blockAttr->layout) {
        case LayOut::NORM:
        case LayOut::ND2NZ:
        case LayOut::ND2ZN:
            originalD1GM = cmd->lb0;
            originalD2GM = cmd->lb1;
            originalD1TR = col;
            originalD2TR = row;
            break;
        case LayOut::DN2NZ:
        case LayOut::DN2ZN:
            originalD2GM = cmd->lb0;
            originalD1GM = cmd->lb1;
            originalD2TR = col;
            originalD1TR = row;
            break;
        default:
            break;
    }

    ASSERT(originalD1TR % dstCount == 0);
    uint64_t splitD1TR = originalD1TR / dstCount;
    /* When splitD1TR is an odd number and the dataType is FP4, the baseAddrGM may be at a halfword position. */
    if (DataTypeIsFP4(cmd->dataType)) {
        ASSERT((splitD1TR & 1) == 0);
    }
    BPQEntryPtr instLeadBPQPtr = nullptr;
    std::shared_ptr<std::vector<BPQEntryPtr>> subInstBPQArray = std::make_shared<std::vector<BPQEntryPtr>>();
    for (uint32_t i = 0; i < dstCount; ++i) {
        BPQEntryPtr entry = std::make_shared<BPQEntry>();
        TileBridgeArg& tileArg = entry->tileArg;
        entry->bid = memReq.bid;
        entry->direction = Direction::GM2TR;
        entry->blockCmd = std::make_shared<BlockCommand>(*memReq.blockCmd);
        entry->blockCmd->dsts.clear();
        entry->blockCmd->dsts.push_back(memReq.blockCmd->dsts[i]);
        entry->pairStatus = ExecStatus::NO_START;

        if (dstCount == 1) {
            entry->attr = BPQAttr::ORIGINAL;
        } else {
            if (i == 0) {
                instLeadBPQPtr = entry;
                entry->attr = BPQAttr::SUB_INST_LEAD;
                entry->activeInstBPQNum = dstCount - 1;
                entry->bdbRelatedBPQNum = dstCount - 1;
            } else {
                entry->rowLeadBPQPtr = instLeadBPQPtr;
                entry->attr = BPQAttr::SUB_INST;
                entry->pairStatus = ExecStatus::WAIT_OTHER_BPQ;
            }
        }

        if (cmd->srcData.size() == 1) {
            double tmpStride = cmd->lb0 * dataSize;
            ASSERT(IsInteger(tmpStride));
            tileArg.strideGM = static_cast<uint32_t>(tmpStride);
        } else {
            tileArg.strideGM = cmd->srcData[1];
        }

        tileArg.op = cmd->tileOp;
        tileArg.srcType = cmd->dataType;
        tileArg.layout = cmd->blockAttr->layout;
        tileArg.baseAddrGM = cmd->srcData[0] + static_cast<uint64_t>((splitD1TR * dataSize) * i);
        tileArg.originalD1GM = originalD1GM;
        tileArg.d1GM = ((splitD1TR * i) > originalD1GM) ? 0 : std::min(splitD1TR, originalD1GM - (splitD1TR * i));
        tileArg.d2GM = originalD2GM;
        tileArg.baseAddrTR0 = cmd->dsts[i]->baseAddr;
        tileArg.d1TR = splitD1TR;
        tileArg.d2TR = originalD2TR;
        tileArg.originalD2TR = originalD2TR;
        tileArg.stid = cmd->stid;

        subInstBPQArray->emplace_back(entry);
    }

    return subInstBPQArray;
}

void BridgePairQ::SplitSubRowBPQ(BlockCommandPtr cmd, std::vector<BPQEntryPtr>& subInstBPQArray)
{
    auto getTloadOriginalD2TR = [](BlockCommandPtr cmd) -> uint64_t {
        uint64_t tileSize = 0;
        for (auto& dst : cmd->dsts) {
            tileSize += dst->size;
        }

        double dataSize = GetDataSize(cmd->dataType);
        uint64_t col = cmd->lb2;
        ASSERT(IsDivisible(tileSize, (col * dataSize)));
        uint64_t row = tileSize / (col * dataSize);
        uint64_t originalD2TR = 0;
        switch (cmd->blockAttr->layout) {
            case LayOut::NORM:
            case LayOut::ND2NZ:
            case LayOut::ND2ZN:
                originalD2TR = row;
                break;
            case LayOut::DN2NZ:
            case LayOut::DN2ZN:
                originalD2TR = col;
                break;
            default:
                ASSERT(false && "Unsupport layout") << cmd->Dump();
                break;
        }

        return originalD2TR;
    };

    uint64_t originalD2TR = 0;
    if (cmd->tileOp == TileOp::TLOAD) {
        originalD2TR = getTloadOriginalD2TR(cmd);
    } else if (cmd->tileOp == TileOp::TMOV || cmd->tileOp == TileOp::TSTORE) {
        originalD2TR = subInstBPQArray[0]->tileArg.d2TR;
    }

    uint32_t splitD2TR = 0;
    if (cmd->blockAttr->layout == LayOut::NORM) {
        splitD2TR = m_config.NORM_LAYOUT_SPLIT_ROW_GRANULARITY;
    } else {
        if (DataTypeIsFP4(cmd->dataType) || (cmd->blockAttr != nullptr && DataTypeIsFP4(cmd->blockAttr->dataType))) {
            splitD2TR = m_config.FP4_OTHER_LAYOUT_SPLIT_ROW_GRANULARITY;
        } else {
            splitD2TR = m_config.OTHER_LAYOUT_SPLIT_ROW_GRANULARITY;
        }
    }

    double dataSize = GetDataSize(cmd->dataType);
    bool needSplitRow = originalD2TR > splitD2TR;
    if (cmd->blockAttr->layout == LayOut::NORM) {
        if (cmd->tileOp == TileOp::TLOAD &&
            static_cast<uint64_t>(cmd->lb2 * dataSize) % m_config.FRACTAL_ROW_BYTES != 0) {
            /*
             * Considering that the row size can be split according to the fractal,
             * it is considered as the TLOAD under the ruminate solution.
             */
            needSplitRow = false;
        }

        if (cmd->tileOp == TileOp::TMOV) {
            /* FIXME: Then consider how to split row for normal layout. */
            needSplitRow = false;
        }
    }

    if (m_config.ENABLE_SPLIT_ROW_BPQ && needSplitRow) {
        if (cmd->tileOp == TileOp::TLOAD || cmd->tileOp == TileOp::TSTORE) {
            SplitTloadAndTstoreSubRowBPQ(subInstBPQArray, splitD2TR);
        } else if (cmd->tileOp == TileOp::TMOV) {
            SplitTmovSubRowBPQ(subInstBPQArray, splitD2TR);
        }
    } else {
        for (BPQEntryPtr entry : subInstBPQArray) {
            m_splitedBPQList.emplace_back(entry);
        }
    }
}

void BridgePairQ::HandleTLoadBPQ(BPQEntryPtr entry)
{
    double dataSize = GetDataSize(entry->tileArg.srcType);
    uint64_t fractalSize = m_config.FRACTAL_ROW_BYTES * m_config.FRACTAL_ROW_NUM;

    ASSERT(entry->tileArg.d1GM <= entry->tileArg.d1TR);
    ASSERT(entry->tileArg.d2GM <= entry->tileArg.d2TR);
    ASSERT((entry->tileArg.baseAddrTR0 & TILE_ALIGN_MASK) == entry->tileArg.baseAddrTR0);

    if (entry->pairStatus == ExecStatus::NO_START) {
        SplitGMReadAddrs(entry->readReqs, entry->tileArg, dataSize,
                         m_config.GLOBAL_MEMORY_CACHELINE_SIZE,
                         m_config.BRIDGE_DATA_BUFFER_DATA_SIZE);
        ASSERT(entry->readReqs.size() <= m_config.BRIDGE_DATA_BUFFER_SIZE);
    }

    double tileSize = entry->tileArg.d1TR * entry->tileArg.d2TR * dataSize;
    if (entry->tileArg.layout != LayOut::NORM) {
        /*
         * When the matrix in GM is ND, the first continuous dimension is the column,
         * so D1TR is Col, and D2TR is Row.
         * When the matrix in GM is DN, the first continuous dimension is the column,
         * so D1TR is Row, and D2TR is Col.
         */
        double rowSize = 0;
        ASSERT(IsDivisible(tileSize, fractalSize));
        if (entry->tileArg.layout == LayOut::ND2NZ || entry->tileArg.layout == LayOut::DN2ZN) {
            ASSERT(entry->tileArg.d2TR % m_config.FRACTAL_ROW_NUM == 0);
            rowSize = entry->tileArg.d1TR * dataSize;
            ASSERT(IsDivisible(rowSize, m_config.FRACTAL_ROW_BYTES));
        } else if (entry->tileArg.layout == LayOut::DN2NZ || entry->tileArg.layout == LayOut::ND2ZN) {
            ASSERT(entry->tileArg.d1TR % m_config.FRACTAL_ROW_NUM == 0);
            rowSize = entry->tileArg.d2TR * dataSize;
            ASSERT(IsDivisible(rowSize, m_config.FRACTAL_ROW_BYTES));
        } else {
            LOG_ERROR_M(Unit::TMA, Stage::NA) << "[Bridge Table]: Unsupport layout.";
            abort();
        }
        if (entry->tileArg.layout == LayOut::ND2NZ || entry->tileArg.layout == LayOut::DN2ZN) {
            uint32_t ringTransD2Num = entry->tileArg.d2TR / m_config.FRACTAL_ROW_NUM * 2;
            uint32_t ringTransD1Num = static_cast<uint64_t>(rowSize) / m_config.FRACTAL_ROW_BYTES;
            SplitGatherLayoutTRWriteAddrs(entry->writeReqs, entry->tileArg, ringTransD2Num, ringTransD1Num);
        } else if (entry->tileArg.layout == LayOut::ND2ZN || entry->tileArg.layout == LayOut::DN2NZ) {
            uint32_t ringTransD2Num = static_cast<uint64_t>(rowSize) / m_config.FRACTAL_ROW_BYTES;
            uint32_t ringTransD1Num = entry->tileArg.d1TR / m_config.FRACTAL_ROW_NUM * 2;
            SplitScatterLayoutTRWriteAddrs(entry->writeReqs, entry->tileArg, ringTransD2Num, ringTransD1Num, dataSize);
        } else {
            LOG_ERROR_M(Unit::TMA, Stage::NA) << "[Bridge Table]: Unsupport layout.";
            abort();
        }
    } else {
        SplitNormTRWriteAddrs(entry->writeReqs, entry->tileArg, dataSize, MAX_TILE_DATA_BYTE);
    }

    m_pmuData.tloadNum++;
    for (const auto& readReq : entry->readReqs) {
        // 查找 readReq.readSize 是否已经存在于 tloadReadStat 中
        auto it = m_pmuData.tloadReadStat.find(readReq.readSize);
        if (it != m_pmuData.tloadReadStat.end()) {
            // 如果找到，增加计数
            it->second++;
        } else {
            // 如果未找到，插入新的键值对，初始值为 1
            m_pmuData.tloadReadStat[readReq.readSize] = 1;
        }
    }
    m_pmuData.tloadWriteNum += entry->writeReqs.size();

    LOG_INFO_M(Unit::TMA, Stage::BPQ) << "[Bridge Table]: bid:" << std::dec << entry->bid.GetVal()
        << " Tload split " << std::dec << entry->readReqs.size()
        << " read req, " << entry->writeReqs.size() << " write req"
        << " to bpq id:" << std::dec << m_bpqIDPool.top();

    if (entry->readReqs.size() == 0) {
        /* After split row on data with padding, BPQ do not need to read GM. */
        entry->pairStatus = ExecStatus::SEND_WRITE;
        entry->genWriteCycle = GetSim()->getCycles();
        if (entry->genReadCycle == 0) {
            IncOutstandingTileOpNum(entry->tileArg.op);
        }
    }

    if (entry->pairStatus == ExecStatus::NO_START) {
        entry->startCycle = GetSim()->getCycles();
    } else if (entry->pairStatus == ExecStatus::WAIT_OTHER_BPQ) {
        entry->waitOtherBPQCycle = GetSim()->getCycles();
    }
    entry->bpqID = m_bpqIDPool.top();
    m_bpq.emplace_back(entry);
    m_bpqIDPool.pop();
}

bool BridgePairQ::ReceiveTLoadReq()
{
    if (m_lsuBridgeTloadQ->Empty()) {
        return false;
    }

    if (RemainBPQSize(false) <= 0) {
        return false;
    }

    if (GetSim()->core->configs.ruminateEnable && id >= GetSim()->core->configs.tauStartId) {
        ASSERT(false && "Tload will not be sent to the TAU.");
    }

    MemReqBus&& memReq = m_lsuBridgeTloadQ->Read();
    LOG_INFO_M(Unit::TMA, Stage::NA) << "[Bridge Table" << id << "]: receive Tload req:" << memReq.blockCmd->Dump();
    GetSim()->GetVerifyManager(memReq.stid)->InitPARGroup(memReq.bid.val, 0);
    GetSim()->GetViewManager(memReq.stid)->InitBIQType(memReq.bid.val, BIQType::TMA_IQ);
    GetSim()->GetViewManager(memReq.stid)->InitPARGroup(memReq.bid.val, 0);
    ASSERT(!m_tidPool.empty());
    memReq.blockCmd->execMachineId = machineId + m_tidPool.front();
    m_tidPool.pop_front();
    ASSERT(memReq.stid == memReq.blockCmd->stid);
    m_tloadBidOrderList[memReq.stid].push_back(memReq.bid);

    uint64_t tileSize = 0;
    for (auto& dst : memReq.blockCmd->dsts) {
        tileSize += dst->size;
    }
    uint64_t col = memReq.blockCmd->lb2;
    double dataSize = GetDataSize(memReq.blockCmd->dataType);
    uint64_t row = tileSize / (col * dataSize);
    m_pmuData.tloadWriteDataSize += col * row * dataSize;
    m_pmuData.tloadWriteValidDataSize += memReq.blockCmd->lb0 * memReq.blockCmd->lb1 * dataSize;

    std::shared_ptr<std::vector<BPQEntryPtr>> subInstBPQArray = SplitTLoadSubInstBPQ(memReq);
    SplitSubRowBPQ(memReq.blockCmd, *subInstBPQArray);

    return true;
}

static void SplitNormTRReadAddrs(std::vector<ReadReq>& readReqs, TileBridgeArg& tileArg, double dataSize,
                                 uint64_t cacheLineSize)
{
    std::unordered_set<uint64_t> uniqueCacheLines;
    uint64_t effectiveSize = static_cast<uint64_t>(std::round(tileArg.d1GM * dataSize));
    for (uint32_t i = 0; i < tileArg.d2GM; ++i) {
        /*
         * FIXME: Consider the case where the number of columns is odd, which may cause the startAddr
         * not be byte-aligned.
         */
        ASSERT((tileArg.d1TR & 1) == 0);
        uint64_t startAddr = tileArg.baseAddrTR0 + static_cast<uint64_t>(i * tileArg.d1TR * dataSize);
        uint64_t endAddr = startAddr + effectiveSize;
        uint64_t readAddr = startAddr & (~(cacheLineSize - 1));
        for (; readAddr < endAddr; readAddr += cacheLineSize) {
            if (uniqueCacheLines.count(readAddr) == 0) {
                ReadReq req;
                req.readAddr = readAddr;
                req.readSize = cacheLineSize;
                readReqs.push_back(req);
                uniqueCacheLines.insert(readAddr);
            }
        }
    }
}

static void SplitNormGMWriteAddrs(std::vector<WriteReq>& writeReqs, TileBridgeArg& tileArg, double dataSize,
                                  uint64_t cacheLineSize)
{
    /*
     * FIXME: Consider the case where the number of columns is odd, which may cause the address
     * not be byte-aligned. The method of generating write requests will be changed later.
     */
    if (DoubleEqual(dataSize, HALF_BYTE_DATA_WIDTH)) {
        ASSERT((tileArg.d1TR & 1) == 0);
        ASSERT((tileArg.d1GM & 1) == 0);
    }
    uint32_t d1TRSize = tileArg.d1TR * dataSize;
    uint32_t d1GMSize = tileArg.d1GM * dataSize;
    for (uint32_t i = 0; i < tileArg.d2GM; ++i) {
        uint64_t writeAddr = tileArg.baseAddrGM + i * tileArg.strideGM;
        uint64_t endAddr = writeAddr + d1GMSize;
        uint64_t offset = 0;
        while (writeAddr < endAddr) {
            uint64_t alignWriteAddr = writeAddr & ~(cacheLineSize - 1);
            uint32_t writeSize = cacheLineSize - (writeAddr % cacheLineSize);
            if (writeAddr + writeSize > endAddr) {
                writeSize = endAddr - writeAddr;
            }
            WriteReq req;
            req.writeAddr = alignWriteAddr;
            req.writeSize = cacheLineSize;
            req.validWriteSize = writeSize;
            req.readAddr = tileArg.baseAddrTR0 + i * d1TRSize + offset;
            req.writeMask = std::vector<bool>(cacheLineSize, false);
            for (uint64_t j = 0; j < req.validWriteSize; ++j) {
                uint64_t pos = (writeAddr - alignWriteAddr) + j;
                req.writeMask.at(pos) = true;
            }
            writeReqs.push_back(req);
            offset += writeSize;
            writeAddr += writeSize;
        }
    }
}

static void SplitTRReadAddrs(std::vector<ReadReq>& readReqs, TileBridgeArg& tileArg, double dataSize,
                             uint64_t cacheLineSize)
{
    uint64_t tileSize = tileArg.d1TR * tileArg.d2TR * dataSize;
    uint64_t readAddr = tileArg.baseAddrTR0;
    uint64_t endAddr = readAddr + tileSize;
    ASSERT((readAddr & ~(cacheLineSize - 1)) == readAddr);
    while (readAddr < endAddr) {
        ReadReq req;
        req.readAddr = readAddr;
        req.readSize = cacheLineSize;
        readReqs.push_back(req);
        readAddr += cacheLineSize;
    }
}

/* NZ2ND */
static void SplitTstoreGMWriteAddrs(std::vector<WriteReq>& writeReqs, TileBridgeArg& tileArg, double dataSize,
                                    uint64_t cacheLineSize)
{
    uint64_t d1GMIdx = 0;
    uint64_t d2GMIdx = 0;
    /*
     * FIXME: Considering the case where d1GM is an odd number may result in a byte where only half of the word is
     * valid in a row. This issue will be considered for modification when encountered in the future.
     */
    if (DoubleEqual(dataSize, HALF_BYTE_DATA_WIDTH)) {
        ASSERT((tileArg.d1GM & 1) == 0);
    }
    uint64_t rowValidSize = tileArg.d1GM * dataSize;
    uint64_t writeAddr = tileArg.baseAddrGM;
    uint64_t writeEndAddr = writeAddr + (tileArg.d2GM - 1) * tileArg.strideGM + rowValidSize;

    while (writeAddr < writeEndAddr) {
        uint64_t alignWriteAddr = writeAddr & ~(cacheLineSize - 1);
        uint64_t nextCacheLineAddr = (writeAddr + cacheLineSize) & ~(cacheLineSize - 1);
        uint64_t writeSize = nextCacheLineAddr - writeAddr;
        /* FIXME: Add write mask to shield non-writable areas */
        if (writeAddr + writeSize > writeEndAddr) {
            writeSize = writeEndAddr - writeAddr;
        }
        WriteReq req;
        req.writeAddr = alignWriteAddr;
        req.writeSize = cacheLineSize;
        req.validWriteSize = writeSize;
        req.baseAddrTR0 = tileArg.baseAddrTR0;
        req.strideGM = tileArg.strideGM;
        req.d1GM = tileArg.d1GM;
        req.d2GM = tileArg.d2GM;
        req.d1TR = tileArg.d1TR;
        req.d2TR = tileArg.d2TR;
        req.d1GMIdx = d1GMIdx;
        req.d2GMIdx = d2GMIdx;
        req.dataSize = dataSize;
        req.srcDataSize = dataSize;
        req.writeMask = std::vector<bool>(cacheLineSize, false);
        for (uint64_t i = 0; i < req.validWriteSize; ++i) {
            uint64_t pos = (writeAddr - alignWriteAddr) + i;
            req.writeMask.at(pos) = true;
        }
        writeReqs.push_back(req);

        writeAddr += writeSize;
        d1GMIdx += writeSize;

        uint64_t skipRowLine = d1GMIdx / tileArg.strideGM;
        d2GMIdx += skipRowLine;
        d1GMIdx %= tileArg.strideGM;
        if (d1GMIdx >= rowValidSize) {
            d1GMIdx = 0;
            d2GMIdx++;
            writeAddr = tileArg.baseAddrGM + d2GMIdx * tileArg.strideGM;
        }
    }
}

void BridgePairQ::SplitTStoreNormSubColBPQ(BPQEntryPtr entry)
{
    constexpr uint64_t maxTstoreNormColNum = 4096;
    uint32_t subCount = (entry->tileArg.d1GM + maxTstoreNormColNum - 1) / maxTstoreNormColNum;

    uint64_t baseAddrGM = entry->tileArg.baseAddrGM;
    uint64_t baseAddrTR0 = entry->tileArg.baseAddrTR0;

    double dataSize = GetDataSize(entry->blockCmd->dataType);
    BPQEntryPtr instLeadBPQPtr = nullptr;
    for (uint32_t i = 0; i < subCount; ++i) {
        BPQEntryPtr subEntry = std::make_shared<BPQEntry>(*entry);

        uint64_t splitD1TR = ((maxTstoreNormColNum * (i + 1)) > entry->tileArg.d1TR) ?
            entry->tileArg.d1TR - maxTstoreNormColNum * i : maxTstoreNormColNum;
        /* When splitD1TR is an odd number and the dataType is FP4, the baseAddrGM may be at a halfword position. */
        if (DataTypeIsFP4(entry->blockCmd->dataType)) {
            ASSERT(((splitD1TR & 1) == 0));
        }

        if (subCount == 1) {
            subEntry->attr = BPQAttr::ORIGINAL;
        } else {
            if (i == 0) {
                instLeadBPQPtr = subEntry;
                subEntry->attr = BPQAttr::SUB_COL_LEAD;
                subEntry->activeInstBPQNum = subCount - 1;
            } else {
                subEntry->rowLeadBPQPtr = instLeadBPQPtr;
                subEntry->attr = BPQAttr::SUB_COL;
            }
        }

        subEntry->tileArg.baseAddrGM = baseAddrGM;
        subEntry->tileArg.d1GM = splitD1TR;
        subEntry->tileArg.baseAddrTR0 = baseAddrTR0;
        subEntry->tileArg.d1TR = splitD1TR;

        m_splitedTStoreBPQList.emplace_back(subEntry);

        baseAddrTR0 += static_cast<uint64_t>(splitD1TR * dataSize);
        baseAddrGM += static_cast<uint64_t>(splitD1TR * dataSize);
    }
}

void BridgePairQ::HandleTStoreBPQ(BPQEntryPtr entry)
{
    ASSERT(entry->tileArg.d1GM <= entry->tileArg.d1TR);
    ASSERT(entry->tileArg.d2GM <= entry->tileArg.d2TR);
    ASSERT((entry->tileArg.baseAddrTR0 & TILE_ALIGN_MASK) == entry->tileArg.baseAddrTR0);

    double dataSize = GetDataSize(entry->tileArg.srcType);
    uint64_t fractalSize = m_config.FRACTAL_ROW_BYTES * m_config.FRACTAL_ROW_NUM;
    double tileSize = entry->tileArg.d1TR * entry->tileArg.d2TR * dataSize;
    if (entry->tileArg.layout != LayOut::NORM) {
        ASSERT(IsDivisible(tileSize, fractalSize));
        ASSERT(entry->tileArg.layout == LayOut::ND2NZ);
        ASSERT(entry->tileArg.d2TR % m_config.FRACTAL_ROW_NUM == 0);
        double rowSize = entry->tileArg.d1TR * dataSize;
        ASSERT(IsDivisible(rowSize, m_config.FRACTAL_ROW_BYTES));
        SplitTRReadAddrs(entry->readReqs, entry->tileArg, dataSize, MAX_TILE_DATA_BYTE);
        SplitTstoreGMWriteAddrs(entry->writeReqs, entry->tileArg, dataSize, m_config.SWCB_MAX_WRITE_SIZE);
    } else {
        SplitNormTRReadAddrs(entry->readReqs, entry->tileArg, dataSize, MAX_TILE_DATA_BYTE);
        SplitNormGMWriteAddrs(entry->writeReqs, entry->tileArg, dataSize, m_config.SWCB_MAX_WRITE_SIZE);
    }
    uint64_t bdbNumPerReadReq = MAX_TILE_DATA_BYTE / m_config.BRIDGE_DATA_BUFFER_DATA_SIZE;
    ASSERT(entry->readReqs.size() * bdbNumPerReadReq <= m_config.BRIDGE_DATA_BUFFER_TSTORE_ONLY_USE_SIZE);

    m_pmuData.tstoreNum++;
    m_pmuData.tstoreReadNum += entry->readReqs.size();
    m_pmuData.tstoreReadDataSize += (entry->readReqs.size() * MAX_TILE_DATA_BYTE);
    for (const auto& writeReq : entry->writeReqs) {
        // 查找 readReq.readSize 是否已经存在于 tloadReadStat 中
        auto it = m_pmuData.tstoreWriteStat.find(writeReq.writeSize);
        if (it != m_pmuData.tstoreWriteStat.end()) {
            // 如果找到，增加计数
            it->second++;
        } else {
            // 如果未找到，插入新的键值对，初始值为 1
            m_pmuData.tstoreWriteStat[writeReq.writeSize] = 1;
        }
    }

    entry->pairStatus = ExecStatus::NO_START;
    entry->startCycle = GetSim()->getCycles();
    entry->bpqID = m_bpqIDPool.top();
    m_bpq.emplace_back(entry);
    m_bpqIDPool.pop();
}

bool BridgePairQ::ReceiveTStoreReq()
{
    if (m_lsuBridgeTstoreQ->Empty()) {
        return false;
    }

    if (RemainBPQSize(true) <= 0) {
        return false;
    }

    if (GetSim()->core->configs.ruminateEnable && id >= GetSim()->core->configs.tauStartId) {
        ASSERT(false && "Tstore will not be sent to the TAU.");
    }

    MemReqBus&& memReq = m_lsuBridgeTstoreQ->Read();
    LOG_INFO_M(Unit::TMA, Stage::NA) << "[Bridge Table" << id << "]: receive Tstore req:" << memReq.blockCmd->Dump();
    ASSERT(!m_tidPool.empty());
    memReq.blockCmd->execMachineId = machineId + m_tidPool.front();
    m_tidPool.pop_front();

    std::vector<BPQEntryPtr> subInstBPQArray;
    BPQEntryPtr entry = std::make_shared<BPQEntry>();
    entry->bid = memReq.bid;
    entry->direction = Direction::TR2GM;
    entry->tileArg.InitByMemReqBus(memReq.blockCmd);
    GetSim()->GetVerifyManager(entry->tileArg.stid)->InitPARGroup(entry->bid.val, 0);
    GetSim()->GetViewManager(entry->tileArg.stid)->InitBIQType(entry->bid.val, BIQType::TMA_IQ);
    GetSim()->GetViewManager(entry->tileArg.stid)->InitPARGroup(entry->bid.val, 0);
    entry->blockCmd = memReq.blockCmd;

    double dataSize = GetDataSize(entry->blockCmd->dataType);
    uint64_t colSize = entry->tileArg.d1TR * dataSize;
    uint64_t bdbNumPerRow = colSize / m_config.BRIDGE_DATA_BUFFER_DATA_SIZE;
    bool colOverflow = bdbNumPerRow > m_config.BRIDGE_DATA_BUFFER_TSTORE_ONLY_USE_SIZE;
    m_pmuData.tstoreWriteValidDataSize += entry->tileArg.d1GM * entry->tileArg.d2GM * dataSize;
    if (entry->tileArg.layout == LayOut::NORM && colOverflow) {
        SplitTStoreNormSubColBPQ(entry);
    } else {
        subInstBPQArray.push_back(entry);
        SplitSubRowBPQ(memReq.blockCmd, subInstBPQArray);
    }

    return true;
}

/* ND2NZ & DN2ZN */
static void SplitTMOVGatherLayoutTRWriteAddrs(std::vector<WriteReq>& writeReqs, TileBridgeArg tileArg,
                                              uint32_t ringTransD2Num, uint32_t ringTransD1Num, double srcDataSize,
                                              double dstDataSize)
{
    constexpr uint32_t fractalSize = FRACTAL_ROW_BYTES * FRACTAL_ROW_NUM;
    constexpr uint32_t ringTransSize = fractalSize / 2;
    constexpr uint32_t ringTransRowNum = FRACTAL_ROW_NUM / 2;
    for (uint32_t i = 0; i < ringTransD2Num; ++i) {
        for (uint32_t j = 0; j < ringTransD1Num; ++j) {
            WriteReq req;
            req.readAddr = tileArg.baseAddrTR0 +
                           i * static_cast<uint64_t>(ringTransRowNum * tileArg.d1TR * srcDataSize) +
                           j * FRACTAL_ROW_BYTES;
            req.writeAddr = tileArg.baseAddrDstTR + i * ringTransSize + j * (tileArg.originalD2TR * FRACTAL_ROW_BYTES);
            req.srcDataSize = srcDataSize;
            req.dataSize = dstDataSize;
            req.d1TR = tileArg.d1TR;
            req.d2TR = tileArg.d2TR;
            req.srcType = tileArg.srcType;
            req.dstType = tileArg.dstType;
            req.writeSize = ringTransSize;
            req.validWriteSize = ringTransSize;
            writeReqs.push_back(req);
        }
    }
}

/* ND2ZN & DN2NZ */
static void SplitTMOVScatterLayoutTRWriteAddrs(std::vector<WriteReq>& writeReqs, TileBridgeArg tileArg,
                                               uint32_t ringTransD2Num, uint32_t ringTransD1Num, double srcDataSize,
                                               double dstDataSize)
{
    constexpr uint32_t fractalSize = FRACTAL_ROW_BYTES * FRACTAL_ROW_NUM;
    constexpr uint32_t ringTransSize = fractalSize / 2;
    constexpr uint32_t halfFractalRowNum = FRACTAL_ROW_NUM / 2;
    uint32_t fractalColNum = FRACTAL_ROW_BYTES / dstDataSize;
    for (uint32_t i = 0; i < ringTransD2Num; ++i) {
        for (uint32_t j = 0; j < ringTransD1Num; ++j) {
            WriteReq req;
            req.readAddr = tileArg.baseAddrTR0 + i * static_cast<uint64_t>(fractalColNum * tileArg.d1TR * srcDataSize) +
                           static_cast<uint64_t>(j * (halfFractalRowNum * srcDataSize));
            req.writeAddr = tileArg.baseAddrDstTR + j * (ringTransSize) + i * (tileArg.d1TR * FRACTAL_ROW_BYTES);
            req.writeSize = ringTransSize;
            req.d1TR = tileArg.d1TR;
            req.d2TR = tileArg.d2TR;
            req.srcDataSize = srcDataSize;
            req.dataSize = dstDataSize;
            req.srcType = tileArg.srcType;
            req.dstType = tileArg.dstType;
            req.validWriteSize = ringTransSize;
            writeReqs.push_back(req);
        }
    }
}

/* ND2ND */
static void SplitTMOVNormalLayoutTRWriteAddrs(std::vector<WriteReq>& writeReqs, TileBridgeArg tileArg,
                                              double srcDataSize, double dstDataSize)
{
    constexpr uint32_t fractalSize = FRACTAL_ROW_BYTES * FRACTAL_ROW_NUM;
    constexpr uint32_t ringTransSize = fractalSize / 2;
    uint64_t readAddr = tileArg.baseAddrTR0;
    uint64_t writeAddr = tileArg.baseAddrDstTR;
    uint64_t trEndAddr = tileArg.baseAddrDstTR + static_cast<uint64_t>(tileArg.d1TR * tileArg.d2TR * dstDataSize);
    double dataScaleFactor = srcDataSize / dstDataSize;
    while (writeAddr < trEndAddr) {
        uint64_t writeSize = ringTransSize;
        if (writeAddr + writeSize > trEndAddr) {
            writeSize = trEndAddr - writeAddr;
        }

        WriteReq req;
        req.readAddr = readAddr;
        req.writeAddr = writeAddr;
        req.writeSize = writeSize;
        req.validWriteSize = writeSize;
        req.srcDataSize = srcDataSize;
        req.dataSize = dstDataSize;
        req.srcType = tileArg.srcType;
        req.dstType = tileArg.dstType;
        req.writeMask = std::vector<bool>(MAX_GLOBAL_MEMORY_CACHELINE_SIZE, false);
        for (uint64_t j = 0; j < req.validWriteSize; ++j) {
            req.writeMask.at(j) = true;
        }
        writeReqs.push_back(req);

        ASSERT(IsInteger(writeSize * dataScaleFactor));
        readAddr += static_cast<uint64_t>(writeSize * dataScaleFactor);
        writeAddr += writeSize;
    }
}

void BridgePairQ::SplitTmovSubRowBPQ(std::vector<BPQEntryPtr>& subInstBPQArray, const uint64_t splitD2TR)
{
    uint32_t splitBPQNum = (subInstBPQArray[0]->tileArg.d2TR + splitD2TR - 1) / splitD2TR;
    std::vector<uint64_t> baseAddrTRArr;
    std::vector<uint64_t> baseAddrDstTRArr;
    std::vector<uint64_t> remainD2TRArr;

    for (BPQEntryPtr entry : subInstBPQArray) {
        baseAddrTRArr.push_back(entry->tileArg.baseAddrTR0);
        baseAddrDstTRArr.push_back(entry->tileArg.baseAddrDstTR);
        remainD2TRArr.push_back(entry->tileArg.d2TR);
    }

    double srcDataSize = GetDataSize(subInstBPQArray[0]->blockCmd->dataType);
    std::vector<BPQEntryPtr> d2HeadEntryArr;
    for (uint32_t i = 0; i < splitBPQNum; ++i) {
        BPQEntryPtr d1HeadEntry = nullptr;
        for (uint32_t j = 0; j < subInstBPQArray.size(); ++j) {
            BPQEntryPtr subInstBPQ = subInstBPQArray[j];
            BPQEntryPtr entry = std::make_shared<BPQEntry>(*subInstBPQ);
            if (j == 0) {
                d1HeadEntry = entry;
                entry->bdbRelatedBPQNum = subInstBPQArray.size() - 1;
            } else {
                entry->rowLeadBPQPtr = d1HeadEntry;
            }
            if (i == 0) {
                entry->attr = (j == 0) ? BPQAttr::SUB_INST_LEAD : BPQAttr::SUB_INST;
                entry->activeInstBPQNum = (j == 0) ? subInstBPQ->activeInstBPQNum : 0;
                entry->activeRowBPQNum = splitBPQNum - 1;
                d2HeadEntryArr.push_back(entry);
            } else {
                entry->attr = (j == 0) ? BPQAttr::SUB_ROW_LEAD : BPQAttr::SUB_ROW;
                entry->activeInstBPQNum = 0;
                entry->activeRowBPQNum = 0;
                entry->instLeadBPQPtr = d2HeadEntryArr[j];
            }
            entry->tileArg.baseAddrTR0 = baseAddrTRArr[j];
            entry->tileArg.baseAddrDstTR = baseAddrDstTRArr[j];
            entry->tileArg.d2TR = std::min(splitD2TR, remainD2TRArr[j]);
            baseAddrTRArr[j] += static_cast<uint64_t>(entry->tileArg.d2TR * entry->tileArg.d1TR * srcDataSize);
            if (entry->tileArg.layout == LayOut::ND2NZ || entry->tileArg.layout == LayOut::DN2ZN) {
                baseAddrDstTRArr[j] += entry->tileArg.d2TR * m_config.FRACTAL_ROW_BYTES;
            } else if (entry->tileArg.layout == LayOut::ND2ZN || entry->tileArg.layout == LayOut::DN2NZ ||
                       entry->tileArg.layout == LayOut::NORM) {
                baseAddrDstTRArr[j] += static_cast<uint64_t>(entry->tileArg.d2TR * entry->tileArg.d1TR * srcDataSize);
            } else {
                ASSERT(false && "Unsupport layout");
            }
            remainD2TRArr[j] -= entry->tileArg.d2TR;

            m_splitedBPQList.emplace_back(entry);
        }
    }
}

void BridgePairQ::HandleTMovBPQ(BPQEntryPtr entry)
{
    double srcDataSize = GetDataSize(entry->tileArg.srcType);
    double dstDataSize = GetDataSize(entry->tileArg.dstType);
    uint64_t fractalSize = m_config.FRACTAL_ROW_BYTES * m_config.FRACTAL_ROW_NUM;

    ASSERT((entry->tileArg.baseAddrTR0 & TILE_ALIGN_MASK) == entry->tileArg.baseAddrTR0);
    double tileSize = entry->tileArg.d1TR * entry->tileArg.d2TR * dstDataSize;
    ASSERT(entry->tileArg.d2TR % m_config.FRACTAL_ROW_NUM == 0);
    double rowSize = entry->tileArg.d1TR * dstDataSize;
    ASSERT(IsDivisible(rowSize, m_config.FRACTAL_ROW_BYTES));

    SplitTRReadAddrs(entry->readReqs, entry->tileArg, srcDataSize, MAX_TILE_DATA_BYTE);

    if (entry->tileArg.layout == LayOut::ND2NZ || entry->tileArg.layout == LayOut::DN2ZN) {
        ASSERT(IsDivisible(tileSize, fractalSize));
        uint32_t ringTransD2Num = entry->tileArg.d2TR / m_config.FRACTAL_ROW_NUM * 2;
        uint32_t ringTransD1Num = static_cast<uint64_t>(entry->tileArg.d1TR * dstDataSize) / m_config.FRACTAL_ROW_BYTES;
        SplitTMOVGatherLayoutTRWriteAddrs(entry->writeReqs, entry->tileArg, ringTransD2Num, ringTransD1Num,
                                          srcDataSize, dstDataSize);
    } else if (entry->tileArg.layout == LayOut::ND2ZN || entry->tileArg.layout == LayOut::DN2NZ) {
        ASSERT(IsDivisible(tileSize, fractalSize));
        uint32_t ringTransD2Num = (entry->tileArg.d2TR * dstDataSize) / m_config.FRACTAL_ROW_BYTES;
        uint32_t ringTransD1Num = entry->tileArg.d1TR  / m_config.FRACTAL_ROW_NUM * 2;
        SplitTMOVScatterLayoutTRWriteAddrs(entry->writeReqs, entry->tileArg, ringTransD2Num, ringTransD1Num,
                                           srcDataSize, dstDataSize);
    } else if (entry->tileArg.layout == LayOut::NORM) {
        SplitTMOVNormalLayoutTRWriteAddrs(entry->writeReqs, entry->tileArg, srcDataSize, dstDataSize);
    } else {
        LOG_ERROR_M(Unit::TMA, Stage::NA) << "[Bridge Table]: Unsupport layout.";
        abort();
    }

    m_pmuData.tcNum++; // this cnt is shared by tmov
    m_pmuData.tcReadNum += entry->readReqs.size();
    m_pmuData.tcReadDataSize += (entry->readReqs.size() * MAX_TILE_DATA_BYTE);
    m_pmuData.tcWriteNum += entry->writeReqs.size();

    ASSERT(entry->readReqs.size() <= m_config.BRIDGE_DATA_BUFFER_SIZE);
    entry->pairStatus = ExecStatus::NO_START;
    entry->startCycle = GetSim()->getCycles();
    entry->bpqID = m_bpqIDPool.top();
    m_bpq.emplace_back(entry);
    m_bpqIDPool.pop();
}

bool BridgePairQ::ReceiveTMovReq()
{
    if (GetSim()->core->configs.ruminateEnable && id < GetSim()->core->configs.tauStartId) {
        return false;
    }

    if (RemainBPQSize(false) <= 0) {
        return false;
    }

    if (bccTAUBlockCmdQ->Empty()) {
        return false;
    }

    BlockCommandPtr blockCmd = bccTAUBlockCmdQ->Front();
    if (blockCmd->tileOp != TileOp::TMOV) {
        return false;
    }
    bccTAUBlockCmdQ->Pop();
    LOG_INFO_M(Unit::TMA, Stage::NA) << "[Bridge Table" << id << "]: receive Tmov req:" << blockCmd->Dump();
    ASSERT(!m_tidPool.empty());
    blockCmd->execMachineId = machineId + m_tidPool.front();
    m_tidPool.pop_front();

    double srcSize = blockCmd->lb0 * blockCmd->lb1 * GetDataSize(blockCmd->dataType);
    ASSERT(srcSize <= blockCmd->srcs[0]->size);
    ASSERT(srcSize <= blockCmd->dsts[0]->size);

    std::vector<BPQEntryPtr> subInstBPQArray;
    BPQEntryPtr entry = std::make_shared<BPQEntry>();
    entry->bid = blockCmd->bid;
    entry->direction = Direction::TR2TR;
    entry->tileArg.InitByMemReqBus(blockCmd);
    entry->blockCmd = blockCmd;

    GetSim()->GetVerifyManager(entry->tileArg.stid)->InitPARGroup(entry->bid.val, 0);
    GetSim()->GetViewManager(entry->tileArg.stid)->InitBIQType(entry->bid.val, BIQType::TMA_IQ);
    GetSim()->GetViewManager(entry->tileArg.stid)->InitPARGroup(entry->bid.val, 0);
    subInstBPQArray.push_back(entry);

    SplitSubRowBPQ(blockCmd, subInstBPQArray);

    return true;
}

static uint32_t SetTileReqId(bool readFromBPQ, bool readFromFake, uint32_t id)
{
    /*
     * +---------------+------+----+
     * | Read From BPQ | Fake | id |
     * +---------------+------+----+
     *      1            1      30
     */
    constexpr uint32_t idOffset = 30;
    ASSERT((id & ((1ULL << idOffset) - 1)) == id);
    return (static_cast<uint32_t>(readFromBPQ) << (idOffset + 1)) | (static_cast<uint32_t>(readFromFake) << idOffset) |
           id;
}

static void UnpackTileReqId(uint32_t reqId, bool& readFromBPQ, bool &readFromFake, uint32_t& id)
{
    constexpr uint32_t idOffset = 30;
    readFromBPQ = (((reqId >> (idOffset + 1)) & 1) != 0U);
    readFromFake = (((reqId >> (idOffset)) & 1) != 0U);
    id = reqId & ((1ULL << idOffset) - 1);
}

TTransTileRegLdReq BridgePairQ::CreateTileLdReq(uint32_t reqId, uint64_t addr, uint64_t size) const
{
    TTransTileRegLdReq req = TTransTileRegLdReq();
    // 调整地址, 按照 Tile 的地址粒度来调整
    req.SetReqId(reqId);
    req.SetSrc(addr);
    req.SetSize(size);

    return req;
}

void BridgePairQ::SendReadTileReq(BPQEntryPtr entry)
{
    if (pTileUtils->IsRingOrXbarMode(false) && !HasValidSpbReadEntry()) {
        return;
    }

    std::pair<uint64_t, uint32_t> kv;
    if (entry->sendSrcTileReadCnt < entry->readOffsetReqs.size()) {
        kv = entry->readOffsetReqs.at(entry->sendSrcTileReadCnt);
    } else {
        kv = entry->readDataReqs.at(entry->sendSrcTileReadCnt - entry->readOffsetReqs.size());
    }
    /* The tile address will be misaligned after the mgather split bpq. */
    uint64_t tileAddr = kv.first & TILE_ALIGN_MASK;

    uint64_t bpqIdBitNum = static_cast<uint64_t>(std::floor(std::log2(m_config.BPQ_SIZE)) + 1);
    uint64_t bpqIdMask = (1ULL << bpqIdBitNum) - 1;
    uint32_t reqId = SetTileReqId(true, false, (entry->sendSrcTileReadCnt << bpqIdBitNum) | (entry->bpqID & bpqIdMask));
    TTransTileRegLdReq req = CreateTileLdReq(reqId, tileAddr, MAX_TILE_DATA_BYTE);
    if (pTileUtils->IsRingOrXbarMode(false)) {
        shared_ptr<CrossStation> station = GetFreeReadStation();
        shared_ptr<Request> pkt = station->Rxreq(req.GetSrc(), 0, req.GetStid());
        pkt->stid = req.GetStid();
        pkt->SetSize(req.GetSize());
        pkt->SetBufId(req.GetReqId());
        pkt->SetPEType(MachineType::TMA);
        station->WriteSpb(pkt);
    } else {
        m_bpqTileRegLdReqQ->Write(req);
    }
    ++entry->sendSrcTileReadCnt;
    if (entry->sendSrcTileReadCnt == (entry->readOffsetReqs.size() + entry->readDataReqs.size())) {
        entry->pairStatus = ExecStatus::WAIT_ADDR_TILE;
    }
    LOG_DEBUG_M(Unit::TMA, Stage::BPQ) << "BridgePairQ::SendReadTileReq, tileAddr=0x" << std::hex << tileAddr
        << std::dec << ", readSize=" << kv.second << ", reqId = " << reqId << ", bpqID = " << entry->bpqID;
}

void BridgePairQ::ReceiveTileData()
{
    uint32_t reqId = 0;
    std::vector<uint8_t> rawData;
    bool haveResp = false;
    uint64_t srcTileAddr = 0;
    if (pTileUtils->IsRingOrXbarMode(false)) {
        for (uint32_t i = 0; i < stations.size(); i++) {
            if (stations[i]->HasNoResp(0)) {
                continue;
            }
            shared_ptr<Request> pkt = stations[i]->GetDataComReqFront(0);
            if (IsVscatterRingReq(pkt)) {
                continue;
            }
            srcTileAddr = pkt->GetAddr();
            uint32_t respId = pkt->GetBufId();
            bool readFromBPQ = false;
            bool readFromFake = false;
            UnpackTileReqId(respId, readFromBPQ, readFromFake, reqId);
            if (!readFromBPQ) {
                continue;
            }
            rawData = std::vector<uint8_t>(pkt->GetData(), pkt->GetData() + pkt->GetSize());
            stations[i]->PopDataComReq(0);
            LOG_INFO_M(Unit::TMA, Stage::BPQ) << "Recv tile data " << *pkt;
            haveResp = true;
            break;
        }
    } else {
        if (m_tileRegBPQLdResQ->Empty()) {
            return;
        }
        TileRegTTransLdRes&& response = m_tileRegBPQLdResQ->Front();
        srcTileAddr = response.GetAddr();
        uint32_t respId = response.GetRespId();
        bool readFromBPQ = false;
        bool readFromFake = false;
        UnpackTileReqId(respId, readFromBPQ, readFromFake, reqId);
        if (!readFromBPQ) {
            return;
        }
        haveResp = true;
        m_tileRegBPQLdResQ->Pop();
        rawData = std::vector<uint8_t>(response.GetData(), response.GetData() + MAX_TILE_DATA_BYTE);
    }

    if (!haveResp) {
        return;
    }

    uint64_t bpqIdBitNum = static_cast<uint64_t>(std::floor(std::log2(m_config.BPQ_SIZE)) + 1);
    uint64_t bpqIdMask = (1ULL << bpqIdBitNum) - 1;
    uint64_t tid = reqId >> bpqIdBitNum;
    uint64_t bpqId = (reqId & bpqIdMask);
    LOG_DEBUG_M(Unit::TMA, Stage::BPQ) << "BridgePairQ::ReceiveTileData : reqId = " << reqId << ", bpqID = "<< bpqId
        << ", tid = "<< tid;
    BPQEntryPtr entry = GetBpqPtr(bpqId);

    ASSERT(entry != nullptr);
    ASSERT(entry->pairStatus == ExecStatus::GET_ADDR_TILE || entry->pairStatus == ExecStatus::WAIT_ADDR_TILE);
    auto handleAddrTile = [this](BPQEntryPtr entry, uint64_t srcTileAddr, std::vector<uint8_t>& rawData, uint64_t tid) {
        uint32_t typeSize = GetDataSize(entry->tileArg.srcType);
        uint64_t readOffset = entry->readOffsetReqs[tid].first - srcTileAddr;
        uint32_t dataNum = (entry->readOffsetReqs[tid].second) / GetDataSize(entry->tileArg.srcType);
        uint64_t dataNumPerTile = MAX_TILE_DATA_BYTE / GetDataSize(entry->tileArg.srcType);
        std::unordered_set<uint64_t> readAddrSet;
        std::stringstream ss;
        ss << "ReceiveTileData: receive addr:[";
        for (ReadReq req : entry->readReqs) {
            readAddrSet.emplace(req.readAddr);
        }
        for (uint32_t i = 0; i < dataNum; ++i) {
            if (!entry->tileArg.predicateMask.at(dataNumPerTile * tid + i)) {
                continue;
            }
            /* Convert uint8_t* data to dest type safely, Offset from default uint16 format */
            uint64_t offset = 0;
            for (uint32_t typeIndex = 0; typeIndex < typeSize; ++typeIndex) {
                offset |= static_cast<uint64_t>(rawData.at(readOffset + i * typeSize + typeIndex)) << (typeIndex * 8);
            }
            uint64_t readAddr = entry->tileArg.baseAddrGM + offset;
            if (entry->tileArg.op == TileOp::VGATHER) { // Vgather的地址为offset地址
                readAddr = offset;
            }
            entry->dataAddrs.at(dataNumPerTile * tid + i) = readAddr;
            ss << "0x" << std::hex << readAddr << ", ";

            /* The data of an element may exist in two cache lines. */
            uint64_t dstDataSize = std::ceil(GetDataSize(entry->tileArg.dstType));
            uint64_t firstCacheLineAddr = readAddr & ~(m_config.GLOBAL_MEMORY_CACHELINE_SIZE - 1);
            uint64_t secondCacheLineAddr = (readAddr + dstDataSize - 1) & ~(m_config.GLOBAL_MEMORY_CACHELINE_SIZE - 1);
            if (firstCacheLineAddr != secondCacheLineAddr) {
                ss << "]" << std::endl;
                LOG_ERROR_M(Unit::TMA, Stage::BPQ) << ss.str();
                ASSERT(false && "Currently, cross-cacheline is not supported.");
            }

            ReadReq readReq;
            if (readAddrSet.count(firstCacheLineAddr) == 0) {
                readAddrSet.emplace(firstCacheLineAddr);
                readReq.readAddr = firstCacheLineAddr;
                readReq.readSize = m_config.GLOBAL_MEMORY_CACHELINE_SIZE;
                entry->readReqs.emplace_back(readReq);
                entry->readReqTagPipeViews.emplace_back(GetSim()->getCycles());
            }

            if (readAddrSet.count(secondCacheLineAddr) == 0) {
                readAddrSet.emplace(secondCacheLineAddr);
                readReq.readAddr = secondCacheLineAddr;
                readReq.readSize = m_config.GLOBAL_MEMORY_CACHELINE_SIZE;
                entry->readReqs.emplace_back(readReq);
                entry->readReqTagPipeViews.emplace_back(GetSim()->getCycles());
            }
        }
        ss << "]" << std::endl;
        LOG_DEBUG_M(Unit::TMA, Stage::BPQ) << ss.str();
    };

    if (tid < entry->readOffsetReqs.size()) {
        handleAddrTile(entry, srcTileAddr, rawData, tid);
    } else {
        tid -= entry->readOffsetReqs.size();
        uint32_t bdbNum = rawData.size() / m_config.BRIDGE_DATA_BUFFER_DATA_SIZE;
        for (uint32_t iter = 0; iter < bdbNum; ++iter) {
            uint64_t bdbId = m_tsBDB->AllocDataBuffer(srcTileAddr);
            ASSERT((bdbId != static_cast<uint64_t>(-1)) && "BDB has no free space!");
            entry->tsBDBIds.at(tid * bdbNum + iter) = bdbId;
            m_tsBDB->WriteData(bdbId, srcTileAddr, m_config.BRIDGE_DATA_BUFFER_DATA_SIZE, rawData.data());
        }
    }

    ++entry->receiveReadCnt;
    if (entry->receiveReadCnt == (entry->readOffsetReqs.size() + entry->readDataReqs.size())) {
        entry->readReqStatus.resize(entry->readReqs.size(), ReadReqStatus::NOSTART);
        entry->readReqSendCnt.resize(entry->readReqs.size(), 0);
        entry->readReqSentAttemped.resize(entry->readReqs.size(), false);
        entry->pairStatus = ExecStatus::SEND_READ;
        entry->genReadCycle = GetSim()->getCycles();
        IncOutstandingTileOpNum(entry->tileArg.op);
    }
}

void BridgePairQ::HandleGatherReadSrc()
{
    ReceiveTileData();
    for (auto entry : m_bpq) {
        if (entry->pairStatus == ExecStatus::GET_ADDR_TILE) {
            SendReadTileReq(entry);
            break;
        }
    }
}

void BridgePairQ::SetBPQEntryState(uint32_t bpqID)
{
    for (auto entry : m_bpq) {
        if (entry->bpqID == bpqID) {
            entry->pairStatus = ExecStatus::COMPLETE;
            cout << "BridgePairQ::SetBPQEntryState, COMPLETE, bpqID=" << bpqID << endl;
            return;
        }
    }
}

void BridgePairQ::StubVectorSendVgather() const
{
    if (m_vectorBridgeBPQReqQ->Full() || m_vectorBridgeBPQRspQ->Full()) {
        return;
    }
    const static uint32_t VEC_BRIDGE_PREF_TID = 0x100000;
    static uint32_t sendIndex = 1;
    VectorBridgeReq reqCmd{};
    reqCmd.cmdType = ReqCmdType::VSCATTER;

    reqCmd.tid = VEC_BRIDGE_PREF_TID + sendIndex;  // TransactionId
    reqCmd.groupSlotId = sendIndex;
    reqCmd.predicateLaneMask = 0x1; // 0x61EF0F

    // 设置目的Data是UINT32 / UINT16 类型 (默认DataType::UINT32)
    reqCmd.dstDataType = DataType::UINT32;

    // Tile register 中源寄存器Tile的基址
    reqCmd.baseAddrTR = 0x0000000 + (sendIndex << 8); // 0x1000000
    // Tile register 中目的寄存器Tile的基址
    reqCmd.baseAddrDstTR = 0x0000000 + (sendIndex << 8);
    m_vectorBridgeBPQReqQ->Write(reqCmd);
    sendIndex = (sendIndex + 1) % MAX_TILE_DATA_BYTE;
    std::cout << "StubVectorSendVgather Fake msg  tid= 0x" << std::hex << reqCmd.tid
              << ", ReqQSize="<< std::dec << m_vectorBridgeBPQReqQ->Size() << endl;
}

void BridgePairQ::HandleVScatterBPQ(BPQEntryPtr entry)
{
    double dataTileDataSize = GetDataSize(entry->tileArg.dstType);
    uint64_t dataTileSize = entry->tileArg.d1TR * entry->tileArg.d2TR * dataTileDataSize;
    uint64_t bdbNum = std::ceil(dataTileSize / m_config.BRIDGE_DATA_BUFFER_DATA_SIZE);
    entry->tsBDBIds.resize(bdbNum);
    entry->dataAddrs.resize(entry->tileArg.d1TR * entry->tileArg.d2TR);
    /* After the SUB_COL BPQ is created, all BPQ IDs can be obtained. */
    if (entry->attr == BPQAttr::SUB_COL_LEAD) {
        entry->pairStatus = ExecStatus::WAIT_D_A_VEC;
    } else if (entry->attr == BPQAttr::SUB_COL || entry->attr == BPQAttr::ORIGINAL) {
        entry->pairStatus = ExecStatus::SEND_DBID_RSP;
    }
    entry->startCycle = GetSim()->getCycles();
    entry->bpqID = m_bpqIDPool.top();
    m_bpq.emplace_back(entry);
    m_bpqIDPool.pop();
}

bool BridgePairQ::ReceiveVectorMsg()
{
    if (GetSim()->core->configs.ruminateEnable && id < GetSim()->core->configs.tauStartId) {
        return false;
    }

    if (RemainBPQSize(false) <= 0) {
        return false;
    }

    if (m_vectorBridgeBPQReqQ->Empty()) {
        return false;
    }

    for (auto entry : m_bpq) {
        if (entry->tileArg.op == TileOp::TLOAD) {
            ASSERT(false && "The memory mode cannot coexist with TLOAD.");
        }
    }

    const VectorBridgeReq &reqCmd = m_vectorBridgeBPQReqQ->Read();
    BPQEntryPtr entry = std::make_shared<BPQEntry>();
    LOG_INFO_M(Unit::TMA, Stage::BPQ) << "[Bridge Table" << id << "]: receive vector req:"
        << MemCmdType2String(reqCmd.cmdType)
        << ", dstType:" << BriefDataType2String(reqCmd.dstDataType)
        << ", group slot id:" << reqCmd.groupSlotId << ", tid:" << reqCmd.tid
        << ", mask:0x" << std::hex << reqCmd.predicateLaneMask
        << ", addr:0x" << reqCmd.baseAddrTR << ", 0x" << reqCmd.baseAddrDstTR;
    // VGATHER: 地址固定为 UINT32 类型; VSCATTER: 地址类型从命令参数读取
    entry->tileArg.srcType = (reqCmd.cmdType == ReqCmdType::VGATHER) ? DataType::UINT32 : reqCmd.srcDataType;
    entry->tileArg.dstType = reqCmd.dstDataType;
    ASSERT(entry->tileArg.dstType != DataType::INVALID);

    entry->bid = reqCmd.bid;
    entry->direction = (reqCmd.cmdType == ReqCmdType::VGATHER) ? Direction::GM2TR : Direction::TR2GM;
    entry->tileArg.op = (reqCmd.cmdType == ReqCmdType::VGATHER) ? TileOp::VGATHER : TileOp::VSCATTER;
    entry->tileArg.d1TR = GetSim()->GetCore()->GetConfig().simtLane;
    entry->tileArg.originalD1TR = entry->tileArg.d1TR;
    entry->tileArg.d2TR = 1;
    if (entry->tileArg.op == TileOp::VGATHER) {
        ASSERT((entry->tileArg.baseAddrTR0 & TILE_ALIGN_MASK) == entry->tileArg.baseAddrTR0);
        ASSERT((entry->tileArg.baseAddrDstTR & TILE_ALIGN_MASK) == entry->tileArg.baseAddrDstTR);
        entry->tileArg.baseAddrTR0 = reqCmd.baseAddrTR;
        entry->tileArg.baseAddrDstTR = reqCmd.baseAddrTR;
    }

    entry->tileArg.predicateMask.resize(GetSim()->GetCore()->GetConfig().simtLane);
    for (uint64_t i = 0; i < sizeof(reqCmd.predicateLaneMask) * CHAR_BIT; ++i) {
        entry->tileArg.predicateMask.at(i) = (reqCmd.predicateLaneMask >> i) & 1;
    }
    entry->tileArg.groupSlotId = reqCmd.groupSlotId;
    entry->tileArg.tid = reqCmd.tid;
    entry->tileArg.stid = reqCmd.stid;
    ASSERT(entry->tileArg.stid != -1U);

    LOG_INFO_M(Unit::TMA, Stage::NA) << "[Bridge Table" << entry->bpqID << "]: receive vgather req!";

    if (entry->tileArg.op == TileOp::VGATHER) {
        SplitGatherBPQ(entry);
    } else if (entry->tileArg.op == TileOp::VSCATTER) {
        SplitScatterBPQ(entry);
    } else {
        ASSERT(false && "Unsupport tile op");
    }

    return true;
}

void BridgePairQ::SplitGatherBPQ(BPQEntryPtr entry)
{
    uint64_t dataNum = entry->tileArg.d1TR * entry->tileArg.d2TR;
    uint64_t splitGranularity = (DoubleEqual(GetDataSize(entry->tileArg.dstType), EIGHT_BYTE_DATA_WIDTH)) ?
        GetSim()->core->configs.simtLane / 2 : GetSim()->core->configs.simtLane;
    uint32_t splitBPQNum = (dataNum + splitGranularity - 1) / splitGranularity;
    double srcTileDataSize = GetDataSize(entry->tileArg.srcType);
    double dstTileDataSize = GetDataSize(entry->tileArg.dstType);
    BPQEntryPtr headPtr = nullptr;
    for (uint32_t i = 0; i < splitBPQNum; ++i) {
        BPQEntryPtr subEntry = std::make_shared<BPQEntry>(*entry);
        uint64_t validLaneNum = (i != splitBPQNum - 1) ? splitGranularity : (dataNum - i * splitGranularity);
        ASSERT(validLaneNum <= sizeof(uint64_t) * CHAR_BIT);

        if (i == 0) {
            headPtr = subEntry;
            subEntry->attr = BPQAttr::SUB_COL_LEAD;
            subEntry->activeInstBPQNum = splitBPQNum - 1;
        } else {
            subEntry->rowLeadBPQPtr = headPtr;
            subEntry->attr = BPQAttr::SUB_COL;
        }

        /* TODO: Current 64 Lane x FP16 result in 128B ring trans, cause ring BW underutilization */
        subEntry->tileArg.baseAddrTR0 += static_cast<uint64_t>(i * splitGranularity * srcTileDataSize);
        subEntry->tileArg.baseAddrDstTR += static_cast<uint64_t>(i * splitGranularity * dstTileDataSize);
        auto beginIt = entry->tileArg.predicateMask.cbegin() + (i * splitGranularity);
        auto endIt = beginIt + validLaneNum;
        subEntry->tileArg.predicateMask.assign(beginIt, endIt);
        subEntry->tileArg.predicateMask.resize(GetSim()->GetCore()->GetConfig().simtLane);
        subEntry->tileArg.d1TR = validLaneNum;
        subEntry->tileArg.d2TR = 1;
        m_splitedBPQList.emplace_back(subEntry);
    }
}

void BridgePairQ::HandleGatherBPQ(BPQEntryPtr entry)
{
    double srcTileDataSize = GetDataSize(entry->tileArg.srcType);
    uint64_t srcTileSize = entry->tileArg.d1TR * entry->tileArg.d2TR * srcTileDataSize;
    uint64_t readAddr = entry->tileArg.baseAddrTR0;
    uint64_t readEndAddr = readAddr + srcTileSize;
    while (readAddr < readEndAddr) {
        uint32_t readSize = (readEndAddr - readAddr < MAX_TILE_DATA_BYTE) ? readEndAddr - readAddr : MAX_TILE_DATA_BYTE;
        entry->readOffsetReqs.push_back({readAddr, readSize});
        readAddr += readSize;
    }
    entry->dataAddrs.resize(entry->tileArg.d1TR * entry->tileArg.d2TR);

    double dstTileDataSize = GetDataSize(entry->tileArg.dstType);
    uint64_t dstTileSize = entry->tileArg.d1TR * entry->tileArg.d2TR * dstTileDataSize;
    uint64_t writeAddr = entry->tileArg.baseAddrDstTR;
    uint64_t writeEndAddr = writeAddr + dstTileSize;
    auto beginIt = entry->tileArg.predicateMask.cbegin();
    while (writeAddr < writeEndAddr) {
        uint64_t writeAlignAddr = writeAddr & TILE_ALIGN_MASK;
        uint64_t nextCacheLineAddr = (writeAddr + MAX_TILE_DATA_BYTE) & TILE_ALIGN_MASK;
        uint32_t writeSize = nextCacheLineAddr - writeAddr;
        if (writeSize > writeEndAddr - writeAddr) {
            writeSize = writeEndAddr - writeAddr;
        }
        uint32_t dataNum = writeSize / dstTileDataSize;
        ASSERT(IsDivisible(writeSize, dstTileDataSize));
        WriteReq req;
        req.writeAddr = writeAlignAddr;
        req.writeSize = MAX_TILE_DATA_BYTE;
        auto endIt = beginIt + dataNum;
        ASSERT(endIt <= entry->tileArg.predicateMask.end());
        auto writeMaskIt = req.writeMask.begin() + (writeAddr - writeAlignAddr);
        for (auto it = beginIt; it != endIt; ++it) {
            std::fill(writeMaskIt, writeMaskIt + dstTileDataSize, *it);
            writeMaskIt += dstTileDataSize;
        }
        req.validWriteSize = std::count(req.writeMask.begin(), req.writeMask.end(), true);
        req.dataSize = dstTileDataSize;
        entry->writeReqs.push_back(req);
        writeAddr += writeSize;
        beginIt += dataNum;
    }
    /* Currently, only one dstTile of the BPQ needs to be read. */
    ASSERT(entry->writeReqs.size() == 1);

    if (entry->tileArg.op == TileOp::MGATHER) {
        m_pmuData.mgatherNum++;
    } else if (entry->tileArg.op == TileOp::VGATHER) {
        m_pmuData.vgatherNum++;
    } else {
        ASSERT(false && "Unsupport tile op");
    }

    entry->pairStatus = ExecStatus::GET_ADDR_TILE;
    entry->startCycle = GetSim()->getCycles();
    entry->bpqID = m_bpqIDPool.top();
    m_bpq.emplace_back(entry);
    m_bpqIDPool.pop();
}

bool BridgePairQ::ReceiveMGatherReq()
{
    if (GetSim()->core->configs.ruminateEnable && id < GetSim()->core->configs.tauStartId) {
        return false;
    }

    if (RemainBPQSize(false) <= 0) {
        return false;
    }

    if (bccTAUBlockCmdQ->Empty()) {
        return false;
    }

    BlockCommandPtr blockCmd = bccTAUBlockCmdQ->Front();
    if (blockCmd->tileOp != TileOp::MGATHER) {
        return false;
    }

    for (auto entry : m_bpq) {
        if (entry->tileArg.op == TileOp::TLOAD) {
            ASSERT(false && "The memory mode cannot coexist with TLOAD.");
        }
    }

    bccTAUBlockCmdQ->Pop();
    LOG_INFO_M(Unit::TMA, Stage::NA) << "[Bridge Table" << id << "]: receive Mgather req:" << blockCmd->Dump();

    ASSERT(!m_tidPool.empty());
    blockCmd->execMachineId = machineId + m_tidPool.front();
    m_tidPool.pop_front();

    BPQEntryPtr entry = std::make_shared<BPQEntry>();
    entry->bid = blockCmd->bid;
    entry->direction = Direction::GM2TR;
    entry->tileArg.InitByMemReqBus(blockCmd);
    entry->tileArg.predicateMask = std::vector<bool>(entry->tileArg.d1TR * entry->tileArg.d2TR, true);
    LOG_INFO_M(Unit::TMA, Stage::NA) << "BlockCmd mgather srcType: " << BriefDataType2String(entry->tileArg.srcType)
                                     << ",  dstType: " << BriefDataType2String(entry->tileArg.dstType);
    GetSim()->GetVerifyManager(entry->tileArg.stid)->InitPARGroup(entry->bid.val, 0);
    GetSim()->GetViewManager(entry->tileArg.stid)->InitBIQType(entry->bid.val, BIQType::TMA_IQ);
    GetSim()->GetViewManager(entry->tileArg.stid)->InitPARGroup(entry->bid.val, 0);
    entry->blockCmd = blockCmd;

    ASSERT((entry->tileArg.baseAddrTR0 & TILE_ALIGN_MASK) == entry->tileArg.baseAddrTR0);

    SplitGatherBPQ(entry);

    return true;
}

void BridgePairQ::SplitScatterBPQ(BPQEntryPtr entry)
{
    uint64_t dataNum = entry->tileArg.d1TR * entry->tileArg.d2TR;
    uint64_t splitGranularity = (DoubleEqual(GetDataSize(entry->tileArg.dstType), EIGHT_BYTE_DATA_WIDTH)) ?
        GetSim()->core->configs.simtLane / 2 : GetSim()->core->configs.simtLane;
    uint32_t splitBPQNum = (dataNum + splitGranularity - 1) / splitGranularity;
    double offsetTileDataSize = GetDataSize(entry->tileArg.srcType);
    double dataTileDataSize = GetDataSize(entry->tileArg.dstType);
    BPQEntryPtr headPtr = nullptr;
    for (uint32_t i = 0; i < splitBPQNum; ++i) {
        BPQEntryPtr subEntry = std::make_shared<BPQEntry>(*entry);
        uint64_t validLaneNum = (i != splitBPQNum - 1) ? splitGranularity : (dataNum - i * splitGranularity);
        ASSERT(validLaneNum <= sizeof(uint64_t) * CHAR_BIT);

        if (i == 0) {
            headPtr = subEntry;
            subEntry->attr = (splitBPQNum == 1) ? BPQAttr::ORIGINAL : BPQAttr::SUB_COL_LEAD;
            subEntry->activeInstBPQNum = splitBPQNum - 1;
        } else {
            subEntry->rowLeadBPQPtr = headPtr;
            subEntry->attr = BPQAttr::SUB_COL;
        }

        /* TODO: Current 64 Lane x FP16 result in 128B ring trans, cause ring BW underutilization */
        if (entry->tileArg.op == TileOp::MSCATTER) {
            subEntry->tileArg.baseAddrTR1 += static_cast<uint64_t>(i * splitGranularity * offsetTileDataSize);
            subEntry->tileArg.baseAddrTR0 += static_cast<uint64_t>(i * splitGranularity * dataTileDataSize);
        }
        auto beginIt = entry->tileArg.predicateMask.cbegin() + (i * splitGranularity);
        auto endIt = beginIt + validLaneNum;
        subEntry->tileArg.predicateMask.assign(beginIt, endIt);
        subEntry->tileArg.predicateMask.resize(GetSim()->GetCore()->GetConfig().simtLane);
        subEntry->tileArg.d1TR = validLaneNum;
        subEntry->tileArg.d2TR = 1;
        m_splitedBPQList.emplace_back(subEntry);
    }
}

void BridgePairQ::HandleMScatterBPQ(BPQEntryPtr entry)
{
    double offsetTileDataSize = GetDataSize(entry->tileArg.srcType);
    uint64_t offsetTileSize = entry->tileArg.d1TR * entry->tileArg.d2TR * offsetTileDataSize;
    uint64_t readAddr = entry->tileArg.baseAddrTR1;
    uint64_t readEndAddr = readAddr + offsetTileSize;
    while (readAddr < readEndAddr) {
        uint32_t readSize = (readEndAddr - readAddr < MAX_TILE_DATA_BYTE) ? readEndAddr - readAddr : MAX_TILE_DATA_BYTE;
        entry->readOffsetReqs.push_back({readAddr, readSize});
        readAddr += readSize;
    }
    /* Currently, only one offset tile of the BPQ needs to be read. */
    ASSERT(entry->readOffsetReqs.size() == 1);
    entry->dataAddrs.resize(entry->tileArg.d1TR * entry->tileArg.d2TR);

    double dataTileDataSize = GetDataSize(entry->tileArg.dstType);
    uint64_t dataTileSize = entry->tileArg.d1TR * entry->tileArg.d2TR * dataTileDataSize;
    readAddr = entry->tileArg.baseAddrDstTR;
    readEndAddr = readAddr + dataTileSize;
    while (readAddr < readEndAddr) {
        uint32_t readSize = (readEndAddr - readAddr < MAX_TILE_DATA_BYTE) ? readEndAddr - readAddr : MAX_TILE_DATA_BYTE;
        entry->readDataReqs.push_back({readAddr, readSize});
        readAddr += readSize;
    }

    uint64_t bdbNum = std::ceil(dataTileSize / m_config.BRIDGE_DATA_BUFFER_DATA_SIZE);
    entry->tsBDBIds.resize(bdbNum);

    entry->pairStatus = ExecStatus::GET_ADDR_TILE;
    entry->startCycle = GetSim()->getCycles();
    entry->bpqID = m_bpqIDPool.top();
    m_bpq.emplace_back(entry);
    m_bpqIDPool.pop();
}

bool BridgePairQ::ReceiveMScatterReq()
{
    if (GetSim()->core->configs.ruminateEnable && id < GetSim()->core->configs.tauStartId) {
        return false;
    }

    if (RemainBPQSize(false) <= 0) {
        return false;
    }

    if (bccTAUBlockCmdQ->Empty()) {
        return false;
    }

    for (auto entry : m_bpq) {
        if (entry->tileArg.op == TileOp::TLOAD) {
            ASSERT(false && "The memory mode cannot coexist with TLOAD.");
        }
    }

    BlockCommandPtr blockCmd = bccTAUBlockCmdQ->Front();
    if (blockCmd->tileOp != TileOp::MSCATTER) {
        return false;
    }
    bccTAUBlockCmdQ->Pop();
    LOG_INFO_M(Unit::TMA, Stage::NA) << "[Bridge Table" << id << "]: receive Mscatter req:" << blockCmd->Dump();

    ASSERT(!m_tidPool.empty());
    blockCmd->execMachineId = machineId + m_tidPool.front();
    m_tidPool.pop_front();

    BPQEntryPtr entry = std::make_shared<BPQEntry>();
    entry->bid = blockCmd->bid;
    entry->direction = Direction::TR2GM;
    entry->tileArg.InitByMemReqBus(blockCmd);
    entry->tileArg.predicateMask = std::vector<bool>(entry->tileArg.d1TR * entry->tileArg.d2TR, true);
    LOG_INFO_M(Unit::TMA, Stage::NA) << "BlockCmd mscatter srcType: " << BriefDataType2String(entry->tileArg.srcType)
                                     << ",  dstType: " << BriefDataType2String(entry->tileArg.dstType);
    GetSim()->GetVerifyManager(entry->tileArg.stid)->InitPARGroup(entry->bid.val, 0);
    GetSim()->GetViewManager(entry->tileArg.stid)->InitBIQType(entry->bid.val, BIQType::TMA_IQ);
    GetSim()->GetViewManager(entry->tileArg.stid)->InitPARGroup(entry->bid.val, 0);
    entry->blockCmd = blockCmd;

    ASSERT((entry->tileArg.baseAddrTR0 & TILE_ALIGN_MASK) == entry->tileArg.baseAddrTR0);
    ASSERT((entry->tileArg.baseAddrTR1 & TILE_ALIGN_MASK) == entry->tileArg.baseAddrTR1);

    SplitScatterBPQ(entry);

    return true;
}

void BridgePairQ::ReceiveMemReq()
{
    if (m_bpq.size() != m_config.BPQ_SIZE) {
        m_lsuBridgeTloadQ->unsetStall();
        m_lsuBridgeTstoreQ->unsetStall();
        bccTAUBlockCmdQ->unsetStall();
    } else {
        return;
    }

    if (m_tidPool.empty()) {
        return;
    }

    bool receiveTload = false;
    bool hasTload = (m_tloadCnt > 0);
    bool hasMemReq = (m_mgatherCnt + m_mscatterCnt + m_vgatherCnt + m_vscatterCnt) > 0;
    for (auto entry: m_splitedBPQList) {
        if (entry->tileArg.op == TileOp::TLOAD) {
            hasTload = true;
        } else if (IsMemoryModeReq(entry->tileArg.op)) {
            hasMemReq = true;
        }
    }
    ASSERT(!(hasTload && hasMemReq));
    bool enableRuminate = GetSim()->core->configs.ruminateEnable;
    bool isTAU = id >= GetSim()->core->configs.tauStartId;

    /* Collect eligible candidates from each source queue */
    std::vector<MemReqCandidate> candidates;

    bool hasTloadInQ = false;
    ROBID tloadBidInQ;
    if (!m_lsuBridgeTloadQ->Empty() && RemainBPQSize(false) > 0) {
        if (!enableRuminate || (enableRuminate && !isTAU)) {
            hasTloadInQ = true;
            tloadBidInQ = m_lsuBridgeTloadQ->Front().bid;
        }
    }

    if (!m_lsuBridgeTstoreQ->Empty() && RemainBPQSize(true) > 0) {
        if (!enableRuminate || (enableRuminate && !isTAU)) {
            candidates.push_back({MemReqSrc::TSTORE, m_lsuBridgeTstoreQ->Front().bid});
        }
    }

    bool hasMemModeReqInQ = false;
    MemReqSrc memModeReqSrc = MemReqSrc::MGATHER;
    ROBID memoryModeReqBidInQ;
    if (!bccTAUBlockCmdQ->Empty() && RemainBPQSize(false) > 0) {
        if (!enableRuminate || (enableRuminate && isTAU)) {
            BlockCommandPtr frontCmd = bccTAUBlockCmdQ->Front();
            if (frontCmd->tileOp == TileOp::TMOV) {
                candidates.push_back({MemReqSrc::TMOV, frontCmd->bid});
            } else if (frontCmd->tileOp == TileOp::MGATHER) {
                hasMemModeReqInQ = true;
                memModeReqSrc = MemReqSrc::MGATHER;
                memoryModeReqBidInQ = frontCmd->bid;
            } else if (frontCmd->tileOp == TileOp::MSCATTER) {
                hasMemModeReqInQ = true;
                memModeReqSrc = MemReqSrc::MSCATTER;
                memoryModeReqBidInQ = frontCmd->bid;
            }
        }
    }

    if (!m_vectorBridgeBPQReqQ->Empty() && RemainBPQSize(false) > 0) {
        ASSERT(!hasTload);
        candidates.push_back({MemReqSrc::VECMSG, m_vectorBridgeBPQReqQ->Front().bid});
    }

    if (hasTloadInQ && hasMemModeReqInQ && !hasTload && hasMemReq) {
        if (LessEqual(memoryModeReqBidInQ, tloadBidInQ)) {
            candidates.push_back({memModeReqSrc, memoryModeReqBidInQ});
        }
    } else if (hasTloadInQ && hasMemModeReqInQ && hasTload && !hasMemReq) {
        if (LessEqual(tloadBidInQ, memoryModeReqBidInQ)) {
            candidates.push_back({MemReqSrc::TLOAD, tloadBidInQ});
        }
    } else if (hasTloadInQ && hasMemModeReqInQ && !hasTload && !hasMemReq) {
        candidates.push_back({MemReqSrc::TLOAD, tloadBidInQ});
        candidates.push_back({memModeReqSrc, memoryModeReqBidInQ});
    } else if (hasTloadInQ && !hasMemModeReqInQ && !hasMemReq) {
        candidates.push_back({MemReqSrc::TLOAD, tloadBidInQ});
    } else if (!hasTloadInQ && hasMemModeReqInQ && !hasTload) {
        candidates.push_back({memModeReqSrc, memoryModeReqBidInQ});
    }

    if (candidates.empty()) {
        if (RemainBPQSize(false) < 0) {
            m_lsuBridgeTloadQ->setStall();
            bccTAUBlockCmdQ->setStall();
        }
        if (RemainBPQSize(true) < 0) {
            m_lsuBridgeTstoreQ->setStall();
        }
        return;
    }

    /* Select the candidate with the oldest (smallest) bid */
    size_t bestIdx = 0;
    for (size_t i = 1; i < candidates.size(); ++i) {
        if (LessEqual(candidates[i].bid, candidates[bestIdx].bid)) {
            bestIdx = i;
        }
    }

    /* Dispatch to the winning Receive* function */
    switch (candidates[bestIdx].src) {
        case MemReqSrc::TLOAD:
            ASSERT(ReceiveTLoadReq());
            receiveTload = true;
            break;
        case MemReqSrc::TSTORE:
            ASSERT(ReceiveTStoreReq());
            break;
        case MemReqSrc::TMOV:
            ASSERT(ReceiveTMovReq());
            break;
        case MemReqSrc::MGATHER:
            ASSERT(ReceiveMGatherReq());
            break;
        case MemReqSrc::MSCATTER:
            ASSERT(ReceiveMScatterReq());
            break;
        case MemReqSrc::VECMSG:
            ASSERT(ReceiveVectorMsg());
            break;
        default:
            LOG_ERROR_M(Unit::TMA, Stage::BPQ) << "Unsupport req:" << static_cast<int>(candidates[bestIdx].src);
            ASSERT(false);
            break;
    }

    /*
     * FIXME: the boundary conditions of the memory mode and non-memory mode will be considered to
     * properly clear the BDB.
     */
    if (!hasMemReq && !hasTload && receiveTload) {
        /* M/Vscatter and TSTORE will clear their own TS BDB entries after they are completed. */
        m_bdb->ClearBuffer();
        m_tag->Clear();
    }

    if (RemainBPQSize(false) < 0) {
        m_lsuBridgeTloadQ->setStall();
        bccTAUBlockCmdQ->setStall();
    }
    if (RemainBPQSize(true) < 0) {
        m_lsuBridgeTstoreQ->setStall();
    }
}

void BridgePairQ::ReceiveVscatterDataAddr()
{
    std::vector<uint8_t> rawData;
    uint64_t bpqID;
    uint64_t stqIndex;
    bool hasResp = false;
    /* TODO: using enum */
    uint8_t rawDataType;
    if (pTileUtils->IsRingOrXbarMode(false)) {
        for (uint32_t i = 0; i < stations.size(); i++) {
            if (stations[i]->HasNoResp()) {
                continue;
            }
            // Step 1: Peek数据（不弹出）
            shared_ptr<Request> pkt = stations[i]->GetDataComReqFront();
            if (!pkt) {
                continue;
            }

            // Step 2: 检查CHIFlit类型是否正确
            if (!IsVscatterRingReq(pkt)) {
                // 不是VSCATTER的tile数据，跳过
                continue;
            }

            // Step 3: 提取 CHIFlit 信息和数据
            CHIFlit recievePkt = pkt->GetFlit();
            bpqID = recievePkt.addr;      // BPQ entry ID
            stqIndex = recievePkt.txnId;  // STQ index
            rawDataType = recievePkt.dataID;
            rawData = std::vector<uint8_t>(pkt->GetData(), pkt->GetData() + pkt->GetSize());

            // Step 6: 所有检查通过，弹出并处理这个tile
            stations[i]->PopDataComReq();
            hasResp = true;
            LOG_INFO_M(Unit::TMA, Stage::BPQ) << "Recv tile data from VEC " << *pkt;

            break;
        }
    } else {
        LOG_INFO_M(Unit::TMA, Stage::BPQ) << "Receive Vscatte Data Addr CHIFlit on RING failed ";
        ASSERT(false && "Currently, direct connection is not supported.");
        return;
    }

    if (!hasResp) {
        return;
    }

    // Step 4: 直接查找 BPQ entry
    BPQEntryPtr entry = GetBpqPtr(bpqID);
    if (!entry) {
        LOG_INFO_M(Unit::TMA, Stage::BPQ)
            << "Received tile data but no matching BPQ entry found: "
            << "bpqID=" << bpqID
            << ", stqIndex=0x" << std::hex << stqIndex << std::dec
            << ". Skipping.";
        ASSERT(false && "Received tile data but no matching BPQ entry found");
    }

    // Step 5: 验证 entry 状态和 stqIndex
    if (entry->tileArg.tid != stqIndex) {
        LOG_DEBUG_M(Unit::TMA, Stage::BPQ)
            << "BPQ entry found but stqIndex mismatch: "
            << "expected stqIndex=0x" << std::hex << entry->tileArg.tid
            << ", got stqIndex=0x" << stqIndex << std::dec
            << ". Skipping.";
        ASSERT(false && "BPQ entry found but stqIndex mismatch");
    }

    // Step 7: 处理 tile 数据内容
    LOG_INFO_M(Unit::TMA, Stage::BPQ)
        << "[Bridge Table" << id << "]: receive vscatter data or addr tile! "
        << "srcType=" << BriefDataType2String(entry->tileArg.srcType)
        << " (" << std::dec << (GetDataSize(entry->tileArg.srcType) * 8) << "-bit), "
        << "dstType=" << BriefDataType2String(entry->tileArg.dstType)
        << " (" << (GetDataSize(entry->tileArg.dstType) * 8) << "-bit), "
        << ", bpq id:" << bpqID
        << ", tid:" << entry->tileArg.tid
        << ", group slot id:" << entry->tileArg.groupSlotId
        << ", data id:" << static_cast<uint64_t>(rawDataType);

    auto checkEntryStatus = [](const BPQEntryPtr entry) {
        if (entry->pairStatus != ExecStatus::WAIT_D_A_VEC) {
            LOG_ERROR_M(Unit::TMA, Stage::BPQ)
                << "VSCATTER entry found but status mismatch: "
                << "expected WAIT_D_A_VEC, got " << static_cast<int>(entry->pairStatus)
                << ". Skipping.";
            ASSERT(false && "BPQ entry found but status mismatch");
        }
    };

    auto handleAddrTile = [this, checkEntryStatus](BPQEntryPtr entry, std::vector<uint8_t>& rawData,
                                                   uint8_t rawDataType, std::vector<BPQEntryPtr>& updateEntrys) {
        /* 处理地址 tile：提取地址，生成 readReqs */
        auto generateReadReqs = [this](BPQEntryPtr entry, uint64_t dataNum, std::vector<uint8_t>& rawData,
                                       uint64_t rawDataStartIdx, uint32_t dataStartIdx) {
            uint64_t srcDataSize = std::ceil(GetDataSize(entry->tileArg.srcType));
            uint64_t dstDataSize = std::ceil(GetDataSize(entry->tileArg.dstType));
            uint64_t cacheLineMask = ~(m_config.GLOBAL_MEMORY_CACHELINE_SIZE - 1);
            std::unordered_set<uint64_t> readAddrSet;
            std::stringstream ss;
            ss << "ReceiveVscatterDataAddr: receive addr:[";
            for (ReadReq req : entry->readReqs) {
                readAddrSet.emplace(req.readAddr);
            }
            for (uint32_t i = 0; i < dataNum; ++i) {
                if (!entry->tileArg.predicateMask.at(dataStartIdx + i)) {
                    continue;
                }
                /* Convert uint8_t* data to dest type safely */
                uint64_t readAddr = 0;
                for (uint32_t typeIndex = 0; typeIndex < srcDataSize; ++typeIndex) {
                    readAddr |= static_cast<uint64_t>(rawData.at(rawDataStartIdx + i * srcDataSize + typeIndex)) <<
                                (typeIndex * 8);
                }
                entry->dataAddrs.at(dataStartIdx + i) = readAddr;
                ss << "0x" << std::hex << readAddr << ", ";

                /* The data of an element may exist in two cache lines. */
                uint64_t firstCacheLineAddr = readAddr & cacheLineMask;
                uint64_t secondCacheLineAddr = (readAddr + dstDataSize - 1) & cacheLineMask;
                if (firstCacheLineAddr != secondCacheLineAddr) {
                    ss << "]" << std::endl;
                    LOG_ERROR_M(Unit::TMA, Stage::BPQ) << ss.str();
                    ASSERT(false && "Currently, cross-cacheline is not supported.");
                }

                ReadReq readReq;
                if (readAddrSet.count(firstCacheLineAddr) == 0) {
                    readAddrSet.emplace(firstCacheLineAddr);
                    readReq.readAddr = firstCacheLineAddr;
                    readReq.readSize = m_config.GLOBAL_MEMORY_CACHELINE_SIZE;
                    entry->readReqs.emplace_back(readReq);
                    entry->readReqTagPipeViews.emplace_back(GetSim()->getCycles());
                }

                if (readAddrSet.count(secondCacheLineAddr) == 0) {
                    readAddrSet.emplace(secondCacheLineAddr);
                    readReq.readAddr = secondCacheLineAddr;
                    readReq.readSize = m_config.GLOBAL_MEMORY_CACHELINE_SIZE;
                    entry->readReqs.emplace_back(readReq);
                    entry->readReqTagPipeViews.emplace_back(GetSim()->getCycles());
                }
            }
            ss << "]" << std::endl;
            LOG_DEBUG_M(Unit::TMA, Stage::BPQ) << ss.str();
        };

        auto findSubEntry = [this](const BPQEntryPtr& entry) -> BPQEntryPtr {
            for (auto it = m_bpq.cbegin(); it != m_bpq.cend(); ++it) {
                const BPQEntryPtr &bpq = *it;
                if (bpq->rowLeadBPQPtr && bpq->rowLeadBPQPtr->bpqID == entry->bpqID) {
                    return bpq;
                }
            }
            return nullptr;
        };

        uint64_t srcDataSize = std::ceil(GetDataSize(entry->tileArg.srcType));
        uint32_t dataNum = rawData.size() / srcDataSize;
        if (std::isless(GetDataSize(entry->tileArg.dstType), EIGHT_BYTE_DATA_WIDTH)) {
            /* 每次接收完整的 Tile 数据 */
            checkEntryStatus(entry);
            updateEntrys = {entry};
            uint64_t dataNumPerTile = MAX_TILE_DATA_BYTE / srcDataSize;
            generateReadReqs(entry, dataNum, rawData, 0, dataNumPerTile * rawDataType);
            ++entry->receiveReadCnt;
        } else {
            if (std::isless(GetDataSize(entry->tileArg.srcType), EIGHT_BYTE_DATA_WIDTH)) {
                /* Tile 数据需要拆分成两份，分别给各自子 BPQ */
                BPQEntryPtr subEntry = findSubEntry(entry);
                ASSERT(subEntry);
                checkEntryStatus(entry);
                checkEntryStatus(subEntry);
                updateEntrys = {entry, subEntry};
                uint64_t rawDataStartIdx = 0;
                for (BPQEntryPtr curEntry : updateEntrys) {
                    uint64_t curEntryDataNum = curEntry->tileArg.d1TR * curEntry->tileArg.d2TR;
                    generateReadReqs(curEntry, curEntryDataNum, rawData, rawDataStartIdx, 0);
                    rawDataStartIdx += curEntryDataNum;
                    ++curEntry->receiveReadCnt;
                }
            } else {
                /* 将会传来两个 Tile，各自 BPQ 接收各自的 */
                BPQEntryPtr subEntry = findSubEntry(entry);
                ASSERT(subEntry);
                BPQEntryPtr curEntry = rawDataType == 0 ? entry : subEntry;
                checkEntryStatus(curEntry);
                updateEntrys = {curEntry};
                generateReadReqs(curEntry, dataNum, rawData, 0, 0);
                ++curEntry->receiveReadCnt;
            }
        }
    };

    std::vector<BPQEntryPtr> updateEntrys;
    /* TODO: Is it necessary to calculate the value based on d1TR and d2TR? */
    constexpr uint32_t dataTileStartDataID = 2;
    if (rawDataType < dataTileStartDataID) {
        handleAddrTile(entry, rawData, rawDataType, updateEntrys);
    } else {
        /* 处理数据 tile：写入 BDB */
        rawDataType -= dataTileStartDataID;
        if (entry->rowLeadBPQPtr) {
            /*
             * If the data is 64 bits, it will be split into two parts.
             * For the sub-BPQ, the value of rawDataType is 4.
             */
            rawDataType--;
        }
        checkEntryStatus(entry);
        updateEntrys = {entry};
        uint32_t bdbNum = rawData.size() / m_config.BRIDGE_DATA_BUFFER_DATA_SIZE;
        for (uint32_t i = 0; i < bdbNum; ++i) {
            uint64_t bdbId = m_tsBDB->AllocDataBuffer(-1);
            ASSERT((bdbId != static_cast<uint64_t>(-1)) && "BDB has no free space!");
            entry->tsBDBIds.at(rawDataType * bdbNum + i) = bdbId;
            m_tsBDB->WriteData(bdbId, -1, m_config.BRIDGE_DATA_BUFFER_DATA_SIZE, rawData.data());
        }
        ++entry->receiveReadCnt;
    }

    // Step 8: 更新计数和状态

    for (auto e : updateEntrys) {
        uint64_t totalDataNum = e->tileArg.d1TR * entry->tileArg.d2TR;
        uint64_t srcDataSize = std::ceil(GetDataSize(e->tileArg.srcType));
        uint64_t dstDataSize = std::ceil(GetDataSize(e->tileArg.dstType));
        uint64_t addrTileNum = totalDataNum * srcDataSize / MAX_TILE_DATA_BYTE;
        uint64_t dataTileNum = totalDataNum * dstDataSize / MAX_TILE_DATA_BYTE;
        uint64_t expectedTotalTiles = addrTileNum + dataTileNum;
        LOG_DEBUG_M(Unit::TMA, Stage::BPQ)
            << "VSCATTER tile received: receiveReadCnt=" << e->receiveReadCnt
            << ", expectedTotalTiles=" << expectedTotalTiles
            << " (srcType=" << BriefDataType2String(e->tileArg.srcType)
            << ", dstType=" << BriefDataType2String(e->tileArg.dstType) << ")";
        if (e->receiveReadCnt == expectedTotalTiles) {
            // 收到了所有 tile，跳转到 SEND_READ 状态
            e->pairStatus = ExecStatus::SEND_READ;
            e->genReadCycle = GetSim()->getCycles();
            e->readReqStatus.resize(e->readReqs.size(), ReadReqStatus::NOSTART);
            e->readReqSendCnt.resize(e->readReqs.size(), 0);
            e->readReqSentAttemped.resize(e->readReqs.size(), false);
            IncOutstandingTileOpNum(e->tileArg.op);
            LOG_DEBUG_M(Unit::TMA, Stage::BPQ) << "All tiles received, status -> SEND_READ";
        }
        if ((GetSim()->core->configs.debug_stub_vec_ring_enable ||
            GetSim()->core->configs.debug_stub_vscatter_plan3_enable) &&
            e->pairStatus == ExecStatus::SEND_READ) {
            e->pairStatus = ExecStatus::COMPLETE;
            LOG_INFO_M(Unit::TMA, Stage::BPQ) << "All tiles received, status -> COMPLETE";
        }
    }
}

void BridgePairQ::SendDBidResponse(BPQEntryPtr entry)
{
    if (pTileUtils->IsRingOrXbarMode(false) && !HasValidSpbReadEntry()) {
        return;
    }

    uint32_t vecReadStationId = pTileUtils->GetUBConfig()->vec_read_port[0];
    uint8_t sendFlag;
    if (pTileUtils->IsRingOrXbarMode(false)) {
        shared_ptr<CrossStation> station = GetFreeReadStation();
        // 修改：addr 字段使用 BPQ entry ID (bpqID)，txnId 使用 STQ index (tid)
        uint64_t bpqIDs = 0;
        if (entry->attr == BPQAttr::ORIGINAL) {
            bpqIDs = entry->bpqID;
        } else if (entry->attr == BPQAttr::SUB_COL) {
            bpqIDs = entry->rowLeadBPQPtr->bpqID & 0xFFFF;  // 低16位
            bpqIDs |= (entry->bpqID & 0xFFFF) << 16;        // 高16位
        } else {
            ASSERT(false && "SendDBidResponse, unsupport case");
        }
        shared_ptr<Request> pkt = station->RxvscatterReq(bpqIDs, entry->tileArg.tid, vecReadStationId);
        pkt->SetPEType(MachineType::TMA);

        // 方案3: 不发送到Ring
        if (GetSim()->core->configs.debug_stub_vscatter_plan3_enable) {
            LOG_INFO_M(Unit::TMA, Stage::NA)
                << "[Plan3] DBID Response CHIFlit created (not sent to Ring): "
                << "bpqID=" << bpqIDs
                << ", stqIndex=0x" << std::hex << entry->tileArg.tid << std::dec
                << ", groupSlotId=" << entry->tileArg.groupSlotId;
            sendFlag = 1;  // 标记成功，但不实际发送
        } else {
            // 正常流程：发送到Ring
            station->WriteSpb(pkt);
            sendFlag = 1;
            LOG_INFO_M(Unit::TMA, Stage::NA) << "Send DBid Response CHIFlit on ring: bpqID=" << bpqIDs
                << ", stqIndex=" << entry->tileArg.tid
                << ", groupSlotId=" << entry->tileArg.groupSlotId;
        }
    } else {
        sendFlag = 0;
        ASSERT(false && "Send DBid Response CHIFlit on RING failed");
    }
    if (sendFlag == 1) {
        entry->pairStatus = ExecStatus::WAIT_D_A_VEC;
    }
}

void BridgePairQ::HandleVscatterTileTrans()
{
    if (GetSim()->core->configs.debug_stub_vec_ring_enable) {
        // 方案A/B: VEC Ring stub - 使用正常流程（走Ring）
        // 1. 从MGB读取Tiles (从Ring接收)
        // 2. 处理SEND_DBID_RSP状态
        for (auto entry : m_bpq) {
            if (entry->pairStatus == ExecStatus::WAIT_D_A_VEC) { //////这里还能再优化
                ReceiveVscatterDataAddr();
                break;
            }
            if (entry->pairStatus == ExecStatus::SEND_DBID_RSP) {
                // 发送DBID Response到Ring
                SendDBidResponse(entry);
                break;  // 一次处理一个
            }
        }
    } else if (GetSim()->core->configs.debug_stub_vscatter_plan3_enable) {
        // 方案3: 保留CHIFlit打包/解包，内部短路
        // 1. 从MGB读取stub tiles并解包 (保留解包逻辑)
        // 2. 处理SEND_DBID_RSP状态
        for (auto entry : m_bpq) {
            if (entry->pairStatus == ExecStatus::WAIT_D_A_VEC) {
                ReceiveVscatterDataAddr();
                break;
            }
            if (entry->pairStatus == ExecStatus::SEND_DBID_RSP) {
                // 发送DBID Response (打包CHIFlit但不发送到Ring)
                SendDBidResponse(entry);

                // 立即生成stub tiles并注入MGB
                GenerateVscatterStubTilesPlan3(entry);

                break;  // 一次处理一个
            }
        }
    } else {
        // 正常流程
        ReceiveVscatterDataAddr();
        for (auto entry : m_bpq) {
            if (entry->pairStatus == ExecStatus::SEND_DBID_RSP) {
                SendDBidResponse(entry);
                break;
            }
        }
    }
}

// 方案3: VSCATTER stub - 保留CHIFlit打包/解包，内部短路
void BridgePairQ::GenerateVscatterStubTilesPlan3(BPQEntryPtr entry)
{
    // 检查配置开关
    if (!GetSim()->core->configs.debug_stub_vscatter_plan3_enable) {
        return;
    }

    // 检查entry状态
    if (entry->pairStatus != ExecStatus::WAIT_D_A_VEC) {
        return;
    }

    // 检查Ring模式
    if (!pTileUtils->IsRingOrXbarMode(false)) {
        LOG_INFO_M(Unit::TMA, Stage::NA) << "[Plan3] Not RingOrXbar mode, skip";
        return;
    }

    // 获取CrossStation
    shared_ptr<CrossStation> station = GetFreeReadStation();
    if (!station) {
        LOG_INFO_M(Unit::TMA, Stage::NA) << "[Plan3] No free station available";
        return;
    }

    // 先注入Addr Tile (did=1)
    InjectStubAddrTilePlan3(entry);

    // 再注入Data Tile (did=0)
    InjectStubDataTilePlan3(entry);

    LOG_INFO_M(Unit::TMA, Stage::NA)
        << "[Plan3] Stub tiles generated for groupSlotId=" << entry->tileArg.groupSlotId
        << ", tid=0x" << std::hex << entry->tileArg.tid << std::dec;
}

void BridgePairQ::InjectStubAddrTilePlan3(BPQEntryPtr entry)
{
    // 获取CrossStation
    shared_ptr<CrossStation> station = GetFreeReadStation();
    if (!station) {
        LOG_INFO_M(Unit::TMA, Stage::NA) << "[Plan3] No free station for Addr Tile";
        return;
    }

    uint32_t vecReadStationId = pTileUtils->GetUBConfig()->vec_read_port[0];
    int tmaNodeId = station->GetId();  // 使用CrossStation的ID作为TMA节点ID

    // 创建Addr Tile数据 (64个UINT32地址)
    alignas(8) uint8_t addrTileData[MAX_TILE_DATA_BYTE];
    memset(addrTileData, 0, MAX_TILE_DATA_BYTE);
    for (int lane = 0; lane < 64; lane++) {
        uint32_t value = 0x1000 + lane;
        std::memcpy(addrTileData + lane * sizeof(uint32_t), &value, sizeof(value));
    }

    // 使用CrossStation::RxvscatterData创建CHIFlit (保留打包逻辑)
    shared_ptr<Request> pkt = station->RxvscatterData(
        entry->tileArg.groupSlotId,  // addr
        entry->tileArg.tid,          // txnId
        1,                           // did = 1 (Addr Tile)
        vecReadStationId,            // srcNode: VectorUnit
        tmaNodeId,                   // tgtNode: TMA (自己)
        0);                          // coreId

    // 设置数据
    pkt->SetData(addrTileData);
    pkt->SetSize(64 * sizeof(uint32_t));
    pkt->SetPEType(MachineType::VECTOR);  // 标记来自VectorUnit

    // 注入到MGB (模拟从Ring接收)
    station->WriteMGB(pkt);

    LOG_INFO_M(Unit::TMA, Stage::NA)
        << "[Plan3] Injected stub Addr Tile: groupSlotId=" << entry->tileArg.groupSlotId
        << ", tid=0x" << std::hex << entry->tileArg.tid << std::dec
        << ", srcNode=" << vecReadStationId
        << ", tgtNode=" << tmaNodeId;
}

void BridgePairQ::InjectStubDataTilePlan3(BPQEntryPtr entry)
{
    // 获取CrossStation
    shared_ptr<CrossStation> station = GetFreeReadStation();
    if (!station) {
        LOG_INFO_M(Unit::TMA, Stage::NA) << "[Plan3] No free station for Data Tile";
        return;
    }

    uint32_t vecReadStationId = pTileUtils->GetUBConfig()->vec_read_port[0];
    int tmaNodeId = station->GetId();

    // 创建Data Tile数据 (64个UINT16数据)
    alignas(8) uint8_t dataTileData[MAX_TILE_DATA_BYTE];
    memset(dataTileData, 0, MAX_TILE_DATA_BYTE);
    for (int lane = 0; lane < 64; lane++) {
        dataTileData[lane] = 0x1234 + lane;  // 简单递增数据
    }

    // 使用CrossStation::RxvscatterData创建CHIFlit (保留打包逻辑)
    shared_ptr<Request> pkt = station->RxvscatterData(
        entry->tileArg.groupSlotId,
        entry->tileArg.tid,
        0,                           // did = 0 (Data Tile)
        vecReadStationId,
        tmaNodeId,
        0);

    // 设置数据
    pkt->SetData(dataTileData);
    pkt->SetSize(64 * sizeof(uint16_t));
    pkt->SetPEType(MachineType::VECTOR);

    // 注入到MGB
    station->WriteMGB(pkt);

    LOG_INFO_M(Unit::TMA, Stage::NA)
        << "[Plan3] Injected stub Data Tile: groupSlotId=" << entry->tileArg.groupSlotId
        << ", tid=0x" << std::hex << entry->tileArg.tid << std::dec
        << ", srcNode=" << vecReadStationId
        << ", tgtNode=" << tmaNodeId;
}

bool BridgePairQ::SendOneReadReq2RFB(BPQEntryPtr bpqEntry)
{
    ASSERT(!IsMemoryModeReq(bpqEntry->tileArg.op));
    if (bpqEntry->pairStatus != ExecStatus::SEND_READ) {
        return false;
    }

    ASSERT(bpqEntry->readReqs.size() != 0);
    RFBEntry rfbEntry;
    rfbEntry.addr = bpqEntry->readReqs[bpqEntry->sendReadCounter].readAddr;
    rfbEntry.size = bpqEntry->readReqs[bpqEntry->sendReadCounter].readSize;
    rfbEntry.bpqId = bpqEntry->bpqID;
    rfbEntry.bid = bpqEntry->bid;
    rfbEntry.stid = bpqEntry->tileArg.stid;
    rfbEntry.memDir = bpqEntry->direction;
    rfbEntry.op = bpqEntry->tileArg.op;

    std::string rfbType;
    if (bpqEntry->direction == Direction::GM2TR) {
        rfbType = "SRFB";
        m_bpqSRFBQ->Write(rfbEntry);
    } else if (bpqEntry->direction == Direction::TR2GM || bpqEntry->direction == Direction::TR2TR) {
        rfbType = "NRFB";
        m_bpqNRFBQ->Write(rfbEntry);
    } else {
        LOG_ERROR_M(Unit::TMA, Stage::BPQ) << "[Bridge Table]: Unsupport direction!";
        abort();
    }

    LOG_INFO_M(Unit::TMA, Stage::BPQ) << "[Bridge Table]: bid:" << std::dec << bpqEntry->bid.GetVal()
        << " Send Read Req to " << rfbType << " bpq id:" << std::dec << rfbEntry.bpqId
        << " addr:0x" << std::hex << rfbEntry.addr
        << " size:" << std::dec << rfbEntry.size << "B";

    bpqEntry->sendReadCounter++; // num of element = readreq Vector width
    if (!IsMemoryModeReq(bpqEntry->tileArg.op) && bpqEntry->sendReadCounter == bpqEntry->readReqs.size()) {
        bpqEntry->pairStatus = ExecStatus::WAIT_READ_DATA;
        bpqEntry->waitReadCycle = GetSim()->getCycles();
    }
    return true;
}

bool BridgePairQ::SendOneWriteReq2WCB(BPQEntryPtr bpqEntry)
{
    if (!IsMemoryModeReq(bpqEntry->tileArg.op) && bpqEntry->pairStatus != ExecStatus::SEND_WRITE) {
        return false;
    }

    if ((bpqEntry->tileArg.op == TileOp::MGATHER || bpqEntry->tileArg.op == TileOp::VGATHER) &&
        bpqEntry->writeReqs.size() == bpqEntry->sendWriteCounter) {
        return false;
    }

    ASSERT(bpqEntry->writeReqs.size() != 0);
    WriteReq& writeReq = bpqEntry->writeReqs[bpqEntry->sendWriteCounter];
    WCBEntry wcbEntry;
    wcbEntry.writeReq = writeReq;
    wcbEntry.bpqId = bpqEntry->bpqID;
    wcbEntry.bid = bpqEntry->bid;
    wcbEntry.layout = bpqEntry->tileArg.layout;
    wcbEntry.op = bpqEntry->tileArg.op;
    wcbEntry.stid = bpqEntry->tileArg.stid;

    std::string wcbType;
    if (bpqEntry->direction == Direction::TR2GM) {
        wcbType = "SWCB";
        AcquireSWCBAvailCnt();
        m_bpqSWCBQ->Write(wcbEntry);
    } else if (bpqEntry->direction == Direction::GM2TR || bpqEntry->direction == Direction::TR2TR) {
        wcbType = "NWCB";
        m_bpqNWCBQ->Write(wcbEntry);
    } else {
        LOG_ERROR_M(Unit::TMA, Stage::BPQ) << "[Bridge Table]: Unsupport direction!";
        abort();
    }
    LOG_INFO_M(Unit::TMA, Stage::BPQ) << "[Bridge Table]: bid:" << std::dec << bpqEntry->bid.GetVal()
        << " Send Write Req to " << wcbType << " bpq id:" << std::dec << wcbEntry.bpqId
        << " addr:0x" << std::hex << wcbEntry.writeReq.writeAddr;

    bpqEntry->sendWriteCounter++;
    if (!IsMemoryModeReq(bpqEntry->tileArg.op) && bpqEntry->sendWriteCounter == bpqEntry->writeReqs.size()) {
        bpqEntry->pairStatus = ExecStatus::WAIT_WRITE_RESOLVE;
        bpqEntry->waitWriteCycle = GetSim()->getCycles();
    }
    return true;
}

uint32_t BridgePairQ::GetRFBRemainSize(Direction direction)
{
    uint32_t canSendReadNum = 0;
    if (direction == Direction::GM2GM || direction == Direction::GM2TR) {
        uint32_t qSize = m_bpqSRFBQ->SizeW() + m_bpqSRFBQ->Size();
        ASSERT(m_srfb->GetInvalidEntryCounter() >= qSize);
        canSendReadNum = m_srfb->GetInvalidEntryCounter() - qSize;
    } else if (direction == Direction::TR2GM || direction == Direction::TR2TR) {
        uint32_t qSize = m_bpqNRFBQ->SizeW() + m_bpqNRFBQ->Size();
        ASSERT(m_nrfb->GetInvalidEntryCounter() >= qSize);
        canSendReadNum = m_nrfb->GetInvalidEntryCounter() - qSize;
    } else {
        LOG_ERROR_M(Unit::TMA, Stage::NA) << "[Bridge Table]: Unsupport direction!";
        abort();
    }
    return canSendReadNum;
}

uint32_t BridgePairQ::GetWCBRemainSize(Direction direction)
{
    uint32_t canSendWriteNum = 0;
    if (direction == Direction::GM2GM || direction == Direction::TR2GM) {
        canSendWriteNum = m_swcbAvailCnt;
    } else if (direction == Direction::GM2TR || direction == Direction::TR2TR) {
        uint32_t qSize = m_bpqNWCBQ->SizeW() + m_bpqNWCBQ->Size();
        ASSERT(m_nwcb->GetInvalidEntryCounter() >= qSize);
        canSendWriteNum = m_nwcb->GetInvalidEntryCounter() - qSize;
    } else {
        LOG_ERROR_M(Unit::TMA, Stage::NA) << "[Bridge Table]: Unsupport direction!";
        abort();
    }
    return canSendWriteNum;
}

uint32_t BridgePairQ::GetBDBRemainSize(Direction direction, uint32_t pendingBDBReqNum)
{
    uint32_t gmGranMultiplier = m_config.GLOBAL_MEMORY_CACHELINE_SIZE / m_config.BRIDGE_DATA_BUFFER_DATA_SIZE;
    uint32_t trGranMultiplier = MAX_TILE_DATA_BYTE / m_config.BRIDGE_DATA_BUFFER_DATA_SIZE;
    uint32_t granMultiplier = std::max(gmGranMultiplier, trGranMultiplier);

    std::shared_ptr<BridgeDataBuffer> bdb = nullptr;
    if (direction == Direction::GM2GM || direction == Direction::GM2TR) {
        bdb = m_bdb;
    } else if (direction == Direction::TR2GM || direction == Direction::TR2TR) {
        bdb = m_tsBDB;
    } else {
        ASSERT(false && "BridgePairQ::GetBDBRemainSize: illegal direction");
    }

    uint32_t bdbInvalidEntryCnt = bdb->GetInvalidEntryCounter();
    uint32_t reservedEntryCnt = pendingBDBReqNum * granMultiplier;
    uint32_t bdbCanUseNum = (bdbInvalidEntryCnt <= reservedEntryCnt) ? 0 : bdbInvalidEntryCnt - reservedEntryCnt;
    if (direction == Direction::GM2GM || direction == Direction::GM2TR) {
        bdbCanUseNum = bdbCanUseNum / gmGranMultiplier;
    } else if (direction == Direction::TR2GM || direction == Direction::TR2TR) {
        bdbCanUseNum = bdbCanUseNum / trGranMultiplier;
    } else {
        ASSERT(false && "BridgePairQ::GetBDBRemainSize: illegal direction");
    }

    return bdbCanUseNum;
}

void BridgePairQ::SendFackChannelReq()
{
    for (auto entry : m_bpq) {
        if ((entry->tileArg.op != TileOp::VGATHER) || (entry->pairStatus != ExecStatus::SEND_READ)) {
            continue;
        }
        FakeChannelEntry fakeChannelEntry{};
        fakeChannelEntry.op = TileOp::VGATHER;
        fakeChannelEntry.bpqId = entry->bpqID;
        for (uint64_t i = 0; i < entry->tileArg.predicateMask.size(); ++i) {
            if (entry->tileArg.predicateMask[i]) {
                fakeChannelEntry.laneMask |= (1 << i);
            }
        }
        fakeChannelEntry.baseAddrDstTR = entry->tileArg.baseAddrDstTR;
        fakeChannelEntry.writeReqs = entry->writeReqs;
        fakeChannelEntry.gatherDataSize = GetDataSize(entry->tileArg.dstType);
        fakeChannelEntry.status = FakeChannelStatus::SEND_READ;
        GatherDataEntry dataEntry{};
        dataEntry.gatherDataSize = fakeChannelEntry.gatherDataSize;
        // 写入后马上发送
        for (auto readReq : entry->dataAddrs) {
            dataEntry.originalAddr = readReq;
            fakeChannelEntry.dataEntry.push_back(dataEntry);
        }
        // todo: 是要保证1拍1个吗
        cout << "BridgePairQ::SendFackChannelReq , bpqId=" << entry->bpqID << ", entry->readReqs.size = "
            << entry->readReqs.size() <<endl;
        fakeChannelNode.bpqDataInfo[entry->bpqID] = fakeChannelEntry;
        entry->pairStatus = ExecStatus::WAIT_READ_DATA;
        break;
    }
}

void BridgePairQ::SendReadWriteReq()
{
    /* FIXME: How BPQ send re? */
    SendReadWriteReqMemoryMode();
    SendReadWriteReqNormal();
}

void BridgePairQ::SendReadWriteReqNormal()
{
    uint32_t maxSendReadNum = static_cast<uint32_t>(m_config.BPQ_SEND_READ_NUM);
    uint32_t maxSendWriteNum = static_cast<uint32_t>(m_config.BPQ_SEND_WRITE_NUM);
    uint32_t maxSendTloadReadNum = static_cast<uint32_t>(m_config.BPQ_SEND_TLOAD_READ_NUM);
    uint32_t maxSendTloadWriteNum = static_cast<uint32_t>(m_config.BPQ_SEND_TLOAD_WRITE_NUM);
    uint32_t maxSendTstoreReadNum = static_cast<uint32_t>(m_config.BPQ_SEND_TSTORE_READ_NUM);
    uint32_t maxSendTstoreWriteNum = static_cast<uint32_t>(m_config.BPQ_SEND_TSTORE_WRITE_NUM);
    ASSERT(m_sim->core != nullptr);
    for (auto entry : m_bpq) {
        if (IsMemoryModeReq(entry->tileArg.op)) {
            continue;
        }
        if (entry->pairStatus == ExecStatus::WAIT_OTHER_BPQ &&
            entry->rowLeadBPQPtr->pairStatus == ExecStatus::SEND_WRITE) {
            entry->pairStatus = ExecStatus::SEND_WRITE;
            entry->genWriteCycle = GetSim()->getCycles();
            if (entry->genReadCycle == 0) {
                IncOutstandingTileOpNum(entry->tileArg.op);
            }
        }

        uint32_t *canSendReadNum = &maxSendReadNum;
        uint32_t *canSendWriteNum = &maxSendWriteNum;
        if (m_config.BPQ_DUAL_CHANNEL) {
            if (entry->tileArg.op == TileOp::TLOAD) {
                canSendReadNum = &maxSendTloadReadNum;
                canSendWriteNum = &maxSendTloadWriteNum;
            } else {
                canSendReadNum = &maxSendTstoreReadNum;
                canSendWriteNum = &maxSendTstoreWriteNum;
            }
        }

        if ((entry->pairStatus == ExecStatus::NO_START || entry->pairStatus == ExecStatus::SEND_READ) &&
            *canSendReadNum > 0) {
            uint32_t pendingBDBReqNum = m_bpqSRFBQ->SizeW() + m_bpqSRFBQ->Size() +
                                        m_bpqNRFBQ->SizeW() + m_bpqNRFBQ->Size();
            uint32_t bdbCanUseNum = GetBDBRemainSize(entry->direction, pendingBDBReqNum);
            uint32_t currDirectCanSendReadNum = GetRFBRemainSize(entry->direction);
            currDirectCanSendReadNum = std::min(currDirectCanSendReadNum, *canSendReadNum);
            currDirectCanSendReadNum = std::min(currDirectCanSendReadNum, bdbCanUseNum);
            if (currDirectCanSendReadNum == 0) {
                continue;
            }

            if (entry->pairStatus == ExecStatus::NO_START) {
                entry->pairStatus = ExecStatus::SEND_READ;
                entry->genReadCycle = GetSim()->getCycles();
                IncOutstandingTileOpNum(entry->tileArg.op);
            }
            while (currDirectCanSendReadNum > 0) {
                if (SendOneReadReq2RFB(entry)) {
                    --(*canSendReadNum);
                    --currDirectCanSendReadNum;
                } else {
                    break;
                }
            }
            continue;
        }

        if (entry->pairStatus == ExecStatus::SEND_WRITE && *canSendWriteNum > 0) {
            uint32_t currDirectCanSendWriteNum = GetWCBRemainSize(entry->direction);
            currDirectCanSendWriteNum = std::min(*canSendWriteNum, currDirectCanSendWriteNum);
            if (currDirectCanSendWriteNum == 0) {
                continue;
            }
            while (currDirectCanSendWriteNum > 0) {
                if (SendOneWriteReq2WCB(entry)) {
                    --currDirectCanSendWriteNum;
                    --(*canSendWriteNum);
                } else {
                    break;
                }
            }
            continue;
        }
    }
}

BPQEntryPtr BridgePairQ::PickEntry()
{
    BPQEntryPtr pickedEntry = nullptr;

    /* {group id : tid} */
    std::unordered_map<uint32_t, uint64_t> groupVscatterEntries;

    for (auto entry : m_bpq) {
        if (entry->tileArg.op == TileOp::VSCATTER) {
            if (groupVscatterEntries.count(entry->tileArg.groupSlotId) != 0) {
                groupVscatterEntries[entry->tileArg.groupSlotId] = std::min(entry->bpqID,
                    groupVscatterEntries[entry->tileArg.groupSlotId]);
            } else {
                groupVscatterEntries[entry->tileArg.groupSlotId] = entry->bpqID;
            }
        }
    }

    bool needRetry = true;
    static uint64_t bpqSentReqNum = 0;
    for (auto entry : m_bpq) {
        if (!IsMemoryModeReq(entry->tileArg.op)) {
            continue;
        }
        /* The subsequent requests are not executed until the VSCATTER in the same group is completed. */
        if (groupVscatterEntries.count(entry->tileArg.groupSlotId) != 0 &&
            entry->bpqID > groupVscatterEntries[entry->tileArg.groupSlotId]) {
            continue;
        }

        if (entry->pairStatus != ExecStatus::SEND_READ) {
            continue;
        }

        if (entry->sendReadCounter == entry->readReqs.size()) {
            continue;
        }

        for (uint64_t readReqId = 0; readReqId < entry->readReqs.size(); ++readReqId) {
            if (entry->readReqSentAttemped[readReqId]) {
                continue;
            }
            if (m_config.BPQ_PERFECT_PICK_REQ && m_tag->HasDataInflight(entry->readReqs[readReqId].readAddr)) {
                continue;
            }
            needRetry = false;
            pickedEntry = entry;
            break;
        }

        if (pickedEntry) {
            ++bpqSentReqNum;
            break;
        }
    }

    if (needRetry || bpqSentReqNum >= m_config.BPQ_MEMORY_MODE_REQ_RETRY_THRESHOLD) {
        for (auto entry : m_bpq) {
            if (!IsMemoryModeReq(entry->tileArg.op)) {
                continue;
            }
            if (entry->pairStatus != ExecStatus::SEND_READ) {
                continue;
            }

            if (entry->sendReadCounter == entry->readReqs.size()) {
                continue;
            }

            for (uint64_t i = 0; i < entry->readReqStatus.size(); ++i) {
                entry->readReqSentAttemped[i] = (entry->readReqStatus[i] == ReadReqStatus::ISSUED);
            }
        }
        bpqSentReqNum = 0;
    }

    return pickedEntry;
}

void BridgePairQ::SendReadWriteReqMemoryMode()
{
    if (m_bpq.empty() || m_config.fake_vgather_enable) {
        return;
    }

    BPQEntryPtr pickedEntry = PickEntry();
    if (!pickedEntry) {
        return;
    }

    /* TODO: Multi-send is not supported currently */
    SendTagReq(pickedEntry);
}

void BridgePairQ::SendTagReq(BPQEntryPtr entry)
{
    if (entry->tileArg.op == TileOp::MGATHER || entry->tileArg.op == TileOp::VGATHER) {
        uint32_t currDirectCanSendWriteNum = GetWCBRemainSize(entry->direction);
        if (currDirectCanSendWriteNum > 0) {
            if (SendOneWriteReq2WCB(entry)) {
                --currDirectCanSendWriteNum;
            }
        }

        // Check if we can send read requests
        if (entry->sendWriteCounter < entry->writeReqs.size()) {
            return;
        }
    }

    if (m_caqAvailCnt != 0 && m_srfbAvailCnt != 0 && m_swcbAvailCnt != 0) {
        /* Pick a request that has not been sent. */
        uint64_t readReqId = 0;
        for (bool hasSend : entry->readReqSentAttemped) {
            if (hasSend) {
                ++readReqId;
                continue;
            }
            if ((m_config.BPQ_PERFECT_PICK_REQ && m_tag->HasDataInflight(entry->readReqs[readReqId].readAddr))) {
                ++readReqId;
                continue;
            }
            break;
        }
        ASSERT(readReqId < entry->readReqs.size());
        entry->readReqSentAttemped[readReqId] = true;
        entry->readReqStatus[readReqId] = ReadReqStatus::ISSUED;
        entry->readReqSendCnt[readReqId]++;
        if (entry->readReqTagPipeViews[readReqId].firstPickCycle == 0) {
            entry->readReqTagPipeViews[readReqId].firstPickCycle = GetSim()->getCycles();
        }
        ++entry->sendReadCounter;
        struct TagReq tagReq = {
            .readReqId = readReqId,
            .entry = entry,
        };
        m_bpqTagReadReqQ->Write(tagReq);
        AcquireCAQAvailCnt();
        AcquireSRFBAvailCnt();
        AcquireSWCBAvailCnt();
        LOG_INFO_M(Unit::TMA, Stage::BPQ) << "[Bridge Table]: bid:" << std::dec << entry->bid
            << " Send Read Req to tag, bpq id:" << std::dec << entry->bpqID
            << " addr:0x" << std::hex << entry->readReqs[readReqId].readAddr;
    }
}

void BridgePairQ::ReceiveRFBReadRslv()
{
    auto handleRFBReq = [this](std::shared_ptr<SimQueue<RFBEntry>>& rfbBpqQ) {
        RFBEntry&& rfbEntry = rfbBpqQ->Read();
        uint64_t bpqId = rfbEntry.bpqId;
        BPQEntryPtr bpqEntry = GetBpqPtr(bpqId);
        ASSERT(bpqEntry);

        LOG_INFO_M(Unit::TMA, Stage::BPQ) << "[Bridge Table]: receive read resolve for bpq id:" << std::dec << bpqId;
        ASSERT(bpqEntry->pairStatus == ExecStatus::SEND_READ || bpqEntry->pairStatus == ExecStatus::WAIT_READ_DATA);
        ++bpqEntry->resolveReadCounter;
        if (IsMemoryModeReq(bpqEntry->tileArg.op)) {
            return;
        }

        if (bpqEntry->resolveReadCounter == bpqEntry->readReqs.size()) {
            ASSERT(bpqEntry->pairStatus == ExecStatus::WAIT_READ_DATA);
            bpqEntry->pairStatus = ExecStatus::SEND_WRITE;
            bpqEntry->genWriteCycle = GetSim()->getCycles();
            if (bpqEntry->genReadCycle == 0) {
                IncOutstandingTileOpNum(bpqEntry->tileArg.op);
            }
        }
    };

    while (!m_nRFBBpqQ->Empty()) {
        handleRFBReq(m_nRFBBpqQ);
    }

    while (!m_sRFBBpqQ->Empty()) {
        handleRFBReq(m_sRFBBpqQ);
    }
}

void BridgePairQ::ReceiveWCBWriteRslv()
{
    auto handleWCBReq = [this](std::shared_ptr<SimQueue<WCBEntry>>& wcbBpqQ) {
        WCBEntry&& wcbEntry = wcbBpqQ->Read();
        uint64_t bpqId = wcbEntry.bpqId;

        BPQEntryPtr bpqEntry = GetBpqPtr(bpqId);
        ASSERT(bpqEntry);
        /* For memory mode, only mgather and vgather can reach this point. */
        if (!IsMemoryModeReq(bpqEntry->tileArg.op)) {
            ASSERT(bpqEntry->resolveReadCounter == bpqEntry->sendReadCounter);
            ASSERT(bpqEntry->pairStatus == ExecStatus::SEND_WRITE ||
                   bpqEntry->pairStatus == ExecStatus::WAIT_WRITE_RESOLVE);
        } else {
            ASSERT(IsGatherReq(bpqEntry->tileArg.op));
            ASSERT(wcbEntry.memDir == MemoryUnit::TR);
        }
        ++bpqEntry->resolveWriteCounter;
        if (bpqEntry->resolveWriteCounter == bpqEntry->writeReqs.size()) {
            if (!IsMemoryModeReq(bpqEntry->tileArg.op)) {
                ASSERT(bpqEntry->pairStatus == ExecStatus::WAIT_WRITE_RESOLVE);
            }
            bpqEntry->pairStatus = ExecStatus::COMPLETE;
            bpqEntry->completeCycle = GetSim()->getCycles();
        }
    };

    while (!m_nWCBBpqQ->Empty()) {
        handleWCBReq(m_nWCBBpqQ);
    }

    while (!m_sWCBBpqQ->Empty()) {
        ReleaseSWCBAvailCnt();
        handleWCBReq(m_sWCBBpqQ);
    }
}

void BridgePairQ::PrintPipeViewLog(BPQEntryPtr entry)
{
    InstPipeViewPtr instInfo = std::make_shared<InstPipeViewInfo>();
    instInfo->bid = entry->bid.val;
    instInfo->cycleInfo = std::make_shared<CycleInfo>();
    instInfo->cycleInfo->startCycle = entry->startCycle;
    instInfo->cycleInfo->waitOtherBPQCycle = entry->waitOtherBPQCycle;
    if (IsMemoryModeReq(entry->tileArg.op)) {
        instInfo->cycleInfo->tagCycle = entry->genReadCycle;
    } else {
        instInfo->cycleInfo->genReadCycle = entry->genReadCycle;
    }
    instInfo->cycleInfo->waitReadCycle = entry->waitReadCycle;
    instInfo->cycleInfo->genWriteCycle = entry->genWriteCycle;
    instInfo->cycleInfo->waitWriteCycle = entry->waitWriteCycle;
    instInfo->cycleInfo->bridgeCompleteCycle = entry->completeCycle;
    instInfo->cycleInfo->retireCycle = GetSim()->getCycles();
    if (IsMemoryModeReq(entry->tileArg.op)) {
        std::stringstream ss;
        ss << GetTileOpName(entry->tileArg.op) << ", srcType: " << BriefDataType2String(entry->tileArg.srcType)
           << ", dstType: " << BriefDataType2String(entry->tileArg.srcType)
           << ", src addr:0x" << std::hex << entry->tileArg.baseAddrTR0
           << ", dst addr:0x" << entry->tileArg.baseAddrDstTR;
        instInfo->label = ss.str();
    } else {
        instInfo->label = entry->blockCmd->Dump();
    }
    GetSim()->GetViewManager(entry->tileArg.stid)->RecordMinst(instInfo);

    if (IsMemoryModeReq(entry->tileArg.op)) {
        for (uint32_t i = 0; i < entry->readReqs.size(); ++i) {
            m_tag->PrintPipeViewLog(entry, i);
        }
        m_srfb->PrintPipeViewLog(entry->bpqID, entry->tileArg.stid);
        m_nwcb->PrintPipeViewLog(entry->bpqID, entry->tileArg.stid);
    }
}

void BridgePairQ::LogSwimLane(BPQEntry &entry)
{
    SwimLogData logData;
    logData.name = entry.blockCmd->DumpFormalName();
    logData.pid = CORE_TOP_MACHINE_ID;
    logData.tid = entry.blockCmd->execMachineId;
    if (entry.genReadCycle != 0) {
        logData.sTime = entry.genReadCycle;
    } else {
        logData.sTime = entry.genWriteCycle;
    }
    entry.blockCmd->eventId = (entry.blockCmd->eventId >= 0) ? entry.blockCmd->eventId : GetSim()->GetEventId();
    logData.eventId = entry.blockCmd->eventId;
    logData.eTime = GetSim()->getCycles();
    logData.hint = entry.DumpSwimLane();
    GetSim()->AddDuration(logData);
    GetSim()->AddDependency(entry.bid.val, entry.blockCmd->eventId, entry.tileArg.stid);
}

bool BridgePairQ::ResolveSplitBPQ(BPQEntryPtr entry, SimQueue<BlockCommandPtr>& wakeUpQ)
{
    auto updateTloadBidOrderList = [this](BPQEntryPtr entry) {
        if (entry->tileArg.op != TileOp::TLOAD) {
            return;
        }
        auto it = std::find(m_tloadBidOrderList[entry->tileArg.stid].cbegin(),
                            m_tloadBidOrderList[entry->tileArg.stid].cend(), entry->bid);
        ASSERT(it != m_tloadBidOrderList[entry->tileArg.stid].cend());
        m_tloadBidOrderList[entry->tileArg.stid].erase(it);
    };

    bool needDestroyBPQ = false;
    std::shared_ptr<BridgeDataBuffer> bdb = m_bdb;
    if (entry->tileArg.op == TileOp::TSTORE || entry->tileArg.op == TileOp::TMOV || IsScatterReq(entry->tileArg.op)) {
        bdb = m_tsBDB;
    }
    if (entry->attr == BPQAttr::ORIGINAL) {
        needDestroyBPQ = true;
        PrintPipeViewLog(entry);
        LogSwimLane(*entry);
        m_tidPool.push_back(entry->blockCmd->execMachineId - machineId);
        ASSERT(entry->activeRowBPQNum == 0);
        ASSERT(entry->activeInstBPQNum == 0);
        entry->blockCmd->cmdExecCompleted = true;
        wakeUpQ.Write(entry->blockCmd);
        GetSim()->core->bctrl->blockROB.completeBlock(entry->bid, true, entry->tileArg.stid);
        updateTloadBidOrderList(entry);
    } else if (entry->attr == BPQAttr::SUB_INST_LEAD) {
        if (entry->bdbRelatedBPQNum == 0) {
            bdb->ReleaseBuffer(entry->bpqID);
        }
        if (entry->activeRowBPQNum == 0) {
            if (!entry->hasSendResolve) {
                entry->hasSendResolve = true;
                entry->blockCmd->cmdExecCompleted = true;
                wakeUpQ.Write(entry->blockCmd);
            }
            if (entry->activeInstBPQNum == 0) {
                needDestroyBPQ = true;
                ASSERT(entry->bdbRelatedBPQNum == 0);
                PrintPipeViewLog(entry);
                LogSwimLane(*entry);
                m_tidPool.push_back(entry->blockCmd->execMachineId - machineId);
                GetSim()->core->bctrl->blockROB.completeBlock(entry->bid, true, entry->tileArg.stid);
                updateTloadBidOrderList(entry);
            }
        }
    } else if (entry->attr == BPQAttr::SUB_ROW_LEAD) {
        if (!entry->hasSendResolve) {
            entry->hasSendResolve = true;
            entry->instLeadBPQPtr->activeRowBPQNum--;
        }
        if (entry->bdbRelatedBPQNum == 0) {
            PrintPipeViewLog(entry);
            needDestroyBPQ = true;
        }
    } else if (entry->attr == BPQAttr::SUB_INST) {
        if (!entry->hasSendResolve) {
            entry->hasSendResolve = true;
            entry->rowLeadBPQPtr->bdbRelatedBPQNum--;
        }
        if (entry->activeRowBPQNum == 0) {
            PrintPipeViewLog(entry);
            needDestroyBPQ = true;
            entry->blockCmd->cmdExecCompleted = true;
            wakeUpQ.Write(entry->blockCmd);
            entry->rowLeadBPQPtr->activeInstBPQNum--;
        }
    } else if (entry->attr == BPQAttr::SUB_ROW) {
        PrintPipeViewLog(entry);
        needDestroyBPQ = true;
        entry->instLeadBPQPtr->activeRowBPQNum--;
        entry->rowLeadBPQPtr->bdbRelatedBPQNum--;
    } else if (entry->attr == BPQAttr::SUB_COL_LEAD) {
        if (!entry->hasSendResolve) {
            if (IsGatherReq(entry->tileArg.op)) {
                /* do nothing */
            } else if (IsScatterReq(entry->tileArg.op)) {
                bdb->ReleaseBuffer(std::unordered_set<uint64_t>(entry->tsBDBIds.begin(), entry->tsBDBIds.end()));
            } else {
                bdb->ReleaseBuffer(entry->bpqID);
            }
            entry->hasSendResolve = true;
        }
        if (entry->activeInstBPQNum == 0) {
            needDestroyBPQ = true;
            ASSERT(entry->bdbRelatedBPQNum == 0);
            entry->blockCmd->cmdExecCompleted = true;
            wakeUpQ.Write(entry->blockCmd);
            PrintPipeViewLog(entry);
            LogSwimLane(*entry);
            m_tidPool.push_back(entry->blockCmd->execMachineId - machineId);
            GetSim()->core->bctrl->blockROB.completeBlock(entry->bid, true, entry->tileArg.stid);
            updateTloadBidOrderList(entry);
        }
    } else if (entry->attr == BPQAttr::SUB_COL) {
        PrintPipeViewLog(entry);
        needDestroyBPQ = true;
        entry->rowLeadBPQPtr->activeInstBPQNum--;
        if (!IsMemoryModeReq(entry->tileArg.op)) {
            bdb->ReleaseBuffer(entry->bpqID);
        }
        if (IsScatterReq(entry->tileArg.op)) {
            bdb->ReleaseBuffer(std::unordered_set<uint64_t>(entry->tsBDBIds.begin(), entry->tsBDBIds.end()));
        }
    } else {
        ASSERT(false && "Will no reached!");
    }

    return needDestroyBPQ;
}

void BridgePairQ::ResolveBlock()
{
    for (auto it = m_bpq.begin(); it != m_bpq.end();) {
        BPQEntryPtr entry = *it;
        if (entry->pairStatus != ExecStatus::COMPLETE) {
            ++it;
            continue;
        }
        LOG_DEBUG_M(Unit::TMA, Stage::NA) << "[Bridge Table]: check counter before resolve: resolveReadCounter = "
                                         << entry->resolveReadCounter
                                         << " sendReadCounter = " << entry->sendReadCounter
                                         << " sendWriteCounter = " << entry->sendWriteCounter;
        ASSERT(entry->resolveWriteCounter == entry->sendWriteCounter);
        /* FIXME: merge complete block and resolve q to one. */
        if (entry->tileArg.op == TileOp::TLOAD) {
            if (!ResolveSplitBPQ(entry, *tmaBCCWakeupQ)) {
                ++it;
                continue;
            }
            --m_tloadCnt;
        } else if (entry->tileArg.op == TileOp::TSTORE) {
            if (!ResolveSplitBPQ(entry, *tmaBCCWakeupQ)) {
                ++it;
                continue;
            }
            --m_tstoreCnt;
        } else if (entry->tileArg.op == TileOp::TMOV) {
            if (!ResolveSplitBPQ(entry, *tauBCCWakeupQ)) {
                ++it;
                continue;
            }
            --m_tmovCnt;
        } else if (entry->tileArg.op == TileOp::MGATHER) {
            if (!ResolveSplitBPQ(entry, *tmaBCCWakeupQ)) {
                ++it;
                continue;
            }
            --m_mgatherCnt;
            m_pmuData.mgatherReadTileNum += entry->readOffsetReqs.size();
            m_pmuData.mgatherReadTagReqNum += entry->readReqs.size();
            m_pmuData.mgatherWriteNum += entry->writeReqs.size();
            m_pmuData.mgatherWriteSize += (entry->tileArg.d1TR * entry->tileArg.d2TR *
                static_cast<uint64_t>(GetDataSize(entry->tileArg.dstType)));
        } else if (entry->tileArg.op == TileOp::VGATHER) {
            if (m_vectorBridgeBPQRspQ->SizeW() != 0) {
                ++it;
                /* Only one resolve can be issued per cycle. */
                continue;
            }
            m_pmuData.vgatherReadTileNum += entry->readOffsetReqs.size();
            m_pmuData.vgatherReadTagReqNum += entry->readReqs.size();
            m_pmuData.vgatherWriteNum += entry->writeReqs.size();
            PrintPipeViewLog(entry);
            VectorBridgeRsp rspCmd = {};
            rspCmd.tid = entry->tileArg.tid;
            rspCmd.groupSlotId = entry->tileArg.groupSlotId;
            m_vectorBridgeBPQRspQ->Write(rspCmd);
            --m_vgatherCnt;
        } else if (entry->tileArg.op == TileOp::MSCATTER) {
            if (!ResolveSplitBPQ(entry, *tmaBCCWakeupQ)) {
                ++it;
                continue;
            }
            --m_mscatterCnt;
            m_pmuData.mscatterNum++;
            m_pmuData.mscatterReadTagReqNum += entry->readReqs.size();
        } else if (entry->tileArg.op == TileOp::VSCATTER) {
            /* No response needs to be sent to other components. */
            PrintPipeViewLog(entry);
            m_tsBDB->ReleaseBuffer(std::unordered_set<uint64_t>(entry->tsBDBIds.begin(), entry->tsBDBIds.end()));
            --m_vscatterCnt;
            m_pmuData.vscatterNum++;
            m_pmuData.vscatterReadTagReqNum += entry->readReqs.size();
        } else {
            LOG_ERROR_M(Unit::TMA, Stage::NA) << "[Bridge Table]: Unsupport tileop for tile bridge";
            abort();
        }
        DecOutstandingTileOpNum(entry->tileArg.op);
        if (!IsMemoryModeReq(entry->tileArg.op)) {
            std::shared_ptr<BridgeDataBuffer> bdb = m_bdb;
            if (entry->tileArg.op == TileOp::TSTORE || entry->tileArg.op == TileOp::TMOV) {
                bdb = m_tsBDB;
            }
            bdb->ReleaseBuffer(entry->bpqID);
        }
        LOG_INFO_M(Unit::TMA, Stage::NA) << "[Bridge Table]: resolve block BPQ" << std::dec << entry->bpqID;
        m_bpqIDPool.emplace(entry->bpqID);
        it = m_bpq.erase(it);
    }
}

bool BridgePairQ::IsBusy()
{
    return m_bpq.size() != 0;
}

bool BridgePairQ::IsTloadBusy()
{
    return m_pmuData.outstandingTloadNum > 0;
}

bool BridgePairQ::IsTstoreBusy()
{
    return m_pmuData.outstandingTstoreNum > 0;
}

bool BridgePairQ::IsTmovBusy()
{
    return m_pmuData.outstandingTmovNum > 0;
}

bool BridgePairQ::IsMgatherBusy()
{
    return m_pmuData.outstandingMgatherNum > 0;
}

bool BridgePairQ::IsMscatterBusy()
{
    return m_pmuData.outstandingMscatterNum > 0;
}

bool BridgePairQ::IsVgatherBusy()
{
    return m_pmuData.outstandingVgatherNum > 0;
}

bool BridgePairQ::IsVscatterBusy()
{
    return m_pmuData.outstandingVscatterNum > 0;
}

void BridgePairQ::IncOutstandingTileOpNum(TileOp op)
{
    if (op == TileOp::TLOAD) {
        m_pmuData.outstandingTloadNum++;
    } else if (op == TileOp::TSTORE) {
        m_pmuData.outstandingTstoreNum++;
    } else if (op == TileOp::TMOV) {
        m_pmuData.outstandingTmovNum++;
    } else if (op == TileOp::MGATHER) {
        m_pmuData.outstandingMgatherNum++;
    } else if (op == TileOp::MSCATTER) {
        m_pmuData.outstandingMscatterNum++;
    } else if (op == TileOp::VGATHER) {
        m_pmuData.outstandingVgatherNum++;
    } else if (op == TileOp::VSCATTER) {
        m_pmuData.outstandingVscatterNum++;
    } else {
        LOG_ERROR_M(Unit::TMA, Stage::NA) << "[Bridge Table]: Unsupport tileop for tile bridge";
        abort();
    }
}

void BridgePairQ::DecOutstandingTileOpNum(TileOp op)
{
    if (op == TileOp::TLOAD) {
        ASSERT(m_pmuData.outstandingTloadNum > 0);
        m_pmuData.outstandingTloadNum--;
    } else if (op == TileOp::TSTORE) {
        ASSERT(m_pmuData.outstandingTstoreNum > 0);
        m_pmuData.outstandingTstoreNum--;
    } else if (op == TileOp::TMOV) {
        ASSERT(m_pmuData.outstandingTmovNum > 0);
        m_pmuData.outstandingTmovNum--;
    } else if (op == TileOp::MGATHER) {
        ASSERT(m_pmuData.outstandingMgatherNum > 0);
        m_pmuData.outstandingMgatherNum--;
    } else if (op == TileOp::MSCATTER) {
        ASSERT(m_pmuData.outstandingMscatterNum > 0);
        m_pmuData.outstandingMscatterNum--;
    } else if (op == TileOp::VGATHER) {
        ASSERT(m_pmuData.outstandingVgatherNum > 0);
        m_pmuData.outstandingVgatherNum--;
    } else if (op == TileOp::VSCATTER) {
        ASSERT(m_pmuData.outstandingVscatterNum > 0);
        m_pmuData.outstandingVscatterNum--;
    } else {
        LOG_ERROR_M(Unit::TMA, Stage::NA) << "[Bridge Table]: Unsupport tileop for tile bridge";
        abort();
    }
}

uint64_t BridgePairQ::GetBPQSize()
{
    return m_bpq.size();
}

uint64_t BridgePairQ::RemainBPQSize(bool isTstore)
{
    uint64_t remainSize = 0;
    if (isTstore) {
        ASSERT(m_config.BPQ_SIZE >= (GetBPQSize()));
        remainSize = m_config.BPQ_SIZE - GetBPQSize();
    } else {
        if (GetSim()->core->configs.ruminateEnable && id >= GetSim()->core->configs.tauStartId) {
            ASSERT(m_config.BPQ_SIZE >= (GetBPQSize()));
            remainSize = m_config.BPQ_SIZE - GetBPQSize();
        } else {
            if (m_config.BPQ_SIZE >= (m_config.BPQ_TSTORE_ONLY_USE_SIZE + GetBPQSize())) {
                remainSize = m_config.BPQ_SIZE - m_config.BPQ_TSTORE_ONLY_USE_SIZE - GetBPQSize();
            } else {
                remainSize = 0;
            }
        }
    }

    return remainSize;
}

BPQEntryPtr BridgePairQ::GetOldestBpqPtr(uint32_t stid)
{
    ROBID oldestBid = m_tloadBidOrderList[stid].front();
    for (auto entry : m_bpq) {
        if (entry->bid == oldestBid && entry->tileArg.stid == stid) {
            return entry;
        }
    }
    return nullptr;
}

BPQEntryPtr BridgePairQ::GetBpqPtr(uint64_t bpqId)
{
    for (auto entry : m_bpq) {
        if (entry->bpqID == bpqId) {
            return entry;
        }
    }
    return nullptr;
}

std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> BridgePairQ::GetSwitchMask(uint64_t bpqId,
    uint64_t baseAddr)
{
    BPQEntryPtr entry = GetBpqPtr(bpqId);
    ASSERT(entry);
    uint64_t cacheLineEndAddr = baseAddr + m_config.GLOBAL_MEMORY_CACHELINE_SIZE;
    double dataSize = GetDataSize(entry->tileArg.dstType);
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> switchMask;

    if (entry->tileArg.op == TileOp::MGATHER || entry->tileArg.op == TileOp::VGATHER) {
        /* Mgather, switchMask[write offset] = (tl bdb entry offset, write size) */
        uint32_t writeAddrOffset = entry->tileArg.baseAddrDstTR - (entry->tileArg.baseAddrDstTR & TILE_ALIGN_MASK);
        for (uint64_t i = 0; i < entry->dataAddrs.size(); ++i) {
            if (!entry->tileArg.predicateMask.at(i)) {
                continue;
            }
            uint64_t dataAddr = entry->dataAddrs[i];
            uint64_t endAddr = dataAddr + static_cast<uint64_t>(std::ceil(dataSize));
            uint32_t writeOffset = writeAddrOffset + static_cast<uint32_t>(i * dataSize);
            uint64_t readOffset = 0;
            uint32_t writeDataSize = 0;
            if (dataAddr >= baseAddr && dataAddr < cacheLineEndAddr) {
                readOffset = dataAddr - baseAddr;
                writeDataSize = std::min(endAddr, cacheLineEndAddr) - dataAddr;
                switchMask[writeOffset] = std::make_pair(dataAddr - baseAddr, writeDataSize);
            } else if (endAddr >= baseAddr && endAddr < cacheLineEndAddr) {
                writeOffset += (baseAddr - dataAddr);
                writeDataSize = static_cast<uint64_t>(dataSize) - (baseAddr - dataAddr);
                switchMask[writeOffset] = std::make_pair(0, writeDataSize);
            }
            if (entry->tileArg.op != TileOp::MGATHER) {
                continue;
            }
            for (uint32_t dataId = 0; dataId < writeDataSize; ++dataId) {
                m_mgatherReadTagDataUsage[baseAddr].set(readOffset + dataId);
            }
        }
    } else if (entry->tileArg.op == TileOp::MSCATTER || entry->tileArg.op == TileOp::VSCATTER) {
        /* Mscatter, switchMasks[tl bdb entry offset] = (ts bdb entry offset, write size) */
        for (uint64_t i = 0; i < entry->dataAddrs.size(); ++i) {
            if (!entry->tileArg.predicateMask.at(i)) {
                continue;
            }
            uint64_t dataAddr = entry->dataAddrs[i];
            uint64_t endAddr = dataAddr + static_cast<uint64_t>(std::ceil(dataSize));
            if (dataAddr >= baseAddr && dataAddr < cacheLineEndAddr) {
                uint32_t writeDataSize = std::min(endAddr, cacheLineEndAddr) - dataAddr;
                switchMask[dataAddr - baseAddr] = std::make_pair(i * dataSize, writeDataSize);
            } else if (endAddr >= baseAddr && endAddr < cacheLineEndAddr) {
                uint32_t writeDataSize = static_cast<uint64_t>(dataSize) - (baseAddr - dataAddr);
                switchMask[0] = std::make_pair(i * dataSize + (baseAddr - dataAddr), writeDataSize);
            }
        }
    } else {
        ASSERT(false && "Will no reached!");
    }

    return switchMask;
}

void BridgePairQ::ReleaseCAQAvailCnt()
{
    ++m_caqAvailCnt;
    ASSERT(m_caqAvailCnt <= m_config.CAQ_SIZE);
}

void BridgePairQ::AcquireCAQAvailCnt()
{
    --m_caqAvailCnt;
    ASSERT(m_caqAvailCnt >= 0);
}

void BridgePairQ::ReleaseSRFBAvailCnt()
{
    ++m_srfbAvailCnt;
    ASSERT(m_srfbAvailCnt <= m_config.SRFB_OUTSTANDING_REQ_NUM);
}

void BridgePairQ::AcquireSRFBAvailCnt()
{
    --m_srfbAvailCnt;
    ASSERT(m_srfbAvailCnt >= 0);
}

void BridgePairQ::ReleaseSWCBAvailCnt()
{
    ++m_swcbAvailCnt;
    ASSERT(m_swcbAvailCnt <= m_config.SWCB_OUTSTANDING_REQ_NUM);
}

void BridgePairQ::AcquireSWCBAvailCnt()
{
    --m_swcbAvailCnt;
    ASSERT(m_swcbAvailCnt >= 0);
}

static uint32_t SetReqTokenId(bool isLoad, uint32_t coreId, uint64_t id)
{
    /*
     * +----------+------------+--------+----+
     * | Lsu Type | Load/Store | coreId | id |
     * +----------+------------+--------+----+
     *      8           1          3      20
     */
    constexpr uint32_t loadStoreBitOffset = 23;
    constexpr uint32_t coreIdBitOffset = 20;
    constexpr uint32_t coreIdBitWidth = loadStoreBitOffset - coreIdBitOffset;
    ASSERT((id & ((1ULL << coreIdBitOffset) - 1)) == id);
    ASSERT((coreId & ((1ULL << coreIdBitWidth) - 1)) == coreId && "Supports up to 8 TMAs");
    return (static_cast<uint32_t>(LSUType::BRIDGE_TABLE) << TID_TYPE_OFFSET) | (isLoad << loadStoreBitOffset) |
           (coreId << coreIdBitOffset) | id;
}

static void UnpackReqTokenId(uint32_t tid, bool& isLoad, uint32_t& coreId, uint64_t& id)
{
    constexpr uint32_t loadStoreBitOffset = 23;
    constexpr uint32_t coreIdBitOffset = 20;
    isLoad = (tid >> loadStoreBitOffset) & 1;
    coreId = ((tid & ((1ULL << loadStoreBitOffset) - 1)) >> coreIdBitOffset);
    id = tid & ((1ULL << coreIdBitOffset) - 1);
}

/* ========== Tag ========== */
Tag::Tag(TagConfig& conf)
    : m_sim(conf.sim), m_config(conf.config), m_bdb(conf.bdb), m_srfb(conf.srfb), m_bpq(conf.bpq),
      m_pmuData(conf.pmuData)
{
}

void Tag::Work()
{
    HandleReadReq();
}

void Tag::Xfer()
{
}

void Tag::Reset()
{
}

SimSys* Tag::GetSim()
{
    return this->m_sim;
}

void Tag::ReportStat()
{
}

std::vector<uint64_t> Tag::Alloc(uint64_t addr)
{
    ASSERT(!CheckVictim());
    ASSERT((addr & (m_config.GLOBAL_MEMORY_CACHELINE_SIZE - 1)) == 0 && "Address not aligned to granularity");
    uint32_t bdbNum = m_config.GLOBAL_MEMORY_CACHELINE_SIZE / m_config.BRIDGE_DATA_BUFFER_DATA_SIZE;
    std::vector<uint64_t> bdbInfo;

    for (uint32_t i = 0; i < bdbNum; ++i) {
        uint64_t bdbId = m_bdb->AllocDataBuffer(addr + i * m_config.BRIDGE_DATA_BUFFER_DATA_SIZE);
        ASSERT((bdbId != static_cast<uint64_t>(-1)) && "BDB has no free space!");
        bdbInfo.push_back(bdbId);
    }
    m_orderedMap.Insert(addr, bdbInfo);

    return bdbInfo;
}

std::vector<uint64_t> Tag::Alloc(uint64_t addr, std::vector<uint64_t>& bdbInfo)
{
    ASSERT(!CheckVictim());
    ASSERT((addr & (m_config.GLOBAL_MEMORY_CACHELINE_SIZE - 1)) == 0 && "Address not aligned to granularity");

    m_orderedMap.Insert(addr, bdbInfo);
    /* update buffer base addr */
    for (uint32_t i = 0; i < bdbInfo.size(); ++i) {
        auto id = bdbInfo[i];
        DataBlock& dataBlock = m_bdb->ReadDataBlock(id);
        dataBlock.baseAddr = addr + i * m_config.BRIDGE_DATA_BUFFER_DATA_SIZE;
    }

    return bdbInfo;
}

void Tag::Erase(uint64_t addr)
{
    m_orderedMap.Erase(addr);
}

void Tag::Clear()
{
    m_orderedMap.Clear();
}

bool Tag::Read(uint64_t addr, std::vector<uint64_t>& bdbInfo)
{
    bool tagHit = m_orderedMap.Find(addr, bdbInfo);
    if (tagHit) {
        /* Update  */
        m_orderedMap.Erase(addr);
        m_orderedMap.Insert(addr, bdbInfo);
    }
    return tagHit;
}

bool Tag::CheckVictim()
{
    return m_orderedMap.Size() == m_config.BRIDGE_DATA_BUFFER_TLOAD_ONLY_USE_SIZE /
        (m_config.GLOBAL_MEMORY_CACHELINE_SIZE / m_config.BRIDGE_DATA_BUFFER_DATA_SIZE);
}

std::pair<uint64_t, std::vector<uint64_t>> Tag::GetVictim()
{
    return m_orderedMap.Front();
}

std::pair<uint64_t, std::vector<uint64_t>> Tag::ReplaceVictim(uint64_t addr)
{
    auto oldestTag = GetVictim();
    auto oldestAddr = oldestTag.first;
    Erase(oldestAddr);
    Alloc(addr, oldestTag.second);

    return std::make_pair(oldestAddr, oldestTag.second);
}

bool Tag::HasDataInflight(uint64_t readAddr)
{
    std::vector<uint64_t> bdbInfo;
    bool tagHit = Read(readAddr, bdbInfo);
    if (tagHit) {
        for (auto& caqOp : m_cacheAaccelssQ->GetRawReadData()) {
            if (caqOp.readAddr == readAddr) {
                return true;
            }
        }

        for (auto& caqOp : m_cacheAaccelssQ->GetRawWriteData()) {
            if (caqOp.readAddr == readAddr) {
                return true;
            }
        }

        for (auto& tagReq : m_bpqTagReadReqQ->GetRawReadData()) {
            BPQEntryPtr entry = tagReq.entry;
            uint64_t readReqId = tagReq.readReqId;
            ReadReq& readReq = entry->readReqs[readReqId];
            if (readReq.readAddr == readAddr) {
                return true;
            }
        }

        for (auto& tagReq : m_bpqTagReadReqQ->GetRawWriteData()) {
            BPQEntryPtr entry = tagReq.entry;
            uint64_t readReqId = tagReq.readReqId;
            ReadReq& readReq = entry->readReqs[readReqId];
            if (readReq.readAddr == readAddr) {
                return true;
            }
        }

        if (m_srfb->CheckInFlightReq(readAddr)) {
            return true;
        }
    }
    return false;
}

void Tag::HandleReadReq()
{
    if (m_bpqTagReadReqQ->Empty()) {
        return;
    }

    TagReq&& tagReq = m_bpqTagReadReqQ->Read();
    BPQEntryPtr entry = tagReq.entry;
    uint64_t readReqId = tagReq.readReqId;

    std::vector<uint64_t> bdbInfo;
    ReadReq& readReq = entry->readReqs[readReqId];
    uint64_t readAddr = readReq.readAddr;
    ASSERT((readAddr & (m_config.GLOBAL_MEMORY_CACHELINE_SIZE - 1)) == 0);

    bool tagHit = Read(readAddr, bdbInfo);
    if (tagHit) {
        /* If the victim is not read out, the corresponding read request cannot be sent. */
        for (auto& caqOp : m_cacheAaccelssQ->GetRawReadData()) {
            if (caqOp.readAddr == readAddr) {
                entry->readReqStatus[readReqId] = ReadReqStatus::NOSTART;
                --entry->sendReadCounter;
                m_bpq.lock()->ReleaseCAQAvailCnt();
                m_bpq.lock()->ReleaseSRFBAvailCnt();
                m_bpq.lock()->ReleaseSWCBAvailCnt();
                LOG_DEBUG_M(Unit::TMA, Stage::TAG) << "[Tag]: addr hit and victim is not read out, bpq id:" << std::dec
                    << entry->bpqID << " read addr:0x" << std::hex << readAddr;
                m_pmuData.tagReqDuplicateHitInflightCnt = (entry->readReqSendCnt[readReqId] > 1) ?
                    m_pmuData.tagReqDuplicateHitInflightCnt + 1 : m_pmuData.tagReqDuplicateHitInflightCnt;
                return;
            }
        }

        for (auto& caqOp : m_cacheAaccelssQ->GetRawWriteData()) {
            if (caqOp.readAddr == readAddr) {
                entry->readReqStatus[readReqId] = ReadReqStatus::NOSTART;
                --entry->sendReadCounter;
                m_bpq.lock()->ReleaseCAQAvailCnt();
                m_bpq.lock()->ReleaseSRFBAvailCnt();
                m_bpq.lock()->ReleaseSWCBAvailCnt();
                LOG_DEBUG_M(Unit::TMA, Stage::TAG) << "[Tag]: addr hit and victim is not read out, bpq id:" << std::dec
                    << entry->bpqID << " read addr:0x" << std::hex << readAddr;
                m_pmuData.tagReqDuplicateHitInflightCnt = (entry->readReqSendCnt[readReqId] > 1) ?
                    m_pmuData.tagReqDuplicateHitInflightCnt + 1 : m_pmuData.tagReqDuplicateHitInflightCnt;
                return;
            }
        }

        if (m_srfb->CheckInFlightReq(readAddr)) {
            entry->readReqStatus[readReqId] = ReadReqStatus::NOSTART;
            --entry->sendReadCounter;
            m_bpq.lock()->ReleaseCAQAvailCnt();
            m_bpq.lock()->ReleaseSRFBAvailCnt();
            m_bpq.lock()->ReleaseSWCBAvailCnt();
            LOG_DEBUG_M(Unit::TMA, Stage::TAG) << "[Tag]: addr hit and data is in flight, bpq id:" << std::dec
                << entry->bpqID << " read addr:0x" << std::hex << readAddr;
            m_pmuData.tagReqDuplicateHitInflightCnt = (entry->readReqSendCnt[readReqId] > 1) ?
                    m_pmuData.tagReqDuplicateHitInflightCnt + 1 : m_pmuData.tagReqDuplicateHitInflightCnt;
            return;
        }

        ASSERT(!m_cacheAaccelssQ->Full());
        struct CacheAaccelssOp cao = {
            .type = (entry->tileArg.op == TileOp::MGATHER || entry->tileArg.op == TileOp::VGATHER) ?
                CacheAaccelssType::READ_TO_NWCB : CacheAaccelssType::WRITE_TO_BDB,
            .bdbIds = bdbInfo,
            .bpqId = entry->bpqID,
            .readReqId = readReqId,
            .readAddr = readAddr,
            .writeAddr = -1ULL,
        };
        m_cacheAaccelssQ->Write(cao);
        m_bpq.lock()->ReleaseSRFBAvailCnt();
        m_bpq.lock()->ReleaseSWCBAvailCnt();
        entry->readReqTagPipeViews[readReqId].hitCycle = GetSim()->getCycles();
        entry->readReqTagPipeViews[readReqId].caqCycle = GetSim()->getCycles() + 1;
        std::stringstream ss;
        ss << "[Tag]: addr hit, bpq id:" << std::dec << entry->bpqID << " read addr:0x" << std::hex << readAddr
           << " bdb ids: {";
        for (auto id : bdbInfo) {
            ss << std::dec << id << ", ";
        }
        ss << "}";
        LOG_DEBUG_M(Unit::TMA, Stage::TAG) << ss.str();
    } else {
        entry->readReqTagPipeViews[readReqId].missCycle = GetSim()->getCycles();
        m_pmuData.tagReqFirstHitThenMissCnt = (entry->readReqSendCnt[readReqId] > 1) ?
            m_pmuData.tagReqFirstHitThenMissCnt + 1 : m_pmuData.tagReqFirstHitThenMissCnt;
        if (CheckVictim()) {
            auto oldestTag = ReplaceVictim(readAddr);
            struct CacheAaccelssOp cao = {
                .type = CacheAaccelssType::READ_TO_SWCB,
                .bdbIds = oldestTag.second,
                .bpqId = entry->bpqID,
                .readReqId = readReqId,
                .readAddr = readAddr,
                .writeAddr = oldestTag.first,
            };
            ASSERT(!m_cacheAaccelssQ->Full());
            m_cacheAaccelssQ->Write(cao);
            entry->readReqTagPipeViews[readReqId].caqCycle = GetSim()->getCycles() + 1;
            std::stringstream ss;
            ss << "[Tag]: addr miss and need victim, bpq id:" << std::dec << entry->bpqID
               << " read addr:0x" << std::hex << readAddr << " write addr:0x" << std::hex << cao.writeAddr
               << " bdb ids: {";
            for (auto id : oldestTag.second) {
                ss << std::dec << id << ", ";
            }
            ss << "}";
            LOG_DEBUG_M(Unit::TMA, Stage::TAG) << ss.str();
        } else {
            bdbInfo = Alloc(readAddr);
            RFBEntry rfbEntry;
            rfbEntry.addr = readReq.readAddr;
            rfbEntry.size = readReq.readSize;
            rfbEntry.bpqId = entry->bpqID;
            rfbEntry.bid = entry->bid;
            rfbEntry.memDir = entry->direction;
            rfbEntry.op = entry->tileArg.op;
            rfbEntry.stid = entry->tileArg.stid;
            rfbEntry.bdbId = bdbInfo;
            m_tagSRFBQ->Write(rfbEntry);
            m_bpq.lock()->ReleaseCAQAvailCnt();
            m_bpq.lock()->ReleaseSWCBAvailCnt();
            entry->readReqTagPipeViews[readReqId].rfbCycle = GetSim()->getCycles() + 1;
            entry->readReqTagPipeViews[readReqId].retireCycle = GetSim()->getCycles() + 2;
            std::stringstream ss;
            ss << "[Tag]: addr miss no victim, bpq id:" << std::dec << entry->bpqID
               << " read addr:0x" << std::hex << readAddr << " bdb ids: {";
            for (auto id : bdbInfo) {
                ss << std::dec << id << ", ";
            }
            ss << "}";
            LOG_DEBUG_M(Unit::TMA, Stage::TAG) << ss.str();
        }
    }
}

void Tag::PrintPipeViewLog(BPQEntryPtr entry, uint64_t readReqId)
{
    std::stringstream ss;
    ss << "Tag Req 0x" << std::hex << entry->readReqs.at(readReqId).readAddr
       << " size 0x" << entry->readReqs.at(readReqId).readSize << std::endl;

    ReadReqPipeView& readReqPV = entry->readReqTagPipeViews.at(readReqId);
    InstPipeViewPtr instInfo = std::make_shared<InstPipeViewInfo>();
    instInfo->bid = entry->bid.val;
    instInfo->cycleInfo = std::make_shared<CycleInfo>();
    instInfo->cycleInfo->startCycle = readReqPV.startCycle;
    instInfo->cycleInfo->firstPickCycle = readReqPV.firstPickCycle;
    instInfo->cycleInfo->hitCycle = readReqPV.hitCycle;
    instInfo->cycleInfo->missCycle = readReqPV.missCycle;
    instInfo->cycleInfo->caqCycle = readReqPV.caqCycle;
    instInfo->cycleInfo->bdbReadCycle = readReqPV.bdbReadCycle;
    instInfo->cycleInfo->bdbWriteCycle = readReqPV.bdbWriteCycle;
    instInfo->cycleInfo->rfbCycle = readReqPV.rfbCycle;
    instInfo->cycleInfo->retireCycle = readReqPV.retireCycle;
    instInfo->label = ss.str();
    GetSim()->GetViewManager(entry->tileArg.stid)->RecordMinst(instInfo);
}

/* ========== Memory Pre Buffer ========== */
MemoryPreBuffer::MemoryPreBuffer(SimSys *sim, std::weak_ptr<BridgePairQ> bpq, std::shared_ptr<BridgeDataBuffer> bdb,
                                 std::shared_ptr<ReadFillBuffer> srfb, TileBridgeConfig config)
    : m_sim(sim), m_bpq(bpq), m_bdb(bdb), m_srfb(srfb), m_config(config)
{
    m_memoryPreBuffer.InitMaxSize(m_config.MPB_SIZE);
}

void MemoryPreBuffer::Work()
{
}

void MemoryPreBuffer::Xfer()
{
    m_memoryPreBuffer.Work();
}

void MemoryPreBuffer::Reset()
{
}

SimSys* MemoryPreBuffer::GetSim()
{
    return m_sim;
}

void MemoryPreBuffer::ReportStat()
{
}

bool MemoryPreBuffer::Empty()
{
    return m_memoryPreBuffer.Empty();
}

bool MemoryPreBuffer::PushData(MPBElem elem)
{
    if (m_memoryPreBuffer.Full()) {
        return false;
    }
    m_memoryPreBuffer.Write(elem);
    return true;
}

void MemoryPreBuffer::FillWCBAndBDB()
{
    ASSERT(!this->Empty());
    MPBElem&& elem = m_memoryPreBuffer.Read();

    LOG_DEBUG_M(MachineType::TMA, Stage::MPB) << "MPB fill wcb and bdb, bpq id:" << std::dec << elem.bpqId;
    BPQEntryPtr bpqEntry = m_bpq.lock()->GetBpqPtr(elem.bpqId);
    if (IsGatherReq(bpqEntry->tileArg.op)) {
        std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> switchMask =
            m_bpq.lock()->GetSwitchMask(elem.bpqId, elem.readAddr);

        SwitchInfo switchInfo;
        switchInfo.bpqId = elem.bpqId;
        switchInfo.data = *elem.data;
        switchInfo.switchMask = switchMask;
        m_switchNetwork->write(switchInfo);
    }

    DataBlock& dataBlock = m_bdb->ReadDataBlock(elem.bdbIds.at(0));
    bool needWriteBDB = (dataBlock.baseAddr == elem.readAddr);
    if (needWriteBDB) {
        uint64_t readAddr = elem.readAddr;
        uint8_t *dataPtr = elem.data->data();
        for (auto bdbId : elem.bdbIds) {
            m_bdb->WriteData(bdbId, readAddr, m_config.BRIDGE_DATA_BUFFER_DATA_SIZE, dataPtr);
            readAddr += m_config.BRIDGE_DATA_BUFFER_DATA_SIZE;
            dataPtr += m_config.BRIDGE_DATA_BUFFER_DATA_SIZE;
        }
    }

    m_srfb->ReleaseEntry(elem.rfbId);

    if (IsScatterReq(bpqEntry->tileArg.op)) {
        ++bpqEntry->resolveReadCounter;
        if (bpqEntry->resolveReadCounter == bpqEntry->readReqs.size()) {
            bpqEntry->pairStatus = ExecStatus::COMPLETE;
            bpqEntry->completeCycle = GetSim()->getCycles();
        }
    }
}

/* ========== Write Coalesce Buffer ========== */
WriteCoalesceBuffer::WriteCoalesceBuffer(WriteCoalesceBufferConfig& config)
    : m_bdb(config.bdb), m_sim(config.sim), m_outStandingReqNum(config.reqNum), m_bdbDataSize(config.bdbDataSize),
      id(config.id), m_pmuData(config.pmuData), m_memUnit(config.memUnit)
{
    this->pTileUtils = config.util;
    this->m_wcbQ.resize(config.reqNum);
    for (size_t i = 0; i < config.reqNum; ++i) {
        this->m_wcbQ[i].entryId = i;
    }
    for (auto& entry : m_wcbQ) {
        entry.data.resize(MAX_TILE_DATA_BYTE);
    }
}

void WriteCoalesceBuffer::Work()
{
    ResolveEntry();
    if (m_memUnit == MemoryUnit::GM) {
        ReceiveGmWriteResp();
    } else if (m_memUnit == MemoryUnit::TR) {
        ReceiveTileWriteResp();
    } else {
        LOG_ERROR_M(Unit::TMA, Stage::NA) << "[Bridge Table]: Unsupport direction!";
        ASSERT(false);
    }

    SendWriteReq();
    CollectData();
    SplitReadBDBReq();
    ReceiveSwitchNetReq();
    ReceiveWriteReqFromBPQ();
}

void WriteCoalesceBuffer::Xfer()
{
}

void WriteCoalesceBuffer::Reset()
{
}

SimSys* WriteCoalesceBuffer::GetSim()
{
    return m_sim;
}

void WriteCoalesceBuffer::Flush(FlushBus &bus)
{
    auto needFlush = [&bus](WCBEntry& entry) {
        return entry.stid == bus.req.stid && LessEqual(bus.req.bid, entry.bid);
    };
    for (uint32_t id = 0; id < m_wcbQ.size(); ++id) {
        WCBEntry& entry = m_wcbQ[id];
        if (!entry.vld) {
            continue;
        }
        if (!needFlush(entry)) {
            continue;
        }

        auto it = std::find(m_pendingReqFifo.cbegin(), m_pendingReqFifo.cend(), id);
        if (it != m_pendingReqFifo.cend()) {
            m_pendingReqFifo.erase(it);
        }
        if (m_memUnit == MemoryUnit::GM && entry.op == TileOp::MGATHER) {
            /* Victim BDB */
            continue;
        }
        if (entry.status == WCBEntryStatus::TRANSMITED || entry.status == WCBEntryStatus::FLUSHED) {
            LOG_INFO_M(Unit::TMA, Stage::NA) << "Flush write entry" << std::dec << id;
            entry.status = WCBEntryStatus::FLUSHED;
        } else {
            entry.vld = false;
            --m_aliveWCBEntryCounter;
        }
    }
}

void WriteCoalesceBuffer::ReportStat()
{
}

void WriteCoalesceBuffer::PrintStatus()
{
    std::stringstream ss;
    if (m_memUnit == MemoryUnit::TR) {
        ss << "NWCB";
    } else {
        ss << "SWCB";
    }
    ss << " size:" << std::dec << m_aliveWCBEntryCounter;
    LOG_DEBUG_M(Unit::TMA, Stage::WCB) << ss.str();
    for (uint32_t id = 0; id < m_wcbQ.size(); ++id) {
        WCBEntry& entry = m_wcbQ[id];
        if (!entry.vld) {
            continue;
        }

        LOG_DEBUG_M(Unit::TMA, Stage::WCB) << "    |--" << std::dec << id << ":BPQ" << entry.bpqId << "--"
            << entry.status;
    }
}

void WriteCoalesceBuffer::ReceiveWriteReqFromBPQ()
{
    while (!m_bpqWCBAllocQ->Empty()) {
        WCBEntry entry = m_bpqWCBAllocQ->Front();
        bool found = false;
        uint64_t entryId = 0;

        for (WCBEntry& qEntry : m_wcbQ) {
            if (!qEntry.vld) {
                found = true;
                qEntry = entry;
                qEntry.vld = true;
                qEntry.entryId = entryId;
                qEntry.memDir = m_memUnit;
                qEntry.startCycle = GetSim()->getCycles();
                if (m_memUnit == MemoryUnit::TR && IsGatherReq(qEntry.op)) {
                    qEntry.status = WCBEntryStatus::WAIT_READ;
                }
                ++m_aliveWCBEntryCounter;
                m_bpqWCBAllocQ->Pop();
                m_pendingReqFifo.push_back(entryId);
                LOG_INFO_M(Unit::TMA, Stage::NA)
                    << (m_memUnit == MemoryUnit::TR ? "[NWCB]:" : "[SWCB]:")
                    << "push entry id " << std::dec << entryId << "into bidFifo";
                if (m_memUnit == MemoryUnit::TR) {
                    ASSERT(entryId == qEntry.entryId);
                }
                break;
            }
            ++entryId;
        }

        ASSERT(found && "WCB received too much data!");
    }
}

void WriteCoalesceBuffer::ReceiveSwitchNetReq()
{
    if (m_memUnit != MemoryUnit::TR) {
        return;
    }

    while (!m_switchNetwork->empty()) {
        SwitchInfo switchInfo = m_switchNetwork->read();
        bool found = false;
        for (WCBEntry& entry : m_wcbQ) {
            if (!entry.vld || entry.bpqId != switchInfo.bpqId) {
                continue;
            }

            found = true;
            ASSERT(entry.status == WCBEntryStatus::WAIT_READ);
            /* FIXME: FP4 is not supported currently, and how to combine halfwords needs to be considered. */
            ASSERT(!DoubleEqual(entry.writeReq.dataSize, HALF_BYTE_DATA_WIDTH));

            /* Fill the data in the wcb data based on the switchMask. */
            FillDataBySwitchMask(switchInfo.switchMask, switchInfo.data, entry.data);
            for (auto it : switchInfo.switchMask) {
                entry.receiveDataSize += it.second.second;
            }

            if (entry.receiveDataSize == entry.writeReq.validWriteSize) {
                entry.status = WCBEntryStatus::DATA_READY;
                entry.dataReadyCycle = GetSim()->getCycles();
            }
        }
        ASSERT(found && "Receive invalid read update req.");
    }
}

static void SplitNormGMReadBDBReq(WCBEntry& entry, uint64_t bdbDataSize)
{
    WriteReq& writeReq = entry.writeReq;
    ASSERT(writeReq.writeAddr != -1ULL);
    ASSERT(writeReq.writeSize != -1ULL);
    ASSERT(writeReq.baseAddrGM != -1ULL);
    ASSERT(writeReq.validWriteSize != -1ULL);
    ASSERT(!DoubleEqual(writeReq.dataSize, -1.0F));
    entry.writeMask = writeReq.writeMask;

    uint64_t readAddr = writeReq.baseAddrGM + writeReq.d2TRIdx * writeReq.strideGM +
                        writeReq.d1TRIdx * writeReq.dataSize;
    uint64_t endAddr = readAddr + writeReq.validWriteSize;

    uint64_t writeOffset = 0;
    for (; writeOffset < entry.writeMask.size(); ++writeOffset) {
        if (entry.writeMask.at(writeOffset)) {
            break;
        }
    }

    while (readAddr < endAddr) {
        uint32_t readSize = bdbDataSize;
        uint64_t nextAlignSrcAddr = (readAddr + bdbDataSize) & ~(bdbDataSize - 1);
        if (readAddr + readSize > nextAlignSrcAddr) {
            readSize = nextAlignSrcAddr - readAddr;
        }
        if (readAddr + readSize > endAddr) {
            readSize = endAddr - readAddr;
        }
        BDBReadReq req = {
            .bpqId = entry.bpqId,
            .readAddr = readAddr,
            .readSize = readSize,
            .writeOffset = writeOffset
        };
        entry.bdbReadReqBuffer.push(req);
        writeOffset += readSize;
        readAddr += readSize;
    }
}

static void SplitNormTRReadBDBReq(WCBEntry& entry, uint64_t bdbDataSize)
{
    ASSERT(entry.writeReq.readAddr != -1ULL);
    ASSERT(entry.writeReq.validWriteSize != -1ULL);
    double srcToDstRatio = entry.writeReq.srcDataSize / entry.writeReq.dataSize;
    uint64_t readAddr = entry.writeReq.readAddr;
    uint64_t endAddr = readAddr + static_cast<uint64_t>(entry.writeReq.validWriteSize * srcToDstRatio);
    entry.writeMask = entry.writeReq.writeMask;

    uint64_t writeOffset = 0;
    for (; writeOffset < entry.writeReq.writeMask.size(); ++writeOffset) {
        if (entry.writeReq.writeMask.at(writeOffset)) {
            break;
        }
    }

    while (readAddr < endAddr) {
        uint32_t readSize = bdbDataSize;
        uint64_t nextAlignSrcAddr = (readAddr + bdbDataSize) & ~(bdbDataSize - 1);
        if (readAddr + readSize > nextAlignSrcAddr) {
            readSize = nextAlignSrcAddr - readAddr;
        }
        if (readAddr + readSize > endAddr) {
            readSize = endAddr - readAddr;
        }
        BDBReadReq req = {
            .bpqId = entry.bpqId,
            .readAddr = readAddr,
            .readSize = readSize,
            .writeOffset = writeOffset
        };
        entry.bdbReadReqBuffer.push(req);
        writeOffset += static_cast<uint64_t>(readSize / srcToDstRatio);
        readAddr += readSize;
    }
}

static void SplitNZ2NDReadBDBReq(WCBEntry& entry, uint64_t bdbDataSize)
{
    constexpr uint32_t rowNumInRingTrans = FRACTAL_ROW_NUM / 2;
    constexpr uint32_t ringTransSize = FRACTAL_ROW_BYTES * rowNumInRingTrans;
    WriteReq& writeReq = entry.writeReq;
    uint64_t writeSize = writeReq.writeSize;
    ASSERT(writeReq.writeSize != -1ULL);
    ASSERT(writeReq.d1GMIdx != -1ULL);
    ASSERT(writeReq.d2GMIdx != -1ULL);
    ASSERT(writeReq.d1GM != -1ULL);
    ASSERT(writeReq.d2TR != -1ULL);
    ASSERT(!DoubleEqual(writeReq.dataSize, -1.0F));
    ASSERT(writeReq.baseAddrTR0 != -1ULL);
    ASSERT(writeReq.strideGM != -1ULL);
    uint64_t writeOffset = 0;
    for (; writeOffset < entry.writeReq.writeMask.size(); ++writeOffset) {
        if (entry.writeReq.writeMask.at(writeOffset)) {
            break;
        }
    }
    /*
     * d1RingTransIdx indicates the current write position in the D1 direction as the nth RingTrans.
     * d2RingTransIdx indicates the current write position in the D2 direction as the nth RingTrans.
     */
    uint64_t d2RingTransIdx = writeReq.d2GMIdx / rowNumInRingTrans;
    uint64_t d1RingTransIdx = writeReq.d1GMIdx / FRACTAL_ROW_BYTES;
    uint64_t ringTransD2Offset = writeReq.d2GMIdx % rowNumInRingTrans;
    uint64_t ringTransD1Offset = writeReq.d1GMIdx % FRACTAL_ROW_BYTES;
    uint64_t d1GMOffset = writeReq.d1GMIdx;
    uint64_t rowValidSize = writeReq.d1GM * writeReq.dataSize;
    uint64_t ringTransCountInD2 = writeReq.d2TR / rowNumInRingTrans;
    while (writeOffset < writeSize) {
        if (d1GMOffset < rowValidSize) {
            uint64_t baseRingTransAddr = writeReq.baseAddrTR0 + d2RingTransIdx * ringTransSize +
                                         d1RingTransIdx * ringTransCountInD2 * ringTransSize;
            uint64_t baseRingTransOffset = ringTransD2Offset * FRACTAL_ROW_BYTES + ringTransD1Offset;
            uint64_t readAddr = baseRingTransAddr + baseRingTransOffset;
            uint64_t alignReadAddr = readAddr & ~(FRACTAL_ROW_BYTES - 1);
            uint64_t readSize = std::min(alignReadAddr + FRACTAL_ROW_BYTES - readAddr,
                                         bdbDataSize - writeOffset);
            BDBReadReq req = {
                .bpqId = entry.bpqId,
                .readAddr = readAddr,
                .readSize = readSize,
                .writeOffset = writeOffset
            };
            entry.bdbReadReqBuffer.push(req);
            for (uint64_t i = 0; i < readSize; ++i) {
                uint64_t pos = writeOffset + i;
                entry.writeMask.at(pos) = true;
            }
            d1GMOffset += readSize;
            writeOffset += readSize;
            ++d1RingTransIdx;
            ringTransD1Offset = 0;
        } else {
            d1GMOffset = 0;
            d1RingTransIdx = 0;
            ringTransD1Offset = 0;
            writeOffset += (writeReq.strideGM - static_cast<uint64_t>(writeReq.d1GM * writeReq.dataSize));
            if (ringTransD2Offset == rowNumInRingTrans - 1) {
                ++d2RingTransIdx;
            } else {
                ++ringTransD2Offset;
            }
        }
    }
}

/* ND2NZ & DN2ZN */
static void SplitGatherReadBDBReq(WCBEntry& entry, uint64_t srcD1Size, uint64_t bdbDataSize)
{
    ASSERT(entry.writeReq.readAddr != -1ULL);
    ASSERT(srcD1Size != -1ULL);
    constexpr uint32_t d2NumInRingTrans = FRACTAL_ROW_NUM / 2;
    uint64_t writeOffset = 0;
    uint64_t readAddr = entry.writeReq.readAddr;
    double dataScaleFactor = entry.writeReq.srcDataSize / entry.writeReq.dataSize;
    for (uint32_t i = 0; i < d2NumInRingTrans; ++i) {
        readAddr = entry.writeReq.readAddr + i * srcD1Size;
        uint64_t rowEndAddr = readAddr + static_cast<uint64_t>(FRACTAL_ROW_BYTES * dataScaleFactor);
        while (readAddr < rowEndAddr) {
            uint64_t nextCacheLineAddr = (readAddr + bdbDataSize) & ~(bdbDataSize - 1);
            uint64_t readSize = static_cast<uint64_t>(FRACTAL_ROW_BYTES * dataScaleFactor);
            if (readAddr + readSize > rowEndAddr) {
                readSize = rowEndAddr - readAddr;
            }
            if (readAddr + readSize > nextCacheLineAddr) {
                readSize = nextCacheLineAddr - readAddr;
            }
            BDBReadReq req = {
                .bpqId = entry.bpqId,
                .readAddr = readAddr,
                .readSize = readSize,
                .writeOffset = writeOffset
            };
            entry.bdbReadReqBuffer.push(req);
            readAddr += readSize;
            writeOffset += static_cast<uint64_t>(readSize / dataScaleFactor);
        }
    }
}

/* ND2ZN & DN2NZ */
static void SplitScatterReadBDBReq(WCBEntry& entry, uint64_t srcD1Size, uint64_t bdbDataSize)
{
    ASSERT(entry.writeReq.readAddr != -1ULL);
    ASSERT(!DoubleEqual(entry.writeReq.dataSize, -1.0F));
    ASSERT(srcD1Size != -1ULL);
    uint32_t d1NumInRingTrans = FRACTAL_ROW_BYTES / entry.writeReq.dataSize;
    uint32_t ringTransD2Size = FRACTAL_ROW_NUM / 2 * entry.writeReq.srcDataSize;
    uint64_t readAddr = entry.writeReq.readAddr;
    for (uint32_t i = 0; i < d1NumInRingTrans; ++i) {
        readAddr = entry.writeReq.readAddr + i * srcD1Size;
        uint64_t writeOffset = i * entry.writeReq.dataSize;
        uint64_t rowEndAddr = readAddr + ringTransD2Size;
        while (readAddr < rowEndAddr) {
            uint64_t nextCacheLineAddr = (readAddr + bdbDataSize) & ~(bdbDataSize - 1);
            uint64_t readSize = ringTransD2Size;
            if (readAddr + readSize > rowEndAddr) {
                readSize = rowEndAddr - readAddr;
            }
            if (readAddr + readSize > nextCacheLineAddr) {
                readSize = nextCacheLineAddr - readAddr;
            }
            ASSERT(IsDivisible(readSize, entry.writeReq.srcDataSize));
            BDBReadReq req;
            req.bpqId = entry.bpqId;
            req.readAddr = readAddr;
            req.readSize = readSize;
            req.writeOffset = writeOffset;
            req.elementNum = readSize / entry.writeReq.srcDataSize;
            entry.bdbReadReqBuffer.push(req);
            readAddr += readSize;
            writeOffset += req.elementNum * FRACTAL_ROW_BYTES;
        }
    }
}

void WriteCoalesceBuffer::SplitReadBDBReq()
{
    for (WCBEntry& entry : m_wcbQ) {
        if (!entry.vld || entry.status != WCBEntryStatus::NO_START) {
            continue;
        }
        if (entry.layout == LayOut::NORM && entry.op == TileOp::TLOAD) {
            SplitNormGMReadBDBReq(entry, m_bdbDataSize);
        } else if (entry.layout == LayOut::NORM && entry.op == TileOp::TSTORE) {
            SplitNormTRReadBDBReq(entry, m_bdbDataSize);
        } else if (entry.layout == LayOut::ND2NZ && entry.op == TileOp::TSTORE) {
            SplitNZ2NDReadBDBReq(entry, m_bdbDataSize);
        } else if ((entry.layout == LayOut::ND2NZ || entry.layout == LayOut::DN2ZN) && entry.op == TileOp::TLOAD) {
            SplitGatherReadBDBReq(entry, entry.writeReq.strideGM, m_bdbDataSize);
        } else if ((entry.layout == LayOut::ND2ZN || entry.layout == LayOut::DN2NZ) && entry.op == TileOp::TLOAD) {
            SplitScatterReadBDBReq(entry, entry.writeReq.strideGM, m_bdbDataSize);
        } else if ((entry.layout == LayOut::ND2NZ || entry.layout == LayOut::DN2ZN) && entry.op == TileOp::TMOV) {
            SplitGatherReadBDBReq(entry, entry.writeReq.d1TR * entry.writeReq.srcDataSize, m_bdbDataSize);
        } else if ((entry.layout == LayOut::ND2ZN || entry.layout == LayOut::DN2NZ) && entry.op == TileOp::TMOV) {
            SplitScatterReadBDBReq(entry, entry.writeReq.d1TR * entry.writeReq.srcDataSize, m_bdbDataSize);
        } else if (entry.layout == LayOut::NORM && entry.op == TileOp::TMOV) {
            SplitNormTRReadBDBReq(entry, m_bdbDataSize);
        } else {
            LOG_ERROR_M(Unit::TMA, Stage::NA) << "[Bridge Table]: Unsupport layout!";
            abort();
        }

        entry.status = WCBEntryStatus::INTERGRATING;
        entry.intergratingCycle = GetSim()->getCycles();
    }
}

// Read bits (<=64) from any bitOffset into a uint64 (little-endian bit ordering: the lower bits come first)
static inline uint64_t ReadBitsLE(const std::vector<uint8_t>& src, uint64_t bitOffset, uint32_t bits)
{
    if (bits == 0) {
        return 0;
    }

    if (bits > 64) {
        throw std::invalid_argument("bits must be <= 64");
    }

    uint64_t value = 0;
    for (uint32_t i = 0; i < bits; ++i) {
        uint64_t byteIdx = (bitOffset + i) / 8;
        uint32_t bitIdx = (bitOffset + i) % 8;
        uint8_t bitMask = static_cast<uint8_t>(1U << bitIdx);

        ASSERT(byteIdx < src.size());
        if ((src[byteIdx] & bitMask) != 0) {
            value |= (static_cast<uint64_t>(1) << i);
        }
    }

    return value;
}

// Writes bits (<=64) to any bit offset (little-endian bit ordering)
static inline void WriteBitsLE(std::vector<uint8_t>& dst, uint64_t bitOffset, uint32_t bits, uint64_t value)
{
    if (bits == 0) {
        return;
    }
    if (bits > 64) {
        throw std::invalid_argument("bits must be <= 64");
    }

    for (uint32_t i = 0; i < bits; ++i) {
        uint64_t byteIdx = (bitOffset + i) / 8;
        uint32_t bitIdx = (bitOffset + i) % 8;
        uint8_t bitMask = static_cast<uint8_t>(1U << bitIdx);

        ASSERT(byteIdx < dst.capacity());
        // Set bit if value has this bit set
        if (((value >> i) & 1) != 0) {
            dst[byteIdx] |= bitMask;
        } else {
            dst[byteIdx] &= ~bitMask;
        }
    }
}

void WriteCoalesceBuffer::CollectData()
{
    auto normalCollectData = [](WCBEntry& entry, std::vector<uint8_t>& data, BDBReadReq& req) {
        if (entry.layout == LayOut::ND2ZN || entry.layout == LayOut::DN2NZ) {
            for (uint64_t i = 0; i < req.elementNum; ++i) {
                uint64_t readOffset = i * (entry.writeReq.srcDataSize);
                ASSERT(data.size() >= (readOffset + entry.writeReq.srcDataSize));
                auto readStart = data.begin() + readOffset;
                auto readEnd = readStart + entry.writeReq.srcDataSize;
                auto writeOffset = req.writeOffset + i * (FRACTAL_ROW_BYTES);
                auto writeStart = entry.data.begin() + writeOffset;
                auto copySize = entry.writeReq.dataSize;
                auto freeMemSize = entry.data.capacity() - writeOffset;
                ASSERT(copySize <= freeMemSize && "Insufficient free memory!");
                std::copy(readStart, readEnd, writeStart);
            }
        } else {
            auto copySize = data.size();
            ASSERT(req.writeOffset < entry.data.capacity());
            auto freeMemSize = entry.data.capacity() - req.writeOffset;
            ASSERT(copySize <= freeMemSize && "Insufficient free memory!");
            std::copy(data.begin(), data.end(), entry.data.begin() + req.writeOffset);
        }
    };
    auto tmovCollectData = [](WCBEntry& entry, std::vector<uint8_t>& data, BDBReadReq& req) {
        if (entry.layout == LayOut::ND2ZN || entry.layout == LayOut::DN2NZ) {
            for (uint64_t i = 0; i < req.elementNum; ++i) {
                uint64_t readOffset = i * (entry.writeReq.srcDataSize);
                uint64_t srcBits = entry.writeReq.srcDataSize * BITS_IN_BYTE;
                ASSERT(req.readSize >= (readOffset + entry.writeReq.srcDataSize));
                uint64_t srcData = ReadBitsLE(data, i * srcBits, srcBits);
                auto writeOffset = req.writeOffset + i * (FRACTAL_ROW_BYTES);
                WriteBitsLE(entry.data, writeOffset * BITS_IN_BYTE, entry.writeReq.dataSize * BITS_IN_BYTE, srcData);
            }
        } else {
            ASSERT(req.writeOffset < entry.data.capacity());
            uint64_t elementNum = req.readSize / entry.writeReq.srcDataSize;
            uint64_t freeMemSize = entry.data.capacity() - req.writeOffset;
            uint64_t writeSize = elementNum * entry.writeReq.dataSize;
            ASSERT(writeSize <= freeMemSize && "Insufficient free memory!");

            for (uint64_t i = 0; i < elementNum; ++i) {
                uint64_t srcBits = entry.writeReq.srcDataSize * BITS_IN_BYTE;
                uint64_t srcData = ReadBitsLE(data, i * srcBits, srcBits);
                uint64_t writeOffset = req.writeOffset + i * entry.writeReq.dataSize;
                WriteBitsLE(entry.data, writeOffset * BITS_IN_BYTE, entry.writeReq.dataSize * BITS_IN_BYTE, srcData);
            }
        }
    };

    for (WCBEntry& entry : m_wcbQ) {
        if (!entry.vld || entry.status != WCBEntryStatus::INTERGRATING) {
            continue;
        }
        if (entry.bdbReadReqBuffer.empty()) {
            entry.status = WCBEntryStatus::DATA_READY;
            entry.dataReadyCycle = GetSim()->getCycles();
            continue;
        }
        BDBReadReq& req = entry.bdbReadReqBuffer.front();
        /* TODO: using q to send read request to bdb */
        std::vector<uint8_t> data = m_bdb->ReadData(req.readAddr, req.readSize, m_bdbDataSize);
        if (entry.op == TileOp::TMOV) {
            tmovCollectData(entry, data, req);
        } else {
            normalCollectData(entry, data, req);
        }
        entry.bdbReadReqBuffer.pop();
    }
}

void WriteCoalesceBuffer::SendWriteReqToMem(size_t idx, WCBEntry& entry)
{
    GFUMemReq req;
    req.vld = true;
    req.size = entry.writeReq.writeSize;
    req.is_store = true;
    req.is_inst  = false;
    req.addr = entry.writeReq.writeAddr;
    req.tid = SetReqTokenId(false, id, entry.entryId);
    req.lsuTypeId = LSUType::BRIDGE_TABLE;
    for (uint64_t i = 0; i < (sizeof(req.bitMask) / sizeof(req.bitMask[0])); ++i) {
        uint64_t chunk = 0;
        for (uint64_t j = 0; j < sizeof(req.bitMask[0]) * CHAR_BIT; ++j) {
            uint64_t pos = i * sizeof(req.bitMask[0]) * CHAR_BIT + j;
            if (entry.writeMask.at(pos)) {
                chunk |= (1ULL << j);
            }
        }
        req.bitMask[i] = chunk;
    }
    auto copySize = std::distance(entry.data.begin(), entry.data.begin() + entry.writeReq.writeSize);
    auto freeMemSize = sizeof(req.data8b);
    ASSERT(freeMemSize >= static_cast<decltype(freeMemSize)>(copySize) && "Insufficient free memory!");
    std::copy(entry.data.begin(), entry.data.begin() + entry.writeReq.writeSize, req.data8b);

    m_wcbMemReqQ->Write(req);

    entry.status = WCBEntryStatus::TRANSMITED;
    entry.txedCycle = GetSim()->getCycles();
    m_pendingReqFifo.erase(m_pendingReqFifo.begin() + idx);
}

TTransTileRegStReq WriteCoalesceBuffer::CreateTileStReq(uint64_t reqId, uint64_t addr, const std::vector<uint8_t>& data,
                                                        uint64_t size, DataMask mask, uint32_t stid) const
{
    TTransTileRegStReq req = TTransTileRegStReq();
    // 调整地址, 按照 Tile 的地址粒度来调整
    req.SetReqId(reqId);
    req.SetDest(addr);
    req.SetData(data.data(), size);
    req.SetSize(size);
    req.SetDataMask(mask);
    req.SetStid(stid);

    return req;
}

bool WriteCoalesceBuffer::SendWriteReqToTileReg(size_t idx, WCBEntry& entry)
{
    if (pTileUtils->IsRingOrXbarMode(false) && !HasValidSpbWriteEntry()) {
        return false;
    }
    uint64_t writeAddr = 0;
    bool tileBankArbSuaccelss = true;
    if (GetSim()->core->configs.singleTierMode && (needWriteRFArb < 0)) {
        tileBankArbSuaccelss = GetSim()->core->CanWriteTileReg(MachineType::TMA);
        if (tileBankArbSuaccelss) {
            GetSim()->core->AcquireWriteTileReg(MachineType::TMA, GetSim()->core->GetConfig().tmaWriteOccTileregLat);
            if (needWriteRFArb < 0) {  // needWriteRFArb为-1表示这是第一次仲裁
                needWriteRFArb = 6;
            }
        }
    }
    if (!tileBankArbSuaccelss && (needWriteRFArb < 0)) {
        return false;
    }
    if (needWriteRFArb >= 0) {
        needWriteRFArb--;
    }
    constexpr uint64_t tileAddr256BAlignMask = (static_cast<uint64_t>(MAX_TILE_DATA_BYTE) - 1);
    DataMask writeDataMask = DataMask(pTileUtils->configs.mask_element_size);

    if (!IsMemoryModeReq(entry.op)) {
        ASSERT((entry.writeReq.writeAddr & tileAddr256BAlignMask) == 0);
        writeAddr = entry.writeReq.writeAddr;
        writeDataMask.Set(0, entry.writeReq.validWriteSize);
    } else {
        writeAddr = entry.writeReq.writeAddr & ~(tileAddr256BAlignMask - 1);
        uint32_t offset = entry.writeReq.writeAddr - writeAddr;
        for (bool mask : entry.writeReq.writeMask) {
            if (mask) {
                writeDataMask.Set(offset, offset + entry.writeReq.dataSize);
            }
            offset += entry.writeReq.dataSize;
        }
    }

    uint32_t reqId = SetTileReqId(false, false, entry.entryId);
    VecTileRegStReq req = CreateTileStReq(reqId, writeAddr, entry.data, MAX_TILE_DATA_BYTE,
                                          writeDataMask, entry.stid);
    entry.tileReqId = req.GetReqId();
    if (pTileUtils->IsRingOrXbarMode(false) && !HasValidSpbWriteEntry(req.GetDest(),
        pTileUtils->configs.strict_closest_injection)) {
        return false;
    }
    if (pTileUtils->IsRingOrXbarMode(false)) {
        shared_ptr<CrossStation> station = nullptr;
        if (pTileUtils->configs.strict_closest_injection) {
            station = GetFreeWriteStation(req.GetDest(), true);
        } else if (pTileUtils->configs.closest_injection) {
            station = GetFreeWriteStation(req.GetDest(), false);
        } else {
            station = GetFreeWriteStation();
        }
        shared_ptr<Request> pkt = station->Rxdat(req.GetDest(), req.GetStid(), 1);
        pkt->SetSize(req.GetSize());
        pkt->SetBufId(req.GetReqId());
        pkt->SetPEType(MachineType::TMA);
        uint8_t data[MAX_TILE_DATA_BYTE];
        uint8_t* stData = req.GetData();
        for (size_t i = 0; i < MAX_TILE_DATA_BYTE; ++i) {
            data[i] = stData[i];
        }
        pkt->SetData(data);
        pkt->dmask = req.GetDataMask();
        station->WriteSpb(pkt);
    } else {
        GetSim()->core->tmaRealWriteRFReqCnt++;
        m_wcbTileRegStReqQ->Write(req);
    }

    entry.status = WCBEntryStatus::TRANSMITED;
    entry.txedCycle = GetSim()->getCycles();
    m_pendingReqFifo.erase(m_pendingReqFifo.begin() + idx);

    std::stringstream ss;
    ss << "SendWriteReqToTileReg: Write tile data: ";
    for (uint32_t i = 0; i < MAX_TILE_DATA_BYTE; ++i) {
        ss << std::hex << static_cast<uint64_t>(entry.data[i]) << ", ";
    }
    ss << std::endl;
    LOG_DEBUG_M(Unit::TMA, Stage::WCB) << ss.str();

    return true;
}

void WriteCoalesceBuffer::SendWriteReq()
{
    m_hasReadyWrite = false;
    m_ringBlocked = false;
    if (m_pendingReqFifo.empty()) {
        return;
    }

    size_t maxReqs = pTileUtils->configs.nonblocking_tma_processing ? m_pendingReqFifo.size() : 1;
    for (size_t i = 0; i < maxReqs; ++i) {
        uint64_t entryId = m_pendingReqFifo[i];
        WCBEntry& entry = m_wcbQ[entryId];
        ASSERT(entryId == entry.entryId);
        if (!entry.vld || entry.status != WCBEntryStatus::DATA_READY) {
            continue;
        }
        LOG_DEBUG_M(Unit::TMA, Stage::WCB) << ((m_memUnit == MemoryUnit::GM) ? "[SWCB]" : "[NWCB]")
                                         << ": send write req for core id:" << std::dec << entryId << " bpq id:"
                                         << entry.bpqId << " entry id:" << entryId;
        m_hasReadyWrite = true;
        if (m_memUnit == MemoryUnit::GM) {
            SendWriteReqToMem(i, entry);
            break;
        } else if (m_memUnit == MemoryUnit::TR) {
            m_ringBlocked = !SendWriteReqToTileReg(i, entry);
            if (!m_ringBlocked) {
                break;
            }
        } else {
            LOG_ERROR_M(Unit::TMA, Stage::NA) << "[Bridge Table]: Unsupport direction!";
            abort();
        }
    }
}

void WriteCoalesceBuffer::ReceiveGmWriteResp()
{
    if (m_wcbMemRspQ->Empty()) {
        return;
    }
    GFUMemReq&& response = m_wcbMemRspQ->Front();
    uint64_t repId = -1;
    uint32_t coreId = -1;
    bool isLoad = false;
    // 不处理fake消息
    if (response.instTypeMap != 0) {
        return;
    }
    UnpackReqTokenId(response.tid, isLoad, coreId, repId);
    if (isLoad) { // Only process Write
        return;
    }
    if (coreId != id) {
        return;
    }
    m_wcbMemRspQ->Pop();
    ASSERT(repId < m_wcbQ.size());
    WCBEntry& entry = m_wcbQ[repId];
    ASSERT(entry.status == WCBEntryStatus::TRANSMITED);
    entry.status = WCBEntryStatus::COMPLETE;
    entry.busSendCycle = response.busSendCycle;
    entry.receiveDBIDCycle = response.receiveDBIDCycle;
    entry.completeCycle = GetSim()->getCycles();

    if (m_memUnit == MemoryUnit::GM) {
        m_pmuData.swcbEntryWriteTotalCycles += (entry.completeCycle - entry.txedCycle);
        m_pmuData.swcbEntryNum++;
    }
}

void WriteCoalesceBuffer::ReceiveTileWriteResp()
{
    bool noResp = true;
    std::unordered_set<uint64_t> tileResIds;
    if (pTileUtils->IsRingOrXbarMode(false)) {
        for (auto &station : stations) {
            if (station->HasNoResp(1)) {
                continue;
            }
            shared_ptr<Request> pkt = station->GetDataComReqFront(1);
            if (IsVscatterRingReq(pkt)) {
                continue;
            }
            uint32_t respId = pkt->GetBufId();
            bool readFromBPQ = false;
            bool readFromFake = false;
            uint32_t reqId = 0;
            UnpackTileReqId(respId, readFromBPQ, readFromFake, reqId);

            if (readFromBPQ || readFromFake) {
                return;
            }
            station->PopDataComReq(1);
            tileResIds.insert(reqId);
            noResp = false;
            LOG_INFO_M(Unit::TMU, Stage::NA) << "Bridge recv store resp " << *pkt;
        }
    } else {
        while (!m_tileRegWCBWrResQ->Empty()) {
            TileRegTTransLdRes&& res = m_tileRegWCBWrResQ->Front();
            uint32_t respId = res.GetRespId();
            bool readFromBPQ = false;
            bool readFromFake = false;
            uint32_t reqId = 0;

            UnpackTileReqId(respId, readFromBPQ, readFromFake, reqId);
            if (readFromBPQ || readFromFake) {
                return;
            }
            m_tileRegWCBWrResQ->Pop();
            tileResIds.insert(reqId);
            noResp = false;
        }
    }
    if (noResp) {
        return;
    }

    for (WCBEntry& entry : m_wcbQ) {
        if (!entry.vld) {
            continue;
        }
        if (entry.status != WCBEntryStatus::TRANSMITED && entry.status != WCBEntryStatus::FLUSHED) {
            continue;
        }

        if (tileResIds.count(entry.tileReqId) != 0) {
            tileResIds.erase(entry.tileReqId);
            LOG_DEBUG_M(Unit::TMA, Stage::WCB) << "[NWCB]: receive tile write response for core id:" << std::dec << id
                                             << " bpq id:" << entry.bpqId;
            if (entry.status == WCBEntryStatus::FLUSHED) {
                entry.vld = false;
                --m_aliveWCBEntryCounter;
                /* TODO: add flush pipeview */
                continue;
            }
            entry.status = WCBEntryStatus::COMPLETE;
            entry.completeCycle = GetSim()->getCycles();
        }
    }
    ASSERT(tileResIds.empty() && "[Bridge Table]: receive unkown tile reg response.");
}

void WriteCoalesceBuffer::PrintPipeViewLog(uint64_t bpqId, uint64_t stid)
{
    if (m_pipeviewInfo.count(bpqId) == 0) {
        return;
    }
    for (InstPipeViewPtr instInfo : m_pipeviewInfo.at(bpqId)) {
        GetSim()->GetViewManager(stid)->RecordMinst(instInfo);
    }

    m_pipeviewInfo.erase(bpqId);
}

void WriteCoalesceBuffer::PrintPipeViewLog(WCBEntry& entry)
{
    InstPipeViewPtr instInfo = std::make_shared<InstPipeViewInfo>();
    instInfo->bid = entry.bid.val;
    instInfo->cycleInfo = std::make_shared<CycleInfo>();
    instInfo->cycleInfo->startCycle = entry.startCycle;
    instInfo->cycleInfo->intergratingCycle = entry.intergratingCycle;
    instInfo->cycleInfo->dataReadyCycle = entry.dataReadyCycle;
    instInfo->cycleInfo->txedCycle = entry.txedCycle;
    instInfo->cycleInfo->busSendCycle = entry.busSendCycle;
    instInfo->cycleInfo->receiveDBIDCycle = entry.receiveDBIDCycle;
    instInfo->cycleInfo->bridgeCompleteCycle = entry.completeCycle;
    instInfo->cycleInfo->retireCycle = entry.completeCycle + 1;
    instInfo->label = entry.Dump();

    if (IsMemoryModeReq(entry.op) && m_memUnit == MemoryUnit::TR) {
        /* BPQ will not wait for the victim of SWCB request to end before it concludes. */
        m_pipeviewInfo[entry.bpqId].push_back(instInfo);
    } else {
        GetSim()->GetViewManager(entry.stid)->RecordMinst(instInfo);
    }
}

void WriteCoalesceBuffer::ResolveEntry()
{
    for (uint32_t id = 0; id < m_wcbQ.size(); ++id) {
        WCBEntry& entry = m_wcbQ[id];
        if (!entry.vld || entry.status != WCBEntryStatus::COMPLETE) {
            continue;
        }
        PrintPipeViewLog(entry);
        if (IsMemoryModeReq(entry.op) && entry.memDir == MemoryUnit::GM) {
            /* Victim resolve */
            m_bpq.lock()->ReleaseSWCBAvailCnt();
        } else {
            m_wcbBpqQ->Write(entry);
        }
        entry.vld = false;
        --m_aliveWCBEntryCounter;
        std::stringstream ss;
        ss << (entry.memDir == MemoryUnit::TR ? "NWCB" : "SWCB");
        ss << ": resolve wcb entry:" << std::dec << id << " bpq id:" << entry.bpqId
           << " op:" << GetTileOpName(entry.op);
        LOG_DEBUG_M(Unit::TMA, Stage::WCB) << ss.str();
    }
}

uint32_t WriteCoalesceBuffer::GetInvalidEntryCounter()
{
    return m_outStandingReqNum - m_aliveWCBEntryCounter;
}

uint32_t WriteCoalesceBuffer::GetAliveEntryCounter()
{
    return m_aliveWCBEntryCounter;
}

uint32_t WriteCoalesceBuffer::GetTxedEntryCounter()
{
    uint32_t txedEntryCounter = 0;
    for (WCBEntry& entry : m_wcbQ) {
        if (entry.vld && entry.status == WCBEntryStatus::TRANSMITED) {
            ++txedEntryCounter;
        }
    }
    return txedEntryCounter;
}

bool WriteCoalesceBuffer::IsWriteRingBlocked() const
{
    return m_ringBlocked;
}

bool WriteCoalesceBuffer::IsWriteReady() const
{
    return m_hasReadyWrite;
}

/* ========== Read Fill Buffer ========== */
ReadFillBuffer::ReadFillBuffer(ReadFillBufferConfig& config)
    : m_readGranularity(config.readGranularity), m_bdbDataSize(config.bdbDataSize), m_outStandingReqNum(config.reqNum),
      m_maxSendReadNum(config.maxSendReadNum), m_bdb(config.bdb), m_tsBDB(config.tsBDB),
      m_sim(config.sim), id(config.id), m_pmuData(config.pmuData), m_memUnit(config.memUnit)
{
    this->pTileUtils = config.util;
    this->m_rfbQ.resize(config.reqNum);
}

void ReadFillBuffer::Work()
{
    ReciveDataAndUpdateStatus();
    SendReadReq();
    ReciveReadReqFromBPQ();
}

void ReadFillBuffer::Xfer()
{
}

void ReadFillBuffer::Reset()
{
}

SimSys* ReadFillBuffer::GetSim()
{
    return m_sim;
}

void ReadFillBuffer::Flush(FlushBus &bus)
{
    auto needFlush = [&bus](RFBEntry& entry) {
        return LessEqual(bus.req.bid, entry.bid);
    };
    for (uint32_t id = 0; id < m_rfbQ.size(); ++id) {
        RFBEntry& entry = m_rfbQ[id];
        if (!entry.vld || !needFlush(entry)) {
            continue;
        }

        auto it = std::find(m_pendingReqFifo.cbegin(), m_pendingReqFifo.cend(), id);
        if (it != m_pendingReqFifo.cend()) {
            m_pendingReqFifo.erase(it);
        }
        if (entry.status == RFBEntryStatus::TRANSMITED || entry.status == RFBEntryStatus::FLUSHED) {
            LOG_INFO_M(Unit::TMA, Stage::NA) << "Flush read entry" << std::dec << id;
            entry.status = RFBEntryStatus::FLUSHED;
        } else {
            entry.vld = false;
            --m_aliveRFBEntryCounter;
        }
    }
}

void ReadFillBuffer::ReportStat()
{
}

void ReadFillBuffer::PrintStatus()
{
    std::stringstream ss;
    if (m_memUnit == MemoryUnit::TR) {
        ss << "NRFB";
    } else {
        ss << "SRFB";
    }
    ss << " size:" << std::dec << m_aliveRFBEntryCounter;
    LOG_DEBUG_M(Unit::TMA, Stage::RFB) << ss.str();
    for (uint64_t rfbId = 0; rfbId < m_rfbQ.size(); ++rfbId) {
        RFBEntry& entry = m_rfbQ[rfbId];
        if (!entry.vld) {
            continue;
        }

        LOG_DEBUG_M(Unit::TMA, Stage::RFB) << "    |--" << std::dec << rfbId << "--BPQ" << entry.bpqId << "--"
                                         << entry.status;
    }
}

void ReadFillBuffer::ReciveReadReqFromBPQ()
{
    auto allocateBDB = [this](RFBEntry& entry, uint32_t granMultiplier) {
        for (uint32_t j = 0; j < granMultiplier; ++j) {
            uint64_t bdbId = m_bdb->AllocDataBuffer(entry.addr + j * m_bdbDataSize, entry.bpqId);
            ASSERT((bdbId != static_cast<uint64_t>(-1)) && "BDB has no free space!");
            entry.bdbId.push_back(bdbId);
        }
    };

    while (!m_bpqRFBQ->Empty()) {
        RFBEntry entry = m_bpqRFBQ->Front();
        bool found = false;
        for (uint32_t i = 0; i < m_rfbQ.size(); ++i) {
            RFBEntry& rfbEntry = m_rfbQ[i];
            if (rfbEntry.vld) {
                continue;
            }

            bool needAllocateBDB = true;
            if (IsMemoryModeReq(entry.op)) {
                needAllocateBDB = false;
            }

            ASSERT((entry.size % m_bdbDataSize) == 0 && "RFB Read Size must be an integer multiple of m_bdbDataSize");
            uint32_t granMultiplier = entry.size / m_bdbDataSize;
            found = true;
            rfbEntry = entry;
            rfbEntry.vld = true;
            if (needAllocateBDB) {
                allocateBDB(rfbEntry, granMultiplier);
            }
            rfbEntry.waitCycle = GetSim()->getCycles();
            m_bpqRFBQ->Pop();
            if (entry.status == RFBEntryStatus::WAIT) {
                m_pendingReqFifo.emplace_back(i);
            }
            LOG_DEBUG_M(Unit::TMA, Stage::RFB) << ((m_memUnit == MemoryUnit::GM) ? "[SRFB]" : "[NRFB]")
                << " receive rfb entry id " << std::dec << i << " from bpq id:" << entry.bpqId
                << ". BDB size = " << m_bdbDataSize << " ,entry req size=" << entry.size;
            ++m_aliveRFBEntryCounter;
            break;
        }

        ASSERT(found && "RFB received too much data!");
    }
}

void ReadFillBuffer::SendReadMemReq(uint64_t entryId, RFBEntry& entry)
{
    constexpr uint64_t addr64BAlignMask = 64 - 1;
    ASSERT((entry.addr & addr64BAlignMask) == 0);

    GFUMemReq req;
    req.vld = true;
    req.size = entry.size;
    req.tid = SetReqTokenId(true, id, entryId);
    req.is_store = false;
    req.is_inst  = false;
    req.addr = entry.addr;
    req.lsuTypeId = LSUType::BRIDGE_TABLE;

    m_rfbMemReqQ->Write(req);
    if (entry.op == TileOp::MGATHER) {
        ++m_pmuData.mgatherSendReadMemReqNum;
    } else if (entry.op == TileOp::VGATHER) {
        ++m_pmuData.vgatherSendReadMemReqNum;
    } else if (entry.op == TileOp::MSCATTER) {
        ++m_pmuData.mscatterSendReadMemReqNum;
    } else if (entry.op == TileOp::VSCATTER) {
        ++m_pmuData.vscatterSendReadMemReqNum;
    }

    entry.status = RFBEntryStatus::TRANSMITED;
    entry.txedCycle = GetSim()->getCycles();
    m_pendingReqFifo.pop_front();
}

TTransTileRegLdReq ReadFillBuffer::CreateTileLdReq(uint64_t reqId, uint64_t addr, uint64_t size) const
{
    TTransTileRegLdReq req = TTransTileRegLdReq();
    // 调整地址, 按照 Tile 的地址粒度来调整
    req.SetReqId(reqId);
    req.SetSrc(addr);
    req.SetSize(size);

    return req;
}

void ReadFillBuffer::SendReadTileReq(uint64_t entryId, RFBEntry& entry)
{
    if (pTileUtils->IsRingOrXbarMode(false) && !HasValidSpbReadEntry()) {
        return;
    }
    constexpr uint64_t tileAddr256BAlignMask = (static_cast<uint64_t>(MAX_TILE_DATA_BYTE) - 1);

    ASSERT((entry.addr & tileAddr256BAlignMask) == 0);
    uint64_t readAddr = entry.addr;

    bool tileBankArbSuaccelss = true;
    if (GetSim()->core->configs.singleTierMode && (needReadRFArb < 0)) {
        tileBankArbSuaccelss = GetSim()->core->CanReadTileReg(MachineType::TMA);
        if (tileBankArbSuaccelss) {
            GetSim()->core->AcquireReadTileReg(MachineType::TMA, GetSim()->core->GetConfig().tmaReadOccTileregLat);
            if (needReadRFArb < 0) {  // needReadRFArb为-1表示这是第一次仲裁
                needReadRFArb = 6;
            }
        }
    }
    if (!tileBankArbSuaccelss && (needReadRFArb < 0)) {
        return;
    }
    if (needReadRFArb >= 0) {
        needReadRFArb--;
    }
    uint32_t reqId = SetTileReqId(false, false, entryId);
    TTransTileRegLdReq req = CreateTileLdReq(reqId, readAddr, m_readGranularity);
    if (pTileUtils->IsRingOrXbarMode(false)) {
        shared_ptr<CrossStation> station = GetFreeReadStation();
        shared_ptr<Request> pkt = station->Rxreq(req.GetSrc(), 0, req.GetStid());
        pkt->SetSize(req.GetSize());
        pkt->SetBufId(req.GetReqId());
        pkt->SetPEType(MachineType::TMA);
        station->WriteSpb(pkt);
    } else {
        GetSim()->core->tmaRealReadRFReqCnt++;
        m_rfbTileRegLdReqQ->Write(req);
    }

    entry.status = RFBEntryStatus::TRANSMITED;
    entry.txedCycle = GetSim()->getCycles();
    m_pendingReqFifo.pop_front();
}

void ReadFillBuffer::SendReadReq()
{
    if (m_pendingReqFifo.empty()) {
        return;
    }

    uint32_t readSentCnt = 0;
    for (uint32_t i = 0; i < m_rfbQ.size(); ++i) {
        uint64_t entryId = m_pendingReqFifo.front();
        RFBEntry& entry = m_rfbQ[i];
        if (!entry.vld || entry.status != RFBEntryStatus::WAIT || i != entryId) {
            continue;
        }

        LOG_DEBUG_M(Unit::TMA, Stage::RFB) << ((m_memUnit == MemoryUnit::GM) ? "[SRFB]" : "[NRFB]")
                                         << ": send read req for core id: " << std::dec << id << " bpq id:"
                                         << entry.bpqId << " entry id:" << i << " addr:0x" << std::hex << entry.addr
                                         << " size:" << std::dec << entry.size << "B";

        if (m_memUnit == MemoryUnit::GM) {
            SendReadMemReq(i, entry);
        } else if (m_memUnit == MemoryUnit::TR) {
            SendReadTileReq(i, entry);
        } else {
            LOG_ERROR_M(Unit::TMA, Stage::NA) << "[Bridge Table]: Unsupport direction!";
            abort();
        }

        if (readSentCnt >= m_maxSendReadNum) {
            break;
        }
        readSentCnt++;
    }
}

void ReadFillBuffer::PrintPipeViewLog(uint64_t bpqId, uint64_t stid)
{
    if (m_pipeviewInfo.count(bpqId) == 0) {
        return;
    }
    for (InstPipeViewPtr instInfo : m_pipeviewInfo.at(bpqId)) {
        GetSim()->GetViewManager(stid)->RecordMinst(instInfo);
    }

    m_pipeviewInfo.erase(bpqId);
}

void ReadFillBuffer::PrintPipeViewLog(RFBEntry& entry)
{
    InstPipeViewPtr instInfo = std::make_shared<InstPipeViewInfo>();
    instInfo->bid = entry.bid.val;
    instInfo->cycleInfo = std::make_shared<CycleInfo>();
    instInfo->cycleInfo->waitCycle = entry.waitCycle;
    instInfo->cycleInfo->txedCycle = entry.txedCycle;
    instInfo->cycleInfo->returnCycle = entry.returnCycle;
    instInfo->cycleInfo->retireCycle = entry.returnCycle + 1;
    instInfo->label = entry.Dump();
    if (IsMemoryModeReq(entry.op)) {
        m_pipeviewInfo[entry.bpqId].push_back(instInfo);
    } else {
        GetSim()->GetViewManager(entry.stid)->RecordMinst(instInfo);
    }
}

void ReadFillBuffer::ReciveTileDataAndUpdateStatus()
{
    if (pTileUtils->IsRingOrXbarMode(false)) {
        for (uint32_t i = 0; i < stations.size() && !m_rfbBpqQ->Full(); i++) {
            if (stations[i]->HasNoResp(0)) {
                continue;
            }
            shared_ptr<Request> pkt = stations[i]->GetDataComReqFront(0);
            if (IsVscatterRingReq(pkt)) {
                continue;
            }
            uint32_t respId = pkt->GetBufId();
            bool readFromBPQ = false;
            bool readFromFake = false;
            uint32_t reqId = 0;
            UnpackTileReqId(respId, readFromBPQ, readFromFake, reqId);
            if (readFromBPQ || readFromFake) {
                continue;
            }
            stations[i]->PopDataComReq(0);
            LOG_INFO_M(Unit::TMA, Stage::NA) << "Recv tile data " << *pkt;
            RefillTileData(reqId, pkt->GetData());
        }
        return;
    }

    if (m_tileRegRFBLdResQ->Empty()) {
        return;
    }

    TileRegTTransLdRes&& response = m_tileRegRFBLdResQ->Front();
    uint32_t respId = response.GetRespId();
    bool readFromBPQ = false;
    bool readFromFake = false;
    uint32_t reqId = 0;
    UnpackTileReqId(respId, readFromBPQ, readFromFake, reqId);
    if (readFromBPQ || readFromFake) {
        return;
    }
    m_tileRegRFBLdResQ->Pop();
    RefillTileData(reqId, response.GetData());
}

void ReadFillBuffer::RefillTileData(uint32_t repId, uint8_t *data)
{
    ASSERT(repId >= 0 && repId < m_rfbQ.size());

    RFBEntry& rfbEntry = m_rfbQ[repId];
    LOG_DEBUG_M(Unit::TMA, Stage::RFB) << "[NRFB]: receive data for core id:" << std::dec << id << " bpq id:"
                                     << rfbEntry.bpqId << " addr:0x" << std::hex << rfbEntry.addr;
    ASSERT(rfbEntry.vld);

    if (rfbEntry.status == RFBEntryStatus::FLUSHED) {
        rfbEntry.vld = false;
        --m_aliveRFBEntryCounter;
        /* TODO: add flush pipeview */
        return;
    }

    ASSERT(rfbEntry.status == RFBEntryStatus::TRANSMITED);
    uint32_t granMultiplier = rfbEntry.size / m_bdbDataSize;
    ASSERT(granMultiplier == rfbEntry.bdbId.size() && "Granularity multiplier must match BDB ID count");

    for (uint32_t i = 0; i < granMultiplier; ++i) {
        uint64_t bdbId = rfbEntry.bdbId[i];
        uint64_t addr = rfbEntry.addr + i * m_bdbDataSize;
        uint8_t *chunkData = data + i * m_bdbDataSize;
        m_bdb->WriteData(bdbId, addr, m_bdbDataSize, chunkData);
    }

    rfbEntry.status = RFBEntryStatus::RETURNED;
    rfbEntry.returnCycle = GetSim()->getCycles();
    rfbEntry.vld = false;
    --m_aliveRFBEntryCounter;
    PrintPipeViewLog(rfbEntry);

    m_rfbBpqQ->Write(rfbEntry);
}

void ReadFillBuffer::ReciveMemDataAndUpdateStatus()
{
    if (m_memRFBRspQ->Empty()) {
        return;
    }
    GFUMemReq&& response = m_memRFBRspQ->Front();
    // RFB暂时不处理fake消息，VGATHER消息走fakeChannel
    if (response.instTypeMap != 0) {
        return;
    }
    uint8_t data[256];
    std::memcpy(data, response.data, sizeof(response.data));
    uint64_t repId = -1;
    uint32_t coreId = -1;
    bool isLoad = false;
    UnpackReqTokenId(response.tid, isLoad, coreId, repId);
    if (!isLoad) { // Only process Read
        return;
    }
    if (coreId != id) {
        return;
    }
    m_memRFBRspQ->Pop();
    ASSERT(repId >= 0);
    ASSERT(repId < m_rfbQ.size());
    RFBEntry& rfbEntry = m_rfbQ[repId];
    LOG_DEBUG_M(Unit::TMA, Stage::RFB) << "[SRFB]: receive data for core id:" << std::dec << id << " bpq id:"
        << rfbEntry.bpqId << " entry id:" << repId << " addr:0x" << std::hex << rfbEntry.addr << std::dec;
    ASSERT(rfbEntry.vld);

    if (rfbEntry.status == RFBEntryStatus::FLUSHED) {
        rfbEntry.vld = false;
        --m_aliveRFBEntryCounter;
        /* TODO: add flush pipeview */
        return;
    }

    ASSERT(rfbEntry.status == RFBEntryStatus::TRANSMITED);
    uint32_t granMultiplier = rfbEntry.size / m_bdbDataSize;
    ASSERT(granMultiplier == rfbEntry.bdbId.size() && "Granularity multiplier must match BDB ID count");

    rfbEntry.status = RFBEntryStatus::RETURNED;
    rfbEntry.returnCycle = GetSim()->getCycles();

    if (IsGatherReq(rfbEntry.op)) {
        auto dataVec = std::make_shared<std::vector<uint8_t>>();
        dataVec->insert(dataVec->begin(), data, data + rfbEntry.size);
        struct MPBElem element = {
            .rfbId = repId,
            .data = dataVec,
            .bdbIds = rfbEntry.bdbId,
            .bpqId = rfbEntry.bpqId,
            .readAddr = rfbEntry.addr,
        };
        ASSERT(m_memoryPreBuffer.lock()->PushData(element));
    } else if (IsScatterReq(rfbEntry.op)) {
        auto dataVec = std::make_shared<std::vector<uint8_t>>();
        dataVec->insert(dataVec->begin(), data, data + rfbEntry.size);

        /* Read data from ts bdb */
        BPQEntryPtr bpqEntry = m_bpq.lock()->GetBpqPtr(rfbEntry.bpqId);
        std::vector<uint8_t> tsBDBData;
        for (auto bdbId : bpqEntry->tsBDBIds) {
            DataBlock dataBlock = m_tsBDB->ReadDataBlock(bdbId);
            tsBDBData.insert(tsBDBData.end(), dataBlock.data.begin(), dataBlock.data.end());
        }

        /* Write data from ts bdb to tl bdb */
        std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> switchMask =
            m_bpq.lock()->GetSwitchMask(rfbEntry.bpqId, rfbEntry.addr);

        /* Fill the data in the TL BDB based on the switchMask. */
        FillDataBySwitchMask(switchMask, tsBDBData, *dataVec);

        struct MPBElem element = {
            .rfbId = repId,
            .data = dataVec,
            .bdbIds = rfbEntry.bdbId,
            .bpqId = rfbEntry.bpqId,
            .readAddr = rfbEntry.addr,
        };
        ASSERT(m_memoryPreBuffer.lock()->PushData(element));
    } else {
        for (uint32_t i = 0; i < granMultiplier; ++i) {
            uint64_t bdbId = rfbEntry.bdbId[i];
            uint64_t addr = rfbEntry.addr + i * m_bdbDataSize;
            uint8_t *chunkData = data + i * m_bdbDataSize;
            m_bdb->WriteData(bdbId, addr, m_bdbDataSize, chunkData);
        }
        rfbEntry.vld = false;
        --m_aliveRFBEntryCounter;
        PrintPipeViewLog(rfbEntry);
        m_rfbBpqQ->Write(rfbEntry);
    }

    if (m_memUnit == MemoryUnit::GM) {
        m_pmuData.srfbEntryReadTotalCycles += (rfbEntry.returnCycle - rfbEntry.txedCycle);
        m_pmuData.srfbEntryNum++;
    }
}

void ReadFillBuffer::ReciveDataAndUpdateStatus()
{
    if (m_memUnit == MemoryUnit::GM) {
        ReciveMemDataAndUpdateStatus();
    } else if (m_memUnit == MemoryUnit::TR) {
        ReciveTileDataAndUpdateStatus();
    } else {
        LOG_ERROR_M(Unit::TMA, Stage::NA) << "[Bridge Table]: Unsupport direction!";
        abort();
    }
}

bool ReadFillBuffer::CheckInFlightReq(uint64_t addr)
{
    for (auto& entry : m_rfbQ) {
        if (!entry.vld) {
            continue;
        }
        if (entry.addr == addr) {
            return true;
        }
    }

    return false;
}

void ReadFillBuffer::ReleaseEntry(uint64_t rfbId)
{
    LOG_DEBUG_M(MachineType::TMA, Stage::RFB) << "ReleaseEntry:" << std::dec << rfbId;
    RFBEntry& entry = m_rfbQ.at(rfbId);
    ASSERT(entry.vld);
    entry.vld = false;
    --m_aliveRFBEntryCounter;
    PrintPipeViewLog(entry);
    if (IsMemoryModeReq(entry.op) && this->m_memUnit == MemoryUnit::GM) {
        m_bpq.lock()->ReleaseSRFBAvailCnt();
    }
}

uint32_t ReadFillBuffer::GetInvalidEntryCounter()
{
    return m_outStandingReqNum - m_aliveRFBEntryCounter;
}

uint32_t ReadFillBuffer::GetAliveEntryCounter()
{
    return m_aliveRFBEntryCounter;
}

uint32_t ReadFillBuffer::GetTxedEntryCounter()
{
    uint32_t txedEntryCounter = 0;
    for (RFBEntry& entry : m_rfbQ) {
        if (entry.vld && entry.status == RFBEntryStatus::TRANSMITED) {
            ++txedEntryCounter;
        }
    }
    return txedEntryCounter;
}

/* ========== Bridge Data Buffer ========== */
BridgeDataBuffer::BridgeDataBuffer(uint32_t bdbSize, uint64_t dataSize)
    : m_dataSize(dataSize)
{
    m_dataBuffer.resize(bdbSize);
}

uint64_t BridgeDataBuffer::AllocDataBuffer(uint64_t baseAddr)
{
    uint32_t bdbId = 0;
    for (; bdbId < m_dataBuffer.size(); ++bdbId) {
        if (!m_dataBuffer[bdbId].vld) {
            break;
        }
    }

    if (bdbId == m_dataBuffer.size()) {
        return -1;
    }

    m_dataBuffer[bdbId] = DataBlock(baseAddr, -1, m_dataSize);
    ++m_aliveEntryCounter;
    return bdbId;
}

uint64_t BridgeDataBuffer::AllocDataBuffer(uint64_t baseAddr, uint64_t bpqId)
{
    uint32_t bdbId = 0;
    for (; bdbId < m_dataBuffer.size(); ++bdbId) {
        if (!m_dataBuffer[bdbId].vld) {
            break;
        }
    }

    if (bdbId == m_dataBuffer.size()) {
        return -1;
    }

    m_dataBuffer[bdbId] = DataBlock(baseAddr, bpqId, m_dataSize);
    ++m_aliveEntryCounter;
    return bdbId;
}

void BridgeDataBuffer::WriteData(uint64_t bdbId, uint64_t addr, uint16_t size, uint8_t* data)
{
    if (bdbId < 0 || bdbId >= m_dataBuffer.size()) {
        LOG_ERROR_M(Unit::TMA, Stage::NA) << "[Bridge Table]: Invalid bdbId" << std::dec << bdbId;
        abort();
    }

    ASSERT(m_dataBuffer[bdbId].vld);
    auto& buffer = m_dataBuffer[bdbId].data;
    uint64_t offset = addr - m_dataBuffer[bdbId].baseAddr;
    if (offset + size > buffer.size()) {
        LOG_ERROR_M(Unit::TMA, Stage::NA) << "[Bridge Table]: Buffer size is smaller than the size "
            << "of the data to be writtern, bdb id:" << bdbId
            << ", base addr:0x" << std::hex << m_dataBuffer[bdbId].baseAddr
            << ", write addr:0x" << addr << ", write size:" << std::dec << size;
        abort();
    }

    auto freeMemSize = buffer.capacity() - offset;
    ASSERT(size <= static_cast<decltype(size)>(freeMemSize) && "Insufficient free memory!");
    std::copy(data, data + size, buffer.begin() + offset);

    std::stringstream ss;
    ss << "write BDB data:[";
    for (uint16_t i = 0; i < size; ++i) {
        ss << std::hex << static_cast<uint64_t>(data[i]) << ", ";
    }
    ss << "]" << std::endl;
    LOG_DEBUG_M(Unit::TMA, Stage::BDB) << ss.str();
}

std::vector<uint8_t> BridgeDataBuffer::ReadData(uint64_t addr, uint16_t size, uint64_t alignByte)
{
    bool found = false;
    std::vector<uint8_t> result(size);
    uint64_t alignAddr = addr & (~(alignByte - 1));
    for (auto& buffer : m_dataBuffer) {
        if (!buffer.vld) {
            continue;
        }
        if (buffer.baseAddr == alignAddr) {
            uint64_t offset = addr - buffer.baseAddr;
            if (offset + size > buffer.data.size()) {
                LOG_ERROR_M(Unit::TMA, Stage::NA) << "[Bridge Table]: Buffer size is smaller than the size of the data"
                    << "to be read, base addr:0x" << std::hex << buffer.baseAddr << ", read addr:0x" << addr
                    << "read size:" << std::hex << size;
                abort();
            }
            found = true;
            auto copySize = std::distance(buffer.data.begin() + offset, buffer.data.begin() + offset + size);
            auto freeMemSize = result.capacity();
            ASSERT(freeMemSize >= static_cast<decltype(freeMemSize)>(copySize) && "Insufficient free memory!");
            std::copy(buffer.data.begin() + offset, buffer.data.begin() + offset + size, result.begin());

            break;
        }
    }

    if (!found) {
        LOG_WARN_M(Unit::TMA, Stage::NA) << "[Bridge Table]: Invalid addr:0x" << std::hex << addr;
    }

    return result;
}

DataBlock& BridgeDataBuffer::ReadDataBlock(uint64_t bdbId)
{
    ASSERT(m_dataBuffer[bdbId].vld);
    return m_dataBuffer[bdbId];
}

bool BridgeDataBuffer::CheckBufferHit(uint64_t bpqId, uint64_t addr, uint64_t alignByte)
{
    uint64_t alignAddr = addr & (~(alignByte - 1));
    for (auto& buffer : m_dataBuffer) {
        if (!buffer.vld || buffer.bpqId != bpqId) {
            continue;
        }
        if (buffer.baseAddr == alignAddr) {
            return true;
        }
    }

    return false;
}

void BridgeDataBuffer::ReleaseBuffer(std::unordered_set<uint64_t> bdbIds)
{
    for (uint64_t bdbId = 0; bdbId < m_dataBuffer.size(); ++bdbId) {
        auto& buffer = m_dataBuffer[bdbId];
        if (bdbIds.count(bdbId) == 0) {
            continue;
        }
        ASSERT(buffer.vld);
        buffer.vld = false;
        buffer.data.clear();
        --m_aliveEntryCounter;
    }
}

void BridgeDataBuffer::ReleaseBuffer(uint64_t bpqId)
{
    for (auto& buffer : m_dataBuffer) {
        if (buffer.vld && buffer.bpqId == bpqId) {
            buffer.vld = false;
            buffer.data.clear();
            --m_aliveEntryCounter;
        }
    }
}

void BridgeDataBuffer::ClearBuffer()
{
    for (auto& buffer : m_dataBuffer) {
        if (buffer.vld) {
            buffer.vld = false;
            buffer.data.clear();
            --m_aliveEntryCounter;
        }
    }
}

uint32_t BridgeDataBuffer::GetInvalidEntryCounter()
{
    return m_dataBuffer.size() - m_aliveEntryCounter;
}

uint32_t BridgeDataBuffer::GetAliveEntryCounter()
{
    return m_aliveEntryCounter;
}

void FakeChannelNode::Work()
{
    ReceiveSocRsp();
    SendChannelEntryMsg();
    CollectTileWriteData();
    SendWriteTileData();
    ReceiveTileWriteResp();
}

void FakeChannelNode::ReceiveTileWriteResp()
{
    if (bpqDataInfo.size() == 0) {
        return;
    }
    ASSERT(bpqInst != nullptr);
    bool noResp = true;

    uint32_t bpqId = 0; // bpqID
    if (bpqInst->GetNWcb()->pTileUtils->IsRingOrXbarMode(false)) {
        for (auto &station : bpqInst->GetNWcb()->stations) {
            if (station->HasNoResp()) {
                continue;
            }
            shared_ptr<Request> pkt = station->GetDataComReqFront();
            if (IsVscatterRingReq(pkt)) {
                continue;
            }
            uint32_t respId = pkt->GetBufId();
            bool readFromBPQ = false;
            bool readFakeChannel = false;
            UnpackTileReqId(respId, readFromBPQ, readFakeChannel, bpqId);
            if (readFromBPQ || !readFakeChannel) {
                continue;
            }
            cout << "FakeChannelNode::ReceiveTileWriteResp, readFakeChannel=" << readFakeChannel
                 << ", bpqId=" << bpqId << endl;
            station->PopDataComReq();
            noResp = false;
            break;
            LOG_INFO_M(Unit::TMU, Stage::NA) << "Receive Tile Write Vector done Resp " << *pkt;
        }
    }

    if (!noResp) {
        bpqDataInfo.erase(bpqId);
        bpqInst->SetBPQEntryState(bpqId);
        cout << "FakeChannelNode::ReceiveTileWriteResp, bpqId=" << bpqId << endl;
    }
}

void FakeChannelNode::CollectTileWriteData()
{
    for (auto it = bpqDataInfo.begin(); it != bpqDataInfo.end(); it++) {
        FakeChannelEntry &fakeChannelEntry = it->second;
        if ((fakeChannelEntry.op != TileOp::VGATHER) || (fakeChannelEntry.status != FakeChannelStatus::COLLECT_DATA)) {
            continue;
        }
        uint32_t index = 0;
        for (auto &entry : fakeChannelEntry.dataEntry) {
            if (((1 << index) & fakeChannelEntry.laneMask) != 0) {
                auto writeOffset = fakeChannelEntry.gatherDataSize * index;
                ASSERT((writeOffset + fakeChannelEntry.gatherDataSize) <= MAX_LANE_DATA_SIZE);
                std::memcpy((fakeChannelEntry.data + writeOffset), entry.gatherData, fakeChannelEntry.gatherDataSize);
            }
            index++;
        }
        fakeChannelEntry.status = FakeChannelStatus::SEND_WRITE;
        cout << "CollectTileWriteData, RFBEntryStatus::FILL_WCB, bpqId=" << fakeChannelEntry.bpqId
                << ", fakeChannelEntry.dataEntry.size() ="<< fakeChannelEntry.dataEntry.size() << endl;
        // 1 cycle 1个
        break;
    }
}

void FakeChannelNode::SendWriteTileData()
{
    for (auto it = bpqDataInfo.begin(); it != bpqDataInfo.end(); it++) {
        FakeChannelEntry &fakeChannelEntry = it->second;
        if ((fakeChannelEntry.op == TileOp::VGATHER) && (fakeChannelEntry.status == FakeChannelStatus::SEND_WRITE)) {
            cout << "fakeChannelEntry.writeReqs.size=" << std::dec << fakeChannelEntry.writeReqs.size()
                 << ", fakeChannelEntry.countWriteTileNum =" << fakeChannelEntry.countWriteTileNum << endl;
            for (auto& writeReq : fakeChannelEntry.writeReqs) {
                if (writeReq.vld) {
                    SendTileWriteData(fakeChannelEntry, writeReq);
                }
            }
            if (fakeChannelEntry.countWriteTileNum == fakeChannelEntry.writeReqs.size()) {
                cout << "SendWriteTileData, RFBEntryStatus::UPDATE_WCB, bpqId=" << fakeChannelEntry.bpqId << endl;
                cout << "SendWriteTileData(), countWriteTileNum=" << fakeChannelEntry.countWriteTileNum
                     << ", writeReqs.size=" << fakeChannelEntry.writeReqs.size() <<endl;
                fakeChannelEntry.status = FakeChannelStatus::WAIT_WRITE_RESP;
            }
            // 1 cycle处理1个 Send Write Data
            break;
        }
    }
}

void FakeChannelNode::SetBridgeMemReqQ(BridgePairQ *bridgePairQ, SimQueue<GFUMemReq> *bridgeMemReqQ,
    SimQueue<GFUMemReq> *bridgeMemRspQ)
{
    bpqInst = bridgePairQ;
    m_bridgeMemReqQ = bridgeMemReqQ;
    m_memBridgeRspQ = bridgeMemRspQ;
}

bool FakeChannelNode::SendTileWriteData(FakeChannelEntry &channelEntry, WriteReq &writeEntry) const
{
    if (bpqInst->GetNWcb()->pTileUtils->IsRingOrXbarMode(false) && !bpqInst->GetNWcb()->HasValidSpbWriteEntry()) {
        cout << "FakeChannelNode::SendTileWriteData  Not Ready !" << endl;
        return false;
    }

    constexpr uint64_t tileAddr256BAlignMask = (static_cast<uint64_t>(MAX_TILE_DATA_BYTE) - 1);
    ASSERT((writeEntry.writeAddr & tileAddr256BAlignMask) == 0);

    /* fill data */
    DataMask writeDataMask = DataMask(pTileUtils->configs.mask_element_size);
    writeDataMask.Set(0, writeEntry.validWriteSize);
    uint32_t entryId = channelEntry.bpqId; // use for send vector done
    uint32_t reqId = SetTileReqId(false, true, entryId);
    bool hasSend = false;
    if (bpqInst->GetNWcb()->pTileUtils->IsRingOrXbarMode(false)) {
        shared_ptr<CrossStation> station = bpqInst->GetNWcb()->GetFreeWriteStation();
        // TODO: stid
        shared_ptr<Request> pkt = station->Rxdat(writeEntry.writeAddr, 0);
        pkt->SetSize(writeEntry.validWriteSize);
        pkt->SetBufId(reqId);
        pkt->SetPEType(MachineType::TMA);
        uint8_t data[MAX_TILE_DATA_BYTE];
        uint8_t* stData = channelEntry.data;
        for (size_t i = 0; i < MAX_TILE_DATA_BYTE; ++i) {
            data[i] = stData[i];
        }
        pkt->SetData(data);
        pkt->dmask = writeDataMask;
        station->WriteSpb(pkt);
        hasSend = true;
    } else {
    }

    if (hasSend) {
        writeEntry.vld = false;
        channelEntry.countWriteTileNum++;
        cout << "FakeChannelNode::SendTileWriteData OK,  channelEntry.countWriteTileNum="
             << channelEntry.countWriteTileNum << endl;
    }
    return true;
}

void FakeChannelNode::SendChannelEntryMsg()
{
    for (auto it = bpqDataInfo.begin(); it != bpqDataInfo.end(); it++) {
        FakeChannelEntry &channelEntry = it->second;
        if ((channelEntry.op != TileOp::VGATHER) || (channelEntry.status != FakeChannelStatus::SEND_READ)) {
            continue;
        }
        std::cout << "SendChannelEntryMsg Entry in" << endl;
        uint64_t sendIndex = 0;
        for (auto entryIt = channelEntry.dataEntry.begin(); entryIt != channelEntry.dataEntry.end(); ++entryIt) {
            // 应该要过滤调mask, laneMask为0则跳过
            if ((channelEntry.laneMask & (1 << sendIndex)) == 0) {
                continue;
            }
            // 有效位都发送出去，没搞容量反压机制
            SendSocReq(sendIndex, channelEntry, *entryIt);
            sendIndex++;
            channelEntry.sendLaneNum++; // 要确定是有效laneMask个数了, 理想soc
        }
        std::cout << "SendChannelEntryMsg, size=" << std::dec << channelEntry.dataEntry.size()
                    << ",bpqId= "<< channelEntry.bpqId << std::hex << ",baseAddrDstTR=" << channelEntry.baseAddrDstTR
                    << ", channelEntry.laneMask=0x" << channelEntry.laneMask
                    << std::dec << ", sendIndex=" << sendIndex
                    << ", channelEntry.sendLaneNum = " << channelEntry.sendLaneNum << endl;
        std::cout << "SendChannelEntryMsg, fakeChannelEntry.writeReqs.size() ="
                    << channelEntry.writeReqs.size() << std::endl;
        channelEntry.status = FakeChannelStatus::WAIT_READ_DATA;
    }
}

void FakeChannelNode::SendSocReq(uint64_t sendIndex, FakeChannelEntry &channelEntry, GatherDataEntry &gatherData)
{
    if (gatherData.status == RFBEntryStatus::TRANSMITED) {
        return;
    }
    // Not cross soc cacheline
    GFUMemReq req{};
    // 仅仅发送VGATHER
    req.instTypeMap = static_cast<uint64_t>(InstTypeBit::TYPE_VGATHER);
    req.vld = true;
    req.size = m_config.BRIDGE_DATA_BUFFER_DATA_SIZE; // entry.size;
    uint32_t msgId = sendIndex ; //  + (channelEntry.bpqId << MAX_LANE_BIT)
    req.tid = SetReqTokenId(true, bpqInst->GetCoreId(), msgId); // Todo: 要不要和其他tload区分tid
    req.is_store = false;
    req.is_inst  = false;
    req.lsuTypeId = LSUType::BRIDGE_TABLE;
    req.bpqID = channelEntry.bpqId;
    req.addr = gatherData.originalAddr & (~(SOC_READ_CACHELINE_ALIGN_MASK - 1));
    m_bridgeMemReqQ->Write(req);
    gatherData.status = RFBEntryStatus::TRANSMITED;
    gatherData.socDataTid = req.tid;
}

void FakeChannelNode::ReceiveSocRsp()
{
    if (m_memBridgeRspQ->Empty()) {
        return;
    }
    GFUMemReq&& response = m_memBridgeRspQ->Front();
    // 仅仅处理VGATHER
    if ((response.instTypeMap & static_cast<uint32_t>(InstTypeBit::TYPE_VGATHER)) == 0) {
        return;
    }
    uint64_t repId = -1;
    uint32_t coreId = -1;
    uint32_t bpqId = response.bpqID;
    bool isLoad = false;
    UnpackReqTokenId(response.tid, isLoad, coreId, repId);
    if (!isLoad) { // Only process Read
        return;
    }
    m_memBridgeRspQ->Pop();
    ASSERT(repId >= 0);
    bool findTidEntry = false;
    FakeChannelEntry &channelEntry = bpqDataInfo[bpqId];
    for (auto entryIt = channelEntry.dataEntry.begin(); entryIt != channelEntry.dataEntry.end(); ++entryIt) {
        GatherDataEntry &dataEntry = *entryIt;
        if (response.tid == dataEntry.socDataTid) {
            findTidEntry = true;
            ASSERT(dataEntry.status == RFBEntryStatus::TRANSMITED);
            dataEntry.status = RFBEntryStatus::RETURNED;
            uint64_t offset = dataEntry.originalAddr -
                              (dataEntry.originalAddr & (~(SOC_READ_CACHELINE_ALIGN_MASK - 1)));
            std::memcpy(dataEntry.gatherData, response.data + offset, dataEntry.gatherDataSize);
            channelEntry.rcvLaneNum++;
            if (channelEntry.rcvLaneNum == channelEntry.sendLaneNum) {
                channelEntry.completeCycle++;
                channelEntry.status = FakeChannelStatus::COLLECT_DATA;
                cout << "FakeChannelNode ReceiveSocRsp:: ALL Received,  vGather status RFBEntryStatus::RETURNED,"
                        "rcvLaneNum=" << channelEntry.rcvLaneNum << ", bpqId =" << bpqId << endl;
            }
            break;
        }
    }

    ASSERT(findTidEntry);
    return;
}
}
}
}
