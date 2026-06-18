#pragma once

#ifndef VECTOR_REQ_BUS_H
#define VECTOR_REQ_BUS_H

#include <unordered_map>
#include "ModelCommon/ROBID.h"
#include "ISACommon/DataType.h"

namespace JCore {

/* VECTOR <-> TMA */
enum class ReqCmdType {
    VGATHER = 0,
    VSCATTER,
    INVALID,
};

inline std::string MemCmdType2String(ReqCmdType type)
{
    static const std::unordered_map<ReqCmdType, std::string> MEM_CMD_NAME_MAP = {
        {ReqCmdType::VGATHER, "VGATHER"},
        {ReqCmdType::VSCATTER, "VSCATTER"},
        {ReqCmdType::INVALID, "INVALID"},
    };
    return MEM_CMD_NAME_MAP.at(type);
}

struct VectorBridgeReq {
    ReqCmdType cmdType = ReqCmdType::INVALID;
    ROBID    bid;       /* Using for TMA pipeview */
    uint32_t tid = 0;  // TransactionId
    uint32_t groupSlotId = 0;
    uint32_t stid = -1U;
    // 因为一个vector是64 lane，mask区分哪些lane有效(64 bit)。每个lane大小是uint32(4 Byte). 64*4=256 Byte
    // 现在vector是64个lane, 但是predicateLaneMask =0x1FF0F, 总共12个有效位（里面还有真实offset的值）。
    // 从soc取load数据（13个）， 也传0x1FF0F对应为1的为准填写dstData的值写到tileReg ?
    // Vector也仅仅predicateLaneMask对应的有效位的data
    uint64_t predicateLaneMask = 0;
    // SrcType源固定是uint32_t，64lane, 64*4=256 Byte 位宽
    // 默认目的DstData是uint32_t类型
    DataType          dstDataType = DataType::INVALID;
    DataType          srcDataType = DataType::INVALID;  // 地址类型

    // Tile register 中的读Offset基址。目前约定baseAddrTR和baseAddrDstTR是同一个地址，Offset内容要求128B对齐
    uint64_t                baseAddrTR = 0;
    // Tile register 中的写Data的基址
    uint64_t                baseAddrDstTR = 0;
};

struct VectorBridgeRsp {
    uint32_t tid = 0;  // TransactionId
    uint32_t groupSlotId = 0;
};

}

#endif