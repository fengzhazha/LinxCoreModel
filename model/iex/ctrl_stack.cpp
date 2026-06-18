#include "iex/ctrl_stack.h"

#include "iex/iex.h"

namespace JCore {


void CtrlStack::Work()
{
}

void CtrlStack::Build()
{
    ptr = 0;
}

void CtrlStack::Reset()
{
    ptr = 0;
}

SimSys* CtrlStack::GetSim()
{
    return iex->GetSim();
}

void CtrlStack::flush(FlushBus& bus) {}

const BrInfo& CtrlStack::Top()
{
    return ctrlStack[ptr - 1];
}

void CtrlStack::Push(BrInfo &brinfo)
{
    ASSERT(ptr <= CTRL_STACK_SIZE);
    ctrlStack[ptr++] = brinfo;
    return;
}

void CtrlStack::Pop()
{
    ASSERT(ptr > 0);
    ptr--;
}

bool CtrlStack::CheckRpc(uint64_t tpc)
{
    if (ptr <= 0) {
        return false;
    }
    if (ptr & 1) {
        return tpc == ctrlStack[ptr - 1].pc;
    }
    return tpc == ctrlStack[RPC_PTR].pc;
}


} // namespace JCore
