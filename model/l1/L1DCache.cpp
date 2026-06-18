#include "l1/L1DCache.h"

#include "../configs/lsu_config.h"
#include "l1/cluster.h"

namespace JCore {

using namespace std;

L1DCache::L1DCache() {}
L1DCache::~L1DCache()
{
    // L1DCache::Build
    for (uint32_t i = 0; i < nset; i++) {
        delete[] cache[i];
    }
    delete[] cache;
}

uint32_t L1DCache::calcSetIdx(uint64_t va) {
    uint32_t index = 0;
    for (uint32_t i = 0; i < bitList.size(); i++) {
        uint32_t tmp = (va >> bitList[i]) & 0x1;
        tmp = tmp << i;
        index |= tmp;
    }
    return index;
}

void L1DCache::Reset() {
    for (uint32_t i = 0; i < nset; i++) {
        for (uint32_t j = 0; j < nway; j++) {
            cache[i][j].state = CS_INV;
        }
    }
}

bool L1DCache::lookup(uint64_t va, bool write, bool *phit) {
    uint32_t set = calcSetIdx(va);
    bool valid = false;
    bool hit = false;
    for (uint32_t i = 0; i < nway; i++) {
        L1Entry *e = &cache[set][i];
        hit = (e->state != CS_INV) && tagMatch(e->addr, va);
        if (hit) {
            valid = write ? (e->state == CS_MOD || e->state == CS_EXCL) : true;
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
L1Entry L1DCache::refill(uint64_t addr, uint64_t *data) {
    uint32_t set = calcSetIdx(addr);
    L1Entry r = cache[set][nway-1];
    for (uint32_t i = nway-1; i > 0; i--) {
        cache[set][i] = cache[set][i-1];
    }
    L1Entry *e = &cache[set][0];
    e->state = CS_EXCL;  // TODO
    for (uint32_t i = 0; i < 8; i++) {
        e->data.bits[i] = data[i];
    }
    e->addr = addr & ~0x3f;
    if (verbose) {
        printf("[L1]: %s. addr 0x%lx, replace 0x%lx(%d)\n", __func__, addr, r.addr, r.state);
    }
    if (DEBUG_VERBOSE_ON || lsuConfigs->verbose) {
        LOG_DEBUG(debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                  "[L1]: %s. addr 0x%lx, replace 0x%lx(%d)", __func__, addr, r.addr, r.state);
    }
    return r;
}

L1Entry L1DCache::refill(uint64_t addr, uint64_t *data, CacheState cs) {
    uint32_t set = calcSetIdx(addr);
    L1Entry r = cache[set][nway-1];
    for (uint32_t i = nway-1; i > 0; i--) {
        cache[set][i] = cache[set][i-1];
    }
    L1Entry *e = &cache[set][0];
    e->state = cs;
    for (uint32_t i = 0; i < 8; i++) {
        e->data.bits[i] = data[i];
    }
    e->addr = addr & ~0x3f;
    if (verbose) {
        printf("[L1]: %s. addr 0x%lx, replace 0x%lx(%d)\n", __func__, addr, r.addr, r.state);
    }
    if (DEBUG_VERBOSE_ON || lsuConfigs->verbose) {
        LOG_DEBUG(debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                  "[L1]: %s. addr 0x%lx, replace 0x%lx(%d)", __func__, addr, r.addr, r.state);
    }
    return r;
}

L1Entry L1DCache::refill(uint64_t addr, uint64_t *data, CacheState cs, PtrPacket pkt) {
    uint32_t set = calcSetIdx(addr);
    L1Entry r = cache[set][nway-1];
    for (uint32_t i = nway-1; i > 0; i--) {
        cache[set][i] = cache[set][i-1];
    }
    L1Entry *e = &cache[set][0];
    e->state = cs;
    for (uint32_t i = 0; i < 8; i++) {
        e->data.bits[i] = data[i];
    }
    e->addr = addr & ~0x3f;
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

void L1DCache::dataUpdate(uint64_t addr, uint64_t data, uint32_t width) {
    L1Entry *e = find(addr);
    ASSERT(e);
    ASSERT(width <= 8);
    uint32_t dwI = (addr >> 3) & 0x7;
    uint64_t *dw_p = &e->data.bits[dwI];
    uint8_t *p = (uint8_t*)dw_p;
    uint8_t *src = (uint8_t*)&data;
    uint32_t off = addr & 0x7;
    ASSERT(off + width <= 8);
    for (uint32_t i = 0; i < width; i++) {
        p[off+i] = src[i];
    }

    // DCache is replaced
    e->state = CS_MOD;
}

void L1DCache::upgradeBroadcast(uint64_t addr) {
    L1Entry *e = find(addr);
    ASSERT(e);
    bool positionVld[64];
    memset(positionVld, true, sizeof(positionVld));

    cluster->broadcastUpgrade(e->addr, e->data, positionVld);
}

uint512_t L1DCache::getData(uint64_t addr) {
    L1Entry *e = find(addr);
    ASSERT(e);
    return e->data;
}

void L1DCache::Work() {

}

void L1DCache::Xfer() {
}

SimSys *L1DCache::GetSim() {
    return sim;
}

bool checkDupSel(uint32_t bit, std::vector<uint64_t> bitsArr) {
    for (uint64_t b : bitsArr) {
        if (b == bit) {
            return true;
        }
    }

    return false;
}

void L1DCache::Build(void) {
    nset = pConfigs->dcache_nset;
    nway = pConfigs->dcache_nway;
    way_size = pConfigs->dcache_way_size;
    verbose = pConfigs->verbose;

    cache = new L1Entry*[nset];
    for (uint32_t i = 0; i < nset; i++) {
        cache[i] = new L1Entry[nway];
    }

    uint32_t seek = 6; // 6 = log2(way_size)
    while (bitList.size() < log2(pConfigs->dcache_nset) && seek < pConfigs->dcache_way_size) {
        if (!checkDupSel(seek, pConfigs->l1_disp_bit_arr)) {
            bitList.push_back(seek);
        }
        seek++;
    }
}

bool L1DCache::queryByTag(uint64_t va)
{
    uint32_t set = calcSetIdx(va);
    for (uint32_t i = 0; i < nway; i++) {
        L1Entry *e = &cache[set][i];
        if ((e->state != CS_INV) && tagMatch(e->addr, va)) {
            return true;
        }
    }

    return false;
}

} // namespace JCore