#include "core/Packet.h"

#include <iostream>
#include <memory>

namespace JCore {

const std::string Packet::CmdName[] = {
	"ReadReq",
	"ReadResp",
	"WriteReq",
	"WriteResp",
	"Writeback",
	"InvalidateReq",
	"InvalidateResp",
	"GetShareReq",
	"GetShareResp",
	"UpgradeReq",
	"UpgradeResp",
    "Prefetch"
};

PtrPacket Packet::createPkt() {
    PtrPacket pkt = std::make_shared<Packet>();
    pkt->l2_miss = false;
    pkt->attrs.reset();
    pkt->pindex = pkt->getId();
    for (uint32_t i = 0; i < 256; i++) {
        pkt->valid[i] = true;
    }
    return pkt;
}

PtrPacket Packet::createPkt(MemCmd cmd, uint32_t tid) {
    PtrPacket pkt = createPkt();
    pkt->cmd = cmd;
    pkt->tid = tid;
    return pkt;
}

PtrPacket Packet::CreateRWPkt(bool write, uint32_t tid) {
    PtrPacket pkt = createPkt();
    if (write) {
        pkt->cmd = WriteReq;
    } else {
        pkt->cmd = ReadReq;
    }
    pkt->tid = tid;
    return pkt;
}

PtrPacket Packet::createRWResp(bool write, bool excl, uint32_t tid) {
    PtrPacket pkt = createPkt();
    if (write) {
        pkt->cmd = WriteResp;
    } else {
        pkt->cmd = ReadResp;
    }
    if (excl) {
        pkt->setExcl();
    }
    pkt->tid = tid;
    return pkt;
}

PtrPacket Packet::createWBPkt(bool dirty) {
    PtrPacket pkt = createPkt();
    pkt->cmd = Writeback;
    if (dirty) {
        pkt->setDirty();
    }
    return pkt;
}

void Packet::getPktCycle(PtrPacket pkt)
{
    l1_out_cycle                = pkt->l1_out_cycle;
    l2_inputQ_cycle             = pkt->l2_inputQ_cycle;
    l2_recv_cycle               = pkt->l2_recv_cycle;
    l2_hit_missfilter_cycle     = pkt->l2_hit_missfilter_cycle;
    l2_missfilter_wakeup_cycle  = pkt->l2_missfilter_wakeup_cycle;
    l2_pick_cycle               = pkt->l2_pick_cycle;
    l2_issue_cycle              = pkt->l2_issue_cycle;
    l2_wait_snp                 = pkt->l2_wait_snp;
    l2_snp_wakeup               = pkt->l2_snp_wakeup;
    l2MissCycle               = pkt->l2MissCycle;
    memRntCycle               = pkt->memRntCycle;
    l2RntCycle                = pkt->l2RntCycle;
    l2_pref_late                = pkt->l2_pref_late;
    l2_pref_useful                = pkt->l2_pref_useful;
}

void Packet::printPktCycle()
{
    std::cout<<"[Packet Cycles]: addr 0x"<<std::hex<<addr;
    std::cout<<", l1_out:"<<std::dec<<l1_out_cycle<<", l2_inputQ:"<<l2_inputQ_cycle<<", l2_recv:"<<l2_recv_cycle;
    if (l2_hit_missfilter_cycle > 0) {
        std::cout<<", hit Miss Filter:"<<l2_hit_missfilter_cycle<<", miss filter wakeup:"<<l2_missfilter_wakeup_cycle;
    }
    std::cout<<", pick:"<<l2_pick_cycle<<", issue:"<<l2_issue_cycle;
    if (l2MissCycle > 0) {
        std::cout<<", l2_miss:"<<l2MissCycle<<", mem_ret:"<<memRntCycle;
    }
    if (l2_wait_snp > 0) {
        std::cout<<", trigger snoop:"<<l2_wait_snp<<", snoop wakeup:"<<l2_snp_wakeup;
    }
    std::cout<<", l2 return:"<<l2RntCycle;
    std::cout<<std::endl;
}

std::ostream& operator<<(std::ostream& out, Packet pkt) {
	out << "[" << pkt.CmdName[pkt.cmd] << "]" << " ";
    out << std::dec << pkt.id << " ";
	out << "0x" << std::hex << pkt.addr;
    out << "(";
    out << std::dec << pkt.tid;
    out << ")";
    out << " tpc 0x" << std::hex << pkt.tpc << std::dec;
    // out << " data " << pkt.data.toStr();
	return out;
}

Packet::Packet() : packet_id(getNextPacketId())
{
}

uint32_t Packet::getNextPacketId()
{
    static std::atomic<uint32_t> packet_id_counter{0};
    return packet_id_counter.fetch_add(1, std::memory_order_relaxed);
}

uint32_t Packet::getId() const
{
    return packet_id;
}

} // namespace JCore
