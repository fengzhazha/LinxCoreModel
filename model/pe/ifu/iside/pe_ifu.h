#pragma once

#include <deque>
#include <vector>

#include "../configs/ifu_config.h"
#include "pe/ifu/bside/IBP.h"
#include "pe/ifu/bside/ifu_PreDec.h"
#include "pe/ifu/bside/ifu_SP.h"
#include "pe/ifu/common/ifu_common.h"
#include "pe/ifu/common/ifu_component.h"
#include "pe/ifu/common/ifu_stats.h"
#include "pe/ifu/iside/ifu_icache.h"
#include "pe/PEBase.h"
#include "ModelCommon/SimInstInfo.h"
#include "pe/PECommon/DecodeState.h"
#include "pe/PECommon/DecodeBundle.h"

#define FEXIT_AUTOGEN_SIZE 3
#define FENTRY_AUTOGEN_SIZE 2
#define TEMPLATE_AUTOGEN_IMM 2048

namespace JCore {


namespace PE_IFU {
struct FetchInfo {
    ROBID bid;
    uint32_t stid = -1U;
    uint64_t addr;
};

class PEIFU {
    friend class BFUComponent;
public:
    PEIFU();
    ~PEIFU();
    MachineType                         machineType = MachineType::UNKNOW_CORE;
    PEBase                              *top;
    SimSys                              *sim;
    uint32_t                            ifuID;
    uint32_t                            memID;  // memory id to L2
    bool                                verbose;
    uint64_t                            stdDecWidth;
    uint64_t                            simtDecWidth;
    uint32_t                            logicalGroups = 0;

    // IFU component
    IFUConfig                           configs;
    IFUStats*                           stats;
    IFUUtils*                           utils;
    std::vector<Pipe>                   pipe;
    std::vector<FetchReqPtr>            f2PipeWait;
    IFUICache                           icache;
    IBP                                 ibp;
    PreDec                              preDecodor;
    SP                                  sp = SP(nullptr);

    /* \brief Receive request packets from L2. */
    INPUT SimQueue<PtrPacket>           *l2_ifu_q;
    /* \brief Fetch control information from bcc. */
    INPUT ThdRingQ<BlkBodyFetchState>*   pe_ifu_q;

    /* \brief Prefetch information. */
    INPUT ThdRingQ<FetchInfo>             pre_fetch_q;

    /* \brief Send request packets from IFU to L2. */
    OUTPUT SimQueue<PtrPacket>          *ifu_l2_q;
    /* \brief Send response packets from IFU to L2. */
    OUTPUT SimQueue<PtrPacket>          *snp_l2_q;
    /* \brief Fetch binary bundle to decode. */
    OUTPUT ThdSimQ<DecodeBundle>*       ifuDecThdQ;
    OUTPUT SimQueue<DecodeBundle>       f4SimBuffer;
    OUTPUT SimQueue<DecodeBundle>       f5SimBuffer;
    /* \brief Branch prediction destination queue */
    RingQueue<BrqEntry>                 *brq;
    /* \brief The pointer of fetchControlQ to generate fetch request. */
    std::vector<uint32_t>                    genFetchReqPtr;
    /* \brief Run ahead of fetch requests buffer. */
    RingQueue<FetchReqPtr>              rahq;
    uint32_t                            m_curF0ThreadId;
    std::vector<bool>                   merge;
    std::vector<uint64_t>               mergeBin;
    std::vector<uint32_t>               mergeSize;
    std::vector<ROBID>                  mergeGid;
    std::vector<uint32_t>               mergeStid;
    RingQueue<uint64_t>                 hasPrefMap;

    bool                                ifustall = false;

    void                                Work();
    void                                Xfer();
    void                                Reset();
    void                                Build(uint32_t peID, uint64_t stdDecWidth, uint64_t simtDecWidth);
    void                                setFlush(FlushBus &flushReq);
    void                                flushFront(ROBID bid, bool flush, uint32_t tpc, uint32_t m_threadId);
    void                                flushByLoad(FlushFrontInfo &flushInfo);
    void                                FlushGidFront(FlushFrontInfo &flushInfo, bool gidOnly = false);
    void                                flushGidFrontByMiss(uint32_t tid, uint32_t stid);
    void                                releaseFeCtrlQ(ROBID bid, uint32_t tid, uint32_t stid);
    SimSys                              *GetSim();
    void                                ReportStat();
    void                                ReportVec(const std::string& name);

    // Input
    /* \brief Input prefetch information from BCC decode. */
    void                                setPrefetchQ(uint64_t tpc, uint32_t bytes, ROBID &bid,
                                                     uint32_t tid, uint32_t stid);
    void                                setPrefetchQ(uint64_t tpc, uint32_t bsize, uint32_t instSize, ROBID &bid,
                                            uint32_t tid, uint32_t stid);
    /* \brief Receive instruction packet from L2 Cache. */
    void                                RecvInstPkt();
    /* \brief Wake up L1 ICache miss requests in pipe and RAHQ. */
    void                                wakeUpMissReq(uint64_t addr);
    /* \brief Sleep L1 hit requests in pipe and RAHQ. */
    void                                sleepHitReq(uint64_t addr);

    // Output
    /* \brief Send instruction packet to L2 Cache. */
    void                                sendInstPkt(PtrPacket pkt, bool resp);

    // Function
    /* \brief Perfect IFU. */
    void                                RunPerfIfu();
    /* \brief Generate perfect fetch request. */
    void                                GenPerfFetchReq(uint32_t tid);
    void                                InsertPerfToIb();
    /* \brief Whether the IFU is idle */
    bool                                IsIfuIdle();
    /* \brief First process flush and cache requests. */
    void                                RunAtPre();
    /* \brief F3: Pre-decoding and Arbitration Branch Prediction. */
    void                                RunF3();
    /* \brief F2: Aaccelss the ICache to obtain binary data. */
    void                                RunF2();
    void                                MoveF2Wait(FetchReqPtr &fetchReq);
    void                                RestoreF2();
    /* \brief F1: Lookup the ICache. */
    void                                RunF1();
    /* \brief F0: Generate fetch requests and Start pipe. */
    void                                RunF0();
    void                                PipeCtrl();
    void                                MoveOneStage(Pipe& src, Pipe& dst);
    void                                PipeMove();
    /* \brief Handle ICache prefetch. */
    void                                HandlePreFetch(uint32_t tid);
    /* \brief Whether the prefetch queue exceeds the threshold and release half. */
    void                                CheckPrefQFull(uint32_t tid);
    void                                CheckPrefQFull(uint32_t size, uint32_t tid);
    /* \brief Whether the front of prefetchQ and the TPC are in the same cacheline. */
    void                                CheckPrefQFront(uint64_t tpc, uint32_t tid);
    /* \brief Generate fetch requests. */
    void                                GenFetchReq();
    bool                                CheckFetchReq(uint32_t pickThread);
    bool                                PickThread(uint32_t &pickThread);
    bool                                IsInPrefMap(uint64_t tpc);
    void                                InsertPrefMap(uint64_t tpc);
    void                                UpdatePrefMap(uint64_t tpc);
    void                                RelasePrefMap(uint64_t tpc);
    void                                WaitForFetch(uint32_t tid);
    void                                ContinueFetch(uint32_t tid, uint64_t fetchTpc, ROBID &bid,
                                                      bool pref, uint32_t stid);
    /* \brief Merge fetch request. */
    void                                mergeFetchReq(FetchReqPtr const& fetchReq, uint32_t fetchWidth);
    /* \brief Select fetch request to lookup when pipe free. */
    void                                LookUpRahqReq();
    /* \brief Select miss fetch request to process when pipe free. */
    void                                HandleRahqReq();
    /* \brief Send fetch data to F4 stage. */
    void                                InsertToF4(FetchReqPtr const& fetchReq);
    /* \brief Send fetch data to F5 stage. */
    void                                InsertToF5();
    /* \brief Send fetch data to instruction bufffer. */
    void                                InsertToIB();
    void                                Dump() const;

    void                                StallEnable();
    void                                StallDisable();
};
}


} // namespace JCore
