#include "tmu/TileUtils.h"

#include <cmath>

namespace JCore {

void TileUtils::Build(std::shared_ptr<ConfigInput> args)
{
    configs.overrideDefaultConfig(args);
    maxAddr = Bytes2Addr(configs.tile_reg_size * KILO) - 1;
    static const std::unordered_map<ChannelType, std::string> CHANNEL_NAME_MAP = {
        {ChannelType::REQ0, "REQ0"},
        {ChannelType::REQ1, "REQ1"},
        {ChannelType::DATA, "DATA"},
        {ChannelType::RSP, "RSP"},
        {ChannelType::SNOOP, "SNOOP"},
        {ChannelType::COUNT, "UNKNOWN"},
    };
    for (uint64_t c = 0; c < static_cast<uint64_t>(ChannelType::COUNT); ++c) {
        for (uint64_t n = 0; n < GetNodeNum(); ++n) {
            uint64_t swimId = c * GetNodeNum() + n;
            std::string swimName = "-node " + std::to_string(n) + "-"
                                   + CHANNEL_NAME_MAP.at(static_cast<ChannelType>(c));
            ringThreadNameMap.emplace(swimId, swimName);
        }
    }
}

uint64_t TileUtils::GetRingThreadID(uint64_t nodeID, ChannelType cType)
{
    return static_cast<uint64_t>(cType) * GetNodeNum() + nodeID;
}

uint64_t TileUtils::GetThreadNum()
{
    return GetNodeNum() * static_cast<uint64_t>(ChannelType::COUNT);
}

std::unordered_map<uint64_t, std::string> TileUtils::GetThreadNameMap()
{
    return ringThreadNameMap;
}

std::string TileUtils::GetThreadName(uint64_t threadID)
{
    return ringThreadNameMap.at(threadID);
}

uint32_t TileUtils::Bytes2TileBusSize(uint32_t size)
{
    if (size <= MIN_LS_DATA_BYETS) {
        return 0;
    }
    ASSERT(size <= MAX_TILE_DATA_BYTE && "Data size is too large!");
    return log2(size / MIN_LS_DATA_BYETS);
}

uint32_t TileUtils::TileBusSize2Bytes(uint32_t size)
{
    return (MIN_LS_DATA_BYETS * pow(LOG_2, size));
}

uint32_t TileUtils::Bytes2Addr(uint32_t size)
{
    return size / configs.bank_element_size;
}

uint32_t TileUtils::Addr2Bytes(uint32_t addr)
{
    return addr * configs.bank_element_size;
}

uint32_t TileUtils::CalcRequestNum(uint32_t size)
{
    uint32_t cnt = 0;
    uint32_t reqSize = MAX_TILE_DATA_BYTE;
    while (size != 0 && reqSize >= configs.bank_element_size) {
        cnt += size / reqSize;
        size -= (size / reqSize) * reqSize;
        reqSize /= LOG_2;
        if (reqSize == configs.bank_element_size && size != 0) {
            cnt += 1;
            size = 0;
        }
    }
    ASSERT(size == 0 && "Something wrong in calculating request number!");
    return cnt;
}

uint32_t TileUtils::GetSingleReqSize(uint32_t size)
{
    uint32_t div = 1;
    while (MAX_TILE_DATA_BYTE / div >= configs.bank_element_size) {
        if (size >= MAX_TILE_DATA_BYTE / div ||
            (size < MAX_TILE_DATA_BYTE / div && MAX_TILE_DATA_BYTE / div == configs.bank_element_size)) {
            return MAX_TILE_DATA_BYTE / div;
        }
        div <<= 1;
    }
    return 0;
}

uint32_t TileUtils::CalcAddrByBytesOffset(uint32_t addr, uint32_t size)
{
    if (size < configs.bank_element_size) {
        size = configs.bank_element_size;
    }
    return (addr + Bytes2Addr(size)) % (maxAddr + 1);
}

uint32_t TileUtils::CalcBytesAddrOffset(uint32_t base, uint32_t offset)
{
    return (base + offset) % Addr2Bytes(maxAddr + 1);
}

void TileUtils::CheckAddrOverflow(uint32_t addr, uint32_t size)
{
    uint32_t endAddr = addr + Bytes2Addr(size);
    if (addr > maxAddr || endAddr > maxAddr + 1) {
        std::cerr << "Tile address overflow! Address range: [";
        std::cerr << std::hex << "0x" << addr << ",0x" << endAddr;
        std::cerr << ")" << "; Maximum address: 0x" << maxAddr << std::endl;
        ASSERT(0);
    }
}

bool TileUtils::CheckOverflow(uint32_t addr, uint32_t size)
{
    uint32_t endAddr = addr + Bytes2Addr(size);
    if (addr > maxAddr || endAddr > maxAddr + 1) {
        std::cerr << "Tile address overflow! Address range: [";
        std::cerr << std::hex << "0x" << addr << ",0x" << endAddr;
        std::cerr << ")" << "; Maximum address: 0x" << maxAddr << std::endl;
        return false;
    }

    return true;
}

const TMUConfig *const TileUtils::GetUBConfig()
{
    return &configs;
}

uint32_t TileUtils::GetDstNode(uint32_t addr)
{
    return (Addr2Bytes(addr) / configs.node_intervel) % GetNodeNum();
}

uint32_t TileUtils::GetNodeNum()
{
    return configs.ring_node_num;
}

bool TileUtils::OnlyCubeToRing()
{
    return configs.only_cube_to_ring;
}

bool TileUtils::IsXbarMode()
{
    return configs.tmu_mode == 1;
}

bool TileUtils::IsRingOrXbarMode(bool isCube)
{
    return IsXbarMode() || IsRingMode(isCube);
}

bool TileUtils::IsRingMode()
{
    return configs.tmu_mode == 0;
}

bool TileUtils::IsRingMode(bool isCube)
{
    return IsRingMode() && (isCube || (!isCube && !OnlyCubeToRing()));
}


} // namespace JCore
