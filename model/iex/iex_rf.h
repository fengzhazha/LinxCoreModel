#pragma once

#include "../configs/core_config.h"
#include "../configs/iex_config.h"
#include "ModelCommon/ModelCommon.h"
#include "ModelCommon/bus/RFReqBus.h"
#include "ModelCommon/bus/FlushBus.h"
#include "ModelCommon/bus/FormalParamReq.h"

namespace JCore {

const uint32_t SINGLE_VRF_SIZE = 256 / sizeof(uint64_t);

/*
    Input: readfile read/write request bus from all pipes

    Output: readfile data return bus

*/
struct RegFileState {
    /* \brief T register files */
    std::vector<std::vector<std::vector<uint64_t>>>          tRegFile;
    /* \brief U register files */
    std::vector<std::vector<std::vector<uint64_t>>>          uRegFile;
    /* \brief global/local physical register files */
    std::vector<uint64_t>                                    glRegFile;
    std::vector<uint64_t>                                    lpRegFile;
    /* TODO: Support max to 128 lanes */
    std::vector<std::vector<std::vector<uint64_t>>>          SIMTMask;
    /* \brief stack physical register files */
    std::vector<uint64_t>                                    stackRegFile;
    /* \brief simt vector register files */
    std::vector<uint64_t>                                    vrfRegFile;
    /* \brief simt predicate register files */
    // std::vector<__uint128_t>                    pRegFile;

    void                                        Build(CoreConfig &configs, uint64_t preg_count,
                                                        uint64_t pe_count, ExecEngineTyp type);
    void                                        Reset();
};

class SimSys;
class IEX;

class RegFile {
private:
    RegFileState                                regFile;

public:
    RegFile() {};
    ~RegFile() {};

    IEXConfig                                   configs;
    IEX                                         *top;
    SimSys                                      *GetSim();

    INPUT std::vector<RFReqBus*>                rf_rd_req;
    INPUT std::vector<RFReqBus*>                rf_wr_req;
    INPUT std::vector<RFReqBus*>                rf_ld_wr_req;
    INPUT std::vector<SimQueue<RFReqBus>*>      pe_rf_wr_req;
    INPUT SimQueue<RFReqBus>                    rf_wr_q;
    OUTPUT std::vector<RFReqBus*>               rf_data_ret;
    INPUT SimQueue<LocalFreeInfo>*              req_vec_siex_q = nullptr;
    OUTPUT SimQueue<LocalFreeInfo>*             rsp_siex_vec_q = nullptr;
    INPUT SimQueue<LocalFreeInfo>*              gsReqVecSiexQ = nullptr;
    OUTPUT SimQueue<LocalFreeInfo>*             gsRspSiexVecQ = nullptr;
    INPUT SimQueue<LocalFreeInfo>*              mtc_req_vec_siex_q = nullptr;
    OUTPUT SimQueue<LocalFreeInfo>*             mtc_rsp_siex_vec_q = nullptr;
    INPUT SimQueue<LocalFreeInfo>*              data_vec_viex_q = nullptr;
    OUTPUT SimQueue<LocalFreeInfo>*             data_viex_vec_q = nullptr;
    INPUT SimQueue<LocalFreeInfo>*              gsDataVecViexQ = nullptr;
    OUTPUT SimQueue<LocalFreeInfo>*             gsDataViexVecQ = nullptr;
    void                                        Build(uint64_t preg_count, uint64_t pe_count);
    void                                        Reset();
    void                                        Work();
    void                                        Xfer();
    void                                        flush(FlushBus &flushReq);

    void                                        receiveReq();
    void                                        recieveWFReqFromRT();
    void                                        readfile();
    void                                        writefile();
    void                                        returnData();
    void                                        bypass();

    uint64_t                                    getGPR(uint32_t ptag);
    void                                        setGPR(uint32_t ptag, uint64_t data);
    // TODO GetSIMTMask
    void                                        SetSIMTMask(uint32_t peid, uint64_t data);
    void BypassReqW2R(RFReqBus* wReq, RFReqBus* rReq, bool loadBypass = false);
};

} // namespace JCore
