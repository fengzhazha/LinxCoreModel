#pragma once

#ifndef TILE_REG
#define TILE_REG

#include "../common/DataStruct/RingQ.h"
#include "ISACommon/TileOpManager.h"
#include "ISACommon/TileInfo.h"

namespace JCore {
struct TileData {
    std::vector<uint8_t> datas;
    TileData(uint64_t size)
    {
        datas.resize(size, 0);
    }
};
using TileDataPtr = std::shared_ptr<TileData>;

struct TileEntry {
    uint64_t tag_ = 0;
    uint64_t sAddr_ = 0;
    uint64_t realSize_ = 0;
    uint64_t maxSize_ = 0;
    uint64_t minSize_ = 0;

    bool allocated = false; // Writable flag bit
    bool writeDone = false; // Single producor
    bool released = false; // Readable flag bit

    TileInfoPtr tileInfo = nullptr;
    TileDataPtr dataPtr = nullptr;

    TileEntry() = default;
    TileEntry(uint64_t tag, uint64_t sAddr, uint64_t maxSize, uint64_t minSize);
    void Reset();
    void Alloc(uint64_t size);
    uint64_t Load(uint64_t addr, uint32_t offset, uint32_t size, bool signedExtend = false);
    void Store(uint64_t addr, uint32_t offset, uint32_t size, uint64_t data);
};

struct TileRegEntry {
    BaseROB<TileEntry> tileEntries;
    OperandType tileType = OperandType::OPD_INVALID;
    uint64_t startAddr_ = 0;
    uint64_t maxTileSize_ = 0;
    uint64_t minTileSize_ = 0;

    TileRegEntry() = default;
    TileRegEntry(OperandType type, uint64_t startAddr, uint64_t indexDist, uint64_t maxOutputNum, uint64_t maxTileSize,
                 uint64_t minTileSize);
    TileEntry& Alloc(uint64_t size);
    void SetReleased(uint64_t addr);
    void SetWriteDone(uint64_t addr);

    uint64_t GetTileTag(uint64_t addr);
    uint64_t Load(uint64_t addr, uint32_t offset, uint32_t size, bool signedExtend = false);
    void Store(uint64_t addr, uint32_t offset, uint32_t size, uint64_t data);

    TileDataPtr GetTileDataPtr(uint64_t addr);
};

class TileReg {
public:
    std::unordered_map<OperandType, TileRegEntry> tileReg;
    uint64_t tileSegmSize = 0;

    TileReg() = default;
    void Init(uint64_t maxIndex, uint64_t maxOutputNum, uint64_t maxTileSize, uint64_t minTileSize);
    TileEntry& AllocTile(OperandType type, uint64_t size);
    void SetReleased(OperandType type, uint64_t addr);
    void Commit(OperandType type, uint64_t addr);

    OperandType GetTileType(uint64_t addr);
    TileEntry& LookUp(OperandType type, uint32_t distance);
    uint64_t Load(uint64_t addr, uint32_t offset, uint32_t size, bool signedExtend = false);
    void Store(uint64_t addr, uint32_t offset, uint32_t size, uint64_t data);

    TileDataPtr GetTileDataPtr(uint64_t addr);
};

}
#endif