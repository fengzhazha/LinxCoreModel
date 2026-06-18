#pragma once

#include <vector>

#include "GFUSim.h"
#include "ModelSpec.h"
#include "SimSys.h"

namespace JCore {

class LoadStoreUnit;
class MtcLoadStoreUnit;

class SOC : public SimObj {
    std::map<LSUType, std::shared_ptr<LoadStoreUnit>> lss;
    std::map<LSUType, std::shared_ptr<MtcLoadStoreUnit>> mtclss;
    std::map<LSUType, SimSys*> sim;
public:
    virtual ~SOC();
    void                                    recvGFURsp();
    void                                    sendGFUReq();
    std::vector<GFUMemReq>                  *gfuL2ReqList;
    std::vector<GFUMemReq>                  *gfuL2BusyList;
    uint32_t                                sendGFUReqNum = 0;
    uint32_t                                recvCHIReqNum = 0;
    GFUMemReq                               getReqById(unsigned tid, bool &matchStore);
    void                                    gfuMemRequest();
    void                                    GfuMtcMemRequest();
    void                                    Reset();
    void                                    Work();
    void                                    Xfer();
    void                                    Init();
    void ReportStat() override {}
    void                                    Build(SimSys *simSys);
    void                                    Build(LSUType typeId, std::shared_ptr<LoadStoreUnit> system);
    void                                    Build(LSUType typeId, std::shared_ptr<MtcLoadStoreUnit> system);
    void                                    Build(LSUType typeId, SimSys *simSys);
    using SimObj::GetSim;
    SimSys                                 *GetSim(LSUType typeId) {return sim[typeId];}
    void                                    SocWork();
    /* fakelsu  <--> soc */
    SimQueue<GFUMemReq>                     *l2SocReqQ;
    SimQueue<GFUMemReq>                     *socL2RspQ;
    bool socFreqConv = true;

    /* BridgeTable <--> SOC */
    INPUT   SimQueue<GFUMemReq>     *bridgeSocReqQ;
    OUTPUT SimQueue<GFUMemReq>      *socBridgeRspQ;
};

} // namespace JCore