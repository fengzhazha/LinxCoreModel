#include "LGPRRF.h"
#include "core/Bus.h"
#include "core/Core.h"

namespace JCore {
    
LGPRRF::LGPRRF() {}
LGPRRF::~LGPRRF() {}

void LGPRRF::Work()
{
    lFreeListInput.Work();
    lFreeListOutput.Work();
}

void LGPRRF::Xfer()
{
    lFreeListInput.Xfer();
    lFreeListOutput.Xfer();
}

void LGPRRF::Build()
{
    lFreeListInput.debugLogger = debugLogger;
    lFreeListInput.SetSim(GetSim());
    lFreeListInput.Build();

    lFreeListOutput.debugLogger = debugLogger;
    lFreeListOutput.SetSim(GetSim());
    lFreeListOutput.Build();
}

bool LGPRRF::CheckStallInput(uint32_t reserve)
{
    return lFreeListInput.CheckStall(reserve);
}

bool LGPRRF::CheckStallOutput(uint32_t reserve)
{
    return lFreeListOutput.CheckStall(reserve);
}

void LGPRRF::Flush(FlushBus &flushReq)
{
    lFreeListInput.Flush(flushReq);
    lFreeListOutput.Flush(flushReq);
}
}