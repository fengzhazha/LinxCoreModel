#include "mtccore/lsu/prefetch/MtcPrefetcher.h"

#include "mtccore/lsu/MtcLoadStoreUnit.h"
#include "SimSys.h"

namespace JCore {

void MtcPrefetcher::insert(MemReqBus &req)
{
    ASSERT(!configs->pref_enable); // not expect enable prefetch
    if (!configs->pref_enable) return;
    if (verbose) {
        printf("[debug-pref]: @%lu tpc 0x%lx, addr 0x%lx\n", sim->getCycles(), req.tpc, req.addr);
    }
    if (verbose) {
        LOG_DEBUG(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::LOW,
                  "[pref]: tpc 0x%lx, addr 0x%lx", req.tpc, req.addr);
    }
    if (configs->pref_stride_enable) {
        stride.train(req);
    }
    if (configs->pref_stream_enable) {
        stream.train(req);
    }
    pref_buf->insert_addr(req.addr);
}

void MtcPrefetcher::Reset(void)
{
    stride.Reset();
    stream.Reset();
}

void MtcPrefetcher::Work(void)
{
    while (!lu_pref_q->Empty()) {
        MemReqBus bus = lu_pref_q->Read();
        insert(bus);
    }

    while (!l1_pref_q->Empty()) {
        PtrPrefFB fb = l1_pref_q->Read();
        if (fb->type == PFTYPE_STREAM) {
            stream.feedback(fb);
        } else if (fb->type == PFTYPE_STRIDE) {
            stride.feedback(fb);
        }
    }
    if (!l2_pref_q->Empty()) {
        PtrPrefFB fb = l2_pref_q->Read();
        stream.L2Feedback(fb);
    }

    stride.Work();
    stream.Work();
    auto check_and_send = [&](std::deque<uint64_t> &queue, int type, int level) {
        uint32_t count = 0;
        while (!queue.empty() && count <= this->configs->pref_l2_send_thresold) {
            uint64_t addr = queue.front();
            if (level == 1) {
                if (pref_buf->insert_pfl1(addr)) {
                    this->pfl1(addr, type);
                }
            } else if (level == 2) {
                if (pref_buf->insert_pfl2(addr)) {
                    this->pfl2(addr, type);
                }
            } else {
                ASSERT(0);
            }
            queue.pop_front();
            count++;
        }
    };
    auto check_and_clear = [&](std::deque<uint64_t> &queue, uint64_t limit) {
        if (queue.size() < limit) {
            return;
        }
        for (uint64_t i = 0; i <(limit / 2); i++) {
            queue.pop_front();
        }
    };

    check_and_send(stride_pfl1_q, PFTYPE_STRIDE, 1);
    check_and_send(stream_pfl1_q, PFTYPE_STREAM, 1);
    if (l2_data_allow) {
        check_and_send(stride_pfl2_q, PFTYPE_STRIDE, 2);
        check_and_send(stream_pfl2_q, PFTYPE_STREAM, 2);
    }
    check_and_clear(stride_pfl1_q, configs->max_prefq_size);
    check_and_clear(stream_pfl1_q, configs->max_prefq_size);
    check_and_clear(stride_pfl2_q, configs->max_prefq_size);
    check_and_clear(stream_pfl2_q, configs->max_prefq_size);
}

void MtcPrefetcher::Xfer(void)
{
    stride.Xfer();
    stream.Xfer();
}

SimSys* MtcPrefetcher::GetSim(void)
{
    return sim;
}

void MtcPrefetcher::Build(void)
{
    verbose = configs->verbose;

    stride.sim        = sim;
    stride.prefetcher = this;
    stride.pfl1_q     = &stride_pfl1_q;
    stride.pfl2_q     = &stride_pfl2_q;
    stride.Build();

    stream.sim        = sim;
    stream.prefetcher = this;
    stream.pfl1_q     = &stream_pfl1_q;
    stream.pfl2_q     = &stream_pfl2_q;
    stream.Build();

    pref_buf = new PrefetchBuffer(32);
}

void MtcPrefetcher::pfl1(uint64_t addr, uint32_t type)
{
    MemReqBus req;
    req.vld      = 1;
    req.is_load  = true;
    req.addr     = addr;
    req.prefetch = true;
    req.size     = 64;
    req.type     = type;
    req.opcode   = Opcode::OP_LWI; // debug info
    pref_l1_q->Write(req);

    if ((VERBOSE_ON && STAGE_ENABLED(StageID::LSU_ALL)) || verbose) {
        printf("[Pref]: %lu, PFL1 0x%lx. type %u\n", sim->getCycles(), req.addr, type);
    }
    if (DEBUG_VERBOSE_ON || top->configs.verbose) {
        LOG_DEBUG(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                  "[Pref]: %lu, PFL1 0x%lx. type %u", sim->getCycles(), req.addr, type);
    }
}

void MtcPrefetcher::pfl2(uint64_t addr, uint32_t type)
{
    PtrPacket req = Packet::createPrefPkt();
    req->addr     = addr;
    req->size     = 64;
    req->user_type = type;
    req->l1_out_cycle = GetSim()->getCycles();
    pref_l2_q->Write(req);
    if ((VERBOSE_ON && STAGE_ENABLED(StageID::LSU_ALL)) || verbose) {
        printf("[Pref]: %lu, PFL2 0x%lx. type %u\n", sim->getCycles(), req->addr, type);
    }
    if (DEBUG_VERBOSE_ON || top->configs.verbose) {
        LOG_DEBUG(top->debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                  "[Pref]: %lu, PFL2 0x%lx. type %u", sim->getCycles(), req->addr, type);
    }
}

bool MtcPrefetcher::PrefetchBuffer::insert_pfl1(uint64_t addr)
{
    bool r;
    if (pfl1_set.count(addr) != 0) {
        r = false;
    } else {
        pfl1_set.insert(addr);
        r = true;
    }
    return r;
}

bool MtcPrefetcher::PrefetchBuffer::insert_pfl2(uint64_t addr)
{
    bool r;
    if (pfl2_set.count(addr) != 0) {
        r = false;
    } else {
        pfl2_set.insert(addr);
        r = true;
    }
    return r;
}

void MtcPrefetcher::PrefetchBuffer::insert_addr(uint64_t addr_i)
{
    uint64_t addr = addr_i & ~0x3fULL;
    if (addr_set.count(addr) == 0) {
        addr_set.insert(addr);
        addr_fifo.emplace_back(addr);
        if (addr_fifo.size() == depth) {
            auto f_addr = addr_fifo.front();
            addr_fifo.pop_front();
            addr_set.erase(f_addr);
        }
        count++;
        if (count == depth) {
            count = 0;
            pfl1_set.clear();
            pfl2_set.clear();
        }
    }
}

MtcPrefetcher::~MtcPrefetcher()
{
    DeletePtr(pref_buf);
}

} // namespace JCore