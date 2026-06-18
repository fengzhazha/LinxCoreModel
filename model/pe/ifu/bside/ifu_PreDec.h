#pragma once

#include "core/Bus.h"
#include "pe/ifu/common/ifu_component.h"

namespace JCore {


namespace PE_IFU {

class PreDec : public IFUComponent {
public:
    PreDec();
    ~PreDec();

    InnerBrType             preDecode(uint64_t bin, uint32_t instWidth, BlockType btype);
    uint64_t                preDecodeDirectJump(uint64_t bin, uint32_t instWidth, BlockType btype, uint64_t tpc);

private:
};
}

} // namespace JCore
