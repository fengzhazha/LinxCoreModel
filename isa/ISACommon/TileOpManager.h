#pragma once

#ifndef TILE_OP_MANAGER
#define TILE_OP_MANAGER

#include <array>
#include <cassert>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "OperandType.h"
#include "../common/Common.h"

namespace JCore {

// For decode
constexpr uint64_t PAR32_DATATYPE_BEGIN = 27;
constexpr uint64_t PAR32_DATATYPE_END = 31;
constexpr uint64_t PAR32_MODE_BEGIN = 25;
constexpr uint64_t PAR32_MODE_END = 26;
constexpr uint64_t PAR32_FUNC_BEGIN = 20;
constexpr uint64_t PAR32_FUNC_END = 24;
constexpr uint64_t PAR32_OPCODE_BEGIN = 15;
constexpr uint64_t PAR32_OPCODE_END = 19;
constexpr uint32_t TILE_TYPE_COUNT = 9;

enum class VREGMode {
    VS8 = 0, // 块内需要2组向量寄存器，每组4个
    VS16, // 块内需要4组向量寄存器，每组4个
    VS32, // 暂不支持
    VS64, // 暂不支持
    VS_INV,
};

inline const std::array<std::string, static_cast<std::size_t>(VREGMode::VS_INV)>& VREGModeNames()
{
    static const std::array<std::string, static_cast<std::size_t>(VREGMode::VS_INV)> VREG_MODE_NAMES = {{
        "VS8",
        "VS16",
        "VS32",
        "VS64",
    }};
    return VREG_MODE_NAMES;
}

inline uint32_t GetVREGNum(VREGMode const& mode)
{
    static const std::unordered_map<VREGMode, uint32_t> VREG_TYPE_NUM = {
        {VREGMode::VS8, 8},
        {VREGMode::VS16, 16},
        {VREGMode::VS32, 32},
        {VREGMode::VS64, 64},
    };
    return VREG_TYPE_NUM.at(mode);
}

inline const std::string& GetVREGModeName(VREGMode mode)
{
    const std::size_t idx = static_cast<std::size_t>(mode);
    const auto& names = VREGModeNames();
    static const std::string INVALID_VREG_MODE = "invalid_vreg_mode";
    if (idx >= names.size()) {
        return INVALID_VREG_MODE;
    }
    return names[idx];
}

enum class ParOpcodeID {MPAR = 0, MSEQ = 1, TMA = 2, TEPL = 3, VPAR = 4, VSEQ = 5, CUBE = 6, FIXP = 7};
enum class ParFunctionMode {ONLYTILE = 0, TILEANDSCALAR = 1, REDUCEANDBROADCAST = 2, EXCEPTION = 3};
enum class ParFunction {
    TMA_TLOAD = 0,
    TMA_TSTORE = 1,
    TMA_TMOV = 2,
    TMA_MGATHER = 4,
    TMA_MSCATTER = 5,

    CUBE_TMATMUL = 0,
    CUBE_TMATMUL_BIAS = 1,
    CUBE_TMATMUL_ACC = 2,
    CUBE_TMATMULMX = 4,
    CUBE_TMATMULMX_BIAS = 5,
    CUBE_TMATMULMX_ACC = 6,
    CUBE_ACCCVT = 8,
    CUBE_TGEMV = 16,
    CUBE_TGEMV_BIAS = 17,
    CUBE_TGEMV_ACC = 18,
    TGEMVMX = 20,
    TGEMVMX_BIAS = 21,
    TGEMVMX_ACC = 22,

    TEPL_TADD = 0,
    TEPL_TSUB = 1,
    TEPL_TMUL = 2,
    TEPL_TDIV = 3,
    TEPL_TREM = 4,
    TEPL_TFMOD = 5,
    TEPL_TAND = 6,
    TEPL_TOR = 7,
    TEPL_TXOR = 8,
    TEPL_TSHL = 9,
    TEPL_TSHR = 10,
    TEPL_TMAX = 11,
    TEPL_TMIN = 12,
    TEPL_TCMP = 13,
    TEPL_TPRELU = 14,
    TEPL_TABS = 15,
    TEPL_TNOT = 16,
    TEPL_TNEG = 17,
    TEPL_TEXP = 18,
    TEPL_TLOG = 19,
    TEPL_TRECIP = 20,
    TEPL_TSQRT = 21,
    TEPL_TRSQRT = 22,
    TEPL_TRELU = 23,
    TEPL_TADDC = 24,
    TEPL_TSUBC = 25,
    TEPL_TSEL = 26,
    TEPL_TCVT = 27,

    TEPL_TADDS = 0,
    TEPL_TSUBS = 1,
    TEPL_TMULS = 2,
    TEPL_TDIVS = 3,
    TEPL_TREMS = 4,
    TEPL_TFMODS = 5,
    TEPL_TANDS = 6,
    TEPL_TORS = 7,
    TEPL_TXORS = 8,
    TEPL_TSHLS = 9,
    TEPL_TSHRS = 10,
    TEPL_TMAXS = 11,
    TEPL_TMINS = 12,
    TEPL_TCMPS = 13,
    TEPL_TLRELU = 14,
    TEPL_TADDSC = 24,
    TEPL_TSUBSC = 25,
    TEPL_TSELS = 26,
    TEPL_TEXPANDS = 27,

    TEPL_TROWSUM = 0,
    TEPL_TROWMAX = 1,
    TEPL_TROWMIN = 2,
    TEPL_TROWPROD = 3,
    TEPL_TROWEXPAND = 4,
    TEPL_TROWEXPANDADD = 5,
    TEPL_TROWEXPANDSUB = 6,
    TEPL_TROWEXPANDMUL = 7,
    TEPL_TROWEXPANDDIV = 8,
    TEPL_TROWEXPANDMAX = 9,
    TEPL_TROWEXPANDMIN = 10,
    TEPL_TROWEXPANDEXPDIF = 11,
    TEPL_TCOLSUM = 16,
    TEPL_TCOLMAX = 17,
    TEPL_TCOLMIN = 18,
    TEPL_TCOLPROD = 19,
    TEPL_TCOLEXPAND = 20,
    TEPL_TCOLEXPANDADD = 21,
    TEPL_TCOLEXPANDSUB = 22,
    TEPL_TCOLEXPANDMUL = 23,
    TEPL_TCOLEXPANDDIV = 24,
    TEPL_TCOLEXPANDMAX = 25,
    TEPL_TCOLEXPANDMIN = 26,
    TEPL_TCOLEXPANDEXPDIF = 27,

    TEPL_ESAVE = 30,
    TEPL_ERCOV = 31
};

enum class ParFunctionTEPL {
    // ParFunctionMode::ONLYTILE 28
    TEPL_TADD = 0,
    TEPL_TSUB,
    TEPL_TMUL,
    TEPL_TDIV,
    TEPL_TREM,
    TEPL_TFMOD,
    TEPL_TAND,
    TEPL_TOR,
    TEPL_TXOR,
    TEPL_TSHL,
    TEPL_TSHR,
    TEPL_TMAX,
    TEPL_TMIN,
    TEPL_TCMP,
    TEPL_TPRELU,
    TEPL_TABS,
    TEPL_TNOT,
    TEPL_TNEG,
    TEPL_TEXP,
    TEPL_TLOG,
    TEPL_TRECIP,
    TEPL_TSQRT,
    TEPL_TRSQRT,
    TEPL_TRELU,
    TEPL_TADDC,
    TEPL_TSUBC,
    TEPL_TSEL,
    TEPL_TCVT,
    // ParFunctionMode::TILEANDSCALAR 28 (includes 9 placeholders)
    TEPL_TADDS,
    TEPL_TSUBS,
    TEPL_TMULS,
    TEPL_TDIVS,
    TEPL_TREMS,
    TEPL_TFMODS,
    TEPL_TANDS,
    TEPL_TORS,
    TEPL_TXORS,
    TEPL_TSHLS,
    TEPL_TSHRS,
    TEPL_TMAXS,
    TEPL_TMINS,
    TEPL_TCMPS,
    TEPL_TLRELU,
    TILEANDSCALAR_RESERVE_1,
    TILEANDSCALAR_RESERVE_2,
    TILEANDSCALAR_RESERVE_3,
    TILEANDSCALAR_RESERVE_4,
    TILEANDSCALAR_RESERVE_5,
    TILEANDSCALAR_RESERVE_6,
    TILEANDSCALAR_RESERVE_7,
    TILEANDSCALAR_RESERVE_8,
    TILEANDSCALAR_RESERVE_9,
    TEPL_TADDSC,
    TEPL_TSUBSC,
    TEPL_TSELS,
    TEPL_TEXPANDS,
    // ParFunctionMode::REDUCEANDBROADCAST 28 (includes 4 placeholders)
    TEPL_TROWSUM,
    TEPL_TROWMAX,
    TEPL_TROWMIN,
    TEPL_TROWPROD,
    TEPL_TROWEXPAND,
    TEPL_TROWEXPANDADD,
    TEPL_TROWEXPANDSUB,
    TEPL_TROWEXPANDMUL,
    TEPL_TROWEXPANDDIV,
    TEPL_TROWEXPANDMAX,
    TEPL_TROWEXPANDMIN,
    TEPL_TROWEXPANDEXPDIF,
    REDUCEANDBROADCAST_RESERVE_1,
    REDUCEANDBROADCAST_RESERVE_2,
    REDUCEANDBROADCAST_RESERVE_3,
    REDUCEANDBROADCAST_RESERVE_4,
    TEPL_TCOLSUM,
    TEPL_TCOLMAX,
    TEPL_TCOLMIN,
    TEPL_TCOLPROD,
    TEPL_TCOLEXPAND,
    TEPL_TCOLEXPANDADD,
    TEPL_TCOLEXPANDSUB,
    TEPL_TCOLEXPANDMUL,
    TEPL_TCOLEXPANDDIV,
    TEPL_TCOLEXPANDMAX,
    TEPL_TCOLEXPANDMIN,
    TEPL_TCOLEXPANDEXPDIF,
    // ParFunctionMode::EXCEPTION 32 (includes 30 placeholders)
    EXCEPTION__RESERVE_1,
    EXCEPTION__RESERVE_2,
    EXCEPTION__RESERVE_3,
    EXCEPTION__RESERVE_4,
    EXCEPTION__RESERVE_5,
    EXCEPTION__RESERVE_6,
    EXCEPTION__RESERVE_7,
    EXCEPTION__RESERVE_8,
    EXCEPTION__RESERVE_9,
    EXCEPTION__RESERVE_10,
    EXCEPTION__RESERVE_11,
    EXCEPTION__RESERVE_12,
    EXCEPTION__RESERVE_13,
    EXCEPTION__RESERVE_14,
    EXCEPTION__RESERVE_15,
    EXCEPTION__RESERVE_16,
    EXCEPTION__RESERVE_17,
    EXCEPTION__RESERVE_18,
    EXCEPTION__RESERVE_19,
    EXCEPTION__RESERVE_20,
    EXCEPTION__RESERVE_21,
    EXCEPTION__RESERVE_22,
    EXCEPTION__RESERVE_23,
    EXCEPTION__RESERVE_24,
    EXCEPTION__RESERVE_25,
    EXCEPTION__RESERVE_26,
    EXCEPTION__RESERVE_27,
    EXCEPTION__RESERVE_28,
    EXCEPTION__RESERVE_29,
    EXCEPTION__RESERVE_30,
    TEPL_ESAVE,
    TEPL_ERCOV
};

inline ParFunctionTEPL operator+(ParFunctionTEPL lhs, ParFunctionTEPL rhs)
{
    using U = typename std::underlying_type<ParFunctionTEPL>::type;
    return static_cast<ParFunctionTEPL>(static_cast<U>(lhs) + static_cast<U>(rhs));
}

inline std::string GetTileTypeStr(OperandType hand)
{
    static std::unordered_map<OperandType, std::string> HAND_TYPE_NAME = {
        {OperandType::OPD_TILE_TLINK,    "T"},
        {OperandType::OPD_TILE_ULINK,    "U"},
        {OperandType::OPD_TILE_MLINK,    "M"},
        {OperandType::OPD_TILE_NLINK,    "N"},
        {OperandType::OPD_TILE_ACC,  "ACC"},
        {OperandType::OPD_TILE_STACK,  "STK"},
        {OperandType::OPD_TILE_MCALL_STK,  "MCALL_STK"},
        {OperandType::OPD_TILE_MCALL_GPR,  "MCALL_GPR"},
        {OperandType::OPD_TILE_DLINK,  "DEP"},
    };
    return HAND_TYPE_NAME[hand];
}

inline uint32_t TileType2Idx(OperandType hand)
{
    return static_cast<uint32_t>(hand) - static_cast<uint32_t>(OperandType::OPD_TILE_TLINK);
}

inline OperandType TileIdx2TileType(uint32_t idx)
{
    return static_cast<OperandType>(idx + static_cast<uint32_t>(OperandType::OPD_TILE_TLINK));
}

enum class TileOp {
    /* VECTOR */
    TADD,
    TSUB,
    TMUL,
    TDIV,
    TREM,
    TFMOD,
    TAND,
    TOR,
    TXOR,
    TSHL,
    TSHR,
    TMAX,
    TMIN,
    TCMP,
    TPRELU,
    TABS,
    TNOT,
    TNEG,
    TEXP,
    TLOG,
    TRECIP,
    TSQRT,
    TRSQRT,
    TRELU,
    TADDC,
    TSUBC,
    TSEL,
    TCVT,
    TADDS,
    TSUBS,
    TMULS,
    TDIVS,
    TREMS,
    TFMODS,
    TANDS,
    TORS,
    TXORS,
    TSHLS,
    TSHRS,
    TMAXS,
    TMINS,
    TCMPS,
    TLRELU,
    TADDSC,
    TSUBSC,
    TSELS,
    TEXPANDS,
    TROWSUM,
    TROWMAX,
    TROWMIN,
    TROWPROD,
    TROWEXPAND,
    TROWEXPANDADD,
    TROWEXPANDSUB,
    TROWEXPANDMUL,
    TROWEXPANDDIV,
    TROWEXPANDMAX,
    TROWEXPANDMIN,
    TROWEXPANDEXPDIF,
    TCOLSUM,
    TCOLMAX,
    TCOLMIN,
    TCOLPROD,
    TCOLEXPAND,
    TCOLEXPANDADD,
    TCOLEXPANDSUB,
    TCOLEXPANDMUL,
    TCOLEXPANDDIV,
    TCOLEXPANDMAX,
    TCOLEXPANDMIN,
    TCOLEXPANDEXPDIF,
    ESAVE,
    ERCOV,
    /* TMA */
    TLOAD,
    TSTORE,
    TMOV,
    MGATHER,
    MSCATTER,
    VGATHER,
    VSCATTER,
    /* CUBE */
    TMATMUL,
    TMATMUL_BIAS,
    TMATMUL_ACC,
    TMATMULMX,
    TMATMULMX_BIAS,
    TMATMULMX_ACC,
    ACCCVT,
    TGEMV,
    TGEMV_BIAS,
    TGEMV_ACC,
    TGEMVMX,
    TGEMVMX_BIAS,
    TGEMVMX_ACC,
    // STASH
    TSTASH_TA_INPUT,
    TSTASH_TO_OUTPUT,
    OTHER_TSTASH,
    // RING
    TRING,
    TRING_TIME_1,
    TRING_TIME_2,
    TRING_TIME_3,
    TRING_TIME_4,
    TINVALID,
    TILE_OP_NUMBER,
};

inline TileOp GetTeplTileOp(ParFunctionTEPL tepl)
{
    switch (tepl) {
        case ParFunctionTEPL::TEPL_TADD:
            return TileOp::TADD;
        case ParFunctionTEPL::TEPL_TSUB:
            return TileOp::TSUB;
        case ParFunctionTEPL::TEPL_TMUL:
            return TileOp::TMUL;
        case ParFunctionTEPL::TEPL_TDIV:
            return TileOp::TDIV;
        case ParFunctionTEPL::TEPL_TREM:
            return TileOp::TREM;
        case ParFunctionTEPL::TEPL_TFMOD:
            return TileOp::TFMOD;
        case ParFunctionTEPL::TEPL_TAND:
            return TileOp::TAND;
        case ParFunctionTEPL::TEPL_TOR:
            return TileOp::TOR;
        case ParFunctionTEPL::TEPL_TXOR:
            return TileOp::TXOR;
        case ParFunctionTEPL::TEPL_TSHL:
            return TileOp::TSHL;
        case ParFunctionTEPL::TEPL_TSHR:
            return TileOp::TSHR;
        case ParFunctionTEPL::TEPL_TMAX:
            return TileOp::TMAX;
        case ParFunctionTEPL::TEPL_TMIN:
            return TileOp::TMIN;
        case ParFunctionTEPL::TEPL_TCMP:
            return TileOp::TCMP;
        case ParFunctionTEPL::TEPL_TPRELU:
            return TileOp::TPRELU;
        case ParFunctionTEPL::TEPL_TABS:
            return TileOp::TABS;
        case ParFunctionTEPL::TEPL_TNOT:
            return TileOp::TNOT;
        case ParFunctionTEPL::TEPL_TNEG:
            return TileOp::TNEG;
        case ParFunctionTEPL::TEPL_TEXP:
            return TileOp::TEXP;
        case ParFunctionTEPL::TEPL_TLOG:
            return TileOp::TLOG;
        case ParFunctionTEPL::TEPL_TRECIP:
            return TileOp::TRECIP;
        case ParFunctionTEPL::TEPL_TSQRT:
            return TileOp::TSQRT;
        case ParFunctionTEPL::TEPL_TRSQRT:
            return TileOp::TRSQRT;
        case ParFunctionTEPL::TEPL_TRELU:
            return TileOp::TRELU;
        case ParFunctionTEPL::TEPL_TADDC:
            return TileOp::TADDC;
        case ParFunctionTEPL::TEPL_TSUBC:
            return TileOp::TSUBC;
        case ParFunctionTEPL::TEPL_TSEL:
            return TileOp::TSEL;
        case ParFunctionTEPL::TEPL_TCVT:
            return TileOp::TCVT;
        case ParFunctionTEPL::TEPL_TADDS:
            return TileOp::TADDS;
        case ParFunctionTEPL::TEPL_TSUBS:
            return TileOp::TSUBS;
        case ParFunctionTEPL::TEPL_TMULS:
            return TileOp::TMULS;
        case ParFunctionTEPL::TEPL_TDIVS:
            return TileOp::TDIVS;
        case ParFunctionTEPL::TEPL_TREMS:
            return TileOp::TREMS;
        case ParFunctionTEPL::TEPL_TFMODS:
            return TileOp::TFMODS;
        case ParFunctionTEPL::TEPL_TANDS:
            return TileOp::TANDS;
        case ParFunctionTEPL::TEPL_TORS:
            return TileOp::TORS;
        case ParFunctionTEPL::TEPL_TXORS:
            return TileOp::TXORS;
        case ParFunctionTEPL::TEPL_TSHLS:
            return TileOp::TSHLS;
        case ParFunctionTEPL::TEPL_TSHRS:
            return TileOp::TSHRS;
        case ParFunctionTEPL::TEPL_TMAXS:
            return TileOp::TMAXS;
        case ParFunctionTEPL::TEPL_TMINS:
            return TileOp::TMINS;
        case ParFunctionTEPL::TEPL_TCMPS:
            return TileOp::TCMPS;
        case ParFunctionTEPL::TEPL_TLRELU:
            return TileOp::TLRELU;
        case ParFunctionTEPL::TEPL_TADDSC:
            return TileOp::TADDSC;
        case ParFunctionTEPL::TEPL_TSUBSC:
            return TileOp::TSUBSC;
        case ParFunctionTEPL::TEPL_TSELS:
            return TileOp::TSELS;
        case ParFunctionTEPL::TEPL_TEXPANDS:
            return TileOp::TEXPANDS;
        case ParFunctionTEPL::TEPL_TROWSUM:
            return TileOp::TROWSUM;
        case ParFunctionTEPL::TEPL_TROWMAX:
            return TileOp::TROWMAX;
        case ParFunctionTEPL::TEPL_TROWMIN:
            return TileOp::TROWMIN;
        case ParFunctionTEPL::TEPL_TROWPROD:
            return TileOp::TROWPROD;
        case ParFunctionTEPL::TEPL_TROWEXPAND:
            return TileOp::TROWEXPAND;
        case ParFunctionTEPL::TEPL_TROWEXPANDADD:
            return TileOp::TROWEXPANDADD;
        case ParFunctionTEPL::TEPL_TROWEXPANDSUB:
            return TileOp::TROWEXPANDSUB;
        case ParFunctionTEPL::TEPL_TROWEXPANDMUL:
            return TileOp::TROWEXPANDMUL;
        case ParFunctionTEPL::TEPL_TROWEXPANDDIV:
            return TileOp::TROWEXPANDDIV;
        case ParFunctionTEPL::TEPL_TROWEXPANDMAX:
            return TileOp::TROWEXPANDMAX;
        case ParFunctionTEPL::TEPL_TROWEXPANDMIN:
            return TileOp::TROWEXPANDMIN;
        case ParFunctionTEPL::TEPL_TROWEXPANDEXPDIF:
            return TileOp::TROWEXPANDEXPDIF;
        case ParFunctionTEPL::TEPL_TCOLSUM:
            return TileOp::TCOLSUM;
        case ParFunctionTEPL::TEPL_TCOLMAX:
            return TileOp::TCOLMAX;
        case ParFunctionTEPL::TEPL_TCOLMIN:
            return TileOp::TCOLMIN;
        case ParFunctionTEPL::TEPL_TCOLPROD:
            return TileOp::TCOLPROD;
        case ParFunctionTEPL::TEPL_TCOLEXPAND:
            return TileOp::TCOLEXPAND;
        case ParFunctionTEPL::TEPL_TCOLEXPANDADD:
            return TileOp::TCOLEXPANDADD;
        case ParFunctionTEPL::TEPL_TCOLEXPANDSUB:
            return TileOp::TCOLEXPANDSUB;
        case ParFunctionTEPL::TEPL_TCOLEXPANDMUL:
            return TileOp::TCOLEXPANDMUL;
        case ParFunctionTEPL::TEPL_TCOLEXPANDDIV:
            return TileOp::TCOLEXPANDDIV;
        case ParFunctionTEPL::TEPL_TCOLEXPANDMAX:
            return TileOp::TCOLEXPANDMAX;
        case ParFunctionTEPL::TEPL_TCOLEXPANDMIN:
            return TileOp::TCOLEXPANDMIN;
        case ParFunctionTEPL::TEPL_TCOLEXPANDEXPDIF:
            return TileOp::TCOLEXPANDEXPDIF;
        case ParFunctionTEPL::TEPL_ESAVE:
            return TileOp::ESAVE;
        case ParFunctionTEPL::TEPL_ERCOV:
            return TileOp::ERCOV;
        default:
            ASSERT(false && "Undefined TEPL TileOp function");
    }
    return TileOp::TINVALID;
}

class TileOpManager {
public:
    static TileOpManager &Inst()
    {
        static TileOpManager inst;
        return inst;
    }

    bool HasTileOp(TileOp opcode) const
    {
        return static_cast<int>(opcode) >= 0 && static_cast<size_t>(opcode) < opcodeInfos_.size();
    }
    bool HasTileOp(const std::string &str) const
    {
        return strToEnum_.count(str) > 0;
    }

    TileOp GetTileOp(const std::string &str) const
    {
        auto it = strToEnum_.find(str);
        assert(it != strToEnum_.end() ||
           (std::cerr << "Invalid key: " << str << std::endl, false));
        return it->second;
    }
    std::string &GetTileOpStr(TileOp opcode)
    {
        if (!HasTileOp(opcode)) {
            static std::string result = "UNDEFINED_TILEOP";
            return result;
        }
        return opcodeInfos_[static_cast<int>(opcode)].str;
    }

    const MachineType &GetMachineType(TileOp opcode)
    {
        assert(HasTileOp(opcode));
        return opcodeInfos_[static_cast<int>(opcode)].coreType;
    }

    std::string PrintSupportTileOpcodes() const
    {
        std::stringstream ss;
        ss << "[";
        bool isFirst = true;
        for (const auto &info : opcodeInfos_) {
            if (!isFirst) {
                ss << ", ";
            }
            isFirst = false;
            ss << info.str;
        }
        ss << "]";
        return ss.str();
    }

private:
    struct TileOpcodeInfo {
        TileOp opcode;
        std::string str;
        MachineType coreType;

        TileOpcodeInfo() = default;
        TileOpcodeInfo(TileOp op, std::string s, MachineType cType)
            : opcode(op), str(s), coreType(cType) {}
    };

private:
    TileOpManager();

    // Register the information for each opcode; currently, only the name has been implemented.
    std::unordered_set<TileOp> registered;
    void RegisterTileOpInfo(TileOp opcode, std::string str, MachineType cType);
    void RegisterTileOpcode();

private:
    std::array<TileOpcodeInfo, static_cast<int>(TileOp::TILE_OP_NUMBER)> opcodeInfos_{};
    std::unordered_map<std::string, TileOp> strToEnum_;
};

inline std::string GetTileOpName(TileOp opcode)
{
    return TileOpManager::Inst().GetTileOpStr(opcode);
}

inline TileOp GetTileOpByString(std::string s)
{
    std::string oldStr = ".";
    std::string newStr = "_";
    if (!s.empty() && s[0] == '.') {
        s.erase(0, 1);
    }

    while (s.find(oldStr) != std::string::npos) {
        s.replace(s.find(oldStr), oldStr.length(), newStr);
    }

    return TileOpManager::Inst().GetTileOp(s);
}

inline bool TileOpAllocACC(TileOp opcode)
{
    return opcode == TileOp::TMATMUL || opcode == TileOp::TMATMUL_BIAS ||
           opcode == TileOp::TMATMULMX || opcode == TileOp::TMATMULMX_BIAS;
}

inline bool ReleaseACCTag(TileOp opcode)
{
    return opcode == TileOp::ACCCVT;
}

inline bool ForwardACCFlag(TileOp opcode)
{
    return opcode == TileOp::TMATMUL || opcode == TileOp::TMATMULMX ||
           opcode == TileOp::TMATMUL_BIAS || opcode == TileOp::TMATMULMX_BIAS ||
           opcode == TileOp::TMATMUL_ACC || opcode == TileOp::TMATMULMX_ACC;
}

inline bool CubeTileOp(TileOp opcode)
{
    const static std::unordered_set<TileOp> CUBE_OP = {
        TileOp::TMATMUL, TileOp::TMATMULMX,
        TileOp::TMATMUL_BIAS, TileOp::TMATMULMX_BIAS,
        TileOp::TMATMUL_ACC, TileOp::TMATMULMX_ACC,
        TileOp::ACCCVT,
    };
    return CUBE_OP.count(opcode) != 0;
}
} // namespace JCore
#endif
