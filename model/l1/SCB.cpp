#include "l1/SCB.h"

#include "core/Core.h"
#include "core/Packet.h"

namespace JCore {
const uint32_t g_maxQueueSize = 1024;

void SCBuffer::Work(void)
{
    picked_lookup = nullptr;
    if (!resp_list.empty() && dcache_allow) {
        picked_lookup = resp_list.front();
        resp_list.pop_front();
        stats_tick();
        return;
    }

    if (!lookup_list.empty() && dcache_allow) {
        picked_lookup = lookup_list.front();
        lookup_list.pop_front();
    }

    stats_tick();
}

void SCBuffer::Xfer(void)
{
    // insert + lookup, insert first

    uint32_t n = 0;
    while (n < n_store_in && !insert_q.empty() && comb_map.size() + 1 < g_maxQueueSize) {
        cur_req = insert_q.front();
        insert_q.pop_front();
        handleInsert();
        n++;
    }

    if (picked_lookup && l2_data_allow) {
        handleLookup();
    }
    if (!mem_resp_q.Empty()) {
        handleMemResp();
    }

    if (full()) {
        handleFull();
        for (uint32_t i = 0; i < top->GetSim()->core->configs.scalar_smt_thread; ++i) {
            ROBID oldestBID = top->GetSim()->core->bctrl->blockROB.getOldestBlockID(i);
            BlockCommandPtr cmd = top->GetSim()->core->bctrl->blockROB.GetBlockCMDPtr(oldestBID, i);
            if (top->top->top->typeId == LSUType::VECTOR_LSU &&
                (cmd->blockType == BlockType::BLK_TYPE_VPAR || cmd->blockType == BlockType::BLK_TYPE_VSEQ)) {
                LOG_INFO_M(Unit::LSU, Stage::NA) << ": Because SCB handle to write. Reset the idle cycle.";
                GetSim()->ResetWaitCycle();
            }
        }
    }

    mem_resp_q.Work();
}

void SCBuffer::Reset(void)
{
    mem_resp_q.Reset();

    for (uint32_t i = 0; i < N_SCB; i++) {
        if (array[i].state != S_EMPTY) {
            free_entry(&array[i]);
        }
    }
}

SimSys* SCBuffer::GetSim(void)
{
    return sim;
}

bool SCBuffer::full(void)
{
    return free_list.size() < n_store_in && insert_q.size() < g_maxQueueSize;
}

void SCBuffer::insert(uint64_t addr, int size, uint64_t data)
{
    ASSERT(size > 0);
    InsertReq req = {
        .addr = addr,
        .size = size,
        .data = data
    };

    insert_q.push_back(req);
}

bool SCBuffer::lookup(uint64_t addr, uint8_t **d_array, bool **v_array)
{
    uint64_t blk_addr = addr & ~0x3f;
    bool r = false;
    if (comb_map.count(blk_addr) != 0) {
        r = true;
        SCBEntry *e = comb_map[blk_addr];
        *d_array = e->data;
        *v_array = e->valid;
    }
    return r;
}

void SCBuffer::setMemResp(PtrPacket pkt)
{
    ASSERT(pkt->tid < N_SCB);
    mem_resp_q.Write(pkt->tid);
}

void SCBuffer::free_entry(SCBEntry *e)
{
    e->state = S_EMPTY;
    free_list.push_back(e);
    for (int i = 0; i < 64; i++) {
        e->valid[i] = false;
        e->full = false;
    }
}

SCBuffer::SCBuffer() {}

SCBuffer::~SCBuffer()
{
    delete[] array;
}

// TODO: handle unalign case
void SCBuffer::handleInsert(void)
{
    ASSERT(!free_list.empty());
    // Check whether hit
    uint64_t blk_addr = cur_req.addr & ~0x3f;
    SCBEntry *e;
    bool is_new = false;
    if (comb_map.count(blk_addr) == 0) {
        // New entry
        e = free_list.front();
        free_list.pop_front();
        ASSERT(e->state == S_EMPTY);
        e->addr = blk_addr;
        e->state = S_VALID;
        comb_map.emplace(blk_addr, e);
        is_new = true;
    } else {
        // Exist
        e = comb_map[blk_addr];
    }
    // Update data
    uint64_t addr = cur_req.addr;
    int      size = cur_req.size;
    uint64_t data = cur_req.data;
    int off = addr & 0x3f;
    uint8_t *ptr = (uint8_t*)&data;
    for (int i = 0; i < size; i++) {
        // dz size = 64Byte
        e->data [off+i] = ptr[i % 8];
        e->valid[off+i] = true;
    }
    sendLUwakeup(blk_addr, e->valid);
    checkEntryFull(e);
    LOG_INFO_M(Unit::LSU, Stage::NA) << "[SCB" << scbID << "]: insert entry. blk_addr 0x" << std::hex << blk_addr
        << ", addr 0x" << addr << ", size " << std::dec << size << ", data 0x" << std::hex << data << "."
        << (is_new ? ". New" : "Combine") << ", full " << e->full;
}

void SCBuffer::sendLUwakeup(uint64_t addr, bool *valid)
{
    MemReqBus bus = MemReqBus();
    bus.vld = true;
    bus.addr = addr;
    bus.tag = addr;
    for (uint32_t i = 0; i < 64; ++i) {
        bus.reqData.positionVld[i] = valid[i];
    }
    if (bus.stid == -1U) {
        for (auto &q : wakeup_scb_lu_q) {
            q->Write(bus);
        }
    } else {
        wakeup_scb_lu_q[bus.stid]->Write(bus);
    }
}

void SCBuffer::handleFull(void)
{
    bool has_wb = false;
    // full entry -> LOOKUP
    for (uint32_t i = 0; i < N_SCB; i++) {
        SCBEntry *e = &array[i];
        if (e->state == S_VALID) {
            if (e->full) {
                has_wb = true;
                e->state = S_LOOKUP;
                lookup_list.push_back(e);
                LOG_INFO_M(Unit::LSU, Stage::NA) << "[SCB" << scbID << "]: block 0x" << std::hex << e->addr
                    << " is ready to evict(full)";
            }
        }
    }

    if (has_wb)
        return;

    // Otherwise, evict random one
    uint32_t start = rand() % N_SCB;
    uint32_t i = start;
    do {
        SCBEntry *e = &array[i];
        if (e->state == S_VALID) {
            e->state = S_LOOKUP;
            lookup_list.push_back(e);
            LOG_INFO_M(Unit::LSU, Stage::NA) << "[SCB" << scbID << "]: block 0x" << std::hex << e->addr
                << " is ready to evict(not full)";
            return;
        }
        i = (i+1) % N_SCB;
    } while (i != start);

    LOG_INFO_M(Unit::LSU, Stage::NA) << "[SCB" << scbID << "]: stall(full)";
    // If has_wb is false, do nothing
}

void SCBuffer::handleLookup(void)
{
    SCBEntry *e = picked_lookup;
    uint64_t blk_addr = e->addr;
    bool valid, hit;
    valid = dcache->lookup(blk_addr, true, &hit);
    if (valid) {
        bool update = false;
        for (int i = 0; i < 64; i++) {
            if (e->valid[i]) {
                dcache->dataUpdate(blk_addr + i, e->data[i], 1);
                update = true;
            }
        }
        comb_map.erase(blk_addr);
        free_entry(e);

        if (update) {
            dcache->upgradeBroadcast(blk_addr);
        }
        LOG_INFO_M(Unit::LSU, Stage::NA) << "[SCB" << scbID << "]: Update block 0x" << std::hex << blk_addr
            << " in DCache";
    } else {
        e->state = S_MISS;
        sendMemReq(e, hit);
        LOG_INFO_M(Unit::LSU, Stage::NA) << "[SCB" << scbID << "]: block 0x" << std::hex << blk_addr
            << " is miss in DCache. upgrade = " << hit;
    }

    // PMU
    stats->scb_lookup_count++;
    if (!valid) {
        stats->scb_miss_count++;
    }
}

void SCBuffer::sendMemReq(SCBEntry *e, bool upgrade)
{
    PtrPacket pkt;
    if (upgrade) {
        pkt = Packet::createUpgrade(e->id);
    } else {
        pkt = Packet::CreateRWPkt(true, e->id);
    }
    pkt->addr = e->addr;
    pkt->size = 64;
    pkt->tid = (pkt->tid << 2) | 2;
    pkt->id = memID_s;
    pkt->l1_out_cycle = GetSim()->getCycles();
    if (pkt->stid == -1U) {
        for (auto &q : pkt_out_q) {
            q->Write(pkt);
        }
    } else {
        pkt_out_q[pkt->stid]->Write(pkt);
    }
    LOG_INFO_M(Unit::LSU, Stage::NA) << "[SCB" << scbID << "]: entry " << std::dec << e->id << " send packet " << *pkt;
}

void SCBuffer::handleMemResp(void)
{
    uint32_t id = mem_resp_q.Read();
    ASSERT(id >= 0 && id < N_SCB);
    SCBEntry *e = &array[id];
    ASSERT(e->state == S_MISS);
    e->state = S_LOOKUP;
    resp_list.push_back(e);
    LOG_INFO_M(Unit::LSU, Stage::NA) << "[SCB" << scbID << "]: block 0x" << std::hex << e->addr
        << " is set to S_LOOKUP";
}

void SCBuffer::checkEntryFull(SCBEntry *e)
{
    bool full = true;
    for (int i = 0; i < 64; i++) {
        full = full & e->valid[i];
        if (!full) break;
    }
    e->full = full;
}

void SCBuffer::Build(void)
{
    verbose = configs->verbose;
    N_SCB = configs->scb_depth;
    if (top->top->top->typeId == LSUType::SCALAR_LSU) {
        N_SCB = configs->scalar_scb_depth;
    }

    array = new SCBEntry[N_SCB];
    for (uint32_t i = 0; i < N_SCB; i++) {
        array[i].id = i;
        free_entry(&array[i]);
    }
    n_store_in = configs->cluster_width;
}

bool SCBuffer::hasDCacheReq(void)
{
    return !resp_list.empty() && !lookup_list.empty();
}

void SCBuffer::stats_tick()
{
    if (full()) {
        stats->scb_full_cycles++;
    }
}

} // namespace JCore