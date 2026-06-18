#include "bctrl/bfu/bfu_hwpf.h"

#include "../configs/bfu_config.h"
#include "bctrl/bfu/bfu_logger.h"
#include "bctrl/bfu/bfu_utils.h"

namespace JCore {


using namespace std;

namespace NS_CORE {

void HWPF::Build() {
}

void HWPF::flush(uint32_t stid) {
    tgt_list.remove_if([stid](PtrHWPFInfo &info) {
        return info->stid == stid;
    });
}

void HWPF::observeFetch(PtrFB const& fb) {
    if (!cfg->hwpf_enable) return;
    assert(cfg->hwpf_policy == "NL" && "only support NL prefetch policy");

    addr_t va_cl = utils->AlignToCL(fb->va);
    for (uint32_t i=cfg->hwpf_nl_depth; i <cfg->hwpf_nl_depth+cfg->hwpf_nl_degree; i++) {
        addr_t pf_tgt = va_cl + i * BFU_CL_NBYTE;
        auto match = [pf_tgt](PtrHWPFInfo &info) {
            return info->va == pf_tgt;
        };
        if (find_if(tgt_list.begin(), tgt_list.end(), match) != tgt_list.end()) {
            logger->debug("HPWF", NIL, "hit tgt list, tgt=0x%llx, fbid=%d\n", pf_tgt, fb->fbid);
        } else {
            tgt_list.push_back(std::make_shared<HWPFInfo>(pf_tgt, 0, fb->stid));
            if (tgt_list.size() == cfg->hwpf_max_pending_req) tgt_list.pop_front();
        }
    }
}

PtrHWPFInfo HWPF::getTarget() {
    assert(tgt_list.size() <= cfg->hwpf_max_pending_req);
    if (tgt_list.empty()) {
        return nullptr;
    } else {
        PtrHWPFInfo tgt = tgt_list.front();
        tgt_list.pop_front();
        return tgt;
    }
}

}

} // namespace JCore
