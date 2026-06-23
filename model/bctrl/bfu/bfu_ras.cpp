#include "bctrl/bfu/bfu_ras.h"

#include "../configs/bfu_config.h"
#include "bctrl/bfu/bfu_logger.h"
#include "bctrl/bfu/bfu_utils.h"

namespace JCore {


namespace NS_CORE {

RAS::RAS() {
};

RAS::~RAS() { 
}

void RAS::Build() {
    spec_table = std::vector<SpecEntry>{cfg->ras_depth};
    cmt_table = std::vector<addr_t>(cfg->ras_depth, 0); // do not use initialization list!!!
}

void RAS::backup(PtrFB const& fb) { 
    fb->ras_info.spec_wptr = spec_wptr; 
    fb->ras_info.spec_tos = spec_tos;
    fb->ras_info.cmt_alloc_ptr = cmt_alloc_ptr;
}

void RAS::restore(PtrFB const& fb) {
    if (cfg->ras_spec_cmt) {
        // clear from current spec_wptr to the backed up one
        // If spec_wptr equals to the backedup one and the entry is valid, 
        // this means we are recovering from a full spec table
        while ((spec_wptr != fb->ras_info.spec_wptr) || 
               (spec_wptr == fb->ras_info.spec_wptr && spec_table[spec_wptr].vld)) {
            spec_wptr = decPtr(spec_wptr);
            if (!spec_table[spec_wptr].vld) {
                std::cerr<<"fbid="<<fb->fbid<<",fbid_local="<<fb->fbid_local<<",fb wptr="<<fb->ras_info.spec_wptr<<",wptr="<<spec_wptr<<std::endl;
                assert(spec_table[spec_wptr].vld);
            }
            spec_table[spec_wptr].vld = false;
        }
    } else {
        spec_wptr = fb->ras_info.spec_wptr;
    }
    spec_tos = fb->ras_info.spec_tos;
    cmt_alloc_ptr = fb->ras_info.cmt_alloc_ptr;
}

void RAS::handleCall(addr_t next_pc) {
    // update spec table
    assert(!spec_table[spec_wptr].vld);
    spec_table[spec_wptr].vld = true;
    spec_table[spec_wptr].tgt = next_pc;
    spec_table[spec_wptr].nos = spec_tos;
    spec_table[spec_wptr].cmt_ptr = cmt_alloc_ptr;
    // manage pointers
    spec_tos = spec_wptr;
    spec_wptr = incPtr(spec_wptr);
    // if new spec_wptr is vld, spec table is full!
    cmt_alloc_ptr = incPtr(cmt_alloc_ptr);
    if (cfg->debug_enable) {
        LOG_INFO_M(Unit::BFU, Stage::NA) << "RAS handle call, push_pc=0x" << std::hex << next_pc
            << std::dec << ", tos=" <<  spec_tos << ", wptr" << spec_wptr << ", bos=" << spec_bos
            << ", alloc=" << cmt_alloc_ptr << " vld=0x" << valids();
    }
    logger->debug("RAS", NIL, "handle call, push_pc=0x%x, tos=%d, wptr=%d, bos=%d, alloc=%d, vld=%lx\n", next_pc, spec_tos, spec_wptr, spec_bos, cmt_alloc_ptr, valids());
}

void RAS::handleReturn() {
    // manage pointers
    spec_tos = spec_table[spec_tos].nos;
    cmt_alloc_ptr = decPtr(cmt_alloc_ptr);
    if (cfg->debug_enable) {
        LOG_INFO_M(Unit::BFU, Stage::NA) << "RAS handle return, tos=" << std::dec << spec_tos
            << ", wptr=" << spec_wptr << ", bos=" << spec_bos << ", cmt_alloc_ptr="
            << cmt_alloc_ptr << ", vld=" << std::hex << valids();
    }
    logger->debug("RAS", NIL, "handle return, tos=%d, wptr=%d, bos=%d, alloc=%d, vld=%lx\n", spec_tos, spec_wptr, spec_bos, cmt_alloc_ptr, valids());
}

idx_t RAS::incPtr(idx_t const& ptr) {
    return (ptr == cfg->ras_depth-1)? 0 : ptr+1;
}

idx_t RAS::decPtr(idx_t const& ptr) {
    return ptr? ptr-1 : cfg->ras_depth-1;
}

uint64_t RAS::valids() {
    uint64_t res = 0;
    for (idx_t i = 0; i <64 && i<cfg->ras_depth; i++) {
        res = res | uint64_t(spec_table[i].vld) << i;
    }
    return res;
}

bool RAS::needStall() {
    // need to stall only if seperate spec/cmt table is enabled
    if (!cfg->ras_spec_cmt || spec_table.empty()) {
        return false;
    }
    // handleCall writes exactly at spec_wptr.  A valid write slot is a real
    // allocation hazard even if recovery has left holes elsewhere in the ring.
    return spec_table[spec_wptr].vld;
}

addr_t RAS::Predict(PtrFB const& fb, pos_t pos) {
    // This method will be called when main predictor predicts a RET
    auto& ras_info = fb->sfb_info[pos].vld ? fb->sfb_info[pos].ras_info : fb->ras_info;
    idx_t tos = cfg->ras_0cycle_pred ? ras_info.spec_tos : spec_tos;
    idx_t cmt_ptr = cfg->ras_0cycle_pred ? ras_info.cmt_alloc_ptr : cmt_alloc_ptr;
    if (cfg->ras_spec_cmt) {
        // spec table empty: use cmt table
        addr_t tgt = spec_table[tos].vld ? spec_table[tos].tgt : cmt_table[decPtr(cmt_ptr)];
        if (cfg->debug_enable) {
            LOG_INFO_M(Unit::BFU, Stage::NA) << "RSA predict, tos=" << std::dec << tos << ", vld="
                << spec_table[tos].vld << std::hex << ", tgt=0x" << tgt << std::dec << ", cmt_alloc_ptr=" << cmt_ptr;
        }
        logger->debug("RAS", NIL, "predict, tos=%d, vld=%d, tgt=0x%x, cmt_alloc_ptr=%d\n", tos, spec_table[tos].vld, tgt, cmt_ptr); 
        return tgt;
    } else {
        return spec_table[tos].tgt;
    }
}

void RAS::RunAtCommit(PtrMachineInst const& h, PtrFB const& fb) {
    if (cfg->ras_spec_cmt && utils->IsCall(h->GetBranchType())) {
        pos_t pos = utils->CalcPosInFB(h->GetBundlePosPC(), fb->va);
        auto info = fb->findShortForward(pos);
        auto& ras_info = info.found ? fb->sfb_info[info.pos].ras_info : fb->ras_info;
        auto &e = spec_table[ras_info.spec_wptr];
        if (!e.vld) {
            if (cfg->debug_enable) {
                LOG_INFO_M(Unit::BFU, Stage::NA) << std::dec << "RAS commit exception fbid=" << fb->fbid
                    << ", fbid_local=" <<fb->fbid_local << std::hex << ", pc_start=0x" << h->pc
                    << ", pos_pc=0x" << h->GetBundlePosPC() << ", end_pc=0x" << utils->NextBlockPC(h);
            }
            std::cerr << std::dec << "fbid=" << fb->fbid << " fbid_local=" << fb->fbid_local << " pc start:0x"
                      << std::hex << h->GetBundlePosPC() << " pc end:0x" << h->GetBundlePosPC() << std::endl;
            assert(e.vld && "spec table valid should be true at commit!");
        }
        // copy to commit table
        cmt_table[e.cmt_ptr] = e.tgt;
        // clear from current spec_bos to spec_wptr, and update spec_bos
        idx_t new_spec_bos = incPtr(ras_info.spec_wptr);
        do {
            assert(spec_table[spec_bos].vld);
            spec_table[spec_bos].vld = false;
            spec_bos = incPtr(spec_bos);
        } while (spec_bos != new_spec_bos);
    }
    if (cfg->debug_enable) {
        LOG_INFO_M(Unit::BFU, Stage::NA) << "RAS after commit, fbid=" << std::dec << fb->fbid
            << ", fbid_local=" << fb->fbid_local << std::hex << ", px=0x" << h->GetBundlePosPC() << std::dec
            << ", tos=" << spec_tos << ", wptr=" << spec_wptr << ", bos=" << spec_bos
            << ", alloc=" << cmt_alloc_ptr << std::hex << ", vld=0x" << valids();
    }
    logger->debug("RAS", NIL, "after commit, fbid=%d, fbid_local=%d, pc=0x%x,"
                  "tos=%d, wptr=%d, bos=%d, alloc=%d, vld=%lx\n",
                  fb->fbid, fb->fbid_local, h->GetBundlePosPC(), spec_tos, spec_wptr, spec_bos, cmt_alloc_ptr, valids());
}

void RAS::runAt0CyclePred(PtrFB const& fb, pos_t pos) {
    if (cfg->ras_0cycle_pred) {
        if (pos != utils->CalcPosInFB(fb->va)) {
            // stash for short forward branch
            pos_t pre_pos = fb->sfb_info[pos].pre_pos;
            fb->sfb_info[pre_pos].ras_info = fb->ras_info;
        }
        // backup ras info
        backup(fb);
        // handle call/return according to ubtb prediction
        if (fb->ubtb_info.taken) {
            BrType type = fb->ubtb_info.taken_type;
            if (utils->IsRet(type)) {
                // override ubtb_info target (must be placed before handleReturn()!)
                fb->ubtb_info.tgt = Predict(fb, pos);
                handleReturn();
                // rewrite hist if this ubtb target is given by RAS
                fb->ubtb_info.path_hist.pop_back();
                fb->ubtb_info.path_hist.emplace_back(fb->ubtb_info.taken_pc, fb->ubtb_info.tgt, type, true);
            } else if (utils->IsCall(type)) {
                handleCall(fb->ubtb_info.end_pc);
            }
        }
    }
}

void RAS::runAtLB0CyclePred(PtrFB const& fb)
{
    if (cfg->ras_0cycle_pred) {
        // backup ras info
        backup(fb);
        // handle call/return according to loopbuffer prediction
        if (fb->lb_info.taken) {
            pos_t pos = fb->lb_info.pos;
            addr_t pc = fb->machineInst[pos]->GetBundlePosPC();
            BrType type = fb->sp_info.attr[pos];
            if (utils->IsRet(type)) {
                handleReturn();
            } else if (utils->IsCall(type)) {
                handleCall(utils->NextBlockPC(pc));
            }
        }
    }
}

void RAS::runAtMainPredFlush(PtrFB const& fb) {
    if (cfg->ras_0cycle_pred) {
        // restore ras info
        restore(fb);
        // handle call/return according to main pred flush information
        if (fb->main_info.taken) {
            if (utils->IsRet(fb->main_info.taken_type)) {
                handleReturn();
            } else if (utils->IsCall(fb->main_info.taken_type)) { // can be BL or BLR!
                handleCall(fb->main_info.end_pc);
            }
        }
    }
}

void RAS::runCallFlush(PtrFB const& fb, pos_t pos) {
    if (cfg->ras_0cycle_pred) {
        // restore ras info
        restore(fb);
        // handle call/return according to main pred flush information
        auto& h = fb->machineInst[pos];
        handleCall(utils->NextBlockPC(h));
    }
}

void RAS::runAtMainPred(PtrFB const& fb) {
    if (!cfg->ras_0cycle_pred) {
        // backup ras info
        backup(fb);
        // handle call/return according to main prediction
        if (fb->main_info.taken) {
            if (utils->IsRet(fb->main_info.taken_type)) {
                handleReturn();
            } else if (utils->IsCall(fb->main_info.taken_type)) {
                handleCall(fb->main_info.end_pc);
            }
        }
    }
}

void RAS::runAtRedirect(PtrFB const& fb) {
    // restore ras info
    restore(fb);
    // handle call/return according to redirect information 
    // 1. BCOND misprediction: no call/ret should be before the mispredicted pos
    // 2. CALL misprediction: should not be BL
    // 3. RET address misprediction: handleReturn() again
    auto& h = fb->machineInst[fb->redir_info.pos];
    if (utils->IsRet(fb->sp_info.attr[fb->redir_info.pos])) {
        handleReturn();
    } else if (utils->IsCall(fb->sp_info.attr[fb->redir_info.pos])) {
        handleCall(utils->NextBlockPC(h));
    }
    // logger->debug("RAS", NIL, "after redirect, tos=%d, wptr=%d, bos=%d, alloc=%d, vld=%lx\n", spec_tos, spec_wptr, spec_bos, cmt_alloc_ptr, valids());
}

void RAS::runAtNuke(PtrMachineInst const& h, PtrFB const& fb) {
    // TODO: is it possible that any call/ret is before the replay pos?
    restore(fb);
    if (h->IsReturn()) {
        handleReturn();
    } else if (h->IsCall()) {
        handleCall(utils->NextBlockPC(h));
    }
    // logger->debug("RAS", NIL, "after nuke, tos=%d, wptr=%d, bos=%d, alloc=%d, vld=%lx\n", spec_tos, spec_wptr, spec_bos, cmt_alloc_ptr, valids());
}

}

} // namespace JCore
