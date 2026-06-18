#pragma once
#include "CommonData.h"
#include "BiMap.h"

namespace JCore {

enum class MachineType {
    BCC = 1,
    BFU,
    SPE,
    VPE,
    MPE,
    CUBE,
    VECTOR,
    VECLITE,
    VECTOR_WL,
    MTC,
    TMA,
    TAU,
    TMU,
    CSU,
    FLUSH_UNIT,
    LSU,
    SIEX,
    VIEX,
    MIEX,
    SIFU,
    VIFU,
    MIFU,
    DFX,
    SOC,
    BUS,
    STASH,
    MLSU,
    SCALPER,
    CCRING,
    CWRING,
    VRF_REN, // VRF Rename
    L1I,
    L1D,
    L2,
    UNKNOW_CORE
};

enum class TileOpType {
    MEM_IN = 0,
    MEM_OUT,
    TILE_MOVE,
    MATMUL,
    VEC_UNARY,
    VEC_BINARY,
    VEC_SCALAR,
    VEC_SORT,
    UNKNOW_TileOpType
};

enum class TileOpAttr {
    ATTR_NORMAL = 0,
    ATTR_COPY_IN_NDNZ,
    ATTR_AXIS,
    UNKNOW_ATTR
};

enum class BIQType {
    VEC_IQ,
    VET_IQ,
    CUBE_IQ,
    BCC_IQ,
    TMA_IQ,
    TAU_IQ,
    MTC_IQ,
    STASH_IQ,
    MCALL_IQ,
    NONE_IQ
};

inline const BiMap<MachineType> &GetMachineTypeDict()
{
    static BiMap<MachineType> dict {
        {
            {MachineType::BCC, "BCC"},
            {MachineType::BFU, "BFU"},
            {MachineType::SPE, "SPE"},
            {MachineType::VPE, "VPE"},
            {MachineType::MPE, "MPE"},
            {MachineType::CUBE, "CUBE"},
            {MachineType::VECTOR, "VECTOR"},
            {MachineType::VECLITE, "VECLITE"},
            {MachineType::VECTOR_WL, "VEC_WL"},
            {MachineType::MTC, "MTC"},
            {MachineType::TMA, "TMA"},
            {MachineType::TAU, "TAU"},
            {MachineType::TMU, "TMU"},
            {MachineType::CSU, "CSU"},
            {MachineType::LSU, "LSU"},
            {MachineType::MLSU, "MLSU"},
            {MachineType::SIEX, "SIEX"},
            {MachineType::VIEX, "VIEX"},
            {MachineType::MIEX, "MIEX"},
            {MachineType::SIFU, "SIFU"},
            {MachineType::VIFU, "VIFU"},
            {MachineType::MIFU, "MIFU"},
            {MachineType::DFX, "DFX"},
            {MachineType::SOC, "SOC"},
            {MachineType::BUS, "BUS"},
            {MachineType::STASH, "STASH"},
            {MachineType::FLUSH_UNIT, "FLUSH"},
            {MachineType::SCALPER, "SCALPER"},
            {MachineType::CCRING, "CCRING"},
            {MachineType::CWRING, "CWRING"},
            {MachineType::VRF_REN, "VRFRename"},
            {MachineType::L1I, "L1I"},
            {MachineType::L1D, "L1D"},
            {MachineType::L2, "L2"},
        }
    };
    return dict;
};

inline std::string MachineName(MachineType type)
{
    return GetMachineTypeDict().Find(type, "UNKNOWN_UNIT");
}

inline MachineType StrToMachineType(const std::string &type)
{
    return GetMachineTypeDict().Find(type, MachineType::UNKNOW_CORE);
}

inline const BiMap<BIQType> &GetBIQTypeDict()
{
    static BiMap<BIQType> dict {
        {
            {BIQType::VEC_IQ, "VEC"},
            {BIQType::VET_IQ, "VET"},
            {BIQType::CUBE_IQ, "CUBE"},
            {BIQType::MTC_IQ, "MTC"},
            {BIQType::TMA_IQ, "TMA"},
            {BIQType::TAU_IQ, "TAU"},
            {BIQType::BCC_IQ, "STD"},
        }
    };
    return dict;
}

inline std::string BIQTypeName(BIQType type)
{
    return GetBIQTypeDict().Find(type, "BCC");
}

const uint64_t CORE_TOP_MACHINE_ID = 1000;
const uint64_t CORE_INTER_VIEW_TID = 1;
const int MACHINE_ID_OFFSET = 10000;

inline uint64_t GetProcessID(MachineType type, size_t sequence, size_t stride = 1)
{
    return (static_cast<uint64_t>(type) * MACHINE_ID_OFFSET) + sequence * stride;
}

inline MachineType GetMachineType(Pid pid)
{
    return static_cast<MachineType>(pid / MACHINE_ID_OFFSET);
}

inline int GetMachineSeq(Pid pid)
{
    return (pid % MACHINE_ID_OFFSET);
}


} // namespace JCore
