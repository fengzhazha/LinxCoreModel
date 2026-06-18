#include "pe/ifu/bside/ifu_PreDec.h"

#include "pe/ifu/iside/pe_ifu.h"

namespace JCore {


using namespace std;

namespace PE_IFU {

PreDec::PreDec() {}
PreDec::~PreDec() {}

InnerBrType PreDec::preDecode(uint64_t bin, uint32_t instWidth, BlockType btype)
{
    return getInstBrType(bin, btype, instWidth);
}
uint64_t PreDec::preDecodeDirectJump(uint64_t bin, uint32_t instWidth, BlockType btype, uint64_t tpc)
{
    uint64_t ret = 0;

    MInst inst = MInst();
    inst.DecodeBin(bin, instWidth);

    switch (inst.opcode) {
        case Opcode::OP_J:
            ret = tpc + static_cast<int64_t>(inst.srcs[SRC0_IDX]->value);
            break;
        case Opcode::OP_JR:
            ret = inst.srcs[SRC0_IDX]->data + static_cast<int64_t>(inst.srcs[SRC2_IDX]->value);
            break;
        default:
            ASSERT(0);
    }

    return ret;
}
}

} // namespace JCore
