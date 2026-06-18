#pragma once

#include <cstdint>
#include <memory>

#include "bctrl/bfu/common/core_type.h"
#include "ISA.h"
#include "bctrl/bfu/common/SPMinst.h"
#include "ModelCommon/SimInstInfo.h"

namespace JCore {

namespace NS_CORE {

struct RelateInfo {
    bool vld = false;
    seq_t fbid = 0;
    seq_t hid = 0;
    pos_t pos = 0;
};

using PtrMachineInst = std::shared_ptr<SimInstInfo>;

struct NukeInfo {
    seq_t fbid = 0;
    seq_t fbid_local = 0;
    uint64_t pc = 0;
    uint32_t stid = -1U;
};
}
} // namespace JCore
