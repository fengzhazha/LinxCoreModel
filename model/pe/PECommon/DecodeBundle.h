#pragma once

#ifndef DECODE_BUNDLE_H
#define DECODE_BUNDLE_H

#include "core/Bus.h"
#include "ModelCommon/ROBID.h"
#include "ModelCommon/ShapeLoopInfo.h"
#include "ModelCommon/ModelEnumDefines.h"
#include "ModelCommon/BlockCommand.h"

namespace JCore {
struct DecodeBundle {
    bool                        vld = false;
    ROBID                       bid;
    ROBID                       gid;
    uint32_t                    stid = -1U;
    /* \brief The id of the group within the block, with each block starting from 0 */
    uint32_t                    logicalGID = 0;
    ShapeLoopInfo               shapelpinfo;
    uint64_t                    bpc = 0;
    uint64_t                    tpc = 0;
    uint64_t                    endTPC = 0;
    uint64_t                    fid = 0;
    uint64_t                    tid = 0;
    /* \brief How many instructions are effective: 1, 2, 3 .. DWIDTH */
    uint32_t                    mask = 0;
    /* \brief Whether this bundle is the first of block */
    bool                        first = false;
    /* \brief Whether this bundle is the last of block */
    bool                        last = false;
    /* \brief Source of block fetch information */
    BlkSrcType                  blkSrcType = BlkSrcType::NONE_SRC;
    /* \brief Instruction data */
    std::vector<uint64_t>       entry;
    bool                        blockNoBranch = false;
    bool                        bundleNoBranch = false;
    uint32_t                    hsize = 0;
    std::vector<uint64_t>       isize;
    std::vector<uint64_t>       tpcArr;
    BlockType                   btype = BlockType::BLK_TYPE_INVAL;
    BIQType                     biqType = BIQType::NONE_IQ;

    CycleBus                   pipe_cycle;

    DecodeBundle() = default;
    DecodeBundle(uint32_t size)
    {
        entry.resize(size);
    }

    std::string Dump()
    {
        std::stringstream oss;
        oss << "Fetch Bundle bid:" << bid;
        oss << " gid:" << gid;
        oss << " tid:" << tid;
        oss << " fid:" << fid;
        oss << " fetchTPC:0x" << std::hex << tpc;
        oss << " instNum:" << std::dec << mask;
        return oss.str();
    }
};
}
#endif
