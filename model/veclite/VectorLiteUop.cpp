#include "veclite/VectorLiteUop.h"
#include "DFX/BlockPipeViewInfo.h"
#include "core/Bus.h"

namespace JCore {

using namespace std;

atomic<uint64_t> VectorLiteUop::nextUid { 0 };

VectorLiteUop::VectorLiteUop(uint32_t idxVal, const BlockCommandPtr& bcmdVal, TileOp tileOpVal)
    : uid(GetNextUid()), idx(idxVal), cmd(bcmdVal), tileOp(tileOpVal), size(UOP_GRANULARITY)
{
    cycleInfo = make_shared<CycleInfo>();
}

VectorLiteUop::~VectorLiteUop() {}

std::string VectorLiteUop::Dump() const
{
    std::stringstream oss;
    oss << GetTileOpName(tileOp)
        << " (BPC:0x" << hex << cmd->bpc << ")"
        << " UOP[" << dec << uid << "]"
        << " latency:" << latency
        << " readRegLatency:" << readRegLatency
        << " size:" << size
        << " offset:" << offset
        << " slice:" << sliceIdx << "/" << totalSlices
        << " colSumLocked:" << colSumLocked;
    return oss.str();
}

InstPipeViewPtr VectorLiteUop::GetPipeViewInfo()
{
    InstPipeViewPtr instInfo = std::make_shared<InstPipeViewInfo>();
    instInfo->isVecLite = true;
    instInfo->bid = cmd->bid.val;
    instInfo->cycleInfo = cycleInfo;
    instInfo->label = Dump();
    return instInfo;
}

uint64_t VectorLiteUop::GetNextUid()
{
    return nextUid.fetch_add(1);
}

} // namespace JCore
