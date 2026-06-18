#ifndef BLOCK_COMMAND_H
#define BLOCK_COMMAND_H

#include <memory>

#include "ISA.h"
#include "ROBID.h"
#include "SimInstInfo.h"
#include "TileOperand.h"
#include "../common/DataStruct/CommonData.h"
#include "ISACommon/DataType.h"

namespace JCore {
class BlockCommand;
using BlockCommandPtr = std::shared_ptr<BlockCommand>;

struct CHandAllocInfo {
    uint64_t accChainId = 0;
    uint64_t startHandId = 0;
    uint64_t endHandId = 0;
    uint64_t num = 0;
    CHandAllocInfo() = default;
};
using CHandAllocInfoPtr = std::shared_ptr<CHandAllocInfo>;

class BlockCommand : public JCore::Block {
public:
    ROBID bid;
    uint32_t stid = -1U;
    ROBID gid;
    uint64_t bpc { 0ULL };
    uint64_t peid = 0;
    BIQType biqType = BIQType::NONE_IQ;

    bool hasSendBRenameLastFlag = false; // 为解决FRET.RA 后有一条C.BSTOP 的问题。BRename 后续会删除，此标记位也需要删除

    uint64_t                startSID = 0;
    uint64_t                startLoadID = 0;
    uint64_t                loadInstCount = 0;
    uint64_t                storeInstCount = 0;
    uint64_t                sid = 0;
    uint64_t                load_id = 0;

    std::vector<TileOperandPtr> srcs;
    std::vector<TileOperandPtr> dsts;

    // For B.IOR
    std::vector<uint64_t> srcPtag;
    std::vector<uint64_t> dstPtag;

    uint32_t stashCnt = 0;
    bool doubleOutput = false;

    uint64_t accChainId = -1U;
    CHandAllocInfoPtr cHandAllocInfo = nullptr;

    bool cmdExecCompleted = false;

    // For debug or view
    uint64_t execMachineId = 0;
    uint64_t blockStartPC = 0;
    int eventId = -1;

    uint64_t startCalCycle = 0;
    uint64_t reqCycle = 0;
    uint32_t coreId = 0;
    uint32_t stashWayId = -1U;

    SimInst machineInst = nullptr;

    void AccumulateBlockInfo(SimInst &inst);

    void Wakeup(BlockCommandPtr cmd, TileOperandPtr tileOpd);
    void FlagDoubleOutput(TileOperandPtr tileOpd);
    bool Ready();
    bool StashReady(bool checkDst = true);
    void HandleUInstBSTOP(SimInst inst);
    void HandleUInstBIOR(SimInst inst);
    void HandleUInstBIOT(SimInst inst);
    bool IsMoveOp() const;
    TileOperandPtr GetFirstValidTileSrc();
    TileOperandPtr GetTileDstByOperandType(OperandType type)
    {
        for (auto dst : dsts) {
            if (dst->handType == type) {
                return dst;
            }
        }
        return nullptr;
    }
    bool SetNewTileDst(TileOperandPtr const& newDst, OperandType type)
    {
        bool suaccelss = false;
        for (auto &dst : dsts) {
            if (dst->handType == type) {
                dst = newDst;
                suaccelss = true;
                break;
            }
        }
        return suaccelss;
    }
    uint64_t GetCHandNum();
    bool IsTloadOrTstore()
    {
        return tileOp == TileOp::TLOAD || tileOp == TileOp::TSTORE;
    }
    std::string Dump();
    std::string DumpFormalName();

    std::string DumpVecBriefName(uint64_t laneNum = 64);
    std::string DumpSwimInfo(uint32_t offset = 0);

    bool IsFromCube() const;

    bool IsVecStash() const
    {
        return tileOp == TileOp::TSTASH_TA_INPUT || tileOp == TileOp::TSTASH_TO_OUTPUT;
    }

    bool IsOtherStash() const
    {
        return tileOp == TileOp::OTHER_TSTASH;
    }
};
using BlockCommandPtr = std::shared_ptr<BlockCommand>;

inline BIQType GetBiqType(BlockType blkType)
{
    switch (blkType) {
        case BlockType::BLK_TYPE_VPAR:
        case BlockType::BLK_TYPE_VSEQ:
            return BIQType::VEC_IQ;
        case BlockType::BLK_TYPE_MPAR:
        case BlockType::BLK_TYPE_MSEQ:
            return BIQType::MCALL_IQ;
        case BlockType::BLK_TYPE_CUBE:
            return BIQType::CUBE_IQ;
        case BlockType::BLK_TYPE_TMA:
            return BIQType::TMA_IQ;
        case BlockType::BLK_TYPE_TEPL:
            return BIQType::VET_IQ;
        default:
            return BIQType::BCC_IQ;
    }
}

inline TileOperandPtr ConvertOperandToTileOperand(POperandPtr operand, bool isDst)
{
    TileOperandPtr tileOpd = std::make_shared<TileOperand>();
    switch (operand->type) {
        case OperandType::OPD_TILE_TLINK:
        case OperandType::OPD_TILE_ULINK:
        case OperandType::OPD_TILE_MLINK:
        case OperandType::OPD_TILE_NLINK:
        case OperandType::OPD_TILE_STACK:
        case OperandType::OPD_TILE_DLINK:
        case OperandType::OPD_TILE_MCALL_STK:
        case OperandType::OPD_TILE_MCALL_GPR:
            tileOpd->vld = true;
            tileOpd->isDst = isDst;
            tileOpd->renamed = operand->renamed;
            tileOpd->handType = operand->type;
            tileOpd->type = operand->type;
            tileOpd->link = operand->value + 1;
            tileOpd->reuse = operand->reuse;
            tileOpd->size = operand->size;
            tileOpd->baseAddr = operand->baseAddr;
            tileOpd->tileTag = operand->ptag;
            tileOpd->ready = operand->ready;
            tileOpd->isZero = (operand->size == 0);
            tileOpd->tileInfo = operand->tileInfo;
            break;
        default:
            return nullptr;
    }
    return tileOpd;
}
} // namespace JCore
#endif
