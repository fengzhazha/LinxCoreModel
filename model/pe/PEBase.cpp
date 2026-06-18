#include "PEBase.h"
#include "core/Core.h"

namespace JCore {
PEBase::PEBase()
{
}
PEBase::~PEBase()
{
}

void PEBase::Work()
{
}

void PEBase::Xfer()
{
}

void PEBase::Reset()
{
}

void PEBase::ReportStat()
{
}

SimSys* PEBase::GetSim()
{
    return sim;
}

uint32_t PEBase::GetThread() const
{
    return threadCnt;
}

void PEBase::SetBlockComplete(ROBID bid, uint32_t stid)
{
    core->bctrl->blockROB.completeBlock(bid, false, stid);
}

void PEBase::SetBlockException(ROBID bid, uint32_t stid, const char *info)
{
    core->bctrl->blockROB.reportException(bid, stid, info);
}

void PEBase::SetBlockRunning(ROBID bid, BlockStatus status, uint32_t stid)
{
    core->bctrl->blockROB.exeBlock(bid, status, stid);
}

void PEBase::SetTerminate(ROBID bid, uint32_t stid)
{
    core->bctrl->blockROB.reportTerminate(bid, stid);
}

void PEBase::ReportBlockFlush(FlushReq req)
{
    pe_flush_rpt_q->Write(req);
}
}
