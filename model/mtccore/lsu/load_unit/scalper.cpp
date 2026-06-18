#include <cstdint>
#include <deque>
#include <iostream>
#include <vector>
#include "mtccore/lsu/MtcLoadStoreUnit.h"
#include "mtccore/lsu/load_unit/scalper.h"

namespace JCore {


void Scalper::Work()
{
    insertInst();
    insertReqest();
    sendToL1();
    mergeStateInfo();
    handleL1miss();
    handleL1Wakeup();
    sendOutInst();
    handleCommit();
    UpdataSendStatus();
}

void Scalper::Reset()
{
    ReqList.clear();
    entryList.clear();
    ReqQ.clear();
    waitTime = 0;
    sendNum = 0;
    receiveNum = 0;
    sentNum = 0;
}

void Scalper::Build()
{
}

void Scalper::Xfer(void)
{
}

SimSys *Scalper::GetSim(void)
{
    return sim;
}

void Scalper::UpdataSendStatus(void)
{
    if (((sendNum % 128) == 0) && (sendNum != 0)) {
        sendNum = 0;
        waitTime = GetSim()->core->configs.scalper_send_pause_time;
    }
}

vector<MemReqBus> Scalper::GetTlsMemReqs(SimInst inst)
{
    vector<MemReqBus> reqs;
    reqs.clear();
    unordered_set<uint64_t> tagSet;
    uint32_t peCnt = top->sim->core->configs.stdPeCount + top->sim->core->configs.simtPeCount;
    MemReqBus bus = inst->GenMemReq(peCnt, 0);

    ASSERT(inst->subrid.val < 512);
    bus.addr = inst->psrcs_[SRC0_IDX]->baseAddr;
    bus.tag = CalTag(bus.addr, true);
    bus.size = inst->psrcs_[SRC0_IDX]->size;
    bus.peID = peCnt;
    bus.vld = true;
    bus.bid = inst->bid;
    bus.gid = inst->gid;
    bus.tpc = inst->pc;
    bus.rid = inst->rid;
    bus.instUid = inst->uid;
    bus.subrid = inst->subrid;
    bus.prefetch = false;
    bus.lsID = inst->lsID;
    bus.toMtcLsu = true;
    bus.is_load = OpcodeIsLoad(inst->opcode);
    inst->pipeCycle->genPrefetchCycle  = top->GetSim()->cycles;

    if (tagSet.count(bus.tag) == 0) {
        tagSet.insert(bus.tag);
        reqs.push_back(bus);
    }

    vector<uint64_t> tagVector(tagSet.begin(), tagSet.end());
    CommitInfo info(inst->bid.GetVal(), inst->gid.GetVal(), inst->subrid.GetVal());
    tagInfo.emplace(info, tagVector);
    return reqs;
}

vector<MemReqBus> Scalper::GetMemReqs(ScEntry entry)
{
    unordered_set<uint64_t> tagSet;
    tagSet.clear();
    SimInst inst = entry.inst;
    vector<MemReqBus> reqs;
    reqs.clear();
    // if (top->sim->perfectGetSet) {
    //     if (inst->ref.gsInfo.src0_vld) {
    //         inst->src0.data = inst->ref.gsInfo.src0_data;
    //     }
    //     if (inst->ref.gsInfo.src1_vld) {
    //         inst->src1.data = inst->ref.gsInfo.src1_data;
    //     }
    //     if (inst->ref.gsInfo.src2_vld) {
    //         inst->src2.data = inst->ref.gsInfo.src2_data;
    //     }
    //     if (inst->ref.gsInfo.src3_vld) {
    //         inst->src3.data = inst->ref.gsInfo.src3_data;
    //     }
    // }
    uint64_t addr = -1U;
    std::unordered_map<uint64_t, std::set<uint32_t>> addrLaneMap;

    if (IsScalarInst(inst->codeLen)) {
        bool ret = inst->Execute();
        ASSERT(ret);
        addr = inst->pdsts_[DST0_IDX]->data;
        addrLaneMap[addr].insert(0);
    } else {
         bool toMem = inst->accMemInfo != nullptr ? !inst->accMemInfo->local : true;
        VecData addrs = inst->pdsts_[DST0_IDX]->vecData;
        for (uint32_t lane = 0; lane < inst->lanes; ++lane) {
            uint64_t curMask = 0; // TODO: inst->srcM.simtMask;
            if (curMask & (1ULL << lane)) {
                bool ret = inst->Execute(lane);
                ASSERT(ret);
                addr = inst->pdsts_[DST0_IDX]->vecData.Get(lane, static_cast<uint32_t>(OperandWidth::OPDW_D));
                /*
                 * FIXME: Temporarily set the tile addr to a higher position to distinguish whether the read/write is
                 * from ddr or tilereg.
                 */
                addrs.Set(addr, lane, static_cast<uint32_t>(OperandWidth::OPDW_D));
                addrLaneMap[addr].insert(lane);
            }
        }
        // 微指令不应该感知block 属于哪个BIQ
        bool isMTCMemReq = (inst->biqType == BIQType::MTC_IQ) ? true : false;

        if (!toMem || (isMTCMemReq)) {
            entry.fsm = MTC_SCPQ_INST_FSM::MTC_INST_SENT;
            CommitInfo info(inst->bid.GetVal(), inst->gid.GetVal(), inst->rid.GetVal());

            LOG_INFO_M(Unit::SCALPER, Stage::PF) <<" send commit info  "<< inst;

            pe_scalper_commit_q->push_back(info);
            pe_iex_lda_array->Write(inst);
            return reqs;
        }
    }

    VecData addrs = inst->pdsts_[DST0_IDX]->vecData;

    for (uint32_t lane = 0; lane < inst->lanes; ++lane) {
        uint64_t curMask = 0; // TODO:inst->srcM.simtMask;
        if (curMask & (1ULL << lane)) {
            addr = inst->pdsts_[DST0_IDX]->vecData.Get(lane, static_cast<uint32_t>(OperandWidth::OPDW_D));
            addrs.Set(addr, lane, static_cast<uint32_t>(OperandWidth::OPDW_D));
            addrLaneMap[addr].insert(lane);
        }
    }
    uint32_t peCnt = top->sim->core->configs.stdPeCount + top->sim->core->configs.simtPeCount;
    for (auto it : addrLaneMap) {
        MemReqBus bus = inst->GenMemReq(peCnt, *it.second.begin());
        if (bus.isCrossCacheLine) {
            MemReqBus bus1;
            MemReqBus bus2;
            MTCGetCrossReq(bus, bus1, bus2);
            if (tagSet.count(bus1.tag) == 0) {
                tagSet.insert(bus1.tag);
                reqs.push_back(bus1);
            }
            if (tagSet.count(bus2.tag) == 0) {
                tagSet.insert(bus2.tag);
                reqs.push_back(bus2);
            }
        } else {
            if (tagSet.count(bus.tag) == 0) {
                tagSet.insert(bus.tag);
                reqs.push_back(bus);
            }
        }
    }
    vector<uint64_t> tagVector(tagSet.begin(), tagSet.end());
    CommitInfo info(inst->bid.GetVal(), inst->gid.GetVal(), inst->rid.GetVal());

    tagInfo.emplace(info, tagVector);
    return reqs;
}

uint32_t Scalper::GetScalperSize()
{
    return entryList.size();
}


void Scalper::insertInst()
{
    if (iexScalperInstQ->size() != 0) {
        ASSERT(GetSim()->core->configs.scalper_enable);
    }
    for (auto &it : *iexScalperInstQ) {
        ASSERT(it != nullptr);
        ASSERT(!it->sentFromSc);
        it->sentFromSc = true;
        it->issued = false;
        ScEntry entry;
        entry.inst = it;
        entry.fsm = MTC_SCPQ_INST_FSM::MTC_INST_SRC_READY;
        entryList.push_back(entry);
        if (entry.inst->opcode == Opcode::OP_TLD) {
            CommitInfo info(entry.inst->bid.GetVal(), entry.inst->gid.GetVal(), entry.inst->subrid.GetVal());
            scLdqOrder->push_back(info);
        }
        LOG_INFO_M(Unit::SCALPER, Stage::PF) <<" Insert inst into SCALPER, inst "<< it;
    }
    iexScalperInstQ->clear();
}


void Scalper::insertReqest()
{
    // 遍历scalper的输入
    vector<MemReqBus> reqs;
    for (uint32_t i = 0; i < entryList.size(); i++) {
        if (entryList[i].fsm != MTC_SCPQ_INST_FSM::MTC_INST_SRC_READY) {
            continue;
        }

        if (entryList[i].inst->biqType == BIQType::TMA_IQ) {
            reqs = GetTlsMemReqs(entryList[i].inst);
        } else {
            reqs = GetMemReqs(entryList[i]);
        }

        for (auto bus : reqs) {
            ASSERT((bus.tag & 0xff) == 0);
            if (ReqQ.count(bus.tag) == 0) {
                BusInfo busInfo;
                busInfo.hitcount = 1;
                busInfo.fsm = MTC_SCPQ_FSM::MTC_SCPQ_IDLE;
                ReqQ.emplace(bus.tag, busInfo);
                ReqList.push_back(bus);
            } else {
                ASSERT(ReqQ.count(bus.tag) == 1);
                ReqQ[bus.tag].hitcount++;
            }
        }
        entryList[i].fsm = MTC_SCPQ_INST_FSM::MTC_INST_SRC_INSERT;
    }
}

// 发消息给L1查询hit信息
void Scalper::sendToL1()
{
    for (auto &bus : ReqList) {
        ASSERT(ReqQ.count(bus.tag) == 1);
        if (ReqQ[bus.tag].fsm == MTC_SCPQ_FSM::MTC_SCPQ_IDLE) {
            bus.fromScalper = true;
            top->SendL1(tag_scalper_l1_array, bus);
            ReqQ[bus.tag].fsm = MTC_SCPQ_FSM::MTC_SCPQ_SENT_L1;
        }
    }
}

void Scalper::updateHitInfo(MemReqBus &bus)
{
    if (ReqQ.count(bus.tag) == 0) {
        return;
    }
    ASSERT(ReqQ.count(bus.tag) == 1);
    ASSERT((ReqQ[bus.tag].fsm == MTC_SCPQ_FSM::MTC_SCPQ_SENT_L1) ||
        (ReqQ[bus.tag].fsm == MTC_SCPQ_FSM::MTC_SCPQ_L1_HIT));

    if (bus.l1_miss && (ReqQ[bus.tag].fsm == MTC_SCPQ_FSM::MTC_SCPQ_SENT_L1)) {
        ReqQ[bus.tag].fsm = MTC_SCPQ_FSM::MTC_SCPQ_L1_MISS;
        LOG_INFO_M(Unit::SCALPER, Stage::PF) <<"L1 miss "<< "addr: 0x" << hex << bus.addr << bus;
    } else {
        ReqQ[bus.tag].fsm = MTC_SCPQ_FSM::MTC_SCPQ_L1_HIT;
    }
}

// 处理L1的回复的hit查询结果
void Scalper::mergeStateInfo(void)
{
    while (!tag_l1_scalper_q->empty()) {
        MemReqBus bus = tag_l1_scalper_q->front();
        tag_l1_scalper_q->pop_front();
        updateHitInfo(bus);
    }
}

// 处理L1 miss的
void Scalper::handleL1miss(void)
{
    for (auto &bus : ReqList) {
        // L1 miss 此处发请求给memory
        ASSERT(ReqQ.count(bus.tag) == 1);
        if (ReqQ[bus.tag].fsm != MTC_SCPQ_FSM::MTC_SCPQ_L1_MISS) { continue; }
        // 发给Memory一定数量req后，需要等几个cycle，否则generic soc会有问题
        if (waitTime > 0) {
            continue;
        }
        // 发给Memory的req数量 - 收到Memory回复的数量 < 384
        if (((sentNum - receiveNum) < 384)) {
            sendMemReq(bus);
            ReqQ[bus.tag].fsm = MTC_SCPQ_FSM::MTC_SCPQ_WAIT_MEM;
            sendNum++;
            sentNum++;
            break;
        } else {
            top->stats->sendMemReqStallCycles++;
        }
    }
    if (waitTime > 0) { waitTime--; }
}

void Scalper::sendMemReq(MemReqBus &bus)
{
    GFUMemReq req;
    req.vld = true;
    req.lsuTypeId = LSUType::MEMORY_LSU;
    req.size = 256;
    req.tid = (2 << TID_TYPE_OFFSET) + (bus.bid.val << 17) + (bus.rid.val << 9) + bus.subrid.val;
    req.prefetch = false;
    ASSERT((bus.tag & 0xff) == 0);
    req.addr = bus.tag & (uint64_t)~0xff;
    req.is_store = false;
    req.cmd = Packet::ReadReq;
    req.bypassL2 = true;
    FindInst(bus)->pipeCycle->sendMemoryReqCycle = top->GetSim()->cycles;
    top->pkt_out_q->Write(req);
}

// 处理L1的回填
void Scalper::handleL1Wakeup()
{
    while (!wakeup_l1_scalper_q->Empty()) {
        MemReqBus bus = wakeup_l1_scalper_q->Front();
        wakeup_l1_scalper_q->Read();
        if (ReqQ.count(bus.tag) == 0) {
            continue;
        }
        ASSERT(ReqQ.count(bus.tag) == 1);
        if (ReqQ[bus.tag].fsm == MTC_SCPQ_FSM::MTC_SCPQ_WAIT_MEM) {
            receiveNum++;
        }
        if ((ReqQ[bus.tag].fsm == MTC_SCPQ_FSM::MTC_SCPQ_IDLE) ||
            (ReqQ[bus.tag].fsm == MTC_SCPQ_FSM::MTC_SCPQ_L1_MISS) ||
            (ReqQ[bus.tag].fsm == MTC_SCPQ_FSM::MTC_SCPQ_WAIT_MEM) ||
            (ReqQ[bus.tag].fsm == MTC_SCPQ_FSM::MTC_SCPQ_SENT_L1)) {
            ReqQ[bus.tag].fsm = MTC_SCPQ_FSM::MTC_SCPQ_L1_HIT;
        }
    }
}

// 接受commit发过来的消息
void Scalper::handleCommit()
{
    while (!pe_scalper_commit_q->empty()) {
        CommitInfo info = pe_scalper_commit_q->front();
        for (auto it = entryList.begin(); it != entryList.end();) {
            uint32_t rid = it->inst->rid.GetVal();
            if (it->inst->opcode == Opcode::OP_TLD) {
                rid = it->inst->subrid.GetVal();
            }
            if ((info.bid == it->inst->bid.GetVal()) &&
                    (info.rid == rid) && (info.gid == it->inst->gid.GetVal())) {
                ASSERT(it->fsm == MTC_SCPQ_INST_FSM::MTC_INST_SENT);
                it = entryList.erase(it);
            } else {
                it++;
            }
        }
        pe_scalper_commit_q->pop_front();
    }
}

SimInst Scalper::FindInst(MemReqBus &bus)
{
    for (auto& entry : entryList) {
        if (entry.inst->opcode != Opcode::OP_TLD) { continue; }
        if (entry.inst->bid.val != bus.bid.val) { continue; }
        if (entry.inst->rid.val != bus.rid.val) { continue; }
        if (entry.inst->gid.val != 0) { continue; }
        if (entry.inst->subrid.val != bus.subrid.val) { continue; }
        return entry.inst;
    }
    ASSERT(false);
    SimInst inst;
    return inst;
}

// 将指令发给下游, 目前是按序发的，
void Scalper::sendOutInst()
{
    for (auto& entry : entryList) {
        if (entry.fsm != MTC_SCPQ_INST_FSM::MTC_INST_SRC_INSERT) {
            continue;
        }
        bool l1hit = true;
        CommitInfo info(entry.inst->bid.GetVal(), entry.inst->gid.GetVal(), entry.inst->rid.GetVal());
        if (entry.inst->opcode == Opcode::OP_TLD) {
            info.rid = entry.inst->subrid.val;
        }
        vector<uint64_t> tagVector = tagInfo[info];
        ASSERT(tagVector.size() > 0);
        std::unordered_map<uint64_t, uint64_t> mm;
        for (auto tag : tagVector) {
            ASSERT((tag & 0xff) == 0);
            ASSERT((ReqQ.count(tag) == 1));
            if (ReqQ[tag].fsm != MTC_SCPQ_FSM::MTC_SCPQ_L1_HIT) {
                l1hit = false;
                mm.clear();
                break;
            }
            if (entry.fsm == MTC_SCPQ_INST_FSM::MTC_INST_SRC_INSERT) {
                if (mm.count(tag) == 0) {
                    mm.emplace(tag, 1);
                } else {
                    mm[tag]++;
                }
            }
        }

        if (l1hit) {
            entry.fsm = MTC_SCPQ_INST_FSM::MTC_INST_L1_HIT;
            entry.inst->pipeCycle->prefetchDataRetCycle = top->GetSim()->cycles;
            LOG_INFO_M(Unit::SCALPER, Stage::PF) <<" receive memory resp, Prefectch data in streamCache, inst "
                << entry.inst;
            for (auto it : mm) {
                ASSERT(ReqQ[it.first].hitcount >= it.second);
                ReqQ[it.first].hitcount = ReqQ[it.first].hitcount - it.second;
            }
        }
    }

    uint32_t cnt = 0;
    for (auto& entry : entryList) {
        if (entry.inst->opcode == Opcode::OP_TLD && entry.fsm == MTC_SCPQ_INST_FSM::MTC_INST_SENT) {
            cnt++;
        } else if (entry.inst->opcode != Opcode::OP_TLD && entry.fsm == MTC_SCPQ_INST_FSM::MTC_INST_SENT) {
            cnt += entry.inst->lanes;
        }
    }

    for (auto& entry : entryList) {
        if (entry.fsm == MTC_SCPQ_INST_FSM::MTC_INST_SENT) { continue; }
        if ((entry.fsm == MTC_SCPQ_INST_FSM::MTC_INST_L1_HIT)) {
            if (entry.inst->opcode == Opcode::OP_TLD) {
                cnt++;
            } else if (entry.inst->opcode != Opcode::OP_TLD) {
                cnt += entry.inst->lanes;
            }
            if (cnt > pConfigs->lu_clusters_depth) {
                top->stats->ldqStallCycles++;
                break;
            }
            if (top->mtciexScbReqQ->getStall()) {
                top->stats->tileScbStallCycles++;
                break;
            }

            CommitInfo info(entry.inst->bid.GetVal(), entry.inst->gid.GetVal(), entry.inst->rid.GetVal());
            if (entry.inst->opcode == Opcode::OP_TLD) {
                info.rid = entry.inst->subrid.val;
            }
            ASSERT(tagInfo.count(info) == 1);
            auto it1 = tagInfo.find(info);
            tagInfo.erase(it1);

            entry.inst->pipeCycle->sendFromScalperCycle = top->GetSim()->cycles;
            pe_iex_lda_array->Write(entry.inst);
            entry.fsm = MTC_SCPQ_INST_FSM::MTC_INST_SENT;
            LOG_INFO_M(Unit::SCALPER, Stage::PF) <<" sendout inst to issueQ, inst"<< entry.inst;
            break;
        } else {
            break;
        }
    }
    removeElements();
}

// 删除hitcount为0的元素
void Scalper::removeElements()
{
    for (auto it = ReqQ.begin(); it != ReqQ.end();) {
        if (it->second.hitcount != 0) {
            ++it;
            continue;
        }
        // 找到hitcount为0的元素，将其从ReqQ中删除
        int num = 0;
        for (auto it1 = ReqList.begin(); it1 != ReqList.end();) {
            if (it1->tag == it->first) {
                // 找到tag为0的元素，将其从ReqList中删除
                it1 = ReqList.erase(it1);
                num++;
            } else {
                ++it1;
            }
        }
        ASSERT(num == 1);
        it = ReqQ.erase(it);
    }
}
}
