#ifndef  S_INTERFACE_COMMON_H
#define S_INTERFACE_COMMON_H

#include <cstdint>

#include "core/Bus.h"
#include "interface/BCCCubeFlush.h"
#include "interface/BCCMemLdReq.h"
#include "interface/BCCMemSnpRes.h"
#include "interface/BCCMemStReq.h"
#include "interface/BCCMMUStReq.h"
#include "interface/BCCTTransFlush.h"
#include "interface/BCCVecFlush.h"
#include "ModelCommon/BlockCommand.h"
#include "interface/CubeTileRegLdReq.h"
#include "interface/CubeTileRegStReq.h"
#include "interface/MemBCCLdRes.h"
#include "interface/MemBCCStRes.h"
#include "interface/MemTTransLdRes.h"
#include "interface/MemTTransStRes.h"
#include "interface/TileRegCubeLdRes.h"
#include "interface/TileRegCubeLdRetry.h"
#include "interface/TileRegCubeStRetry.h"
#include "interface/TileRegTTransLdRes.h"
#include "interface/TileRegTTransLdRetry.h"
#include "interface/TileRegTTransStRetry.h"
#include "interface/TileRegVecLdRes.h"
#include "interface/TileRegVecLdRetry.h"
#include "interface/TileRegVecStRetry.h"
#include "interface/TTransMemLdReq.h"
#include "interface/TTransMemStReq.h"
#include "interface/TTransTileRegLdReq.h"
#include "interface/TTransTileRegStReq.h"
#include "interface/VecTileRegLdReq.h"
#include "interface/VecTileRegStReq.h"
#include "interface/Credit.h"

namespace JCore {
class InterfaceCommon {
public:
    // interface connection
};
} // namespace JCore

#endif