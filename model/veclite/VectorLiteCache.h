#ifndef BLOCKISA_MODEL_VECLITE_CACHE_H
#define BLOCKISA_MODEL_VECLITE_CACHE_H
#include <cstdint>
#include <map>
#include <vector>

#include "core/Bus.h"
#include "ISACommon/TileOpManager.h"
#include "ModelCommon/ROBID.h"
#include "ModelCommon/ModelCommon.h"
#include "interface/TileRegVecLdRes.h"
#include "interface/VecTileRegLdReq.h"
#include "interface/VecTileRegStReq.h"

namespace JCore {
// 数据类型定义
using DataBlock = std::vector<uint8_t>;  // 256字节的数据块
using DataVector = std::vector<DataBlock>;  // 2KB -> 8个数据块的向量

// Entry状态枚举
enum class EntryState {
    INVALID,      // 无效状态
    DATA_READY,   // 数据就绪
    DATA_REUSE    // 数据重用
};

// Entry结构体
struct CacheEntry {
    bool valid = false; // 是否被占用
    uint32_t entryId = 0; // Entry ID
    ROBID   bid = ROBID();
    uint32_t tileTagId = 0; // Tag ID
    OperandType handType = OperandType::OPD_INVALID;
    uint32_t tileSize = 0;
    uint32_t filledBlocks = 0; // 已填充的数据块数量 (bitmask)
    uint64_t allocateCycle = 0; // 分配时间
    OperandType tileType = OperandType::OPD_INVALID;
    DataVector data; // 数据向量，大小为data_block_count
    uint64_t address = 0; // Tile起始地址
    EntryState state = EntryState::INVALID;  // 状态机状态

    CacheEntry() = default;
};

class VectorLite;
class VectorLiteCache {
public:
    VectorLiteCache();
    ~VectorLiteCache();

    // 禁用拷贝构造和赋值操作
    VectorLiteCache(const VectorLiteCache&) = delete;
    VectorLiteCache& operator=(const VectorLiteCache&) = delete;

    VectorLite*                         top = nullptr;
    void                                Xfer() const;
    void                                Work();
    void                                Build();
    bool                                CheckStall() const;
    SimSys*                             GetSim();
    void                                SetSim(SimSys *s);

    void                                SetFlush(const FlushBus &bus) const;
    bool                                IsIdle();
    void                                Stats();

    bool AllocateEntry(uint32_t tileTagId, OperandType handType, ROBID bid, uint32_t tileSize, uint64_t address);

    bool FillData(uint32_t tileTagId, ROBID bid, uint64_t address, const DataBlock& data);
    // 功能2: 存储数据到dst buffer中的可用entry (TO)
    bool StoreData(uint32_t tileTagId, uint64_t address, const DataVector& data);
    void ProcessStoreData();
    // 获取entry的完整数据用于发送
    bool GetEntryData(uint32_t entryId, DataVector& outData);

    bool ClearEntry(uint32_t tileTagId);

    // 功能4: 根据tag id读取2KB数据
    void HandleData();
    bool ReadData(uint32_t tileTagId, DataVector& outData);

    bool IsSrcReady(uint32_t tileTagId);

    size_t GetUsedEntryCount() const;
    size_t GetUsedDstEntryCount() const;
    void PrintBufferStatus() const;
    bool UpdateEntryState(uint32_t tileTagId, EntryState newState);

    INNER SimQueue<TileRegVecLdRes>     *tileRegLdResQ;
    INNER SimQueue<TileRegVecLdRes>     *tileRegWrRspQ;
    INNER SimQueue<VecTileRegLdReq>     *tileRegLdReqQ;
    INNER SimQueue<VecTileRegStReq>     *tileRegStReqQ;

    INPUT SimQueue<ScbDrainBus>         *drainScbDataQ;
    OUTPUT SimQueue<ScbDrainBus>        *drainScbRespQ;

private:
    SimSys*                             sim = nullptr;
    uint64_t writeRegTimes = 0;     // 当前cycle计数
    // Store请求状态枚举
    enum class StoreRequestState {
        PENDING,        // 待发送，尚未开始发送任何block
        SENDING,        // 正在发送中，已发送部分block
        WAITING_RESP,   // 所有block已发送完成，等待响应
        COMPLETED       // 所有响应已收到，可以释放
    };

    // Store请求结构体（增强版）
    struct StoreRequest {
        uint32_t tileTagId;           // entry的tag id
        uint32_t entryId;         // entry在buffer中的索引
        uint64_t address;          // 基地址
        uint64_t writeCyscle;       // 写入时间戳，用于age排序
        uint32_t sendReqNum;       // 已发送的请求数量 (0-8)
        uint32_t receiveRespNum;   // 已收到的响应数量 (0-8)
        StoreRequestState state;   // 当前状态

        // 记录每个block的请求ID，用于匹配响应
        std::vector<uint32_t> blockReqIds;  // 每个block对应的请求ID，大小=8

        StoreRequest()
            : tileTagId(0), entryId(0), address(0), writeCyscle(0),
              sendReqNum(0), receiveRespNum(0),
              state(StoreRequestState::PENDING)
        {
            blockReqIds.resize(8, 0);
        }

        // 检查是否所有block都已发送
        bool AllBlocksSent() const
        {
            return sendReqNum >= 8;
        }

        // 检查是否所有响应都已收到
        bool AllResponsesReceived() const
        {
            return receiveRespNum >= 8;
        }

        // 检查是否还有未发送的block
        bool HasUnsentBlocks() const
        {
            return sendReqNum < 8;
        }

        // 获取下一个要发送的block索引
        uint32_t GetNextBlockIndex() const
        {
            return sendReqNum;  // 按顺序发送
        }

        // 记录响应
        void RecordResponse(uint32_t reqId)
        {
            // 查找对应的block索引
            for (size_t i = 0; i < blockReqIds.size(); ++i) {
                if (blockReqIds[i] == reqId) {
                    receiveRespNum++;
                    break;
                }
            }
        }
    };
    using StoreRequestT = std::shared_ptr<StoreRequest>;

    // 存储待处理的store请求队列（按age排序）
    std::vector<StoreRequestT> pendingStores;
    void                                ReciveDrainData() const;
    bool                                HandleDrainData(const ScbDrainBus &bus) const;

    int32_t FindAvailableEntry();
    int32_t FindAvailableDstEntry();

    int32_t FindEntryByTag(uint32_t tileTagId) const;
    int32_t FindEntryInDstByTag(uint32_t tileTagId) const;

    // 验证数据向量大小
    bool ValidateDataVector(const DataVector& data) const;

    // 清除指定buffer中的entry
    void ClearEntry(CacheEntry& entry) const;

    std::vector<CacheEntry> bufferSrcs;  // src buffer (统一管理TA/TB/TC)
    std::vector<CacheEntry> bufferDsts;  // dst buffer (TO)

    bool SendStoreRequest(const StoreRequestT& req);
    void HandleStoreResponse();
    bool GetEntryBlockData(uint32_t entryId, uint32_t blockIdx, DataVector& outData);
    void UpdateEntryTimes(uint32_t entryId, uint64_t times);
    void RemoveCompletedStore();
    void PrintPendingStores() const;

    uint32_t bufferEntryCnt = 0;      // 每个buffer的entry数量
    uint32_t dataBlockCnt = 0;        // 数据块数量
    uint32_t dataBlockSize = 0;       // 每个数据块大小
};
} // namespace JCore

#endif
