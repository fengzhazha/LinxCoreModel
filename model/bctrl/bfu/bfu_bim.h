#pragma once

#include <vector>

#include "bctrl/bfu/bfu_component.h"

namespace JCore {


namespace NS_CORE {

class BIM : public BFUComponent {
    struct BIMEntry {
        uint32_t size = 0;
        std::vector<BranchInfo> br;
        
        void Build(uint32_t nsize) { br.resize(nsize); Reset(); }
        void Reset() {
            for (uint32_t i = 0; i < br.size(); i++) {
                br[i].Reset();
            }
            size = 0;
        }
        bool full() { return size >= br.size(); }
        bool isTaken(uint32_t i) {
            return (br[i].vld && br[i].scnt.isTaken());
        }
        uint32_t getEntryIdx(addr_t pc) {
            uint32_t idx = -1U;
            for (uint32_t i = 0; i < br.size(); i++) {
                if (br[i].vld && br[i].pc == pc) {
                    idx = i;
                    break;
                }
            }
            return idx;
        }
        void evit(uint32_t idx, uint32_t replace_idx) {
            if (idx < replace_idx) {
                for (uint32_t i = replace_idx - 1; i >= idx; i--) {
                    br[i+1] = br[i];
                    if (i == 0) {
                        break;
                    }
                }
            } else if (idx > replace_idx) {
                for (uint32_t i = replace_idx; i < idx; i++) {
                    br[i] = br[i+1];
                }
            }
        }
        void insert(uint32_t idx, BrType branch, addr_t target, addr_t taken_pc, addr_t end_pc) {
            if (!(br[idx].vld && br[idx].pc == taken_pc &&
                branch == BranchType::BLK_BR_DIRECT && br[idx].attr != BranchType::BLK_BR_CALL)) {
                br[idx].attr = branch;
            }
            br[idx].vld = true;
            br[idx].pc = taken_pc;
            br[idx].tgt = target;
            br[idx].end_pc = end_pc;
        }
        uint32_t update(BrType branch, addr_t target, addr_t taken_pc, addr_t end_pc) {
            uint32_t idx = getEntryIdx(taken_pc);
            if (idx != -1U) {
                insert(idx, branch, target, taken_pc, end_pc);
                br[idx].ucnt.inc();
                return idx;
            }
            // replace
            if (full()) {
                uint32_t replace_idx = -1U;
                for (uint32_t i = 0; i < br.size(); i++) {
                    // recently not use and no taken
                    if (!br[i].scnt.isTaken() && br[i].ucnt.isZero()) {
                        replace_idx = -1U;
                    }
                    // insert in fetch order
                    if ((br[i].pc > taken_pc || i == br.size()-1) && idx != -1U) {
                        idx = i;
                    }
                }
                if (replace_idx == -1U) {
                    // decrease use count
                    for (uint32_t i = 0; i < br.size(); i++) {
                        br[i].ucnt.dec();
                    }
                    idx = -1U;
                } else {
                    // insert to idx, evit replace_idx
                    evit(idx, replace_idx);
                    br[idx].Reset();
                    insert(idx, branch, target, taken_pc, end_pc);
                }
                return idx;
            }
            // insert in pc order
            for (uint32_t i = 0; i < br.size(); i++) {
                if (!br[i].vld || (br[i].vld && br[i].pc > taken_pc)) {
                    idx = i;
                    evit(idx, size);
                    break;
                }
            }
            size ++;
            br[idx].Reset();
            insert(idx, branch, target, taken_pc, end_pc);
            return idx;
        }
    };

    set_t nset;
    std::vector<BIMEntry> bt;

public:
    BIM();
    ~BIM();

    void Build() override;
    void Lookup(PtrFB const& fb, const pos_t& pos);
    void Predict(PtrFB const& fb, pos_t pos);
    void update(PtrFB const& fb, pos_t pos);

    inline set_t CalcSetIdx(addr_t pc);
};

}

} // namespace JCore
