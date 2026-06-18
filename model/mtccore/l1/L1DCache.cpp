#include "mtccore/l1/MTC_L1DCache.h"

#include "../configs/mlsu_config.h"
#include "mtccore/l1/MTC_L1Clusters.h"

namespace JCore {

using namespace std;

MTC_L1DCache::MTC_L1DCache() {}
MTC_L1DCache::~MTC_L1DCache()
{
    // MTC_L1DCache::Build
    for (uint32_t i = 0; i < nset; i++) {
        delete[] cache[i];
    }
    delete[] cache;
}

uint32_t MTC_L1DCache::calcSetIdx(uint64_t va)
{
    uint32_t index = 0;
    for (uint32_t i = 0; i < bitList.size(); i++) {
        uint32_t tmp = (va >> bitList[i]) & 0x1;
        tmp = tmp << i;
        index |= tmp;
    }
    return index;
}

void MTC_L1DCache::Reset()
{
    for (uint32_t i = 0; i < nset; i++) {
        for (uint32_t j = 0; j < nway; j++) {
            cache[i][j].state = MTC_CS_INV;
        }
    }
}

bool MTC_L1DCache::lookup(uint64_t va, bool write, bool *phit)
{
    uint32_t set = calcSetIdx(va);
    bool valid = false;
    bool hit = false;
    for (uint32_t i = 0; i < nway; i++) {
        MTC_L1Entry *e = &cache[set][i];
        hit = (e->state != MTC_CS_INV) && tagMatch(e->addr, va);
        if (hit) {
            valid = true;
            if (valid) {
                e->demand = true;
                updateLRU(set, i);
            }
            break;
        }
    }
    if (verbose) {
        printf("[L1]: %s. addr 0x%lx, valid %d\n", __func__, va, valid);
    }
    if (DEBUG_VERBOSE_ON || lsuConfigs->verbose) {
        LOG_DEBUG(debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                  "[L1]: %s. addr 0x%lx, valid %d", __func__, va, valid);
    }
    if (phit) *phit = hit;
    return valid;
}

// For compatibility reason
MTC_L1Entry MTC_L1DCache::refill(uint64_t addr, uint64_t *data)
{
    uint32_t set = calcSetIdx(addr);
    MTC_L1Entry r = cache[set][nway-1];
    for (uint32_t i = nway-1; i > 0; i--) {
        cache[set][i] = cache[set][i-1];
    }
    MTC_L1Entry *e = &cache[set][0];
    e->state = MTC_CS_VALID;  // TODO
    for (uint32_t i = 0; i < 32; i++) {
        e->data.bits[i] = data[i];
    }
    ASSERT((addr & 0xff) == 0);
    e->addr = addr;
    if (verbose) {
        printf("[L1]: %s. addr 0x%lx, replace 0x%lx(%d)\n", __func__, addr, r.addr, r.state);
    }
    if (DEBUG_VERBOSE_ON || lsuConfigs->verbose) {
        LOG_DEBUG(debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                  "[L1]: %s. addr 0x%lx, replace 0x%lx(%d)", __func__, addr, r.addr, r.state);
    }
    return r;
}

MTC_L1Entry MTC_L1DCache::refill(uint64_t addr, uint64_t *data, MTC_CacheState cs)
{
    uint32_t set = calcSetIdx(addr);
    MTC_L1Entry r = cache[set][nway-1];
    for (uint32_t i = nway-1; i > 0; i--) {
        cache[set][i] = cache[set][i-1];
    }
    MTC_L1Entry *e = &cache[set][0];
    e->state = cs;
    for (uint32_t i = 0; i < 32; i++) {
        e->data.bits[i] = data[i];
    }
    ASSERT((addr & 0xff) == 0);
    e->addr = addr;
    if (verbose) {
        printf("[L1]: %s. addr 0x%lx, replace 0x%lx(%d)\n", __func__, addr, r.addr, r.state);
    }
    if (DEBUG_VERBOSE_ON || lsuConfigs->verbose) {
        LOG_DEBUG(debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                  "[L1]: %s. addr 0x%lx, replace 0x%lx(%d)", __func__, addr, r.addr, r.state);
    }
    return r;
}

MTC_L1Entry MTC_L1DCache::refill(uint64_t addr, uint64_t *data, MTC_CacheState cs, PtrPacket pkt)
{
    uint32_t set = calcSetIdx(addr);
    MTC_L1Entry r = cache[set][nway-1];
    for (uint32_t i = nway-1; i > 0; i--) {
        cache[set][i] = cache[set][i-1];
    }
    MTC_L1Entry *e = &cache[set][0];
    e->state = cs;
    for (uint32_t i = 0; i < 32; i++) {
        e->data.bits[i] = data[i];
    }
    ASSERT((addr & 0xff) == 0);
    e->addr = addr;
    e->prefetch = pkt->hasPref();
    e->demand   = !e->prefetch;
    if (e->prefetch) {
        e->pref_type = pkt->user_type;
    }
    if (verbose) {
        printf("[L1]: %s. addr 0x%lx, replace 0x%lx(%d)\n", __func__, addr, r.addr, r.state);
    }
    if (DEBUG_VERBOSE_ON || lsuConfigs->verbose) {
        LOG_DEBUG(debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                  "[L1]: %s. addr 0x%lx, replace 0x%lx(%d)", __func__, addr, r.addr, r.state);
    }
    return r;
}

void MTC_L1DCache::upgradeBroadcast(uint64_t addr)
{
    MTC_L1Entry *e = find(addr);
    ASSERT(e);
    bool positionVld[256];

    memset(positionVld, true, sizeof(positionVld));

    cluster->broadcastUpgrade(e->addr, e->data, positionVld);
}

uint2048_t MTC_L1DCache::getData(uint64_t addr)
{
    MTC_L1Entry *e = find(addr);
    ASSERT(e);
    return e->data;
}

void MTC_L1DCache::Work()
{
}

void MTC_L1DCache::Xfer()
{
}

SimSys *MTC_L1DCache::GetSim()
{
    return sim;
}

bool mtccheckDupSel(uint32_t bit, std::vector<uint64_t> bitsArr)
{
    for (uint64_t b : bitsArr) {
        if (b == bit) {
            return true;
        }
    }

    return false;
}

void MTC_L1DCache::Build(void)
{
    nset = pConfigs->dcache_nset;
    nway = pConfigs->dcache_nway;
    way_size = pConfigs->dcache_way_size;
    verbose = pConfigs->verbose;

    cache = new MTC_L1Entry *[nset];
    for (uint32_t i = 0; i < nset; i++) {
        cache[i] = new MTC_L1Entry[nway];
    }

    uint32_t seek = 8; // 6 = log2(way_size)
    while (bitList.size() < log2(pConfigs->dcache_nset) && seek < pConfigs->dcache_way_size) {
        if (!mtccheckDupSel(seek, pConfigs->l1_disp_bit_arr)) {
            bitList.push_back(seek);
        }
        seek++;
    }
}

bool MTC_L1DCache::queryByTag(uint64_t va)
{
    uint32_t set = calcSetIdx(va);
    for (uint32_t i = 0; i < nway; i++) {
        MTC_L1Entry *e = &cache[set][i];
        if ((e->state != MTC_CS_INV) && tagMatch(e->addr, va)) {
            return true;
        }
    }

    return false;
}

} // namespace JCore