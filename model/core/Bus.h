#ifndef  BLOCKISA_MODEL_BUS_H
#define  BLOCKISA_MODEL_BUS_H

#pragma once
#include <bitset>
#include <cstdint>
#include <iostream>
#include <vector>
#include "bctrl/bfu/common/MachineInst.h"
#include "core/Packet.h"
#include "ISACommon/DataType.h"
#include "ModelCommon/ModelCommon.h"
#include "include/ModelSpec.h"
#include "ModelCommon/bus/RFReqBus.h"
#include "ModelCommon/bus/PEResolveBus.h"
#include "ModelCommon/ShapeLoopInfo.h"
#include "ModelCommon/BlockCommand.h"
#include "ModelCommon/bus/MemReqBus.h"
#include "ModelCommon/bus/FlushBus.h"
#include "ModelCommon/bus/FetchReqBus.h"
#include "ModelCommon/bus/PEControlBus.h"
#include "ModelCommon/bus/BSizeBus.h"
#include "ModelCommon/bus/MemRequest.h"
#include "ModelCommon/DelayQueue.h"
#include "ModelCommon/bus/ResolveBlockBus.h"
#include "ModelCommon/bus/GroupInfo.h"
#include "ModelCommon/bus/NoneFlushBus.h"
#include "ModelCommon/bus/PrefFB.h"
#include "ModelCommon/bus/CoreControlBus.h"
#include "ModelCommon/bus/BlkBodyFetchState.h"
#include "ModelCommon/bus/FormalParamReq.h"
#include "ModelCommon/bus/StackRenameBus.h"
#include "ModelCommon/LSUUtils.h"
#include "ModelCommon/LpvInfo.h"
#include "ISA.h"
#include "set"
#include "include/SimSys.h"
#include "DataStruct/MachineId.h"
#include "ModelCommon/CycleInfo.h"

#define MAX_BF_SIZE 1024
#define MAX_ST_Q_SIZE 2048

namespace JCore {
// 目前仅剩MTC-Core 有残留，mtc-core 目前无人维护，后续准备删除。
const uint64_t TILE_MASK = 0xFF00000000000000;

// 结构冗余，后续被block-command 替代，后续删除。
/* Fully-Decoded Block header format
 * Machine block headers */
struct FullBlockHeader {
    bool                    vld = false;
    bool                    isInline = true;
    ROBID                   bid;
    ROBID                   bSeq;
    uint32_t                peid = 0;
    uint32_t                fbid = 0;
    uint32_t                stid = -1U;
    uint32_t                fbid_local = 0;
    uint64_t                bpc = 0;
    uint64_t                bstartBpc = 0;
    uint64_t                tpc = 0;
    uint32_t                threadID = 0;
    uint64_t                debugId = UINT64_MAX;
    BlockType               type = BlockType::BLK_TYPE_INVAL;
    BranchType              branchType = BranchType::BLK_BR_INVAL;
    uint32_t                size = 0;
    uint32_t                realSize = 0;
    /* \brief Count the number of instructions in the current block. */
    bool                    instsCntVld = false;
    int                     instsCnt = 0;
    BSizeBus                instStats = BSizeBus();
    int32_t                 signedImm = 0;
    Opcode                  opcode = Opcode::OP_INVALID;
    // BlockOpcode             opcode = BlockOpcode::MEM_SET;
    uint32_t                attribute = 0;
    uint32_t                magic = 0;
    uint32_t                inputRegCount = 0;
    uint32_t                inputRegMask = 0;
    uint32_t                outputRegCount = 0;
    uint32_t                outputRegMask = 0;

    DataType                dataType = DataType::INVALID;
    DataType                tileDType;
    DataType                tileSType;
    uint32_t                tileSrcCnt = 0;
    uint32_t                tileRnCnt = 0;
    uint32_t                seq = 0;
    uint64_t                payloadPC = 0;
    uint64_t                fallBPC = 0;
    uint64_t                divertBPC = 0;
    uint32_t                getBitMask = 0;
    uint32_t                setBitMask = 0;
    uint32_t                specGetMask = 0;
    uint32_t                specSetMask = 0;
    uint64_t                encoding = 0;
    int64_t                 btextOffset = 0;
    bool                    parallelFetch = false;
    bool                    simtLastHeader = false;
    // ParallelMode            parallel_mode = ParallelMode::PARA_INVEL_MODE;
    std::vector<uint32_t>   payInlineBin;

    uint64_t                inlineBin = 0;
    bool                    isLast = false;
    bool                    isInst = false;
    bool                    isTemplate = false;
    CodeTemplateID          templateID = CodeTemplateID::NON_TEMPLATE;
    TemplateIDSet templateIDSet;
    uint32_t                auxHdrCnt = 1; // auxiliary block header count, bstart is included by default
    uint32_t                auxSimtHdrCnt = 2; // auxiliary Simt block header count, btext battr is included by default
    uint64_t                RegSrc0 = 0;
    uint64_t                RegSrc1 = 0;
    uint64_t                RegSrc2 = 0;
    uint32_t                regCnt = 0;

    uint32_t                brHint = 0;
    // to delete: for mempush/mempop in v0.36
    uint32_t                memBitMask;
    int64_t                 storeOffset;
    int64_t                 loadOffset;
    /* FRET header.vld */
    uint32_t                fretHeaderVld = 0;

    /* ACC Rename */
    uint32_t                ACC_size = 0;
    bool                    ACC_flag = false;

    /* For BranchType::BSB */
    bool                    storeStart = false;
    bool                    sbarCertain = false;
    SpecType                specType;
    uint64_t                storeCount = 0;
    uint64_t                loadCount = 0;
    uint64_t                stackGetCnt = 0;
    uint64_t                addrBase = 0;

    uint64_t                predictTarget = 0;
    bool                    predictTaken = 0;

    bool                    resolveTargetVld = 0;
    uint64_t                resolveTarget = 0;
    bool                    resolveTaken = 0;
    bool                    resolveMisPredict = 0;

    /* For TLOAD/TSTORE */
    TileBridgeBus           tile;
    uint64_t                baseAddrGM = 0;
    /* FIXME: b.ior in tload/tstore */
    uint32_t                src0_atag = 0;

    /* bcc->bisq credit backpressure */
    bool                    hasBiqCredit = false;

    /* \brief a flag for header to write link register */
    bool                    link = false;
    /* \brief a flag for simulation trap */
    bool                    trap = false;
    /* \brief a flag for hyper block */
    bool                    hyper = false;
    /* \brief encoding size of the block header */
    uint32_t                hsize = 0;
    /* \brief machine header used in bfu */
    NS_CORE::PtrMachineInst        machineInst = nullptr;
    /* If the block is the next of the WFE block, only use for codetemplate */
    bool                    wfeNext = false;
    /* The origin data of block header, only use for codetemplate */
    uint32_t                data[2];
    /* Indicate whether to generate push msgbuf inst in codetemplate */
    bool                    needGenPushMsgBufInst = false;

    /* \brief konata log only */
    uint64_t                seqNum = 0;

    /* \brief PMU only */
    uint64_t                createCycle = 0;
    uint64_t                f1Cycle = 0;
    uint64_t                fetchCycle = 0;
    uint64_t                f3Cycle = 0;
    uint64_t                f4Cycle = 0;
    uint64_t                f5Cycle = 0;
    uint64_t                fetchReturnCycle = 0;
    uint64_t                decodeCycle = 0;
    uint64_t                renameCycle = 0;
    uint64_t                renamedCycle = 0;
    uint64_t                issue_cycle = 0;
    uint64_t                dispatchCycle = 0;
    uint64_t                iqCycle = 0;
    uint64_t                exe_cycle = 0;
    uint64_t                completeCycle = 0;
    uint64_t                retireCycle = 0;
    uint64_t                coreId = 0;
    /* \brief For inline block software simulation. */
    bool                    smallBlk = false;
    std::vector<uint32_t>   payloadBin;
    /* \brief Statistics for move Reg count */
    uint64_t                moveCnt = 0;
    /* \brief for BlockOpcode::BLBAR Template Block */
    uint64_t                blbar_offset = 0;
    uint64_t                prefetch_count = 0;
    /* \brief start of Block */
    // BlockCodeType           blockBstartType = BlockCodeType::BLK_CODE_TYPE_INVAL;
    uint64_t                lastTpc = 0;
    uint64_t                laneVld = 64;
    bool                    simt_tp_resolved = false;
    bool                    renamed = false;
    bool                    isLastHeader = false;
    bool                    autogen = false;
    uint64_t                setRegMask = 0;
    ROBID                   renameBid;
    uint64_t                curBid = 0;
    BIQType                 biqType = BIQType::NONE_IQ;
    BIQType                 biqRealType = BIQType::NONE_IQ;
    // BIQType                 biqType = BIQType::BCC_IQ;
    // BIQType                 biqRealType = BIQType::BCC_IQ;
    // for debug
    uint64_t                bin = 0;
    bool                    simEndAcrc = false;
    bool                    isSplitBlk = false;
    uint64_t                tileOpId = 0; // 泳道图 id
    TileOp                  tileOp = TileOp::TINVALID;
    bool                    headerAtAll = false;
    uint32_t                biorCount = 0;
    uint32_t                recvIEXBiorCount = 0;
    // for B.ARG
    bool                    canon = false;
    bool                    warmupHint = false;
    LayOut                  layout = LayOut::LAYOUT_COUNT;
    // TileConvertType         tileCVTType = TileConvertType::NORMAL;
    bool                    subCoreTemplate = false;
    /* \brief loadId, storeId */
    uint64_t                startSID = 0;
    uint64_t                startLoadID = 0;
    uint64_t                sid = 0;
    uint64_t                load_id = 0;
    ROBID                   lsID;

    uint32_t                biot_id = 0;
    uint64_t                dTag = -1U;
    std::vector<TileOperandPtr> tileSrcs;
    std::vector<TileOperandPtr> tileDsts;

    bool AllInstsArrived() { return instsCntVld && instsCnt == 0; }
    FullBlockHeader() { bid.val = 0; bid.wrap = false; bSeq.val = 0; bSeq.wrap = false; }
};

typedef std::shared_ptr<FullBlockHeader> BHeader;

/* Bus print utilities */
std::ostream& operator<<(std::ostream& out, BHeader const &header);
std::ostream& operator<<(std::ostream& out, IDBus &fullID);
std::ostream& operator<<(std::ostream& out, MemReqBus const &memReqBus);
std::ostream& operator<<(std::ostream& out, MemRequest const &memRequest);
std::ostream& operator<<(std::ostream& out, VecData const &vd);
std::ostream& operator<<(std::ostream& out, FlushBus const &flushBus);
std::ostream& operator<<(std::ostream& out, FetchReqBus &fetchReqBus);
std::ostream& operator<<(std::ostream& out, PEControlBus &peControlBus);
std::ostream& operator<<(std::ostream& out, PEResolveBus &rlsvBus);
std::ostream& operator<<(std::ostream& out, RFReqBus &rfBus);
std::ostream& operator<<(std::ostream& out, LpvInfo &lpvInfo);
std::ostream& operator<<(std::ostream& out, StackRenameBus &bus);
std::ostream& operator<<(std::ostream& out, MDBBus &bus);
} // namespace JCore
#endif
