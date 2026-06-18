#pragma once

#include <map>
#include <vector>

#include "../configs/stack_config.h"
#include "core/Bus.h"
#include "core/stack_stats.h"
#include "SimSys.h"
#include "plat/DebugLog.h"

namespace JCore {

using namespace std;

class SimSys;
class Core;

struct StackRenameTableInfo {
    PLpvInfo lpvInfo;
    bool    ready   = false;
};

/* Stack rename state */
struct StackRenameUnitState {
    vector<bool>                        freeList;
    uint32_t                            freeSize;
    vector<StackRenameTableInfo>        stackPtagReady;
    ROBID                               renamePtr;
    StackRenameUnitState() : freeSize(0) {}
};

/* Stack set rename table : for release ans flush */
struct StackPRFTable {
    bool                                vld;
    bool                                stack;
    bool                                get;
    bool                                lsid_vld;
    ROBID                               rid;
    ROBID                               lsID;
    uint32_t                            sptag;
    int64_t                             imm;
    uint32_t                            tpc;
    bool                                retired;
    unordered_map<int64_t, uint32_t>    stackCheckPoint;
    StackPRFTable() { vld = false; stack = false; get = false; lsid_vld = false; retired = false; sptag = 0; imm = 0; }
};

/* Block stack rename state */
struct BlockStackState {
    bool                                vld = false;
    bool                                end = false; // TODO: optimize for stack rename stall before end of the block
    bool                                isTemplate = false;
    ROBID                               bid;
    uint32_t                            peID = 0;
    uint32_t                            bpc = 0;
    uint32_t                            stid = -1U;
    SimQueue<StackRenameBus>            inputQ;
    bool                                noNeedRename = false;
    bool                                newBlock = false;
    bool                                isHyper = false;
    bool                                bSetSP = false;
    bool                                preRename = false;
    bool                                spOffset = false;
    bool                                renamed = false;
    deque<StackPRFTable>                stackSetPRFTable;
    uint32_t                            rptr = 0;
    deque<StackPRFTable>                stackGetPRFTable;
    uint32_t                            rptrG = 0;
    int64_t                             imm = 0;
    int64_t                             localImm = 0;
    unordered_map<int64_t, uint32_t>    stackCheckPoint;
    uint32_t                            retireSetCnt = 0;
    uint32_t                            totalSetCnt = 0;
    bool                                isCounting = true;
    void                                Reset();
};

/* Block stack offset table */
struct bStackState {
    bool                                vld = false;
    bool                                updating = false;
    bool                                decodeDone = false;
    bool                                setSP = false;
    uint32_t                            waitCount = 0;
    ROBID                               bid;
    deque<StackRenameBus>               stackReqQ;
    void Reset() { vld = false; updating = false; decodeDone = false; setSP = false; waitCount = 0; stackReqQ.clear(); }
};

class StackRenameUnit : public SimObj {
private:
    StackRenameUnitState                current;
    StackRenameUnitState                next;
    vector<BlockStackState>             blockState;
    unordered_map<uint32_t, bStackState> bStack;
    deque<ROBID>                        preRenameQ;
    bool                                historyEnable;
    unordered_map<uint64_t, uint64_t>   pc_conf_table;
    unordered_map<int64_t, uint32_t>    smap;
    unordered_map<int64_t, uint32_t>    cmap;
public:
    virtual ~StackRenameUnit();
    Core                                *core;
    SimSys                              *sim;
    std::shared_ptr<DebugLog>           debugLogger = nullptr;
    StackConfig                         configs;
    StackStats                          *stats;
    void                                Work();
    void                                Xfer();
    void                                Build();
    void                                Reset();
    void ReportStat() override {}
    void                                flush(FlushBus flushReq);
    void                                flushByRid(FlushBus flushReq);
    void                                flushByBid(FlushBus flushReq);
    void                                replay(FlushBus flushReq);
    void                                replayByBid(FlushBus flushReq);
    void                                replayByRid(FlushBus flushReq);
    void                                replayForSingleBlk(ROBID bid);
    SimSys                              *GetSim() {return sim;}
    SimQueue<uint64_t>                  *stack_error_pc_q;
    /* \brief Recieve rename reuest from PE */
    void                                renameStack(StackRenameBus renameReq);
    /* \brief Rename stack load */
    void                                renameStackLoad(StackRenameBus &renameReq);
    /* \brief Rename stack store */
    void                                renameStackStore(StackRenameBus &renameReq);
    /* \brief Allocate stack physical register */
    uint32_t                            allocSptag(int64_t imm);
    /* \brief Release stack physical register */
    void                                releaseSptag(uint32_t sptag);
    /* \brief Insert stack set to table */
    void                                insertSatckSetTable(StackRenameBus &spRegReq);
    /* \brief Insert stack get to table */
    void                                insertSatckGetTable(StackRenameBus &spRegReq);
    /* \brief Set block rename status */
    void                                setBlockRenameState(BlockCommandPtr &cmd);
    /* \brief Whole block retire */
    void                                setBlockRetire(ROBID bid);
    /* \brief Set stack physical register ready */
    void                                setSpPtagReady(uint32_t sptag, bool ready, PLpvInfo &lpvInfo);
    /* \brief check physical register ready */
    bool                                checkSpPtagReady(uint32_t sptag);
    /* \brief check physical register lpv */
    PLpvInfo                            getSpPtagLpvInfo(uint32_t sptag);
    /* \brief Stack set instruction retire */
    void                                stackRetire(ROBID bid, ROBID rid, ROBID lsID);
    /* \brief Get offset rename status */
    bool                                getRenameStatus(int64_t imm);
    /* \brief Set offset rename status */
    void                                setRenameStatus(bool all, int64_t imm);
    /* \brief Mark hyper block */
    void                                setBlockHyper(ROBID bid);
    /* \brief Check if block has been renamed */
    bool                                getBlockStatus(ROBID bid);
    /* \brief Look up stack get/set PRF table */
    StackRenameBus                      lookupStackPRFTable(StackRenameBus &renameReq);
    /* \brief get pre-renamed oldest block */
    bool                                getPreRenamedStatus();
    /* \brief Renamed block check complete */
    void                                popPreRenamedBID();
    /* \brief Rename for diffrent type of request */
    bool                                renameForReq(StackRenameBus &renameReq);
    /* \brief Renew offset of sp addressing lconst */
    void                                renewLongOffset(ROBID bid, ROBID lsID, int64_t imm);
    /* \brief Check if the block has been recorded */
    bool                                checkBlockRecorded(uint64_t bpc);
    /* \brief Recieve stack rename information from refcore */
    void                                setHistoryTable(StackRenameBus req, uint64_t bpc);
    void                                setHistoryTableVld(uint64_t bpc);
    /* \brief mdb-table by tpc */
    bool                                checkConfTable(uint64_t tpc);
    bool                                checkSmapVld(int64_t imm);
    void                                decConfTableCount(uint64_t tpc);
    /* \brief check freelist size */
    bool                                checkStall(StackRenameBus &req);
    /* \brief If ptag is not enough for oldest block, free ptag by cmap */
    void                                freeForOldestBlock(ROBID bid);
    /* \brief Release ptag in cmap */
    void                                resetCmap();
    /* \brief Change smap base on offset */
    void                                smapOffset(int64_t imm);
    /* \brief Change cmap base on offset */
    void                                cmapOffset(int64_t imm);
    /* \breif Statistics for ptag */
    void                                rptOcupiedPtag();
    /* \brief Statistics for renamed block per cycle */
    void                                rptRenamedNum(uint64_t num);
    /* \brief Genarate rename request for template block */
    void                                genTemplateStack(const BlockCommandPtr cmd);
    void                                genExitTemplate(const BlockCommandPtr cmd);
    void                                GenFEntryTemplate(const BlockCommandPtr cmd);
    /* \brief commit instructions for oldest block */
    void                                stackRetire(uint32_t stid);
    /* \brief check stack rename done */
    bool                                CheckRename(ROBID bid);
    /* \brief set stack rename SP state */
    void                                setSPStat(ROBID bid);
    /* \brief set block end */
    void                                setBend(ROBID bid);
    /* \brief check load/store sp recorder */
    bool                                isStackInst(ROBID bid, uint32_t tpc, bool load);
    bool                                StackRenameEnable(uint32_t peid = 0);
};

} // namespace JCore