#pragma once

#include <vector>

#include "bctrl/bfu/bfu_component.h"

namespace JCore {


namespace NS_CORE {

class TAGE : public BFUComponent {
    struct TaggedEntry {
        UCnt u;
        addr_t tag = -1U;
        uint32_t size = 0;
        SatCnt3b w;
        std::vector<BranchInfo> br;

        void Build(uint32_t nsize) { br.resize(nsize); Reset(); }
        void Reset() {
            for (uint32_t i = 0; i < br.size(); i++) {
                br[i].Reset();
            }
            size = 0;
            tag = -1U;
            u.Reset();
            w.Reset(true);
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
    // tbl_idx, set_idx, way_idx
    std::vector<std::vector<std::vector<TaggedEntry>>> tt; 

    msize_t ntable;
    msize_t nway;
    std::vector<msize_t> hist_len;
    std::vector<msize_t> tag_len;
    std::vector<msize_t> nset;
    uint32_t id = 0;
    uint32_t ghrHistNbit = 0;

public:    
    TAGE();
    ~TAGE();
    void Build() override;
    void Lookup(PtrFB const& fb, const pos_t& pos);
    void Predict(PtrFB const& fb, pos_t pos);
    void update(PtrFB const& fb, pos_t pos);
    void allocate(PtrFB const& fb, pos_t pos);

    idx_t calcSetIdx(ghr_t* hist, addr_t pc, idx_t table_idx);
    tag_t calcTag(ghr_t* hist, addr_t pc, idx_t table_idx);
    void getTakenInfo(TAGEInfo::LookupInfo& info, TaggedEntry& e, addr_t pc);
    bool MisPrim(idx_t primIdx);
    void RptHashStats(idx_t t, tag_t tag, addr_t pc, addr_t tgt);
    void SetID(uint32_t i);
};

}

} // namespace JCore
