#include "mtccore/l1/MTC_SCBuffer.h"
#include "mtccore/l1/MTC_L1Top.h"
#include "mtccore/lsu/MtcLoadStoreUnit.h"
#include "core/Core.h"
#include "core/Packet.h"

namespace JCore {
const uint32_t g_maxQueueSize = 1024;
void MTC_SCBuffer::Work(void)
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


void MTC_SCBuffer::Xfer(void)
{
    uint32_t n = 0;
    while (n < n_store_in && !insert_q.empty() && comb_map.size() + 1 < g_maxQueueSize) {
        cur_req = insert_q.front();
        insert_q.pop_front();
        handleInsert();
        n++;
    }

    if (picked_lookup) {
        handleLookup();
    }

    if (full()) {
        handleFull();
        ROBID oldestBID = top->GetSim()->core->bctrl->blockROB.getOldestBlockID(0);
        BlockCommandPtr cmd = top->GetSim()->core->bctrl->blockROB.GetBlockCMDPtr(oldestBID, 0);
        if (((top->top->top->typeId  == LSUType::VECTOR_LSU) || (top->top->top->typeId  == LSUType::MEMORY_LSU)) &&
            IsBlockTypeParallel(cmd->blockType)) {
            if ((VERBOSE_ON && STAGE_ENABLED(StageID::LSU_ALL))) {
                cout << "[LSU]: Because SCB handle to write. Reset the idle cycle." << endl;
            }
            GetSim()->ResetWaitCycle();
        }
    }
}

void MTC_SCBuffer::Reset(void)
{
    mem_resp_q.Reset();

    for (uint32_t i = 0; i < N_SCB; i++) {
        if (array[i].state != MTC_S_EMPTY) {
            free_entry(&array[i]);
        }
    }
}

SimSys* MTC_SCBuffer::GetSim(void)
{
    return sim;
}

bool MTC_SCBuffer::full(void)
{
    return free_list.size() < n_store_in && insert_q.size() < g_maxQueueSize;
}

void MTC_SCBuffer::insert(uint64_t addr, int size, uint64_t data)
{
    ASSERT(size > 0);
    InsertReq req;
    req.addr = addr;
    req.size = size;
    memcpy(&req.data, &data, req.size);

    insert_q.push_back(req);
}

bool MTC_SCBuffer::lookup(uint64_t addr, uint8_t **d_array, bool **v_array)
{
    uint64_t blk_addr = addr & ~0xff;
    bool r = false;
    if (comb_map.count(blk_addr) != 0) {
        r = true;
        MTC_SCBEntry *e = comb_map[blk_addr];
        *d_array = e->data;
        *v_array = e->valid;
    }
    return r;
}

void MTC_SCBuffer::setMemResp(PtrPacket pkt)
{
    ASSERT(pkt->tid < N_SCB);
    mem_resp_q.Write(pkt->tid);
}

void MTC_SCBuffer::free_entry(MTC_SCBEntry *e)
{
    e->state = MTC_S_EMPTY;
    free_list.push_back(e);
    for (int i = 0; i < 256; i++) {
        e->valid[i] = false;
        e->full = false;
    }
}

MTC_SCBuffer::MTC_SCBuffer() {}

MTC_SCBuffer::~MTC_SCBuffer()
{
    delete[] array;
}

// TODO: handle unalign case
void MTC_SCBuffer::handleInsert(void)
{
    ASSERT(!free_list.empty());
    // Check whether hit
    uint64_t blk_addr = cur_req.addr & (uint64_t)~0xff;
    MTC_SCBEntry *e;
    bool is_new = false;
    if (comb_map.count(blk_addr) == 0) {
        // New entry
        e = free_list.front();
        free_list.pop_front();
        ASSERT(e->state == MTC_S_EMPTY);
        e->addr = blk_addr;
        e->state = MTC_S_VALID;
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
    int off = addr & 0xff;
    uint8_t *ptr = (uint8_t*)&data;
    for (int i = 0; i < size; i++) {
        // dz size = 64Byte
        e->data [off+i] = ptr[i % 8];
        e->valid[off+i] = true;
    }
    
    if (cur_req.opcode != Opcode::OP_TSD) {
        sendLUwakeup(blk_addr, e->valid);
    }
    
    checkEntryFull(e);
    if ((VERBOSE_ON && STAGE_ENABLED(StageID::LSU_ALL)) || this->configs->verbose) {
        printf("[SCB%d]: insert entry. blk_addr 0x%lx, addr 0x%lx, size %d, data 0x%lx. %s. full %d\n", scbID,
               blk_addr, addr, size, data, is_new ? "New" : "Combine", e->full);
    }
    if (DEBUG_VERBOSE_ON || lsuConfigs->verbose) {
        LOG_DEBUG(debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                  "[SCB%d]: insert entry. blk_addr 0x%lx, addr 0x%lx, size %d, data 0x%lx. %s. full %d",
                  scbID, blk_addr, addr, size, data, is_new ? "New" : "Combine", e->full);
    }
}

void MTC_SCBuffer::sendLUwakeup(uint64_t addr, bool *valid)
{
    MemReqBus bus = MemReqBus();
    bus.toMtcLsu = true;
    bus.vld = true;
    bus.addr = addr;
    bus.tag = addr;

    for (uint32_t j = 0; j < 256; ++j) {
        bus.mtc_reqData.positionVld[j] = valid[j];
    }
    wakeup_scb_lu_q->Write(bus);
}

void MTC_SCBuffer::handleFull(void)
{
    bool has_wb = false;
    // full entry -> LOOKUP
    for (uint32_t i = 0; i < N_SCB; i++) {
        MTC_SCBEntry *e = &array[i];
        if (e->state == MTC_S_VALID) {
            if (e->full) {
                has_wb = true;
                e->state = MTC_S_LOOKUP;
                lookup_list.push_back(e);
            }
            if ((VERBOSE_ON && STAGE_ENABLED(StageID::LSU_ALL)) || this->configs->verbose) {
                printf("[SCB%d]: block 0x%lx is ready to evict(full)\n", scbID, e->addr);
            }
        }
        if (e->state == MTC_S_VALID && e->full && (DEBUG_VERBOSE_ON || lsuConfigs->verbose)) {
            LOG_DEBUG(debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                      "[SCB%d]: block 0x%lx is ready to evict(full)", scbID, e->addr);
        }
    }

    if (has_wb)
        return;
    
    // Otherwise, evict random one
    uint32_t start = rand() % N_SCB;
    uint32_t i = start;
    do {
        MTC_SCBEntry *e = &array[i];
        if (e->state == MTC_S_VALID) {
            e->state = MTC_S_LOOKUP;
            lookup_list.push_back(e);
            if ((VERBOSE_ON && STAGE_ENABLED(StageID::LSU_ALL)) || this->configs->verbose) {
                printf("[SCB%d]: block 0x%lx is ready to evict(not full)\n", scbID, e->addr);
            }
            if (DEBUG_VERBOSE_ON || lsuConfigs->verbose) {
                LOG_DEBUG(debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                          "[SCB%d]: block 0x%lx is ready to evict(not full)", scbID, e->addr);
            }
            return;
        }
        i = (i+1) % N_SCB;
    } while (i != start);

    if ((VERBOSE_ON && STAGE_ENABLED(StageID::LSU_ALL)) || this->configs->verbose) {
        printf("[SCB%d]: stall(full)\n", scbID);
    }
    if (DEBUG_VERBOSE_ON || lsuConfigs->verbose) {
        LOG_DEBUG(debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                  "[SCB%d]: stall(full)", scbID);
    }
    // If has_wb is false, do nothing
}

void MTC_SCBuffer::handleLookup(void)
{
    MTC_SCBEntry *e = picked_lookup;
    sendMemReq(e);
    free_entry(e);
    stats->scb_lookup_count++;
}

void MTC_SCBuffer::sendMemReq(MTC_SCBEntry *e)
{
    PtrPacket pkt = Packet::createPkt(Packet::Writeback, e->id);
    pkt->addr = e->addr;
    pkt->size = 256;
    pkt->tid = (static_cast<int>(LSUType::MEMORY_LSU) << TID_TYPE_OFFSET) + (uint32_t)pkt->tid;
    pkt->id = memID_s;
    pkt->l1_out_cycle = GetSim()->getCycles();
    pkt_out_q->Write(pkt);
    pkt->size = configs->dcache_way_size;
    ASSERT(sizeof(e->data) >= 256);
    pkt->mtc_data.extractFrom(e->data, e->valid);
    for (uint32_t i = 0; i < 256; i++) {
        pkt->valid[i] = e->valid[i];
    }
    pkt_out_q->Write(pkt);
    if ((VERBOSE_ON && STAGE_ENABLED(StageID::LSU_ALL)) || this->configs->verbose) {
        printf("[SCB%d]: entry %d send packet ", scbID, e->id);
        std::cout << *pkt;
        printf("\n");
    }
    if (DEBUG_VERBOSE_ON || lsuConfigs->verbose) {
        LOG_DEBUG_STRUCT(debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                         *pkt, "[SCB%d]: entry %d send packet ", scbID, e->id);
    }
}

void MTC_SCBuffer::checkEntryFull(MTC_SCBEntry *e)
{
    bool full = true;
    for (int i = 0; i < 256; i++) {
        full = full & e->valid[i];
        if (!full) break;
    }
    e->full = full;
}

void MTC_SCBuffer::Build(void)
{
    verbose = configs->verbose;
    N_SCB = configs->scb_depth;
    if (top->top->top->typeId == LSUType::SCALAR_LSU) {
        N_SCB = configs->scalar_scb_depth;
    }

    array = new MTC_SCBEntry[N_SCB];
    for (uint32_t i = 0; i < N_SCB; i++) {
        array[i].id = i;
        free_entry(&array[i]);
    }
    n_store_in = configs->cluster_width;
}

bool MTC_SCBuffer::hasDCacheReq(void)
{
    return !resp_list.empty() && !lookup_list.empty();
}

void MTC_SCBuffer::stats_tick()
{
    if (full()) {
        stats->scb_full_cycles++;
    }
}

} // namespace JCore