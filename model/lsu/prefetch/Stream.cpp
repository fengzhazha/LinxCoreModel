#include "lsu/prefetch/Stream.h"

#include <algorithm>

#include "../configs/lsu_config.h"
#include "lsu/lsu.h"
#include "lsu/prefetch/Prefetcher.h"

namespace JCore {

void Stream::Reset(void) {
    filter_buf.clear();

    for (auto p : mru1_list) {
        *p = {};
    }

    for (auto p : mru0_list) {
        *p = {};
    }

    stream_disable = false;

    pfl1_count       = 0;
    pfl1_bad_count   = 0;
    pfl1_count_n     = 0;
    pfl1_bad_count_n = 0;
    interval_count   = 0;
}

void Stream::Build(void) {
    for (uint32_t i = 0; i < l0_table_size; i++) {
        auto p = std::make_shared<L0Entry>();
        mru0_list.push_back(p);
    }

    for (uint32_t i = 0; i < l1_table_size; i++) {
        auto p = std::make_shared<L1Entry>();
        mru1_list.push_back(p);
    }
    auto &cfg = prefetcher->configs;
    ACCURACY_HIGH = float(cfg->pref_stream_accuracy_high) / cfg->pref_stream_propor_base;
    ACCURACY_LOW = float(cfg->pref_stream_accuracy_low) / cfg->pref_stream_propor_base;
    THRE_LATENESS = float(cfg->pref_stream_thre_lateness) / cfg->pref_stream_propor_base;
    THRE_POLLUTION = float(cfg->pref_stream_thre_poll) / cfg->pref_stream_propor_base;
    THRE_MISS = float(cfg->pref_stream_thre_miss) / cfg->pref_stream_propor_base;
    confLevel = VERY_CONSERVATIVE;
    confArray.resize(LVL_NUM);
    // distance = region_size * prf_depth * pf_lev_mole / pf_lev_deno
    confArray[VERY_CONSERVATIVE].level = VERY_CONSERVATIVE;
    confArray[VERY_CONSERVATIVE].region_size  = 512;
    confArray[VERY_CONSERVATIVE].pf_lev_mole = 1;
    confArray[VERY_CONSERVATIVE].pf_lev_deno = 1;
    confArray[CONSERVATIVE].level = CONSERVATIVE;
    confArray[CONSERVATIVE].region_size = 512;
    confArray[CONSERVATIVE].pf_lev_mole = 2;
    confArray[CONSERVATIVE].pf_lev_deno = 1;
    confArray[MIDDLE1].level = MIDDLE1;
    confArray[MIDDLE1].region_size = 512;
    confArray[MIDDLE1].pf_lev_mole = 4;
    confArray[MIDDLE1].pf_lev_deno = 1;
    confArray[MIDDLE2].level = MIDDLE2;
    confArray[MIDDLE2].region_size = 1024;
    confArray[MIDDLE2].pf_lev_mole = 4;
    confArray[MIDDLE2].pf_lev_deno = 1;
    confArray[MIDDLE3].level = MIDDLE3;
    confArray[MIDDLE3].region_size = 1024;
    confArray[MIDDLE3].pf_lev_mole = 8;
    confArray[MIDDLE3].pf_lev_deno = 1;
    confArray[AGGRESSIVE].level = AGGRESSIVE;
    confArray[AGGRESSIVE].region_size = 1024;
    confArray[AGGRESSIVE].pf_lev_mole = 16;
    confArray[AGGRESSIVE].pf_lev_deno = 1;
    confArray[VERY_AGGRESSIVE].level = VERY_AGGRESSIVE;
    confArray[VERY_AGGRESSIVE].region_size = 2048;
    confArray[VERY_AGGRESSIVE].pf_lev_mole = 16;
    confArray[VERY_AGGRESSIVE].pf_lev_deno = 1;
    setPFParam();
}

void Stream::train(MemReqBus &req) {
    if (stream_disable) return;

    if (filter_line(req.addr)) {
        return;
    }
    if (prefetcher->verbose) {
        LOG_INFO_M(Unit::LSU, Stage::NA) << "[Stream]: " << __func__ << ". tpc 0x" << hex << req.tpc
            << ", addr 0x" << req.addr;
    }
    uint64_t align_addr = req.addr & ~0x3f;
    bool hit;

    hit = train_l1(align_addr);
    if (!hit) {
        train_l0(align_addr);
    }
}

bool Stream::filter_line(uint64_t addr) {
    bool r = true;
    uint64_t align_addr = addr & ~0x3f;
    auto it = std::find(filter_buf.begin(), filter_buf.end(), align_addr);
    if (it == filter_buf.end()) {
        r = false;
        filter_buf.push_back(align_addr);
        if (filter_buf.size() > filter_buf_depth) {
            filter_buf.pop_front();
        }
    }
    return r;
}

bool Stream::train_l1(uint64_t addr) {
    bool hit = false;
    uint64_t tri_rgn = to_region_addr(addr);
    for (auto it = mru1_list.begin(); it != mru1_list.end(); it++) {
        auto e = *it;
        if (e->region_addr == tri_rgn) {
            uint32_t off = to_region_off(addr);
            if (!e->bits.bits[off]) {
                e->bits.bits[off] = true;
                e->aaccelss_cnt++;
            }
            hit = true;
            if (prefetcher->verbose) {
                LOG_INFO_M(Unit::LSU, Stage::NA) << __func__ << ": region 0x" << hex << e->region_addr
                    << ", aaccelss_cnt " << dec << e->aaccelss_cnt;
            }
        }
        if (region_p1(e->region_addr) == tri_rgn) {
            if (e->aaccelss_cnt > (region_off_n/2 > 2*l1_trig_thr ? 2*l1_trig_thr : l1_trig_thr)) {
                uint64_t l1_lev = (pfl1_depth * (pref_level >= 3 ? 3 : pref_level));
                l1_lev = l1_lev > 0 ? l1_lev : 1;
                uint64_t l2_lev = pfl2_depth * pfl2_lev_mole / pfl2_lev_deno;
                l2_lev = l2_lev > 0 ? l2_lev : 1;
                uint64_t pfl1_base = tri_rgn + region_size * l1_lev;
                uint64_t pfl2_base = tri_rgn + region_size * l2_lev;
                if (prefetcher->verbose) {
                    LOG_INFO_M(Unit::LSU, Stage::NA) << __func__ << ": tri_rgn 0x" << hex << tri_rgn
                        << ", pfl1_base 0x" << pfl1_base << ", pfl2_base 0x" << pfl2_base;
                }
                for (uint32_t i = 0; i < region_off_n; i++) {
                    uint64_t pfl1_addr = pfl1_base + (i << 6);
                    uint64_t pfl2_addr = pfl2_base + (i << 6);
                    pfl1_q->push_back(pfl1_addr);
                    pfl2_q->push_back(pfl2_addr);
                    pfl1_count_n++;
                    if (prefetcher->verbose) {
                        LOG_INFO_M(Unit::LSU, Stage::NA) << "[Stream]: PFL1 0x" << hex << pfl1_addr;
                    }
                }
            }
            e->region_addr = tri_rgn;
            e->bits        = {};
            e->aaccelss_cnt  = 0;
            hit = true;
        } else if (region_m1(e->region_addr) == tri_rgn) {
            if (e->aaccelss_cnt > l1_trig_thr) {
                uint64_t pfl1_base = tri_rgn - region_size * pfl1_depth;
                uint64_t pfl2_base = tri_rgn - region_size * pfl2_depth;
                if (prefetcher->verbose) {
                LOG_INFO_M(Unit::LSU, Stage::NA) << __func__ << ": tri_rgn 0x" << hex << tri_rgn
                    << ", pfl1_base 0x" << pfl1_base;
                }
                for (uint32_t i = 0; i < region_off_n; i++) {
                    uint64_t pfl1_addr = pfl1_base + (i << 6);
                    uint64_t pfl2_addr = pfl2_base + (i << 6);
                    pfl1_q->push_back(pfl1_addr);
                    pfl2_q->push_back(pfl2_addr);
                    pfl1_count_n++;
                    if (prefetcher->verbose) {
                        LOG_INFO_M(Unit::LSU, Stage::NA) << "[Stream]: PFL1 0x" << hex << pfl1_addr;
                    }
                }
            }
            e->region_addr = tri_rgn;
            e->bits        = {};
            e->aaccelss_cnt  = 0;
            hit = true;
        }

        if (e->region_addr == region_p1(tri_rgn)) {
            // Ignore
            hit = true;
        }
        if (hit) {
            mru1_list.erase(it);
            mru1_list.push_front(e);
            break;
        }
    }
    return hit;
}

void Stream::train_l0(uint64_t addr) {
    uint64_t tri_rgn = to_region_addr(addr);
    bool hit = false;
    for (auto it = mru0_list.begin(); it != mru0_list.end(); it++) {
        auto e = *it;
        if (e->region_addr == tri_rgn) {
            uint32_t off = to_region_off(addr);
            if (!e->bits.bits[off]) {
                e->bits.bits[off] = true;
                e->aaccelss_cnt++;
                if (e->aaccelss_cnt > l1_trig_thr) {
                    insert_l1(e);
                    *e = {};
                    mru0_list.erase(it);
                    mru0_list.push_back(e);
                } else {
                    mru0_list.erase(it);
                    mru0_list.push_front(e);
                }
            }

            hit = true;
            break;
        }
    }

    if (!hit) {
        insert_l0(addr);
    }
}

uint64_t Stream::to_region_addr(uint64_t addr) {
    return (addr & ~region_mask);
}

uint32_t Stream::to_region_off(uint64_t addr) {
    return ((addr & region_mask) >> 6);
}

uint64_t Stream::region_p1(uint64_t addr) {
    return (addr + region_size);
}

uint64_t Stream::region_m1(uint64_t addr) {
    return (addr - region_size);
}

void Stream::insert_l0(uint64_t addr) {
    PtrL0Entry e = mru0_list.back();
    uint64_t rgn_addr = to_region_addr(addr);
    uint32_t off      = to_region_off(addr);
    e->aaccelss_cnt     = 1;
    e->bits           = {};
    e->bits.bits[off] = true;
    e->region_addr    = rgn_addr;
    mru0_list.pop_back();
    mru0_list.push_front(e);
}

void Stream::insert_l1(PtrL0Entry l0_e) {
    PtrL1Entry e = mru1_list.back();
    e->aaccelss_cnt  = l0_e->aaccelss_cnt;
    e->bits        = l0_e->bits;
    e->region_addr = l0_e->region_addr;
    e->conf        = 0;
    mru1_list.pop_back();
    mru1_list.push_front(e);
    if (prefetcher->verbose) {
        LOG_INFO_M(Unit::LSU, Stage::NA) << __func__ << ": region_addr 0x" << hex << l0_e->region_addr;
    }
}

void Stream::Work(void) {
    if (pfl1_bad_count > (pfl1_count/2)) {
        stream_disable = true;
    } else {
        stream_disable = false;
    }

    if (prefetcher->configs->pref_stream_dyn_enable && !pref_bad) {
        if (pfl1_count_n == 512) {
            pfl1_count += pfl1_count_n;
            pfl1_count_n = 0;
            if (pfl1_bad_count_n < 64) {
                pref_level = (pref_level + 1) >= 4 ? 4 : (pref_level+1);
            } else {
                pref_level = 1;
                pref_bad = true;
            }
            pfl1_bad_count_n = 0;
            if (pfl1_bad_count_n < 64 && prefetcher->verbose) {
                LOG_INFO_M(Unit::LSU, Stage::NA) << "[Stream]: increase prefetch level to " << pref_level;
            } else if (prefetcher->verbose) {
                LOG_INFO_M(Unit::LSU, Stage::NA) << "[Stream]: switch to pref_bad";
            }
        }
    }
    if (prefetcher->verbose) {
        LOG_INFO_M(Unit::LSU, Stage::NA) << "[Stream]: cycle: " << prefetcher->GetSim()->getCycles()
            << ", region_size:" << region_size << ", pfl1_dep:" << pfl1_depth
            << ", pfl2_dep:" << pfl2_depth << ", pref_lvl:" << pref_level;
    }
}

void Stream::Xfer(void) {

}

void Stream::feedback(PtrPrefFB fb) {
    if (fb->bad) {
        pfl1_bad_count_n++;
    }
    if (prefetcher->verbose) {
        LOG_INFO_M(Unit::LSU, Stage::NA) << "[Stream]: bad 0x" << hex << fb->addr << dec
            << ". " << __func__ << ": pfl1 bad/total = " << pfl1_bad_count << "/" << pfl1_count;
    }
}

void Stream::L2Feedback(PtrPrefFB fb)
{
    if (!fb->vld || !fb->isFromL2) {
        return;
    }
    float accuracy = calRate(fb->total_pref_useful, fb->total_pref_sent_to_next, fb->interval_pref_useful,
        fb->interval_pref_sent_to_next);
    float lateness = calRate(fb->total_pref_late, fb->total_pref_useful, fb->interval_pref_late,
        fb->interval_pref_useful);
    float pollution = calRate(fb->total_dmd_miss_due_pref, fb->total_dmd_miss, fb->interval_dmd_miss_due_pref,
        fb->interval_dmd_miss);
    float miss = calRate(fb->total_dmd_miss, fb->total_data_req, fb->interval_dmd_miss, fb->interval_data_req);
    if (prefetcher->verbose) {
        LOG_INFO_M(Unit::LSU, Stage::NA) << "[Stream]: L2 Feedback. The accuracy " << accuracy
            << ", lateness " << lateness << ", pollution_due_pref " << pollution << ", miss_rate" << miss;
    }
    if (!prefetcher->configs->fdp_stream_enable) {
        return;
    }
    PFConfOP op = getPFConfOP(accuracy, lateness, pollution, miss);
    if (op == INCREMENT) {
        confLevel = incPFLevel(confLevel);
        prefetcher->top->stats->fdp_stream_inc++;
        // prefetcher->top->lsu.at(3).stats->fdp_stream_inc++;
        setPFParam();
        if (prefetcher->verbose) {
            LOG_INFO_M(Unit::LSU, Stage::NA) << "[Stream]: " << getStr(op) << " prefetch conf. region_size 0x"
                << hex << region_size;
        }
    } else if (op == DECREMENT) {
        confLevel = decPFLevel(confLevel);
        prefetcher->top->stats->fdp_stream_dec++;
        // prefetcher->top->lsu.at(3).stats->fdp_stream_dec++;
        setPFParam();
        if (prefetcher->verbose) {
            LOG_INFO_M(Unit::LSU, Stage::NA) << "[Stream]: " << getStr(op) << " prefetch conf. region_size 0x"
                << hex << region_size;
        }
    }
}

std::string Stream::getStr(PFConfOP op)
{
    if (op == INCREMENT) {
        return "INCREMENT";
    } else if (op == NOT_CHANGE) {
        return "NOT_CHANGE";
    }
    return "DECREMENT";
}

PFConfOP Stream::getPFConfOP(float accuracy, float lateness, float pollution, float miss)
{
    PFParaState accuracy_state = PARA_LOW;
    PFParaState lateness_state = PARA_LOW;
    PFParaState poll_state     = PARA_LOW;
    PFParaState miss_state     = PARA_LOW;
    PFConfOP operate = NOT_CHANGE;
    if (accuracy > ACCURACY_HIGH) {
        accuracy_state = PARA_HIGH;
    } else if (accuracy > ACCURACY_LOW) {
        accuracy_state = PARA_MID;
    }
    if (lateness > THRE_LATENESS) {
        lateness_state = PARA_HIGH;
    }
    if (pollution > THRE_POLLUTION) {
        poll_state = PARA_HIGH;
    }
    if (miss > THRE_MISS) {
        miss_state = PARA_HIGH;
    }
    if ((accuracy_state == PARA_HIGH && lateness_state == PARA_HIGH) ||
        (accuracy_state == PARA_MID && lateness_state == PARA_HIGH && poll_state == PARA_LOW)) {
        operate = INCREMENT;
    }
    if ((accuracy_state == PARA_HIGH && lateness_state == PARA_LOW && poll_state == PARA_HIGH) ||
        (accuracy_state == PARA_MID && poll_state == PARA_HIGH) || 
        (accuracy_state == PARA_LOW && lateness_state == PARA_HIGH) ||
        (accuracy_state == PARA_LOW && lateness_state == PARA_LOW && poll_state == PARA_HIGH)) {
        operate = DECREMENT;
    }
    if (prefetcher->configs->fdp_stream_miss_enable && miss_state == PARA_HIGH) {
        if (operate == NOT_CHANGE && confLevel != VERY_AGGRESSIVE && confLevel != AGGRESSIVE) {
            operate = INCREMENT;
        }
        if (operate == DECREMENT) {
            operate = NOT_CHANGE;
        }
    }
    if (prefetcher->verbose) {
        LOG_INFO_M(Unit::LSU, Stage::NA) << "[Stream]: " << getStr(operate) << " prefetch conf.";
    }
    return operate;
}

void Stream::setPFParam()
{
    region_size = confArray[confLevel].region_size;
    region_mask = region_size - 1;
    region_off_n = region_size >> 6;
    pfl2_lev_mole = confArray[confLevel].pf_lev_mole;
    pfl2_lev_deno = confArray[confLevel].pf_lev_deno;
}

} // namespace JCore