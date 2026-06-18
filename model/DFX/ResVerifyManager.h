#pragma once

#ifndef RES_VERIFY_MANAGER
#define RES_VERIFY_MANAGER

#include "../configs/dfx_config.h"
#include "../common/DataStruct/RingQ.h"
#include "../common/Common.h"
#include "../isa/ISACommon/BlockVerifyInfo.h"

constexpr int DEFAULT_LGROB_THRESHOLD = 8;

namespace JCore {
enum class VerifyErrorType {
    ERROR_DEADLOCK,
    ERROR_BPC_NE,       // Fetch block pc error
    ERROR_TILE_DATA_NE, // Parallel/Cube Block TileReg Data not equal
    ERROR_TPC_NE,       // Fetch minst pc not equal
    ERROR_RES_NE,       // Minst res not equal
    ERROR_MINST_NUM_NE, // Minsts number does not equal
    ERROR_NONE,
};
inline std::string GetErrorName(VerifyErrorType type)
{
    switch (type) {
        case VerifyErrorType::ERROR_DEADLOCK:
            return "Deadlock";
        case VerifyErrorType::ERROR_BPC_NE:
            return "Block Start PC Error";
        case VerifyErrorType::ERROR_TILE_DATA_NE:
            return "TileReg Data Error";
        case VerifyErrorType::ERROR_TPC_NE:
            return "MInst TPC Error";
        case VerifyErrorType::ERROR_RES_NE:
            return "MInst Res Error";
        case VerifyErrorType::ERROR_MINST_NUM_NE:
            return "Redundant: refMinst Empty but model Minst ";
        default:
            return "Undefined Error";
    }
}

struct FlushBus;
class SimSys;
class ResVerifyManager {
public:
    SimSys *sim;
    DFXConfig config;
    ResVerifyManager() = default;
    ~ResVerifyManager() = default;

    /* \brief Interface for recording information required for result verification */
    void Init(uint64_t bRobDepth);
    void SetBlockBlockStatus(uint64_t blockId, bool status);

    bool RefBlockInfoListFull();
    void RecordRefBlockInfo(BlockVerifyPtr info);
    void AppendRefInstInfo(BlockVerifyPtr info);

    void RecordPerfectBlockInfo(BlockPerfectPtr bfuInfo, BlockPerfectPtr ifuInfo);

    /* \brief Reset Block Info status. */
    void ResetBlockInfo(uint64_t blockId, uint64_t bpc);
    void RecordLoopManagerLastBlock(uint64_t blockId);
    void RecordBlockCompleted(uint64_t blockId);
    void RecordBlockTileRegData(uint64_t blockId, std::shared_ptr<TileRegVerifyData> tileRegData);
    void RecordMinst(InstVerifyInfo &minst);
    void InitPARGroup(uint64_t blockId, uint64_t groupNum);
    void RecordPARGroupCompleted(uint64_t blockId, uint64_t groupId);
    void RecordPARMinst(InstVerifyInfo &minst);

    void CompletedBlock(uint64_t blockId);
    bool Verify(uint64_t width);
    bool VerifyBlockInfo(uint64_t blockId);
    bool VerifyBEndInst(uint64_t blockId);
    bool VerifyScalarBlockInfo(uint64_t blockId);
    bool VerifyParBlockInfo(uint64_t blockId);
    bool VerifyTileRegData(std::shared_ptr<TileRegVerifyData> refData, std::shared_ptr<TileRegVerifyData> modelData);
    bool VerifyMinst(InstVerifyInfo &refInst, InstVerifyInfo &mInst);

    bool IsDeadLock();
    void DumpDeadLockInfo();
    void DumpErrorInfo(uint64_t blockId);
    std::string GetErrorInfo();
    uint64_t GetVerifiedBlockCount() const;
    uint64_t GetverifiedMinstCount() const;
    void ResetWaitCycle();
    void Flush(const FlushBus &flushReq, uint64_t commitBid);
    void Flush(uint64_t blockId);

    bool CheckBFUFrontLast();
    bool CheckBFUFrontIsFirst();
    uint64_t GetBFUFrontPC();

private:
    bool enableResCheck = false;
    /* \brief Reference block information */
    RingQ<BlockVerifyPtr> mRefBlockInfo;

    /* \brief PerfectBP Information */
    RingQ<BlockPerfectPtr> bfuPerfectTrace;
    RingQ<BlockPerfectPtr> ifuPerfectTrace;
    uint64_t bfuInstPtr = 0;
    uint64_t ifuInstPtr = 0;

    /* \brief Model Execute Block information */
    std::vector<BlockVerifyPtr> mModelBlockInfo;

    uint64_t blockRobDepth = 0;
    uint64_t currentBlockId = 0;
    uint64_t waitCycles = 0;
    uint64_t verifiedMinstCount = 0;
    uint64_t verifiedBlockCount = 0;

    VerifyErrorType errorType = VerifyErrorType::ERROR_NONE;
    std::string errorInfo = "";
    const std::string TAB_1 = "    ";
    const std::string TAB_2 = "        ";
    const std::string TAB_3 = "            ";
};
} // namespace JCore
#endif