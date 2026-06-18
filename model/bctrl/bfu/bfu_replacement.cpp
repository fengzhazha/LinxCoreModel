#include "bctrl/bfu/bfu_replacement.h"

#include <cassert>

namespace JCore {


namespace NS_CORE {

NMRU::NMRU(set_t nset, way_t nway) : ReplacementPolicy(nset, nway) {
    repdata = std::vector<std::vector<RepData>>(nset, std::vector<RepData>(nway));
    for (auto& s : repdata) {
        for (auto& e : s) {
            e.valid = e.used = false;
        }
    }
}

void NMRU::Reset(set_t set_idx, way_t way_idx) {
    repdata[set_idx][way_idx].valid = false;
    repdata[set_idx][way_idx].used = false;

}

void NMRU::invalidate(set_t set_idx, way_t way_idx) {
    repdata[set_idx][way_idx].valid = false;
    repdata[set_idx][way_idx].used = false;
}

void NMRU::insert(set_t set_idx, way_t way_idx) {
    repdata[set_idx][way_idx].valid = true;
    touch(set_idx, way_idx);
}

void NMRU::touch(set_t set_idx, way_t way_idx) {
    repdata[set_idx][way_idx].used = true;
    // if all entries in the set are recently used, revert other ways 
    bool is_all_used = true;
    for (auto& e : repdata[set_idx]) {
        is_all_used &= e.used;
    }
    if (is_all_used) {
        for (auto& e : repdata[set_idx]) {
            e.used = false;
        }
        if (repdata[set_idx].size() > 1) {
            repdata[set_idx][way_idx].used = true;
        }
    }
}

way_t NMRU::getVictim(set_t set_idx) {
    std::vector<RepData>& set = repdata[set_idx];
    // if there is an invalid entry, select it as a victim
    for (way_t w=0; w<nway; w++) {
        if (!set[w].valid) return w;
    }
    // if there is no invalid entry, select the first entry with used bit = 0
    for (way_t w=0; w<nway; w++) {
        if (!set[w].used) return w;
    }
    assert(0 && "Error: NMRU cannot get victim.");
}

bool NMRU::isValid(set_t set_idx, way_t way_idx) {
    return repdata[set_idx][way_idx].valid;
}

}

} // namespace JCore
