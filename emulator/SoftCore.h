#pragma once

#ifndef SOFT_CORE_
#define SOFT_CORE_

#include <cstdint>
#include <string>

#include "Memory.h"
#include "ISA.h"
#include "calculate/MInstCalculator.h"
#include "ArchStatus.h"
#include "MInstFunc.h"
#include "softfloat-types.h"
#include "../third_party/json.hpp"
#include "../configs/softcore_config.h"
#include "../isa/calculate/CubeCalculate.h"

namespace JCore {

struct EmulatorCommandLineArgs {
    std::string filename;
    std::string logname;
    int traceMode = 0;
    uint64_t blockNum = static_cast<uint64_t>(-1);
    uint64_t stopPC = static_cast<uint64_t>(-1);
    uint64_t stopCycles = static_cast<uint64_t>(-1);
    uint64_t debugLogStartPC = static_cast<uint64_t>(-1);
    bool initStackWithEnv = false;
    std::vector<std::string> cfgs;
};

struct TcopyShape {
    size_t validRow = 0;
    size_t validCol = 0;
    size_t totalRow = 0;
    size_t totalCol = 0;
    size_t tileRegDstNum = 0;
    size_t tileRegSrcNum = 0;
    size_t tileTotalSize = 0;
    uint64_t memBaseAddr = 0;
    uint64_t stride = 0;
    DataType dataType;
    LayOut tileConvertType = LayOut::NORM;
    TcopyShape(uint64_t m, uint64_t n)
        :validRow(m), validCol(n) {}
};

struct CubeMatShape {
    uint64_t m = 0;
    uint64_t n = 0;
    uint64_t k = 0;
    uint64_t externM = 0;
    uint64_t externN = 0;
    uint64_t externK = 0;
    /* MAMULB/MAMULBAC/MAMULB.ACC only support 32bit. if 16, need to convert 32bit; if 64bit, illegal */
    DataType srcLType = DataType::INVALID;
    DataType srcRType = DataType::INVALID;
    DataType dstType = DataType::INVALID; // 默认FP32
    CubeMatShape(uint64_t m, uint64_t n, uint64_t k, DataType srcTypeL, DataType srcTypeR, DataType type)
        : m(m), n(n), k(k), srcLType(srcTypeL), srcRType(srcTypeR), dstType(type) {}

    void InitShape(uint64_t shapeM, uint64_t shapeN, uint64_t shapeK, DataType type)
    {
        m = shapeM;
        n = shapeN;
        k = shapeK;
        dstType = type;
    }

    void InitExtern(uint64_t m, uint64_t n, uint64_t k)
    {
        this->externM = m;
        this->externN = n;
        this->externK = k;
    }
};

class SoftCore {
public:
    bool                verbose = false;
    bool                gfrunning = false;
    bool                recordMInst = false;

    /* CKPT file */
    bool ckpt_file = false;

    // component
    EmulatorCommandLineArgs inputArgs;
    std::shared_ptr<ConfigInput> cfgs;
    SoftCoreConfig      config;
    SoftMemory          *memory = nullptr;

    // Execute status
    bool                coreSimEnd = false;

    struct ThreadStatus {
        /* \brief Current block PC */
        uint64_t            pc = -1;
        uint64_t            nextPc = -1;
        uint64_t            blockId = 0;
        uint64_t            minstId = 0;
        bool                simEnd = false;
        BlockFuncPtr        currentBlock = nullptr;
        ArchStatus          archStatus;
    };

    std::vector<ThreadStatus> threadStatus;

    float_status         status = {
        float_round_nearest_even, 0, floatx80_precision_x, false, false, false, false, false, false, false
    };

    std::vector<std::vector<MInst>> instLogs;

    SoftCore() = default;
    ~SoftCore();

    void Init();
    void Step();
    void EmulatorBlock(uint64_t threadID);
    void ReportStat();

    void ResetPC(uint64_t newPc, uint64_t threadID);
    uint64_t GetPC(uint64_t threadID);

    /* Arch Status Interface */
    void SetGPR(int idx, uint64_t value, uint64_t threadID);
    uint64_t GetGPR(int idx, uint64_t threadID);
    void SetSystemReg(uint64_t id, uint64_t value, uint64_t threadID);
    uint64_t GetSystemReg(uint64_t id, uint64_t threadID);
    FRMMode GetFRM(uint64_t threadID); // CSTATE
    void SetBPC(uint64_t newBPC, uint64_t threadID);
    uint64_t GetBPC(uint64_t threadID);

    uint64_t GetTotalBlockNum();

    void EnableTrace();

    /* check the result of checkpoint */
    void CheckCkptRlt(const char* filename);
    /* Fetch binary from memory and build MInst */
    MInstFuncPtr FetchDecodeInst(uint64_t fetchPc, uint64_t threadID);
    void InstConstraintCheck(MInstFuncPtr &inst, BlockFuncPtr &currentBlock);
    /* Execute MInst: 1.Prepare src operands data; 2.calculate; 3.write dst operands data to arch status */
    void ExecuteMinst(MInstFuncPtr inst, BlockFuncPtr &currentBlock);
    void ExecuteSTDMinst(MInstFuncPtr inst, BlockFuncPtr &currentBlock);
    void ExecuteSIMTMinst(MInstFuncPtr inst, BlockFuncPtr &currentBlock);
    void StatSTDMInst(MInstFuncPtr inst, BlockFuncPtr &currentBlock, bool firstInst);
    void StatSIMTMInst(MInstFuncPtr inst, BlockFuncPtr &currentBlock, uint64_t laneID);
    void PreProcessBlock(BlockFuncPtr &block);
    void PostProcessBlock(BlockFuncPtr &currentBlock);
    void CommitCurrentBlock(BlockFuncPtr &currentBlock);
    uint64_t UpdateNextPC(MInstFuncPtr inst, BlockFuncPtr &currentBlock);
    bool IsBlockEnd(BlockFuncPtr &currentBlock);
    bool IsSimEnd(MInstFuncPtr inst);

    /* Detail execute functions */
    void GetSrcData(MInstFuncPtr inst, BlockFuncPtr &currentBlock, uint64_t lane = 0);
    void Calculate(MInstFuncPtr inst, BlockFuncPtr &currentBlock);
    void SetDstData(MInstFuncPtr inst, BlockFuncPtr &currentBlock, uint64_t lane = 0);
    void ExecuteTemplate(BlockFuncPtr &currentBlock);
    /* MCOPY\MSET\FENTRY\FEXIT\FRET.RA\FRET.STK generate code template. */
    void GenCodeTemplate(BlockFuncPtr block);
    /* Execute parallel tile operations */
    void ExecutePTO(BlockFuncPtr block);
    void ExecuteTMA(BlockFuncPtr block);
    void ExecuteCUBE(BlockFuncPtr block);
    void ExecuteTEPL(BlockFuncPtr block);

    void GetSrcOperandData(MInstFuncPtr inst, BlockFuncPtr &currentBlock, uint64_t lane, OperandPtr src);
    void SetDstOperandData(MInstFuncPtr inst, BlockFuncPtr &currentBlock, uint64_t lane, OperandPtr dst);

    void AccumulateBlockInfo(MInstFuncPtr inst, BlockFuncPtr &currentBlock);
    void PrepareSIMT(BlockFuncPtr &currentBlock);
    void InitSIMTRegInfo(BlockFuncPtr &currentBlock);
    void ProcessSIMTEnd(MInstFuncPtr inst, BlockFuncPtr &currentBlock);
    void HandleNextBSTART(MInstFuncPtr inst, BlockFuncPtr block);
    void AaccelssMemoryLoad(MInstFuncPtr inst);
    void AaccelssMemoryStore(MInstFuncPtr inst);
    void AtomicAaccelssMemory(MInstFuncPtr inst);
    void CacheMaintainAaccelssMemory(MInstFuncPtr inst) const;

    // return a matrix shape of [m, n] in row major
    std::vector<uint64_t> LoadFromTileRegister(DataType dataType, size_t m, size_t n, uint64_t tileBaseAddr,
                                               FractalType type, uint64_t threadId);
    std::vector<uint64_t> LoadFromAcc(DataType dataType, size_t m, size_t n, uint64_t tileBaseAddr, uint64_t thread);
    std::vector<uint64_t> LoadFromTileRegisterRD(DataType dataType, size_t m, size_t n, uint64_t tileBaseAddr,
                                                 uint64_t thread);
    std::vector<uint64_t> LoadFromTileRegisterCD(DataType dataType, size_t m, size_t n, uint64_t tileBaseAddr,
                                                 uint64_t thread);
    std::vector<uint64_t> LoadFromTileRegisterNn(DataType dataType, std::pair<size_t, size_t> entireMatrix,
                                                 std::pair<size_t, size_t> blockMatrix, uint64_t tileBaseAddr,
                                                 uint64_t thread);
    std::vector<uint64_t> LoadFromTileRegisterNz(DataType dataType, std::pair<size_t, size_t> entireMatrix,
                                                 std::pair<size_t, size_t> blockMatrix, uint64_t tileBaseAddr,
                                                 uint64_t thread);
    std::vector<uint64_t> LoadFromTileRegisterZn(DataType dataType, std::pair<size_t, size_t> entireMatrix,
                                                 std::pair<size_t, size_t> blockMatrix, uint64_t tileBaseAddr,
                                                 uint64_t thread);
    std::vector<uint64_t> LoadFromTileRegisterZz(DataType dataType, std::pair<size_t, size_t> entireMatrix,
                                                 std::pair<size_t, size_t> blockMatrix, uint64_t tileBaseAddr,
                                                 uint64_t thread);
    void DataFormatCvt(std::vector<uint64_t>& dst, JCore::DataType srcType, JCore::DataType dstType, uint64_t thread);
    void ScaleAcc(std::vector<uint64_t>& dst, JCore::DataType srcType, bool needScale, uint64_t scaleVal);
    void WriteToTileRegister(const std::vector<uint64_t> &matrixData, std::pair<size_t, size_t> matrixShape,
                             DataType dataType, uint64_t tileBaseAddr, FractalType storeType, uint64_t thread);
    void WriteToAcc(const std::vector<uint64_t>& matrixData, CubeMatShape shape, uint64_t thread);
    void AccToTileRegister(const std::vector<uint64_t> &matrixData, std::pair<size_t, size_t> matrixShape,
                           DataType dataType, uint64_t tileBaseAddr, LayOut tileConvertType, bool canon,
                           uint64_t thread);

    void RowMax(std::vector<uint64_t>& dst, uint64_t rowMaxBase,
                JCore::DataType dType, std::pair<size_t, size_t> matShape, uint64_t thread);
    void MatrixMul(std::vector<uint64_t>& dst, const std::vector<uint64_t>& srcL,
                   const std::vector<uint64_t>& srcR, const CubeMatShape& shape);
    void MatrixAdd(std::vector<uint64_t>& dst, const std::vector<uint64_t>& srcL,
                   const std::vector<uint64_t>& srcR, const CubeMatShape& shape);
    void MatrixScale(std::vector<uint64_t>& src, const std::vector<uint64_t>& srcScale,
                      std::pair<size_t, size_t> matrix, size_t elementsPerScale,
                      DataType dataType);
    void MatrixScaleLHiF4(std::vector<uint64_t>& src, std::vector<uint64_t>& srcScale,
                      std::pair<size_t, size_t> matrix) const;
    void MatrixScaleRHiF4(std::vector<uint64_t>& src, std::vector<uint64_t>& srcScale,
                      std::pair<size_t, size_t> matrix) const;

    JCore::DataType ConvertTo4ByteType(JCore::DataType dstType);

    void ExecuteTLOAD(BlockFuncPtr block, TcopyShape& tCopy);
    void ExecuteTSTORE(BlockFuncPtr block, TcopyShape& tCopy);
    void ExecuteTMOV(BlockFuncPtr block, TcopyShape& tCopy);
    void ExecuteMGATHER(BlockFuncPtr block);
    void ExecuteMSCATTER(BlockFuncPtr block);

    void ExecuteTADD(BlockFuncPtr block,
                     std::pair<size_t, size_t> totalMatrix,
                     std::pair<size_t, size_t> validMatrix,
                     DataType dataType);
    void ExecuteTSUB(BlockFuncPtr block,
                     std::pair<size_t, size_t> totalMatrix,
                     std::pair<size_t, size_t> validMatrix,
                     DataType dataType);
    void ExecuteTMUL(BlockFuncPtr block,
                     std::pair<size_t, size_t> totalMatrix,
                     std::pair<size_t, size_t> validMatrix,
                     DataType dataType);
    void ExecuteTMAX(BlockFuncPtr block,
                     std::pair<size_t, size_t> totalMatrix,
                     std::pair<size_t, size_t> validMatrix,
                     DataType dataType);
    void ExecuteTEXP(BlockFuncPtr block,
                     std::pair<size_t, size_t> totalMatrix,
                     std::pair<size_t, size_t> validMatrix,
                     DataType dataType);
    void ExecuteTRECIP(BlockFuncPtr block,
                       std::pair<size_t, size_t> totalMatrix,
                       std::pair<size_t, size_t> validMatrix,
                       DataType dataType);
    void ExecuteTMULS(BlockFuncPtr block,
                      std::pair<size_t, size_t> totalMatrix,
                      std::pair<size_t, size_t> validMatrix,
                      DataType dataType, uint64_t scalar);
    void ExecuteTEXPANDS(BlockFuncPtr block,
                         std::pair<size_t, size_t> totalMatrix,
                         std::pair<size_t, size_t> validMatrix,
                         DataType dataType, uint64_t scalar);
    void ExecuteTROWSUM(BlockFuncPtr block,
                        std::pair<size_t, size_t> validMatrix,
                        DataType dataType, size_t totalRow);
    void ExecuteTROWMAX(BlockFuncPtr block,
                        std::pair<size_t, size_t> validMatrix,
                        DataType dataType, size_t totalRow);
    void ExecuteTROWEXPAND(BlockFuncPtr block,
                           std::pair<size_t, size_t> totalMatrix,
                           std::pair<size_t, size_t> validMatrix,
                           DataType dataType);
    void ExecuteTCOLSUM(BlockFuncPtr block,
                        std::pair<size_t, size_t> validMatrix,
                        DataType dataType, size_t totalCol);
    void ExecuteTCOLMAX(BlockFuncPtr block,
                        std::pair<size_t, size_t> validMatrix,
                        DataType dataType, size_t totalCol);
    void ExecuteTCOLEXPANDSUB(BlockFuncPtr block,
                              std::pair<size_t, size_t> totalMatrix,
                              std::pair<size_t, size_t> validMatrix,
                              DataType dataType);
    void ExecuteTCOLEXPANDMUL(BlockFuncPtr block,
                              std::pair<size_t, size_t> totalMatrix,
                              std::pair<size_t, size_t> validMatrix,
                              DataType dataType);
    void ExecuteTCVT(BlockFuncPtr block,
                     std::pair<size_t, size_t> totalMatrix,
                     std::pair<size_t, size_t> validMatrix,
                     DataType dataType,
                     DataType dstType);

    MInstFuncPtr InitCTInst(BlockFuncPtr block, Opcode op, EncodeLen len = EncodeLen::ENL_W);
    void ExecuteCTInst(MInstFuncPtr inst, BlockFuncPtr &block);
    void GenCodeMCOPY(BlockFuncPtr block);
    void GenCodeMCOPYStage1(BlockFuncPtr block);
    void GenCodeMCOPYStage2(BlockFuncPtr block);
    void GenCodeMCOPYStage3(BlockFuncPtr block);
    void GenCodeMSET(BlockFuncPtr block);
    void GenCodeMSETZero(BlockFuncPtr block);
    void GenCodeMSETNoneZero(BlockFuncPtr block);
    void GenCodeFENTRY(BlockFuncPtr block);
    void GenCodeFEXIT(BlockFuncPtr block);
    void GenCodeFRETRA(BlockFuncPtr block);
    void GenCodeFRETSTK(BlockFuncPtr block);
    bool ExecuteSysCall(BlockFuncPtr &currentBlock);
};

struct adderss_region {
    uint64_t start_addr;
    uint64_t end_addr;
};
}
#endif