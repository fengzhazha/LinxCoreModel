#include "TileReg.h"

#include "MInst.h"

namespace JCore {
void TileReg::Init(uint64_t maxIndex, uint64_t maxOutputNum, uint64_t maxTileSize, uint64_t minTileSize)
{
    uint64_t addr = 0;
    tileSegmSize = ((maxIndex + maxOutputNum) * maxTileSize);
    tileReg[OperandType::OPD_TILE_TLINK] = TileRegEntry(OperandType::OPD_TILE_TLINK, addr, maxIndex, maxOutputNum,
        maxTileSize, minTileSize);
    addr += tileSegmSize;
    tileReg[OperandType::OPD_TILE_ULINK] = TileRegEntry(OperandType::OPD_TILE_ULINK, addr, maxIndex, maxOutputNum,
        maxTileSize, minTileSize);
    addr += tileSegmSize;
    tileReg[OperandType::OPD_TILE_MLINK] = TileRegEntry(OperandType::OPD_TILE_MLINK, addr, maxIndex, maxOutputNum,
        maxTileSize, minTileSize);
    addr += tileSegmSize;
    tileReg[OperandType::OPD_TILE_NLINK] = TileRegEntry(OperandType::OPD_TILE_NLINK, addr, maxIndex, maxOutputNum,
        maxTileSize, minTileSize);
    addr += tileSegmSize;
    tileReg[OperandType::OPD_TILE_ACC] = TileRegEntry(OperandType::OPD_TILE_ACC, addr, 1, 1, maxTileSize * NUM2,
        minTileSize * NUM2);
    addr += tileSegmSize;
    tileReg[OperandType::OPD_TILE_STACK] = TileRegEntry(OperandType::OPD_TILE_STACK, addr, 1, 1, maxTileSize,
        minTileSize);
}

TileEntry& TileReg::AllocTile(OperandType type, uint64_t size)
{
    return tileReg[type].Alloc(size);
}

void TileReg::SetReleased(OperandType type, uint64_t addr)
{
    tileReg[type].SetReleased(addr);
}

void TileReg::Commit(OperandType type, uint64_t addr)
{
    tileReg[type].SetWriteDone(addr);
}

OperandType TileReg::GetTileType(uint64_t addr)
{
    OperandType res = TileIdx2TileType(addr / tileSegmSize);
    ASSERT(OperandTypeIsTile(res)) << "Unsupport tile type " << static_cast<uint64_t>(res);
    return res;
}

uint64_t TileReg::Load(uint64_t addr, uint32_t offset, uint32_t size, bool signedExtend)
{
    OperandType type = GetTileType(addr);
    return tileReg[type].Load(addr, offset, size, signedExtend);
}

void TileReg::Store(uint64_t addr, uint32_t offset, uint32_t size, uint64_t data)
{
    OperandType type = GetTileType(addr);
    return tileReg[type].Store(addr, offset, size, data);
}

TileEntry& TileReg::LookUp(OperandType type, uint32_t distance)  // 返回baseAddr
{
    auto it = tileReg.find(type);
    assert((it != tileReg.end()) && "No matching tileReg was found");
    TileEntry &tileEntry = it->second.tileEntries.BackToFrontIndex(distance);
    return tileEntry;
}

TileDataPtr TileReg::GetTileDataPtr(uint64_t addr)
{
    OperandType type = GetTileType(addr);
    return tileReg[type].GetTileDataPtr(addr);
}

TileRegEntry::TileRegEntry(OperandType type, uint64_t startAddr, uint64_t indexDist, uint64_t maxOutputNum,
                           uint64_t maxTileSize, uint64_t minTileSize)
{
    tileType = type;
    startAddr_ = startAddr;
    maxTileSize_ = maxTileSize;
    minTileSize_ = minTileSize;
    tileEntries = BaseROB<TileEntry>(indexDist + maxOutputNum, indexDist + maxOutputNum, "Func" + GetTileTypeStr(type));
    tileEntries.BuildWithInit([&startAddr, &maxTileSize, &minTileSize](uint32_t i) -> TileEntry {
        return TileEntry(i, startAddr + i * maxTileSize, maxTileSize, minTileSize);
    });
}

TileEntry& TileRegEntry::Alloc(uint64_t size)
{
    if (tileEntries.IsFull()) {
        auto &deEntry = tileEntries.Dealloc();
        deEntry.Reset();
    }
    auto &entry = tileEntries.Alloc();
    entry.Alloc(size);
    return entry;
}

void TileRegEntry::SetReleased(uint64_t addr)
{
    uint64_t tag = GetTileTag(addr);
    tileEntries[tag].released = true;
}

void TileRegEntry::SetWriteDone(uint64_t addr)
{
    uint64_t tag = GetTileTag(addr);
    tileEntries[tag].writeDone = true;
}

uint64_t TileRegEntry::GetTileTag(uint64_t addr)
{
    return (addr - startAddr_) / maxTileSize_;
}

uint64_t TileRegEntry::Load(uint64_t addr, uint32_t offset, uint32_t size, bool signedExtend)
{
    uint64_t tag = GetTileTag(addr);
    return tileEntries[tag].Load(addr, offset, size, signedExtend);
}

void TileRegEntry::Store(uint64_t addr, uint32_t offset, uint32_t size, uint64_t data)
{
    uint64_t tag = GetTileTag(addr);
    tileEntries[tag].Store(addr, offset, size, data);
}

TileDataPtr TileRegEntry::GetTileDataPtr(uint64_t addr)
{
    uint64_t tag = GetTileTag(addr);
    return tileEntries[tag].dataPtr;
}

TileEntry::TileEntry(uint64_t tag, uint64_t sAddr, uint64_t maxSize, uint64_t minSize)
    : tag_(tag), sAddr_(sAddr), realSize_(0U), maxSize_(maxSize), minSize_(minSize)
{
}

void TileEntry::Reset()
{
    dataPtr = nullptr;
    tileInfo = nullptr;
    allocated = false;
    writeDone = false;
    released = false;
}

void TileEntry::Alloc(uint64_t size)
{
    /* ASSERT(size == 0 || (size >= minSize_ && size <= maxSize_)); */
    Reset();
    dataPtr = std::make_shared<TileData>(maxSize_);
    realSize_ = size;
}

uint64_t TileEntry::Load(uint64_t addr, uint32_t offset, uint32_t size, bool signedExtend)
{
    ASSERT(size > 0) << "src tile size cannot be 0";
    uint64_t data = 0;
    uint64_t baseAddr = (addr + offset);
    uint64_t localPos = baseAddr - sAddr_;
    ASSERT(localPos + size <= maxSize_) << std::hex << "TileLoad: 0x" << baseAddr << " size: 0x" << size
        << "Load address is overflow!";

    uint32_t loadSize = 0;
    auto handleSigned = [](uint64_t& data, uint32_t size, bool signedLoad) {
        switch (size) {
            case 8:
                break;
            case 4:
                if (signedLoad) {
                    if (data & (1 << 31)) {
                        data |= 0xFFFFFFFF00000000;
                    }
                }
                break;
            case 2:
                if (signedLoad) {
                    if (data & (1 << 15)) {
                        data |= 0xFFFFFFFFFFFF0000;
                    }
                }
                break;
            case 1:
                if (signedLoad) {
                    if (data & (1 << 7)) {
                        data |= 0xFFFFFFFFFFFFFF00;
                    }
                }
                break;
            default:
                break;
        }
    };
    while (size != loadSize) {
        data |= static_cast<uint64_t>(dataPtr->datas[localPos]) << (NUM8 * loadSize);
        loadSize++;
        localPos++;
    }
    if (signedExtend) {
        handleSigned(data, size, signedExtend);
    }
    return data;
}

void TileEntry::Store(uint64_t addr, uint32_t offset, uint32_t size, uint64_t data)
{
    if (size == 0) {
        return;
    }
    uint64_t baseAddr = (addr + offset);
    uint32_t pos = baseAddr - sAddr_;
    ASSERT(pos + size <= maxSize_) << std::hex << "baseAddr: 0x" << baseAddr << " size: 0x" << size
        << " Store address is overflow!";
    uint32_t storeSize = 0;
    while (size != storeSize) {
        dataPtr->datas[pos] = static_cast<uint8_t>(data & ((1 << NUM8) - 1));
        data = data >> NUM8;
        storeSize++;
        pos++;
    }
}
}
