#include "l2/L2Cache.h"

#include "ModelSpec.h"
#include "SimSys.h"

namespace JCore {
using namespace std;
void L2Cache::Work() {
    if (!snp_l2_q->Empty()) {
        PtrPacket resp = snp_l2_q->Read();
        missq.pushRespPkt(resp);
        if (verbose) {
            cout << "[L2]: recv snoop packet. " << *resp << "\n";
        }
        if (verbose) {
            LOG_DEBUG_STRUCT(debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                             *resp, "[L2]: recv snoop packet. Packet: ");
        }
    }

    if (pref_throw) {
        pref_throw = false;
        missq.throwPref();
    }

    PtrPacket l1pkt;
    do {
        l1pkt = nullptr;

        // Replace the oldest prefetch request
        bool has_req = !l1_l2_q->Empty() || !inst_l2_q->Empty() || !hpref_l2_q->Empty();
        if (has_req && missq.normalStall()) {
            bool release = missq.replacePref();
            if (!release) {
                if (verbose)
                    printf("[L2]: l1_l2_q is pending, but missq is full\n");
            }
            if (!release && verbose) {
                LOG_DEBUG(debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                          "[L2]: l1_l2_q is pending, but missq is full");
            }
        }

        if (!inst_l2_q->Empty() && !missq.normalStall() && !GetSim()->systemStatus.EcallRunning()) {
            l1pkt = inst_l2_q->Read();
            l1pkt->setInst();   // FIXME: set inst attr in fetch side
            stats->inst_req_count++;
        } else if (!l1_l2_q->Empty() && !missq.normalStall()) {
            l1pkt = l1_l2_q->Read();
            if (l1pkt->isRead()) {
                if (l1pkt->hasPref()) {
                    stats->pfl1_req_count++;
                } else {
                    stats->dmd_load_req_count++;
                }
            } else if (l1pkt->isWrite()) {
                stats->store_req_count++;
            }
            stats->data_req_count++;
            stats->intreval_data_req_count++;
        } else if (!hpref_l2_q->Empty() && !missq.normalStall() && !GetSim()->systemStatus.EcallRunning()) {
            l1pkt = hpref_l2_q->Read();
            l1pkt->setInst();
            stats->hpref_req_count++;
        } else if (!pref_l2_q->Empty() && !missq.prefStall()) {
            l1pkt = pref_l2_q->Read();
        } else if (lsuL2ReqQ != nullptr && !lsuL2ReqQ->Empty()) {
            l1pkt = lsuL2ReqQ->Read();
        }
        if (l1pkt) {
            missq.pushL1Pkt(l1pkt);
            LOG_INFO_M(Unit::LSU, Stage::NA) << "[L2]: recv packet: " << *l1pkt;
            if (verbose) {
                LOG_DEBUG_STRUCT(debugLogger, Unit::LSU, Stage::NA, UINT32_MAX, UINT64_MAX, LogLevel::CRITICAL,
                                 *l1pkt, "[L2]: recv packet. Packet: ");
            }
        }

        if (missq.stall()) {
            break;
        }

    } while (l1pkt);

    if (pref_l2_q->Empty()) {
        pref_l2_q->unsetStall();
    } else {
        pref_l2_q->setStall();
    }

    missq.Work();
    l2mem.Work();

    PtrPacket l2pkt;
    while (missq.getL2Pkt(l2pkt)) {
        if (l2pkt->transferReq || l2pkt->vCoreReq) {
            l2LsuRspQ->Write(l2pkt);
            continue;
        }
        constexpr uint32_t IFU_PKT_ID_OFFSET = 16;
        if ((l2pkt->id >= IFU_PKT_ID_OFFSET) || (l2pkt->id < memID_s)) {
            l2_inst_q->Write(l2pkt);
        } else {
            l2_l1_q->Write(l2pkt);
        }
    }
    if (updateL2Prefetcher && stats->interval_l2_replace_count != 0) {
        feedbackToPrefetcher();
    }
}

void L2Cache::Reset() {
    missq.Reset();
    l2mem.Reset();
}

void L2Cache::Xfer() {
    missq.Xfer();
}

SimSys* L2Cache::GetSim() {
    return sim;
}

void L2Cache::Build() {
    config = make_shared<L2Config>();
    config->overrideDefaultConfig(GetSim()->getCfgs());
    stats = std::make_shared<L2Stats>(GetSim()->getRpt());

    verbose = config->verbose;

    missq.sim    = sim;
    missq.config = config;
    missq.stats  = stats;
    missq.top = this;
    missq.lsuTypeId = lsuTypeId;
    missq.Build();

    l2mem.config = config;
    l2mem.stats = stats;
    l2mem.top = this;
    l2mem.Build();

    missq.l2_mem_q = l2_mem_q;
    missq.mem_l2_q = mem_l2_q;
    missq.l2m = &l2mem;
}

bool L2Cache::data_allow(void) {
    return !missq.normalReqFull();
}

bool L2Cache::inst_allow(void) {
    return !missq.normalReqFull();
}

bool L2Cache::pfl2_allow(void)
{
    return !missq.pfl2Full();
}

void L2Cache::feedbackToPrefetcher()
{
    PtrPrefFB fb = std::make_shared<PrefFB>();
    fb->vld = true;
    fb->isFromL2 = true;
    fb->total_pref_sent_to_next     = stats->total_dprefl2_to_mem_count;
    fb->total_pref_useful           = stats->total_dprefl2_useful_count;
    fb->total_pref_late             = stats->total_dprefl2_late_count;
    fb->total_data_req              = stats->data_req_count - stats->intreval_data_req_count;
    fb->total_dmd_miss              = stats->total_dmd_miss_count;
    fb->total_dmd_miss_due_pref       = stats->total_dmd_miss_due_pref_count;
    fb->interval_pref_sent_to_next  = stats->interval_dprefl2_to_mem_count;
    fb->interval_pref_useful        = stats->interval_dprefl2_useful_count;
    fb->interval_pref_late          = stats->interval_dprefl2_late_count;
    fb->interval_data_req           = stats->intreval_data_req_count;
    fb->interval_dmd_miss           = stats->interval_dmd_miss_count;
    fb->interval_dmd_miss_due_pref    = stats->interval_dmd_miss_due_pref_count;
    l2_pref_q->Write(fb);

    updateL2Prefetcher = false;
    stats->total_l2_replace_count       += stats->interval_l2_replace_count;
    stats->total_dprefl2_to_mem_count   += stats->interval_dprefl2_to_mem_count;
    stats->total_dprefl2_useful_count   += stats->interval_dprefl2_useful_count;
    stats->total_dprefl2_late_count     += stats->interval_dprefl2_late_count;
    stats->total_dmd_miss_count         += stats->interval_dmd_miss_count;
    stats->total_dmd_miss_due_pref_count  += stats->interval_dmd_miss_due_pref_count;
    stats->interval_l2_replace_count      = 0;
    stats->interval_dprefl2_to_mem_count  = 0;
    stats->interval_dprefl2_useful_count  = 0;
    stats->interval_dprefl2_late_count    = 0;
    stats->intreval_data_req_count        = 0;
    stats->interval_dmd_miss_count        = 0;
    stats->interval_dmd_miss_due_pref_count = 0;
}

void L2Cache::ReportStat()
{
    stats->ReportSoc();
}

} // namespace JCore
