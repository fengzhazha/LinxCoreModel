#pragma once
#ifndef CYCLE_INFO_H
#define CYCLE_INFO_H

#include <cstdint>
#include <memory>

namespace JCore {

/* Cycle recorder */
struct CycleInfo {
    /* \brief Create fetch request cycle. */
    uint64_t                createCycle = 0;
    /* \brief Enter pipeline PF0 cycle. (Start) */
    uint64_t                f0Cycle = 0;
    /* \brief Leave pipeline PF1 cycle. (Lookup ICache) */
    uint64_t                f1Cycle = 0;
    /* \brief Leave pipeline PF2 cycle. (Get Data) */
    uint64_t                f2Cycle = 0;
    /* \brief Leave pipeline PF3 cycle. (Insert to IB) */
    uint64_t                f3Cycle = 0;
     /* \brief Leave pipeline PF4 cycle. (Insert to IB) */
    uint64_t                f4Cycle = 0;
     /* \brief Leave pipeline PF5 cycle. (Insert to IB) */
    uint64_t                f5Cycle = 0;

    /* \brief cycle of fetching inst from pe used in pipeview */
    uint64_t                fetchCycle = 0;
    /* \brief cycle of returning inst to pe used in pipeview */
    uint64_t                fetchReturnCycle = 0;
    /* \brief cycle of round-robin for pending decoding inst used in pipeview */
    uint64_t                d0Cycle = 0;
    /* \brief cycle of decoding inst used in pipeview */
    uint64_t                decodeCycle = 0;
    /* \brief cycle of renaming inst registers used in pipeview */
    uint64_t                renameCycle = 0;
    /* \brief cycle of renamed inst registers used in pipeview */
    uint64_t                d3Cycle = 0;
    /* \brief cycle of renamed inst registers used in pipeview */
    uint64_t                renamedCycle = 0;
    /* \brief cycle of dispatching inst used in pipeview */
    uint64_t                dispatchCycle = 0;
    /* \brief cycle of inst entering issue queue cycle */
    uint64_t                iqCycle = 0;
    /* \brief cycle of inst ready cycle */
    uint64_t                rdyCycle = 0;
    /* \brief cycle of issuing/picking inst used in pipeview */
    uint64_t                p1Cycle = 0;
    /* \brief cycle of inst reading registers used in pipeview */
    uint64_t                i1Cycle = 0;
    /* \brief cycle of inst data bypass used in pipeview */
    uint64_t                i2Cycle = 0;
    /* \brief cycle of inst iq pending used in pipeview (veclite) */
    uint64_t                s1Cycle = 0;
    /* \brief cycle of inst executing used in pipeview */
    uint64_t                e0Cycle = 0;
    uint64_t                e1Cycle = 0;
    uint64_t                e2Cycle = 0;
    uint64_t                e3Cycle = 0;
    uint64_t                e4Cycle = 0;
    uint64_t                e5Cycle = 0;
    /* \brief cycle of inst entering load queue used in pipeview */
    uint64_t                lsuRecvCycle = 0;
    /* \brief cycle of selecting from load queue used in pipeview */
    uint64_t                ldqPickCycle = 0;
    uint64_t                ldqAllocCycle = 0;
    /* \brief cycle of sending request from load queue used in pipeview */
    uint64_t                ldqIssueCycle = 0;
    /* \brief cycle of inst l1 missing */
    uint64_t                l1MissCycle = 0;
    /* \brief cycle of inst l2 missing */
    uint64_t                l2MissCycle = 0;
    /* \brief cycle of memory returning data */
    uint64_t                memRntCycle = 0;
    /* \brief cycle of l2 returning data */
    uint64_t                l2RntCycle = 0;
    /* \brief cycle of l1 returning data */
    uint64_t                l1RntCycle = 0;
    /* \brief cycle to return load data from lsu to pe */
    uint64_t                ldRntCycle = 0;
    /* \brief cycle of inst writeback stage */
    uint64_t                w1Cycle = 0;
    /* \brief cycle of inst writeback stage */
    uint64_t                w2Cycle = 0;
    /* \brief cycle of inst complete */
    uint64_t                completeCycle = 0;
    /* \brief cycle of inst retire */
    uint64_t                retireCycle = 0;
    /* \brief cycle of push spb */
    uint64_t                reqSpbCycle = 0;
    /* \brief cycle of on ring */
    uint64_t                reqOnRingCycle = 0;
    /* \brief cycle off ring */
    uint64_t                reqOffRingCycle = 0;
    /* \brief cycle push mgb */
    uint64_t                reqMgbCycle = 0;
    /* \brief cycle push frq */
    uint64_t                frqCycle = 0;
    /* \brief cycle read/write tile register */
    uint64_t                tileWrCycle = 0;
    /* \brief cycle of push spb */
    uint64_t                resSpbCycle = 0;
    /* \brief cycle of on ring */
    uint64_t                resOnRingCycle = 0;
    /* \brief cycle off ring */
    uint64_t                resOffRingCycle = 0;
    /* \brief cycle push mgb */
    uint64_t                resMgbCycle = 0;
    /* \brief cycle response to core */
    uint64_t                resToCoreCycle = 0;
    /* \brief cycle CUBE UOP tile mul stage */
    uint64_t                cubeUopStartCycle = 0;
    uint64_t                cubeUopIssueCycle = 0;
    uint64_t                cubeUopRenameCycle = 0;
    uint64_t                cubeUopGenloadCycle = 0;
    uint64_t                cubeUopWaitCycle = 0;
    uint64_t                cubeUopSrcAreadyCycle = 0;
    uint64_t                cubeUopSrcBreadyCycle = 0;
    uint64_t                cubeUopSrcCreadyCycle = 0;
    uint64_t                cubeUopRdbufferCycle = 0;
    uint64_t                cubeUopCtrlCycle = 0;
    uint64_t                cubeUopCalcCycle = 0;
    uint64_t                cubeUopL0cwrCycle = 0;
    uint64_t                cubeUopCommitCycle = 0;
    /* \brief cycle CUBE UOP ACCCVT stage */
    uint64_t                ACCCVTStartCycle = 0;
    uint64_t                ACCCVTRenameCycle = 0;
    uint64_t                ACCCVTIssueCycle = 0;
    uint64_t                ACCCVTArbCycle = 0;
    uint64_t                ACCCVTWaitCycle = 0;
    uint64_t                ACCCVTSrcRdyCycle = 0;
    uint64_t                ACCCVTSrcDataCycle = 0;
    uint64_t                ACCCVTFixPipeCycle = 0;
    /* \brief cycle for tile bridge */
    uint64_t                startCycle = 0;
    uint64_t                waitOtherBPQCycle = 0;
    uint64_t                genReadCycle = 0;
    uint64_t                tagCycle = 0;
    uint64_t                waitReadCycle = 0;
    uint64_t                genWriteCycle = 0;
    uint64_t                waitWriteCycle = 0;
    uint64_t                intergratingCycle = 0;
    uint64_t                dataReadyCycle = 0;
    uint64_t                txedCycle = 0;
    uint64_t                busSendCycle = 0;
    uint64_t                receiveDBIDCycle = 0;
    uint64_t                waitCycle = 0;
    uint64_t                returnCycle = 0;
    uint64_t                bridgeCompleteCycle = 0;

    /* \brief cycle for tile bridge tag pipe */
    uint64_t                firstPickCycle = 0;
    uint64_t                hitCycle = 0;
    uint64_t                missCycle = 0;
    uint64_t                caqCycle = 0;
    uint64_t                bdbReadCycle = 0;
    uint64_t                bdbWriteCycle = 0;
    uint64_t                rfbCycle = 0;

    /* \brief cycle for mtc tload/tstore */
    uint64_t                instStartCycle = 0;
    uint64_t                sendToScalperCycle = 0;
    uint64_t                sendToTileReqCycle = 0;
    uint64_t                genPrefetchCycle = 0;
    uint64_t                prefetchDataRetCycle = 0;
    uint64_t                sendFromScalperCycle = 0;
    uint64_t                sendMemoryReqCycle = 0;
    uint64_t                genLoadReadReqCycle = 0;
    uint64_t                tileDataRetCycle = 0;
    uint64_t                genStoreReqCycle = 0;
    uint64_t                loadDataReturnCycle = 0;
    uint64_t                tlsCompleteCycle = 0;
};

using CycleBus = std::shared_ptr<CycleInfo>;
} // namespace JCore
#endif
