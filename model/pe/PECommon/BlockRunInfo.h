#pragma once

#ifndef BLOCK_RUN_INFO_H
#define BLOCK_RUN_INFO_H

#include "core/Bus.h"
#include "ModelCommon/ROBID.h"
#include "ModelCommon/ShapeLoopInfo.h"
#include "ModelCommon/ModelEnumDefines.h"
#include "ModelCommon/BlockCommand.h"

namespace JCore {
struct BlockRunInfo {
    bool                        vld = false;
    bool                        lastFlag = false;
    ROBID                       bid;
    ROBID                       gid;
    uint32_t stid = -1U;
    /* \brief The id of the group within the block, with each block starting from 0 */
    uint32_t                    logicalGID = 0;
    ShapeLoopInfo               shapelpinfo;
    uint32_t                    tid = 0;
    bool                        blkNoBranch = true;
    bool                        blkRenamed = false;
    BlkSrcType                  blkSrcType = BlkSrcType::NONE_SRC;
    uint32_t                    gSetMask = 0;
    bool                        isTemplate = false;
    CodeTemplateID              templateID = CodeTemplateID::NON_TEMPLATE;
    TemplateIDSet templateIDSet;
    BlockCommandPtr             blockCmd = nullptr;
    uint32_t                    genInstCnt = 0;
    uint32_t                    genNoFixInstCnt = 0;
    uint32_t                    specSetMask = 0;
    uint32_t                    specGetMask = 0;
    uint32_t                    getBitMask = 0;
    uint32_t                    setBitMask = 0;
    uint32_t                    totalInstCnt = 0;
    uint32_t                    instWdith = 0;
    SimQueue<SimInst>             extendInstQ;
    SimQueue<SimInst>             alignInstQ;
    SimQueue<SimInst>             endingInstQ;
    bool                        firstDecode = true;
    uint32_t                    srcOffset = 0;
    uint32_t                    dstOffset = 0;
    uint32_t                    ct_stCnt = 0;
    uint32_t                    ct_ldCnt = 0;
    MemCopyStatus               followStatus = MemCopyStatus::FOLLOW_LD;
    bool                        bigBlk = false;
    TileOp                      tileOpID;

    uint32_t shapeK = 1;
    uint32_t shapeM = 1;
    uint32_t shapeN = 1;

    bool                        ct_vld = false;
    bool                        isGenDczvaInst = false;

    BlockRunInfo() = default;
};
}
#endif
