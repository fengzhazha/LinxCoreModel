#pragma once

#ifndef FETCH_REQ_BUS_H
#define FETCH_REQ_BUS_H

#include <cstdint>
#include <vector>

#include "../CycleInfo.h"
#include "../ROBID.h"
#include "../ModelEnumDefines.h"
#include "../ModelCommon/SimInstInfo.h"

namespace JCore {
enum FetchReqStatus {
    ICACHE_IDEL = 0, ICACHE_HIT, ICACHE_WAIT, ICACHE_MISS, ICACHE_GET
};

/* Data bus format for fetch request */
struct FetchReqBus {
    bool                    vld = false;
    uint32_t                mask = 0;
    uint64_t                bpc = 0;
    uint64_t                tpc = 0;
    ROBID                   bid;
    ROBID                   gid;
    uint32_t                stid = -1U;
    /* \brief The id of the group within the block, with each block starting from 0 */
    uint32_t                    logicalGID = 0;
    ShapeLoopInfo               shapelpinfo;
    uint64_t                fid = 0;
    uint32_t                m_threadId = 0;
    /* \brief First of the block fetch. */
    bool                    first = false;
    /* \brief Last of the block fetch */
    bool                    last = false;
    /* \brief Source of block fetch information. */
    BlkSrcType              blkSrcType = NONE_SRC;

    /* \brief Whether two blocks merge to a fetch request. */
    bool                    merge = false;
    /* \brief Mask of first block of merged blocks. */
    uint32_t                mask1 = 0;
    /* \brief BID of second block of merged blocks. */
    ROBID                   nextBid;
    /* \brief BPC of second block of merged blocks. */
    uint64_t                nextBPC = 0;
    /* \brief Last of the second block fetch. */
    bool                    nextLast = false;
    /* \brief Source of block fetch information. */
    BlkSrcType              nextBlkSrcType;
    /* \brief end TPC of the second block. */
    uint64_t                nextEndTPC = 0;

    /* \brief Header size for isTemplate generation */
    uint32_t                hsize = 0;
    /* \brief Total micro-instructions which will be generated */
    uint32_t                genTotalInstCount = 0;
    /* \brief Micro-instructions which had been generated */
    uint32_t                generatedInstCount = 0;
    /* \brief the block with no branch instrucitons */
    bool                    blockNoBranch = false;
    /* \brief the fetch bundle with no branch instrucitons */
    bool                    bundleNoBranch = false;
    uint64_t                bp_dst = 0;
    /* \brief Lane ID (Threaded PE only) */
    uint32_t                laneID = 0;
    /* \brief Fetch status in IFU fetchReqBuffer */
    FetchReqStatus                status = ICACHE_IDEL;
    /* \brief end TPC of the block */
    uint64_t                endTPC = 0;
    /* \brief Instruction size for 16/32 encoding */
    std::vector<uint64_t>   isize;
    std::vector<uint64_t>   data;
    std::vector<uint64_t>   tpcArr;
    uint32_t                instCnt = 0;
    uint64_t                mergeBin = 0;
    uint32_t                mergeSize = 0;

    CycleBus               pipe_cycle;
    BlockType              btype = BlockType::BLK_TYPE_INVAL;
    BIQType                biqType = BIQType::NONE_IQ;
    FetchReqBus() : pipe_cycle(new CycleInfo)
    {
        vld = false;
        mask = 0;
        bpc = 0;
        tpc = 0;
        bid = ROBID();
        fid = 0;
        first = false;
        last = false;
        blkSrcType = BlkSrcType::NONE_SRC;

        merge = false;
        mask1 = 0;
        nextBid = ROBID();
        nextBPC = 0;
        nextLast = false;
        nextBlkSrcType = BlkSrcType::NONE_SRC;
        nextEndTPC = 0;

        hsize = 0;
        genTotalInstCount = 0;
        generatedInstCount = 0;
        blockNoBranch = false;
        bundleNoBranch = false;
        bp_dst = 0;
        laneID = 0;
        status = ICACHE_IDEL;
        endTPC = 0;
        isize.resize(0);
        data.resize(0);
    }

    void EnableFetchData(uint32_t dataSize)
    {
        data.resize(0);
        for (uint32_t i = 0; i < dataSize; i++) {
            data.emplace_back(0);
        }
        isize.resize(dataSize);
        tpcArr.resize(dataSize);
    }
};

using FetchReqPtr = std::shared_ptr<FetchReqBus>;
}
#endif