#pragma once

#include "bctrl/bfu/common/MachineInst.h"
#include "bctrl/bfu/common/mem_req.h"
#include "core/Bus.h"
#include "core/Packet.h"

namespace JCore {


struct BFUInterface {
    SimQueue<NS_CORE::PtrMemReq>* bfu_l2c_q;
    SimQueue<NS_CORE::PtrMemReq>* l2c_bfu_q;
    SimQueue<PtrPacket>* bfu_l2_q;
    SimQueue<PtrPacket>* bfu_hpre_l2_q;
    SimQueue<PtrPacket>* l2_bfu_q;
    SimQueue<PtrPacket>* snp_l2_q;
    std::vector<SimQueue<NS_CORE::PtrMachineInst>*> bfu_be_q;
    std::vector<SimQueue<NS_CORE::PtrMachineInst>*> be_bfu_rslv_q;
    SimQueue<NS_CORE::NukeInfo>* be_bfu_nuke_info_q;
    std::vector<SimQueue<NS_CORE::PtrMachineInst>*> be_bfu_cmt_q;
    NS_CORE::PtrMachineInst be_bfu_nuke;
};


} // namespace JCore
