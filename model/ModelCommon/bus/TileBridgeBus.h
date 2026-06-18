#pragma once

#ifndef TILE_BRIDGE_BUS_H
#define TILE_BRIDGE_BUS_H

#include <cstdint>

#include "ISA.h"

namespace JCore {
struct TileBridgeBus {
    bool                    vld = false;
    TileOp                  op = TileOp::TINVALID;
    DataType                dType = DataType::INT16;
    LayOut                  layout = LayOut::LAYOUT_COUNT;
    // Global momery 第一维：ND时表示列个数， DN时表示行个数
    uint32_t                d1GM = 0;
    // Global momery 第二维：ND时表示行个数， DN时表示列个数
    uint32_t                d2GM = 0;
    // Global momery 中第二维中开头两个元素的距离
    uint32_t                strideGM = 0;
    // Tile register 中形态的列个数
    uint32_t                d1TR = 0;
    // Tile register 中形态的行个数
    uint32_t                d2TR = 0;
    // Tile register 中的基址
    uint64_t                baseAddrTR = 0;
    uint64_t                baseAddrDstTR = 0;
    // load/store id
    uint64_t                sid = 0;
    uint64_t                load_id = 0;
    // FIXME: ptag of dst tile reg
    uint64_t                ptag = 0;
    uint64_t                CalcRealSize()
    {
        return (BytesOf(dType) * d1GM * d2GM);
    }
    uint64_t                CalcMemSize()
    {
        return d2GM * strideGM;
    }
    uint64_t                CalcD1Size()
    {
        return d1GM * BytesOf(dType);
    }
};
}
#endif