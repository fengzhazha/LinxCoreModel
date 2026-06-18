#ifndef BLOCKISA_MODEL_PE_LOCAL_FREELIST_H
#define BLOCKISA_MODEL_PE_LOCAL_FREELIST_H

#include <vector>
#include <unordered_map>

#include "core/Bus.h"
#include "../common/Common.h"
#include "plat/DebugLog.h"

namespace JCore {

class Core;
struct FormalParamEntry {
    bool vld = false;
    bool isRI = false;
    ROBID bid;
    std::vector<uint32_t> ptagList;
};

class LocalFreeList {
public:
    LocalFreeList();
    ~LocalFreeList();
    void Work();
    void Xfer();
    void Build();
    void Alloc();
    void Release();
    uint32_t AllocPtag(LocalFreeInfo &info);
    uint32_t LookupPtag(ROBID &bid, uint32_t offset, uint32_t stid);
    uint32_t DispatchPtag(ROBID &bid, uint32_t offset, uint32_t stid);
    uint32_t GetPtagData(ROBID &bid, uint32_t offset, uint32_t stid);
    void ReleaseBlock(ROBID &bid, uint32_t stid);
    void ReleasePtag(uint32_t ptag);
    bool CheckStall(uint32_t reserve = 1);
    void Flush(FlushBus &flushReq);
    void SetSim(SimSys* sim) { this->sim = sim; }
    SimSys* GetSim() { return sim; }

    std::shared_ptr<DebugLog> debugLogger;

    INPUT SimQueue<LocalFreeInfo>   *rreq_vec_srf_q = nullptr;
    OUTPUT SimQueue<LocalFreeInfo>  *rrsp_srf_vec_q = nullptr;

    INPUT SimQueue<LocalFreeInfo>   *wreq_vec_srf_q = nullptr;
    OUTPUT SimQueue<LocalFreeInfo>  *wrsp_srf_vec_q = nullptr;

    INPUT SimQueue<LocalFreeInfo>   *gsRreqVecSrfQ = nullptr;
    OUTPUT SimQueue<LocalFreeInfo>  *gsRrspSrfVecQ = nullptr;

    INPUT SimQueue<LocalFreeInfo>   *gsWreqVecSrfQ = nullptr;
    OUTPUT SimQueue<LocalFreeInfo>  *gsWrspSrfVecQ = nullptr;

    INPUT SimQueue<LocalFreeInfo>   *mtc_rreq_vec_srf_q = nullptr;
    OUTPUT SimQueue<LocalFreeInfo>  *mtc_rrsp_srf_vec_q = nullptr;

    INPUT SimQueue<LocalFreeInfo>   *mtc_wreq_vec_srf_q = nullptr;
    OUTPUT SimQueue<LocalFreeInfo>  *mtc_wrsp_srf_vec_q = nullptr;

private:
    SimSys *sim = nullptr;
    Core *core = nullptr;
    uint32_t depth = 0;
    uint32_t size = 0;
    std::vector<std::unordered_map<uint32_t, FormalParamEntry>> blockFormalMap;
    std::vector<LocalFreeInfo> fList;
};

}

#endif