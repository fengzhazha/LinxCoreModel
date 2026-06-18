#pragma once

#include <cstdint>
#include <functional>
#include <iostream>
#include <queue>
#include <string>
#include <vector>

#include "ISA.h"
#include "ModelCommon/ModelEnumDefines.h"

#define MAX_CACHE_DOT_CNT (24)

namespace JCore {

enum class InstTypeBit {
    TYPE_VGATHER = 0x1,
};

struct GFUMemReq {
    bool                    vld;
    /* \brief blsu req handle for indexing to blsu req */
    uint32_t                lsuHandle;
    LSUType                 lsuTypeId = LSUType::SCALAR_LSU;
    /* \brief 64bit Physical Address */
    uint64_t                addr;
    /* \brief Token ID for this request, used for memory return */
    uint32_t                tid;
    /* \brief miss queue ID for this request, used for memory return */
    uint32_t                missQid;
    /* \brief Size of memory request, ranging from 1,2,4,8,16,32,64 */
    uint32_t                size;
    /* \brief Read or write request */
    int                     is_store = 0;
    /* \brief whether is inst fetch */
    bool                    is_inst = false;
    /* \brief Whether this request is prefetch */
    bool                    prefetch;
    uint32_t                instTypeMap = 0; // InstTypeBit bitmap
    uint32_t                bpqID;
    /* \brief Request data for load return and write evict */
    union {
        uint64_t                data[32];
        uint8_t                 data8b[256];
    };
    /* \brief Block BF1 stage fetch cycle */
    uint64_t                fetchCycle;
    /* \brief Data Load/Store Request Latency Statistic */
    uint64_t                requestCycle;
    uint64_t                bitMask[4] = {0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
                                          0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF};
    /* \brief atomic instruction */
    bool                    is_llsc;
    /* \brief sc result return */
    uint64_t                llsc;
    /* \brief need address translation or not */
    bool                    needAddrMap = true;
    /* \brief Aaccelpt the request */
    uint64_t                socAaccelptCycle = 0;
    uint64_t                busSendCycle = 0;
    uint64_t                receiveDBIDCycle = 0;
    /* \brief Return the request */
    uint64_t                socReturnCycle = 0;
    bool                    bypassL2 = false;

    bool                    isCacheable;
    /* \brief is cache snoop send to cpu */
    bool                    isSnoop = false;
    bool                    transferReq = false;
    bool                    vCoreReq = false;

  union {
        uint32_t          usrDefRsv[34]; // user self defined  reserved filed

        struct {
            uint32_t      transIdReq;   // the value is accumulated every time when rcv a req from up_level
            uint32_t      transId;      // the value is accumulated every time a request is sent 
            uint32_t      globalCoreId; // used for CA AXI Atomic ID

            uint8_t       dataType;
            uint8_t       cacheType;
            uint8_t       bankId;
            uint8_t       l2HitInd; // 请求是否在L2 hit

            uint32_t      pktId;

            uint8_t       cmd; // MemCmd
            uint8_t       l2MissTrcInd;  // L2 miss时，开启跟踪指示
            uint16_t      ldWtConflictCnt; // B类数据 Load / store(write_throught)冲突次数        

            uint32_t      dotCyc[MAX_CACHE_DOT_CNT]; // 时延打点，记录数据包到达每个处理流程的时刻(单位cycle)
            uint64_t      reqId; // request id
            uint8_t       l1HitInd;
            uint8_t       direct2L3;
            uint32_t      responseCycle;
            uint8_t       rsv[2];
        };
    };
};


} // namespace JCore
