#ifndef  S_TILE_BUS_TRANSFER_H
#define S_TILE_BUS_TRANSFER_H

#include "../configs/tmu_config.h"
#include "interface/ConstConfig.h"
#include "include/Request.h"

namespace JCore {

class TileUtils {
public:
    void                                    Build(std::shared_ptr<ConfigInput> args);
    uint32_t                                Bytes2TileBusSize(uint32_t bytes);
    uint32_t                                TileBusSize2Bytes(uint32_t bytes);
    uint32_t                                Bytes2Addr(uint32_t bytes);
    uint32_t                                Addr2Bytes(uint32_t addr);
    uint32_t                                CalcRequestNum(uint32_t size);
    uint32_t                                GetSingleReqSize(uint32_t size);
    uint32_t                                CalcAddrByBytesOffset(uint32_t addr, uint32_t size);
    uint32_t                                CalcBytesAddrOffset(uint32_t base, uint32_t offset);
    void                                    CheckAddrOverflow(uint32_t addr, uint32_t size);
    bool                                    CheckOverflow(uint32_t addr, uint32_t size);
    const TMUConfig *const                  GetUBConfig();
    uint32_t                                GetDstNode(uint32_t addr);
    uint32_t                                GetNodeNum();
    bool                                    IsRingMode(bool isCube);
    bool                                    IsXbarMode();
    bool                                    IsRingOrXbarMode(bool isCube);
    uint64_t                                GetThreadNum();
    std::unordered_map<uint64_t, std::string> GetThreadNameMap();
    std::string                             GetThreadName(uint64_t threadID);
    uint64_t                                GetRingThreadID(uint64_t nodeID, ChannelType cType);
    // uint64_t                                AlignTo256B()
    TMUConfig                               configs;
private:
    uint32_t                                maxAddr = 0;
    std::unordered_map<uint64_t, std::string> ringThreadNameMap;
    bool                                    OnlyCubeToRing();
    bool                                    IsRingMode();
};

} // namespace JCore

#endif
