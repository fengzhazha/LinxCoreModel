#ifndef  BLOCKISA_MODEL_CORE_H
#define  BLOCKISA_MODEL_CORE_H
#pragma once

#include <chrono>
#include <map>
#include <vector>

#include "../configs/core_config.h"
#include "bctrl/BCtrl.h"
#include "bctrl/bfu/bfu.h"
#include "core/Bus.h"
#include "core/FlushControl.h"
#include "core/interface.h"
#include "core/Packet.h"
#include "core/SRename.h"
#include "cube/CubeCore.h"
#include "iex/ctrl_stack.h"
#include "iex/iex.h"
#include "interface/InterfaceCommon.h"
#include "l2/L2Cache.h"
#include "lsu/lsu.h"
#include "ModelSpec.h"
#include "pe/PEBase.h"
#include "plat/DebugLog.h"
#include "tmu/ring/CrossStation.h"
#include "tmu/ring/Ring.h"
#include "tmu/TileRegister.h"
#include "tmu/TileUtils.h"
#include "vectorcore/VectorCore.h"
#include "mtccore/memcore/MemoryCore.h"
#include "vectorcore/vpe/vrf/VRFRename.h"
#include "vectorcore/vpe/LGPR/LGPRRF.h"
#include "bridgetable/TileBridge.h"
#include "tmu/xbar/Island.h"
#include "vectorcore/VectorTop.h"
#include "DataStruct/MachineId.h"
#include "soc/RandomModelSoC.h"
#include "ModelCommon/bus/VectorBridgeBus.h"
#include "veclite/VectorLiteTop.h"

#if defined GENERIC_SOC || defined GENERIC_SOC_NEW
    #include "generic_soc/soc_wrapper.h"
#else
    #include "soc/soc_wrapper.h"
#endif

namespace JCore {

class SimSys;
class FlushControl;
class LoadStoreUnit;
class MtcLoadStoreUnit;
class FakeLsu;
class L2Cache;

class InstTracer;
using PInstTracer = std::shared_ptr<InstTracer>;

#define MAX_VECTOR_BRIDGE_SIZE (8)

class ConfigTable {
public:
    std::string configName;

    // 手动添加配置项
    void addConfig(const std::string& name, const std::string& value)
    {
        configPairs.push_back(std::make_pair(name, value));
    }

    void setConfig(const std::string& name, const std::string& value)
    {
        auto extractValue = [this](const std::string& input) -> std::string {
            size_t equalPos = input.find('=');
            if (equalPos != std::string::npos) {
                std::string value = input.substr(equalPos + 1);
                auto trimStr = [](std::string& str) {
                    str.erase(0, str.find_first_not_of(" \t\r\n"));
                    str.erase(str.find_last_not_of(" \t\r\n") + 1);
                };
                trimStr(value);
                return value;
            }
            return input;
        };

        for (auto& pair : configPairs) {
            if (pair.first == name) {
                pair.second = extractValue(value);
                return;
            }
        }
        addConfig(name, extractValue(value));
    }

    std::string getConfig(const std::string& name, const std::string& defaultValue = "") const
    {
        for (const auto& pair : configPairs) {
            if (pair.first == name) {
                return pair.second;
            }
        }
        return defaultValue;
    }

    bool removeConfig(const std::string& name)
    {
        auto it = std::remove_if(configPairs.begin(), configPairs.end(),
            [&name](const std::pair<std::string, std::string>& pair) {
                return pair.first == name;
            });
        if (it != configPairs.end()) {
            configPairs.erase(it, configPairs.end());
            return true;
        }
        return false;
    }

    void clearConfigs()
    {
        configPairs.clear();
    }

    size_t getConfigCount() const
    {
        return configPairs.size();
    }

    void printTable() const
    {
        if (configPairs.empty()) {
            std::cout << "No configuration items found." << std::endl;
            return;
        }

        size_t maxKeyLength = 15; // 最小宽度
        size_t maxValueLength = 10; // 最小宽度

        for (const auto& pair : configPairs) {
            if (pair.first.length() > maxKeyLength) {
                maxKeyLength = pair.first.length();
            }
            if (pair.second.length() > maxValueLength) {
                maxValueLength = pair.second.length();
            }
        }

        maxKeyLength += 2;
        maxValueLength += 2;

        printHorizontalLine(maxKeyLength, maxValueLength);
        std::cout << "| " << std::left << std::setw(maxKeyLength) << configName
                  << " | " << std::setw(maxValueLength) << "Value" << " |" << std::endl;
        printHorizontalLine(maxKeyLength, maxValueLength);

        for (const auto& pair : configPairs) {
            std::cout << "| " << std::left << std::setw(maxKeyLength) << pair.first
                      << " | " << std::setw(maxValueLength) << pair.second << " |" << std::endl;
        }

        printHorizontalLine(maxKeyLength, maxValueLength);
    }

private:
    std::vector<std::pair<std::string, std::string>> configPairs;

    static void trim(std::string& str)
    {
        str.erase(0, str.find_first_not_of(" \t"));
        str.erase(str.find_last_not_of(" \t") + 1);
    }

    void printHorizontalLine(size_t keyWidth, size_t valueWidth) const
    {
        std::cout << "+" << std::string(keyWidth + 2, '-')
                  << "+" << std::string(valueWidth + 2, '-')
                  << "+" << std::endl;
    }
};
#define OFFSET_COUNT        2
enum OFFSET_TYPE {
    BTEXT_START_OFFSET,
    BNEXT_OFFSET
};

// Score Board类：用于维护每个PE的占用状态
class TileBankScoreBoard {
public:
    TileBankScoreBoard();
    // 设置占用N个周期
    void SetOccupancy(MachineType pe, int cycles);
    // 每个周期更新，返回是否仍被占用
    bool Update();
    // 检查是否被占用
    bool IsOccupied() const;
    // 获取当前占用的PE
    MachineType GetOccupiedPE() const;
    // 获取剩余周期数
    int GetRemainingCycles() const;
    // 重置score board
    void Reset();
    
private:
    MachineType occupiedPE_;
    int remainingCycles_;
    int currentCycle_;
};

class Core : public SimObj {
private:
    /* \brief Direct return aaccelsses to corresponding module */
    void                            route();
    void                            watch_dog(void);
    TileBankScoreBoard readScoreBoard_;   // 读口score board
    TileBankScoreBoard writeScoreBoard_;  // 写口score board
    MachineType readNextPriority_;   // 读口下一个优先级
    MachineType writeNextPriority_;  // 写口下一个优先级

public:
    virtual ~Core();
    SimSys                          *sim;
    std::shared_ptr<IEX>            iex[IEX_NUM];
    CoreInterface                   coreInterface;
    InterfaceCommon                 *intf; // Uniform interface
    std::shared_ptr<FlushControl>   flushUnit;
    std::shared_ptr<BCtrlUnit>      bctrl;
    std::vector<std::shared_ptr<PEBase>> peArray;
    std::shared_ptr<VRFRename>      vrfRename;
    std::shared_ptr<LGPRRF>         lgprRF;
    std::shared_ptr<StackRenameUnit> sRenameUnit;
    std::vector<std::shared_ptr<LoadStoreUnit>> memIntf;
    std::vector<std::shared_ptr<MtcLoadStoreUnit>> MtcmemIntf;
    std::shared_ptr<TileRegister>   tileReg;
    TileUtils                       tileUtils;
    std::vector<std::shared_ptr<CrossStation>> stations;
    std::map<int, std::string>      nodeToPEMap;
    std::shared_ptr<Ring>           ccRing;
    std::shared_ptr<Ring>           cwRing;
    std::shared_ptr<Island>         island;
    std::shared_ptr<TraceLog::TraceLogger> traceLogger;
    CtrlStack                       ctrl_stack;
    std::map<SystemReg, uint64_t>   sysregFile;
    /* \brief Offset GPR */
    bool                            offsetGprVld = false;
    uint64_t                        offsetGpr[OFFSET_COUNT] = {0};

    CoreConfig                      configs;

    ConfigTable                     bccPrintTable;
    ConfigTable                     vecPrintTable;
    ConfigTable                     cubePrintTable;
    ConfigTable                     tmuPrintTable;
    ConfigTable                     mtcPrintTable;
    ConfigTable                     tmaPrintTable;

    /* \brief Pointer to System-on-Chip */
    #if defined GENERIC_SOC || defined GENERIC_SOC_NEW
        std::shared_ptr<SocGeneric>   soc;
    #else
        std::shared_ptr<SOC>        soc;
    #endif
    RandomModelSoC                 socRandomModel;
    /* \brief Global queues */
    SimQueue<GFUMemReq>             bFetchReqQ;
    SimQueue<GFUMemReq>             iFetchReqQ;
    std::vector<SimQueue<GFUMemReq>>memReqQ;
    std::vector<DelayQueue<GFUMemReq>> mem_delay;

    SimQueue<VectorBridgeReq> vecBridgeReqQ;
    SimQueue<VectorBridgeRsp> vecBridgeRspQ;

    SimQueue<GFUMemReq>             bFetchRetQ;
    SimQueue<GFUMemReq>             iFetchRetQ;
    std::vector<SimQueue<GFUMemReq>>memRetQ;

    std::vector<SimQueue<PtrPacket>>             inst_l2_q;
    std::vector<SimQueue<PtrPacket>>             l2_inst_q;
    std::vector<std::vector<SimQueue<PtrPacket>>>l2_ifu_array;
    std::vector<SimQueue<PtrPacket>>             l2_bfu_q;
    std::vector<SimQueue<PtrPacket>>             hpref_l2_q;
    std::vector<SimQueue<PtrPacket>>             snp_l2_q;

    /* delayed flush request */
    NS_CORE::PtrMachineInst                nukeMHdr;

    // record minsts that are not recorded in statistics
    unordered_map<Opcode, uint64_t> otherInstMap;
    std::shared_ptr<DebugLog>       debug_log;
    PInstTracer                     pTracer { nullptr };

    uint64_t                        warmupBlockNum;
    uint64_t                        warmupMinstNum;
    bool                            warmupFinished;
    std::chrono::system_clock::time_point timePoint;
    std::vector<std::shared_ptr<MemoryCore>> mtcCores;
    std::vector<std::shared_ptr<CubeCore>> cubeCores;
    std::shared_ptr<VectorTop>      vectorTop;
    std::shared_ptr<VectorLiteTop>  vecliteTop;
    std::shared_ptr<L2Cache>        m_l2Cache = NULL;
    size_t                          globalSeqToSwimPtr = 0;
    uint64_t                        cubeBusyCycle = 0;
    uint64_t                        vecBusyCycle = 0;
    uint64_t                        mtcBusyCycle = 0;
    uint64_t                        tmaBusyCycle = 0;
    uint64_t                        tloadBusyCycle = 0;
    uint64_t                        writeBlockedByRingCycle = 0;
    uint64_t                        tstoreBusyCycle = 0;
    uint64_t                        tmovBusyCycle = 0;
    uint64_t                        mgatherBusyCycle = 0;
    uint64_t                        mscatterBusyCycle = 0;
    uint64_t                        vgatherBusyCycle = 0;
    uint64_t                        vscatterBusyCycle = 0;
    uint64_t                        tileopTotalCycle = 0;
    uint64_t                        tileIdleCycle = 0;
    uint64_t                        vectorCubeConcurrentCycle = 0;
    uint64_t                        vectorCubeActiveCycle = 0;
    uint64_t                        cubeTMAConcurrentCycle = 0;
    uint64_t                        cubeTMAActiveCycle = 0;
    uint64_t                        cubeReadRFReqCnt = 0;
    uint64_t                        cubeReadRFReqArbFailCnt = 0;
    uint64_t                        cubeReadRFReqArbFailByPriCnt = 0;
    uint64_t                        cubeRdBlockByTmaCnt = 0;
    uint64_t                        cubeRdBlockByVecCnt = 0;
    uint64_t                        tmaReadRFReqCnt = 0;
    uint64_t                        tmaReadRFReqArbFailCnt = 0;
    uint64_t                        tmaReadRFReqArbFailByPriCnt = 0;
    uint64_t                        tmaRdBlockByCubeCnt = 0;
    uint64_t                        tmaRdBlockByVecCnt = 0;
    uint64_t                        vectorReadRFReqCnt = 0;
    uint64_t                        vectorReadRFReqArbFailCnt = 0;
    uint64_t                        vectorReadRFReqArbFailByPriCnt = 0;
    uint64_t                        vecRdBlockByTmaCnt = 0;
    uint64_t                        vecRdBlockByCubeCnt = 0;

    uint64_t                        cubeRealReadRFReqCnt = 0;
    uint64_t                        cubeRealWriteRFReqCnt = 0;
    uint64_t                        tmaRealReadRFReqCnt = 0;
    uint64_t                        tmaRealWriteRFReqCnt = 0;
    uint64_t                        vectorRealReadRFReqCnt = 0;
    uint64_t                        vectorRealWriteRFReqCnt = 0;

    uint64_t                        cubeWriteRFReqCnt = 0;
    uint64_t                        cubeWriteRFReqArbFailCnt = 0;
    uint64_t                        cubeWriteRFReqArbFailByPriorityCnt = 0;
    uint64_t                        cubeWrBlockByTmaCnt = 0;
    uint64_t                        cubeWrBlockByVecCnt = 0;
    uint64_t                        tmaWriteRFReqCnt = 0;
    uint64_t                        tmaWriteRFReqArbFailCnt = 0;
    uint64_t                        tmaWriteRFReqArbFailByPriorityCnt = 0;
    uint64_t                        tmaWrBlockByCubeCnt = 0;
    uint64_t                        tmaWrBlockByVecCnt = 0;
    uint64_t                        vectorWriteRFReqCnt = 0;
    uint64_t                        vectorWriteRFReqArbFailCnt = 0;
    uint64_t                        vectorWriteRFReqArbFailByPriorityCnt = 0;
    uint64_t                        vecWrBlockByTmaCnt = 0;
    uint64_t                        vecWrBlockByCubeCnt = 0;

    std::vector<std::shared_ptr<GFSIM::TileBridge::TileBridge>> tileBridges;
    
    CoreConfig                      GetConfig() const {return configs;}
    void                            buildCore(SimSys *sim);
    void                            buildInterface();
    void                            BuildFlushUnit();
    void                            BuildBCC();
    void                            BuildStackRename();
    uint32_t                        GetPECount();
    uint32_t                        GetRelativePEID(uint32_t peID, ExecEngineTyp peType);
    uint32_t                        GetMtcPeID();
    uint32_t                        GetVecPENum() const;
    uint32_t                        GetMtcPEIndex() const;
    void                            BuildSTDPE();
    void                            SetPEAttr();
    void                            BuildIEX();
    void                            ConnectIEXIntf();

    void                            BuildCubeCore();
    void                            BuildVecCore();
    void                            BuildVeclite();
    void                            BuildMTCCore();
    void                            BuildSIMTIEX();
    void                            ConnectSIMTIEXIntf();
    void                            BuildMEMIEX();
    void                            ConnectMEMIEXIntf();
    void                            BuildTileReg();
    void                            BuildLSU();
    void                            BuildScalarLSU();
    void                            BuildVectorLSU();
    void                            BuildMTCLSU();
    void                            BuildSOC();
    uint32_t                        GetMLSUScalpersize();
    void                            ConnectBFUTOL2();
    void                            InitRingAndXbar();
    void                            BuildQueues();
    void                            ConnectFlushUnit();
    void                            BuildTileBridge();
    void                            PrintCoreStatus(uint32_t stid); // For deadlock print core status

    void                            InitSwimLogger();

    void                            setGPR(uint32_t atag, uint64_t data, uint32_t stid);
    uint64_t                        getGPR(uint32_t atag, uint32_t stid);
    void                            setSysreg(SystemReg sysID,uint64_t data);
    uint64_t                        getSysreg(SystemReg sysID);
    void                            setOffGPR(uint32_t offsetType, uint64_t data);
    uint64_t                        getOffGPR(uint32_t offsetType);
    void                            enableOffGPR();
    void                            disableOffGPR();
    bool                            getSwitchOffGPR();


    void                            Work();
    void                            Xfer();
    SimSys                          *GetSim() {return sim;}
    void                            Reset();
    void                            setBPC(uint64_t bpc, uint32_t stid);
    uint32_t                        getPECount();
    bool                            IsScalarPe(uint32_t peId);
    bool                            IsScalarPe(MachineType machineType) const;
    bool                            IsVecPe(uint32_t peId);
    bool                            IsVecPe(MachineType machineType) const;
    bool                            IsMtcPe(uint32_t peId);
    bool                            IsMtcIex(MachineType type) const;

    void                            PrintConfigs();
    void                            SetBccConfigs(const std::string& name, const std::string& value);
    void                            SetConfigs(const std::string& name, const std::string& value);
    void                            SetTmuConfigs(const std::string& name, const std::string& value);
    void                            SetCubeConfigs(const std::string& name, const std::string& value);
    void                            SetMtcConfigs(const std::string& name, const std::string& value);
    void                            SetTmaConfigs(const std::string& name, const std::string& value);

    void                            ReportBCCTileRename();
    void                            ReportBCC();
    void                            ReportCUBE();
    void                            ReportVectorCore();
    void                            ReportMTCCore();
    void                            ReportTileReg();
    void                            ReportTileLoadLatDis();
    void                            ReportStat() override;
    void                            ReportKeyStat();
    void                            ReportBCCKeyStat();
    void                            ReportVectorKeyStat();
    void                            ReportTMAKeyStat();
    void                            CountStats();
    bool                            IsVectorIex(MachineType type) const;
    bool                            IsVectorIex(ExecEngineTyp type) const;
    void                            ReportTopdown();

    void                            setWarmup(uint64_t wB, uint32_t wI);
    void                            resetStats();
    void                            OnWarmupFinish();

    /* \brief handle system requests from iex pipe */
    void                            handleSysReq();

    void                            illegalConfCheck();

    /* brief Communication bus connecection */
    void                            ConnectTileBus();
    void                            SetTRDirectAaccelssLatency();
    template <class T>
    void                            SetSimArrayLatency(vector<SimQueue<T>*> const& simArray,
                                                       const uint32_t latency) const;
    void                            ConnetIexBus();
    void                            ConnectBIQBus();
    void                            XCoreSimQXfer();
    Core                            *GetThis() { return this; }
    void                            InitFakeLsu();
    void                            InitL2Cache();
    void                            InitSoc();
    void                            BuildRenameUnit();

    IDBus                           GetScalarOldestInfo(ROBID &oldestId);
    IDBus                           GetSimtOldestInfo();
    uint32_t                        GetScalarPEDecWidth() const;
    uint32_t                        GetVecPEDecWidth() const;

    bool                            IsFullBP();
    bool                            IsPerfectBP();

    // 仲裁：实现读写口的RR仲裁
    // 查询读口是否可用（指定PE是否可以申请）
    bool CanReadTileReg(MachineType pe);
    bool CanWriteTileReg(MachineType pe);
    void AcquireReadTileReg(MachineType pe, int latency);
    void AcquireWriteTileReg(MachineType pe, int latency);
    bool IsReadOccupied() const;
    bool IsWriteOccupied() const;
    MachineType GetReadPriority() const;
    MachineType GetWritePriority() const;
    MachineType GetReadOccupiedPE() const;
    MachineType GetWriteOccupiedPE() const;
    void ResetTileRegArb();
    // RR优先级更新规则：CUBE -> VECLITE -> TMA
    void UpdateNextPriority(MachineType& currentPriority) const;
};

} // namespace JCore
#endif
