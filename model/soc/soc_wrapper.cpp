#include "soc/soc_wrapper.h"

#include "lsu/lsu.h"
#include "mtccore/lsu/MtcLoadStoreUnit.h"

using namespace JCore;
// external function calls
#ifdef EXT_SOC_INTF
extern void createLightCoreSoC();
extern bool isChiTxnMgrFull();
extern bool hasDatRsp();
extern unsigned popDatRsp();
extern void pushDatReq(unsigned memTxnId, bool isRd, uint64_t paddr, uint64_t vaddr, bool isExcl);
extern void SoC_xfer();
extern void SoC_work();
#else
void createLightCoreSoC(){
    cerr<<"We do not support SOC simulation in ARM environment"<<endl;
    exit(-1);
}
bool isChiTxnMgrFull(){return true;}
bool hasDatRsp(){return false;}
unsigned popDatRsp(){return 0;}
void pushDatReq(unsigned memTxnId, bool isRd, uint64_t paddr, uint64_t vaddr, bool isExcl){}
void SoC_xfer() {}
void SoC_work() {}
#endif

static void simMemOp(SimSys *sim, GFUMemReq &req);

void SOC::Init()
{
    createLightCoreSoC();
    gfuL2ReqList = new vector<GFUMemReq>;
    gfuL2BusyList = new vector<GFUMemReq>;
}

void SOC::Build(LSUType typeId, shared_ptr<LoadStoreUnit> system) {
    uint32_t typeIndex = (typeId == LSUType::SCALAR_LSU) ? 0 : 1;
    cout<<"soc::Build the typeIndex=" << typeIndex << endl;
    lss[typeId] = system;
    sim[typeId] = system->GetSim();
}

void SOC::Build(LSUType typeId, shared_ptr<MtcLoadStoreUnit> system)
{
    uint32_t typeIndex = (typeId == LSUType::MEMORY_LSU) ? 3 : 0;
    cout<<"soc::Build the typeIndex=" << typeIndex << endl;
    mtclss[typeId] = system;
    sim[typeId] = system->GetSim();
}

void SOC::Build(LSUType typeId, SimSys *simSys)
{
    sim[typeId] = simSys;
}

void SOC::Build(SimSys *simSys)
{
    sim[LSUType::SCALAR_LSU] = simSys;
}

void SOC::SocWork()
{
    while (l2SocReqQ != nullptr && !l2SocReqQ->Empty()) {
        GFUMemReq memReqToSoC = l2SocReqQ->Read();
        memReqToSoC.socAaccelptCycle = GetSim(memReqToSoC.lsuTypeId)->getCycles();
        if (GetSim(memReqToSoC.lsuTypeId)->verboseON) {
            cout<<"Mem requst aaccelpt, 0x"<<hex<<memReqToSoC.addr<<" tid "<<memReqToSoC.tid<<endl;
        }
        gfuL2ReqList->push_back(memReqToSoC);
    }
    sendGFUReq();
    if (!gfuL2BusyList->empty()) {
        recvGFURsp();
        SoC_work();
        SoC_xfer();
    }
}

void SOC::Work() {
    gfuMemRequest();
    sendGFUReq();
    LSUType typeId = LSUType::SCALAR_LSU;
    if (!gfuL2BusyList->empty()) {
        typeId = gfuL2BusyList->front().lsuTypeId;
        if (typeId == LSUType::BRIDGE_TABLE) {
            typeId = LSUType::SCALAR_LSU;
        }
    }

    bool socClockGatingFlag;
    if (typeId == LSUType::MEMORY_LSU) {
        socClockGatingFlag = mtclss[typeId]->configs.soc_clock_gating;
    } else {
        socClockGatingFlag = lss[typeId]->configs.soc_clock_gating;
    }

    if (!socClockGatingFlag || !gfuL2BusyList->empty()) {
        recvGFURsp();
        SoC_work();
        SoC_xfer();
    }
}

void SOC::Xfer() {

}

void SOC::Reset() {

}

static void ValidateReqSize(GFUMemReq& gfuMemReq)
{
    if (gfuMemReq.size != 64) {
        // Generic Soc need Addr 128 bit align
        ASSERT((gfuMemReq.addr & 0x7f) == 0);
    }

    if (gfuMemReq.bypassL2) {
        ASSERT(gfuMemReq.size == 256);
    } else {
        /* FIXME: Modify so that the bridge can partial write. */
        if (gfuMemReq.lsuTypeId != LSUType::BRIDGE_TABLE) {
            ASSERT(gfuMemReq.size == 64);
        }
    }
}

void SOC::recvGFURsp() {
    uint32_t recChiNumPerCycle = 0;
    while (hasDatRsp()) {
        recChiNumPerCycle++;
        if (recChiNumPerCycle>2) break;

        unsigned tid = popDatRsp();
        bool matchStore = false;
        GFUMemReq gfuMemReq(getReqById(tid, matchStore));
        if (!gfuMemReq.vld) {
            continue;
        }

        recvCHIReqNum++;

        // LSU process the response from SOC
        // If load request match the unfinished store request, get data from store request.
        if (!(!gfuMemReq.is_store && matchStore)) {
            ValidateReqSize(gfuMemReq);
            simMemOp(sim[gfuMemReq.lsuTypeId], gfuMemReq);
        }
        bool bo = (gfuMemReq.transferReq || gfuMemReq.vCoreReq);
        if (socL2RspQ != nullptr && bo) {
            socL2RspQ->Write(gfuMemReq);
            continue;
        }

        if (gfuMemReq.lsuTypeId == LSUType::BRIDGE_TABLE) {
            socBridgeRspQ->Write(gfuMemReq);
            continue;
        }

        if (gfuMemReq.is_store) {
            continue;
        }

        if (gfuMemReq.lsuTypeId == LSUType::MEMORY_LSU) {
            mtclss[gfuMemReq.lsuTypeId]->pkt_in_q->Write(gfuMemReq);
        } else {
            lss[gfuMemReq.lsuTypeId]->pkt_in_q->Write(gfuMemReq);
        }
    }
}

void SOC::sendGFUReq() {
    for (auto it = gfuL2ReqList->begin(); it != gfuL2ReqList->end(); ) {
        if (isChiTxnMgrFull()) break;
        GFUMemReq gfuMemReq = *it;
        sendGFUReqNum++;
        pushDatReq(gfuMemReq.tid, !gfuMemReq.is_store, gfuMemReq.addr, gfuMemReq.addr, false);
        gfuL2BusyList->push_back(gfuMemReq);
        it = gfuL2ReqList->erase(it);
    }
}

GFUMemReq SOC::getReqById(unsigned tid, bool &matchStore) {
    GFUMemReq gfuMemReq;
    for (auto it = gfuL2BusyList->begin(); it != gfuL2BusyList->end(); ++it) {
        if (it->tid == tid) {
            gfuMemReq = *it;
            // TODO: FIX SOC
            // Just to fix the issues that should be fixed by soc.
            // Load request arrived while store request was unfinished, and then get bad data.
            if (!(*it).is_store) {
                for (auto sit = gfuL2BusyList->begin(); sit != it; ++sit){
                    if ((*sit).is_store && ((*sit).addr == (*it).addr)) {
                        uint32_t loopcnt = (*it).size >> 3;
                        for (uint64_t i = 0; i < loopcnt; i++) {
                            gfuMemReq.data[i] = (*sit).data[i];
                        }
                        matchStore = true;
                    }
                }
            }
            gfuMemReq.socReturnCycle = GetSim(gfuMemReq.lsuTypeId)->getCycles();
            if (GetSim(gfuMemReq.lsuTypeId)->verboseON)
                cout << "[SOC_wrapper] Mem request return, 0x" << hex << it->addr << " tid 0x" << it->tid <<
                    " cycle: " << dec << gfuMemReq.socReturnCycle << endl;
            gfuL2BusyList->erase(it);
            return gfuMemReq;
        }
    }
    return gfuMemReq;
}

void SOC::gfuMemRequest() {
    for (auto it = lss.begin(); it != lss.end(); ++it) {
        while (!it->second->pkt_out_q->Empty()) {
            GFUMemReq memReqToSoC = it->second->pkt_out_q->Read();
            memReqToSoC.socAaccelptCycle = GetSim(memReqToSoC.lsuTypeId)->getCycles();
            if (GetSim(memReqToSoC.lsuTypeId)->verboseON)
                cout << "Mem requst aaccelpt, 0x" << hex << memReqToSoC.addr << " tid " << memReqToSoC.tid << endl;
            gfuL2ReqList->push_back(memReqToSoC);
        }
    }

    GfuMtcMemRequest();
    while (!bridgeSocReqQ->Empty()) {
        GFUMemReq memReqToSoC = bridgeSocReqQ->Read();
        memReqToSoC.socAaccelptCycle = GetSim(memReqToSoC.lsuTypeId)->getCycles();
        ASSERT(memReqToSoC.tid != UINT32_MAX);
        if (GetSim(memReqToSoC.lsuTypeId)->verboseON) {
            cout << "Mem requst aaccelpt, 0x" << hex << memReqToSoC.addr << " tid " << memReqToSoC.tid << endl;
        }
        gfuL2ReqList->push_back(memReqToSoC);
    }
}


void SOC::GfuMtcMemRequest()
{
    for (auto it = mtclss.begin(); it != mtclss.end(); ++it) {
        while (!it->second->pkt_out_q->Empty()) {
            GFUMemReq memReqToSoC = it->second->pkt_out_q->Read();
            memReqToSoC.socAaccelptCycle = GetSim(memReqToSoC.lsuTypeId)->getCycles();
            ASSERT(memReqToSoC.tid != UINT32_MAX);
            if (GetSim(memReqToSoC.lsuTypeId)->verboseON) {
                cout << "[SOC_wrapper] Memory core Mem requst aaccelpt, 0x" << hex << memReqToSoC.addr << " tid " << memReqToSoC.tid
                    << "cycle: " << dec << memReqToSoC.socAaccelptCycle << endl;
            }
            gfuL2ReqList->push_back(memReqToSoC);
        }
    }
}

static void simMemOp(SimSys *sim, GFUMemReq &req)
{
    auto canWrite = [](uint32_t loopCnt, GFUMemReq &req)->bool {
        uint32_t idx = loopCnt / 64;
        uint32_t offset = loopCnt % 64;
        if ((req.bitMask[idx] & (1ULL << offset)) == 0) {
            return false;
        }
        return true;
    };
    int loopcnt = req.size >> 3;
    uint32_t aaccelssByte = 8;

    if (req.bypassL2) {
        ASSERT(req.size == 256);
    } else {
        if (req.lsuTypeId != LSUType::BRIDGE_TABLE) {
            ASSERT(req.size == 64);
        }
    }

    if (req.lsuTypeId == LSUType::BRIDGE_TABLE) {
        loopcnt = req.size;
        aaccelssByte = 1;
    }
    if (req.is_store) {
        for (int i = 0; i < loopcnt; i++) {
            uint64_t data = 0;
            if (req.lsuTypeId == LSUType::BRIDGE_TABLE && !canWrite(i, req)) {
                continue;
            }
            std::memcpy(&data, req.data8b + (i * aaccelssByte), aaccelssByte);
            sim->storeData(req.addr + i * aaccelssByte, data, aaccelssByte);
        }
    } else if (req.is_inst) {
        // Fetch
        for (int i = 0; i < loopcnt; i++) {
            uint64_t data = sim->fetchData(req.addr + i * aaccelssByte, aaccelssByte);
            std::memcpy(req.data8b + (i * aaccelssByte), &data, aaccelssByte);
        }
    } else {
        // Load
        for (int i = 0; i < loopcnt; i++) {
            uint64_t data = sim->loadData(req.addr + i * aaccelssByte, aaccelssByte, false);
            std::memcpy(req.data8b + (i * aaccelssByte), &data, aaccelssByte);
        }
    }
}

SOC::~SOC()
{
    DeletePtr(gfuL2ReqList);
    DeletePtr(gfuL2BusyList);
}