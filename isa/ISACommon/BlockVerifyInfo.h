#pragma once

#ifndef VERIFY_INFO
#define VERIFY_INFO

#include <memory>
#include <vector>
#include <deque>

#include "../common/DataStruct/MachineId.h"
#include "OpcodeManager.h"

namespace JCore {

class TileRegVerifyData {
public:
    uint64_t shapeM = 0;
    uint64_t shapeN = 0;
    std::vector<uint64_t> data;
    bool operator == (const TileRegVerifyData& other)
    {
        bool sameShape = (this->shapeM == other.shapeM && this->shapeN == other.shapeN);
        bool dataVer = true;
        if (data.size() != 0) {
            for (size_t i = 0; i < this->data.size(); i++) {
                if (this->data[i] != other.data[i]) {
                    dataVer = false;
                    break;
                }
            }
        }

        return sameShape && dataVer;
    }

    void Dump() const
    {
        std::cout << "=== TileRegVerifyData Dump ===" << std::endl;
        std::cout << "\tshapeM: " << std::dec << shapeM << std::endl;
        std::cout << "\tshapeN: " << shapeN << std::endl;
        std::cout << "\tdata.size(): " << data.size() << std::endl;
        std::cout << "\t(shapeM * shapeN): " << (shapeM * shapeN) << std::endl;

        std::cout << "\tdata : [";
        if (data.size() > 0) {
            for (size_t i = 0; i < data.size(); ++i) {
                std::cout << std::hex << data[i];
                if (i < data.size() - 1) {
                    std::cout << ", ";
                }
            }
        } else {
            std::cout << "none";
        }
        std::cout << "]" << std::endl;

        // 数据一致性检查
        bool dataConsistent = (data.size() == (shapeM * shapeN));
        std::cout << "\tdataConsistent: " << (dataConsistent ? "✓ " : "✗ ") << std::endl;

        std::cout << "==================================" << std::endl;
    }
};

struct InstVerifyInfo {
    bool isReferenc = false;
    bool check = true;
    bool autoGen = false;
    bool terminate = false;

    uint64_t bid = 0;
    uint64_t tid = 0;
    uint64_t lgid = 0; // logic group id in Parallel Block
    uint64_t gid = 0; // physical group id in Parallel Block
    uint64_t rid = 0;

    uint64_t coreId = 0;
    uint64_t tpc = 0;
    Opcode opcode = Opcode::OP_INVALID;
    uint64_t data = 0;

    bool isSIMTMinst = false;
    uint64_t lane = 0;
    uint64_t isLastLane = false;

    InstVerifyInfo() = default;
};

struct GroupVerifyInfo {
    bool isCompleted = false;
    uint64_t lgid = 0; // logic group id in Parallel Block
    uint64_t gid = 0; // physical group id in Parallel Block
};

class BlockVerifyInfo {
public:
    bool isReferenc = false;
    uint64_t bid = 0;
    uint64_t bpc = 0;
    bool isLastBlock = false; // for loop manager.

    bool modelCompleted = false;
    std::deque<InstVerifyInfo> instVerifyInfoList;

    bool isParBlock = false;
    uint64_t groupNum = 0;
    uint64_t completedGroupNum = 0;
    uint64_t groupVerifyPtr = 0;
    std::vector<GroupVerifyInfo> groupStatus;
    std::vector<std::deque<InstVerifyInfo>> groupInstVerifyInfoList; // for smit minst

    bool verifyTileRegData = false;
    std::vector<std::shared_ptr<TileRegVerifyData>> tileRegData;

    void Reset(uint64_t blockId)
    {
        bid = blockId;
        bpc = 0;

        modelCompleted = false;
        instVerifyInfoList.clear();
        isParBlock = false;
        groupNum = 0;
        completedGroupNum = 0;
        groupVerifyPtr = 0;
        groupStatus.clear();
        groupInstVerifyInfoList.clear();

        verifyTileRegData = false;
        tileRegData.clear();
    }
};

using BlockVerifyPtr = std::shared_ptr<BlockVerifyInfo>;

class BlockPerfectInfo {
public:
    bool isBFUPCList = true;
    uint64_t bpc = 0;
    std::deque<uint64_t> instPCList;

    BlockPerfectInfo() = default;
    BlockPerfectInfo(bool bfu) : isBFUPCList(bfu) {}
    ~BlockPerfectInfo() = default;
    void Reset()
    {
        bpc = 0;
        instPCList.clear();
    }
};
using BlockPerfectPtr = std::shared_ptr<BlockPerfectInfo>;

} // namespace JCore
#endif