#ifndef RENAME_REQ_BUS
#define RENAME_REQ_BUS

#include "ISACommon/OperandType.h"
#include "ModelCommon/ModelCommon.h"

namespace JCore
{

struct RenameBus {
    bool vld = false;
    bool isWrite = false;
    bool ready = false;
    bool released = false;
    bool pair = false;
    bool move = false;
    bool bend = false;
    bool retired = false;
    uint32_t peID = 0;
    uint32_t coreID = 0;
    uint32_t tid = 0;
    uint32_t lane = 0;
    ROBID bid;
    ROBID rid;
    ROBID gid;
    OperandType type = OperandType::OPD_INVALID;
    uint32_t atag = -1U;
    uint32_t ptag = -1U;
    void Reset()
    {
        vld = false;
    }
};

struct RenameOptInfo {
    bool move = false;
    bool dataVld = false;
    uint64_t data = 0;
    int64_t offset = 0;
};

using PRenameOpt = std::shared_ptr<RenameOptInfo>;

struct RenameMapInfo {
    bool released = false;
    uint32_t ptag = -1U;
    // for optimization
    PRenameOpt opt = nullptr;
};

}

#endif