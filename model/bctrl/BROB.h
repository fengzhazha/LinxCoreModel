#pragma once

#include <map>
#include <vector>

#include "core/Bus.h"
#include "ModelSpec.h"
#include "pe/PECommon/OPEState.h"

namespace JCore {

class BCtrlUnit;

struct BlockStatusEntry {
    bool                    running = false;
    bool                    stallOnLoad = false;
    bool                    stallOnGet = false;
    bool                    stall = false;
    bool                    forwardSet = false;
    uint32_t                ipc = 0;

    void                    Reset();
};

struct BROBEntry {
    ROBID                   id;
    /* All information of the block */
    BHeader                 header;
    BlockCommandPtr         blockCMD = nullptr;
    /* Runtime status of the block */
    /* Dispatched result */
    uint32_t                peID = 0;
    /* Block status, only used for trace mode 2 */
    BlockStatusEntry        perf;
    /* Mark that block is only generate TM for SIMT block, only flag that not need to record DFX */
    bool                    simtTBCm = false;
    std::string             exceptionInfo = "";
    /* ACC Rename */
    uint64_t                accChainID = 0;
    uint64_t                accTagPtr = 0;

    void                    Reset();
    BROBEntry() { header = std::make_shared<FullBlockHeader>(); }
    BlockStatus             status = BlockStatus::FREE;
    std::string DumpPipeViewInfo();
    std::string Dump();
};

struct BROBState {
    std::vector<BROBEntry>  entry;
    /* Allocate pointer - Block Decode */
    ROBID                   allocPtr;
    /* Dispatch pointer - Block Dispatch */
    ROBID                   dispatchPtr;
    /* Rename pointer - Block Rename */
    ROBID                   renameStartPtr;
    /* Rename pointer - Block Rename */
    ROBID                   renamePtr;
    /* Commit pointer - Block retire */
    ROBID                   commitPtr;
    /* Non Flush pointer - Block cannot flush */
    ROBID                   nonFlushBid;
    /* Current ROB size */
    uint32_t                size = 0;
    /* Store id count total */
    uint64_t                sbarID;
    /* Current calculated block ptr */
    ROBID                   sbarPtr;

    /* Outstanding size for each PE lane */
    std::vector<uint32_t>   osdBSize;
    /* Only used for trace mode 2 */
    std::vector<uint32_t>   ipcPE;

    void CheckNonFlushBid();
};

class BlockROB : public SimObj {
private:
    std::vector<BROBState>     current;
    // BROBState               next;

    uint32_t                peCount;
    std::vector<bool>                    runSysBlock;
    std::vector<bool>                    runBarrierBlock;
    bool                    runLoopBlock = false;
    std::vector<bool>                    excpStall;
    std::vector<bool>                    terminate;
    /* \brief flag indicating that ckpt has started */
    bool                    pmc_started = false;

    std::map<uint64_t, uint32_t> misPredCounter;
    uint32_t                retire_total_inst;

    /* \brief pmu */
    uint32_t                issued_cube_blocks = 0;
    uint32_t                issued_vector_blocks = 0;
    uint32_t                issuedTempleteBlocks = 0;
    uint32_t                issued_tload = 0;
    uint32_t                issued_tstore = 0;

    std::vector<uint32_t>                smt_issued_cube_blocks_array;
    std::vector<uint32_t>                smt_issued_vector_blocks_array;
    std::vector<uint32_t>                smt_issued_tload_array;
    std::vector<uint32_t>                smt_issued_tstore_array;

    /* \brief The last block that sets its acc info */
    std::vector<ROBID>                   lastSetACCBlock;

public:
    std::vector<BROBState>  next;
    BCtrlUnit               *top;

    std::vector<SimQueue<ResolveBlockBus>*>   iex_brob_rslvblk;
    // TODO: tload tstore(issue) tcopy rslv
    SimQueue<FlushReq>          *bcc_flush_rpt_q;
    /* brief Ptag retire request from BROB to iex-ptag-ready */
    OUTPUT SimQueue<ROBID>      *block_rtable_retire_q;
    OUTPUT SimQueue<IDBus>      *blcok_lane_retire_q;
    OUTPUT SimQueue<IDBus>      *m_bccLsuTloadCommitQ;
    OUTPUT SimQueue<IDBus>      *m_bccLsuTstoreCommitQ;

    void                    Reset();
    void                    Reset(uint32_t stid);
    void                    Work();
    void                    Work(uint32_t stid);
    void                    Xfer();
    SimSys                  *GetSim();
    void ReportStat() override {}
    /* \brief Allocate a block in BROB, increment allocPtr */
    ROBID                   allocBlock(BHeader &header, BlockCommandPtr cmd, uint32_t pos);
    /* \brief Tells the BROB a block has been issued to PE */
    void                    IssueBlock(ROBID bid, uint32_t peID, uint32_t stid);
    /* \brief Tells the BROB a block has been replaceed */
    void                    replay(FlushBus &flushReq);
    /* \brief Tells the BROB a block has been placed in PE ROB */
    void                    exeBlock(ROBID bid, BlockStatus status, uint32_t stid);
    /* \brief Tells the BROB a block has been completed */
    void                    SetBlockCommand(ROBID bid, BlockCommandPtr cmd);
    void                    completeBlock(ROBID bid, bool empty, uint32_t stid);
    /* \brief Tells the BROB the acc chain ID and tag ptr */
    void                    SetACC(ROBID bid, uint64_t prevChainID, uint64_t prevAccPtr,
                                   uint64_t lastChainID, uint64_t lastAccPtr, uint32_t stid);
    uint64_t                GetACCChainID(ROBID flushBid, uint32_t stid);
    uint64_t                GetACCTagPtr(ROBID flushBid, uint32_t stid);
    /* \brief Tells the BROB a block has been resolved */
    void                    resolveBlock();
    void                    resolveBlock(const IDBus &idBus, bool misPred, bool resolveTaken, uint64_t resolveTarget,
                                uint32_t stid);
    /* \brief Report the BROB the current block is running (PMU only) */
    void                    reportRunning(ROBID bid, Opcode op, uint32_t pe, uint32_t stid);
    void                    reportStall(ROBID bid, bool load, bool get, uint32_t pe, uint32_t stid);
    void                    reportException(ROBID bid, uint32_t stid, std::string info = "");
    void                    reportTerminate(ROBID bid, uint32_t stid);
    void                    reportRealBsize(const uint32_t pos, OPEState *opeState, uint32_t stid);
    void                    reportBlockRenaming(ROBID bid, uint32_t stid);
    void                    reportBlockRenamed(ROBID bid, uint32_t stid);
    /* \brief Retrieve the current status of block */
    BlockStatus getBlockStatus(ROBID bid, uint32_t stid)
    {
        ASSERT(stid < current.size());
        return current[stid].entry[bid.val].status;
    }
    /* \brief Retrieve the peID of block */
    uint32_t getBlockLaneID(ROBID bid, uint32_t stid)
    {
        ASSERT(stid < current.size());
        return current[stid].entry[bid.val].peID;
    }
    void                    SimtRecoverd(const FlushBus &flushReq);

    BlockCommandPtr         GetLastBlockCMDPtr(ROBID bid, uint32_t stid);
    BlockCommandPtr         GetBlockCMDPtr(ROBID bid, uint32_t stid);
    BlockCommandPtr         GetNextBlockCMDPtr(ROBID bid, uint32_t stid);
    BHeader                 getBlockHeader(ROBID bid, uint32_t stid);
    void                    setBlockHeaderGetMask(ROBID bid, uint32_t getBitMask, uint32_t stid);
    void                    setBlockHeaderSetMask(ROBID bid, uint32_t setBitMask, uint32_t stid);
    void                    setBlockHeaderInputMask(ROBID bid, uint32_t getMask, uint32_t stid);
    void                    setBlockHeaderOutputMask(ROBID bid, uint32_t setMask, uint32_t stid);
    void                    setBlockHeaderSetRegMask(ROBID bid, uint32_t setBitMask, uint32_t stid);

    void                    deliveryStoreID(uint32_t stid);
    void                    setStoreCount(ROBID bid, uint64_t storeCount, uint32_t stid);
    bool                    BlockCOMPLETED(BROBEntry &nEntry, BROBEntry &bentry, ROBID &cPtr, uint32_t stid);
    void                    commitBlock(uint32_t stid);
    void                    recoverBlock(FlushBus &flushReq);

    ROBID                   getOldestBlockID(uint32_t stid) {return current[stid].commitPtr;}
    bool IsOldestBlkComplete(uint32_t stid)
    {
        ASSERT(stid < current.size());
        return current[stid].entry[current[stid].commitPtr.val].status == BlockStatus::COMPLETED;
    }
    uint32_t GetOldestBlockPEID(uint32_t stid)
    {
        ASSERT(stid < current.size());
        return current[stid].entry[current[stid].commitPtr.val].peID;
    }
    uint32_t getBROBSize(uint32_t stid)
    {
        ASSERT(stid < current.size());
        return current[stid].size;
    }
    uint32_t getBROBCapacity(uint32_t stid)
    {
        ASSERT(stid < current.size());
        return current[stid].entry.size();
    }
    uint32_t getGapToOldest(ROBID bid, uint32_t stid)
    {
        ASSERT(stid < current.size());
        return CalGap(bid, current[stid].commitPtr, current[stid].entry.size());
    }
    ROBID GetAllocPtr(uint32_t stid)
    {
        ASSERT(stid < current.size());
        return current[stid].allocPtr;
    }
    bool                    needStall(uint32_t pos, uint32_t stid);
    void                    setNeedFlush(ROBID &bid, uint32_t stid);
    void                    setFlushed(FlushBus &flushReq);
    void                    Build();
    void                    Build(uint32_t stid);
    bool                    getSysBlockStatus(uint32_t stid) {return runSysBlock[stid];}
    void                    setSysBlockStatus(uint32_t stid, bool status) {runSysBlock[stid] = status;}
    bool                    GetBarrierBlockStatus(uint32_t stid) const {return runBarrierBlock[stid];}
    void                    SetBarrierBlockStatus(uint32_t stid, bool status) {runBarrierBlock[stid] = status;}
    // bool                    getLoopBlockStatus(uint32_t stid) {return runLoopBlock[stid];}
    // void                    setLoopBlockStatus(uint32_t stid, bool status) {runLoopBlock[stid] = status;}
    void                    setExcpStatus(uint32_t stid, bool status) {excpStall[stid] = status;}
    bool                    hasException(uint32_t stid) {return excpStall[stid];}
    void                    setTerminateStatus(uint32_t stid, bool status) {terminate[stid] = status;}
    bool                    getTerminateStatus(uint32_t stid) {return terminate[stid];}
    bool                    CheckRename(ROBID &bid, uint32_t stid);
    BHeader                 getNextBlockHeader(ROBID bid, uint32_t stid);
    std::string             PrintLastStatusAndReturnOldest(uint32_t stid);
    void                    GatherStats();
    void                    incStatsIssuedBlocks(ROBID bid, uint32_t stid);
    void                    decStatsIssuedBlocks(ROBID bid, uint32_t stid);
    void                    PrintStatus(uint32_t stid, bool enableFlag = false);
    void                    ReportLocalRegBlockCommit(ROBID bid, uint32_t stid);
    bool needFlush(ROBID bid, uint32_t stid)
    {
        ASSERT(stid < current.size());
        return (current[stid].entry[bid.val].status == BlockStatus::NEEDFLUSH ||
        next[stid].entry[bid.val].status == BlockStatus::NEEDFLUSH);
    }
    uint32_t                GetRetiredInstNum();
    void                    SetRetiredInstNum(uint32_t num);

    ROBID                   GetNonFlushOldestBid(uint32_t stid);
};
} // namespace JCore
