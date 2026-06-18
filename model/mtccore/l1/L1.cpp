#include "mtccore/l1/MTC_L1Cache.h"

namespace JCore {

using namespace std;

MTC_L1Cache::MTC_L1Cache() {}
MTC_L1Cache::~MTC_L1Cache() {}

MTC_L1Entry* MTC_L1Cache::find(uint64_t addr)
{
    MTC_L1Entry *r = nullptr;
    uint32_t set = calcSetIdx(addr);
    for (uint32_t i = 0; i < nway; i++) {
        MTC_L1Entry *e = &cache[set][i];
        if (e->state != MTC_CS_INV && tagMatch(e->addr, addr)) {
            r = e;
            break;
        }
    }
    return r;
}

uint32_t MTC_L1Cache::calcSetIdx(uint64_t va)
{
    return (va>>8) & (nset-1);  // 8 = (uint64_t)log2(way_size)
}

uint64_t MTC_L1Cache::transToPa(uint64_t va)
{
    return va>>8;
}

bool MTC_L1Cache::tagMatch (uint64_t a0, uint64_t a1)
{
    return (a0>>8) == (a1>>8);
}

void MTC_L1Cache::updateLRU(uint32_t set, uint32_t way)
{
    MTC_L1Entry tmp = cache[set][way];
    for (uint32_t j = way; j > 0; j--) {
        cache[set][j] = cache[set][j-1];
    }
    cache[set][0] = tmp;
}

MTC_L1Entry MTC_L1Cache::invd(uint64_t addr)
{
    MTC_L1Entry *e = find(addr);
    if (e == nullptr) {
        MTC_L1Entry r;
        r.state = MTC_CS_INV;
        return r;
    }
    ASSERT(e);
    MTC_L1Entry r = *e;
    e->state = MTC_CS_INV;
    return r;
}

void MTC_L1Cache::Reset()
{
}

void MTC_L1Cache::Work()
{
}

void MTC_L1Cache::Xfer()
{
}

SimSys *MTC_L1Cache::GetSim()
{
    return NULL;
}

void MTC_L1Cache::Build()
{
}

} // namespace JCore