#pragma once

#ifndef BLK_BODY_FETCH_STATE_H
#define BLK_BODY_FETCH_STATE_H

#include <cstdint>

#include "../ROBID.h"
#include "../ShapeLoopInfo.h"
#include "../ModelEnumDefines.h"
#include "ModelSpec.h"

namespace JCore {
struct BlkBodyFetchState {
    bool                        vld = false;
    ROBID                       bid;
    ROBID                       gid;
    uint32_t                    stid = -1U;
    /* \brief The id of the group within the block, with each block starting from 0 */
    uint32_t                    logicalGID = 0;
    ShapeLoopInfo               shapelpinfo;
    uint64_t                    bpc = 0;
    uint64_t                    startTPC = 0;
    uint64_t                    fetchTPC = 0;
    uint64_t                    endTPC = 0;
    bool                        isTemplate = false;
    CodeTemplateID              templateID = CodeTemplateID::NON_TEMPLATE;
    /* \brief Source of block fetch information */
    BlkSrcType                  blkSrcType = BlkSrcType::NONE_SRC;
    BIQType                     biqType = BIQType::NONE_IQ;
    bool                        first = false;
    uint32_t                    hsize = 0;
    uint32_t                    isize = 0;
    bool                        noBranch = false;
    HyperTraceEntry             hyperTrace;
    BlockType                   btype = BlockType::BLK_TYPE_INVAL;
    uint64_t                    mask = 0xFFFFFFFFFFFFFFFF;
    bool                        waitForBranch = false;

    BlkBodyFetchState() {}
};

}
#endif