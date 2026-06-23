#pragma once

#include <string>

#include "DataStruct/MachineId.h"

namespace JCore {

enum class Unit {
    UNKNOW_CORE,
    BCC,
    BCtrlUnit,
    BFU,
    BUS,
    CUBE,
    DFX,
    FCVT,
    FDIV,
    FLUSH_UNIT,
    FMLA,
    GM,
    IALU,
    IMAC,
    L1D,
    L2,
    LSU,
    MIEX,
    MLSU,
    MTC,
    PERMUTE,
    SCALPER,
    SOC,
    SPE,
    STASH,
    TAU,
    TMA,
    TMU,
    TR,
    UNDEF,
    UNIT_NUM,
    VECTOR,
    VECLITE,
    VRF_REN,

    AaccelssSoftMemory,
    AddSwimLane,
    AllocACCDep,
    AllocWayId,
    BIQFull,
    BlockStashCompelte,
    Build,
    BuildInterface,
    BuildScalarPE,
    CheckBIQStall,
    CheckBROBStall,
    CheckBlockCmdStall,
    CheckFlushStall,
    CheckLoadCltStall,
    CheckLoadQEmpty,
    CheckLoadStall,
    CheckMemoryReq,
    CheckRename,
    CheckSTDPEStall,
    CheckSimStall,
    CheckStall,
    CheckStoreStall,
    CheckTileRegStall,
    CountAddr,
    DispatchBlockCmd,
    DispatchToPE,
    DumpWayFreeLists,
    FlagDoubleOutput,
    Flush,
    FreeWayId,
    FreeWayWithTag,
    GatherStats,
    GenFEntryTemplate,
    GenFlushRequest,
    GetAvailWayNum,
    GetCommitID,
    GetCoreNum,
    GetIEXQueue,
    GetMinHops,
    GetScalpersize,
    GetSim,
    GetStashDst,
    GetTileRegInfo,
    GetVerbose,
    IncLastAllocPtr,
    InsertBlockCmd,
    L1Allow,
    L2Allow,
    Lookup,
    LookupAndGenReq,
    MCallTileRelese,
    OperandComplete,
    PickTStore,
    PickThread,
    PrevTileRelease,
    RecordBccEfficient,
    RecvStashReq,
    RecvStashResp,
    Reduce,
    ReleaseEntry,
    ReleaseMCallStatus,
    RenameSingleTileDst,
    Report,
    ReportBlockRetired,
    ReportLocalRelease,
    ReportStat,
    Reset,
    ResetStats,
    RetireBlock,
    RetireMCallGroup,
    RetireTileTag,
    RollBackLCubeCredit,
    RslvDirectly,
    RunFetchStage5,
    SMTTerminate,
    SelectMinPEIDToQueue,
    SendL1,
    SendSimL1,
    SendStashReq,
    SetConsumerInfo,
    SetFlush,
    SetMCallStatus,
    SetTileSrcRslv,
    StackRenameEnable,
    StashRetireFree,
    StatsMemBound,
    TrackFreeSize,
    TryMCallAlloc,
    WakeupBIssueQEntry,
    WakeupBlockCmd,
    WakeupMoveDependency,
    WakeupTile,
    WindowSlides,
    Work,
    Xfer,
    aaccelssSoftMemory,
    allocSptag,
    buildInterface,
    checkBlockRecorded,
    checkCommit,
    checkConfTable,
    checkDeadLock,
    checkLoadStall,
    checkSimStall,
    checkSmapVld,
    checkSpPtagReady,
    checkStall,
    checkStoreStall,
    cmapOffset,
    countAddr,
    decConfTableCount,
    dispatch,
    flush,
    flushByBid,
    flushByRid,
    freeForOldestBlock,
    genExitTemplate,
    genTemplateStack,
    getBlockStatus,
    getCommitID,
    getPreRenamedStatus,
    getRenameStatus,
    getSpPtagLpvInfo,
    getVerbose,
    handleLoadReq,
    insertSatckGetTable,
    insertSatckSetTable,
    insertStq,
    interrupt,
    isStackInst,
    l1Allow,
    l2Allow,
    lookupStackPRFTable,
    mdbCheck,
    popPreRenamedBID,
    releaseSptag,
    renameForReq,
    renameStack,
    renameStackLoad,
    renameStackStore,
    renewLongOffset,
    replay,
    replayByBid,
    replayByRid,
    resetCmap,
    resetStats,
    rptDispatchStats,
    rptOcupiedPtag,
    rptRenamedNum,
    sendL1,
    sendSimL1,
    setBend,
    setBlockHyper,
    setBlockRenameState,
    setBlockRetire,
    setCancel,
    setFlush,
    setHistoryTable,
    setHistoryTableVld,
    setReg,
    setRegFileWrite,
    setRenameStatus,
    setSPStat,
    setSpPtagReady,
    smapOffset,
    stackRetire,
    store,
};

enum class Stage {
    NA,
    NIL,
    BDB,
    BPQ,
    BROB,
    CAQ,
    COMPLETE,
    CT,
    D0,
    D1,
    D2,
    DATA_DONE,
    E0,
    E1,
    E2,
    E3,
    E4,
    E5,
    EX,
    F0,
    F1,
    F2,
    F3,
    F4,
    F5,
    GEN_ALIGN,
    GEN_ENDING,
    GEN_FOLLOW,
    GEN_INS,
    I,
    I1,
    I2,
    IDLE,
    INVALID,
    IS,
    LOAD,
    LW,
    MGB,
    MPB,
    P1,
    PF,
    PIPE_STAGE,
    RB,
    RE,
    RESOLVED,
    RFB,
    RING,
    RN,
    ROB,
    S,
    S1,
    SPB,
    STAGE_E1,
    STAGE_E2,
    STAGE_E3,
    START,
    STASH,
    TAG,
    TILE_REG_STAGE,
    W1,
    W2,
    WAIT_RF,
    WB,
    WCB,
};

inline std::string EnumToString(Unit unit)
{
    switch (unit) {
#define LINX_UNIT_CASE(name) case Unit::name: return #name
        LINX_UNIT_CASE(UNKNOW_CORE);
        LINX_UNIT_CASE(BCC);
        LINX_UNIT_CASE(BFU);
        LINX_UNIT_CASE(CUBE);
        LINX_UNIT_CASE(DFX);
        LINX_UNIT_CASE(GM);
        LINX_UNIT_CASE(L1D);
        LINX_UNIT_CASE(LSU);
        LINX_UNIT_CASE(MIEX);
        LINX_UNIT_CASE(MLSU);
        LINX_UNIT_CASE(MTC);
        LINX_UNIT_CASE(SCALPER);
        LINX_UNIT_CASE(SOC);
        LINX_UNIT_CASE(SPE);
        LINX_UNIT_CASE(STASH);
        LINX_UNIT_CASE(TMA);
        LINX_UNIT_CASE(TMU);
        LINX_UNIT_CASE(VECTOR);
        LINX_UNIT_CASE(VECLITE);
        LINX_UNIT_CASE(VRF_REN);
        default:
            return "UNIT";
#undef LINX_UNIT_CASE
    }
}

inline std::string EnumToString(Stage stage)
{
    switch (stage) {
#define LINX_STAGE_CASE(name) case Stage::name: return #name
        LINX_STAGE_CASE(NA);
        LINX_STAGE_CASE(F0);
        LINX_STAGE_CASE(F1);
        LINX_STAGE_CASE(F2);
        LINX_STAGE_CASE(F3);
        LINX_STAGE_CASE(F4);
        LINX_STAGE_CASE(F5);
        LINX_STAGE_CASE(D0);
        LINX_STAGE_CASE(D1);
        LINX_STAGE_CASE(D2);
        LINX_STAGE_CASE(E1);
        LINX_STAGE_CASE(E2);
        LINX_STAGE_CASE(E3);
        LINX_STAGE_CASE(E4);
        LINX_STAGE_CASE(WB);
        default:
            return "STAGE";
#undef LINX_STAGE_CASE
    }
}

inline Unit MachineTypeToUnit(MachineType type)
{
    switch (type) {
        case MachineType::BCC: return Unit::BCC;
        case MachineType::BFU: return Unit::BFU;
        case MachineType::SPE: return Unit::SPE;
        case MachineType::CUBE: return Unit::CUBE;
        case MachineType::VECTOR: return Unit::VECTOR;
        case MachineType::VECLITE: return Unit::VECLITE;
        case MachineType::MTC: return Unit::MTC;
        case MachineType::TMA: return Unit::TMA;
        case MachineType::TMU: return Unit::TMU;
        case MachineType::LSU: return Unit::LSU;
        case MachineType::MLSU: return Unit::MLSU;
        case MachineType::MIEX: return Unit::MIEX;
        case MachineType::DFX: return Unit::DFX;
        case MachineType::SOC: return Unit::SOC;
        case MachineType::STASH: return Unit::STASH;
        case MachineType::FLUSH_UNIT: return Unit::FLUSH_UNIT;
        case MachineType::SCALPER: return Unit::SCALPER;
        case MachineType::L1D: return Unit::L1D;
        default: return Unit::UNKNOW_CORE;
    }
}

inline std::string EnumToString(MachineType type)
{
    return EnumToString(MachineTypeToUnit(type));
}

inline Unit StrToUnit(const std::string& type)
{
    return MachineTypeToUnit(StrToMachineType(type));
}

} // namespace JCore
