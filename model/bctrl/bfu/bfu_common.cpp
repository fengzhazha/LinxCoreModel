#include "bctrl/bfu/bfu_common.h"

namespace JCore {


namespace NS_CORE {

UBTBInfo& UBTBInfo::operator=(UBTBInfo &oldInfo) {
    this->hit = oldInfo.hit;
    this->set_idx = oldInfo.set_idx;
    this->way_idx = oldInfo.way_idx;
    this->taken = oldInfo.taken;
    this->tgt = oldInfo.tgt;
    this->taken_pc = oldInfo.taken_pc;
    this->end_pc = oldInfo.end_pc;
    this->taken_type = oldInfo.taken_type;
    this->path_hist.assign(oldInfo.path_hist.begin(), oldInfo.path_hist.end());
    return *this;
}

IBTBInfo& IBTBInfo::operator=(IBTBInfo &oldInfo) {
    this->table_idx = oldInfo.table_idx;
    this->tgt = oldInfo.tgt;
    this->lkup_info.assign(oldInfo.lkup_info.begin(), oldInfo.lkup_info.end());
    return *this;
}

TAGEInfo& TAGEInfo::operator=(TAGEInfo &oldInfo) {
    this->prim_idx = oldInfo.prim_idx;
    this->altn_idx = oldInfo.altn_idx;
    this->taken = oldInfo.taken;
    this->taken_pc = oldInfo.taken_pc;
    this->tgt = oldInfo.tgt;
    this->end_pc = oldInfo.end_pc;
    this->lkup_info.assign(oldInfo.lkup_info.begin(), oldInfo.lkup_info.end());
    return *this;
}

MainInfo& MainInfo::operator=(MainInfo &oldInfo) {
    this->vld = oldInfo.vld;
    this->taken = oldInfo.taken;
    this->end_pc_vld = oldInfo.end_pc_vld;
    this->taken_type = oldInfo.taken_type;
    this->pos = oldInfo.pos;
    this->end_pos = oldInfo.end_pos;
    this->tgt = oldInfo.tgt;
    this->taken_pc = oldInfo.taken_pc;
    this->end_pc = oldInfo.end_pc;

    this->is_from_sp = oldInfo.is_from_sp;
    this->is_from_bim = oldInfo.is_from_bim;
    this->is_from_tage = oldInfo.is_from_tage;
    this->is_from_ibtb = oldInfo.is_from_ibtb;
    this->is_from_loop = oldInfo.is_from_loop;
    this->is_from_ras = oldInfo.is_from_ras;
    this->is_from_ubtb = oldInfo.is_from_ubtb;
    this->path_hist.assign(oldInfo.path_hist.begin(), oldInfo.path_hist.end());
    return *this;
}

SFInfo FetchBundle::findShortForward(pos_t pos) {
    SFInfo info = SFInfo();
    for (pos_t pos_it = pos; pos_it >= 0; pos_it--) {
        if (pos_it >= BFU_BANDWIDTH) {
            break;
        }
        if (!sfb_info[pos_it].vld) {
            continue;
        }
        if (sfb_info[pos_it].main_info.pos >= pos) {
            info.found = true;
            info.pos = pos_it;
            break;
        }
    }
    return info;
}

void FetchBundle::recoverFB(pos_t pos) {
    auto info = findShortForward(pos);
    if (info.found) {
        pos_t pos_e = info.pos;
        for (uint32_t i = 0; i < BFU_TAGE_NPAGE; i++) {
            ghr_info[i] = sfb_info[pos_e].ghr_info[i];
            tage_info[i] = sfb_info[pos_e].tage_info[i];
        }
        ubtb_info = sfb_info[pos_e].ubtb_info;
        pbtb_info = sfb_info[pos_e].pbtb_info;
        ibtb_info = sfb_info[pos_e].ibtb_info;
        bim_info = sfb_info[pos_e].bim_info;
        ras_info = sfb_info[pos_e].ras_info;
        main_info = sfb_info[pos_e].main_info;
        recent_va = sfb_info[pos_e].va;
        va_prev = sfb_info[pos_e].va_prev;
        while (sfb_info[pos_e].vld) {
            sfb_info[pos_e].vld = false;
            pos_e = sfb_info[pos_e].tgt_pos;
        }
    }
}

bool FetchBundle::isFallThrough()
{
    for (uint32_t pos = 0; pos < BFU_BANDWIDTH; ++pos) {
        if (sp_info.attr[pos] != BranchType::BLK_BR_FALL) {
            return false;
        }
    }
    return true;
}

void FetchBundle::setInvalid(pos_t pos) {
    for ( ; pos < BFU_BANDWIDTH; ++pos) {
        if (machineInst[pos]) {
            machineInst[pos]->bfuInfo->vld = false;
        }
    }
}

bool Pipe::Empty()
{
    for (auto &mem :fb) {
        if (mem != nullptr) {
            return false;
        }
    }
    return true;
}

void Pipe::Flush()
{
    state = NS_CORE::PipeState::INVALID;
    stallIdx = 0;
    for (auto &f : fb) {
        f = nullptr;
    }
}

void Pipe::Flush(uint32_t stid)
{
    for (auto &f : fb) {
        if (f && f->stid == stid) {
            state = NS_CORE::PipeState::INVALID;
            f = nullptr;
            stallIdx = 0;
        }
    }
}

void Pipe::Reset()
{
    state = NS_CORE::PipeState::INVALID;
    stallIdx = 0;
    for (auto &f : fb) {
        f = nullptr;
    }
}

void Pipe::FlushByFbidGlobal(seq_t fbid, bool localFlush, uint32_t stid)
{
    bool flushed = true;
    for (auto &f : fb) {
        if (f == nullptr) {
            continue;
        }

        if (f->stid != stid) {
            flushed = false;
            continue;
        }

        if (!f || fbid < f->fbid || (localFlush && f->fbid == fbid)) {
            f = nullptr;
            continue;
        }
        flushed = false;
    }
    if (flushed) {
        state = NS_CORE::PipeState::INVALID;
        stallIdx = 0;
    }
}

void Pipe::FlushByFbidAndHid(seq_t fbid, seq_t hid, uint32_t stid)
{
    bool flushed = true;
    for (auto &f : fb) {
        if (f == nullptr) {
            continue;
        }
        if (f->stid != stid) {
            flushed = false;
            continue;
        }
        if (!f || fbid < f->fbid || (!f->global && hid != f->hid)) {
            f = nullptr;
            continue;
        }
        if (!f->global) {
            continue;
        }
        bool body = false;
        for (pos_t pos = 0; pos < BFU_BANDWIDTH; pos++) {
            if (f->machineInst[pos] == nullptr) {
                continue;
            }
            ASSERT(f->machineInst[pos]->bfuInfo && f->machineInst[pos]->bfuInfo->spInfo);
            if (f->machineInst[pos]->bfuInfo->hid == hid) {
                body = true;
                flushed = false;
            }
            if (body && (!f->machineInst[pos]->bfuInfo->spInfo->isInst || f->machineInst[pos]->bfuInfo->spInfo->isLast)) {
                f->main_info.pos = pos;
                f->end = true;
                f->machineInst[pos]->bfuInfo->spInfo->isLast = true;
                f->machineInst[pos]->bfuInfo->spInfo->isInst = true;
            }
        }
    }
    if (flushed) {
        state = NS_CORE::PipeState::INVALID;
        stallIdx = 0;
    }
}

uint32_t Pipe::GetFirstValidFBIdx()
{
    for (uint32_t i = 0; i < fb.size(); i++) {
        if (fb[i] != nullptr) {
            return i;
        }
    }
    return -1U;
}

void Pipe::Build(size_t size)
{
    fb.resize(size);
}

void LocalPipe::FreePipe()
{
    occupied = false;
    occupiedStid = -1U;
    ready = false;
    sizeVld = false;
    sizeGet = false;
    taken_type = BranchType::BLK_BR_INVAL;
    taken_pc = -1U;
    end_pc = -1U;
    tgt = 0;
}

bool LocalPipe::FreePipeBycheckPipe()
{
    if (pipe[F0].state == NS_CORE::PipeState::INVALID && pipe[F2].state == NS_CORE::PipeState::INVALID &&
        (pipe[F3].state == NS_CORE::PipeState::INVALID || (pipe[F3].fb[FIR] && pipe[F3].fb[FIR]->end))) {
        FreePipe();
    }
    return !occupied;
}

void LocalPipe::Flush(seq_t fbid, bool localFlush, uint32_t stid)
{
    for (auto &p : pipe) {
        p.FlushByFbidGlobal(fbid, localFlush, stid);
    }
    // TODO: stid
    if (occupied && occupiedStid == stid && (occupied_fbid > fbid || (fbid == occupied_fbid && localFlush))) {
        FreePipe();
    }
}

}

} // namespace JCore
