#pragma once

#include <vector>

#include "bctrl/bfu/bfu_component.h"
#include "bctrl/bfu/bfu_replacement.h"

namespace JCore {


namespace NS_CORE {

class LoopPred : public BFUComponent {
    struct TableEntry {
        idx_t idx = -1U;
        addr_t pc = -1U;
        uint32_t curr_cnt = 0;
        uint32_t last_cnt = 0;
        uint32_t conf = 0;
        void Reset() {
            pc = -1U; 
            curr_cnt = last_cnt = conf = 0;
        }
    };

    /** A prediction is valid (active) only if pred_table predicts ALL branches in a self-loop. 
     *  This avoids potential mispredictions when a pred_table entry is inserted at the middle of the self-loop.
    */
    bool active = false;
    addr_t last_pc = -1U;

    std::vector<TableEntry> train_table;
    std::vector<TableEntry> pred_table;
    ReplacementPolicy* train_rep {nullptr};
    ReplacementPolicy* pred_rep {nullptr};

public:
    LoopPred();
    ~LoopPred();

    void Build() override;

    /** do prediction at main prediction */
    void Predict(PtrFB const& fb, pos_t pos);

    /** do train at commit time */
    void RunAtCommit(PtrMachineInst const& h, PtrFB const& fb);

    /** handle bru flush and nuke flush */
    void runAtRedirect(PtrMachineInst const& h, PtrFB const& fb);
    void runAtNuke(PtrMachineInst const& h, PtrFB const& fb);

private:
    TableEntry* Lookup(addr_t pc, std::vector<TableEntry>& table);
    void backup(PtrFB const& fb, LoopInfo& info, TableEntry* active_entry);
    void restore(LoopInfo& info);
};


}

} // namespace JCore
