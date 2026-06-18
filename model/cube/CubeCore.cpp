#include "cube/CubeCore.h"

#include "core/Core.h"
#include "DFX/InstTracer.h"

namespace JCore {
using namespace std;

CubeUop::CubeUop() : uid(GetInstNextUid()) {}

CubeUop::~CubeUop() {}

void MatrixManager::SetManagerState(BlockCommandPtr cmd, uint64_t setATileSizeRow,
    uint64_t setSrcATileDataSize, uint64_t setSrcBTileDataSize)
{
    uint64_t accTile = 0;
    const uint64_t src0Idx = 0;
    const uint64_t src1Idx = 1;
    const uint64_t src2Idx = 2;
    const uint64_t src3Idx = 3;
    const uint64_t src4Idx = 4;
    blockCmd = cmd;
    for (uint64_t i = 0; i < cmd->srcs.size(); i++) {
        if (cmd->srcs[i]->handType == OperandType::OPD_TILE_ACC) {
            accTile++;
            continue;
        }
        switch (i - accTile) {
            case src0Idx:
                src0 = cmd->srcs[0]->baseAddr;
                src0Type = cmd->dataType;
                break;
            case src1Idx:
                src1 = cmd->srcs[1]->baseAddr;
                src1Type = cmd->dataType;
                break;
            case src2Idx:
                src2 = cmd->srcs[2]->baseAddr;
                src2Type = DataType::FP32;
                break;
            default:
                break;
        }
    }
    if (cmd->tileOp == TileOp::ACCCVT) {
        dst = cmd->dsts[0]->baseAddr;
        dstType = (cmd->blockAttr != nullptr && cmd->blockAttr->dataType != DataType::INVALID)
                  ? cmd->blockAttr->dataType : cmd->dataType;
        dstElementWidth = BytesOf(dstType);
    }
    if (cmd->tileOp == TileOp::TMATMULMX || cmd->tileOp == TileOp::TMATMULMX_ACC) {
        src0 = cmd->srcs[src0Idx]->baseAddr;
        src0Type = cmd->dataType;
        src1 = cmd->srcs[src2Idx]->baseAddr;
        src1Type = cmd->dataType;
        srcMXLeft = cmd->srcs[src1Idx]->baseAddr;
        srcMXRight = cmd->srcs[src3Idx]->baseAddr;
        needScale = true;
    }
    if (cmd->tileOp == TileOp::TMATMULMX_BIAS) {
        src0 = cmd->srcs[src0Idx]->baseAddr;
        src0Type = cmd->dataType;
        src1 = cmd->srcs[src2Idx]->baseAddr;
        src1Type = cmd->dataType;
        srcMXLeft = cmd->srcs[src1Idx]->baseAddr;
        srcMXRight = cmd->srcs[src3Idx]->baseAddr;
        src2 =  cmd->srcs[src4Idx]->baseAddr;
        needScale = true;
    }
    if (cmd->blockAttr != nullptr && cmd->blockAttr->dataType != DataType::INVALID) {
        src1Type = cmd->blockAttr->dataType;
    }
    srcElementWidth = BytesOf(src0Type);
    if (setATileSizeRow != 0) {
        tileSizeM = setATileSizeRow;
    }

    srcATileDataSize = setSrcATileDataSize;
    srcBTileDataSize = setSrcBTileDataSize;

    tileSizeK = srcATileDataSize / tileSizeM / srcElementWidth;
    if (src0Type == DataType::FP4) {
        tileSizeK *= 2;
    }
    tileSizeN = 16 *  srcBTileDataSize / 512;

    if (src0Type != src1Type) {
        mappingNum = BytesOf(src0Type) / BytesOf(src1Type);
        if (src1Type == DataType::FP4) {
            mappingNum *= 2;
        }
    }
    m = cmd->lb0;
    tileM = (m + tileSizeM - 1) / tileSizeM;
    n = cmd->lb1;
    tileN = (n + tileSizeN - 1) / tileSizeN;
    k = cmd->lb2;
    tileK = (k + tileSizeK - 1) / tileSizeK;
    op = cmd->tileOp;
    tileOpId = cmd->bid.val;
    bid = cmd->bid;
    ACC_size = m * n * dstElementWidth;
    stage = MMStage::LOAD;
    tag = cmd->dsts[0]->tileTag;
    if (op == TileOp::TMATMUL_ACC || op == TileOp::TMATMULMX_ACC || op == TileOp::ACCCVT) {
        needAcc = true;
    }
    if (op == TileOp::TMATMUL_BIAS || op == TileOp::TMATMULMX_BIAS) {
        needLoadTileC = true;
    }
    if (op == TileOp::ACCCVT) {
        ACCCVT_type = cmd->blockAttr->layout;
        ACCCVT_canon = cmd->blockAttr->canon;
    }
    if (cmd->dsts.size() == 2 && cmd->tileOp == TileOp::ACCCVT && ACCCVT_type == LayOut::NZ2ND) {
        dst1 = cmd->dsts[1]->baseAddr;
        needRowMax = true;
        totalStoreCount += (tileM * tileSize * accElementWidth + 256 - 1) / 256;
    }
}

bool CubeCore::IsDataTypeDouble(DataType type) const
{
    return type == DataType::FP4 || type == DataType::FP4_1 ||
           type == DataType::HIF4 || type == DataType::INT4 ||
           type == DataType::UINT4;
}

bool CubeCore::IsFP4(DataType type) const
{
    return type == DataType::FP4 || type == DataType::FP4_1 ||
           type == DataType::HIF4 || type == DataType::INT4 ||
           type == DataType::UINT4;
}

void CubeCore::Build()
{
    cfgs.overrideDefaultConfig(GetSim()->getCfgs());
    cubeStats = std::make_shared<CubeCoreStats>(GetSim()->getRpt());
    CubeBCCCreditQ.resize(cfgs.cube_thread_num);
    threadWorkingCycle.resize(cfgs.cube_thread_num);
    CubeTileLdReqQ.resize(cfgs.cube_thread_num);
    TileCubeLdResQ.resize(cfgs.cube_thread_num);
    TileRegCubeLdRetryQ.resize(cfgs.cube_thread_num);
    CubeTileStReqQ.resize(cfgs.cube_thread_num);
    TileRegCubeStRetryQ.resize(cfgs.cube_thread_num);
    BCCCubeFlushQ.resize(cfgs.cube_thread_num);
    cubeStateMachine.resize(cfgs.cube_thread_num);
    L0A[0].resize(cfgs.l0a_buffer_entry);
    L0B[0].resize(cfgs.l0b_buffer_entry);
    ACC.resize(cfgs.l0c_buffer_entry);
    perfectLoad = cfgs.perfectLoad;
    perfectScaleLoad = cfgs.perfectScaleLoad;
    L0ASize = cfgs.l0a_buffer_entry;
    L0BSize = cfgs.l0b_buffer_entry;
    ACCSize = cfgs.l0c_buffer_entry;
    enableLRU = cfgs.enableLRU;
    showTMUPipeView = cfgs.showTMUPipeView;
    ASSERT(cfgs.cube_thread_num == 1);
    cubeL0Size = (cfgs.l0a_buffer_entry * cfgs.l0a_buffer_entry_width) / (1024 * 8); // Bytes
    cubeACCSize = (cfgs.l0c_buffer_entry * cfgs.l0c_buffer_entry_width) / (1024 * 8); // Bytes
    fifoMaxSize = cfgs.uop_isq_entry_size;
    limitSubChainIsqEntry = cfgs.limitSubChainIsqEntry;
    subChainIsqEntryNum = cfgs.subChainIsqEntryNum;
    setATileSizeRow = cfgs.setATileSizeRow;
    setATileSizeCol = cfgs.setATileSizeCol;
    setBTileSizeCol = cfgs.setBTileSizeCol;
    // init l0buffer entry size to 4096 bits
    setSrcATileDataSize = cfgs.l0a_buffer_entry_width / 8;
    setSrcBTileDataSize = cfgs.l0b_buffer_entry_width / 8;
    uopExeCycle = cfgs.maddUopExeCycle;
    ACCFifoMaxSize = cfgs.tcvt_isq_entry_size;
    TileLdUopBuffer.resize(GetSim()->core->configs.scalar_smt_thread);
    CubeUopBuffer.resize(GetSim()->core->configs.scalar_smt_thread);
    rowMaxBufferMap.resize(GetSim()->core->configs.scalar_smt_thread);
    bidCubeSMMap.resize(GetSim()->core->configs.scalar_smt_thread);
    globalUopMap.resize(GetSim()->core->configs.scalar_smt_thread);
    chainROB.resize(GetSim()->core->configs.scalar_smt_thread);
    cubeROB.resize(GetSim()->core->configs.scalar_smt_thread);
    extantAccChainId.resize(GetSim()->core->configs.scalar_smt_thread);
    l0cRenameInfoMap.resize(GetSim()->core->configs.scalar_smt_thread);
    l0cRenameAccTagMap.resize(GetSim()->core->configs.scalar_smt_thread);
    cubeVerifyDataMap.resize(GetSim()->core->configs.scalar_smt_thread);
    GetSim()->core->SetCubeConfigs("uop_isq_depth", std::to_string(fifoMaxSize));
    GetSim()->core->SetCubeConfigs("tcvt_isq_depth", std::to_string(ACCFifoMaxSize));
    GetSim()->core->SetCubeConfigs("L0A_size", std::to_string(cubeL0Size));
    GetSim()->core->SetCubeConfigs("L0B_size", std::to_string(cubeL0Size));
    GetSim()->core->SetCubeConfigs("L0C_size", std::to_string(cubeACCSize));
    cubeDataCheck = cfgs.cube_data_check;
    for (uint64_t i = 0; i < cfgs.blockCmdBufferSize; i++) {
        blockCmdBufferIDPool.push_back(i);
    }
    for (size_t i = 0; i < cubeThreadNum; i++) {
        for (size_t j = 0; j < reqIdMaxNum; j++) {
            loadIdPool[i].push_back(j + coreIndex * reqIdMaxNum);
            storeIdPool[i].push_back(j + coreIndex * reqIdMaxNum);
        }
    }
}

void CubeCore::Xfer()
{
    for (size_t i = 0; i < cubeStateMachine.size(); i++) {
        uopExeFifo[i].Work();
        tileLoadReqBuffer[i].Work();
        tileStoreReqBuffer[i].Work();
    }
    CubeUopFifo.Work();
    tileLdUopFifo.Work();
    for (uint32_t stid = 0; stid < chainROB.size(); ++stid) {
        for (auto it = chainROB[stid].cbegin(); it != chainROB[stid].cend(); it++) {
            chainROB[stid][it->first].Work();
        }
    }
    for (uint32_t stid = 0; stid < CubeUopBuffer.size(); ++stid) {
        for (auto it = CubeUopBuffer[stid].cbegin(); it != CubeUopBuffer[stid].cend(); it++) {
            CubeUopBuffer[stid][it->first].Work();
        }

        for (auto it = TileLdUopBuffer[stid].cbegin(); it != TileLdUopBuffer[stid].cend(); it++) {
            TileLdUopBuffer[stid][it->first].Work();
        }
    }
    for (uint32_t stid = 0; stid < cubeROB.size(); ++stid) {
        cubeROB[stid].Work();
    }
    ACCCVTUopBuffer.Work();
    ACCCVTUopFifo.Work();
    FixPipeFifo.Work();
    CheckBusy();
    CheckLoadBusy();
}

void CubeCore::Reset()
{
}

CubeCore::~CubeCore()
{
}

SimSys* CubeCore::GetSim()
{
    return sim;
}

void CubeCore::Work()
{
    PickTileOp();
    // 处理 ACC rename、产生 uop
    ROBWork();
    // block cmd 请求取入
    FetchReq2ROB();
    // 将 ACCVT 等的 uop 插入到发射队列
    SendACCCVTUop2Fifo();
    // 检查 ACCVT ready 标记
    ACCCVTUopWork();
    // 分型转换
    FixPipeWork();
    // 将 matmul 等的 uop 插入到发射队列
    SendUop2Fifo();
    SendTileLdUop();
    // 发射队列：分配 L0A/L0B/bufferC 空间，产生发往 tmu 的 load 请求
    UopFifoWork();
    // 发射 UOP
    TileLdRename();
    // TileLd请求生成及L0A/L0B重命名
    PickMainChainUop();
    // acccvt生成rowmax store
    GenRowMaxStore();
    // load 请求发往 tmu
    SendTileLoad2TileReg();
    // 接受 tmu 返回请求
    HandleTileLdRes();
    SendL0CStReq2TileReg();
    HandleCubeStResp();
    // 执行 uop 计算
    ExeFifoWork();
    // 检查 tmu 数据返回，以及 bufferC 在 K 轴叠加完成后写到 ACC
    L0BufferWork();
    // 检查 uop 是否执行完来 resolve cube
    for (uint32_t stid = 0; stid < chainROB.size(); ++stid) {
        for (auto it = chainROB[stid].cbegin(); it != chainROB[stid].cend(); it++) {
            TileOpRslv(it->first, stid);
        }
    }
    cubeStats->totalCycles++;
    for (uint32_t stid = 0; stid < cubeROB.size(); ++stid) {
        if (cubeROB[stid].Size() > 0) {
            cubeStats->cubeThreadUsedCycles[0]++;
            cubeStats->accTotalUse += accNum;
            cubeStats->l0cTotalUse += AACCELntryUseNum;
            cubeStats->l0aTotalUse += l0AEntryUseNum;
            cubeStats->l0bTotalUse += l0BEntryUseNum;
        }
    }
    if (AACCELntryUseNum > 0) {
        cubeStats->l0CUseCnt += AACCELntryUseNum;
        cubeStats->l0CUseVld++;
    }
    if (l0AEntryUseNum > 0) {
        cubeStats->l0AUseCnt += l0AEntryUseNum;
        cubeStats->l0AUseVld++;
    }
    if (l0BEntryUseNum > 0) {
        cubeStats->l0BUseCnt += l0BEntryUseNum;
        cubeStats->l0BUseVld++;
    }
}

bool CubeCore::IsCubeBusy()
{
    for (uint32_t stid = 0; stid < cubeROB.size(); ++stid) {
        if (cubeROB[stid].Size() > 0) {
            return true;
        }
    }
    return false;
}

void CubeCore::CheckBusy()
{
    bool currentBusy = IsCubeBusy();
    if (currentBusy) {
        cubeStats->cubeBusyCycle++;
    }
    if (currentBusy && !lastCycleCubeBusy) {
        cubeBusyStartCycle = GetSim()->getCycles();
    }
    if (!currentBusy && lastCycleCubeBusy) {
        // busy to free
        SwimLogData logData;
        logData.name = "Cube busy";
        logData.pid = machineId;
        logData.tid = CORE_INTER_VIEW_TID;
        logData.sTime = cubeBusyStartCycle;
        logData.eTime = GetSim()->getCycles();
        logData.eventId = GetSim()->GetEventId();
        logData.hint = "Cube busy";
        GetSim()->AddDuration(logData);
    }
    lastCycleCubeBusy = currentBusy;
}

bool CubeCore::CheckLoadBusy() const
{
    bool res = loadIdPool[0].size() < reqIdMaxNum;
    if (res) {
        cubeStats->cubeLoadBusyCycle++;
    }
    return res;
}

void CubeCore::FetchReq2ROB()
{
    for (uint32_t stid = 0; stid < GetSim()->core->configs.scalar_smt_thread; ++stid) {
        if (bccCubeBlockCmdQ->Empty()) {
            break;
        }
        auto &readQ = bccCubeBlockCmdQ->GetRawReadData();
        // Find the stid cmd
        auto it = readQ.begin();
        for (; it != readQ.end();++it) {
            if ((*it)->stid == stid) {
                break;
            }
        }

        if (it == readQ.end()) {
            continue;
        }

        BlockCommandPtr cmd = (*it);
        if (cubeROB[cmd->stid].Size() >= cfgs.blockCmdBufferSize) {
            continue;
        }
        if (!CanGetBlkCmdFromBissue(cmd)) {
            continue;
        }
        if (blockCmdBufferIDPool.empty()) {
            continue;
        }
        if (std::find(extantAccChainId[cmd->stid].begin(), extantAccChainId[cmd->stid].end(), cmd->accChainId)
            == extantAccChainId[cmd->stid].end()) {
            extantAccChainId[cmd->stid].push_back(cmd->accChainId);
        }
        if (std::find((*mutiCoreChainInfo)[coreIndex][cmd->stid].cbegin(),
            (*mutiCoreChainInfo)[coreIndex][cmd->stid].cend(), cmd->accChainId)
            == (*mutiCoreChainInfo)[coreIndex][cmd->stid].cend()) {
            (*mutiCoreChainInfo)[coreIndex][cmd->stid].push_back(cmd->accChainId);
        }
        readQ.erase(it); 
        LOG_INFO_M(Unit::CUBE, Stage::NA) << "Receive block Cmd: " << cmd->Dump();
        uint64_t robId = blockCmdBufferIDPool.front();
        blockCmdBufferIDPool.pop_front();
        cmd->execMachineId = machineId + robId;
        GetSim()->GetVerifyManager(cmd->stid)->InitPARGroup(cmd->bid.val, 0);
        GetSim()->GetViewManager(cmd->stid)->InitBIQType(cmd->bid.val, BIQType::CUBE_IQ);
        GetSim()->GetViewManager(cmd->stid)->InitPARGroup(cmd->bid.val, 0);
        CubeRobInfo Info = CubeRobInfo();
        Info.blockCmd = cmd;
        chainROB[cmd->stid][cmd->accChainId].Write(Info);
        cubeROB[cmd->stid].Write(Info);
        if (bidCubeSMMap[cmd->stid].find(cmd->bid.val) != bidCubeSMMap[cmd->stid].end()) {
            bidCubeSMMap[cmd->stid].erase(bidCubeSMMap[cmd->stid].find(cmd->bid.val));
        }
        bidCubeSMMap[cmd->stid][cmd->bid.val].SetManagerState(cmd, setATileSizeRow,
                                                              setSrcATileDataSize, setSrcBTileDataSize);
        bidCubeSMMap[cmd->stid][cmd->bid.val].ACC_flag = cmd->accChainId;
        InitRowMaxBuffer(cmd->bid.val, cmd->stid);
        MatrixManager& mm = bidCubeSMMap[cmd->stid][cmd->bid.val];
        cubeVerifyDataMap[cmd->stid][cmd->bid.val].resize(mm.tileN * mm.tileM * mm.tileSizeM * mm.tileSizeN);
        if (Info.blockCmd->tileOp != TileOp::ACCCVT) {
            uint64_t mnk = static_cast<double>(cmd->lb0 * cmd->lb1 * cmd->lb2);
            if (IsDataTypeDouble(cmd->dataType)) {
                mnk = mnk * 2;
            }
            if (cmd->dataType == DataType::FP32 || cmd->dataType == DataType::TF32 ||
                cmd->dataType == DataType::HF32 || cmd->dataType == DataType::INT32 ||
                cmd->dataType == DataType::UINT32) {
                cubeStats->totalCubeUsed += mnk / 16 / 8 / 16;
            } else if (cmd->dataType == DataType::FP16 || cmd->dataType == DataType::BF16 ||
                        cmd->dataType == DataType::INT16 || cmd->dataType == DataType::UINT16) {
                cubeStats->totalCubeUsed += mnk / 16 / 16 / 16;
            } else if (cmd->dataType == DataType::FP8 || cmd->dataType == DataType::HIF8 ||
                        cmd->dataType == DataType::SF8 || cmd->dataType == DataType::INT8 ||
                        cmd->dataType == DataType::UINT8 || cmd->dataType == DataType::FP8_1) {
                cubeStats->totalCubeUsed += mnk / 16 / 32 / 16;
            } else if (cmd->dataType == DataType::HIF4 || cmd->dataType == DataType::INT4 ||
                        cmd->dataType == DataType::UINT4 || cmd->dataType == DataType::FP4 ||
                        cmd->dataType == DataType::FP4_1) {
                cubeStats->totalCubeUsed += mnk / 16 / 64 / 16;
            }
        }
        if (mm.needRowMax) {
            InitRowMaxBuffer(cmd->bid.val, cmd->stid);
        }
        cubeStats->exeTileOpCount++;
        bidCubeSMMap[cmd->stid][cmd->bid.val].issueToROBCycle = GetSim()->getCycles();
    }
}

void CubeCore::ROBWork()
{
    for (uint32_t stid = 0; stid < extantAccChainId.size(); ++stid) {
        if (cubeROB[stid].Empty()) {
            continue;
        }
        for (size_t i = 0; i < extantAccChainId[stid].size(); i++) {
            ChainRobRename(extantAccChainId[stid][i], stid);
        }
    }
}

void CubeCore::ChainRobRename(uint64_t accChainId, uint32_t stid)
{
    auto& readQ = chainROB[stid][accChainId].GetRawReadData();
    for (size_t i = 0; i < readQ.size(); i++) {
        uint32_t bid = readQ[i].blockCmd->bid.val;
        uint32_t stId = readQ[i].blockCmd->stid;
        ASSERT(stId == stid);
        MatrixManager& mm = bidCubeSMMap[stid][bid];
        if (mm.renamed) {
            continue;
        }
        ROBRename(readQ[i]);
        if (!mm.genUop && mm.renamed) {
            GenUop(readQ[i]);
        }
        if (!mm.renamed) {
            break;
        }
    }
}

void CubeCore::ROBRename(CubeRobInfo& info)
{
    if (!info.renameACC) {
        if (info.blockCmd->tileOp == TileOp::TMATMUL || info.blockCmd->tileOp == TileOp::TMATMUL_BIAS ||
            info.blockCmd->tileOp == TileOp::TMATMULMX || info.blockCmd->tileOp == TileOp::TMATMULMX_BIAS) {
            uint64_t accFlag = info.blockCmd->accChainId;
            uint64_t stid = info.blockCmd->stid;
            if (l0cRenameAccTagMap[stid].find(accFlag) == l0cRenameAccTagMap[stid].end()) {
                LOG_INFO_M(Unit::CUBE, Stage::NA) << "ACC rename alloc. Cmd: " << info.blockCmd->Dump();
                AllocACC(info);
            } else if (l0cRenameAccTagMap[stid][accFlag].lastRenameBid ==
                l0cRenameAccTagMap[stid][accFlag].lastRslvBid) {
                LOG_INFO_M(Unit::CUBE, Stage::NA) << "ACC rename free. Cmd: " << info.blockCmd->Dump();
                FreeACC(bidCubeSMMap[info.blockCmd->stid][info.blockCmd->bid.val]);
            }
        }
        if (info.blockCmd->tileOp == TileOp::TMATMUL_ACC || info.blockCmd->tileOp == TileOp::TMATMULMX_ACC ||
            info.blockCmd->tileOp == TileOp::ACCCVT) {
            LOG_INFO_M(Unit::CUBE, Stage::NA) << "ACCVT rename free. Cmd: " << info.blockCmd->Dump();
            RenameACC(info);
        }
    }
}

void CubeCore::AllocACC(CubeRobInfo& info)
{
    uint32_t bid = info.blockCmd->bid.val;
    uint32_t stid = info.blockCmd->stid;
    MatrixManager& mm = bidCubeSMMap[stid][bid];
    uint64_t needEntryNum = mm.tileM * mm.tileN;
    if (ACCSize >= needEntryNum + AACCELntryUseNum) {
        for (size_t i = 0; i < mm.tileM; i++) {
            for (size_t j = 0; j < mm.tileN; j++) {
                FindFreeACC(i, j, bid, stid);
            }
        }
        l0cRenameAccTagMap[stid][mm.ACC_flag].allocMap = l0cRenameInfoMap[stid][bid].allocMap;
        l0cRenameAccTagMap[stid][mm.ACC_flag].dependBid = bid;
        l0cRenameAccTagMap[stid][mm.ACC_flag].lastRenameBid = bid;
        mm.renameACC = true;
        mm.renamed = true;
        preRobInfo = info;
        AACCELntryUseNum += needEntryNum;
        accNum++;
    }
}

void CubeCore::SetACCToZero(CubeRobInfo& info)
{
    uint32_t bid = info.blockCmd->bid.val;
    uint32_t stid = info.blockCmd->stid;
    auto allocMap = l0cRenameInfoMap[stid][bid].allocMap;
    for (auto it = allocMap.begin(); it != allocMap.end(); it++) {
        (*it).second->OnlyReset();
    }
}

void CubeCore::FreeACC(MatrixManager& mm)
{
    uint32_t stid = mm.blockCmd->stid;
    auto& allocMap = l0cRenameAccTagMap[stid][mm.ACC_flag].allocMap;
    for (auto it = allocMap.begin(); it != allocMap.end(); it++) {
        (*it).second->FreeAndReset();
        AACCELntryUseNum--;
    }
    accNum--;
    l0cRenameAccTagMap[stid].erase(l0cRenameAccTagMap[stid].find(mm.ACC_flag));
}

void CubeCore::RenameACC(CubeRobInfo& info)
{
    uint32_t bid = info.blockCmd->bid.val;
    uint32_t stid = info.blockCmd->stid;
    MatrixManager& mm = bidCubeSMMap[stid][bid];
    uint64_t accFlag = info.blockCmd->accChainId;
    if (l0cRenameAccTagMap[stid].find(accFlag) == l0cRenameAccTagMap[stid].end()) {
        return;
    }
    l0cRenameInfoMap[stid][bid].allocMap = l0cRenameAccTagMap[stid][mm.ACC_flag].allocMap;
    mm.renameACC = true;
    mm.dependBid = l0cRenameAccTagMap[stid][mm.ACC_flag].dependBid;
    l0cRenameAccTagMap[stid][mm.ACC_flag].dependBid = mm.bid.val;
    l0cRenameAccTagMap[stid][mm.ACC_flag].lastRenameBid = bid;
    mm.renamed = true;
    preRobInfo = info;
}

void CubeCore::FindFreeACC(uint64_t i, uint64_t j, uint32_t bid, uint32_t stid)
{
    for (size_t k = 0; k < ACC.size(); k++) {
        if (ACC[k].free) {
            ACC[k].index.push_back(std::make_pair(i, j));
            ACC[k].free = false;
            if (l0cRenameInfoMap[stid].count(bid) != 0) {
                l0cRenameInfoMap[stid][bid].allocMap[ACC[k].index[0]] = &ACC[k];
            } else {
                L0CRenameInfo info;
                info.allocMap[ACC[k].index[0]] = &ACC[k];
                l0cRenameInfoMap[stid][bid] = info;
            }
            break;
        }
    }
}

void CubeCore::GenUop(CubeRobInfo& info)
{
    uint64_t bid = info.blockCmd->bid.val;
    uint32_t stid = info.blockCmd->stid;
    MatrixManager& mm = bidCubeSMMap[stid][bid];
    if (mm.op == TileOp::TMATMUL || mm.op == TileOp::TMATMUL_BIAS || mm.op == TileOp::TMATMUL_ACC ||
        mm.op == TileOp::TMATMULMX || mm.op == TileOp::TMATMULMX_BIAS || mm.op == TileOp::TMATMULMX_ACC) {
        GenUop2Buffer(bid, stid);
        mm.genUop = true;
    }
    if (mm.op == TileOp::ACCCVT) {
        GenACCCVTUop(bid, stid);
        mm.genUop = true;
    }
}

void CubeCore::GenACCCVTUop(uint64_t bid, uint32_t stid)
{
    static uint64_t gIndex = 0;
    MatrixManager& mm = bidCubeSMMap[stid][bid];
    if (mm.ACCCVT_type == LayOut::NORM) {
        GenACCCVTUopNormal(bid, gIndex, stid);
    } else if (mm.ACCCVT_type == LayOut::NZ2ND) {
        GenACCCVTUopNZ2ND(bid, gIndex, stid);
    } else if (mm.ACCCVT_type == LayOut::NZ2DN) {
        GenACCCVTUopNZ2DN(bid, gIndex, stid);
    }
}

void CubeCore::GenACCCVTUopNormal(uint64_t bid, uint64_t& gIndex, uint32_t stid)
{
    MatrixManager& mm = bidCubeSMMap[stid][bid];
    MatrixManager& dependMm = bidCubeSMMap[stid][mm.dependBid];
    const uint64_t storeSize = 256;
    uint64_t accTileSize = dependMm.tileSizeN * dependMm.tileSizeM * dependMm.accElementWidth;
    for (size_t j = 0; j < dependMm.tileN; j++) {
        for (size_t i = 0; i < dependMm.tileM; i++) {
            for (size_t count = 0; count < accTileSize / storeSize; count++) {
                shared_ptr<ACCCVTUop> uop = make_shared<ACCCVTUop>();
                ACCCVTDependInfo dependInfo = ACCCVTDependInfo();
                uop->gIndex = gIndex++;
                dependInfo.index.first = i;
                dependInfo.index.second = j;
                dependInfo.tileOffset = count * storeSize;
                uop->bid = bid;
                uop->dependentTileIdx.push_back(dependInfo);
                uop->offset = (j * dependMm.tileM + i) * accTileSize + count * storeSize;
                uop->storeSize = storeSize;
                uop->stid = mm.blockCmd->stid;
                l0cRenameInfoMap[stid][bid].allocMap[dependInfo.index]->dependenceCount++;
                ACCCVTUopBuffer.Write(uop);
                mm.totalStoreCount++;
                mm.totalUopCount++;
                cubeStats->totalStoreCount++;
                LOG_DEBUG_M(Unit::CUBE, Stage::NA) << mm.bid.val << "ACCVT(Normal) Generate UOP, ACC flag " <<
                    mm.ACC_flag << " dependBid: " << mm.dependBid << " " << *uop;
            }
        }
    }
}

void CubeCore::GenACCCVTUopNZ2ND(uint64_t bid, uint64_t& gIndex, uint32_t stid)
{
    MatrixManager& mm = bidCubeSMMap[stid][bid];
    MatrixManager& dependMm = bidCubeSMMap[stid][mm.dependBid];
    const uint64_t storeDataSize = 256;
    const uint64_t groupNum = storeDataSize / dependMm.accElementWidth / dependMm.tileSizeN;
    for (size_t j = 0; j < dependMm.tileN; j = j + groupNum) {  // if datatype convert, stride is not 4
        for (size_t i = 0; i < dependMm.tileM; i++) {
            for (size_t count = 0; count < dependMm.tileSizeM; count++) {
                shared_ptr<ACCCVTUop> uop = make_shared<ACCCVTUop>();
                uop->gIndex = gIndex++;
                uop->bid = bid;
                uop->stid = mm.blockCmd->stid;
                FindDependTileNZ2ND(i, j, count, uop, stid);
                uop->offset = (i * dependMm.tileSizeM + count) *
                              dependMm.tileN * dependMm.tileSizeN * dependMm.accElementWidth +
                              j * dependMm.tileSizeN * dependMm.accElementWidth;
                uop->row = i * mm.tileSizeM + count;
                ACCCVTUopBuffer.Write(uop);
                mm.totalUopCount++;
                mm.totalStoreCount++;
                cubeStats->totalStoreCount++;
                LOG_DEBUG_M(Unit::CUBE, Stage::NA) << mm.bid.val << "ACCVT(NZ2ND) Generate UOP, ACC flag " <<
                    mm.ACC_flag << " dependBid: " << mm.dependBid  << " " << *uop;
            }
        }
    }
}

void CubeCore::FindDependTileNZ2ND(uint64_t tileM, uint64_t tileN, uint64_t row, shared_ptr<ACCCVTUop> uop,
    uint32_t stid)
{
    MatrixManager& mm = bidCubeSMMap[stid][uop->bid];
    MatrixManager& dependMm = bidCubeSMMap[stid][mm.dependBid];
    const uint64_t storeDataSize = 256;
    const uint64_t groupNum = storeDataSize / dependMm.accElementWidth / dependMm.tileSizeN;
    for (size_t n = 0; n < groupNum && tileN + n < dependMm.tileN; n++) {
        ACCCVTDependInfo dependInfo = ACCCVTDependInfo();
        dependInfo.index.first = tileM;
        dependInfo.index.second = tileN + n;
        dependInfo.row = row;
        uop->dependentTileIdx.push_back(dependInfo);
        uop->storeSize += dependMm.accElementWidth * dependMm.tileSizeN;
        l0cRenameInfoMap[stid][uop->bid].allocMap[dependInfo.index]->dependenceCount++;
    }
}

void CubeCore::GenACCCVTUopNZ2DN(uint64_t bid, uint64_t& gIndex, uint32_t stid)
{
    MatrixManager& mm = bidCubeSMMap[stid][bid];
    MatrixManager& dependMm = bidCubeSMMap[stid][mm.dependBid];
    const uint64_t storeDataSize = 256;
    const uint64_t groupNum = storeDataSize / dependMm.tileSizeM / dependMm.accElementWidth;
    uint64_t bytesPerRow = dependMm.tileM * dependMm.tileSizeM * mm.dstElementWidth;
    for (size_t j = 0; j <dependMm.tileN; j++) {
        for (size_t i = 0; i < dependMm.tileM; i = i + groupNum) {
            for (size_t count = 0; count < dependMm.tileSizeN; count++) {
                shared_ptr<ACCCVTUop> uop = make_shared<ACCCVTUop>();
                uop->gIndex = gIndex++;
                uop->bid = bid;
                uop->storeSize = 0;
                uop->stid = mm.blockCmd->stid;
                FindDependTileNZ2DN(i, j, count, uop, stid);
                uop->offset = (j * dependMm.tileSizeN + count) * bytesPerRow;
                ACCCVTUopBuffer.Write(uop);
                mm.totalUopCount++;
                mm.totalStoreCount++;
                cubeStats->totalStoreCount++;
                LOG_DEBUG_M(Unit::CUBE, Stage::NA) << mm.bid.val << "ACCVT(NZ2DN) Generate UOP, ACC flag " <<
                    mm.ACC_flag << " dependBid: " << mm.dependBid  << " " << *uop;
            }
        }
    }
}

void CubeCore::FindDependTileNZ2DN(uint64_t tileM, uint64_t tileN, uint64_t col, shared_ptr<ACCCVTUop> uop,
    uint32_t stid)
{
    MatrixManager& mm = bidCubeSMMap[stid][uop->bid];
    MatrixManager& dependMm = bidCubeSMMap[stid][mm.dependBid];
    for (size_t m = 0; m < 4 && tileM + m < dependMm.tileM; m++) {
        ACCCVTDependInfo dependInfo = ACCCVTDependInfo();
        dependInfo.index.first = tileM + m;
        dependInfo.index.second = tileN;
        dependInfo.col = col;
        uop->dependentTileIdx.push_back(dependInfo);
        uop->storeSize += 64;
        l0cRenameInfoMap[stid][uop->bid].allocMap[dependInfo.index]->dependenceCount++;
    }
}

void CubeCore::SendACCCVTUop2Fifo()
{
    if (!ACCCVTUopBuffer.Empty() && ACCCVTUopFifo.Size() < ACCFifoMaxSize) {
        auto uop = ACCCVTUopBuffer.Read();
        uop->ACCCVTStartCycle = GetSim()->getCycles();
        uop->ACCCVTRenameCycle = uop->ACCCVTStartCycle;
        uop->ACCCVTIssueCycle = uop->ACCCVTRenameCycle + 1;
        uop->ACCCVTArbCycle = uop->ACCCVTIssueCycle + 1;
        uop->ACCCVTWaitCycle = uop->ACCCVTArbCycle + 1;
        ACCCVTUopFifo.Write(uop);
        LOG_DEBUG_M(Unit::CUBE, Stage::NA) << uop->bid << "ACCVT Insert ISQ " << *uop;
    }
}

void CubeCore::ACCCVTUopWork()
{
    if (!ACCCVTUopFifo.Empty()) {
        auto& readQ = ACCCVTUopFifo.GetRawReadData();
        for (size_t i = 0; i < ACCCVTUopFifo.Size(); i++) {
            CheckACCCVTSrcReady(readQ[i]);
        }
        for (size_t i = 0; i < ACCCVTUopFifo.Size(); i++) {
            if (readQ[i]->srcRdy) {
                FixPipeFifo.Write(readQ[i]);
                // set tileop first acccvt start
                SetAcccvtStartCycle(readQ[i]->bid, readQ[i]->stid);
                readQ[i]->ACCCVTSrcDataCycle = GetSim()->getCycles() + 1;
                readQ[i]->ACCCVTFixPipeCycle = GetSim()->getCycles() + 2;
                readQ[i]->commitCycle = GetSim()->getCycles() + 6;
                LOG_DEBUG_M(Unit::CUBE, Stage::NA) << readQ[i]->bid << "ACCVT issue. " << *readQ[i];
                readQ.erase(readQ.begin() + i);
                break;
            }
        }
    }
}

void CubeCore::SetAcccvtStartCycle(uint64_t bid, uint32_t stid)
{
    if (bidCubeSMMap[stid][bid].acccvtRange.first == 0) {
        bidCubeSMMap[stid][bid].acccvtRange.first = GetSim()->getCycles() + 1;
    }
}

void CubeCore::FixPipeWork()
{
    if (!FixPipeFifo.Empty()) {
        cubeStats->acccvtBusyCycle++;
        auto uop = FixPipeFifo.Front();
        if (GetSim()->getCycles() >= uop->commitCycle) {
            // update tileop last acccvt end
            bidCubeSMMap[uop->stid][uop->bid].acccvtRange.second = GetSim()->getCycles();
            GenACCCVTStore(uop);
            PrintACCCVTPipeView(uop);
            auto& mm = bidCubeSMMap[uop->stid][uop->bid];
            mm.retireUopCount++;
            LOG_INFO_M(Unit::CUBE, Stage::NA) << "uop Resolve cnt:" << dec << mm.retireUopCount << " B" << mm.bid;
            FixPipeFifo.Read();
        }
    }
}

bool CubeCore::CheckACCCVTSrcReady(shared_ptr<ACCCVTUop> uop)
{
    if (uop->srcRdy) {
        return true;
    }
    uint64_t dependBid = bidCubeSMMap[uop->stid][uop->bid].dependBid;
    for (size_t i = 0; i < uop->dependentTileIdx.size(); i++) {
        auto index = uop->dependentTileIdx[i].index;
        if (!(l0cRenameInfoMap[uop->stid][uop->bid].allocMap[index]->ready &&
              l0cRenameInfoMap[uop->stid][uop->bid].allocMap[index]->bid == dependBid)) {
            LOG_DEBUG_M(Unit::CUBE, Stage::NA) << uop->bid << "ACCVT is waitting. dependBid: " << dependBid << *uop;
            return false;
        }
        uop->dependentTileIdx[i].ready = true;
    }
    uop->ACCCVTSrcRdyCycle = GetSim()->getCycles();
    uop->srcRdy = true;
    LOG_DEBUG_M(Unit::CUBE, Stage::NA) << uop->bid << "ACCVT is ready. " << *uop;
    return true;
}

void CubeCore::GenACCCVTStore(shared_ptr<ACCCVTUop> uop)
{
    MatrixManager& mm = bidCubeSMMap[uop->stid][uop->bid];
    uint64_t tileStartAddr = mm.dst;
    uint64_t addr = tileStartAddr + uop->offset;
    CubeTileRegStReq req = CubeTileRegStReq();
    req.SetBid(mm.blockCmd->bid);
    req.SetDest(addr);
    req.SetSize(MAX_TILE_DATA_BYTE);
    req.SetStid(uop->stid);
    // set data mask
    DataMask mask = DataMask(pTileUtils->configs.mask_element_size);
    mask.Reset();
    mask.Set(0, uop->storeSize - 1);
    req.SetDataMask(mask);
    GenACCCVTData(req.GetData(), uop);
    tileStoreReqBuffer[0].Write(req);
    // free ACC
    for (size_t i = 0; i < uop->dependentTileIdx.size(); i++) {
        auto index = uop->dependentTileIdx[i].index;
        auto accPtr = l0cRenameInfoMap[uop->stid][uop->bid].allocMap[index];
        accPtr->dependenceCount--;
        if (accPtr->dependenceCount == 0) {
            LOG_DEBUG_M(Unit::CUBE, Stage::NA) <<dec<< uop->bid << "Debug ACC Ptr "<< *accPtr
                << *uop;
            accPtr->FreeAndReset();
            AACCELntryUseNum--;
        }
    }
    LOG_DEBUG_M(Unit::CUBE, Stage::NA) << uop->bid << "ACCVT gen store. " << *(uop);
}

void CubeCore::GenACCCVTData(uint8_t* dataPtr, shared_ptr<ACCCVTUop> uop)
{
    auto mm = bidCubeSMMap[uop->stid][uop->bid];
    if (mm.ACCCVT_type == LayOut::NORM) {
        GenACCCVTDataNormal(dataPtr, uop);
    } else if (mm.ACCCVT_type == LayOut::NZ2ND) {
        GenACCCVTDataNZ2ND(dataPtr, uop);
    } else if (mm.ACCCVT_type == LayOut::NZ2DN) {
        GenACCCVTDataNZ2DN(dataPtr, uop);
    }
}

void CubeCore::GenACCCVTDataNormal(uint8_t* dataPtr, shared_ptr<ACCCVTUop> uop)
{
    auto mm = bidCubeSMMap[uop->stid][uop->bid];
    auto accPtr = l0cRenameInfoMap[uop->stid][uop->bid].allocMap[uop->dependentTileIdx[0].index];
    for (size_t count = 0; count < 256; count++) {
        dataPtr[count] = accPtr->data[count + uop->dependentTileIdx[0].tileOffset];
    }
}

void CubeCore::GenACCCVTDataNZ2ND(uint8_t* dataPtr, shared_ptr<ACCCVTUop> uop)
{
    uint64_t count = 0;
    auto& mm = bidCubeSMMap[uop->stid][uop->bid];
    for (size_t i = 0; i < uop->dependentTileIdx.size(); i++) {
        auto accPtr = l0cRenameInfoMap[uop->stid][uop->bid].allocMap[uop->dependentTileIdx[i].index];
        for (size_t j = 0; j < mm.tileSizeN; j++) {
            uint64_t data = accPtr->dataLayout[uop->dependentTileIdx[i].row][j];
            for (size_t idx = 0; idx < 4; idx++) {
                dataPtr[count] = (data >> (idx * 8)) & 0xff;
                count++;
            }
        }
    }
}

void CubeCore::GenACCCVTDataNZ2DN(uint8_t* dataPtr, shared_ptr<ACCCVTUop> uop)
{
    uint64_t count = 0;
    auto& mm = bidCubeSMMap[uop->stid][uop->bid];
    for (size_t i = 0; i < uop->dependentTileIdx.size(); i++) {
        auto accPtr = l0cRenameInfoMap[uop->stid][uop->bid].allocMap[uop->dependentTileIdx[i].index];
        for (size_t j = 0; j < mm.tileSizeM; j++) {
            uint64_t data = accPtr->dataLayout[j][uop->dependentTileIdx[i].col];
            for (size_t idx = 0; idx < 4; idx++) {
                dataPtr[count] = (data >> (idx * 8)) & 0xff;
                count++;
            }
        }
    }
}

void CubeCore::HandleTileLdRes()
{
    bool empty = true;
    for (uint32_t stid = 0; stid < cubeROB.size(); ++stid) {
        if (cubeROB[stid].Size() != 0) {
            empty = false;
        }
    }
    if (empty) {
        return;
    }

    if (pTileUtils->IsRingOrXbarMode(true)) {
        for (uint32_t i = 0; i < stationReadRange; i++) {
            auto &station = stations[i];
            if (station->HasNoResp(coreIndex)) {
                continue;
            }
            auto pkt = station->GetDataComReq(coreIndex);
            MatrixManager& mm = bidCubeSMMap[TileLoadReqIdMap[0][pkt->GetBufId()].stid]
                                            [TileLoadReqIdMap[0][pkt->GetBufId()].bid];
            if (IsResidualLoad(0, pkt->GetBufId(), TileLoadReqIdMap[0][pkt->GetBufId()].bid, mm.blockCmd->stid)) {
                DeleteRequestInTileRequestMap(pkt);
                loadIdPool[0].push_back(pkt->GetBufId());
                UpdataUsedLdPool(0, pkt->GetBufId(), TileLoadReqIdMap[0][pkt->GetBufId()].bid, mm.blockCmd->stid);
                UpdataResidualLdPool(0, pkt->GetBufId(), TileLoadReqIdMap[0][pkt->GetBufId()].bid, mm.blockCmd->stid);
                continue;
            }
            mm.toCoreCycle = GetSim()->getCycles();
            if (!mm.firstTOCoreInit) {
                mm.firstTMUToCoreCycle = GetSim()->getCycles();
                mm.firstTOCoreInit = true;
            }
            // update tile op last load end cycle
            mm.loadRange.second = GetSim()->getCycles();
            cubeStats->toCoreCount++;
            cubeStats->cubeLoadReqLatencyMap[GetSim()->getCycles() - pkt->reqInSpbTime]++;
            cubeStats->totalLoadReqCycle += GetSim()->getCycles() - pkt->reqInSpbTime;
            UpdataUsedLdPool(0, pkt->GetBufId(), TileLoadReqIdMap[0][pkt->GetBufId()].bid, mm.blockCmd->stid);
            PrintRingPkt(("[Cube Core] Get tile load response from ring. "), pkt);
            TileLoadReqIdMap[0][pkt->GetBufId()].bufferPtr->readyCount++;
            // 置buffer bid 为tileop bid
            TileLoadReqIdMap[0][pkt->GetBufId()].bufferPtr->bid = TileLoadReqIdMap[0][pkt->GetBufId()].bid;
            TileLoadReqIdMap[0][pkt->GetBufId()].bufferPtr->wrote = true;
            MoveData2L0Buffer(TileLoadReqIdMap[0][pkt->GetBufId()], pkt->GetData());
            loadIdPool[0].push_back(pkt->GetBufId());
            DeleteRequestInTileRequestMap(pkt);
            InstPipeViewPtr instInfo = std::make_shared<InstPipeViewInfo>();
            instInfo->bid = mm.bid.val;
            instInfo->cycleInfo = std::make_shared<CycleInfo>();
            instInfo->cycleInfo->completeCycle = GetSim()->getCycles();
            instInfo->cycleInfo->retireCycle = GetSim()->getCycles();
            instInfo->label = "CUBE_TL" + std::to_string(pkt->uopId) +
                              " BufID:" + std::to_string(pkt->GetBufId()) +
                              " Addr:" + std::to_string(TileLoadReqIdMap[0][pkt->GetBufId()].bufferPtr->tileRegAddr) +
                              " bufferType:" + TileLoadReqIdMap[0][pkt->GetBufId()].bufferType;
            if (pipeViewVerbose) {
                DeliveryPipeInfo(instInfo->cycleInfo, pkt);
                GetSim()->GetViewManager(mm.blockCmd->stid)->RecordMinst(instInfo);
            }
        }
    } else {
        uint64_t count = 0;
        while (count < ldRspHandleNum && !TileCubeLdResQ[0]->Empty()) {
            TileRegCubeLdRes res = TileCubeLdResQ[0]->Read();
            uint64_t reqId = res.GetRespId();
            if (IsResidualLoad(0, reqId, TileLoadReqIdMap[0][reqId].bid, res.GetStid())) {
                loadIdPool[0].push_back(reqId);
                UpdataUsedLdPool(0, reqId, TileLoadReqIdMap[0][reqId].bid, res.GetStid());
                UpdataResidualLdPool(0, reqId, TileLoadReqIdMap[0][reqId].bid, res.GetStid());
                continue;
            }
            // update tile op last load end cycle
            bidCubeSMMap[TileLoadReqIdMap[0][reqId].stid][TileLoadReqIdMap[0]
                        [reqId].bid].loadRange.second = GetSim()->getCycles();
            MoveData2L0Buffer(TileLoadReqIdMap[0][reqId], res.GetData());
            TileLoadReqIdMap[0][reqId].bufferPtr->readyCount++;
            loadIdPool[0].push_back(reqId);
            UpdataUsedLdPool(0, reqId, TileLoadReqIdMap[0][reqId].bid, res.GetStid());
            LOG_DEBUG_M(Unit::CUBE, Stage::NA) << "Receving ld req from tmu";
        }
    }
}

void CubeCore::MoveData2L0Buffer(TileLoadInfo loadInfo, uint8_t* dataPtr)
{
    const uint64_t size = 256 / sizeof(uint8_t);
    for (size_t i = 0; i < size; i++) {
        ASSERT(i + loadInfo.offset < loadInfo.bufferPtr->data.size());
        loadInfo.bufferPtr->data[i + loadInfo.offset] = dataPtr[i];
    }
}

void CubeCore::PrintRingPkt(string stage, shared_ptr<Request> const &pkt)
{
    LOG_DEBUG_M(Unit::CUBE, Stage::NA) << stage << *pkt;
}

void CubeCore::SetLoopMWidth(uint64_t bid, uint32_t stid)
{
    MatrixManager& mm = bidCubeSMMap[stid][bid];
    if (mm.bufferLoopMWidth > mm.tileM) {
        mm.bufferLoopMWidth = mm.tileM;
    }
    if (mm.bufferLoopMWidth * mm.tileK > L0ASize) {
        mm.bufferLoopMWidth = 4;
    }
    if (mm.bufferLoopMWidth * mm.tileK > L0ASize) {
        mm.bufferLoopMWidth = 2;
    }
}

void CubeCore::GenUop2Buffer(uint64_t bid, uint32_t stid)
{
    uint64_t index = 0;
    MatrixManager& mm = bidCubeSMMap[stid][bid];
    vector<shared_ptr<CubeUop>> headUop;
    SetLoopMWidth(bid, stid);
    uint64_t bufferLoopM = (mm.tileM + mm.bufferLoopMWidth - 1) / mm.bufferLoopMWidth;
    mm.bufferLoopNWidth = bufferCCount / mm.bufferLoopMWidth;
    uint64_t bufferLoopN = (mm.tileN + mm.bufferLoopNWidth - 1) / mm.bufferLoopNWidth;
    for (size_t i = 0; i < bufferLoopM; i++) {
        for (size_t j = 0; j < bufferLoopN; j++) {
            headUop.clear();
            GenInnerUop(bid, i, j, headUop, index, stid);
        }
    }
}

void CubeCore::GenInnerUop(uint64_t bid, size_t i, size_t j,
                           vector<shared_ptr<CubeUop>>& headUop, uint64_t& index, uint32_t stid)
{
    MatrixManager& mm = bidCubeSMMap[stid][bid];
    for (size_t k = 0; k < mm.tileK; k++) {
        GenLoopKUop(bid, i, j, k, headUop, index, stid);
    }
}

void CubeCore::GenLoopKUop(uint64_t bid, size_t i, size_t j, size_t k,
                           std::vector<shared_ptr<CubeUop>>& headUop, uint64_t& index, uint32_t stid)
{
    MatrixManager& mm = bidCubeSMMap[stid][bid];
    uint64_t bufferLoopM = (mm.tileM + mm.bufferLoopMWidth - 1) / mm.bufferLoopMWidth;
    uint64_t bufferLoopN = (mm.tileN + mm.bufferLoopNWidth - 1) / mm.bufferLoopNWidth;
    for (size_t n = 0; n < mm.bufferLoopNWidth; n++) {
        if (j == bufferLoopN - 1 && n == mm.tileN - j * mm.bufferLoopNWidth) {
            break;
        }
        for (size_t m = 0; m < mm.bufferLoopMWidth; m++) {
            if (i == bufferLoopM - 1 && m == mm.tileM - i * mm.bufferLoopMWidth) {
                break;
            }
            shared_ptr<CubeUop> uop = make_shared<CubeUop>();
            cubeStats->totalUopCount++;
            uop->uopIndex = index;
            uop->globleUopIndex = globleUopIndex;
            index++;
            globleUopIndex++;
            mm.totalUopCount++;
            uop->bid = bid;
            uop->srcA.first = i * mm.bufferLoopMWidth + m;
            uop->srcA.second = k;
            uop->srcValidA = true;
            uop->srcB.first = k;
            uop->srcB.second = j * mm.bufferLoopNWidth + n;
            uop->srcValidB = true;
            uop->dst.first = i * mm.bufferLoopMWidth + m;
            uop->dst.second = j * mm.bufferLoopNWidth + n;
            uop->stid = mm.blockCmd->stid;
            if (k == 0 && (mm.op == TileOp::TMATMUL_ACC || mm.op == TileOp::TMATMULMX_ACC ||
                mm.op == TileOp::TMATMUL_BIAS)) {
                uop->srcC.first = i * mm.bufferLoopMWidth + m;
                uop->srcC.second = j * mm.bufferLoopNWidth + n;
                uop->genL0CLoad = true;
                uop->srcValidC = true;
            }
            if (mm.op == TileOp::TMATMUL_ACC || mm.op == TileOp::TMATMULMX_ACC) {
                uop->fromACC = true;
            }
            if (k == 0) {
                headUop.push_back(uop);
                uop->needAllocBufferC = true;
            }
            if (k != 0) {
                if (globalUopMap[uop->stid].count(bid) != 0) {
                    ASSERT(headUop[n * mm.bufferLoopMWidth + m]);
                    globalUopMap[uop->stid][bid][uop] = headUop[n * mm.bufferLoopMWidth + m];
                } else {
                    map<shared_ptr<CubeUop>, shared_ptr<CubeUop>> mapTmp;
                    mapTmp[uop] = headUop[n * mm.bufferLoopMWidth + m];
                    globalUopMap[uop->stid][bid] = mapTmp;
                }
            }
            uop->srcAMaskR = mm.tileSizeM;
            uop->srcAMaskC = mm.tileSizeK;
            uop->srcBMaskR = mm.tileSizeK;
            uop->srcBMaskC = mm.tileSizeN;
            if (uop->srcA.first == mm.tileM - 1 && mm.m % mm.tileSizeM != 0) {
                uop->srcAMaskR = mm.m % mm.tileSizeM;
            }
            if (uop->srcA.second == mm.tileK - 1 && mm.k % mm.tileSizeK != 0) {
                uop->srcAMaskC = mm.k % mm.tileSizeK;
            }
            if (uop->srcB.first == mm.tileK - 1 && mm.k % mm.tileSizeK != 0) {
                uop->srcBMaskR = mm.k % mm.tileSizeK;
            }
            if (uop->srcB.second == mm.tileN - 1 && mm.n % mm.tileSizeN != 0) {
                uop->srcBMaskC = mm.n % mm.tileSizeN;
            }
            uop->accChainId = mm.ACC_flag;
            CubeUopBuffer[mm.blockCmd->stid][mm.bid.val].Write(uop);
            TileLdUopBuffer[mm.blockCmd->stid][mm.bid.val].Write(uop);
            LOG_DEBUG_M(Unit::CUBE, Stage::NA) << mm.bid.val << "Generate UOP, ACC flag " <<
            mm.ACC_flag << " " << *uop;
            if (core->pTracer->IsEnabled()) {
                auto info = make_shared<CubeInstDumpInfo>(uop);
                info->peName = "cube";
                core->pTracer->Push(mm.bid, info);
                core->pTracer->PushInstrEvent(mm.bid, uop, InstrEvent::SPLIT);
            }
        }
    }
}
void CubeCore::SendUop2Fifo()
{
    bool send = false;
    for (size_t i = 0; i < exeTileOpChain.size() && !send; i++) {
        send = SendUopChainBid(exeTileOpChain[i].bid, exeTileOpChain[i].stid);
    }
}

bool CubeCore::SendUopChainBid(uint64_t bid, uint32_t stid)
{
    if (CubeUopBuffer[stid][bid].Empty()) {
        return false;
    }
    uint64_t count = 0;
    while (count < sendUopNum && CubeUopFifo.Size() + count < fifoMaxSize) {
        auto uop = CubeUopBuffer[stid][bid].Read();
        auto mm = bidCubeSMMap[uop->stid][uop->bid];
        CubeUopFifo.Write(uop);
        uop->startCycle = GetSim()->getCycles();
        uop->issueCycle = uop->startCycle;
        count++;
        // when first uop dispatch to isq means matmul start
        SetSwimLaneStartCycle(uop->bid, uop->stid);
        LOG_DEBUG_M(Unit::CUBE, Stage::IS) << mm.bid.val << "Issue uop to UOPFIFO, ACC flag " << dec << uop->accChainId
            << " " << *uop;

        if (core->pTracer->IsEnabled()) {
            core->pTracer->PushInstrEvent(mm.bid, uop, InstrEvent::FIFO);
        }
    }
    return true;
}

void CubeCore::SendTileLdUop()
{
    bool send = false;
    for (size_t i = 0; i < exeTileOpChain.size() && !send; i++) {
        send = SendTileLdUopByBid(exeTileOpChain[i].bid, exeTileOpChain[i].stid);
    }
}

bool CubeCore::SendTileLdUopByBid(uint64_t bid, uint32_t stid)
{
    if (TileLdUopBuffer[stid][bid].Empty()) {
        return false;
    }
    uint64_t count = 0;
    while (count < sendUopNum && tileLdUopFifo.Size() + count < tileLdFifoMaxSize) {
        tileLdUopFifo.Write(TileLdUopBuffer[stid][bid].Read());
        count++;
    }
    return true;
}

void CubeCore::UopFifoWork()
{
    if (CubeUopFifo.Empty()) {
        return;
    }
    auto &readQ = CubeUopFifo.GetRawReadData();
    for (auto it = readQ.begin(); it != readQ.end(); it++) {
        CheckAlloc(*it);
        GenUopLoadReq(0, *it);
        AllocAndRenameC(0, *it);
    }

    if (!oooIssue) {
        return;
    }

    for (auto uopIt = readQ.begin(); uopIt != readQ.end(); uopIt++) {
        (*uopIt)->readyForIssue = CheckUopReady(0, *uopIt);
        if (!(*uopIt)->readyForIssue) {
            continue;
        }
        LOG_DEBUG_M(Unit::CUBE, Stage::RB) << (*uopIt)->bid << "Uop is ready." << *(*uopIt);
        if (core->pTracer->IsEnabled()) {
            core->pTracer->PushInstrEvent(bidCubeSMMap[(*uopIt)->stid][(*uopIt)->bid].bid, *uopIt, InstrEvent::READY);
        }
    }
}

void CubeCore::TileLdRename()
{
    if (tileLdUopFifo.Empty()) {
        return;
    }
    auto &readQ = tileLdUopFifo.GetRawReadData();
    for (auto it = readQ.begin(); it != readQ.end(); it++) {
        bool preRenameA = true;
        bool preRenameB = true;
        bool preRenameLeftScale = true;
        bool preRenameRightScale = true;
        if (it != readQ.begin()) {
            auto preIt = it - 1;
            preRenameA = (*preIt)->allocL0A;
            preRenameB = (*preIt)->allocL0B;
            preRenameLeftScale = (*preIt)->allocLeftScale;
            preRenameRightScale = (*preIt)->allocRightScale;
        }
        bool renameCompleted = TileLdRenameAB(0, *it, preRenameA, preRenameB,
                                              preRenameLeftScale, preRenameRightScale); // alloc L0A&L0B
        if (renameCompleted) {
            LOG_INFO_M(Unit::CUBE, Stage::RN) << (*it)->bid << " L0A/B renamed and dequeue " << *(*it);
            readQ.erase(it);
            return;
        }
    }
}

bool CubeCore::TileLdRenameAB(uint64_t threadId, CubeUopPtr uop, bool preRenameA, bool preRenameB,
                              bool preRenameLeftScale, bool preRenameRightScale)
{
    auto mm = bidCubeSMMap[uop->stid][uop->bid];
    if (preRenameB && !uop->allocL0B && !FindSameBTile(threadId, uop)) {
        if (CanAllocL0B(threadId)) {
            AllocL0B(threadId, uop);
            GenL0BLoadReq(threadId, uop);
            if (uop->renameCycle == 0) {
                LOG_DEBUG_M(Unit::CUBE, Stage::RN) << mm.bid << "Alloc L0B. " << *uop;
                uop->renameCycle = GetSim()->getCycles();
            }
        } else {
            FreeL0B(threadId, uop);
        }
    }
    if (preRenameA && !uop->allocL0A && !FindSameATile(threadId, uop)) {
        if (CanAllocL0A(threadId)) {
            AllocL0A(threadId, uop);
            GenL0ALoadReq(threadId, uop);
            if (uop->renameCycle == 0) {
                LOG_DEBUG_M(Unit::CUBE, Stage::RN) << mm.bid << "Alloc L0A. " << *uop;
                uop->renameCycle = GetSim()->getCycles();
            }
        } else {
            FreeL0A(threadId, uop); // TODO:fsl 当L0满，需要判断反压情况,何时替换
        }
    }
    if (mm.needScale && preRenameLeftScale && !uop->allocLeftScale && !FindSameLetfScaleTile(uop)) {
        if (CanAllocLeftScale()) {
            AllocLeftScale(uop);
            GenLeftScaleLoadReq(uop);
            LOG_DEBUG_M(Unit::CUBE, Stage::RN) << mm.bid << "Alloc Left Scale Buffer, " << *uop;
        } else {
            FreeLeftScaleBuffer(uop);
        }
    }
    if (mm.needScale && preRenameRightScale && !uop->allocRightScale && !FindSameRightScaleTile(uop)) {
        if (CanAllocRightScale()) {
            AllocRightScale(uop);
            GenRightScaleLoadReq(uop);
            LOG_DEBUG_M(Unit::CUBE, Stage::RN) << mm.bid << "Alloc Right Scale Buffer, " << *uop;
        } else {
            FreeRightScaleBuffer(uop);
        }
    }
    if (!mm.needScale && uop->allocL0A && uop->allocL0B) {
        return true;
    }
    if (mm.needScale && uop->allocLeftScale && uop->allocRightScale &&
        uop->allocL0A && uop->allocL0B) {
        return true;
    }
    return false;
}

bool CubeCore::UopOOOIssue()
{
    auto &readQ = CubeUopFifo.GetRawReadData();
    for (auto uopIt = readQ.begin(); uopIt != readQ.end(); uopIt++) {
        MatrixManager& mm = bidCubeSMMap[(*uopIt)->stid][(*uopIt)->bid];
        if ((*uopIt)->readyForIssue) {
            uopExeFifo[0].Write(*uopIt);
            // set tileop first mmad begin
            if (bidCubeSMMap[(*uopIt)->stid][(*uopIt)->bid].mmadRange.first == 0) {
                bidCubeSMMap[(*uopIt)->stid][(*uopIt)->bid].mmadRange.first = GetSim()->getCycles() + 1;
            }
            (*uopIt)->calcStartCycle = GetSim()->getCycles();
            (*uopIt)->endCycle = GetSim()->getCycles() + uopExeCycle;
            (*uopIt)->l0abRdCycle = GetSim()->getCycles() + uopRdL0Cycle;
            (*uopIt)->bufcWrCycle = GetSim()->getCycles() + uopWrBufcCycle;
            BufferC[0][(*uopIt)->BuffeCrIndex].ready = false;
            LOG_INFO_M(Unit::CUBE, Stage::RB) << dec << "B"<< mm.bid.val << " send uop to cube unit and read L0Buffer"
                << *(*uopIt);
            if (core->pTracer->IsEnabled()) {
                core->pTracer->PushInstrEvent(mm.bid, *uopIt, InstrEvent::READY);
            }
            readQ.erase(uopIt);
            return true;
        }
    }
    return false;
}

void CubeCore::PickMainChainUop()
{
    bool empty = true;
    for (uint32_t stid = 0; stid < cubeROB.size(); ++stid) {
        if (cubeROB[stid].Size() != 0) {
            empty = false;
        }
    }
    if (empty) {
        return;
    }
    UopOOOIssue();
}

uint64_t CubeCore::GetLdId(uint64_t threadId, uint64_t bid, uint32_t stid)
{
    uint64_t res = loadIdPool[threadId].front();
    loadIdPool[threadId].pop_front();
    auto info = std::pair<uint64_t, ChainTileInfo>(res, ChainTileInfo(bid, stid));
    usedLoadIdPool[threadId].push_back(info);
    return res;
}

bool CubeCore::IsResidualLoad(uint64_t threadId, uint64_t reqId, uint64_t bid, uint32_t stid)
{
    auto& resLoadPool = residualLoadIdPool[threadId];
    return !(find_if(resLoadPool.begin(), resLoadPool.end(),
        [reqId, bid, stid](std::pair<uint64_t, ChainTileInfo> &info) {
            return (info.first == reqId && info.second.stid == stid && info.second.bid == bid);
        }) == resLoadPool.end());
}

void CubeCore::UpdataResidualLdPool(uint64_t threadId, uint64_t reqId, uint64_t bid, uint32_t stid)
{
    auto& resLoadPool = residualLoadIdPool[threadId];
    // auto info = std::pair<uint64_t, ChainTileInfo>(reqId, ChainTileInfo(bid, stid));
    // resLoadPool.erase(find(resLoadPool.begin(), resLoadPool.end(), info));

    resLoadPool.erase(std::remove_if(resLoadPool.begin(), resLoadPool.end(), [reqId, bid, stid]
        (std::pair<uint64_t, ChainTileInfo> &info) {
            return (info.first == reqId && info.second.stid == stid && info.second.bid == bid);
        }), resLoadPool.end());
}

void CubeCore::UpdataUsedLdPool(uint64_t threadId, uint64_t reqId, uint64_t bid, uint32_t stid)
{
    auto& uLoadPool = usedLoadIdPool[threadId];
    // auto info = std::pair<uint64_t, ChainTileInfo>(reqId, ChainTileInfo(bid, stid));
    // uLoadPool.erase(find(uLoadPool.begin(), uLoadPool.end(), info));

    uLoadPool.erase(std::remove_if(uLoadPool.begin(), uLoadPool.end(), [reqId, bid, stid]
        (std::pair<uint64_t, ChainTileInfo> &info) {
            return (info.first == reqId && info.second.stid == stid && info.second.bid == bid);
        }), uLoadPool.end());
}

uint64_t CubeCore::GetStId(uint64_t threadId)
{
    uint64_t res = storeIdPool[threadId].front();
    storeIdPool[threadId].pop_front();
    return res;
}

uint64_t CubeCore::GetTileAAddr(shared_ptr<CubeUop> uop)
{
    MatrixManager& mm = bidCubeSMMap[uop->stid][uop->bid];
    uint64_t tileOffset = uop->srcA.second * mm.tileM * mm.srcATileDataSize
                          + uop->srcA.first * mm.srcATileDataSize;
    return (mm.src0) + tileOffset / GetSim()->core->tileUtils.configs.bank_element_size;
}

uint64_t CubeCore::GetTileBAddr(shared_ptr<CubeUop> uop)
{
    MatrixManager& mm = bidCubeSMMap[uop->stid][uop->bid];
    uint64_t tileOffset = uop->srcB.first *  mm.tileN * mm.srcBTileDataSize
                          + uop->srcB.second * mm.srcBTileDataSize;
    return (mm.src1) + tileOffset / GetSim()->core->tileUtils.configs.bank_element_size;
}

bool CubeCore::FindSameATile(uint64_t threadId, shared_ptr<CubeUop> uop)
{
    for (size_t i = 0; i < L0A[threadId].size(); i++) {
        auto& indexVex = L0A[threadId][i].index;
        bool find = std::find(indexVex.begin(), indexVex.end(), uop->srcA) != indexVex.end();
        if ((!L0A[threadId][i].free && find && L0A[threadId][i].bid == uop->bid) ||
            L0A[threadId][i].tileRegAddr == GetTileAAddr(uop)) {
            L0A[threadId][i].free = false;
            ++L0A[threadId][i].dependenceCount;
            uop->allocL0A = true;
            uop->genL0ALoad = false;
            uop->l0ABufferIndex = i;
            UpdateL0ALRU(i);
            if (uop->renameCycle == 0) {
                uop->renameCycle = GetSim()->getCycles();
            }
            LOG_DEBUG_M(Unit::CUBE, Stage::RN) << uop->bid << "Hit L0A[" << i << "]" << *uop;
            return true;
        }
    }
    return false;
}

bool CubeCore::FindSameBTile(uint64_t threadId, shared_ptr<CubeUop> uop)
{
    for (size_t i = 0; i < L0B[threadId].size(); i++) {
        auto& indexVex = L0B[threadId][i].index;
        bool find = std::find(indexVex.begin(), indexVex.end(), uop->srcA) != indexVex.end();
        if ((!L0B[threadId][i].free && find && L0B[threadId][i].bid == uop->bid &&
            L0B[threadId][i].stid == uop->stid) ||
            L0B[threadId][i].tileRegAddr == GetTileBAddr(uop)) {
            L0B[threadId][i].free = false;
            L0B[threadId][i].free = false;
            ++L0B[threadId][i].dependenceCount;
            uop->allocL0B = true;
            uop->genL0BLoad = false;
            uop->l0BBufferIndex = i;
            UpdateL0BLRU(i);
            if (uop->renameCycle == 0) {
                uop->renameCycle = GetSim()->getCycles();
            }
            LOG_DEBUG_M(Unit::CUBE, Stage::RN) << uop->bid << "Hit L0B[" << i << "]" << *uop;
            return true;
        }
    }
    return false;
}

bool CubeCore::FindSameLetfScaleTile(shared_ptr<CubeUop> uop)
{
    for (size_t i = 0; i < leftScaleBuffer.size(); i++) {
        auto& indexVex = leftScaleBuffer[i].index;
        bool find = std::find(indexVex.begin(), indexVex.end(), uop->srcA) != indexVex.end();
        if ((!leftScaleBuffer[i].free && find && leftScaleBuffer[i].bid == uop->bid) ||
            leftScaleBuffer[i].tileRegAddr == GetTileAAddr(uop)) {
            leftScaleBuffer[i].free = false;
            ++leftScaleBuffer[i].dependenceCount;
            uop->allocLeftScale = true;
            uop->genLeftScaleLoad = false;
            uop->leftScaleIndex = i;
            UpdateLeftScaleLRU(i);
            if (uop->renameCycle == 0) {
                uop->renameCycle = GetSim()->getCycles();
            }
            LOG_DEBUG_M(Unit::CUBE, Stage::RN) << uop->bid << "Hit LeftScale[" << i << "]" << *uop;
            return true;
        }
    }
    return false;
}

bool CubeCore::FindSameRightScaleTile(shared_ptr<CubeUop> uop)
{
    for (size_t i = 0; i < rightScaleBuffer.size(); i++) {
        auto& indexVex = rightScaleBuffer[i].index;
        bool find = std::find(indexVex.begin(), indexVex.end(), uop->srcA) != indexVex.end();
        if ((!rightScaleBuffer[i].free && find && rightScaleBuffer[i].bid == uop->bid) ||
            rightScaleBuffer[i].tileRegAddr == GetTileAAddr(uop)) {
            rightScaleBuffer[i].free = false;
            ++rightScaleBuffer[i].dependenceCount;
            uop->allocRightScale = true;
            uop->genRightScaleLoad = false;
            uop->rightScaleIndex = i;
            UpdateRightScaleLRU(i);
            if (uop->renameCycle == 0) {
                uop->renameCycle = GetSim()->getCycles();
            }
            LOG_DEBUG_M(Unit::CUBE, Stage::RN) << uop->bid << "Hit RightScale[" << i << "]" << *uop;
            return true;
        }
    }
    return false;
}

void CubeCore::UpdateL0ALRU(uint64_t id)
{
    if (!enableLRU) {
        return;
    }
    if (find(l0aLru.begin(), l0aLru.end(), id) != l0aLru.end()) {
        l0aLru.erase(find(l0aLru.begin(), l0aLru.end(), id));
        LOG_DEBUG_M(Unit::CUBE, Stage::NA) << "Pop LRU : Hit L0A[" << id << "] size:" << l0aLru.size();
    }
    l0aLru.push_back(id);
    LOG_DEBUG_M(Unit::CUBE, Stage::NA) << "Push LRU: Hit L0A[" << id << "] size:" << l0aLru.size();
}

void CubeCore::UpdateL0BLRU(uint64_t id)
{
    if (!enableLRU) {
        return;
    }
    if (find(l0bLru.begin(), l0bLru.end(), id) != l0bLru.end()) {
        l0bLru.erase(find(l0bLru.begin(), l0bLru.end(), id));
        LOG_DEBUG_M(Unit::CUBE, Stage::NA) << "Pop LRU : Hit L0B[" << id << "] size:" << l0bLru.size();
    }
    l0bLru.push_back(id);
    LOG_DEBUG_M(Unit::CUBE, Stage::NA) << "Push LRU: Hit L0B[" << id << "] size:" << l0bLru.size();
}

void CubeCore::UpdateLeftScaleLRU(uint64_t id)
{
    if (find(leftScaleLru.begin(), leftScaleLru.end(), id) != leftScaleLru.end()) {
        leftScaleLru.erase(find(leftScaleLru.begin(), leftScaleLru.end(), id));
    }
    leftScaleLru.push_back(id);
}

void CubeCore::UpdateRightScaleLRU(uint64_t id)
{
    if (find(rightScaleLru.begin(), rightScaleLru.end(), id) != rightScaleLru.end()) {
        rightScaleLru.erase(find(rightScaleLru.begin(), rightScaleLru.end(), id));
    }
    rightScaleLru.push_back(id);
}

bool CubeCore::CanAllocL0A(uint64_t threadId)
{
    for (size_t i = 0; i < L0A[threadId].size(); i++) {
        if (L0A[threadId][i].free) {
            return true;
        }
    }
    return false;
}

bool CubeCore::CanAllocL0B(uint64_t threadId)
{
    for (size_t i = 0; i < L0B[threadId].size(); i++) {
        if (L0B[threadId][i].free) {
            return true;
        }
    }
    return false;
}

bool CubeCore::CanAllocBufferC(uint64_t threadId)
{
    for (size_t i = 0; i < BufferC[threadId].size(); i++) {
        if (BufferC[threadId][i].free) {
            freeAndAllocBufferC = false;
            return true;
        }
    }
    return false;
}

bool CubeCore::CanAllocLeftScale()
{
    for (size_t i = 0; i < leftScaleBuffer.size(); i++) {
        if (leftScaleBuffer[i].free) {
            return true;
        }
    }
    return false;
}

bool CubeCore::CanAllocRightScale()
{
    for (size_t i = 0; i < rightScaleBuffer.size(); i++) {
        if (rightScaleBuffer[i].free) {
            return true;
        }
    }
    return false;
}

void CubeCore::AllocL0A(uint64_t threadId, shared_ptr<CubeUop> uop)
{
    for (size_t i = 0; i < L0A[threadId].size(); i++) {
        if (L0A[threadId][i].free) {
            L0A[threadId][i].index.push_back(uop->srcA);
            L0A[threadId][i].bufferIdx = i;
            L0A[threadId][i].dependenceCount++;
            L0A[threadId][i].free = false;
            L0A[threadId][i].bid = uop->bid;
            L0A[threadId][i].stid = uop->stid;
            uop->l0ABufferIndex = i;
            uop->allocL0A = true;
            l0AEntryUseNum++;
            UpdateL0ALRU(i);
            break;
        }
    }
}

void CubeCore::AllocL0B(uint64_t threadId, shared_ptr<CubeUop> uop)
{
    for (size_t i = 0; i < L0B[threadId].size(); i++) {
        if (L0B[threadId][i].free) {
            SetL0BEntryMapping(uop, i);
            L0B[threadId][i].bufferIdx = i;
            L0B[threadId][i].dependenceCount++;
            L0B[threadId][i].free = false;
            L0B[threadId][i].bid = uop->bid;
            L0B[threadId][i].stid = uop->stid;
            uop->l0BBufferIndex = i;
            uop->allocL0B = true;
            l0BEntryUseNum++;
            UpdateL0BLRU(i);
            break;
        }
    }
}

void CubeCore::AllocLeftScale(shared_ptr<CubeUop> uop)
{
    for (size_t i = 0; i < leftScaleBuffer.size(); i++) {
        if (leftScaleBuffer[i].free) {
            SetLeftScaleEntryMapping(uop, i);
            leftScaleBuffer[i].bufferIdx = i;
            leftScaleBuffer[i].dependenceCount++;
            leftScaleBuffer[i].free = false;
            leftScaleBuffer[i].bid = uop->bid;
            uop->leftScaleIndex = i;
            uop->allocLeftScale = true;
            UpdateLeftScaleLRU(i);
            break;
        }
    }
}

void CubeCore::AllocRightScale(shared_ptr<CubeUop> uop)
{
    for (size_t i = 0; i < rightScaleBuffer.size(); i++) {
        if (rightScaleBuffer[i].free) {
            SetRightScaleEntryMapping(uop, i);
            rightScaleBuffer[i].bufferIdx = i;
            rightScaleBuffer[i].dependenceCount++;
            rightScaleBuffer[i].free = false;
            rightScaleBuffer[i].bid = uop->bid;
            uop->rightScaleIndex = i;
            uop->allocRightScale = true;
            UpdateRightScaleLRU(i);
            break;
        }
    }
}

// 当混精度计算时，L0B entry会按行（k维度）对应多个计算分型
void CubeCore::SetL0BEntryMapping(shared_ptr<CubeUop> uop, uint64_t l0bIndex)
{
    auto mm = bidCubeSMMap[uop->stid][uop->bid];
    for (size_t i = (uop->srcB.first / mm.mappingNum) * mm.mappingNum; i < mm.mappingNum; i++) {
        L0B[0][l0bIndex].index.push_back(std::make_pair(i, uop->srcB.second));
    }
}

void CubeCore::SetLeftScaleEntryMapping(shared_ptr<CubeUop> uop, uint64_t leftScaleIndex)
{
    const uint64_t dataWidth = core->GetConfig().singleTierMode ? 512 : 256;
    auto mm = bidCubeSMMap[uop->stid][uop->bid];
    uint64_t leftScaleTileK = mm.tileSizeK / 32;
    uint64_t leftScaleTileSize = leftScaleTileK * 16;
    if (mm.src0Type == DataType::HIF4) {
        leftScaleTileSize = mm.tileSizeM * mm.tileSizeK / 64 * 32; // 每64个元素共享32位的缩放因子
    }
    uint64_t includeNum = dataWidth / leftScaleTileSize;
    uint64_t tileIndex = uop->srcA.first * mm.tileK + uop->srcA.second;
    uint64_t startIndex = tileIndex - (tileIndex % includeNum);
    uint64_t startM = startIndex / mm.tileK;
    uint64_t startK = startIndex % mm.tileK;
    uint64_t count = 0;
    for (size_t i = startM; i < mm.tileM; i++) {
        for (size_t j = startK; j < mm.tileK; j++) {
            if (count >= includeNum) {
                return;
            }
            leftScaleBuffer[leftScaleIndex].index.push_back(std::make_pair(i, j));
            count++;
        }
    }
}

void CubeCore::SetRightScaleEntryMapping(shared_ptr<CubeUop> uop, uint64_t rightScaleIndex)
{
    const uint64_t dataWidth = core->GetConfig().singleTierMode ? 512 : 256;
    auto mm = bidCubeSMMap[uop->stid][uop->bid];
    uint64_t rightScaleTileK = mm.tileSizeK / 32;
    uint64_t rightScaleTileSize = rightScaleTileK * 16;
    if (mm.src1Type == DataType::HIF4) {
        rightScaleTileSize = mm.tileSizeM * mm.tileSizeK / 64 * 32; // 每64个元素共享32位的缩放因子
    }
    uint64_t includeNum = dataWidth / rightScaleTileSize;
    uint64_t tileIndex = uop->srcA.second * mm.tileK + uop->srcA.first;
    uint64_t startIndex = tileIndex - (tileIndex % includeNum);
    uint64_t startK = startIndex % mm.tileK;
    uint64_t startN = startIndex / mm.tileK;
    uint64_t count = 0;
    for (size_t j = startN; j < mm.tileN; j++) {
        for (size_t i = startK; i < mm.tileK; i++) {
            if (count >= includeNum) {
                return;
            }
            rightScaleBuffer[rightScaleIndex].index.push_back(std::make_pair(i, j));
            count++;
        }
    }
}

void CubeCore::AllocBufferC(uint64_t threadId, shared_ptr<CubeUop> uop)
{
    auto mm = bidCubeSMMap[uop->stid][uop->bid];
    for (size_t i = 0; i < BufferC[threadId].size(); i++) {
        if (BufferC[threadId][i].free) {
            BufferC[threadId][i].free = false;
            BufferC[threadId][i].bufferIdx = i;
            BufferC[threadId][i].loopKBound = mm.tileK;
            uop->BuffeCrIndex = i;
            uop->allocBufferC = true;
            BufferC[threadId][i].index.push_back(uop->dst);
            BufferC[threadId][i].stid = uop->stid;
            LOG_INFO_M(Unit::CUBE, Stage::RN) << uop->bid << "Alloc BufferC[" << i << "]";
            if (mm.op == TileOp::TMATMUL || mm.op == TileOp::TMATMULMX) {
                BufferC[threadId][i].ready = true;
            }
            break;
        }
    }
    if (freeAndAllocBufferC) {
        tmpBufferC[threadId].free = false;
        tmpBufferC[threadId].bufferIdx = threadId;
        tmpBufferC[threadId].loopKBound = mm.tileK;
        uop->BuffeCrIndex = freeAndAllocIndex;
        uop->allocBufferC = true;
        tmpBufferC[threadId].index.push_back(uop->dst);
        tmpBufferC[threadId].stid = uop->stid;
        LOG_INFO_M(Unit::CUBE, Stage::RN) << uop->bid << "Alloc tmp BufferC[" << freeAndAllocIndex << "]";
        if (mm.op == TileOp::TMATMUL || mm.op == TileOp::TMATMULMX) {
            tmpBufferC[threadId].ready = true;
        }
    }
}

bool CubeCore::CheckAlloc(shared_ptr<CubeUop> uop)
{
    if (uop->allocL0A && uop->allocL0B && uop->allocBufferC) {
        if (uop->waitingForLoadCycle == 0) {
            uop->waitingForLoadCycle = GetSim()->getCycles();
        }
        return true;
    }
    return false;
}

void CubeCore::AllocAndRenameC(uint64_t threadId, shared_ptr<CubeUop> uop)
{
    auto mm = bidCubeSMMap[uop->stid][uop->bid];
    if (!uop->allocBufferC && uop->needAllocBufferC && CanAllocBufferC(threadId)) {
        AllocBufferC(threadId, uop);
        if (uop->renameCycle == 0) {
            LOG_INFO_M(Unit::CUBE, Stage::RN) << mm.bid << "Alloc Buffer C. " << *uop;
            uop->renameCycle = GetSim()->getCycles();
        }
    }
    if (!uop->needAllocBufferC && !uop->allocBufferC && globalUopMap[uop->stid][uop->bid][uop]->allocBufferC) {
        uop->BuffeCrIndex = globalUopMap[uop->stid][uop->bid][uop]->BuffeCrIndex;
        uop->allocBufferC = true;
        LOG_INFO_M(Unit::CUBE, Stage::RN) << mm.bid << "Alloc Buffer C. " << *uop
            << " links BufferC [" << uop->BuffeCrIndex << "]";
    }
    if (core->pTracer->IsEnabled()) {
        core->pTracer->PushInstrEvent(mm.bid, uop, InstrEvent::ALLOC);
    }
}

void CubeCore::FreeL0A(uint64_t threadId, shared_ptr<CubeUop> uop)
{
    if (enableLRU) {
        LRUFreeL0A(threadId, uop);
    } else {
        FreeL0ANormal(threadId, uop);
    }
}

void CubeCore::LRUFreeL0A(uint64_t threadId, shared_ptr<CubeUop> uop)
{
    MatrixManager& mm = bidCubeSMMap[uop->stid][uop->bid];
    for (size_t i = 0; i < l0aLru.size(); i++) {
        size_t j = l0aLru[i];
        if (L0A[threadId][j].dependenceCount > 0 && i != l0aLru.size() - 1) {
            continue;
        } else if (L0A[threadId][j].dependenceCount == 0 && !L0A[threadId][j].free) {
            L0A[threadId][j].FreeAndReset();
            l0AEntryUseNum--;
            LOG_DEBUG_M(Unit::CUBE, Stage::NA) << mm.bid << "Free L0A[" << j << "]" << *uop;
            l0aLru.erase(find(l0aLru.begin(), l0aLru.end(), j));
            AllocL0A(0, uop);
            GenL0ALoadReq(0, uop);
            break;
        }
    }
}

void CubeCore::FreeL0ANormal(uint64_t threadId, shared_ptr<CubeUop> uop)
{
    MatrixManager& mm = bidCubeSMMap[uop->stid][uop->bid];
    for (size_t i = 0; i < L0A[threadId].size(); i++) {
        if (!L0A[threadId][i].free && L0A[threadId][i].dependenceCount == 0) {
            L0A[threadId][i].FreeAndReset();
            l0AEntryUseNum--;
            AllocL0A(0, uop);
            GenL0ALoadReq(0, uop);
            LOG_DEBUG_M(Unit::CUBE, Stage::NA) << mm.bid << "Free L0A[" << i << "]" << *uop;
        }
    }
}

void CubeCore::FreeL0B(uint64_t threadId, shared_ptr<CubeUop> uop)
{
    if (enableLRU) {
        LRUFreeL0B(threadId, uop);
    } else {
        FreeL0BNormal(threadId, uop);
    }
}

void CubeCore::LRUFreeL0B(uint64_t threadId, shared_ptr<CubeUop> uop)
{
    MatrixManager& mm = bidCubeSMMap[uop->stid][uop->bid];
    for (size_t i = 0; i < l0bLru.size(); i++) {
        size_t j = l0bLru[i];
        if (L0B[threadId][j].dependenceCount > 0 && i != l0bLru.size() - 1) {
            continue;
        } else if (L0B[threadId][j].dependenceCount == 0 && !L0B[threadId][j].free) {
            L0B[threadId][j].FreeAndReset();
            l0BEntryUseNum--;
            LOG_DEBUG_M(Unit::CUBE, Stage::NA) << mm.bid << "Free L0B[" << j << "]" << *uop;
            l0bLru.erase(find(l0bLru.begin(), l0bLru.end(), j));
            AllocL0B(0, uop);
            GenL0BLoadReq(0, uop);
            break;
        }
    }
}

void CubeCore::FreeL0BNormal(uint64_t threadId, shared_ptr<CubeUop> uop)
{
    MatrixManager& mm = bidCubeSMMap[uop->stid][uop->bid];
    for (size_t i = 0; i < L0B[threadId].size(); i++) {
        if (!L0B[threadId][i].free && L0B[threadId][i].dependenceCount == 0) {
            L0B[threadId][i].FreeAndReset();
            l0BEntryUseNum--;
            AllocL0B(0, uop);
            GenL0BLoadReq(0, uop);
            LOG_DEBUG_M(Unit::CUBE, Stage::NA) << mm.bid << "Free L0B[" << i << "]" << *uop;
        }
    }
}

void CubeCore::FreeLeftScaleBuffer(shared_ptr<CubeUop> uop)
{
    for (size_t i = 0; i < leftScaleLru.size(); i++) {
        size_t j = leftScaleLru[i];
        if (leftScaleBuffer[j].dependenceCount > 0 && i != leftScaleLru.size() - 1) {
            continue;
        } else if (leftScaleBuffer[j].dependenceCount == 0 && !leftScaleBuffer[j].free) {
            leftScaleBuffer[j].FreeAndReset();
            leftScaleLru.erase(find(leftScaleLru.begin(), leftScaleLru.end(), j));
            AllocLeftScale(uop);
            GenLeftScaleLoadReq(uop);
            break;
        }
    }
}

void CubeCore::FreeRightScaleBuffer(shared_ptr<CubeUop> uop)
{
    for (size_t i = 0; i < rightScaleLru.size(); i++) {
        size_t j = rightScaleLru[i];
        if (rightScaleBuffer[j].dependenceCount > 0 && i != rightScaleLru.size() - 1) {
            continue;
        } else if (rightScaleBuffer[j].dependenceCount == 0 && !rightScaleBuffer[j].free) {
            rightScaleBuffer[j].FreeAndReset();
            rightScaleLru.erase(find(rightScaleLru.begin(), rightScaleLru.end(), j));
            AllocRightScale(uop);
            GenRightScaleLoadReq(uop);
            break;
        }
    }
}

void CubeCore::GenUopLoadReq(uint64_t threadId, shared_ptr<CubeUop> uop)
{
    auto mm = bidCubeSMMap[uop->stid][uop->bid];
    if (mm.needAcc && uop->allocBufferC && !BufferC[threadId][uop->BuffeCrIndex].alreadyGen &&
        l0cRenameInfoMap[uop->stid][uop->bid].allocMap[uop->srcC]->ready &&
        bidCubeSMMap[uop->stid][uop->bid].dependBid == l0cRenameInfoMap[uop->stid][uop->bid].allocMap[uop->srcC]->bid) {
        BufferC[threadId][uop->BuffeCrIndex].alreadyGen = true;
        GetACCTileData(uop);
    }
}

void CubeCore::GenL0ALoadReq(uint64_t threadId, shared_ptr<CubeUop> uop)
{
    if (perfectLoad) {
        L0A[threadId][uop->l0ABufferIndex].ready = true;
        return;
    }
    MatrixManager& mm = bidCubeSMMap[uop->stid][uop->bid];
    const uint64_t dataWidth = core->GetConfig().singleTierMode ? 512 : 256;
    uint64_t elementWidth = BytesOf(mm.src0Type);
    uint64_t reqCount = mm.srcATileDataSize / dataWidth;
    CubeTileRegLdReq req;
    req.SetStid(uop->stid);
    TileLoadInfo info;
    info.bufferType = "L0A";
    uint64_t tileOffset = uop->srcA.second * mm.tileM * mm.srcATileDataSize
                          + uop->srcA.first * mm.srcATileDataSize;
    uint64_t tileStartAddr = mm.src0;
    uint64_t tileBaseAddr = tileStartAddr + tileOffset / GetSim()->core->tileUtils.configs.bank_element_size;
    L0A[threadId][uop->l0ABufferIndex].tileRegAddr = tileBaseAddr;
    for (size_t i = 0; i < reqCount; i++) {
        req = CubeTileRegLdReq();
        req.cubeUopIndex = uop->globleUopIndex;
        req.isL0ALoad = true;
        req.cubeTileIndex = uop->srcA;
        req.SetSize(MAX_TILE_DATA_BYTE);
        req.SetSrc(tileBaseAddr +  i * dataWidth / GetSim()->core->tileUtils.configs.bank_element_size);
        info.req = req;
        info.bid = uop->bid;
        info.tileOpIndex = uop->uopIndex;
        info.bufferPtr = &L0A[threadId][uop->l0ABufferIndex];
        info.bufferPtr->loadCount++;
        info.offset = i * dataWidth / sizeof(uint8_t);
        info.uop = uop;
        info.stid = uop->stid;
        info.accFlg = uop->accChainId;
        uop->uopLoadGenCount++;
        tileLoadReqBuffer[threadId].Write(info);
        LOG_DEBUG_M(Unit::CUBE, Stage::NA) << mm.bid << "Generate L0A load request. " << *uop
            << " src base:0x" << std::hex << mm.src0 << std::dec << " m:" << mm.m << " tileK:" << mm.tileSizeK
                << " tileSizeM:" << mm.tileSizeM << " elementWidth:" << elementWidth << " " << req;

        if (core->pTracer->IsEnabled()) {
            core->pTracer->PushInstrEvent(mm.bid, uop, InstrEvent::REQ);
        }
        cubeStats->totalLoadCount++;
    }
    if (reqCount == 0 && mm.srcATileDataSize % dataWidth != 0) {
        req = CubeTileRegLdReq();
        req.cubeUopIndex = uop->globleUopIndex;
        req.isL0ALoad = true;
        req.cubeTileIndex = uop->srcA;
        req.SetSize(128);
        req.SetSrc(tileBaseAddr);
        info.req = req;
        info.bid = uop->bid;
        info.tileOpIndex = uop->uopIndex;
        info.bufferPtr = &L0A[threadId][uop->l0ABufferIndex];
        info.bufferPtr->loadCount++;
        info.offset = 0;
        info.uop = uop;
        info.stid = uop->stid;
        info.accFlg = uop->accChainId;
        uop->uopLoadGenCount++;
        tileLoadReqBuffer[threadId].Write(info);
        LOG_DEBUG_M(Unit::CUBE, Stage::NA) << mm.bid << "Generate L0A load request. " << *uop
            << " src base:0x" << std::hex << mm.src0 << std::dec << " m:" << mm.m << " tileK:" << mm.tileSizeK
                << " tileSizeM:" << mm.tileSizeM << " elementWidth:" << elementWidth << " " << req;

        if (core->pTracer->IsEnabled()) {
            core->pTracer->PushInstrEvent(mm.bid, uop, InstrEvent::REQ);
        }
        cubeStats->totalLoadCount++;
    }
}

void CubeCore::GenL0BLoadReq(uint64_t threadId, shared_ptr<CubeUop> uop)
{
    if (perfectLoad) {
        L0B[threadId][uop->l0BBufferIndex].ready = true;
        return;
    }
    MatrixManager& mm = bidCubeSMMap[uop->stid][uop->bid];
    const uint64_t dataWidth = core->GetConfig().singleTierMode ? 512 : 256;
    uint64_t elementWidth = BytesOf(mm.src1Type);
    uint64_t reqCount = mm.srcBTileDataSize / dataWidth;
    CubeTileRegLdReq req;
    req.SetStid(uop->stid);
    TileLoadInfo info;
    info.bufferType = "L0B";
    auto beginIndex = uop->srcB;
    beginIndex.first = (beginIndex.first / mm.mappingNum) * mm.mappingNum;
    uint64_t tileOffset = beginIndex.first *  mm.tileN * mm.srcBTileDataSize
                          + beginIndex.second * mm.srcBTileDataSize;
    uint64_t tileStartAddr = mm.src1;
    uint64_t tileBaseAddr = tileStartAddr + tileOffset / GetSim()->core->tileUtils.configs.bank_element_size;
    L0B[threadId][uop->l0BBufferIndex].tileRegAddr = tileBaseAddr;
    for (size_t i = 0; i < reqCount; i++) {
        req = CubeTileRegLdReq();
        req.cubeUopIndex = uop->globleUopIndex;
        req.isL0BLoad = true;
        req.cubeTileIndex = beginIndex;
        req.SetSize(MAX_TILE_DATA_BYTE);
        req.SetSrc(tileBaseAddr + i * dataWidth / GetSim()->core->tileUtils.configs.bank_element_size);
        info.req = req;
        info.bid = uop->bid;
        info.bufferPtr = &L0B[threadId][uop->l0BBufferIndex];
        info.bufferPtr->loadCount++;
        info.offset = i * dataWidth / sizeof(uint8_t);
        info.tileOpIndex = uop->uopIndex;
        info.accFlg = uop->accChainId;
        uop->uopLoadGenCount++;
        info.stid = uop->stid;
        tileLoadReqBuffer[threadId].Write(info);
        LOG_DEBUG_M(Unit::CUBE, Stage::NA) << mm.bid << "Generate L0B load request. " << *uop
            << " src base:0x" << std::hex << mm.src1 << std::dec << " m:" << mm.m << " tileK:" << mm.tileSizeK
                << " tileSizeN:" << mm.tileSizeN << " elementWidth:" << elementWidth << " " << req;

        if (core->pTracer->IsEnabled()) {
            core->pTracer->PushInstrEvent(mm.bid, uop, InstrEvent::REQ);
        }
        cubeStats->totalLoadCount++;
    }
    if (reqCount == 0 && mm.srcBTileDataSize % dataWidth != 0) {
        req = CubeTileRegLdReq();
        req.cubeUopIndex = uop->globleUopIndex;
        req.isL0BLoad = true;
        req.cubeTileIndex = beginIndex;
        req.SetSize(128);
        req.SetSrc(tileBaseAddr);
        info.req = req;
        info.bid = uop->bid;
        info.bufferPtr = &L0B[threadId][uop->l0BBufferIndex];
        info.bufferPtr->loadCount++;
        info.offset = 0;
        info.tileOpIndex = uop->uopIndex;
        info.accFlg = uop->accChainId;
        info.stid = uop->stid;
        uop->uopLoadGenCount++;
        tileLoadReqBuffer[threadId].Write(info);
        LOG_DEBUG_M(Unit::CUBE, Stage::NA) << mm.bid << "Generate L0B load request. " << *uop
            << " src base:0x" << std::hex << mm.src1 << std::dec << " m:" << mm.m << " tileK:" << mm.tileSizeK
                << " tileSizeN:" << mm.tileSizeN << " elementWidth:" << elementWidth << " " << req;

        if (core->pTracer->IsEnabled()) {
            core->pTracer->PushInstrEvent(mm.bid, uop, InstrEvent::REQ);
        }
        cubeStats->totalLoadCount++;
    }
}

void CubeCore::GenLeftScaleLoadReq(shared_ptr<CubeUop> uop)
{
    if (perfectScaleLoad) {
        leftScaleBuffer[uop->leftScaleIndex].ready = true;
        return;
    }
    MatrixManager& mm = bidCubeSMMap[uop->stid][uop->bid];
    const uint64_t dataWidth = core->GetConfig().singleTierMode ? 512 : 256;
    CubeTileRegLdReq req;
    TileLoadInfo info;
    info.bufferType = "L0LeftScale";
    uint64_t leftScaleTileK = mm.tileSizeK / 32;
    uint64_t leftScaleTileSize = leftScaleTileK * 16;
    if (mm.src0Type == DataType::HIF4) {
        leftScaleTileSize = mm.tileSizeM * mm.tileSizeK / 64 * 32; // 每64个元素共享32位的缩放因子
    }
    uint64_t includeNum = dataWidth / leftScaleTileSize;
    uint64_t tileIndex = uop->srcA.first * mm.tileK + uop->srcA.second;
    uint64_t tileOffset = (tileIndex - (tileIndex % includeNum)) * leftScaleTileSize;
    uint64_t tileStartAddr = mm.srcMXLeft;
    uint64_t tileBaseAddr = tileStartAddr + tileOffset / GetSim()->core->tileUtils.configs.bank_element_size;
    leftScaleBuffer[uop->leftScaleIndex].tileRegAddr = tileBaseAddr;
    req = CubeTileRegLdReq();
    req.cubeUopIndex = uop->globleUopIndex;
    req.SetSize(MAX_TILE_DATA_BYTE);
    req.SetSrc(tileBaseAddr / GetSim()->core->tileUtils.configs.bank_element_size);
    info.req = req;
    info.bid = uop->bid;
    info.tileOpIndex = uop->uopIndex;
    info.bufferPtr = &leftScaleBuffer[uop->leftScaleIndex];
    info.bufferPtr->loadCount++;
    info.offset = 0;
    info.uop = uop;
    info.accFlg = uop->accChainId;
    uop->uopLoadGenCount++;
    tileLoadReqBuffer[0].Write(info);
    cubeStats->totalLoadCount++;
}

void CubeCore::GenRightScaleLoadReq(shared_ptr<CubeUop> uop)
{
    if (perfectScaleLoad) {
        rightScaleBuffer[uop->rightScaleIndex].ready = true;
        return;
    }
    MatrixManager& mm = bidCubeSMMap[uop->stid][uop->bid];
    const uint64_t dataWidth = core->GetConfig().singleTierMode ? 512 : 256;
    CubeTileRegLdReq req;
    TileLoadInfo info;
    info.bufferType = "L0RightScale";
    uint64_t rightScaleTileK = mm.tileSizeK / 32;
    uint64_t rightScaleTileSize = rightScaleTileK * 16;
    if (mm.src1Type == DataType::HIF4) {
        rightScaleTileSize = mm.tileSizeM * mm.tileSizeK / 64 * 32; // 每64个元素共享32位的缩放因子
    }
    uint64_t includeNum = dataWidth / rightScaleTileSize;
    uint64_t tileIndex = uop->srcA.second * mm.tileK + uop->srcA.first;
    uint64_t tileOffset = (tileIndex - (tileIndex % includeNum)) * rightScaleTileSize;
    uint64_t tileStartAddr = mm.srcMXRight;
    uint64_t tileBaseAddr = tileStartAddr + tileOffset / GetSim()->core->tileUtils.configs.bank_element_size;
    rightScaleBuffer[uop->rightScaleIndex].tileRegAddr = tileBaseAddr;
    req = CubeTileRegLdReq();
    req.cubeUopIndex = uop->globleUopIndex;
    req.SetSize(MAX_TILE_DATA_BYTE);
    req.SetSrc(tileBaseAddr / GetSim()->core->tileUtils.configs.bank_element_size);
    info.req = req;
    info.bid = uop->bid;
    info.tileOpIndex = uop->uopIndex;
    info.bufferPtr = &rightScaleBuffer[uop->rightScaleIndex];
    info.bufferPtr->loadCount++;
    info.offset = 0;
    info.uop = uop;
    info.accFlg = uop->accChainId;
    uop->uopLoadGenCount++;
    tileLoadReqBuffer[0].Write(info);
    cubeStats->totalLoadCount++;
}

void CubeCore::GenBufferCLoadReq(uint64_t threadId, shared_ptr<CubeUop> uop)
{
    MatrixManager& mm = bidCubeSMMap[uop->stid][uop->bid];
    const uint64_t dataWidth = 256;
    uint64_t elementWidth = BytesOf(mm.src2Type);
    uint64_t reqCount = mm.tileSizeM * mm.tileSizeN * elementWidth / dataWidth;
    CubeTileRegLdReq req;
    req.SetStid(uop->stid);
    TileLoadInfo info;
    uint64_t tileOffset = uop->srcC.second * mm.tileSizeN * mm.tileSizeM * mm.tileM * elementWidth
                          + uop->srcC.first * mm.tileSizeN * mm.tileSizeM * elementWidth;
    uint64_t tileStartAddr = mm.src2;
    uint64_t tileBaseAddr = tileStartAddr + tileOffset / GetSim()->core->tileUtils.configs.bank_element_size;
    for (size_t i = 0; i < reqCount; i++) {
        req = CubeTileRegLdReq();
        req.cubeUopIndex = uop->globleUopIndex;
        req.isL0CLoad = true;
        req.cubeTileIndex = uop->srcC;
        req.SetSize(MAX_TILE_DATA_BYTE);
        req.SetSrc(tileBaseAddr + i * dataWidth / GetSim()->core->tileUtils.configs.bank_element_size);
        info.req = req;
        info.bid = uop->bid;
        info.bufferPtr = &BufferC[threadId][uop->BuffeCrIndex];
        info.bufferPtr->loadCount++;
        info.offset = i * dataWidth / sizeof(uint8_t);
        info.tileOpIndex = uop->uopIndex;
        uop->uopLoadGenCount++;
        info.stid = uop->stid;
        tileLoadReqBuffer[threadId].Write(info);
        LOG_DEBUG_M(Unit::CUBE, Stage::NA) << " Bid" << mm.bid  << " Generate Matrix C load request. " << *uop
            << " src base:0x" << std::hex << mm.src2 << std::dec << " m:" << mm.m << " tileSizeN:" << mm.tileSizeN
                << " tileSizeM:" << mm.tileSizeM << " elementWidth:" << elementWidth << " " << req;

        if (core->pTracer->IsEnabled()) {
            core->pTracer->PushInstrEvent(mm.bid, uop, InstrEvent::REQ);
        }
        cubeStats->totalLoadCount++;
    }
}

void CubeCore::GetACCTileData(shared_ptr<CubeUop> uop)
{
    uint64_t bid = uop->bid;
    MatrixManager& mm = bidCubeSMMap[uop->stid][bid];
    uint64_t dependBid = mm.dependBid;
    auto index = uop->srcC;
    auto tilePtr = l0cRenameInfoMap[uop->stid][bid].allocMap[index];
    if (tilePtr->ready && tilePtr->bid == dependBid) {
        BufferC[0][uop->BuffeCrIndex].data = tilePtr->data;
        BufferC[0][uop->BuffeCrIndex].alreadyGen = true;
        BufferC[0][uop->BuffeCrIndex].ready = true;
        return;
    }
}

void CubeCore::SendTileLoad2TileReg()
{
    bool empty = true;
    for (uint32_t stid = 0; stid < cubeROB.size(); ++stid) {
        if (cubeROB[stid].Size() != 0) {
            empty = false;
        }
    }
    if (empty) {
        return;
    }
    if (pTileUtils->IsRingOrXbarMode(true)) {
        for (size_t i = 0; i < cubeStateMachine.size(); i++) {
            LOG_DEBUG_M(Unit::CUBE, Stage::NA) << "loadIdPool[i].empty(): " << loadIdPool[i].empty()
                << " tileLoadReqBuffer[i].Empty(): " << tileLoadReqBuffer[i].Empty()
                << " HasValidSpbReadEntry(): " << HasValidSpbReadEntry();
            while (!loadIdPool[i].empty() && !tileLoadReqBuffer[i].Empty() && HasValidSpbReadEntry()) {
                auto info = PickLoadReq();
                uint64_t reqId = GetLdId(i, info.bid, info.stid);
                auto station = GetFreeReadStation();
                auto pkt = station->Rxreq(info.req.GetSrc(), info.req.GetStid());
                pkt->SetSize(info.req.GetSize());
                pkt->SetBufId(reqId);
                pkt->SetCoreId(coreIndex);
                pkt->SetPEType(MachineType::CUBE);
                pkt->uopId = info.req.cubeUopIndex;
                station->WriteSpb(pkt);
                ASSERT(i < TileLoadReqIdMap.size());
                TileLoadReqIdMap[i][reqId] = info;
                // set tileop first load start
                SetTileOpLdStartCycle(info.bid, info.stid);
                // for debugging
                AddRequestToTileRequestMap(pkt);
                PrintRingPkt("[Cube Core] Send tile load request to ring. ", pkt);
            }
        }
        return;
    }
    uint64_t count = 0;
    uint64_t reqId;
    TileLoadInfo info;
    CubeTileRegLdReq req;
    bool tileBankArbSuaccelss = true;
    bool needReadRF = false;
    for (size_t i = 0; i < cubeStateMachine.size(); i++) {
        if (!loadIdPool[i].empty() && !tileLoadReqBuffer[i].Empty()) {
            needReadRF = true;
            break;
        }
    }
    if (core->GetConfig().singleTierMode && needReadRF && (needReadRFArb < 0)) {
        tileBankArbSuaccelss = core->CanReadTileReg(MachineType::CUBE);
        if (tileBankArbSuaccelss) {
            core->AcquireReadTileReg(MachineType::CUBE, core->GetConfig().cubeReadOccTileregLat);
            if (needReadRFArb < 0) {  // needWriteRFArb为-1表示这是第一次仲裁
                needReadRFArb = 2;
            }
        }
    }
    if (!tileBankArbSuaccelss && (needReadRFArb < 0)) {
        return;
    };
    if (needReadRFArb >= 0) {
        needReadRFArb--;
    }
    for (size_t i = 0; i < cubeStateMachine.size(); i++) {
        while (count < sendReqNum && !loadIdPool[i].empty() && !tileLoadReqBuffer[i].Empty()) {
            info = tileLoadReqBuffer[i].Read();
            req = info.req;
            reqId = GetLdId(i, info.bid, info.stid);
            req.SetReqId(reqId);
            LOG_DEBUG_M(Unit::CUBE, Stage::NA) << "Sending ld req to tmu";
            ASSERT(i < CubeTileLdReqQ.size());
            CubeTileLdReqQ[i]->Write(req);
            core->cubeRealReadRFReqCnt++;
            ASSERT(i < TileLoadReqIdMap.size());
            TileLoadReqIdMap[i][reqId] = info;
            // set tileop first load start
            SetTileOpLdStartCycle(info.bid, info.stid);
        }
    }
}

void CubeCore::SetTileOpLdStartCycle(uint64_t bid, uint32_t stid)
{
    if (bidCubeSMMap[stid][bid].loadRange.first == 0) {
        bidCubeSMMap[stid][bid].loadRange.first = GetSim()->getCycles();
    }
}

void CubeCore::L0BufferWork()
{
    bool empty = true;
    for (uint32_t stid = 0; stid < cubeROB.size(); ++stid) {
        if (cubeROB[stid].Size() != 0) {
            empty = false;
        }
    }
    if (empty) {
        return;
    }
    // all should be moved to l0a/b ready check
    for (size_t i = 0; i < cubeStateMachine.size(); i++) {
        for (size_t j = 0; j < L0A[i].size(); j++) {
            if (L0A[i][j].loadCount != 0 && L0A[i][j].loadCount == L0A[i][j].readyCount) {
                L0A[i][j].ready = true;
                LOG_DEBUG_M(Unit::CUBE, Stage::NA) << L0A[i][j].bid << "L0A ready. " << L0A[i][j];
            }
        }
        for (size_t k = 0; k < L0B[i].size(); k++) {
            if (L0B[i][k].loadCount != 0 && L0B[i][k].loadCount == L0B[i][k].readyCount) {
                L0B[i][k].ready = true;
                LOG_DEBUG_M(Unit::CUBE, Stage::NA) << L0B[i][k].bid << "L0B ready. " << L0B[i][k];
            }
        }
        // should be moved to bufferc ready check
        for (size_t l = 0; l < BufferC[i].size(); l++) {
            LOG_DEBUG_M(Unit::CUBE, Stage::NA) << BufferC[i][l].bid << "BufferC[" << l << "] loopK = " <<
                                                BufferC[i][l].loopKCount << "/" << BufferC[i][l].loopKBound;
            if (BufferC[i][l].loadCount != 0 && BufferC[i][l].loadCount == BufferC[i][l].readyCount) {
                BufferC[i][l].ready = true;
            }
        }
        for (size_t m = 0; m < leftScaleBuffer.size(); m++) {
            if (leftScaleBuffer[m].loadCount != 0 && leftScaleBuffer[m].loadCount == leftScaleBuffer[m].readyCount) {
                leftScaleBuffer[m].ready = true;
            }
            if (rightScaleBuffer[m].loadCount != 0 && rightScaleBuffer[m].loadCount == rightScaleBuffer[m].readyCount) {
                rightScaleBuffer[m].ready = true;
            }
        }
    }
}

void CubeCore::TileOpRslv(uint64_t accFlg, uint32_t stid)
{
    if (chainROB[stid][accFlg].Empty()) {
        return;
    }
    auto info = chainROB[stid][accFlg].Front();
    MatrixManager& mm = bidCubeSMMap[info.blockCmd->stid][info.blockCmd->bid.val];
    if (mm.retireUopCount == mm.totalUopCount && mm.stage != MMStage::IDLE && mm.renamed) {
        if (mm.op == TileOp::ACCCVT && mm.totalStoreCount != mm.alreadyStoreCount) {
            return;
        }
        cubeStats->totalUopExeCycles += GetSim()->getCycles() - mm.firstUopL0CwrCycle;
        cubeStats->totalToCoreCycles += mm.toCoreCycle - mm.firstTMUToCoreCycle;
        PushDFX(info.blockCmd->bid.val, info.blockCmd->stid);
        if (mm.op != TileOp::ACCCVT) {
            // exeTileOpChain.erase(std::find(exeTileOpChain.begin(), exeTileOpChain.end(), mm.bid.val));
            exeTileOpChain.erase(find_if(exeTileOpChain.begin(), exeTileOpChain.end(), [&mm](ChainTileInfo& info) {
                return (info.stid == mm.blockCmd->stid && info.bid == mm.bid.val);
            }));
        }
        l0cRenameAccTagMap[stid][mm.ACC_flag].lastRslvBid = mm.bid.val;
        if (mm.op == TileOp::ACCCVT) {
            l0cRenameAccTagMap[stid].erase(l0cRenameAccTagMap[stid].find(mm.ACC_flag));
            extantAccChainId[mm.blockCmd->stid].erase(
                std::find(extantAccChainId[mm.blockCmd->stid].begin(), extantAccChainId[mm.blockCmd->stid].end(),
                mm.ACC_flag));
            (*mutiCoreChainInfo)[coreIndex][stid].erase(std::find((*mutiCoreChainInfo)[coreIndex][stid].cbegin(),
                                                                  (*mutiCoreChainInfo)[coreIndex][stid].cend(),
                                                                  info.blockCmd->accChainId));
            info.blockCmd->cmdExecCompleted = true;
            cubeBCCWakeupQ->Write(info.blockCmd);
        }
        GetSim()->core->bctrl->blockROB.completeBlock(mm.bid, true, mm.blockCmd->stid);
        LOG_INFO_M(Unit::CUBE, Stage::NA) << "Cube Resolve B" << dec << mm.bid;
        cubeRsvlCount++;
        globalUopMap[mm.blockCmd->stid][mm.bid.val].clear();
        if (retireList.size() == retireListSize) {
            retireList.erase(retireList.begin());
        }
        if (info.blockCmd->tileOp == TileOp::ACCCVT) {
            accNum--;
        }
        retireList.push_back(chainROB[stid][accFlg].Read());
        auto& readQ = cubeROB[stid].GetRawReadData();
        for (auto it = readQ.begin(); it != readQ.end(); it++) {
            if (it->blockCmd->bid.val == info.blockCmd->bid.val) {
                readQ.erase(it);
                break;
            }
        }
        blockCmdBufferIDPool.push_back(mm.blockCmd->execMachineId - machineId);
        SwimLogData logData;
        logData.name = std::to_string(mm.bid.val) + " " + GetTileOpName(mm.op);
        logData.pid = CORE_TOP_MACHINE_ID;
        logData.tid = mm.blockCmd->execMachineId;
        logData.sTime = mm.calculateStartCycle;
        logData.eTime = GetSim()->getCycles();
        logData.hint = mm.DumpSwimInfo();
        mm.blockCmd->eventId = (mm.blockCmd->eventId >= 0) ? mm.blockCmd->eventId : GetSim()->GetEventId();
        logData.eventId = mm.blockCmd->eventId;
        GetSim()->AddDuration(logData);
        GetSim()->AddDependency(mm.bid.val, mm.blockCmd->eventId, mm.blockCmd->stid);
        if (mm.loadRange.first != 0) {
            cubeStats->loadRange.cycleRange.push_back(mm.loadRange);
        }
        if (mm.mmadRange.first != 0) {
            cubeStats->mmadRange.cycleRange.push_back(mm.mmadRange);
        }
        if (mm.acccvtRange.first != 0) {
            cubeStats->acccvtRange.cycleRange.push_back(mm.acccvtRange);
        }
        mm.Reset();
    }
}

void CubeCore::PushDFX(uint64_t bid, uint32_t stid)
{
    MatrixManager& mm = bidCubeSMMap[stid][bid];
    if (cubeDataCheck && mm.blockCmd->tileOp != TileOp::ACCCVT) {
        std::shared_ptr<TileRegVerifyData> cubeData = std::make_shared<TileRegVerifyData>();
        cubeData->shapeM = mm.m;
        cubeData->shapeN = mm.n;
        cubeData->data = cubeVerifyDataMap[stid][mm.bid.val];
        if (cubeData->data.size() != mm.m * mm.n) {
            cubeData->data.resize(mm.m * mm.n);
        }
        GetSim()->GetVerifyManager(mm.blockCmd->stid)->RecordBlockTileRegData(mm.bid.val, cubeData);
        cubeVerifyDataMap[stid].erase(cubeVerifyDataMap[stid].find(mm.bid.val));
    }
}

bool CubeCore::CheckUopReady(uint64_t threadId, shared_ptr<CubeUop> uop)
{
    MatrixManager& mm = bidCubeSMMap[uop->stid][uop->bid];
    bool readyA = CheckSrcAReady(threadId, uop->l0ABufferIndex) && uop->allocL0A;
    bool readyB = CheckSrcBReady(threadId, uop->l0BBufferIndex) && uop->allocL0B;
    bool readyC = CheckSrcCReady(threadId, uop) && uop->allocBufferC;

    if (readyA && uop->srcAReadyCycle == 0) {
        uop->srcAReadyCycle = GetSim()->getCycles();
    }
    if (readyB && uop->srcBReadyCycle == 0) {
        uop->srcBReadyCycle = GetSim()->getCycles();
    }
    if (readyC && uop->srcCReadyCycle == 0) {
        uop->srcCReadyCycle = GetSim()->getCycles();
    }
    if (perfectLoad) {
        bool loopkCheck = BufferC[0][uop->BuffeCrIndex].loopKCount == uop->srcA.second;
        if (!mm.needScale) {
            return uop->allocL0A && uop->allocL0B && readyC && loopkCheck;
        } else {
            return uop->allocL0A && uop->allocL0B && readyC && loopkCheck
                   && uop->allocLeftScale && uop->allocRightScale;
        }
    }
    bool loopkCheck = BufferC[0][uop->BuffeCrIndex].loopKCount == uop->srcA.second;
    if (!mm.needScale) {
        return readyA && readyB && readyC && loopkCheck;
    } else {
        return readyA && readyB && readyC && loopkCheck && uop->allocLeftScale && uop->allocRightScale
               && leftScaleBuffer[uop->leftScaleIndex].ready && rightScaleBuffer[uop->rightScaleIndex].ready;
    }
}

bool CubeCore::CheckSrcAReady(uint64_t threadId, uint64_t bufferIndex)
{
    return L0A[threadId][bufferIndex].ready;
}

bool CubeCore::CheckSrcBReady(uint64_t threadId, uint64_t bufferIndex)
{
    return L0B[threadId][bufferIndex].ready;
}

bool CubeCore::CheckSrcCReady(uint64_t threadId, shared_ptr<CubeUop> uop)
{
    return BufferC[threadId][uop->BuffeCrIndex].ready;
}

void CubeCore::SendL0CStReq2TileReg()
{
    bool empty = true;
    for (uint32_t stid = 0; stid < cubeROB.size(); ++stid) {
        if (cubeROB[stid].Size() != 0) {
            empty = false;
        }
    }
    if (empty) {
        return;
    }
    if (pTileUtils->IsRingOrXbarMode(true)) {
        for (size_t i = 0; i < cubeStateMachine.size(); i++) {
            while (!tileStoreReqBuffer[i].Empty() && HasValidSpbWriteEntry()) {
                auto req = tileStoreReqBuffer[i].Read();
                auto station = GetFreeWriteStation();
                auto pkt = station->Rxdat(req.GetDest(), req.GetStid());
                pkt->cmd = bidCubeSMMap[req.GetStid()][req.GetBid().val].blockCmd;
                pkt->SetSize(req.GetSize());
                pkt->SetBufId(req.GetReqId());
                pkt->SetData(req.GetData());
                pkt->SetCoreId(coreIndex);
                pkt->SetPEType(MachineType::CUBE);
                station->WriteSpb(pkt);
                // when first tstore send means acccvt begin in swimlane
                SetSwimLaneStartCycle(req.GetBid().val, req.GetStid());
                PrintRingPkt("[Cube Core] Send tile store request to ring. ", pkt);
            }
        }
    } else {
        uint64_t count = 0;
        bool tileBankArbSuaccelss = true;
        bool needWriteRF = false;
        for (size_t i = 0; i < cubeStateMachine.size(); i++) {
            if (!tileStoreReqBuffer[i].Empty()) {
                needWriteRF = true;
                break;
            }
        }
        auto handleTileBankArbitration = [this, &needWriteRF]() -> bool {
            if (!core->GetConfig().singleTierMode || (needWriteRFArb >= 0) || !needWriteRF) {
                return true;
            }
            if (!core->CanWriteTileReg(MachineType::CUBE)) {
                return false;
            }
            core->AcquireWriteTileReg(MachineType::CUBE, core->GetConfig().cubeWriteOccTileregLat);
            if (needWriteRFArb < 0) {
                needWriteRFArb = 6;
            }
            return true;
        };
        tileBankArbSuaccelss = handleTileBankArbitration();
        if (!tileBankArbSuaccelss && (needWriteRFArb < 0)) {
            return;
        };
        if (needWriteRFArb >= 0) {
            needWriteRFArb--;
        }
        for (size_t i = 0; i < cubeStateMachine.size(); i++) {
            while (count < sendReqNum && !tileStoreReqBuffer[i].Empty()) {
                auto req = tileStoreReqBuffer[i].Read();
                CubeTileStReqQ[i]->Write(req);
                core->cubeRealWriteRFReqCnt += 1;
                count++;
                // when first tstore send means acccvt begin in swimlane
                SetSwimLaneStartCycle(req.GetBid().val, req.GetStid());
            }
        }
    }
}

void CubeCore::ExeFifoWork()
{
    bool empty = true;
    for (uint32_t stid = 0; stid < cubeROB.size(); ++stid) {
        if (cubeROB[stid].Size() != 0) {
            empty = false;
        }
    }
    if (empty) {
        return;
    }
    bool hasCubeOp = false;
    for (size_t i = 0; i < cubeStateMachine.size(); i++) {
        ExeFifoThreadWork(i);
        if (cubeStateMachine[i].stage != MMStage::IDLE) {
            hasCubeOp = true;
        }
    }
    sim->cubeRunning = hasCubeOp;
}

void CubeCore::ExeFifoThreadWork(uint64_t threadId)
{
    if (!uopExeFifo[threadId].Empty()) {
        cubeStats->maddBusyCycle++;
        auto& readQ = uopExeFifo[threadId].GetRawReadData();
        for (auto uopIt = readQ.cbegin(); uopIt != readQ.cend(); uopIt++) {
            FreeL0BufferEntry(*uopIt);
            PreWeakUpBufferc(*uopIt);
        }
        auto uop = uopExeFifo[threadId].Front();
        MatrixManager& mm = bidCubeSMMap[uop->stid][uop->bid];
        if (uop->endCycle <= GetSim()->getCycles()) {
            uop->commitCycle = GetSim()->getCycles();
            if (mm.firstUopL0CwrCycle == 0) {
                mm.firstUopL0CwrCycle = uop->commitCycle;
                cubeStats->headerOverHeadCycle +=  mm.firstUopL0CwrCycle - mm.issueToROBCycle;
            }
            // update tileop last mmad end cycle
            bidCubeSMMap[uop->stid][uop->bid].mmadRange.second = GetSim()->getCycles();
            uopExeFifo[threadId].Read()->complete = true;
            uop->retireCycle = GetSim()->getCycles();
            mm.retireUopCount++;
            LOG_INFO_M(Unit::CUBE, Stage::NA) << "uop Resolve cnt:" << dec << mm.retireUopCount << " B" << mm.bid << " " << *uop;
            PrintCubeUopPipeView(uop);
            if (core->pTracer->IsEnabled()) {
                core->pTracer->PushInstrEvent(mm.bid, uop, InstrEvent::DONE);
            }
        }
    }
}

void CubeCore::CalcTileMul(uint64_t threadId, shared_ptr<CubeUop> uop)
{
    MatrixManager& mm = bidCubeSMMap[uop->stid][uop->bid];
    float_status status = {
        float_round_nearest_even, 0, floatx80_precision_x, false, false, false, false, false, false, false
    };
    vector<vector<uint64_t>> tileA(mm.tileSizeM, vector<uint64_t>(mm.tileSizeK, 0));
    vector<vector<uint64_t>> tileB(mm.tileSizeK, vector<uint64_t>(mm.tileSizeN, 0));
    vector<vector<uint64_t>> tileC(mm.tileSizeM, vector<uint64_t>(mm.tileSizeN, 0));
    L0Buffers& bufferA = L0A[threadId][uop->l0ABufferIndex];
    L0Buffers& bufferB = L0B[threadId][uop->l0BBufferIndex];
    L0Buffers& bufferC = BufferC[threadId][uop->BuffeCrIndex];
    PaddingTile(tileA, bufferA, mm.src0Type, TileFormat::Z);
    PaddingTile(tileB, bufferB, mm.src0Type, TileFormat::N);
    PaddingTile(tileC, bufferC, mm.dstType, TileFormat::Z);
    MaskLeftTile(uop, tileA);
    MaskRightTile(uop, tileB);
    for (size_t i = 0; i < tileC.size(); i++) {
        for (size_t j = 0; j < tileC[0].size(); j++) {
            for (size_t k = 0; k < mm.tileSizeK; k++) {
                tileC[i][j] = CubeCalculate::EleAdd(tileC[i][j],
                                             CubeCalculate::EleMul(tileA[i][k],
                                                                   tileB[k][j],
                                                                   mm.accType,
                                                                   status),
                                             mm.accType,
                                             status);
            }
        }
    }
    WriteBufferC(threadId, uop, tileC);
}

void CubeCore::MaskLeftTile(shared_ptr<CubeUop> uop, vector<vector<uint64_t>>& tileA)
{
    MatrixManager& mm = bidCubeSMMap[uop->stid][uop->bid];
    for (size_t i = uop->srcAMaskR; i < mm.tileSizeM; i++) {
        for (size_t j = 0; j < mm.tileSizeK; j++) {
            tileA[i][j] = 0;
        }
    }
    for (size_t j = uop->srcAMaskC; j < mm.tileSizeK; j++) {
        for (size_t i = 0; i < mm.tileSizeM; i++) {
            tileA[i][j] = 0;
        }
    }
}

void CubeCore::MaskRightTile(shared_ptr<CubeUop> uop, vector<vector<uint64_t>>& tileB)
{
    MatrixManager& mm = bidCubeSMMap[uop->stid][uop->bid];
    for (size_t i = uop->srcBMaskR; i < mm.tileSizeK; i++) {
        for (size_t j = 0; j < mm.tileSizeN; j++) {
            tileB[i][j] = 0;
        }
    }
    for (size_t j = uop->srcBMaskC; j < mm.tileSizeN; j++) {
        for (size_t i = 0; i < mm.tileSizeK; i++) {
            tileB[i][j] = 0;
        }
    }
}

void CubeCore::WriteBufferC(uint64_t threadId, shared_ptr<CubeUop> uop, vector<vector<uint64_t>>& tileC)
{
    uint64_t offset = 0;
    MatrixManager& mm = bidCubeSMMap[uop->stid][uop->bid];
    L0Buffers& bufferC = BufferC[threadId][uop->BuffeCrIndex];
    uint64_t elementWidth = mm.accElementWidth;
    for (size_t i = 0; i < tileC.size(); i++) {
        for (size_t j = 0; j < tileC[0].size(); j++) {
            if (offset + elementWidth > bufferC.data.size()) {
                std::cerr << "write overflow at i=" << i << ", j=" << j << ", offset=" << offset << std::endl;
                ASSERT(0);
            }
            bufferC.Store(offset, elementWidth, tileC[i][j]);
            offset += elementWidth;
        }
    }
    bufferC.dataLayout = tileC;
    bufferC.bid = uop->bid;
}


void CubeCore::PaddingTile(vector<vector<uint64_t>>& targetVector, L0Buffers& srcBuffer,
                           DataType datatype, TileFormat format)
{
    uint64_t elementWidth = BytesOf(datatype);
    if (format == TileFormat::Z) {
        uint64_t offset = 0;
        for (size_t i = 0; i < targetVector.size(); i++) {
            for (size_t j = 0; j < targetVector[0].size(); j++) {
                targetVector[i][j] = srcBuffer.Load(offset, elementWidth);
                offset += elementWidth;
            }
        }
    }
    if (format == TileFormat::N) {
        uint64_t offset = 0;
        for (size_t j = 0; j < targetVector[0].size(); j++) {
            for (size_t i = 0; i < targetVector.size(); i++) {
                targetVector[i][j] = srcBuffer.Load(offset, elementWidth);
                offset += elementWidth;
            }
    }
}
}

void CubeCore::PreWriteACC(uint64_t threadId, L0Buffers& tileC)
{
    auto accPtr = l0cRenameInfoMap[tileC.stid][tileC.bid].allocMap[tileC.index[0]];
    accPtr->data = tileC.data;
    accPtr->bid = tileC.bid;
    accPtr->ready = true;
    accPtr->dataLayout = tileC.dataLayout;
}

void CubeCore::PreWeakUpBufferc(shared_ptr<CubeUop> uop)
{
    if (uop->bufcWrCycle == GetSim()->getCycles() && !uop->preWrite) {
        uop->preWrite = true;
        uint64_t bufferCIdx = uop->BuffeCrIndex;
        BufferC[0][bufferCIdx].loopKCount++;
        // if this uop is the last madd in loopk, write directly to acc
        if (BufferC[0][bufferCIdx].loopKBound != 0 &&
            BufferC[0][bufferCIdx].loopKCount == BufferC[0][bufferCIdx].loopKBound) {
            PreWriteACC(0, BufferC[0][bufferCIdx]);
            GenTileVerifyData(BufferC[0][bufferCIdx], uop->stid);
            BufferC[0][bufferCIdx].FreeAndReset();
        } else {
            BufferC[0][bufferCIdx].ready = true; // if not last, pre-weak up related uop
        }
    }
}

void CubeCore::FreeL0BufferEntry(shared_ptr<CubeUop> uop)
{
    MatrixManager& mm = bidCubeSMMap[uop->stid][uop->bid];
    if (uop->l0abRdCycle == GetSim()->getCycles()) {
        CalcTileMul(0, uop);
        L0A[0][uop->l0ABufferIndex].dependenceCount--;
        L0B[0][uop->l0BBufferIndex].dependenceCount--;
        if (mm.needScale) {
            leftScaleBuffer[uop->leftScaleIndex].dependenceCount--;
            rightScaleBuffer[uop->rightScaleIndex].dependenceCount--;
        }
        ASSERT(L0A[0][uop->l0ABufferIndex].dependenceCount < cfgs.l0a_buffer_entry);
        ASSERT(L0B[0][uop->l0BBufferIndex].dependenceCount < cfgs.l0b_buffer_entry);
        LOG_DEBUG_M(Unit::CUBE, Stage::LW) << mm.bid.val << "CALC done, loopKCount: "
             << BufferC[0][uop->BuffeCrIndex].loopKCount << " write data to L0CBuffer" << (*uop);
    }
}

void CubeCore::FreeL0AL0B()
{
    for (size_t i = 0; i < cubeStateMachine.size(); i++) {
        MatrixManager& mm = cubeStateMachine[i];
        for (size_t j = 0; j < L0A[i].size(); j++) {
            L0A[i][j].FreeAndReset();
            L0B[i][j].FreeAndReset();
        }
        for (size_t j = 0; j < bufferCCount; j++) {
            BufferC[i][j].FreeAndReset();
        }
        mm.l0ASize = 0;
        mm.l0BSize = 0;
        l0aLru.clear();
        l0bLru.clear();
    }
}

void CubeCore::GenTileVerifyData(L0Buffers& bufferC, uint32_t stid)
{
    MatrixManager& mm = bidCubeSMMap[stid][bufferC.bid];
    uint64_t offset = (bufferC.index[0].second * mm.tileM + bufferC.index[0].first) * mm.tileSizeM * mm.tileSizeN;
    for (size_t i = 0; i < mm.tileSizeM * mm.tileSizeN; i++) {
        cubeVerifyDataMap[stid][bufferC.bid][i + offset] = bufferC.Load(i * mm.accElementWidth, mm.accElementWidth);
    }
}

void CubeCore::PrintPipeViewLog(shared_ptr<CubeUop> uop, uint64_t threadId)
{
    uint64_t loadCycle = uop->calcStartCycle - uop->startCycle - 1;
    cubeStats->totalLoadCycle += loadCycle;
    cubeStats->cubeLoadLatencyMap[loadCycle]++;
    cubeStats->totalExeUopCount++;
    cubeStats->retireCycle += uop->retireCycle - uop->commitCycle;
    // Please use the new ViewManager to print the pipeview
}

void CubeCore::PrintACCCVTPipeView(shared_ptr<ACCCVTUop> uop)
{
    InstPipeViewPtr instInfo = std::make_shared<InstPipeViewInfo>();
    instInfo->bid = uop->bid;
    instInfo->cycleInfo = std::make_shared<CycleInfo>();
    instInfo->cycleInfo->ACCCVTStartCycle = uop->ACCCVTStartCycle;
    instInfo->cycleInfo->ACCCVTRenameCycle = uop->ACCCVTRenameCycle;
    instInfo->cycleInfo->ACCCVTIssueCycle = uop->ACCCVTIssueCycle;
    instInfo->cycleInfo->ACCCVTArbCycle = uop->ACCCVTArbCycle;
    instInfo->cycleInfo->ACCCVTWaitCycle = uop->ACCCVTWaitCycle;
    instInfo->cycleInfo->ACCCVTSrcRdyCycle = uop->ACCCVTSrcRdyCycle;
    instInfo->cycleInfo->ACCCVTSrcDataCycle = uop->ACCCVTSrcDataCycle;
    instInfo->cycleInfo->ACCCVTFixPipeCycle = uop->ACCCVTFixPipeCycle;
    instInfo->cycleInfo->retireCycle = uop->commitCycle;
    instInfo->label = "ACCCVT" + std::to_string(uop->gIndex);
    MatrixManager& mm = bidCubeSMMap[uop->stid][uop->bid];
    GetSim()->GetViewManager(mm.blockCmd->stid)->RecordMinst(instInfo);
}

void CubeCore::PrintCubeUopPipeView(shared_ptr<CubeUop> uop)
{
    InstPipeViewPtr instInfo = std::make_shared<InstPipeViewInfo>();
    instInfo->bid = uop->bid;
    instInfo->cycleInfo = std::make_shared<CycleInfo>();
    instInfo->cycleInfo->cubeUopStartCycle = uop->startCycle;
    instInfo->cycleInfo->cubeUopIssueCycle = uop->issueCycle;
    instInfo->cycleInfo->cubeUopRenameCycle = uop->startCycle + 1;
    if (uop->loadGenCycle != 0) {
        instInfo->cycleInfo->cubeUopGenloadCycle = uop->loadGenCycle;
        instInfo->cycleInfo->cubeUopWaitCycle = uop->loadGenCycle;
    } else {
        instInfo->cycleInfo->cubeUopWaitCycle = uop->waitingForLoadCycle;
    }
    if (uop->srcAReadyCycle <= instInfo->cycleInfo->cubeUopRenameCycle) {
        uop->srcAReadyCycle = instInfo->cycleInfo->cubeUopRenameCycle + 1;
    }
    instInfo->cycleInfo->cubeUopSrcAreadyCycle = uop->srcAReadyCycle;
    instInfo->cycleInfo->cubeUopSrcBreadyCycle = uop->srcBReadyCycle - 1;
    instInfo->cycleInfo->cubeUopSrcCreadyCycle = uop->srcCReadyCycle;
    instInfo->cycleInfo->cubeUopRdbufferCycle = uop->calcStartCycle;
    const uint64_t pipeCycle1 = 1;
    instInfo->cycleInfo->cubeUopCtrlCycle = uop->calcStartCycle + pipeCycle1;
    const uint64_t pipeCycle2 = 3;
    instInfo->cycleInfo->cubeUopCalcCycle = uop->calcStartCycle + pipeCycle2;
    const uint64_t pipeCycle3 = 10;
    instInfo->cycleInfo->cubeUopL0cwrCycle = uop->calcStartCycle + pipeCycle3;
    instInfo->cycleInfo->cubeUopCommitCycle = uop->commitCycle;
    instInfo->cycleInfo->retireCycle = uop->commitCycle;
    instInfo->label = "CUBE_MAD:" + std::to_string(uop->globleUopIndex) +
                      " A[" + std::to_string(uop->srcA.first) + "][" + std::to_string(uop->srcA.second) + "]" +
                      " B[" + std::to_string(uop->srcB.first) + "][" + std::to_string(uop->srcB.second) + "]" +
                      " BufC[" + std::to_string(uop->BuffeCrIndex) + "]" +
                      " L0A[" + std::to_string(uop->l0ABufferIndex) + "]:" +
                      std::to_string(L0A[0][uop->l0ABufferIndex].tileRegAddr) +
                      " L0B[" + std::to_string(uop->l0BBufferIndex) + "]:" +
                      std::to_string(L0B[0][uop->l0BBufferIndex].tileRegAddr);
    MatrixManager& mm = bidCubeSMMap[uop->stid][uop->bid];
    GetSim()->GetViewManager(mm.blockCmd->stid)->RecordMinst(instInfo);
}

void CubeCore::HandleCubeStResp()
{
    bool empty = true;
    for (uint32_t stid = 0; stid < cubeROB.size(); ++stid) {
        if (cubeROB[stid].Size() != 0) {
            empty = false;
        }
    }
    if (empty) {
        return;
    }
    if (pTileUtils->IsRingOrXbarMode(true)) {
        for (uint32_t i = stationReadRange; i < stations.size(); i++) {
            auto &station = stations[i];
            if (station->HasNoResp(coreIndex)) {
                continue;
            }
            auto pkt = station->GetDataComReq(coreIndex);
            MatrixManager& mm = bidCubeSMMap[pkt->cmd->stid][pkt->cmd->bid.val];
            mm.alreadyStoreCount++;
            PrintRingPkt(("[Cube Core] Get tile store response from ring. "), pkt);
        }
    } else {
        if (!tileRegCubeWrResQ->Empty()) {
            auto req = tileRegCubeWrResQ->Read();
            MatrixManager& mm = bidCubeSMMap[req.GetStid()][req.GetBid().val];
            mm.alreadyStoreCount++;
        }
    }
}

void CubeCore::ReportStat()
{
    std::string coreName = std::to_string(coreIndex);
    cubeStats->Report(coreName);
}

TileLoadInfo CubeCore::PickLoadReq()
{
    TileLoadInfo info = TileLoadInfo();
    PickLoadInfo(info);
    return info;
}

bool CubeCore::PickLoadInfo(TileLoadInfo& info)
{
    auto& readQ = tileLoadReqBuffer[0].GetRawReadData();
    for (auto it = readQ.begin(); it != readQ.end(); it++) {
        info = *it;
        readQ.erase(it);
        return true;
    }
    return false;
}

std::ostream& operator<<(std::ostream& out, CubeUop &uop)
{
    out << " uop index: " << uop.globleUopIndex << "  A[" << uop.srcA.first << "]["<< uop.srcA.second << "]";
    if (uop.allocL0A) {
        out << "(bufferIdx: " << uop.l0ABufferIndex << ")";
    }

    out << " * B[" << uop.srcB.first << "]["<< uop.srcB.second <<"]";
    if (uop.allocL0B) {
        out << "(bufferIdx: " << uop.l0BBufferIndex << ")";
    }

    out << " -> C[" << uop.dst.first << "]["<< uop.dst.second <<"]";
    if (uop.allocBufferC) {
        out << "(bufferIdx: " << uop.BuffeCrIndex << ")";
    }

    out << " bid " << uop.bid << " stid " << uop.stid;

    return out;
}

std::ostream& operator<<(std::ostream& out, ACCCVTUop &uop)
{
    out << " uop index: " << uop.gIndex;
    for (auto &dep : uop.dependentTileIdx) {
        if (!dep.ready) {
            out << " Dep[" << dep.index.first << "]["<< dep.index.second << "]";
        }
    }

    return out;
}

std::ostream& operator<<(std::ostream& out, L0Buffers &buffer)
{
    out << " L0buffer bufferIdx: " << buffer.bufferIdx << " dependenceCount: " << buffer.dependenceCount
        << "  buffer[" << buffer.index[0].first << "]["<< buffer.index[0].second << "]";
    return out;
}

bool CubeCore::CanGetBlkCmdFromBissue(BlockCommandPtr cmd)
{
    for (size_t i = 0; i < (*mutiCoreChainInfo)[coreIndex][cmd->stid].size(); i++) {
        if ((*mutiCoreChainInfo)[coreIndex][cmd->stid][i] == cmd->accChainId) {
            return true;
        }
    }
    for (auto it = mutiCoreChainInfo->cbegin(); it != mutiCoreChainInfo->cend(); it++) {
        if (it->first != coreIndex) {
            // std::map<uint32_t, std::vector<uint32_t>> &mutiCoreChainInfoMap = it->second;
            bool find = FindExtantChainId((*mutiCoreChainInfo)[it->first][cmd->stid], cmd);
            if (find) {
                return false;
            }
        }
    }
    return true;
}

bool CubeCore::FindExtantChainId(vector<uint64_t> chainIdArray, BlockCommandPtr cmd) const
{
    for (size_t i = 0; i < chainIdArray.size(); i++) {
        if (chainIdArray[i] == cmd->accChainId) {
            return true;
        }
    }
    return false;
}

void CubeCore::InitRowMaxBuffer(uint64_t bid, uint32_t stid)
{
    auto mm = bidCubeSMMap[stid][bid];
    if (mm.needRowMax) {
        rowMaxBufferMap[stid][bid].ready = false;
        rowMaxBufferMap[stid][bid].rowMaxValue.clear();
        rowMaxBufferMap[stid][bid].rowMaxValue.resize(mm.tileM * mm.tileSize);
    }
}

void CubeCore::GenRowMaxStore()
{
    for (uint32_t stid = 0; stid < rowMaxBufferMap.size(); ++stid) {
        for (auto it = rowMaxBufferMap[stid].cbegin(); it != rowMaxBufferMap[stid].cend(); it++) {
            auto mm = bidCubeSMMap[stid][it->first];
            if (mm.retireUopCount == mm.totalUopCount && mm.stage != MMStage::IDLE &&
                mm.renamed && mm.op == TileOp::ACCCVT) {
                GenRowMaxStReq(it->first, stid);
            }
        }
    }
}

void CubeCore::GenRowMaxStReq(uint64_t bid, uint32_t stid)
{
    auto mm = bidCubeSMMap[stid][bid];
    const uint64_t storeSize = 256;
    uint64_t addr = mm.dst1;
    uint64_t totalStoreSize = mm.tileM * mm.tileSize * mm.accElementWidth;
    uint64_t storeReqCount = (totalStoreSize + storeSize - 1) / storeSize;
    for (size_t i = 0; i < storeReqCount; i++) {
        CubeTileRegStReq req = CubeTileRegStReq();
        req.SetBid(mm.blockCmd->bid);
        req.SetDest(addr);
        req.SetSize(MAX_TILE_DATA_BYTE);
        req.SetStid(stid);
        DataMask mask = DataMask(pTileUtils->configs.mask_element_size);
        mask.Reset();
        mask.Set(0, storeSize - 1);
        if (i == storeReqCount - 1) {
            mask.Set(0, totalStoreSize - (i * storeSize) - 1);
        }
        req.SetDataMask(mask);
        tileStoreReqBuffer[0].Write(req);
    }
}

void CubeCore::SetSwimLaneStartCycle(uint64_t bid, uint32_t stid)
{
    if (bidCubeSMMap[stid][bid].calculateStartCycle == 0) {
        bidCubeSMMap[stid][bid].calculateStartCycle = GetSim()->getCycles();
    }
}

void CubeCore::SetFlush(FlushBus &signal)
{
    auto bid = signal.req.bid;
    auto& readQ = cubeROB[signal.req.stid].GetRawReadData();
    for (auto it = readQ.cbegin(); it != readQ.cend();) {
        if (NeedFlush(bid, *it, signal.req.stid)) {
            BlockFlush(*it);
            it = readQ.erase(it);
        } else {
            it++;
        }
    }
    auto& writeQ = cubeROB[signal.req.stid].GetRawWriteData();
    for (auto it = writeQ.cbegin(); it != writeQ.cend();) {
        if (NeedFlush(bid, *it, signal.req.stid)) {
            it = writeQ.erase(it);
        } else {
            it++;
        }
    }
}

bool CubeCore::NeedFlush(ROBID bid, CubeRobInfo cubeRobInfo, uint32_t stid) const
{
    return LessEqual(bid, cubeRobInfo.blockCmd->bid) && stid == cubeRobInfo.blockCmd->stid;
}

void CubeCore::BlockFlush(CubeRobInfo blkInfo)
{
    uint64_t bid = blkInfo.blockCmd->bid.val;
    // dealloc acc
    FlushDeallocACC(blkInfo, blkInfo.blockCmd->stid);
    // flush uop buffer
    FlushUopBuffer(bid, blkInfo.blockCmd->stid);
    // flush tilelduopbuffer
    FlushTileLdUopBuffer(bid, blkInfo.blockCmd->stid);
    // flash load req
    FlushLoadReq(bid, blkInfo.blockCmd->stid);
    // flush uop isq
    FlushUopFifo(bid, blkInfo.blockCmd->stid);
    // flush cube uint & fixpipe
    FlushUopExeFifo(bid, blkInfo.blockCmd->stid);
    FilterFlushedLoadRes(bid, blkInfo.blockCmd->stid);
}

void CubeCore::FlushDeallocACC(CubeRobInfo blkInfo, uint32_t stid)
{
    uint64_t accChainId = blkInfo.blockCmd->accChainId;
    uint64_t bid = blkInfo.blockCmd->bid.val;
    auto& mm = bidCubeSMMap[stid][bid];
    if (blkInfo.blockCmd->tileOp == TileOp::TMATMUL_BIAS || blkInfo.blockCmd->tileOp == TileOp::TMATMUL ||
        blkInfo.blockCmd->tileOp == TileOp::TMATMULMX_BIAS || blkInfo.blockCmd->tileOp == TileOp::TMATMULMX) {
        if (l0cRenameAccTagMap[stid].find(accChainId) != l0cRenameAccTagMap[stid].end()) {
            FreeACC(mm);
        }
    }
}

void CubeCore::FlushUopBuffer(uint64_t bid, uint32_t stid)
{
    CubeUopBuffer[stid][bid].Reset();
    auto& acccvtReadQ = ACCCVTUopBuffer.GetRawReadData();
    for (auto it = acccvtReadQ.cbegin(); it != acccvtReadQ.cend();) {
        if ((*it)->bid == bid) {
            it = acccvtReadQ.erase(it);
        } else {
            it++;
        }
    }
    auto& acccvtWriteQ = ACCCVTUopBuffer.GetRawWriteData();
    for (auto it = acccvtWriteQ.cbegin(); it != acccvtWriteQ.cend();) {
        if ((*it)->bid == bid) {
            it = acccvtWriteQ.erase(it);
        } else {
            it++;
        }
    }
}

void CubeCore::FlushTileLdUopBuffer(uint64_t bid, uint32_t stid)
{
    TileLdUopBuffer[stid][bid].Reset();
}

void CubeCore::FlushLoadReq(uint64_t bid, uint32_t stid)
{
    auto& readQ = tileLoadReqBuffer[0].GetRawReadData();
    for (auto it = readQ.cbegin(); it != readQ.cend();) {
        if ((*it).bid == bid && (*it).stid == stid) {
            it = readQ.erase(it);
        } else {
            it++;
        }
    }
    auto& writeQ = tileLoadReqBuffer[0].GetRawWriteData();
    for (auto it = writeQ.cbegin(); it != writeQ.cend();) {
        if ((*it).bid == bid && (*it).stid == stid) {
            it = writeQ.erase(it);
        } else {
            it++;
        }
    }
}

void CubeCore::FlushUopFifo(uint64_t bid, uint32_t stid)
{
    auto& readQ = CubeUopFifo.GetRawReadData();
    for (auto it = readQ.cbegin(); it != readQ.cend();) {
        if ((*it)->bid == bid && (*it)->stid == stid) {
            ISQRollBackL0EntryStat(*it);
            it = readQ.erase(it);
        } else {
            it++;
        }
    }
    auto& writeQ = CubeUopFifo.GetRawWriteData();
    for (auto it = writeQ.cbegin(); it != writeQ.cend();) {
        if ((*it)->bid == bid && (*it)->stid == stid) {
            it = writeQ.erase(it);
        } else {
            it++;
        }
    }
    auto& acccvtReadQ = ACCCVTUopFifo.GetRawReadData();
    for (auto it = acccvtReadQ.cbegin(); it != acccvtReadQ.cend();) {
        if ((*it)->bid == bid && (*it)->stid == stid) {
            it = acccvtReadQ.erase(it);
        } else {
            it++;
        }
    }
    auto& acccvtWriteQ = ACCCVTUopFifo.GetRawWriteData();
    for (auto it = acccvtWriteQ.cbegin(); it != acccvtWriteQ.cend();) {
        if ((*it)->bid == bid && (*it)->stid == stid) {
            it = acccvtWriteQ.erase(it);
        } else {
            it++;
        }
    }
}

void CubeCore::ISQRollBackL0EntryStat(shared_ptr<CubeUop> uop)
{
    if (uop->allocL0A) {
        L0A[0][uop->l0ABufferIndex].dependenceCount--;
    }
    if (uop->allocL0B) {
        L0B[0][uop->l0BBufferIndex].dependenceCount--;
    }
    if (uop->allocBufferC) {
        BufferC[0][uop->BuffeCrIndex].FreeAndReset();
    }
}

void CubeCore::FlushUopExeFifo(uint64_t bid, uint32_t stid)
{
    auto& readQ = uopExeFifo[0].GetRawReadData();
    for (auto it = readQ.cbegin(); it != readQ.cend();) {
        if ((*it)->bid == bid && (*it)->stid == stid) {
            CubeUnitRollBackL0EntryStat(*it);
            it = readQ.erase(it);
        } else {
            it++;
        }
    }
    auto& writeQ = uopExeFifo[0].GetRawWriteData();
    for (auto it = writeQ.cbegin(); it != writeQ.cend();) {
        if ((*it)->bid == bid && (*it)->stid == stid) {
            CubeUnitRollBackL0EntryStat(*it);
            it = writeQ.erase(it);
        } else {
            it++;
        }
    }
    auto& fixPipeReadQ = FixPipeFifo.GetRawReadData();
    for (auto it = fixPipeReadQ.cbegin(); it != fixPipeReadQ.cend();) {
        if ((*it)->bid == bid && (*it)->stid == stid) {
            it = fixPipeReadQ.erase(it);
        } else {
            it++;
        }
    }
    auto& fixPipeWriteQ = FixPipeFifo.GetRawWriteData();
    for (auto it = fixPipeWriteQ.cbegin(); it != fixPipeWriteQ.cend();) {
        if ((*it)->bid == bid && (*it)->stid == stid) {
            it = fixPipeWriteQ.erase(it);
        } else {
            it++;
        }
    }
}

void CubeCore::CubeUnitRollBackL0EntryStat(const shared_ptr<CubeUop>& uop)
{
    if (uop->l0abRdCycle < GetSim()->getCycles()) {
        L0A[0][uop->l0ABufferIndex].dependenceCount--;
        L0B[0][uop->l0BBufferIndex].dependenceCount--;
    }
    BufferC[0][uop->BuffeCrIndex].FreeAndReset();
}

void CubeCore::FilterFlushedLoadRes(uint64_t bid, uint32_t stid)
{
    for (size_t i = 0; i < usedLoadIdPool[0].size(); i++) {
        if (usedLoadIdPool[0][i].second.bid == bid && usedLoadIdPool[0][i].second.stid == stid) {
            residualLoadIdPool[0].emplace_back(usedLoadIdPool[0][i]);
        }
    }
}

void CubeCore::PickTileOp()
{
    if (exeTileOpChain.empty()) {
        bool empty = true;
        for (uint32_t stid = 0; stid < cubeROB.size(); ++stid) {
            if (cubeROB[stid].Size() != 0) {
                empty = false;
            }
        }
        if (!empty) {
            PickExeTileOp();
        }
    }

    if (IsExeTileOpSendAllLd()) {
        PickExeTileOp();
    }
}

void CubeCore::PickExeTileOp()
{
    auto doRemove = [this](std::deque<CubeRobInfo>& readQ) {
        for (auto it = readQ.cbegin(); it != readQ.cend(); it++) {
            if (find_if(exeTileOpChain.begin(), exeTileOpChain.end(), [&it](ChainTileInfo& info) {
                    return (info.stid == it->blockCmd->stid && info.bid == it->blockCmd->bid.val);
                }) == exeTileOpChain.end() && bidCubeSMMap[it->blockCmd->stid][it->blockCmd->bid.val].renamed &&
                bidCubeSMMap[it->blockCmd->stid][it->blockCmd->bid.val].op != TileOp::ACCCVT) {
                exeTileOpChain.emplace_back(it->blockCmd->bid.val, it->blockCmd->stid);
                break;
            }
        }
    };
    for (uint32_t stid = 0; stid < extantAccChainId.size(); ++stid) {
        for (size_t i = 0; i < extantAccChainId[stid].size(); i++) {
            uint64_t chainId = extantAccChainId[stid][i];
            auto readQ = chainROB[stid][chainId].GetRawReadData();
            doRemove(readQ);
        }
    }
}

bool CubeCore::IsExeTileOpSendAllLd()
{
    bool res = true;
    for (size_t i = 0; i < exeTileOpChain.size(); i++) {
        uint32_t stid = exeTileOpChain[i].stid;
        uint32_t bid = exeTileOpChain[i].bid;
        res = res && TileLdUopBuffer[stid][bid].Empty();
    }
    return res;
}
} // namespace JCore
