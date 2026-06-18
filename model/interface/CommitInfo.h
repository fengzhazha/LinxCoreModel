#pragma once
#ifndef BLOCKISA_MODEL_COMMITINFO_H
#define BLOCKISA_MODEL_COMMITINFO_H

#include <cstdint>
#include <cstddef>

namespace JCore {

class CommitInfo {
public:
    uint32_t       bid;
    uint32_t       gid;
    uint32_t       rid;
    CommitInfo(uint32_t b, uint32_t g, uint32_t r)
        : bid(b), gid(g), rid(r) {}

    bool operator==(const CommitInfo& other) const
    {
        return bid == other.bid && gid == other.gid && rid == other.rid;
    }
};

class CommitInfoHash {
public:
    size_t operator()(const CommitInfo& ci) const
    {
        size_t h1 = std::hash<uint64_t>{}(ci.bid);
        size_t h2 = std::hash<uint64_t>{}(ci.gid);
        size_t h3 = std::hash<uint64_t>{}(ci.rid);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

}
#endif  // BLOCKISA_MODEL_COMMITINFO_H