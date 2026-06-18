#include "pe/ifu/iside/ifu_icache.h"

#include "core/Core.h"
#include "pe/ifu/common/ifu_stats.h"
#include "pe/ifu/common/ifu_utils.h"
#include "pe/ifu/iside/pe_ifu.h"

namespace JCore {


using namespace std;

namespace PE_IFU {

IFUICache::IFUICache() {}

IFUICache::~IFUICache()
{
    for (uint32_t i = 0; i < nset; i++) {
        if (cache[i] != nullptr) {
            delete[] cache[i];
        }
    }
    if (cache != nullptr) {
        delete[] cache;
    }
}

void IFUICache::Build()
{
    verbose = configs->verbose;
    nset = configs->icache_depth;
    nway = configs->icache_width;
    way_size = configs->icache_way_size;

    cache = new L1Entry* [nset];
    for (uint32_t i = 0; i < nset; i++) {
        cache[i] = new L1Entry[nway];
    }
    stallPtr = nullptr;
    logLabel = "[L1I" + std::to_string(id);
}

void IFUICache::Reset()
{
    for (uint32_t i = 0; i < nset; i++) {
        for (uint32_t j = 0; j < nway; j++) {
            cache[i][j].state = CS_INV;
        }
    }
    stats->Reset();
}

void IFUICache::Work() {}

void IFUICache::Xfer() {}

SimSys *IFUICache::GetSim()
{
    return sim;
}

bool IFUICache::GetDataStall()
{
    if (configs->icache_perfect) {
        ASSERT(missQ.size() == 0);
        return false;
    }

    ASSERT(missQ.size() <= configs->icache_nostd);
    return stallPtr || (missQ.size() >= configs->icache_nostd);
}

bool IFUICache::LookupStall()
{
    return false;
}

bool IFUICache::CanSendReq()
{
    if (configs->icache_perfect) {
        return true;
    }

    return missQ.size() < configs->icache_nostd;
}

bool IFUICache::CanSendPref()
{
    if (configs->icache_perfect) {
        return true;
    }

    return missQ.size() < configs->icache_pf_nostd;
}

bool IFUICache::lookupTag(uint64_t virAddr)
{
    if (configs->icache_perfect) {
        return true;
    }

    stats->l1_aaccelss++;
    uint32_t setID = calcSetIdx(virAddr);
    bool hit = false;
    for (uint32_t w = 0; w<nway; w++) {
        L1Entry *entry = &cache[setID][w];
        if (tagMatch(entry->addr, virAddr) && entry->state!=CS_INV) {
            hit = true;
            updateLRU(setID, w);
            break;
        }
    }
    if (!hit) {
        stats->l1_miss++;
    }
    return hit;
}

// void instr_intercept()

void IFUICache::getCacheData(FetchReqPtr const& fetchReq, uint32_t fetchWidth)
{
    const uint16_t DATA_ENTRY_WIDTH = 8;
    uint512_t dataPerfect;
    if (configs->icache_perfect) {
        uint16_t fetch_cnt = ifu->configs.ifu_fetch_width / WIDTH_64;
        uint32_t startTpc = utils->calcAddr(fetchReq->tpc);
        for (uint32_t i = 0; i < fetch_cnt; ++i) {
            dataPerfect.bits[i] = GetSim()->fetchData(startTpc + i * WIDTH_64, WIDTH_64);
        }
    }

    L1Entry *entry = find(fetchReq->tpc);
    uint32_t offset = fetchReq->tpc & (way_size - 1);
    uint16_t fetch_cnt = ifu->configs.ifu_fetch_width / WIDTH_16;
    uint32_t totalWidth = 0;
    uint32_t curWidth = 0;
    uint32_t maxIdx = 8;
    uint64_t bin = 0;
    const uint32_t num = 8;
    const uint32_t maxWidthSize = ifu->configs.ifu_fetch_width;
    const uint32_t val64 = 64;
    auto getbin = [&fetchReq, &entry, &dataPerfect, DATA_ENTRY_WIDTH, maxIdx, this] (uint32_t oft, uint32_t iSize,
        uint32_t d_idx, uint32_t num)
        -> uint64_t {
        uint64_t *dataArray = NULL;
        if (!this->configs->icache_perfect) {
            if (entry == nullptr) {
                cout << "TPC: 0x" << hex << fetchReq->tpc << " can not found in icache" << endl;
                ASSERT(entry != nullptr && "No data found based on this TPC!");
            }
            dataArray = entry->data.bits;
        } else {
            dataArray = dataPerfect.bits;
        }

        if ((DATA_ENTRY_WIDTH - oft) < iSize && d_idx < maxIdx) {
            return (dataArray[d_idx - 1] >> (oft * num)) |
                    (dataArray[d_idx] << (val64 - oft * num));
        } else {
            return dataArray[d_idx - 1] >> (oft * num);
        }
    };

    /* Hint:
     * Calculate the position of each instruction in the data item and obtain it.
    */
    for (uint32_t j = 0; j < fetch_cnt; j++) {
        // the index of the jth instruction in the entry
        uint32_t d_idx = (offset + curWidth) / DATA_ENTRY_WIDTH + 1;
        if (d_idx > maxIdx) {
            break;
        }
        // the offset of the jth instruction in the data
        uint32_t oft = (offset + curWidth) % DATA_ENTRY_WIDTH;
        ASSERT(j < fetchReq->tpcArr.size());
        fetchReq->tpcArr[j] = fetchReq->tpc + curWidth;
        // Last inst need to merge.
        if (fetchReq->merge) {
            uint32_t movBit = fetchReq->mergeSize * num;
            fetchReq->tpcArr[j] -= fetchReq->mergeSize;
            fetchReq->isize[j] = CheckMInstSize(fetchReq->mergeBin);
            bin = getbin(oft, fetchReq->isize[j] - fetchReq->mergeSize, d_idx, num);
            fetchReq->data[j] = (bin << movBit) | fetchReq->mergeBin;
        } else {
            bin = getbin(oft, WIDTH_16, d_idx, num);
            fetchReq->isize[j] = CheckMInstSize(bin);
            fetchReq->data[j] = getbin(oft, fetchReq->isize[j], d_idx, num);
        }

        // TODO: For debug, do not delete.
        // cout << "get cache data block " << fetchReq->bid << " Group " << fetchReq->gid << " fid: " << fetchReq->fid
        //     << " tid: " << fetchReq->m_threadId << " stid: " << fetchReq->stid;
        // if (fetchReq->merge) {
        //     cout << " from merge. mergeSize: " << dec << fetchReq->mergeSize
        //         << " mergeBin: 0x" << hex << fetchReq->mergeBin;
        // }
        // cout << " start TPC: 0x" << hex << fetchReq->tpc << " current tpc: 0x" << fetchReq->tpcArr[j]
        //     << " bin: 0x" << fetchReq->data[j]
        //     << " isize: " << dec << fetchReq->isize[j] << " curWidth: " << curWidth << " offset: " << offset
        //     << " inst count: " << fetchReq->instCnt;
        // if (way_size < offset + curWidth + fetchReq->isize[j] || curWidth + fetchReq->isize[j] > maxWidthSize) {
        //     uint32_t movBit = (fetchReq->isize[j] - fetchReq->mergeSize) * num;
        //     fetchReq->mergeBin = (fetchReq->data[fetchReq->instCnt] << movBit) >> movBit;
        //     cout << " need to merge.mergeSize: " << dec << min(way_size - offset - curWidth, maxWidthSize - curWidth)
        //         << " mergeBin: 0x" << hex << ((fetchReq->data[fetchReq->instCnt] << movBit) >> movBit);
        // }
        // cout << endl;

        if (way_size < offset + curWidth + fetchReq->isize[j] || curWidth + fetchReq->isize[j] > maxWidthSize) {
            fetchReq->merge = true;
            fetchReq->mergeSize = way_size - offset - curWidth;
            fetchReq->mergeSize = min(fetchReq->mergeSize, maxWidthSize - curWidth);
            uint32_t movBit = (fetchReq->isize[j] - fetchReq->mergeSize) * num;
            fetchReq->mergeBin = (fetchReq->data[fetchReq->instCnt] << movBit) >> movBit;
            break;
        }
        ++fetchReq->instCnt;
        totalWidth += fetchReq->isize[j];
        curWidth += fetchReq->isize[j];

        if (fetchReq->merge) {
            curWidth -= fetchReq->mergeSize;
            fetchReq->merge = false;
            fetchReq->mergeBin = 0;
            fetchReq->mergeSize = 0;
        }
    }
    fetchReq->mask = totalWidth;

    LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel << " ICache get data at 0x" << hex << fetchReq->tpc;
}

L1Entry IFUICache::refill(uint64_t addr, uint64_t *data)
{
    uint32_t setID = calcSetIdx(addr);
    addr = utils->calcAddr(addr);
    L1Entry r = cache[setID][nway-1];

    for (uint32_t i = nway-1; i > 0; i--) {
        cache[setID][i] = cache[setID][i-1];
    }
    L1Entry *entry = &cache[setID][0];
    entry->state = CS_EXCL;
    entry->addr = utils->calcAddr(addr);
    const uint32_t num = 8;
    for (uint32_t i = 0; i < way_size / num; i++) {
        entry->data.bits[i]= data[i];
    }
    stats->l1Refill++;
    LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel << " Recv ICache refill set " << dec << setID << " at 0x"
        << hex << entry->addr;
        if (r.state != CS_INV) {
            LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel << ", evict out at 0x" << hex << r.addr;
        }
        // cout << " data is: ";
        // for (uint32_t i = 0; i < way_size / num; i++) {
        //     cout << entry->data.bits[i] << " ";
        // }
    return r;
}

bool IFUICache::lookUpMissQ(uint64_t tpc)
{
    uint64_t tagAddr = utils->calcAddr(tpc);
    auto it = find_if(missQ.begin(), missQ.end(), [&tagAddr](MissQEntry& entry) {
        return entry.addr == tagAddr;
    });

    return (it!=missQ.end());
}

bool IFUICache::insertMissQ(uint64_t tpc, bool pref)
{
    if (configs->icache_perfect) {
        return true;
    }

    uint64_t tagAddr = utils->calcAddr(tpc);
    if (tagAddr == 0) {
        LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel << "target TPC is 0. lookup TPC is 0x" << hex << tpc;
        return false;
    }
    // TODO: missQ/miss buffer stall full and stall
    if (false) {
        stats->missBufferStall++;
    }
    missQ.emplace_back(tagAddr, pref, GetSim()->getCycles());
    return true;
}

void IFUICache::SendMissReq()
{
    for (auto &entry : missQ) {
        if (entry.sent) {
            continue;
        }
        entry.sent = true;
        stats->l2_aaccelss++;
        const uint32_t size = 64;

        if (configs->req_mode) {
            GFUMemReq req;
            req.vld = true;
            req.addr = entry.addr;
            req.tid = (id<<2) + 1; // top bit for ifuID, last 2-bit for mode
            req.size = size;
            req.is_store = false;
            req.is_llsc = false;
            req.llsc = 0;
            req.prefetch = false;
            GetSim()->core->iFetchReqQ.Write(req);
        } else {
            // send inst packet request
            PtrPacket pkt = Packet::CreateRWPkt(false, (id<<2) + 1);
            pkt->addr = entry.addr;
            pkt->id = ifu->memID;
            pkt->size = size;
            if (entry.isPref) {
                pkt->setPref();
            }
            ifu->sendInstPkt(pkt, false);
        }
        LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel << "Send Miss Requset at 0x" << hex << entry.addr;
        break;
    }
}

void IFUICache::releaseMissQ(uint64_t addr, bool pref)
{
    addr = utils->calcAddr(addr);
    auto it = find_if(missQ.begin(), missQ.end(), [&addr](MissQEntry& entry) {
        return  entry.addr == addr;
    });
    if (it != missQ.end()) {
        missQ.erase(it);
    }
    if (stallPtr && utils->calcAddr(stallPtr->tpc) == addr) {
        stallPtr = nullptr;
    }
}
}


} // namespace JCore
