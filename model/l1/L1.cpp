#include "l1/L1.h"

namespace JCore {

using namespace std;

L1Cache::L1Cache() {}
L1Cache::~L1Cache() {}

L1Entry* L1Cache::find(uint64_t addr) {
    L1Entry *r = nullptr; 
    uint32_t set = calcSetIdx(addr);
    for (uint32_t i = 0; i < nway; i++) {
        L1Entry *e = &cache[set][i];
        if (e->state != CS_INV && tagMatch(e->addr, addr)) {
            r = e;
            break;
        }
    }
    return r;
}

uint32_t L1Cache::calcSetIdx(uint64_t va) {
    return (va>>6) & (nset-1);  // 6 = (uint64_t)log2(way_size)
}

uint64_t L1Cache::transToPa(uint64_t va) {
    return va>>6;
}

bool L1Cache::tagMatch (uint64_t a0, uint64_t a1) {
    return (a0>>6) == (a1>>6);
}

void L1Cache::updateLRU(uint32_t set, uint32_t way) {
    L1Entry tmp = cache[set][way];
    for (uint32_t j = way; j > 0; j--) {
        cache[set][j] = cache[set][j-1];
    }
    cache[set][0] = tmp;
}

L1Entry L1Cache::invd(uint64_t addr) {
    L1Entry *e = find(addr);
    if (e == nullptr) {
        L1Entry r;
        r.state = CS_INV;
        return r;
    }
    ASSERT(e);
    L1Entry r = *e;
    e->state = CS_INV;
    return r;
}

L1Entry L1Cache::getS(uint64_t addr) {
    L1Entry *e = find(addr);
    if (e == nullptr) {
        L1Entry r;
        r.state = CS_INV;
        return r;
    }
    ASSERT(e);
    L1Entry r = *e;
    ASSERT(e->state == CS_MOD || e->state == CS_EXCL);
    e->state = CS_SHARE;
    return r;
}

bool L1Cache::upgrade(uint64_t addr) {
    L1Entry *e = find(addr);
    if (e == nullptr) {
        return false;
    }
    ASSERT(e->state == CS_SHARE);
    e->state = CS_EXCL;
    return true;
}

void L1Cache::Reset() {

}

void L1Cache::Work() {

}

void L1Cache::Xfer() {

}

SimSys *L1Cache::GetSim() {
    return NULL;
}

void L1Cache::Build() {

}

} // namespace JCore