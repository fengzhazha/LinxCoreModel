#include "bctrl/bfu/bfu_loop_pred.h"

#include "../configs/bfu_config.h"
#include "bctrl/bfu/bfu_logger.h"
#include "bctrl/bfu/bfu_utils.h"

namespace JCore {


using namespace std;

namespace NS_CORE {

LoopPred::LoopPred() {
}

LoopPred::~LoopPred()
{
    DeletePtr(train_rep);
    DeletePtr(pred_rep);
}

void LoopPred::Build() {
    train_table = std::vector<TableEntry>{cfg->loop_pred_depth};
    pred_table = std::vector<TableEntry>{cfg->loop_pred_depth};
    for (idx_t i = 0; i <cfg->loop_pred_depth; i++) {
        train_table[i].idx = i;
        pred_table[i].idx = i;
    }
    train_rep = new NMRU(1, cfg->loop_pred_depth);
    pred_rep = new NMRU(1, cfg->loop_pred_depth);
}

void LoopPred::Predict(PtrFB const& fb, pos_t pos) {
    if (!cfg->loop_pred_enable) return;
    // Lookup by header pc (after SP, already concated), but save information in fb using pos. (FIXME)
    auto h = fb->machineInst[pos];
    addr_t pc = h->GetBundlePosPC();

    auto pred_entry = Lookup(pc, pred_table);
    auto& info = fb->loop_info[pos];

    // update active states when pc changes (currently we only care about single-pc self-loop, FIXME)
    if (last_pc != pc) {
        active = (pred_entry != nullptr);
        if (active) pred_entry->curr_cnt = 0;
    }
    last_pc = pc;

    // do prediction (hit and is in active prediction)
    if (active && pred_entry) {
        info.hit = true;
        info.pred_taken = !(++(pred_entry->curr_cnt) == pred_entry->last_cnt);
        pred_rep->touch(0, pred_entry->idx);
        logger->debug("LP", NIL, "LP Predict, fbid=%d, pc=0x%x, idx=%d, taken=%d\n", fb->fbid, pc, pred_entry->idx, info.pred_taken);
        stats->loop_predicted ++;
    }

    // backup information (after current header changes LP states, so we only need to restore on misprediction)
    backup(fb, info, pred_entry);
}

void LoopPred::RunAtCommit(PtrMachineInst const& h, PtrFB const& fb) {
    if (!cfg->loop_pred_enable) return;
    // use the PC of the header rather than pos-based va
    addr_t pc = h->GetBundlePosPC();
    pos_t pos = utils->CalcPosInFB(h, fb->va);

    auto pred_entry = Lookup(pc, pred_table);
    auto train_entry = Lookup(pc, train_table);
    assert(!(pred_entry && train_entry));
    
    // hits pred table, do not update train table
    if (pred_entry) {
        if (h->bfuInfo->resolve_mispredict && fb->loop_info[pos].hit) {
            pred_entry->Reset();
            pred_rep->invalidate(0, pred_entry->idx);
            logger->debug("LP", NIL, "invalidate pred table entry, pc=%x, idx=%d\n", pc, pred_entry->idx);
            stats->loop_mispred ++;
        }
        return;
    }
    // hits train table, update the counters and confidence
    if (train_entry) {
        train_entry->curr_cnt ++;
        if (!h->bfuInfo->resolve_taken) {
            if (train_entry->last_cnt == train_entry->curr_cnt) train_entry->conf ++;
            else train_entry->conf = train_entry->conf? train_entry->conf-1 : 0;
            train_entry->last_cnt = train_entry->curr_cnt;
            train_entry->curr_cnt = 0;
        }
        train_rep->touch(0, train_entry->idx);
        // if conf is higher than train threshold, move to pred table
        if (train_entry->conf >= cfg->loop_pred_conf_thd) {
            idx_t alloc_idx = pred_rep->getVictim(0);
            addr_t evict_pc = pred_table[alloc_idx].pc;
            pred_table[alloc_idx] = *train_entry;
            pred_rep->insert(0, alloc_idx);
            train_entry->Reset();
            train_rep->invalidate(0, train_entry->idx);
            logger->debug("LP", NIL, "upgrade to pred table, pc=%x, train_idx=%d, pred_idx=%d, loop_cnt=%d, evict_pc=0x%x\n", 
                          pc, train_entry->idx, alloc_idx, pred_table[alloc_idx].last_cnt, evict_pc);
            stats->loop_trained ++;
        }
    }
    // allocate on new one on miss
    if (!train_entry && h->IsCond() && !h->bfuInfo->resolve_taken && 
        h->bfuInfo->resolve_mispredict && pc == fb->sp_info.tgt[pos]) {
        // TODO: it's better to use h->predict_tgt rather than sp_info, if BROB doesn't modify prediction info. 
        idx_t alloc_idx = train_rep->getVictim(0);
        auto& e = train_table[alloc_idx];
        e.Reset();
        e.pc = pc;
        train_rep->insert(0, alloc_idx);
        logger->debug("LP", NIL, "allocate train table, pc=%x, idx=%d\n", pc, alloc_idx);
    }
}

void LoopPred::runAtRedirect(PtrMachineInst const& h, PtrFB const& fb) {
    if (!cfg->loop_pred_enable) return;
    // TODO: invalidate on misprediction?
    pos_t pos = utils->CalcPosInFB(h, fb->va);
    // restore LP states right AFTER this header
    restore(fb->loop_info[pos]);
}

void LoopPred::runAtNuke(PtrMachineInst const& h, PtrFB const& fb) {
    if (!cfg->loop_pred_enable) return;
    pos_t pos = utils->CalcPosInFB(h, fb->va);
    restore(fb->loop_info[pos]);
}

LoopPred::TableEntry* LoopPred::Lookup(addr_t pc, vector<TableEntry>& table) {
    for (auto& e : table) {
        if (pc == e.pc) return &e;
    }
    return nullptr;
}

void LoopPred::backup(PtrFB const& fb, LoopInfo& info, TableEntry* entry) {
    info.backup_active = active;
    if (active && entry) {
        info.backup_pc = entry->pc;
        info.backup_curr_cnt = entry->curr_cnt;
    }
}

void LoopPred::restore(LoopInfo& info) {
    active = info.backup_active;
    if (active) {
        auto e = Lookup(info.backup_pc, pred_table);
        if (e) e->curr_cnt = info.backup_curr_cnt;  
    }
}

}

} // namespace JCore
