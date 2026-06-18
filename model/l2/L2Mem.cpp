#include "l2/L2Mem.h"

#include "l2/L2Cache.h"

namespace JCore {

L2Entry* L2Mem::find(uint64_t addr, bool lru) {
    L2Entry *e;
    uint64_t tag = addr2tag(addr);
    int index = addr2idx(addr);
    for (uint64_t i = 0; i < config->cache_nway; i=i+1) {
        e = &cache[index][i];
        if (e->valid && e->tag == tag) {
            if (lru) {
                L2Entry tmp = *e;
                for (int j = i; j > 0; j--) {
                    cache[index][j] = cache[index][j-1];
                }
                cache[index][0] = tmp;
                e = &cache[index][0];
            }
            return e;
        }
    }
    return nullptr;
}

bool L2Mem::writeback(MissQEntry *e) {
    L2Entry *l2e;

    l2e = find(e->addr, true);
    if (!l2e) {
        return false;
    }
    ASSERT(l2e);
    if (e->dirty) {
        l2e->dirty = true;
        l2e->data = e->data;
    }

    // If e->update is true, only data updating is required
    if (!e->update) {
        // Update coherence
        if (!(config->l1i_l2_inclusion_policy == "WI" && e->inst) &&
            !(config->l1d_l2_inclusion_policy == "WI" && !e->inst)) {
            ASSERT(l2e->hold_set.count(e->src) == 1);
            l2e->hold_set.erase(e->src);
        }

    }

    if (verbose) {
        info("%s. addr = 0x%lx, dirty = %d, data = %s, share %d, hold_set = ", __func__, e->addr, l2e->dirty, l2e->data.toStr(), l2e->share);
        for (auto i : l2e->hold_set) {
            printf("%d ", i);
        }
        printf("\n");
    }
    if (verbose) {
        std::string info;
        for (auto i : l2e->hold_set) {
            info += std::to_string(i) + " ";
        }
        LOG_DEBUG(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                  "[L2] L2Mem. %s. addr = 0x%lx, dirty = %d, data = %s, share %d, hold_set = %s",
                  __func__, e->addr, l2e->dirty, l2e->data.toStr(), l2e->share, info.c_str());
    }
    return true;
}

bool L2Mem::lookup(MissQEntry *e) {
    bool hit = false;
    L2Entry *l2e = find(e->addr, true);
    if (l2e) {
        e->data  = l2e->data;
        e->dirty = false;
        hit = true;
        bool share = l2e->share;

        if (!e->pref) {
            if (!l2e->demand) {
                e->req_pkt->l2_pref_useful = true;
            }
            l2e->demand = true;
        }

        // If no change to L2, dup is true
        if (e->write || e->upgrade) {
            if (!l2e->share && (l2e->hold_set.count(e->src) != 0)) {
                e->dup = true;
            } else {
                e->dup = false;
            }
        } else {
            // read
            if (l2e->hold_set.count(e->src) != 0) {
                // no matter share or exclusive
                e->dup = true;
            } else {
                e->dup = false;
            }
        }

        // Return if this is duplicate request
        if (e->dup) return hit;

        // No update for prefetch request
        if (e->pref) return hit;

        // Update coherence
        if (e->write || e->upgrade) {
            // write miss or upgrade
            e->dst_set = l2e->hold_set;
            if (e->upgrade && e->dst_set.count(e->src) != 0) {
                e->dst_set.erase(e->src);
            }
            share = false;
            l2e->hold_set.clear();
        } else {
            // read miss
            if (l2e->hold_set.size() == 0) {
                if ((config->l1i_l2_inclusion_policy != "WI" && e->inst) &&
                    (config->l1d_l2_inclusion_policy != "WI" && !e->inst)) {
                    share = true;
                } else {
                    share = false;
                }
            } else if (!l2e->share && (l2e->hold_set.count(e->src) == 0)) {
                share = true;
                e->dst_set = l2e->hold_set;
            }
        }
        if (!(config->l1i_l2_inclusion_policy == "WI" && e->inst) &&
            !(config->l1d_l2_inclusion_policy == "WI" && !e->inst)) {
            l2e->hold_set.insert(e->src);   // Dup should be removed
        }
        e->excl    = !share;
        l2e->share = share;
    }

    if (verbose) {
        info("%s. addr = 0x%lx, hit = %d, dup = %d, data = %s, share %d, hold_set = ", __func__, e->addr, hit, e->dup,
                hit ? e->data.toStr() : "empty", l2e ? l2e->share : 0);
        if (l2e) {
            for (auto i : l2e->hold_set) {
                printf("%d ", i);
            }
        }
        printf("\n");
    }
    if (verbose) {
        std::string info;
        if (l2e) {
            for (auto i : l2e->hold_set) {
                info += std::to_string(i) + " ";
            }
        }
        LOG_DEBUG(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                  "[L2] L2Mem. %s. addr = 0x%lx, hit = %d, dup = %d, data = %s, share %d, hold_set = ",
                  __func__, e->addr, hit, e->dup, hit ? e->data.toStr() : "empty", l2e ? l2e->share : 0, info.c_str());
    }
    return hit;
}

void L2Mem::replace(MissQEntry *e, L2Entry *out) {
    L2Entry *ptgt;
    uint64_t index = addr2idx(e->addr);

    if (verbose) {
        info("%s. addr 0x%lx\n", __func__, e->addr);
    }
    if (verbose) {
        LOG_DEBUG(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                  "[L2] L2Mem. %s. addr 0x%lx", __func__, e->addr);
    }

    // Choose victim in LRU
    *out = cache[index][config->cache_nway-1];
    for (uint64_t i = config->cache_nway-1; i>0; i--) {
        cache[index][i] = cache[index][i-1];
    }

    // MRU position
    ptgt = &cache[index][0];
    ptgt->valid = true;
    ptgt->dirty = false;
    ptgt->data = e->data;
    ptgt->tag = addr2tag(e->addr);
    ptgt->addr = e->addr;
    ptgt->prefetch = e->req_pkt->isPref();
    ptgt->demand   = !ptgt->prefetch;
    if (ptgt->prefetch) {
        ptgt->pref_type = e->req_pkt->user_type;
    }

    // Update coherence
    ptgt->hold_set.clear();
    bool share = false;
    if (!e->pref && (!(config->l1i_l2_inclusion_policy == "WI" && e->inst) &&
                    !(config->l1d_l2_inclusion_policy == "WI" && !e->inst))) {
        if (e->inst) {
            share = true;
        }
        ptgt->hold_set.insert(e->src);
        e->excl = !share;
    }
    ptgt->share = share;
}

void L2Mem::freeSnoopFilter(uint64_t addr, uint32_t src) {
    L2Entry *l2e = find(addr, true);
    if (!l2e) {
        return;
    }
    if (l2e->hold_set.count(src) == 1) {
        l2e->hold_set.erase(src);
        if (verbose) {
            info("%s. addr = 0x%lx, src = %u \n", __func__, addr, src);
        }
        if (verbose) {
            LOG_DEBUG(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                      "[L2] L2Mem. %s. addr = 0x%lx, src = %u", __func__, addr, src);
        }
    } else {
        ASSERT(0 && "Error L2 snoop filter");
    }
}

void L2Mem::Work() {

}

void L2Mem::Reset() {
    for (uint64_t i = 0; i < config->cache_nset; i++) {
        for (uint64_t j = 0; j < config->cache_nway; j++) {
            cache[i][j].valid = false;
        }
    }
}

void L2Mem::Xfer() {

}

SimSys* L2Mem::GetSim() {
    return nullptr;
}

void L2Mem::Build() {
    verbose = config->verbose;
    cache = new L2Entry*[config->cache_nset];
    for (uint64_t i = 0; i < config->cache_nset; i++) {
        cache[i] = new L2Entry[config->cache_nway];
    }
}

L2Mem::~L2Mem() {
    for (uint64_t i = 0; i < config->cache_nset; i++) {
        delete[] cache[i];
    }
    delete[] cache;
}

void L2Mem::dumpIndex(int index) {
    printf("L2 Line %d:\n", index);
    for (uint64_t i = 0; i < config->cache_nway; i++) {
        printf("    [%lu]. addr 0x%lx = %s\n", i, tag2addr(cache[index][i].tag), cache[index][i].data.toStr());
    }
}

} // namespace JCore
