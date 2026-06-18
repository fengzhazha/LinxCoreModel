#ifndef  BLOCKISA_MODEL_MODLE_SPEC_H
#define  BLOCKISA_MODEL_MODLE_SPEC_H
#pragma once

#include <cassert>
#include <cstdint>
#include <deque>
#include <memory>
#include <queue>
#include <cstdint>
#include <vector>

#include "ISA.h"
#include "DataStruct/MachineId.h"

namespace JCore {
// Commented-out macros are defined in the Build script
/* ----------Block Component Parameters--------- */

#define LSU_ID_RANGE                (UINT32_MAX - 1)

/* \brief Base Simulator Class */
class SimSys;
class SimObj {
public:
    MachineType machineType = MachineType::UNKNOW_CORE;
    uint64_t machineId = 0;
    /* \brief logic simulation methods */
    virtual void Work() =0;
    /* \brief shadow buffer commits */
    virtual void Xfer() =0;
    /* \brief Reset */
    virtual void Reset() =0;
    virtual void ReportStat() = 0;
    virtual SimSys *GetSim() {return nullptr;};
    virtual ~SimObj() = 0;
};

inline SimObj::~SimObj() {}


enum class StageID {
    BCC_ALL,
    BCC_FETCH,
    BCC_DECODE,
    BCC_DISPATCH,
    BCC_ISSUE,
    BCC_PRF,
    BCC_ROB,
    BROB_FULL,
    BIFU_ALL,
    BIFU_FETCH,
    BIEX_ALL,
    BIEX_GET_ALLOC,
    BIEX_SET_ALLOC,
    BIEX_GET_ISSUE,
    BIEX_SET_ISSUE,
    BIEX_SET_WAKEUP,
    BIEX_GRF,           // global read file
    BCC_RENAME_ALL,
    BRENAME_S1,
    BRENAME_S2,
    BRENAME_S3,
    OPE_FETCH,
    OPE_DECODE,
    OPE_RENAME,
    OPE_ISSUE,
    OPE_ISSUE_FULL,
    OPE_DISPATCH,
    OPE_PRF,
    OPE_EXE,
    OPE_WB,
    OPE_ROB,
    OPE_ROB_FULL,
    OPE_RETIRE,
    OPE_BYPASS,
    OPE_ALL,
    LSU_ALL,
    IFU_ALL
};

template <class T>
class CycArray {
public:
    std::vector<T>          entry;
    uint64_t                rptr;
    uint64_t                wptr;
    uint64_t                size;
    uint64_t                capacity;

    CycArray() : rptr(0), wptr(0), size(0), capacity(0) {}
    CycArray(uint64_t s)
        : rptr(0), wptr(0), size(0), capacity(s) {
        entry.resize(capacity);
    }

    ~CycArray() {}

    void Reset() {
        entry.clear();
        entry.resize(capacity);
    }

    void push_back(T const pkt) {
        entry[wptr] = pkt;
        wptr = (wptr + 1) % capacity;
        size++;
        ASSERT(size <= capacity);
    }

    void incWPtr() {
        wptr = (wptr + 1) % capacity;
        size++;
        ASSERT(size <= capacity);
    }

    void pop_front() {
        entry[rptr] = T();
        rptr = (rptr + 1) % capacity;
        ASSERT(size > 0);
        size--;
    }

    T &front() {
        return entry[rptr];
    }

    T &back() {
        return entry[wptr];
    }

    T* aaccelss(uint64_t idx) {
        uint64_t entry_idx = (idx + rptr) % capacity;
        return &entry[entry_idx];
    }

    T &operator[](uint64_t idx) {
        uint64_t entry_idx = (idx + rptr) % capacity;
        return entry[entry_idx];
    }

    bool checkFull()
    {
        return size >= capacity;
    }

    bool checkEmty()
    {
        return size == 0;
    }
};

struct InstTrace {
    uint64_t            tpc = 0;
    uint64_t            res = 0;
    bool                jump = false;
};

struct HyperTraceEntry {
    uint64_t                bpc = 0;
    bool                    isHyper = false;
    std::deque<InstTrace>   instQ;
};

struct LDAddrMapInfo {
    bool                    vld = false;
    uint64_t                depend_id = -1U;
};

struct LoadStoreInfo {
    bool                    ref_vld = false;
    bool                    is_load = false;
    bool                    id_vld = false; // whether youngest store for this address
    uint64_t                id = -1U;       // store--id
    uint64_t                addr = -1U;
    uint64_t                data = -1U;
    uint32_t                size = -1U;
    std::deque<uint64_t>    depend_id;      // load--dependent store id
    uint64_t                bid = -1U;
    void Reset() { ref_vld = false; depend_id.clear(); }
};

struct GetSetInfo {
    bool                ref_vld = false;
    bool                src0_vld = false;
    bool                src1_vld = false;
    bool                src2_vld = false;
    bool                src3_vld = false;
    // uint64_t            src0_atag = -1U;
    // uint64_t            src1_atag = -1U;
    // uint64_t            src2_atag = -1U;
    uint64_t            src0_data = -1U;
    uint64_t            src1_data = -1U;
    uint64_t            src2_data = -1U;
    uint64_t            src3_data = -1U;

    bool                dst1_vld = false;
    // uint64_t            dst1_atag = -1U;
    uint64_t            dst1_data = -1U;

    uint64_t            bpc = -1U;      // For debugging only
    uint64_t            tpc = -1U;      // For debugging only
};

/* \brief Golden reference information entry */
struct RefTraceEntry {
    uint64_t                        bpc = -1U;
    std::deque<GetSetInfo>          gsInfoQ;
    uint64_t                        gsRPtr = 0;
    std::deque<LoadStoreInfo>       lsInfoQ;
    uint64_t                        lsRPtr = 0;

    RefTraceEntry() {
        gsInfoQ.clear();
        lsInfoQ.clear();
        bpc = -1U;
        gsRPtr = 0;
        lsRPtr = 0;
    }
    void incGSRPtr() { gsRPtr = gsRPtr + 1; }
    void decGSRPtr() {
        if (gsInfoQ.size()) {
            if (!gsRPtr) {
                return;
            }
            gsRPtr--;
        }
    }
    void resetGSRPtr() { gsRPtr = 0; }

    void incLSRPtr() { lsRPtr = lsRPtr + 1; }
    void decLSRPtr() {
        if (lsInfoQ.size()) {
            if (!lsRPtr) {
                return;
            }
            lsRPtr--;
        }
    }
    void resetLSRPtr() { lsRPtr = 0; }
    void dequeLSInfo() {
        if (lsInfoQ.empty()) {
            return;
        }
        decLSRPtr();
        lsInfoQ.pop_front();
    }
};

/* \brief fetch request info and bin info */
struct FetchInfo {
    uint64_t                        pc;
    uint32_t                        size;
    uint32_t                        bnextSize;
    bool                            isHeader;
    bool                            isMinst;
    bool                            isLastInst;
    bool                            branchTaken;
    uint64_t                        bin; // only for debug
    uint32_t                        battrCnt;

    FetchInfo()
    {
        Reset();
    }
    void Reset()
    {
        pc = 0x0;
        size = bnextSize = 0;
        isHeader = isMinst = isLastInst = branchTaken = false;
        bin = 0x0;
        battrCnt = 0;
    }
};

struct EcallDat {
    uint64_t bpc;
    uint64_t bid;
    std::vector<uint64_t> tpc;
    std::vector<uint64_t> src;
    bool check = false;
};

struct CheckInfo {
    bool check = false;
    uint64_t loadId = 0;
    uint64_t storeId = 0;
};

struct ReferenceInfo {
    /* \brief BPC trace input for perfect BP */
    std::queue<FetchInfo>           headerTrace;
    /* \brief Golden reference information for perfect simulation mode */
    CycArray<RefTraceEntry>         refTrace;
    /* \brief Store queue for perfect simulation mode */
    std::deque<LoadStoreInfo>       refStq;
    /* \brief Store counter for perfect simulation mode */
    uint64_t                        refStoreCount = 0;
    std::vector<EcallDat>           ecall_data;
    uint64_t                        bptr = 0;
    bool                            ckpt_file = false;
    void retireLSInfo(uint32_t bid) {
        if (refTrace.entry[bid].lsInfoQ.empty()) {
            return;
        }
        auto retiredLS = refTrace.entry[bid].lsInfoQ.front();
        if (!retiredLS.is_load) {
            for (auto it = refStq.begin(); it != refStq.end(); ) {
                if (it->id == retiredLS.id) {
                    it = refStq.erase(it);
                    break;
                }
                it++;
            }
        }
        refTrace.entry[bid].dequeLSInfo();
    }
    uint64_t getEcallData(uint64_t bpc, uint64_t tpc, uint64_t bid) {
        for (uint32_t k = 0; k < ecall_data.size(); k++) {
            if (bpc == ecall_data[k].bpc && ecall_data[k].bid == bid) {
                ecall_data[k].bid = bid;
                for (uint32_t i = 0; i < ecall_data[k].tpc.size(); i++) {
                    if (ecall_data[k].bpc == bpc && ecall_data[k].tpc[i] == tpc) {
                        return ecall_data[k].src[i];
                    }
                }
            }
        }
        return uint64_t(-1);
    }
    void retireEcall(uint64_t bpc) {
        ASSERT (bpc == ecall_data[0].bpc);
        ecall_data.erase(ecall_data.begin());
        return;
    }
    size_t getEcallInstCount(uint64_t bpc) {
        uint64_t fixCount = 25;
        uint64_t fixCountCkpt = 24;
        if (ckpt_file) {
            fixCount = fixCountCkpt;
        }
        for (uint32_t i = 0; i < ecall_data.size(); i++) {
            if (bpc == ecall_data[i].bpc) {
                size_t i_sz = (ecall_data[i].tpc.size() - fixCount) / 2 * 3 + fixCount;
                return i_sz;
            }
        }
        return 0;
    }
};


// Macros for printing & debugging
#define VERBOSE_ON (GetSim()->verboseON)
#define VERBOSE2_ON (GetSim()->verboseON2)
#define DEBUG_VERBOSE_ON (GetSim()->DebugVerboseON)

#define STAGE_ENABLED(A) (GetSim()->verboseSwitch.find(A) != GetSim()->verboseSwitch.end())
#define STAGE_ENABLED2(A) (GetSim()->verboseSwitch2.find(A) != GetSim()->verboseSwitch2.end())

} // namespace JCore
#endif
