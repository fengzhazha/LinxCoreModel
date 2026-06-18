#pragma once

#ifndef MEM_REQUEST_H
#define MEM_REQUEST_H

#include <cstdint>

#include "ISA.h"
#include "../ROBID.h"
#include "../SimInstInfo.h"

namespace JCore {
struct MemRequest {
    uint64_t uid = 0;
    bool val = false;
    bool isLoad = false;
    VecData addrs;
    uint32_t tpc = 0;
    uint32_t peID = 0;
    Opcode opcode = Opcode::OP_INVALID;
    uint32_t laneSize = 0;
    uint32_t realReqCnt = 0;
    uint32_t lanes = 0;
    uint32_t tileLdqCyc = 0;
    uint32_t tileLdMissCyc = 0;
    uint32_t tileLdRefillCyc = 0;
    uint64_t e2Cyc = 0;
    uint64_t e3Cyc = 0;
    uint64_t e4Cyc = 0;
    ROBID bid;
    ROBID rid;
    ROBID subrid;
    // TODO: gid
    ROBID gid;
    ROBID lsID;
    uint32_t tid = 0;
    uint32_t thread = 0;
    VecData data;
    uint64_t mask = 0;
    uint32_t width = 0;
    uint32_t start = 0;
    uint32_t waitToResolve = 0;
    uint32_t stid = -1U;
    bool toMemory = false;
    bool gatherLd = false;
    bool isMTCMemReq = false;
    bool isCTInst = false;

    ReqCmdType cmdType = ReqCmdType::INVALID;
    uint32_t transactionId = 0;
    uint32_t groupSlotId = 0;
    DataType dType = DataType::INVALID;

    // for debug
    SimInst uinst;
};
}
#endif