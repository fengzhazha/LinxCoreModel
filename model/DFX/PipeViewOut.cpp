#include "PipeViewOut.h"

#include "include/SimSys.h"

namespace JCore {
void PipeViewOut::Init(std::string fileName)
{
    cycles = sim->getCycles();
    oFile = std::unique_ptr<std::ofstream>{ new std::ofstream(fileName, std::ios::out)};
    Print("Kanata", version);
    Print("C=", cycles);
}

void PipeViewOut::PrintBlockStatus(BlockPipeViewPtr &blockInfo)
{
    const CycleBus &pipe = blockInfo->cycleInfo;
    const uint64_t pipeViewId = Start(pipe->createCycle, ++blockId, blockInfo->tid);
    SetCurrentRetireCycle(pipe->retireCycle);
    AddComment(pipeViewId, CommentType::LEFT_PANE, blockInfo->label);
    StartStage("F0", pipeViewId, pipe->createCycle);
    StartStage("F1", pipeViewId, pipe->f1Cycle);
    StartStage("F2", pipeViewId, pipe->fetchCycle);
    StartStage("F3", pipeViewId, pipe->fetchReturnCycle);
    StartStage("D1", pipeViewId, pipe->decodeCycle);
    StartStage("PE", pipeViewId, pipe->p1Cycle);
    StartStage("CM", pipeViewId, pipe->completeCycle);
    End(pipe->retireCycle, pipeViewId, retireId++, EndType::RETIRED);
    FlushBuffer();
}

void PipeViewOut::PrintGroup(GroupPipeViewPtr &groupInfo)
{
    const CycleBus &pipe = groupInfo->cycleInfo;
    const uint64_t pipeViewId = Start(pipe->f0Cycle, blockId, groupInfo->tid);
    SetCurrentRetireCycle(pipe->retireCycle);
    AddComment(pipeViewId, CommentType::LEFT_PANE, groupInfo->label);
    StartStage("F0", pipeViewId, pipe->createCycle);
    StartStage("F1", pipeViewId, pipe->f1Cycle);
    StartStage("F2", pipeViewId, pipe->fetchCycle);
    StartStage("F3", pipeViewId, pipe->fetchReturnCycle);
    StartStage("D1", pipeViewId, pipe->decodeCycle);
    StartStage("PE", pipeViewId, pipe->p1Cycle);
    StartStage("CM", pipeViewId, pipe->completeCycle);
    End(pipe->retireCycle, pipeViewId, retireId++, EndType::RETIRED);
    FlushBuffer();
}

void PipeViewOut::PrintUinst(InstPipeViewPtr &instInfo)
{
    if (instInfo->isVecLite) {
        PrintVecLiteUinst(instInfo);
        return;
    }
    const CycleBus &pipe = instInfo->cycleInfo;
    const uint64_t pipeViewId = Start(pipe->f0Cycle, blockId, instInfo->tid);
    SetCurrentRetireCycle(pipe->retireCycle);
    AddComment(pipeViewId, CommentType::LEFT_PANE, instInfo->label);
    StartStage("F0", pipeViewId, pipe->f0Cycle);
    StartStage("F1", pipeViewId, pipe->f1Cycle);
    StartStage("F2", pipeViewId, pipe->f2Cycle);
    StartStage("F3", pipeViewId, pipe->f3Cycle);
    StartStage("F4", pipeViewId, pipe->f4Cycle);
    StartStage("F5", pipeViewId, pipe->f5Cycle);
    StartStage("D0", pipeViewId, pipe->d0Cycle);
    StartStage("D1", pipeViewId, pipe->decodeCycle);
    StartStage("D2", pipeViewId, pipe->renameCycle);
    StartStage("D3", pipeViewId, pipe->d3Cycle);
    StartStage("S1", pipeViewId, pipe->dispatchCycle);
    Duration("IQ", pipeViewId, pipe->iqCycle, pipe->p1Cycle);
    Duration("RD", pipeViewId, pipe->rdyCycle, pipe->p1Cycle);
    StartStage("P1", pipeViewId, pipe->p1Cycle);
    StartStage("I1", pipeViewId, pipe->i1Cycle);
    StartStage("I2", pipeViewId, pipe->i2Cycle);
    StartStage("E0", pipeViewId, pipe->e0Cycle);
    StartStage("E1", pipeViewId, pipe->e1Cycle);
    StartStage("E2", pipeViewId, pipe->e2Cycle);
    StartStage("E3", pipeViewId, pipe->e3Cycle);
    StartStage("E4", pipeViewId, pipe->e4Cycle);
    StartStage("E5", pipeViewId, pipe->e5Cycle);
    StartStage("E1", pipeViewId, pipe->lsuRecvCycle);
    if (pipe->ldqAllocCycle != 0) {
        StartStage("Ldq", pipeViewId, pipe->ldqAllocCycle);
    }
    StartStage("L1M", pipeViewId, pipe->l1MissCycle);
    if (pipe->l2MissCycle != 0) {
        constexpr auto minL1MissLatency = 2;
        if (pipe->l2MissCycle >= pipe->l1MissCycle + minL1MissLatency) {
            StartStage("L2M", pipeViewId, pipe->l2MissCycle);
        }
    }
    StartStage("MR", pipeViewId, pipe->memRntCycle);
    StartStage("L2R", pipeViewId, pipe->l2RntCycle);
    StartStage("LQP", pipeViewId, pipe->ldqPickCycle);
    StartStage("LQI", pipeViewId, pipe->ldqIssueCycle);
    StartStage("L1R", pipeViewId, pipe->l1RntCycle);
    StartStage("LR", pipeViewId, pipe->ldRntCycle);
    StartStage("REQSPB", pipeViewId, pipe->reqSpbCycle);
    StartStage("REQONRI", pipeViewId, pipe->reqOnRingCycle);
    StartStage("REQOFFRI", pipeViewId, pipe->reqOffRingCycle);
    StartStage("REQMGB", pipeViewId, pipe->reqMgbCycle);
    StartStage("FRQ", pipeViewId, pipe->frqCycle);
    StartStage("TR", pipeViewId, pipe->tileWrCycle);
    StartStage("RESSPB", pipeViewId, pipe->resSpbCycle);
    StartStage("RESONRI", pipeViewId, pipe->resOnRingCycle);
    StartStage("RESOFFRI", pipeViewId, pipe->resOffRingCycle);
    StartStage("RESMGB", pipeViewId, pipe->resMgbCycle);
    StartStage("TOCORE", pipeViewId, pipe->resToCoreCycle);
    StartStage("W1", pipeViewId, pipe->w1Cycle);
    StartStage("W2", pipeViewId, pipe->w2Cycle);
    StartStage("CM", pipeViewId, pipe->completeCycle);

    StartStage("Issue", pipeViewId, pipe->cubeUopIssueCycle);
    StartStage("ReName", pipeViewId, pipe->cubeUopRenameCycle);
    StartStage("Wait", pipeViewId, pipe->cubeUopWaitCycle);
    StartStage("srcAready", pipeViewId, pipe->cubeUopSrcAreadyCycle);
    StartStage("srcBready", pipeViewId, pipe->cubeUopSrcBreadyCycle);
    StartStage("RdBuffer", pipeViewId, pipe->cubeUopRdbufferCycle);
    StartStage("Ctrl", pipeViewId, pipe->cubeUopCtrlCycle);
    StartStage("Calc", pipeViewId, pipe->cubeUopCalcCycle);
    StartStage("L0CWr", pipeViewId, pipe->cubeUopL0cwrCycle);
    StartStage("Commit", pipeViewId, pipe->cubeUopCommitCycle);

    StartStage("Issue", pipeViewId, pipe->ACCCVTIssueCycle);
    StartStage("Arb", pipeViewId, pipe->ACCCVTArbCycle);
    StartStage("Wait", pipeViewId, pipe->ACCCVTWaitCycle);
    StartStage("Src Rdy", pipeViewId, pipe->ACCCVTSrcRdyCycle);
    StartStage("Src Data", pipeViewId, pipe->ACCCVTSrcDataCycle);
    StartStage("FixPipe", pipeViewId, pipe->ACCCVTFixPipeCycle);

    /* Using for bridge stage */
    StartStage("Start", pipeViewId, pipe->startCycle);
    StartStage("WaitB", pipeViewId, pipe->waitOtherBPQCycle);
    StartStage("Wait", pipeViewId, pipe->waitCycle);
    StartStage("GenR", pipeViewId, pipe->genReadCycle);
    StartStage("Tag", pipeViewId, pipe->tagCycle);
    StartStage("WaitR", pipeViewId, pipe->waitReadCycle);
    StartStage("GenW", pipeViewId, pipe->genWriteCycle);
    StartStage("WaitW", pipeViewId, pipe->waitWriteCycle);
    StartStage("Integ", pipeViewId, pipe->intergratingCycle);
    StartStage("Ready", pipeViewId, pipe->dataReadyCycle);
    StartStage("TXed", pipeViewId, pipe->txedCycle);
    StartStage("Bus", pipeViewId, pipe->busSendCycle);
    StartStage("DBID", pipeViewId, pipe->receiveDBIDCycle);
    StartStage("Ret", pipeViewId, pipe->returnCycle);
    StartStage("Comp", pipeViewId, pipe->bridgeCompleteCycle);

    StartStage("Pick", pipeViewId, pipe->firstPickCycle);
    StartStage("Hit", pipeViewId, pipe->hitCycle);
    StartStage("Miss", pipeViewId, pipe->missCycle);
    StartStage("CAQ", pipeViewId, pipe->caqCycle);
    StartStage("BDBR", pipeViewId, pipe->bdbReadCycle);
    StartStage("BDBW", pipeViewId, pipe->bdbWriteCycle);
    StartStage("RFB", pipeViewId, pipe->rfbCycle);

    /* Using for mtc stage */
    StartStage("Start", pipeViewId, pipe->instStartCycle);
    StartStage("ToScalper", pipeViewId, pipe->sendToScalperCycle);
    StartStage("ToTile", pipeViewId, pipe->sendToTileReqCycle);
    StartStage("GenPreReq", pipeViewId, pipe->genPrefetchCycle);
    StartStage("sendMeoryReq", pipeViewId, pipe->sendMemoryReqCycle);
    StartStage("PreDataRet", pipeViewId, pipe->prefetchDataRetCycle);
    StartStage("Fromscalper", pipeViewId, pipe->sendFromScalperCycle);
    StartStage("GenLoadReq", pipeViewId, pipe->genLoadReadReqCycle);
    StartStage("genTileReadReq", pipeViewId, pipe->tileDataRetCycle);
    StartStage("TileDataRet", pipeViewId, pipe->genStoreReqCycle);
    StartStage("LoadDataRet", pipeViewId, pipe->loadDataReturnCycle);
    StartStage("Commit", pipeViewId, pipe->tlsCompleteCycle);

    End(pipe->retireCycle, pipeViewId, retireId++, EndType::RETIRED);
    FlushBuffer();
}

void PipeViewOut::PrintVecLiteUinst(InstPipeViewPtr &instInfo)
{
    const CycleBus &pipe = instInfo->cycleInfo;
    const uint64_t pipeViewId = Start(pipe->f0Cycle, blockId, instInfo->tid);
    SetCurrentRetireCycle(pipe->retireCycle);
    AddComment(pipeViewId, CommentType::LEFT_PANE, instInfo->label);
    StartStage("F", pipeViewId, pipe->f0Cycle);
    StartStage("P", pipeViewId, pipe->p1Cycle);
    StartStage("I", pipeViewId, pipe->i1Cycle);
    StartStage("S", pipeViewId, pipe->s1Cycle);
    StartStage("E1", pipeViewId, pipe->e1Cycle);
    StartStage("E2", pipeViewId, pipe->e2Cycle);
    StartStage("E3", pipeViewId, pipe->e3Cycle);
    StartStage("E4", pipeViewId, pipe->e4Cycle);
    StartStage("E5", pipeViewId, pipe->e5Cycle);
    StartStage("W1", pipeViewId, pipe->w1Cycle);
    StartStage("W2", pipeViewId, pipe->w2Cycle);
    StartStage("CM", pipeViewId, pipe->completeCycle);
    End(pipe->retireCycle, pipeViewId, retireId++, EndType::RETIRED);
    FlushBuffer();
}

}
