#ifndef  S_TILE_REGISTER_H
#define S_TILE_REGISTER_H

#include <vector>

#include "../configs/tmu_config.h"
#include "core/Bus.h"
#include "interface/ConstConfig.h"
#include "interface/InterfaceCommon.h"
#include "tmu/ring/RingNodeObj.h"
#include "tmu/ring/CrossStation.h"
#include "tmu/TileRegStat.h"
#include "tmu/StashCtrlUnit.h"

namespace JCore {

enum class TilePriorityState {
    VEC_CUBE_TTRANS = 0,
    TTRANS_VEC_CUBE,
    CUBE_TTANS_VEC,
};
enum class DataPipeState {
    STASH_OTHERS = 0,
    OTHERS_STASH,
};
class TileRegBank {
public:
    uint8_t                                 Load(uint32_t rowIdx, uint32_t colIdx);
    void                                    Store(uint32_t rowIdx, uint32_t colIdx, uint8_t data);
    bool                                    IsAaccelssed();
    void                                    SetAaccelssState(bool aaccelss);
    uint32_t                                GetColIdx(uint32_t addrBits);
    TileRegBank() : aaccelssed(false) {}
    TileRegBank(uint32_t size, uint32_t elementSize)
        : aaccelssed(false), intervel(elementSize), size(size),
          mem(size/elementSize, std::vector<uint8_t>(elementSize/BITS_IN_BYTE, 0)) {}
private:
    bool                                    aaccelssed = false;
    uint32_t                                intervel = 0;
    uint32_t                                size = 0;
    std::vector<std::vector<uint8_t>>       mem;
};

struct StashLdReq {
public:
    uint32_t GetRemainSize() const { return req->GetSize() - (sendIndex + 1) * 256; }
    uint32_t GetAddr() {
        uint32_t addr = req->GetAddr() + sendIndex * 256;
        sendIndex++;
        return addr;
    }
    unsigned GetStation() {return stationId;}
    void SetStation(unsigned id) {stationId = id;}
public:
    StashLdReq(std::shared_ptr<Request> request) : req(request), sendIndex(0), stationId(0) {}
    std::shared_ptr<Request> req;
    uint32_t sendIndex = 0;
    unsigned stationId;
};
struct StashLdResp {
public:
    unsigned GetStation() {return stationId;}
    void SetStation(unsigned id) {stationId = id;}
public:
    StashLdResp(std::shared_ptr<Request> request, unsigned id) : req(request), stationId(id) {}
    std::shared_ptr<Request> req;
    unsigned stationId;
};

class TileRegister : public RingNodeObj {
public:
    TileRegister() {}
    virtual ~TileRegister() {}
    // parameter configuration
    TMUConfig                                configs;
    TileUtils                               *pTileUtils;
    std::shared_ptr<StashCtrlUnit>               stashCtrlUnit;
    TileRegStat                             stat;
    // IO ports;
    INPUT std::vector<SimQueue<CubeTileRegLdReq>*>        cubeTileRegLdReqArray;
    INPUT std::vector<SimQueue<CubeTileRegStReq>*>        cubeTileRegStReqArray;
    INPUT std::vector<SimQueue<TTransTileRegLdReq>*>      tTransTileRegLdReqArray;
    INPUT std::vector<SimQueue<TTransTileRegStReq>*>      tTransTileRegStReqArray;
    INPUT std::vector<SimQueue<VecTileRegLdReq>*>         vecTileRegLdReqArray;
    INPUT std::vector<SimQueue<VecTileRegStReq>*>         vecTileRegStReqArray;
    INPUT std::vector<SimQueue<VecTileRegLdReq>*>         mtcTileRegLdReqArray;
    INPUT std::vector<SimQueue<VecTileRegStReq>*>         mtcTileRegStReqArray;
    INPUT std::vector<SimQueue<TTransTileRegLdReq>*>      bridgeTileRegLdReqArray;
    INPUT std::vector<SimQueue<TTransTileRegStReq>*>      bridgeTileRegStReqArray;

    OUTPUT std::vector<SimQueue<TileRegCubeLdRes>*>       tileRegCubeLdResArray;
    OUTPUT std::vector<SimQueue<TileRegCubeLdRetry>*>     tileRegCubeLdRetryArray;
    OUTPUT std::vector<SimQueue<TileRegCubeStRetry>*>     tileRegCubeStRetryArray;
    OUTPUT std::vector<SimQueue<TileRegCubeLdRes>*>       tileRegCubeWrResArray;
    OUTPUT std::vector<SimQueue<TileRegTTransLdRes>*>     tileRegTTransLdResArray;
    OUTPUT std::vector<SimQueue<TileRegTTransLdRetry>*>   tileRegTTransLdRetryArray;
    OUTPUT std::vector<SimQueue<TileRegTTransStRetry>*>   tileRegTTransStRetryArray;
    OUTPUT std::vector<SimQueue<TileRegVecLdRes>*>        tileRegVecLdResArray;
    OUTPUT std::vector<SimQueue<TileRegVecLdRes>*>        tileRegVecWrResArray;
    OUTPUT std::vector<SimQueue<TileRegVecLdRetry>*>      tileRegVecLdRetryArray;
    OUTPUT std::vector<SimQueue<TileRegVecStRetry>*>      tileRegVecStRetryArray;

    OUTPUT std::vector<SimQueue<TileRegVecLdRes>*>        tileRegMtcLdResArray;
    OUTPUT std::vector<SimQueue<TileRegVecLdRes>*>        tileRegMtcWrResArray;
    OUTPUT std::vector<SimQueue<TileRegVecLdRetry>*>      tileRegMtcLdRetryArray;
    OUTPUT std::vector<SimQueue<TileRegVecStRetry>*>      tileRegMtcStRetryArray;

    OUTPUT std::vector<SimQueue<TileRegTTransLdRes>*>     tileRegBridgeLdResArray;
    OUTPUT std::vector<SimQueue<TileRegTTransLdRetry>*>   tileRegBridgeLdRetryArray;
    OUTPUT std::vector<SimQueue<TileRegTTransLdRes>*>     tileRegBridgeWrResArray;
    OUTPUT std::vector<SimQueue<TileRegTTransStRetry>*>   tileRegBridgeStRetryArray;

    SimSys                                  *sim;
    SimSys                                  *GetSim();
    void                                    Build();
    void                                    AddNodeForStashCtrl(std::shared_ptr<CrossStation> station);
    void                                    Reset();
    void                                    Work();
    void                                    Xfer();
    void ReportStat() override {}
    void                                    SetShowLdStCntOn()
    {
        lsGraphOn = true;
    }
    bool verbose = false;
    void GetTileRegData(TileOperandPtr &tileInfo, std::vector<uint64_t> &data, uint32_t stid);
    void SetZeroData(uint32_t addr, uint32_t size, uint32_t stid);
protected:
    bool                                    lsGraphOn = false;
    uint32_t                                curRenameAddr = 0;
    uint32_t                                ptr;
    uint32_t                                restSize;
    TilePriorityState                       priorityState;
    DataPipeState                           dataPipePriority;
    uint32_t                                colNum;
    uint32_t                                rowNum;
    uint32_t                                maxAddr = 0UL;
    uint64_t                                loadThreadID;
    uint32_t                                preLoadByte;
    uint32_t                                loadByte;
    uint64_t                                storeThreadID;
    uint32_t                                preStoreByte;
    uint32_t                                storeByte;
    uint32_t                                spbWrieNum;
    uint32_t                                tmuOtherPriorityNum;
    std::vector<std::vector<TileRegBank>>   banks;
    std::vector<int>                        stashRR;
    /* \brief Get bank index in row */
    uint32_t                                GetBankRowIdx(uint32_t addr);
    /* \brief Get bank index in colnumn */
    uint32_t                                GetBankColIdx(uint32_t addr);
    /* \brief Get bank index in one-dimensional space order */
    uint32_t                                GetBankIdx(uint32_t addr, uint32_t stid);
    /* \brief Get row index in bank */
    uint32_t                                GetSegmentIdxInBank(uint32_t addr);
    /* \brief Get bank base address at current bank segmentation */
    uint32_t                                AlignToBankAddr(uint32_t addr);
    /* \brief Get bank base address at next bank segmentation */
    uint32_t                                AlignToNextBankAddr(uint32_t addr);
    /* \brief Check if bank conflict on read or write */
    bool                                    BankConflict(uint32_t addr, uint32_t size, uint32_t stid);
    /* \brief Load data from register */
    uint8_t*                                LoadData(uint32_t addr, uint32_t size, uint32_t stid);
    /* \brief Store data to register */
    void                                    StoreData(uint32_t addr, uint32_t size, uint8_t* data, DataMask mask,
                                                uint32_t stid);
    /* \brief Handle request from vector core */
    void                                    HandleVecRequest();
    /* \brief Handle write request from vector core */
    void                                    HandleVecWriteRequest();
    /* \brief Handle read request from vector core */
    void                                    HandleVecReadRequest();

    void                                    HandleMtcRequest();
    /* \brief Handle write request from vector core */
    void                                    HandleMtcWriteRequest();
    /* \brief Handle read request from vector core */
    void                                    HandleMtcReadRequest();

    /* \brief Handle request from cube core */
    void                                    HandleCubeRequest();
    /* \brief Handle write request from cube core */
    void                                    HandleCubeWriteRequest();
    /* \brief Handle read request from cube core */
    void                                    HandleCubeReadRequest();
    /* \brief Handle request from transfer core */
    void                                    HandleTTransRequest();
    /* \brief Handle write request from transfer core */
    void                                    HandleTTransWriteRequest();
    /* \brief Handle read request from transfer core */
    void                                    HandleTTransReadRequest();

    /* \brief Handle request from bridge */
    void                                    HandleBridgeRequest();
    /* \brief Handle write request from bridge */
    void                                    HandleBridgeWriteRequest();
    /* \brief Handle read request from bridge */
    void                                    HandleBridgeReadRequest();

    /* \brief Handle read request from ring */
    void                                    HandleRingOrXbarRequest();
    void                                    ReceiveStashRequest(std::shared_ptr<CrossStation> station,
                                                                std::deque<StashLdReq>& stashQueue);
    void                                    HandleStashRequest(std::deque<StashLdReq>& stashQueue);
    void                                    HandStationRequest(std::shared_ptr<CrossStation> station);
    void                                    StashPipeInResp(std::shared_ptr<Request> req,
                                                            std::shared_ptr<Request> resp);
    /* \brief Check address overflow */
    void                                    AddCounter(uint32_t bytes, uint64_t id);

private:
    std::vector<std::deque<StashLdReq>> stashQueues;
    std::unordered_map<uint64_t, uint64_t> addrCheckerHash;
    std::deque<StashLdResp> stashRespQueue;
    void CheckAddr(uint32_t addr, uint32_t bankIdx, uint32_t rowInBank, uint32_t colInBank);
    struct IdxStruct {
        uint32_t bankIdx;
        uint32_t rowInBank;
        uint32_t colInBank;
    };
    IdxStruct Addr2Idx(uint32_t addr, uint32_t stid);
};


} // namespace JCore

#endif