#pragma once

#include <cstdint>
#include <iostream>

#include "../configs/bctrl_config.h"
#include "../configs/cube_config.h"
#include "bctrl/bctrl_stats.h"
#include "bctrl/BIFU.h"
#include "bctrl/BIssue.h"
#include "bctrl/BMDB.h"
#include "bctrl/BROB.h"
#include "bctrl/spe/DCTop.h"
#include "bctrl/spe/SPE.h"
#include "core/Bus.h"
#include "ModelSpec.h"
#include "plat/DebugLog.h"

namespace JCore {

class SimSys;
class Core;

struct BCtrlState {
    /* \brief rob stall signal */
    bool                    robStall = false;
    /* \brief rename stall signal */
    bool                    renameStall = false;
    /* \brief External control from inputs */
    CoreControlBus          extControl;
    /* \brief Standard dispatch pointer */
    uint32_t                dispatchSTDLane = 0;
    /* \brief Auxiliary dispatch pointer */
    uint32_t                dispatchAUXLane = 0;
    /* \brief Block Sequence at Dispatch */
    bool                    dispatchVld = false;
    bool                    dispatchStall = false;
    uint32_t                dispatchBSeq = 0;

    uint32_t                dispatchBid = 0;
    uint32_t                debug_counter = 0;
};

struct MarkPeData {
    uint32_t                peid = 0;
    ROBID                   bid;
    uint64_t                index = 0;
    ROBID                   preBid;
    uint64_t                preTpc = 0;
    uint64_t                prePeid = 0;
    uint64_t                instSize = 0;
    bool                    first = true;
    uint64_t                tpc = 0;
    BlockType               btype;
};

class BCtrlUnit : public SimObj {
protected:
    bool                      recover = false;
    uint32_t                  getMask = 0;
    uint32_t                  setMask = 0;
public:
    BCtrlState                current;
    BCtrlState                next;
    // TODO: remove
    // BHeader                   last_header = std::shared_ptr<FullBlockHeader>(nullptr);
    std::vector<BHeader>      last_header;

    bool                      block_head_renamed = false;

    /* \brief Count the number of instructions in the current block. */
    BHeader                   terminate_header = std::shared_ptr<FullBlockHeader>(nullptr);
    uint32_t                  block_head_insts_cnt = 0;

    /* \brief Block Header Fetch unit */
    BlockIFU                  blockFetchUnit;
    /* \brief Block Reorder Buffer */
    BlockROB                  blockROB;
    /* \brief Block Dispatch unit */
    // BlockDispatch             blockDispatchUnit;
    BlockIssueQueueUnit       blockIssueQueueUnit;
    std::shared_ptr<SPE>      scalarPE = nullptr;

    // bool                      decodeStall;
    // bool                      robStall;
    std::vector<bool>         robStall;
    std::vector<bool>         decodeStall;
    // std::vector<BDispCntEntry> dispatchLaneCnt;
    INPUT SimQueue<Credit>                      *m_biqBCCCreditQ;
    OUTPUT SimQueue<IDBus>                      *dispatch_id_q;

    BCtrlUnit();
    virtual ~BCtrlUnit();
    SimSys                    *sim;
    Core                      *core;
    BCtrlConfig               configs;
    CUBEConfig                cubeConfig;
    BCtrlStats*               stats;
    std::shared_ptr<DebugLog> debugLogger = nullptr;
    BMDB                      bmdb;
    ROBID                     lastBid;
    uint64_t                  loadCount = 0;
    uint64_t                  storeCount = 0;
    bool                      readFile = false;
    bool                      waitMcopy = false;
    vector<uint64_t>          regData;
    std::vector<bool>         flushRecovering;
    /* \for perfectls model, Simt is counted as one block. */
    std::vector<uint64_t>     curBid;

    /* \brief Decode block header */
    void                      RslvDirectly(BHeader &header);
    void                      RecordBccEfficient() const;
    void                      Reset();
    void                      Work();
    void                      Xfer();
    void                      interrupt(CoreControlBus control, uint32_t stid);
    SimSys                    *GetSim() {return sim;}
    void ReportStat() override {}

    void                      flush(FlushBus flushReq);
    void                      replay(FlushBus flushReq);

    void                      Build();
    void                      BuildScalarPE();
    void                      RetireBlock(const ROBID& bid, uint32_t stid);
    bool                      StackRenameEnable(uint32_t peid = 0);
    void                      ReportLocalRelease(OperandType typ, uint32_t seq, uint32_t tid, uint32_t peid);

    std::vector<ROBID> currentBID;
    std::vector<BHeader> currentBHeader;
    std::vector<BlockCommandPtr> currentBlkCmd;
    std::vector<SimInst> stallInst;

    uint32_t PickThread() const;
    void RunFetchStage5(uint32_t stid);
    uint64_t DispatchToPE();
    bool CheckStall(uint32_t stid);
    bool CheckBROBStall(uint64_t slot, uint32_t stid);
    bool CheckSTDPEStall(uint64_t slot);
    void SMTTerminate(uint32_t stid);
};

/* \brief Print a block header to Ostream */
std::ostream& operator<<(std::ostream& out, BHeader const &header);

/* \brief Print a block rob entry to Ostream */
std::ostream& operator<<(std::ostream& out, BROBEntry const &brobEntry);


} // namespace JCore
