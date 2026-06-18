#include "veclite/VectorLiteCache.h"
#include "veclite/VectorLite.h"
#include "core/Bus.h"

namespace JCore {

VectorLiteCache::VectorLiteCache() {}
VectorLiteCache::~VectorLiteCache() {}

void VectorLiteCache::Build()
{
    bufferEntryCnt = top->GetConfig().tileBufferCout;
    dataBlockCnt = top->GetConfig().dataBlockCount;
    dataBlockSize = top->GetConfig().dataBlockSize;
    SetSim(top->GetSim());

    bufferSrcs.resize(bufferEntryCnt);
    for (uint32_t j = 0; j < bufferEntryCnt; ++j) {
        bufferSrcs[j].entryId = j;
        bufferSrcs[j].data.resize(dataBlockCnt);
        for (uint32_t k = 0; k < dataBlockCnt; ++k) {
            bufferSrcs[j].data[k].resize(dataBlockSize);
        }
    }

    // 初始化dst buffer (TO)
    bufferDsts.resize(bufferEntryCnt);
    for (uint32_t j = 0; j < bufferEntryCnt; ++j) {
        bufferDsts[j].entryId = j;
        bufferDsts[j].data.resize(dataBlockCnt);
        for (uint32_t k = 0; k < dataBlockCnt; ++k) {
            bufferDsts[j].data[k].resize(dataBlockSize);
        }
    }
}

bool VectorLiteCache::CheckStall() const
{
    return false;
}

void VectorLiteCache::SetSim(SimSys *s)
{
    sim = s;
}

void VectorLiteCache::Work()
{
    HandleData();
    ProcessStoreData();
    ReciveDrainData();
}

void VectorLiteCache::Xfer() const
{
}

void VectorLiteCache::SetFlush(const FlushBus &bus) const
{
    (void)bus;
}

int32_t VectorLiteCache::FindAvailableEntry()
{
    for (size_t i = 0; i < bufferSrcs.size(); ++i) {
        if (!bufferSrcs[i].valid) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

int32_t VectorLiteCache::FindAvailableDstEntry()
{
    for (size_t i = 0; i < bufferDsts.size(); ++i) {
        if (!bufferDsts[i].valid) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

int32_t VectorLiteCache::FindEntryByTag(uint32_t tileTagId) const
{
    for (size_t i = 0; i < bufferSrcs.size(); ++i) {
        if (bufferSrcs[i].valid && bufferSrcs[i].tileTagId == tileTagId) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

// 根据tag id查找entry (dst tile buffer)
int32_t VectorLiteCache::FindEntryInDstByTag(uint32_t tileTagId) const
{
    for (size_t i = 0; i < bufferDsts.size(); ++i) {
        if (bufferDsts[i].valid && bufferDsts[i].tileTagId == tileTagId) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

bool VectorLiteCache::AllocateEntry(uint32_t tileTagId, OperandType handType, ROBID bid,
    uint32_t tileSize, uint64_t address)
{
    // 检查tag_id是否已存在
    int32_t existingIndex = FindEntryByTag(tileTagId);
    if (existingIndex >= 0) {
        std::cerr << "Warning: Tag ID " << std::dec << tileTagId << " with handType " <<
            static_cast<uint32_t>(handType) << " already exists in src buffer with handType " <<
            static_cast<uint32_t>(bufferSrcs[existingIndex].handType) << std::endl;
        ASSERT(false);
    }

    // 查找可用entry
    int32_t availIndex = FindAvailableEntry();
    if (availIndex < 0) {
        return false;
    }

    // 分配entry
    auto& entry = bufferSrcs[availIndex];
    entry.valid = true;
    entry.entryId = availIndex;
    entry.bid = bid;
    entry.tileTagId = tileTagId;
    entry.handType = handType;
    entry.tileSize = tileSize;
    entry.filledBlocks = 0;
    entry.allocateCycle = writeRegTimes++;
    entry.address = address;
    entry.state = EntryState::INVALID;  // 初始状态为INVALID，等待数据填充

    // 清空数据
    for (size_t i = 0; i < dataBlockCnt; ++i) {
        std::fill(entry.data[i].begin(), entry.data[i].end(), 0);
    }

    return true;
}

bool VectorLiteCache::FillData(uint32_t tileTagId, ROBID bid,
    uint64_t address, const DataBlock& data)
{
    (void)bid;
    // 验证数据块大小
    if (data.size() != dataBlockSize) {
        std::cerr << "Error: Data block size mismatch. Expected: " << dataBlockSize
                  << ", Got: " << data.size() << std::endl;
        ASSERT(false);
    }

    // 查找entry
    int32_t entryIndex = FindEntryByTag(tileTagId);
    if (entryIndex < 0) {
        std::cerr << "Error: Tag ID " << tileTagId << " not found in src buffer" << std::endl;
        ASSERT(false);
    }

    auto& entry = bufferSrcs[entryIndex];
    // 通过地址偏移计算block索引
    uint64_t baseAddr = entry.address;
    uint64_t offset = address - baseAddr;

    // 检查地址是否在有效范围内
    if (offset >= entry.tileSize) {
        std::cerr << "Error: Address 0x" << std::hex << address
                  << " is out of range [0x" << baseAddr << ", 0x"
                  << (baseAddr + entry.tileSize) << ")" << std::dec << std::endl;
        ASSERT(false);
    }

    // 计算block索引
    uint32_t blockIndex = offset / dataBlockSize;

    // 验证block索引是否有效
    if (blockIndex >= dataBlockCnt) {
        std::cerr << "Error: Invalid block index " << blockIndex
                  << " (offset: " << offset << ", block_size: " << dataBlockSize << ")" << std::endl;
        ASSERT(false);
    }

    // 检查该block是否已经被填充
    if ((entry.filledBlocks & (1 << blockIndex)) != 0) {
        std::cerr << "Warning: Block " << blockIndex << " for tag " << tileTagId
                  << " (addr: 0x" << std::hex << address << std::dec << ") is already filled" << std::endl;
        ASSERT(false);
    }

    // 验证地址是否对齐到block边界
    if ((address - baseAddr) % dataBlockSize != 0) {
        std::cerr << "Warning: Address 0x" << std::hex << address
                  << " is not aligned to block size " << dataBlockSize << std::dec << std::endl;
    }

    // 填充数据到指定的block
    std::memcpy(entry.data[blockIndex].data(), data.data(), dataBlockSize);

    // 标记该block已填充
    entry.filledBlocks |= (1 << blockIndex);

    // 检查是否所有block都已填充完成
    uint32_t expectedBlocks = (entry.tileSize + dataBlockSize - 1) / dataBlockSize;
    uint32_t filledCount = 0;
    for (uint32_t i = 0; i < dataBlockCnt; ++i) {
        if ((entry.filledBlocks & (1 << i)) != 0) {
            filledCount++;
        }
    }

    if (filledCount >= expectedBlocks) {
        // 所有数据已填充完成，更新状态为DATA_READY
        entry.state = EntryState::DATA_READY;
    }

    return true;
}

bool VectorLiteCache::IsSrcReady(uint32_t tileTagId)
{
    int32_t index = FindEntryByTag(tileTagId);
    if (index < 0) {
        return false;
    }

    auto& entry = bufferSrcs[index];
    return (entry.valid && entry.state == EntryState::DATA_READY);
}

void VectorLiteCache::HandleData()
{
    if (!top->pTileUtils->IsRingOrXbarMode(false)) {
        while (!tileRegLdResQ->Empty()) {
            auto resp = tileRegLdResQ->Read();

            // 获取响应中的地址和数据
            uint64_t address = resp.GetAddr();
            const uint8_t* refillData = resp.GetData();

            uint32_t tileTagId = resp.GetTagId();
            ROBID bid = resp.GetBid();

            // 检查entry是否已存在
            int32_t entryIndex = FindEntryByTag(tileTagId);
            if (entryIndex < 0) {
                std::cerr << "Error: Entry not allocated for tileTagId: " << tileTagId << std::endl;
                ASSERT(false);
                continue;
            }

            DataBlock blockData(refillData, refillData + MAX_TILE_DATA_BYTE);
            // 调用FillData填充单个block
            bool fillSuaccelss = FillData(tileTagId, bid, address, blockData);
            if (!fillSuaccelss) {
                ASSERT(false);
                break;
            }
        }
    }
}

bool VectorLiteCache::ClearEntry(uint32_t tileTagId)
{
    bool found = false;

    // 释放 src tile buffer
    int32_t index = FindEntryByTag(tileTagId);
    if (index >= 0) {
        printf("Clear entry, tag=%u, index=%u\n", tileTagId, index);
        ClearEntry(bufferSrcs[index]);
        found = true;
    }

    if (!found) {
        std::cerr << "Warning: Tag ID " << tileTagId << " not found in any buffer" << std::endl;
        ASSERT(false);
    }

    return found;
}

bool VectorLiteCache::ReadData(uint32_t tileTagId, DataVector& outData)
{
    // 在src buffer中查找
    int32_t index = FindEntryByTag(tileTagId);
    if (index >= 0) {
        auto& entry = bufferSrcs[index];
        if (entry.state != EntryState::DATA_READY) {
            ASSERT(false);
        }

        // 准备输出数据
        outData.resize(dataBlockCnt);
        for (size_t i = 0; i < dataBlockCnt; ++i) {
            outData[i].resize(dataBlockSize);
            std::memcpy(outData[i].data(), entry.data[i].data(), dataBlockSize);
        }
        return true;
    }

    // 在dst buffer中查找
    int32_t dstIndex = FindEntryInDstByTag(tileTagId);
    if (dstIndex >= 0) {
        auto& entry = bufferDsts[dstIndex];

        // 准备输出数据
        outData.resize(dataBlockCnt);
        for (size_t i = 0; i < dataBlockCnt; ++i) {
            outData[i].resize(dataBlockSize);
            std::memcpy(outData[i].data(), entry.data[i].data(), dataBlockSize);
        }
        return true;
    }

    return false;
}

size_t VectorLiteCache::GetUsedEntryCount() const
{
    size_t count = 0;
    for (const auto& entry : bufferSrcs) {
        if (entry.valid) {
            count++;
        }
    }
    return count;
}

// 获取已使用entry数量 (dst tile buffer)
size_t VectorLiteCache::GetUsedDstEntryCount() const
{
    size_t count = 0;
    for (const auto& entry : bufferDsts) {
        if (entry.valid) {
            count++;
        }
    }
    return count;
}

void VectorLiteCache::PrintBufferStatus() const
{
    std::cout << "=== VectorLiteCache Status ===" << std::endl;
    std::cout << "Configuration: " << bufferEntryCnt << " entries, "
              << dataBlockCnt << " blocks x "
              << dataBlockSize << " bytes" << std::endl;

    // 打印src tile buffer
    size_t used = GetUsedEntryCount();
    std::cout << "Src Buffer: ";
    for (const auto& entry : bufferSrcs) {
        if (entry.valid) {
            std::cout << "[Tag:" << entry.tileTagId << " State:";
            switch (entry.state) {
                case EntryState::DATA_READY: std::cout << "READY"; break;
                case EntryState::DATA_REUSE: std::cout << "REUSE"; break;
                default: std::cout << "INVALID"; break;
            }
            std::cout << "] ";
        }
    }
    std::cout << "(Used: " << used << "/" << bufferEntryCnt << ")" << std::endl;

    // 打印dst tile buffer
    size_t dstUsed = GetUsedDstEntryCount();
    std::cout << "Dst Buffer: ";
    for (const auto& entry : bufferDsts) {
        if (entry.valid) {
            std::cout << "[Tag:" << entry.tileTagId << " State:";
            switch (entry.state) {
                case EntryState::DATA_READY: std::cout << "READY"; break;
                case EntryState::DATA_REUSE: std::cout << "REUSE"; break;
                default: std::cout << "INVALID"; break;
            }
            std::cout << "] ";
        }
    }
    std::cout << "(Used: " << dstUsed << "/" << bufferEntryCnt << ")" << std::endl;
}

void VectorLiteCache::ClearEntry(CacheEntry& entry) const
{
    entry.valid = false;
    entry.tileTagId = 0;
    entry.address = 0;
    entry.state = EntryState::INVALID;
    entry.tileType = OperandType::OPD_INVALID;
    entry.filledBlocks = 0;
    entry.allocateCycle = 0;
    entry.bid = ROBID();
    entry.tileSize = 0;

    // 清空数据
    for (auto& dataBlock : entry.data) {
        std::fill(dataBlock.begin(), dataBlock.end(), 0);
    }
}

bool VectorLiteCache::UpdateEntryState(uint32_t tileTagId, EntryState newState)
{
    int32_t index = FindEntryByTag(tileTagId);
    if (index < 0) {
        std::cerr << "Error: Tag ID " << tileTagId << " not found in src buffer" << std::endl;
        return false;
    }

    bufferSrcs[index].state = newState;
    return true;
}

void VectorLiteCache::ProcessStoreData()
{
    // 1. 处理写响应
    HandleStoreResponse();

    // 2. 按age排序pendingStores（只处理PENDING和SENDING状态的）
    std::vector<StoreRequestT> activeRequests;
    for (auto& req : pendingStores) {
        if (req->state == StoreRequestState::PENDING ||
            req->state == StoreRequestState::SENDING) {
            activeRequests.push_back(req);
        }
    }
    // 按写入时间排序
    std::sort(activeRequests.begin(), activeRequests.end(),
        [](const StoreRequestT& a, const StoreRequestT& b) {
        return a->writeCyscle < b->writeCyscle;
    });
    // 3. 发送新的store请求（按age顺序，每cycle最多st_band个）
    uint32_t sentCount = 0;
    for (auto& req : activeRequests) {
        if (sentCount >= 1) {
            break;
        }

        // 只处理未完成所有block发送的请求
        if ((req->state == StoreRequestState::PENDING) ||
            ((req->state == StoreRequestState::SENDING) && req->HasUnsentBlocks())) {
            // 更新状态为SENDING
            if (req->state == StoreRequestState::PENDING) {
                req->state = StoreRequestState::SENDING;
            }

            // 发送当前block
            if (SendStoreRequest(req)) {
                sentCount++;
            } else {
                // 非ring下不会在这
                break;
            }

            // 检查是否所有block都已发送完成
            if (req->AllBlocksSent()) {
                req->state = StoreRequestState::WAITING_RESP;
            }
        }
    }

    // 4. 移除已完成的请求
    RemoveCompletedStore();
}

// 发送store请求到下游
bool VectorLiteCache::SendStoreRequest(const StoreRequestT& req)
{
    // 获取下一个要发送的block索引
    uint32_t blockIdx = req->GetNextBlockIndex();
    // 检查是否还有未发送的block
    if (blockIdx >= dataBlockCnt) {
        return false;
    }
    // 获取该block的数据
    DataVector blockData;
    if (!GetEntryBlockData(req->entryId, blockIdx, blockData)) {
        std::cerr << "Error: Failed to get block data for entry " << req->entryId
                  << ", block " << blockIdx << std::endl;
        return false;
    }
    // 准备请求数据
    uint64_t targetAddr = req->address + (blockIdx * dataBlockSize);
    uint32_t dataSize = dataBlockSize;
    VecTileRegStReq stReq;
    uint32_t reqId = top->tileReqId++;
    stReq.SetReqId(reqId);
    stReq.SetDest(targetAddr);
    stReq.SetData(blockData[0].data(), dataSize);
    stReq.SetSize(dataSize);
    DataMask mask = DataMask(1);
    stReq.SetDataMask(mask);
    tileRegStReqQ->Write(stReq);
    // 记录请求ID和block的对应关系
    req->blockReqIds[blockIdx] = reqId;
    req->sendReqNum++;
    return true;
}

// 处理store响应
void VectorLiteCache::HandleStoreResponse()
{
    while (!tileRegWrRspQ->Empty()) {
        auto resp = tileRegWrRspQ->Read();
        uint32_t respId = resp.GetRespId();

        // 查找对应的pending request
        bool found = false;
        for (auto& pending : pendingStores) {
            // 只处理WAITING_RESP状态的请求
            if (pending->state != StoreRequestState::WAITING_RESP) {
                continue;
            }
            // 查找匹配的请求ID
            for (size_t i = 0; i < pending->blockReqIds.size(); ++i) {
                if (pending->blockReqIds[i] != respId) {
                    continue;
                }
                pending->receiveRespNum++;
                found = true;

                // 检查是否所有响应都已收到
                if (pending->AllResponsesReceived()) {
                    pending->state = StoreRequestState::COMPLETED;
                }
                break;
            }
            if (found) {
                break;
            }
        }

        if (!found) {
            std::cerr << "Warning: Received response for unknown request ID: " << respId << std::endl;
        }
    }
}

// 从pendingStores中移除已完成的请求
void VectorLiteCache::RemoveCompletedStore()
{
    auto it = pendingStores.begin();
    while (it != pendingStores.end()) {
        auto req = *it;
        if (req->state == StoreRequestState::COMPLETED) {
            // 释放entry
            printf("Remove entry, tag=%u, index=%u\n", bufferDsts[req->entryId].tileTagId, req->entryId);
            ClearEntry(bufferDsts[req->entryId]);
            it = pendingStores.erase(it);
        } else {
            ++it;
        }
    }
}

// 获取entry中指定block的数据
bool VectorLiteCache::GetEntryBlockData(uint32_t entryId, uint32_t blockIdx,
    DataVector& outData)
{
    if (entryId >= bufferEntryCnt || blockIdx >= dataBlockCnt) {
        return false;
    }

    auto& entry = bufferDsts[entryId];
    if (!entry.valid) {
        return false;
    }

    // 返回单个block的数据
    outData.resize(1);
    outData[0].resize(dataBlockSize);
    std::memcpy(outData[0].data(), entry.data[blockIdx].data(), dataBlockSize);

    return true;
}

void VectorLiteCache::ReciveDrainData() const
{
    while (!drainScbDataQ->Empty()) {
        ScbDrainBus bus = drainScbDataQ->Front();
        if (!HandleDrainData(bus)) {
            break;
        }
        drainScbDataQ->Read();
    }
}

bool VectorLiteCache::HandleDrainData(const ScbDrainBus &bus) const
{
    // TODO: check for stall
    drainScbRespQ->Write(bus);
    return true;
}
}
