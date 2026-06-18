#include "mtccore/lsu/prefetch/MtcStream.h"

#include <algorithm>

#include "../configs/mlsu_config.h"
#include "mtccore/lsu/MtcLoadStoreUnit.h"
#include "mtccore/lsu/prefetch/MtcPrefetcher.h"

namespace JCore {

void MtcStream::Reset(void)
{
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

void MtcStream::Build(void)
{
    for (uint32_t i = 0; i < l0_table_size; i++) {
        auto p = std::make_shared<L0Entry>();
        mru0_list.push_back(p);
    }

    for (uint32_t i = 0; i < l1_table_size; i++) {
        auto p = std::make_shared<MTC_L1Entry>();
        mru1_list.push_back(p);
    }
    auto &cfg = prefetcher->configs;
    ACCURACY_HIGH = float(cfg->pref_stream_accuracy_high) / cfg->pref_stream_propor_base;
    ACCURACY_LOW = float(cfg->pref_stream_accuracy_low) / cfg->pref_stream_propor_base;
    THRE_LATENESS = float(cfg->pref_stream_thre_lateness) / cfg->pref_stream_propor_base;
    THRE_POLLUTION = float(cfg->pref_stream_thre_poll) / cfg->pref_stream_propor_base;
    THRE_MISS = float(cfg->pref_stream_thre_miss) / cfg->pref_stream_propor_base;
    confLevel = MTCVERY_MTCCONSERVATIVE;
    confArray.resize(MTCLVL_NUM);
    // distance = region_size * prf_depth * pf_lev_mole / pf_lev_deno
    confArray[MTCVERY_MTCCONSERVATIVE].level = MTCVERY_MTCCONSERVATIVE;
    confArray[MTCVERY_MTCCONSERVATIVE].region_size  = 512;
    confArray[MTCVERY_MTCCONSERVATIVE].pf_lev_mole = 1;
    confArray[MTCVERY_MTCCONSERVATIVE].pf_lev_deno = 1;
    confArray[MTCCONSERVATIVE].level = MTCCONSERVATIVE;
    confArray[MTCCONSERVATIVE].region_size = 512;
    confArray[MTCCONSERVATIVE].pf_lev_mole = 2;
    confArray[MTCCONSERVATIVE].pf_lev_deno = 1;
    confArray[MTCMIDDLE1].level = MTCMIDDLE1;
    confArray[MTCMIDDLE1].region_size = 512;
    confArray[MTCMIDDLE1].pf_lev_mole = 4;
    confArray[MTCMIDDLE1].pf_lev_deno = 1;
    confArray[MTCMIDDLE2].level = MTCMIDDLE2;
    confArray[MTCMIDDLE2].region_size = 1024;
    confArray[MTCMIDDLE2].pf_lev_mole = 4;
    confArray[MTCMIDDLE2].pf_lev_deno = 1;
    confArray[MTCMIDDLE3].level = MTCMIDDLE3;
    confArray[MTCMIDDLE3].region_size = 1024;
    confArray[MTCMIDDLE3].pf_lev_mole = 8;
    confArray[MTCMIDDLE3].pf_lev_deno = 1;
    confArray[MTCAGGRESSIVE].level = MTCAGGRESSIVE;
    confArray[MTCAGGRESSIVE].region_size = 1024;
    confArray[MTCAGGRESSIVE].pf_lev_mole = 16;
    confArray[MTCAGGRESSIVE].pf_lev_deno = 1;
    confArray[MTCVERY_MTCAGGRESSIVE].level = MTCVERY_MTCAGGRESSIVE;
    confArray[MTCVERY_MTCAGGRESSIVE].region_size = 2048;
    confArray[MTCVERY_MTCAGGRESSIVE].pf_lev_mole = 16;
    confArray[MTCVERY_MTCAGGRESSIVE].pf_lev_deno = 1;
    setPFParam();
}

void MtcStream::train(MemReqBus &req)
{
    if (stream_disable) return;

    if (filter_line(req.addr)) {
        return;
    }
    if (prefetcher->verbose) {
        printf("[Stream]: %s. tpc 0x%lx, addr 0x%lx\n", __func__, req.tpc, req.addr);
        LOG_DEBUG(prefetcher->top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::LOW,
            "[Stream]: %s. tpc 0x%lx, addr 0x%lx", __func__, req.tpc, req.addr);
    }

    uint64_t align_addr = req.addr & ~0x3f;
    bool hit;

    hit = train_l1(align_addr);
    if (!hit) {
        train_l0(align_addr);
    }
}

bool MtcStream::filter_line(uint64_t addr)
{
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

void MtcStream::outputq(uint32_t region_off_n, uint64_t pfl1_base, uint64_t pfl2_base)
{
    for (uint32_t i = 0; i < region_off_n; i++) {
        uint64_t pfl1_addr = pfl1_base + (i << 6);
        uint64_t pfl2_addr = pfl2_base + (i << 6);
        pfl1_q->push_back(pfl1_addr);
        pfl2_q->push_back(pfl2_addr);
        pfl1_count_n++;
        if (prefetcher->verbose) {
            printf("[Stream]: PFL1 0x%lx\n", pfl1_addr);
            LOG_DEBUG(prefetcher->top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX,
                LogLevel::LOW, "[Stream]: PFL1 0x%lx", pfl1_addr);
        }
    }
}

bool MtcStream::train_l1(uint64_t addr)
{
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
                printf("%s: region 0x%lx, aaccelss_cnt %u\n", __func__, e->region_addr, e->aaccelss_cnt);
                LOG_DEBUG(prefetcher->top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::LOW,
                    "%s: region 0x%lx, aaccelss_cnt %u\n", __func__, e->region_addr, e->aaccelss_cnt);
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
                outputq(region_off_n, pfl1_base, pfl2_base);
            }
        } else if ((region_m1(e->region_addr) == tri_rgn) && (e->aaccelss_cnt > l1_trig_thr)) {
            uint64_t pfl1_base = tri_rgn - region_size * pfl1_depth;
            uint64_t pfl2_base = tri_rgn - region_size * pfl2_depth;
            if (prefetcher->verbose) {
                printf("%s: tri_rgn 0x%lx, pfl1_base 0x%lx\n", __func__, tri_rgn, pfl1_base);
                LOG_DEBUG(prefetcher->top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::LOW,
                    "%s: tri_rgn 0x%lx, pfl1_base 0x%lx", __func__, tri_rgn, pfl1_base);
            }
            outputq(region_off_n, pfl1_base, pfl2_base);
        }

        if ((region_m1(e->region_addr) == tri_rgn) || (region_p1(e->region_addr) == tri_rgn)) {
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

void MtcStream::train_l0(uint64_t addr)
{
    uint64_t tri_rgn = to_region_addr(addr);
    bool hit = false;
    for (auto it = mru0_list.begin(); it != mru0_list.end(); it++) {
        auto e = *it;
        if (e->region_addr != tri_rgn) continue;
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

    if (!hit) {
        insert_l0(addr);
    }
}

uint64_t MtcStream::to_region_addr(uint64_t addr)
{
    return (addr & ~region_mask);
}

uint32_t MtcStream::to_region_off(uint64_t addr)
{
    return ((addr & region_mask) >> 6);
}

uint64_t MtcStream::region_p1(uint64_t addr)
{
    return (addr + region_size);
}

uint64_t MtcStream::region_m1(uint64_t addr)
{
    return (addr - region_size);
}

void MtcStream::insert_l0(uint64_t addr)
{
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

void MtcStream::insert_l1(PtrL0Entry l0_e)
{
    PtrMTC_L1Entry e = mru1_list.back();
    e->aaccelss_cnt  = l0_e->aaccelss_cnt;
    e->bits        = l0_e->bits;
    e->region_addr = l0_e->region_addr;
    e->conf        = 0;
    mru1_list.pop_back();
    mru1_list.push_front(e);
    if (prefetcher->verbose) {
        printf("%s: region_addr 0x%lx\n", __func__, l0_e->region_addr);
        LOG_DEBUG(prefetcher->top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::LOW,
            "%s: region_addr 0x%lx", __func__, l0_e->region_addr);
    }
}

void MtcStream::Work(void)
{
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
                printf("[Stream]:increase prefetch level to %d\n", pref_level);
                LOG_DEBUG(prefetcher->top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::LOW,
                          "[Stream]: increase prefetch level to %d", pref_level);
            } else if (prefetcher->verbose) {
                printf("[Stream]: switch to pref_bad\n");
                LOG_DEBUG(prefetcher->top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::LOW,
                          "[Stream]: switch to pref_bad");
            }
        }
    }
    if (prefetcher->verbose) {
        printf("[Stream]: cycle: %ld, region_size:%ld, pfl1_dep:%d, pfl2_dep:%d, pref_lvl:%d",
            prefetcher->GetSim()->getCycles(), region_size, pfl1_depth, pfl2_depth, pref_level);
        LOG_DEBUG(prefetcher->top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::LOW,
            "[Stream]: cycle: %ld, region_size:%ld, pfl1_dep:%d, pfl2_dep:%d, pref_lvl:%d",
            prefetcher->GetSim()->getCycles(), region_size, pfl1_depth, pfl2_depth, pref_level);
    }
}

void MtcStream::Xfer(void)
{
}

void MtcStream::feedback(PtrPrefFB fb)
{
    if (fb->bad) {
        pfl1_bad_count_n++;
    }
    if (prefetcher->verbose) {
        printf("[Stream]: bad 0x%lx\n", fb->addr);
        printf("%s: pfl1 bad/total = %lu/%lu\n", __func__, pfl1_bad_count, pfl1_count);
        LOG_DEBUG(prefetcher->top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::LOW,
            "[Stream]: bad 0x%lx. %s: pfl1 bad/total = %lu/%lu", fb->addr, __func__, pfl1_bad_count, pfl1_count);
    }
}

void MtcStream::L2Feedback(PtrPrefFB fb)
{
    if (!fb->vld || !fb->isFromL2) {
        return;
    }
    float accuracy = mtccalRate(fb->total_pref_useful, fb->total_pref_sent_to_next, fb->interval_pref_useful,
        fb->interval_pref_sent_to_next);
    float lateness = mtccalRate(fb->total_pref_late, fb->total_pref_useful, fb->interval_pref_late,
        fb->interval_pref_useful);
    float pollution = mtccalRate(fb->total_dmd_miss_due_pref, fb->total_dmd_miss, fb->interval_dmd_miss_due_pref,
        fb->interval_dmd_miss);
    float miss = mtccalRate(fb->total_dmd_miss, fb->total_data_req, fb->interval_dmd_miss, fb->interval_data_req);
    if (prefetcher->verbose) {
        printf("[Stream]: L2 Feedback. The accuracy %.2f, lateness %.2f, pollution_due_pref %.2f, miss_rate%.2f\n",
            accuracy, lateness, pollution, miss);
        LOG_DEBUG(prefetcher->top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::LOW,
                  "[Stream]: L2 Feedback. The accuracy %.2f, lateness %.2f, pollution_due_pref %.2f, miss_rate%.2f",
                  accuracy, lateness, pollution, miss);
    }
    if (!prefetcher->configs->fdp_stream_enable) {
        return;
    }
    MTCPFConfOP op = getPFConfOP(accuracy, lateness, pollution, miss);
    if (op == MTCINCREMENT) {
        confLevel = incPFLevel(confLevel);
        prefetcher->top->stats->fdp_stream_inc++;
        // prefetcher->top->lsu.at(3).stats->fdp_stream_inc++;
        setPFParam();
        if (prefetcher->verbose) {
            printf("[Stream]: %s prefetch conf. region_size 0x%lx\n", getStr(op).c_str(), region_size);
            LOG_DEBUG(prefetcher->top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::LOW,
                      "[Stream]: %s prefetch conf. region_size 0x%lx", getStr(op).c_str(), region_size);
        }
    } else if (op == MTCDECREMENT) {
        confLevel = decPFLevel(confLevel);
        prefetcher->top->stats->fdp_stream_dec++;
        // prefetcher->top->lsu.at(3).stats->fdp_stream_dec++;
        setPFParam();
        if (prefetcher->verbose) {
            printf("[Stream]: %s prefetch conf. region_size 0x%lx\n", getStr(op).c_str(), region_size);
            LOG_DEBUG(prefetcher->top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::LOW,
                      "[Stream]: %s prefetch conf. region_size 0x%lx", getStr(op).c_str(), region_size);
        }
    }
}

std::string MtcStream::getStr(MTCPFConfOP op)
{
    if (op == MTCINCREMENT) {
        return "MTCINCREMENT";
    } else if (op == MTCNOT_CHANGE) {
        return "MTCNOT_CHANGE";
    }
    return "MTCDECREMENT";
}

MTCPFConfOP MtcStream::getPFConfOP(float accuracy, float lateness, float pollution, float miss)
{
    MTCPFParaState accuracy_state = MTCPARA_LOW;
    MTCPFParaState lateness_state = MTCPARA_LOW;
    MTCPFParaState poll_state     = MTCPARA_LOW;
    MTCPFParaState miss_state     = MTCPARA_LOW;
    MTCPFConfOP operate = MTCNOT_CHANGE;
    if (accuracy > ACCURACY_HIGH) {
        accuracy_state = MTCPARA_HIGH;
    } else if (accuracy > ACCURACY_LOW) {
        accuracy_state = MTCPARA_MID;
    }
    if (lateness > THRE_LATENESS) {
        lateness_state = MTCPARA_HIGH;
    }
    if (pollution > THRE_POLLUTION) {
        poll_state = MTCPARA_HIGH;
    }
    if (miss > THRE_MISS) {
        miss_state = MTCPARA_HIGH;
    }
    if ((accuracy_state == MTCPARA_HIGH && lateness_state == MTCPARA_HIGH) ||
        (accuracy_state == MTCPARA_MID && lateness_state == MTCPARA_HIGH && poll_state == MTCPARA_LOW)) {
        operate = MTCINCREMENT;
    }
    if ((accuracy_state == MTCPARA_HIGH && lateness_state == MTCPARA_LOW && poll_state == MTCPARA_HIGH) ||
        (accuracy_state == MTCPARA_MID && poll_state == MTCPARA_HIGH) ||
        (accuracy_state == MTCPARA_LOW && lateness_state == MTCPARA_HIGH) ||
        (accuracy_state == MTCPARA_LOW && lateness_state == MTCPARA_LOW && poll_state == MTCPARA_HIGH)) {
        operate = MTCDECREMENT;
    }
    if (prefetcher->configs->fdp_stream_miss_enable && miss_state == MTCPARA_HIGH) {
        if (operate == MTCNOT_CHANGE && confLevel != MTCVERY_MTCAGGRESSIVE && confLevel != MTCAGGRESSIVE) {
            operate = MTCINCREMENT;
        }
        if (operate == MTCDECREMENT) {
            operate = MTCNOT_CHANGE;
        }
    }
    if (prefetcher->verbose) {
        printf("[Stream]: %s prefetch conf.\n", getStr(operate).c_str());
        LOG_DEBUG(prefetcher->top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::LOW,
                  "[Stream]: %s prefetch conf.", getStr(operate).c_str());
    }
    return operate;
}

void MtcStream::setPFParam()
{
    region_size = confArray[confLevel].region_size;
    region_mask = region_size - 1;
    region_off_n = region_size >> 6;
    pfl2_lev_mole = confArray[confLevel].pf_lev_mole;
    pfl2_lev_deno = confArray[confLevel].pf_lev_deno;
}

} // namespace JCore