#pragma once

#ifndef BLOCK_SIZE_BUS_H
#define BLOCK_SIZE_BUS_H

#include <vector>
#include <unordered_map>
#include <cstdint>

#include "../ROBID.h"
#include "ISA.h"

namespace JCore {
struct BlockRelativeIndexReg {
    // T/U/vt/vu/vm/vn {replative-dist, count}
    std::unordered_map<OperandType, std::unordered_map<uint64_t, uint64_t>> regRelativeDist;
    // T/U/vt/vu/vm/vn {rob-dist, count}
    std::unordered_map<OperandType, std::unordered_map<uint64_t, uint64_t>> regRobDist;
};
using BlockRelativeStats = std::shared_ptr<BlockRelativeIndexReg>;

struct BlockInstStats {
    uint64_t totalCommitInst = 0;
    std::unordered_map<InstGroup, uint64_t> instGroupCommitInstNum;
};
using BlockInstStatsPtr = std::shared_ptr<BlockInstStats>;
struct BSizeBus {
    ROBID                   bid;
    uint32_t                peid = 0;
    uint32_t                bSize = 0;
    BlockRelativeStats      instRelativeRegStats = nullptr;
    BlockInstStatsPtr       instCommitStats = nullptr;
};
}
#endif