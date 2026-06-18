#pragma once

#ifndef OPE_STATE_H
#define OPE_STATE_H

#include <vector>

#include "core/Bus.h"
#include "ModelCommon/bus/BSizeBus.h"

namespace JCore {

struct localRegInfo {
    std::vector<uint64_t>                   tRegSrcCnt;        // the count of t_reg src in a block
    std::vector<uint64_t>                   uRegSrcCnt;
    std::vector<uint64_t>                   vtRegSrcCnt;
    std::vector<uint64_t>                   vuRegSrcCnt;
    std::vector<uint64_t>                   vmRegSrcCnt;
    std::vector<uint64_t>                   vnRegSrcCnt;
    std::vector<uint64_t>                   tRegSrcRobDist;    // the distance of rob when aaccelss localreg
    std::vector<uint64_t>                   uRegSrcRobDist;
    std::vector<uint64_t>                   vtRegSrcRobDist;    // the distance of rob when aaccelss localreg
    std::vector<uint64_t>                   vuRegSrcRobDist;
    std::vector<uint64_t>                   vmRegSrcRobDist;
    std::vector<uint64_t>                   vnRegSrcRobDist;

    localRegInfo()
    {
        Reset();
    }
    void                                    Reset()
    {
        const uint64_t linkMaxSize = 8;
        tRegSrcCnt.resize(linkMaxSize, 0);
        uRegSrcCnt.resize(linkMaxSize, 0);
        vtRegSrcCnt.resize(linkMaxSize, 0);
        vuRegSrcCnt.resize(linkMaxSize, 0);
        vmRegSrcCnt.resize(linkMaxSize, 0);
        vnRegSrcCnt.resize(linkMaxSize, 0);
        tRegSrcRobDist.resize(linkMaxSize, 0);
        uRegSrcRobDist.resize(linkMaxSize, 0);
        vtRegSrcRobDist.resize(linkMaxSize, 0);
        vuRegSrcRobDist.resize(linkMaxSize, 0);
        vmRegSrcRobDist.resize(linkMaxSize, 0);
        vnRegSrcRobDist.resize(linkMaxSize, 0);
    }
};
using localRegInfoPtr = std::shared_ptr<localRegInfo>;
/* All pipelined states for OoO PE */
struct OPEState {
    std::vector<BlockRelativeStats> instRelativeRegStats;
    std::vector<BlockInstStatsPtr> instCommitStats;

    void                                    Reset(uint64_t depth)
    {
        instRelativeRegStats.clear();
        instCommitStats.clear();
        for (uint64_t i = 0; i < depth; i++) {
            instRelativeRegStats.emplace_back(std::make_shared<BlockRelativeIndexReg>());
            instCommitStats.emplace_back(std::make_shared<BlockInstStats>());
        }
    }
    void                                    ReleaseEntry(uint32_t pos)
    {
        instRelativeRegStats[pos] = std::make_shared<BlockRelativeIndexReg>();
        instCommitStats[pos] = std::make_shared<BlockInstStats>();
    }
};
}
#endif