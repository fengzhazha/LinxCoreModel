#pragma once

#include <cstdint>
#include <cstdio>
#include <memory>
#include <unordered_set>

#include "core/Packet.h"

namespace JCore {

struct L1Pkt {
    bool writeback = false; // 0: lookup, 1: writeback
    bool dirty = false;     // 0: clean, 1: dirty
    bool write = false;     // 0: read, 1: write
    bool share = false;
    uint512_t data;
    uint64_t addr = 0;
    int src = 0;
};

struct L2Pkt {
    bool inv = false;
    bool getS = false;
    bool owned = false; // false: shared, true: owned
    int dst = 0;
    uint64_t addr = 0;
    uint512_t data;
};

struct MemReq {
    int delay = 0;
    int id = 0;
    bool write = false;
    uint64_t addr = 0;
    uint512_t data;
};

struct MemResp {
    int id = 0;
    uint512_t data;
};

struct L2Entry {
    bool valid = false;
    bool dirty = false;
    bool share = false;
    uint64_t tag = 0;
    uint64_t addr = 0;
    uint512_t data;
    std::unordered_set<int> hold_set;
    uint32_t pref_type = 0;
    bool prefetch = false;
    bool demand = false;
};

} // namespace JCore