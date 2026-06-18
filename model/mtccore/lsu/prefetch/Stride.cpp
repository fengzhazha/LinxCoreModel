#include "mtccore/lsu/prefetch/MtcStride.h"

#include <memory>

#include "mtccore/lsu/MtcLoadStoreUnit.h"
#include "mtccore/lsu/prefetch/MtcPrefetcher.h"
#include "SimSys.h"

namespace JCore {

bool MtcStride::train(MemReqBus &req)
{
    if (stride_disable) return false;
    if (prefetcher->verbose) {
        printf("[Stride]: %s. tpc 0x%lx, addr 0x%lx\n", __func__, req.tpc, req.addr);
        LOG_DEBUG(prefetcher->top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::LOW,
                  "[Stride]: %s. tpc 0x%lx, addr 0x%lx", __func__, req.tpc, req.addr);
    }
    bool filtered = false;
    bool hit  = false;
    PtrEntry e = nullptr;
    bool same_line = false;
    uint64_t req_align = req.addr & ~0x3f;
    for (auto p : mru_list) {
        if (p->pc == req.tpc) {
            hit = true;
            uint64_t delta;
            if (p->line_delta) {
                delta = (req.addr & ~0x3f) - (p->addr & ~0x3f);
            } else {
                delta = (req.addr - p->addr);
            }
            if (delta == 0) return true; // ignore
            // match
            switch (p->stage) {
                case 0:
                    if (delta < 0x40) {
                        p->line_delta = true;
                        p->stride = 0x40;
                    } else {
                        p->stride = delta;
                    }
                    p->stage = 1;
                    break;
                case 1:
                    if (p->stride == delta) {
                        p->stage = 2;
                    } else {
                        p->stride = delta;
                        p->stage  = 1;
                    }
                    break;
                case 2:
                    if (p->stride == delta) {
                        e = p;
                        same_line = ((p->addr & ~0x3f) == req_align);
                        filtered = true;
                    } else {
                        p->stage  = 1;
                        p->stride = delta;
                    }
                    break;
                default:
                    printf("%s: unexpected stage %d\n", __func__, p->stage);
                    ASSERT(0);
            }
            p->addr = req.addr;
            break;
        }
    }

    if (hit && e && !same_line) {
        uint64_t pfl1_addr;
        uint64_t pfl2_addr;
        uint64_t pfl1_delta = e->stride * pfl1_depth * (e->depth >= 2 ? 2 : 1);
        uint64_t pfl2_delta = e->stride * pfl2_depth *  e->depth;

        if (e->line_delta) {
            pfl1_addr = req_align + pfl1_delta;
            pfl2_addr = req_align + pfl2_delta;
        } else {
            pfl1_addr = (req.addr + pfl1_delta) & ~0x3f;
            pfl2_addr = (req.addr + pfl2_delta) & ~0x3f;
        }

        if (!filter_line(l1_filter, pfl1_addr)) {
            pfl1_q->push_back(pfl1_addr);
            pfl1_count_n++;
        }
        if (!filter_line(l2_filter, pfl2_addr)) {
            pfl2_q->push_back(pfl2_addr);
        }
        if (!filter_line(l1_filter, pfl1_addr) && prefetcher->verbose) {
            printf ("[Stride]: PFL1 0x%lx, E->depth %d\n", pfl1_addr, e->depth);
            LOG_DEBUG(prefetcher->top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::LOW,
                "[Stride]: PFL1 0x%lx, E->depth %d", pfl1_addr, e->depth);
        }
        if (!filter_line(l2_filter, pfl2_addr) && prefetcher->verbose) {
            printf("[Stride]: PFL2 0x%lx, E->depth %d\n", pfl2_addr, e->depth);
            LOG_DEBUG(prefetcher->top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::LOW,
                "[Stride]: PFL2 0x%lx, E->depth %d", pfl2_addr, e->depth);
        }

        // Update depth
        if (e->fix_depth) return filtered;
        uint64_t ti = sim->getCycles() - e->pref_time;
        int ti_i = (ti >= 15) ? 0 : (ti >= 8) ? 1 : 2;
        e->ti_list[ti_i]++;

        if (e->ti_list[ti_i] >= DEPTH_TI_THRE) {
            e->depth = (ti_i + 1);
            e->fix_depth = true;
        }
        e->pref_time = sim->getCycles();
        
        // ignore if not triggered or trig but same line
    } else {
        // Allocate new entry
        PtrEntry lru = mru_list.back();
        lru->addr  = req.addr;
        lru->pc    = req.tpc;
        lru->stage = 0;
        lru->pref_time = sim->getCycles();
        lru->line_delta = false;
        lru->depth = 1;
        lru->fix_depth = false;
        bzero(lru->ti_list, sizeof(int)*3);
        mru_list.pop_back();
        mru_list.push_front(lru);
    }

    return filtered;
}

void MtcStride::Reset(void)
{
    for (auto &e : mru_list) {
        e->pc = -1;
    }

    stride_disable = false;

    pfl1_count       = 0;
    pfl1_bad_count   = 0;
    pfl1_count_n     = 0;
    pfl1_bad_count_n = 0;
    interval_count   = 0;

    pfl1_depth = 4;
    pfl2_depth = 16;
}

void MtcStride::Build(void)
{
    for (int i = 0; i < N; i++) {
        auto n = std::make_shared<Entry>();
        mru_list.push_back(n);
    }
}

bool MtcStride::filter_line(std::deque<uint64_t> &filter, uint64_t addr)
{
    bool r = true;
    auto it = std::find(filter.begin(), filter.end(), addr);
    if (it == filter.end()) {
        r = false;
        filter.push_back(addr);
        if (filter.size() > 4) {
            filter.pop_front();
        }
    }
    return r;
}

void MtcStride::Work(void)
{
}

void MtcStride::Xfer(void)
{
}

void MtcStride::feedback(PtrPrefFB fb)
{
    if (fb->bad) {
        pfl1_bad_count_n++;
    }
    if (prefetcher->verbose) {
        printf("[Stride]: bad 0x%lx\n", fb->addr);
        LOG_DEBUG(prefetcher->top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::LOW,
                  "[Stride]: bad 0x%lx", fb->addr);
    }
}

} // namespace JCore