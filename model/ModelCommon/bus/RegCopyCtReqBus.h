#pragma once

#ifndef REG_COPY_CT_REQ_BUS_H
#define REG_COPY_CT_REQ_BUS_H

#include "../ROBID.h"
#include "ISACommon/DataType.h"

namespace JCore {
struct CopyCtReqBus {
    uint32_t                tid = 0;
    uint32_t                tpc = 0;
    uint32_t                transactionId = 0;
    uint32_t                groupSlotId = 0;
    uint32_t                gprAddr = 0;
    bool                    isCopyOut = false;
    OperandType             copyType = OperandType::OPD_INVALID;
    bool                    needCopyIn = false;
    bool                    isCopyIn = false;
    ROBID                   bid;
    ROBID                   gid;
    uint32_t                stid = -1U;
    ShapeLoopInfo           shapelpinfo;
    DataType                addrDataType = DataType::INVALID;  // 用于VGATHER: 保存原始LOAD指令的地址数据类型
};

}
#endif
