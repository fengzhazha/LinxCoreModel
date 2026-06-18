#include "l1/clusters_top.h"

#include <cmath>

namespace JCore {

void L1Top::Reset(void)
{
    for (auto &c : clusters)
        c.Reset();
}

void L1Top::Build(void)
{
    configs.overrideDefaultConfig(GetSim()->getCfgs());
    clusters.resize(configs.l1_clts_num);
    resp_l2_l1_array.resize(configs.l1_clts_num);
    for (uint32_t i = 0; i < clusters.size(); ++i) {
        clusters[i].top = this;
        clusters[i].sim = sim;
        clusters[i].cID = i;
        clusters[i].memID_s = memID_s;
        clusters[i].pConfigs = &configs;
        clusters[i].pref_lu_l1_q = pref_lu_l1_array[i];
        clusters[i].tag_lu_l1_q = tag_lu_l1_array[i];
        clusters[i].lookup_lu_l1_q = lookup_lu_l1_array[i];
        clusters[i].tag_lu_scb_q = tag_lu_scb_array[i];
        clusters[i].lookup_lu_scb_q = lookup_lu_scb_array[i];
        clusters[i].commit_su_scb_q = commit_su_scb_array[i];
        clusters[i].resp_l2_l1_q = &resp_l2_l1_array[i];
        clusters[i].pkt_out_q = wakeup_l1_lu_q;
        clusters[i].wakeup_scb_lu_q = wakeup_scb_lu_q;
        clusters[i].top = this;
        clusters[i].Build();
    }
}

uint64_t L1Top::addr2L1cID(uint64_t addr) {
    ASSERT(configs.l1_clts_num <= pow(2, configs.l1_disp_bit_arr.size()));
    return Addr2cID(addr, configs.l1_disp_bit_arr);
}

void L1Top::dispL2Resp(void)
{
    while (!lookup_l2_l1_q->Empty()) {
        PtrPacket pkt = lookup_l2_l1_q->Read();
        uint32_t cID = addr2L1cID(pkt->addr);
        clusters[cID].resp_l2_l1_q->push_back(pkt);
    }
}

void L1Top::Work(void)
{
    dispL2Resp();
    for (auto &c : clusters)
        c.Work();
}

void L1Top::Xfer(void)
{
    for (auto &q : wakeup_l1_lu_q) {
        q->Work();
    }
    for (auto &q : upgrade_l1_lu_q) {
        q->Work();
    }
    for (auto &c : clusters) {
        c.Xfer();
    }
}

SimSys *L1Top::GetSim(void)
{
    return sim;
}

bool L1Top::dataAllow()
{
    for (auto &c : clusters) {
        if (!c.dataAllow()) {
            return false;
        }
    }

    return true;
}

} // namespace JCore