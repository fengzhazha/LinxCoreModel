#pragma once

#ifndef PE_CONTROL_BUS_H
#define PE_CONTROL_BUS_H

#include <cstdint>

#include "../ROBID.h"
#include "../ModelEnumDefines.h"
#include "../ModelCommon/SimInstInfo.h"

namespace JCore {
/* Bus format for control request */
struct PEControlBus {
    bool                    vld;
    ROBID                   bid;
    uint64_t                bpc;
    uint64_t                tpc;
    uint64_t                fetchTpc;
    /* \brief block header size */
    uint32_t                hsize;
    /* \brief block body size */
    uint32_t                bsize;
    /* \brief block Micro-instruction size (unit is Bytes) */
    uint32_t                isize;
    bool                    isTemplate;
    CodeTemplateID          templateID;
    TemplateIDSet templateIDSet;
    /* \brief Source of block fetch information */
    BlkSrcType              blkSrcType;
    uint32_t                getMask;
    uint32_t                setMask;

    bool                    noBranch;
    bool                    empty;
    bool                    noSplitBlk = true;
    BlockType               btype = BlockType::BLK_TYPE_INVAL;
    PEControlBus()
    {
        vld = false;
        bid = ROBID();
        bpc = 0;
        tpc = 0;
        fetchTpc = 0;
        hsize = 0;
        bsize = 0;
        isize = 0;
        isTemplate = false;
        templateID = CodeTemplateID::NON_TEMPLATE;
        blkSrcType = BlkSrcType::NONE_SRC;
        noBranch = false;
        empty = false;
        getMask = 0;
        setMask = 0;
    }
};

struct PEStatusBus {
    bool                    vld;
    bool                    available;
    bool                    isMemType;
};
}
#endif