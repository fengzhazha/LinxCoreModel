#pragma once

#ifndef TILE_INFO_H
#define TILE_INFO_H

#include <memory>

#include "LayOut.h"
#include "DataType.h"

namespace JCore {
struct TileInfo {
    DataType dataType = DataType::INVALID;

    /* \brief Tile info bug not-use */
    uint64_t row = 0;
    uint64_t col = 0;
    uint64_t validRow = 0;
    uint64_t validCol = 0;
    uint64_t padValue = 0;
    LayoutType bigLayout = LayoutType::INVALID;
    LayoutType smallLayout = LayoutType::INVALID;

    TileInfo() = default;
};
using TileInfoPtr = std::shared_ptr<TileInfo>;
}
#endif