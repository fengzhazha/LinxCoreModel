#pragma once

#ifndef DECODE_STATE_H
#define DECODE_STATE_H

#include <vector>

#include "core/Bus.h"
#include "bctrl/LocalRegMgr.h"
#include "vectorcore/vpe/vrf/VRFRename.h"

namespace JCore {
struct BrqEntry {
    bool                    vld = false;
    ROBID                   bid;
    uint64_t                src = 0;
    uint64_t                dst = 0;
    BrqEntry() = default;
    BrqEntry(ROBID id, uint64_t s, uint64_t d) {vld = true; bid = id; src = s; dst = d;}
};
struct DecodeState {
    bool                    vld = false;
    bool                    stall = false;
    BlkSrcType              blkSrcType = BlkSrcType::NONE_SRC;
    ROBID                   bid;
    CodeTemplateID          templateID = CodeTemplateID::NON_TEMPLATE;
    ROBID                   lsID;
    uint64_t                sid = 0;
    std::vector<ROBID>      vcSid;
    std::vector<bool>       winding;
    uint64_t                load_id = 0;
    ROBID                   tid;
    ROBID                   uid;
    std::vector<std::shared_ptr<LocalRegMgr>> tRegMgr;
    std::vector<std::shared_ptr<LocalRegMgr>> uRegMgr;
    std::shared_ptr<VRFRename>           vrfRename;
    std::vector<std::shared_ptr<LocalRegMgr>> predRegMgr;
};
}
#endif