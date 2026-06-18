#pragma once
#ifndef SYS_REG_H
#define SYS_REG_H

#include <cstdint>
#include <string>
#include <unordered_map>

namespace JCore {
enum class SystemReg {
    SYS_INVALID = -1,

    SYS_TP = 0x00,
    SYS_GP = 0x01,

    SYS_TIME = 0x10,
    SYS_CYCLE = 0x11,

    SYS_CSTATE = 0x20,
    SYS_LXLCID = 0x21,
    SYS_VENDOR = 0x22,
    SYS_VERSION = 0x23,
    SYS_CAPA = 0x24,
    SYS_CAPA_EN = 0x25,

    SYS_TEMP_CORE_ID = 0x30, // 仅为调试SMT，非正式功能，指令集未定义此系统寄存器

    SYS_BLOCKNUM = 0x50,
    SYS_BLOCKID = 0x51,

    SYS_TR1 = 0x800,
    SYS_TR2,
    SYS_SYSCNT = 0x810,
    SYS_CW = 0x820,

    SYS_MSGBCR = 0x0830,
    SYS_MSGBD1,
    SYS_MSGBD2,
    SYS_MSGBD3,
    SYS_MSGBD4,
    SYS_MSGBD5,
    SYS_MSGBD6,
    SYS_MSGBD7,
    SYS_MSGBD8,
    SYS_MSGBD9,
    SYS_MSGBD10,

    SYS_PMEVCNT1 = 0x101,
    SYS_PMEVCNT2,
    SYS_PMEVCNT3,
    SYS_PMEVCNT4,
    SYS_PMEVCNT5,
    SYS_PMEVCNT6,
    SYS_PMEVCNT7,
    SYS_PMEVCNT8,
    SYS_PMEVCNT9,
    SYS_PMEVCNT10,
    SYS_PMEVCNT11,
    SYS_PMEVCNT12,
    SYS_PMEVCNT13,
    SYS_PMEVCNT14,
    SYS_PMEVCNT15,
    SYS_PMEVCNT16,
    SYS_PMEVCNT17,
    SYS_PMEVCNT18,
    SYS_PMEVCNT19,
    SYS_PMEVCNT20,
    SYS_PMEVCNT21,
    SYS_PMEVCNT22,
    SYS_PMEVCNT23,
    SYS_PMEVCNT24,
    SYS_PMEVCNT25,
    SYS_PMEVCNT26,
    SYS_PMEVCNT27,
    SYS_PMEVCNT28,
    SYS_PMEVCNT29,
    SYS_PMEVCNT30,
    SYS_PMEVCNT31,
    SYS_PMEVCFG,
    SYS_PMEVCFG1,
    SYS_PMEVCFG2,
    SYS_PMEVCFG3,
    SYS_PMEVCFG4,
    SYS_PMEVCFG5,
    SYS_PMEVCFG6,
    SYS_PMEVCFG7,
    SYS_PMEVCFG8,
    SYS_PMEVCFG9,
    SYS_PMEVCFG10,
    SYS_PMEVCFG11,
    SYS_PMEVCFG12,
    SYS_PMEVCFG13,
    SYS_PMEVCFG14,
    SYS_PMEVCFG15,
    SYS_PMEVCFG16,
    SYS_PMEVCFG17,
    SYS_PMEVCFG18,
    SYS_PMEVCFG19,
    SYS_PMEVCFG20,
    SYS_PMEVCFG21,
    SYS_PMEVCFG22,
    SYS_PMEVCFG23,
    SYS_PMEVCFG24,
    SYS_PMEVCFG25,
    SYS_PMEVCFG26,
    SYS_PMEVCFG27,
    SYS_PMEVCFG28,
    SYS_PMEVCFG29,
    SYS_PMEVCFG30,
    SYS_PMEVCFG31,

    SYS_PMCR = 0x140,
    SYS_PMCNTENSET,
    SYS_PMCNTENCLR,
    SYS_PMINTENSET,
    SYS_PMINTENCLR,
    SYS_PMOVSSET,
    SYS_PMOVSCLR,
    SYS_PMCCFILTR,
    SYS_PMCEID0,
    SYS_PMCEID1,
    SYS_PMCEID2,
    SYS_PMCEID3,
    SYS_PMCEID4,
    SYS_PMCEID5,
    SYS_PMCEID6,
    SYS_PMCEID7,

    PRIVATE_LC0 = 0xd02,
    PRIVATE_LC1,
    PRIVATE_LC2,
    PRIVATE_LB0 = 0xe02,
    PRIVATE_LB1,
    PRIVATE_LB2,
    SYS_CURLC0,
    SYS_NEXTLC0,
    SYS_PLB0,
    SYS_FSSR,

    SYS_REG_NUMBER,
};

const std::unordered_map<SystemReg, std::string> gSysRegTab = {
    {SystemReg::SYS_TP, "tp"},
    {SystemReg::SYS_GP, "gp"},
    {SystemReg::SYS_TIME, "time"},
    {SystemReg::SYS_CYCLE, "cycle"},
    {SystemReg::SYS_CSTATE, "cstate"},
    {SystemReg::SYS_LXLCID, "lxlcid"},
    {SystemReg::SYS_VENDOR, "vendor"},
    {SystemReg::SYS_VERSION, "version"},
    {SystemReg::SYS_CAPA, "capa"},
    {SystemReg::SYS_CAPA_EN, "capa_en"},

    {SystemReg::SYS_TEMP_CORE_ID, "tmp_coreID"},
    {SystemReg::SYS_BLOCKNUM, "BlockNum"},
    {SystemReg::SYS_BLOCKID, "BlockID"},

    {SystemReg::SYS_TR1, "tr1"},
    {SystemReg::SYS_TR2, "tr2"},
    {SystemReg::SYS_SYSCNT, "syscnt"},
    {SystemReg::SYS_CW, "cw"},
    {SystemReg::SYS_MSGBCR, "msgbcr"},
    {SystemReg::SYS_MSGBD1, "msgbd1"},
    {SystemReg::SYS_MSGBD2, "msgbd2"},
    {SystemReg::SYS_MSGBD3, "msgbd3"},
    {SystemReg::SYS_MSGBD4, "msgbd4"},
    {SystemReg::SYS_MSGBD5, "msgbd5"},
    {SystemReg::SYS_MSGBD6, "msgbd6"},
    {SystemReg::SYS_MSGBD7, "msgbd7"},
    {SystemReg::SYS_MSGBD8, "msgbd8"},
    {SystemReg::SYS_MSGBD9, "msgbd9"},
    {SystemReg::SYS_MSGBD10, "msgbd10"},
    {SystemReg::SYS_PMEVCNT1, "pmevcnt1"},
    {SystemReg::SYS_PMEVCNT2, "pmevcnt2"},
    {SystemReg::SYS_PMEVCNT3, "pmevcnt3"},
    {SystemReg::SYS_PMEVCNT4, "pmevcnt4"},
    {SystemReg::SYS_PMEVCNT5, "pmevcnt5"},
    {SystemReg::SYS_PMEVCNT6, "pmevcnt6"},
    {SystemReg::SYS_PMEVCNT7, "pmevcnt7"},
    {SystemReg::SYS_PMEVCNT8, "pmevcnt8"},
    {SystemReg::SYS_PMEVCNT9, "pmevcnt9"},
    {SystemReg::SYS_PMEVCNT10, "pmevcnt10"},
    {SystemReg::SYS_PMEVCNT11, "pmevcnt11"},
    {SystemReg::SYS_PMEVCNT12, "pmevcnt12"},
    {SystemReg::SYS_PMEVCNT13, "pmevcnt13"},
    {SystemReg::SYS_PMEVCNT14, "pmevcnt14"},
    {SystemReg::SYS_PMEVCNT15, "pmevcnt15"},
    {SystemReg::SYS_PMEVCNT16, "pmevcnt16"},
    {SystemReg::SYS_PMEVCNT17, "pmevcnt17"},
    {SystemReg::SYS_PMEVCNT18, "pmevcnt18"},
    {SystemReg::SYS_PMEVCNT19, "pmevcnt19"},
    {SystemReg::SYS_PMEVCNT20, "pmevcnt20"},
    {SystemReg::SYS_PMEVCNT21, "pmevcnt21"},
    {SystemReg::SYS_PMEVCNT22, "pmevcnt22"},
    {SystemReg::SYS_PMEVCNT23, "pmevcnt23"},
    {SystemReg::SYS_PMEVCNT24, "pmevcnt24"},
    {SystemReg::SYS_PMEVCNT25, "pmevcnt25"},
    {SystemReg::SYS_PMEVCNT26, "pmevcnt26"},
    {SystemReg::SYS_PMEVCNT27, "pmevcnt27"},
    {SystemReg::SYS_PMEVCNT28, "pmevcnt28"},
    {SystemReg::SYS_PMEVCNT29, "pmevcnt29"},
    {SystemReg::SYS_PMEVCNT30, "pmevcnt30"},
    {SystemReg::SYS_PMEVCNT31, "pmevcnt31"},
    {SystemReg::SYS_PMEVCFG, "pmevcfg"},
    {SystemReg::SYS_PMEVCFG1, "pmevcfg1"},
    {SystemReg::SYS_PMEVCFG2, "pmevcfg2"},
    {SystemReg::SYS_PMEVCFG3, "pmevcfg3"},
    {SystemReg::SYS_PMEVCFG4, "pmevcfg4"},
    {SystemReg::SYS_PMEVCFG5, "pmevcfg5"},
    {SystemReg::SYS_PMEVCFG6, "pmevcfg6"},
    {SystemReg::SYS_PMEVCFG7, "pmevcfg7"},
    {SystemReg::SYS_PMEVCFG8, "pmevcfg8"},
    {SystemReg::SYS_PMEVCFG9, "pmevcfg9"},
    {SystemReg::SYS_PMEVCFG10, "pmevcfg10"},
    {SystemReg::SYS_PMEVCFG11, "pmevcfg11"},
    {SystemReg::SYS_PMEVCFG12, "pmevcfg12"},
    {SystemReg::SYS_PMEVCFG13, "pmevcfg13"},
    {SystemReg::SYS_PMEVCFG14, "pmevcfg14"},
    {SystemReg::SYS_PMEVCFG15, "pmevcfg15"},
    {SystemReg::SYS_PMEVCFG16, "pmevcfg16"},
    {SystemReg::SYS_PMEVCFG17, "pmevcfg17"},
    {SystemReg::SYS_PMEVCFG18, "pmevcfg18"},
    {SystemReg::SYS_PMEVCFG19, "pmevcfg19"},
    {SystemReg::SYS_PMEVCFG20, "pmevcfg20"},
    {SystemReg::SYS_PMEVCFG21, "pmevcfg21"},
    {SystemReg::SYS_PMEVCFG22, "pmevcfg22"},
    {SystemReg::SYS_PMEVCFG23, "pmevcfg23"},
    {SystemReg::SYS_PMEVCFG24, "pmevcfg24"},
    {SystemReg::SYS_PMEVCFG25, "pmevcfg25"},
    {SystemReg::SYS_PMEVCFG26, "pmevcfg26"},
    {SystemReg::SYS_PMEVCFG27, "pmevcfg27"},
    {SystemReg::SYS_PMEVCFG28, "pmevcfg28"},
    {SystemReg::SYS_PMEVCFG29, "pmevcfg29"},
    {SystemReg::SYS_PMEVCFG30, "pmevcfg30"},
    {SystemReg::SYS_PMEVCFG31, "pmevcfg31"},
    {SystemReg::SYS_PMCR, "pmcr"},
    {SystemReg::SYS_PMCNTENSET, "pmcntenset"},
    {SystemReg::SYS_PMCNTENCLR, "pmcntenclr"},
    {SystemReg::SYS_PMINTENSET, "pmintenset"},
    {SystemReg::SYS_PMINTENCLR, "pmintenclr"},
    {SystemReg::SYS_PMOVSSET, "pmovsset"},
    {SystemReg::SYS_PMOVSCLR, "pmovsclr"},
    {SystemReg::SYS_PMCCFILTR, "pmccfiltr"},
    {SystemReg::SYS_PMCEID0, "pmceid0"},
    {SystemReg::SYS_PMCEID1, "pmceid1"},
    {SystemReg::SYS_PMCEID2, "pmceid2"},
    {SystemReg::SYS_PMCEID3, "pmceid3"},
    {SystemReg::SYS_PMCEID4, "pmceid4"},
    {SystemReg::SYS_PMCEID5, "pmceid5"},
    {SystemReg::SYS_PMCEID6, "pmceid6"},
    {SystemReg::SYS_PMCEID7, "pmceid7"},
    {SystemReg::SYS_FSSR, "fssr"},
};
inline std::string GetSysRegName(uint32_t sysRegId)
{
    if (sysRegId >= static_cast<uint32_t>(SystemReg::SYS_REG_NUMBER)) {
        return std::to_string(sysRegId);
    }
    return gSysRegTab.at(static_cast<SystemReg>(sysRegId));
}

enum class LocalStatusRegID {
    LSR_BPC = 0,
    LSR_BPCN = 1,
    LSR_OTHERS = 2,
    LSR_INVALID,
};

inline const std::unordered_map<LocalStatusRegID, std::string>& LocalStatusRegNames()
{
    static const std::unordered_map<LocalStatusRegID, std::string> LOCAL_STATUS_NAMES = {{
        {LocalStatusRegID::LSR_BPC, "BPC"},
        {LocalStatusRegID::LSR_BPCN, "BPCN"},
        {LocalStatusRegID::LSR_OTHERS, "OTHERS"},
    }};
    return LOCAL_STATUS_NAMES;
}

inline const std::string& GetLocalStatuRegIDName(LocalStatusRegID id)
{
    const auto& names = LocalStatusRegNames();
    static const std::string INVALID_LSR_NAMES = "invalid_lsr";
    if (names.find(id) == names.end()) {
        return INVALID_LSR_NAMES;
    }
    return names.at(id);
}

} // namespace JCore

#endif