#pragma once
#ifndef ROBID_H
#define ROBID_H

#include <cstdint>
#include <functional>
#include <iostream>

namespace JCore {
/* ROB ID with wrap around bit */
struct ROBID {
    bool                    valid = true;
    bool                    wrap = false;
    uint32_t                val = 0;
    uint32_t GetVal() const { return val;}
    ROBID() = default;
    ROBID(uint32_t id, bool mark) : wrap(mark), val(id) {}
    ~ROBID() = default;
    ROBID(const ROBID&) = default;
    ROBID& operator=(const ROBID&) = default;
};

struct ROBIDKeyHash {
    std::size_t operator() (const ROBID& key) const
    {
        size_t key1 = std::hash<uint32_t>{} (key.val);
        size_t key2 = std::hash<bool>{} (key.wrap);
        return key2 ^ (key1 << 1);
    }
};

void DisableROBID(ROBID &id);
bool IsDisableROBID(const ROBID &id);

/* ROBID Operations */
/* Increment reorder buffer ID with wrap around bit */
void IncROBID(ROBID &id, uint32_t ribSize);
void AddROBID(ROBID &id, uint32_t off, uint32_t ribSize);
void SubROBID(ROBID &id, uint32_t off, uint32_t ribSize);
int  DeltaBID(ROBID a, ROBID b, uint32_t size);
/* ROBID Operations */
/* Compare two layers of ROBID */
bool LessEqual(ROBID bid0, ROBID gid0, ROBID rid0, ROBID bid1, ROBID gid1, ROBID rid1);
bool LessEqual(ROBID bid0, ROBID rid0, ROBID bid1, ROBID rid1);
bool LessEqual(ROBID bid0, ROBID bid1);
bool LessROBID(ROBID bid0, ROBID bid1);
bool LessROBID(ROBID bid0, ROBID rid0, ROBID bid1, ROBID rid1);
bool LessROBID(ROBID bid0, ROBID gid0, ROBID rid0, ROBID bid1, ROBID gid1, ROBID rid1);
bool LessEqualROBID(ROBID bid0, ROBID rid0, ROBID bid1, ROBID rid1);
uint32_t CalGap(ROBID bid0, ROBID bid1, uint64_t size);

bool operator<(ROBID lhs, ROBID rhs);
bool operator>(ROBID lhs, ROBID rhs);
bool operator<=(ROBID lhs, ROBID rhs);
bool operator>=(ROBID lhs, ROBID rhs);
bool operator==(ROBID lhs, ROBID rhs);
bool operator!=(ROBID lhs, ROBID rhs);
struct RobIdWrapView { ROBID const& id; };
inline RobIdWrapView AsWrap(ROBID const& id) { return {id}; }
std::ostream& operator<<(std::ostream& out, ROBID const &id);
std::ostream& operator<<(std::ostream& out, RobIdWrapView v);
}

#endif