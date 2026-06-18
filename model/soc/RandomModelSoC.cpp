#include <cstdint>
#include "core/Core.h"
#include "SimSys.h"
#include "RandomModelSoC.h"
using namespace JCore;

constexpr uint32_t CREDIT_UPDATE_PERIOD = 1;   // in term of ticks, normally should less than DDR latency

RandomModelSoC::RandomModelSoC()
{
    sim = nullptr;
}

RandomModelSoC::~RandomModelSoC()
{
}

void RandomModelSoC::Build(SimSys *simSys)
{
    sim = simSys;
    ASSERT(sim != nullptr);
    maxReadCredit = sim->core->configs.max_rd_crdt;
    maxWriteCredit = sim->core->configs.max_wr_crdt;
    // Gamma distribute
    uint32_t gammaSeedRead  = sim->core->configs.soc_lat_random_seed;
    uint32_t gammaSeedWrite = sim->core->configs.soc_lat_random_seed;
    if (gammaSeedRead != 0) {
        gammaSeedWrite++;        // for Read & Write to use different seed
    }

    // slot 0 for read
    SetupGammaGen(0, sim->core->configs.min_read_latency, sim->core->configs.read_latency_diver, gammaSeedRead);
    // slot 1 for write
    SetupGammaGen(1, sim->core->configs.min_write_latency, sim->core->configs.write_latency_diver, gammaSeedWrite);
}

bool RandomModelSoC::IsCreditEnough(const GFUMemReq &gfuMemReq)
{
    uint32_t size = GetTnxNum(gfuMemReq.addr, gfuMemReq.size) * ESL_PORT_WIDTH;
    uint32_t rwCredit = (gfuMemReq.is_store == 0) ? l2ReadCredit : l2WriteCredit;
    if (rwCredit < size) {
        LOG_INFO_M(Unit::SOC, Stage::NA) << "RandomModelSoC::IsCreditEnough Warning, Donot send OK, rwCredit = "
                << rwCredit << ", size = " << size << ", gfuMemReq.size = " << gfuMemReq.size
                << std::hex << ", gfuMemReq.addr = 0x" << gfuMemReq.addr << std::dec;
        return false;
    }
    return true;
}

void RandomModelSoC::Xfer()
{
    randomModelQueue.Xfer();
    for (uint32_t i = 0; i < sim->core->memReqQ.size(); ++i) {
        while (!sim->core->memReqQ[i].Empty()) {
            const GFUMemReq &entryReq = sim->core->memReqQ[i].Front();
            if (!IsCreditEnough(entryReq)) {
                // No credit may wait next tick
                return;
            }
            GFUMemReq req = sim->core->memReqQ[i].Read();
            if (req.lsuTypeId == LSUType::BRIDGE_TABLE) {
                constexpr uint64_t bridgeCacheLineSize = 128;
                ASSERT((req.addr & ~(bridgeCacheLineSize - 1)) == req.addr);
                ASSERT((req.size & ~(bridgeCacheLineSize - 1)) == req.size);
            }
            req.socAaccelptCycle = sim->getCycles();
            // Send Req to Random soc model
            SetQueue(req);
        }
    }
}

void RandomModelSoC::Work()
{
    ASSERT(sim != nullptr);
    randomModelQueue.Work();
    UpdateCredit();
    while (!randomModelQueue.empty()) {
        GFUMemReq gfuMemReq = randomModelQueue.read();
        gfuMemReq.socReturnCycle = sim->getCycles();
        CheckReqSize(gfuMemReq);
        SimMemOper(sim, gfuMemReq);
        int lsuIndex = static_cast<int>(gfuMemReq.lsuTypeId);
        if ((gfuMemReq.is_store == 0) || (gfuMemReq.lsuTypeId == LSUType::BRIDGE_TABLE)) {
            sim->core->memRetQ[lsuIndex].Write(gfuMemReq);
        }
        
        int isBridgeTable = (gfuMemReq.lsuTypeId == LSUType::BRIDGE_TABLE) ? 1 : 0;
        LOG_INFO_M(Unit::SOC, Stage::NA) << "RandomModelSoC::Work, Msg Rsp Write Gap = "
            << (gfuMemReq.socReturnCycle - gfuMemReq.socAaccelptCycle)  << ", gfuMemReq.is_store = "
            << gfuMemReq.is_store << ", size = " << gfuMemReq.size << ", isBridgeTable = " << isBridgeTable
            << std::hex << ", gfuMemReq.addr = 0x" << gfuMemReq.addr << std::dec;
    }
}

uint32_t RandomModelSoC::GetTnxNum(const uint64_t addr, const uint32_t size) const
{
    constexpr uint32_t BL4 = 4;
    constexpr uint32_t BL2 = 2;
    constexpr uint32_t BL1 = 1;
    constexpr uint32_t BL4_SHIFT = 9;
    constexpr uint32_t BL2_SHIFT = 8;
    constexpr uint32_t BL1_SHIFT = 7;
    ASSERT(size > 0);
    ASSERT(size <= (ESL_PORT_WIDTH * BL4));
    const uint64_t addrTail = addr + size - 1;
    const uint64_t addrBl4Align = addr >> BL4_SHIFT;
    const uint64_t addrBl2Align = addr >> BL2_SHIFT;
    const uint64_t addrBl1Align = addr >> BL1_SHIFT;

    const uint64_t addrTailBl4Align = addrTail >> BL4_SHIFT;
    const uint64_t addrTailBl2Align = addrTail >> BL2_SHIFT;
    const uint64_t addrTailBl1Align = addrTail >> BL1_SHIFT;

    uint32_t alignSize = BL1;
    if (addrBl4Align != addrTailBl4Align) {
        alignSize = size / ESL_PORT_WIDTH;
    } else if (addrBl2Align != addrTailBl2Align) {
        alignSize = BL4;
    } else if (addrBl1Align != addrTailBl1Align) {
        alignSize = BL2;
    } else {
        alignSize = BL1;
    }
    return alignSize;
}

void RandomModelSoC::UpdateCredit()
{
    ASSERT(sim != nullptr);
    l2ReadCredit += CREDIT_UPDATE_PERIOD * (sim->core->configs.read_bandwidth_limit);
    l2WriteCredit += CREDIT_UPDATE_PERIOD * (sim->core->configs.write_bandwidth_limit);

    if (l2ReadCredit > maxReadCredit) {
        l2ReadCredit = maxReadCredit;
    }
    if (l2WriteCredit > maxWriteCredit) {
        l2WriteCredit = maxWriteCredit;
    }
}

void RandomModelSoC::Reset()
{
    l2ReadCredit = maxReadCredit;
    l2WriteCredit = maxWriteCredit;
}

void RandomModelSoC::SetQueue(const GFUMemReq &gfuMemReq)
{
    SetCredit(gfuMemReq);
    SetDelayQueue(gfuMemReq);
}

void RandomModelSoC::SetCredit(const GFUMemReq &gfuMemReq)
{
    uint32_t size = GetTnxNum(gfuMemReq.addr, gfuMemReq.size) * ESL_PORT_WIDTH;
    uint32_t &rwCredit = (gfuMemReq.is_store == 0)  ? l2ReadCredit : l2WriteCredit;
    ASSERT(size <= rwCredit);
    // Do read/write consume credit
    rwCredit -= size;
}

uint32_t RandomModelSoC::GetRspNum() const
{
    // When core AS ready, can do (!gfuMemReq.is_store) ? GetTnxNum(gfuMemReq.addr, gfuMemReq.size) : 1;
    return 1;
}

void RandomModelSoC::SetDelayQueue(const GFUMemReq &gfuMemReq)
{
    uint32_t gammaSlot = (gfuMemReq.is_store == 0) ? 0 : 1;
    uint32_t rspNum = GetRspNum();
    for (uint32_t i = 0; i < rspNum; ++i) {
        uint32_t latency = GammaGen(gammaSlot);
        // Save to the latency Queue
        randomModelQueue.write(gfuMemReq, latency);
        LOG_INFO_M(Unit::SOC, Stage::NA) << "RandomModelSoC::SetDelayQueue, Core Send Req to SoC, latency = "
            << latency << ", is_store = " << gfuMemReq.is_store << ", addr = 0x" << gfuMemReq.addr << std::dec;
    }
}

void RandomModelSoC::SetupGammaGen(uint32_t slotId, uint32_t mean, uint32_t variance, uint32_t seed)
{
    ASSERT(mean > 0);          // Mean of SoC Latency's Gamma Distribution must > 0
    ASSERT(variance > 0);      // Variance of SoC Latency's Gamma Distribution must > 0
    ASSERT(mean * mean > variance); // variance of Gamma Distribution must <= mean^2

    double dMean = static_cast<double>(mean);
    double dVariance = static_cast<double>(variance);
    double alpha = (dMean * dMean) / dVariance;  // shape param
    double beta = dVariance / dMean;              // scale param
    
    auto& gen = gamma[slotId];
    gen.rng.seed(seed);
    gen.dist = std::gamma_distribution<double>(alpha, beta);
    gen.threshold = dMean + 3.0 * dVariance;
}

uint32_t RandomModelSoC::GammaGen(uint32_t slotId)
{
    auto& gen = gamma[slotId];
    double rawValue = gen.dist(gen.rng);
    if (rawValue > gen.threshold) {
        rawValue = gen.threshold;
        LOG_INFO_M(Unit::SOC, Stage::NA) << "Gamma Distribution Get a result larger than triple variance " << rawValue;
    }
    ASSERT(rawValue >= 0);
    return static_cast<uint32_t>(rawValue);
}

void RandomModelSoC::SimMemOper(SimSys *sim, GFUMemReq &req)
{
    auto canWrite = [](uint32_t loopCnt, GFUMemReq &req)->bool {
        uint32_t idx = loopCnt / 64;
        uint32_t offset = loopCnt % 64;
        if ((req.bitMask[idx] & (1ULL << offset)) == 0) {
            return false;
        }
        return true;
    };
    int loopcnt = req.size >> 3;
    uint32_t aaccelssByte = 8;
    if (req.bypassL2) {
        ASSERT(req.size == 256);
    } else {
        if (req.lsuTypeId != LSUType::BRIDGE_TABLE) {
            ASSERT(req.size == 64);
        }
    }

    if (req.lsuTypeId == LSUType::BRIDGE_TABLE) {
        loopcnt = req.size;
        aaccelssByte = 1;
    }
    if (req.is_store != 0) {
        for (int i = 0; i < loopcnt; i++) {
            uint64_t data = 0;
            if (req.lsuTypeId == LSUType::BRIDGE_TABLE && !canWrite(i, req)) {
                continue;
            }
            std::memcpy(&data, req.data8b + (i * aaccelssByte), aaccelssByte);
            sim->storeData(req.addr + i * aaccelssByte, data, aaccelssByte);
        }
    } else if (req.is_inst) {
        // Fetch
        for (int i = 0; i < loopcnt; i++) {
            uint64_t data = sim->fetchData(req.addr + i * aaccelssByte, aaccelssByte);
            std::memcpy(req.data8b + (i * aaccelssByte), &data, aaccelssByte);
        }
    } else {
        // Load
        for (int i = 0; i < loopcnt; i++) {
            uint64_t data = sim->loadData(req.addr + i * aaccelssByte, aaccelssByte, false);
            std::memcpy(req.data8b + (i * aaccelssByte), &data, aaccelssByte);
        }
    }
}

void RandomModelSoC::CheckReqSize(const GFUMemReq& gfuMemReq)
{
    if (gfuMemReq.bypassL2) {
        ASSERT(gfuMemReq.size == 256);
    } else {
        /* FIXME: Modify so that the bridge can partial write. */
        if (gfuMemReq.lsuTypeId != LSUType::BRIDGE_TABLE) {
            ASSERT(gfuMemReq.size == 64);
        }
    }
}