#include "LSUUtils.h"

#include "include/SimSys.h"
#include "core/Core.h"

using namespace std;
namespace JCore {
uint64_t SignExtend(uint64_t data, Opcode opcode)
{
    switch(opcode) {
        case Opcode::OP_LWI:
        case Opcode::OP_LW:
        case Opcode::OP_LWI_U:
        case Opcode::OP_LW_PCR:
            data = ((data>>31)&1) ? (((uint64_t)0xFFFFFFFF << 32) | (data & 0x00000000FFFFFFFF)) : (data & 0x00000000FFFFFFFF);
            break;
        case Opcode::OP_LHI:
        case Opcode::OP_LH:
        case Opcode::OP_LHI_U:
        case Opcode::OP_LH_PCR:
            data = ((data>>15)&1) ? (((uint64_t)0x0000FFFFFFFFFFFF << 16) | (data & 0x000000000000FFFF)) : (data & 0x000000000000FFFF);
            break;
        case Opcode::OP_LBI:
        case Opcode::OP_LB:
        case Opcode::OP_LB_PCR:
            data = ((data>>7)&1) ? (((uint64_t)0x00FFFFFFFFFFFFFF << 8) | (data & 0x00000000000000FF)) : (data & 0x00000000000000FF);
            break;
        case Opcode::OP_LWUI:
        case Opcode::OP_LWU:
        case Opcode::OP_LWUI_U:
        case Opcode::OP_LWU_PCR:
            data = data & 0x00000000FFFFFFFF;
            break;
        case Opcode::OP_LHUI:
        case Opcode::OP_LHU:
        case Opcode::OP_LHUI_U:
        case Opcode::OP_LHU_PCR:
            data = data & 0x000000000000FFFF;
            break;
        case Opcode::OP_LBUI:
        case Opcode::OP_LBU:
        case Opcode::OP_LBU_PCR:
            data = data & 0x00000000000000FF;
            break;
        default:
            break;
    }
    return data;
}

uint64_t Addr2cID(uint64_t addr, vector<uint64_t> dispByBitArr)
{
    vector<uint64_t> bitValArr;
    for (auto &b : dispByBitArr) {
        bitValArr.emplace_back((addr >> b) & 0x1);
    }

    uint32_t id = 0;
    for (uint32_t i = 0; i < bitValArr.size(); ++i) {
        id = static_cast<size_t>(id + (bitValArr[i] << i));
    }

    return id;
}

bool AddrCrossCacheline(uint64_t addr, uint32_t size, bool toMtcLsu)
{
    if (!toMtcLsu) {
        if (((addr & 0x3f) + size) <= 0x40) {
            return false;
        } else {
            return true;
        }
    } else {
        if (((addr & 0xff) + size) <= 0x100) {
            return false;
        } else {
            return true;
        }
    }
}

void GetCrossReq(MemReqBus &bus, MemReqBus &bus1, MemReqBus &bus2)
{
    // First bus
    bus1 = bus;
    bus1.size = 0x40 - (bus.addr & 0x3f);
    ASSERT(bus1.size < bus.size);

    // Second bus
    bus2 = bus;
    bus2.specWakeup = false;
    bus2.addr = (bus.addr & ~0x3f) + 0x40;
    bus2.tag = CalTag(bus2.addr);
    bus2.size = bus.size - bus1.size;
    ASSERT(bus2.size < bus.size);

    // Organize data
    if (!bus.is_load) {
        uint64_t mask = 0xFFFFFFFFFFFFFFFF;
        bus1.data = bus.data & (mask >> (bus2.size * 8));
        bus2.data = bus.data >> (bus1.size * 8);
    }
}

void MTCGetCrossReq(MemReqBus &bus, MemReqBus &bus1, MemReqBus &bus2)
{
    // First bus
    bus1 = bus;
    bus1.size = 0x100 - (bus.addr & 0xff);
    assert(bus1.size < bus.size);

    // Second bus
    bus2 = bus;
    bus2.specWakeup = false;
    bus2.addr = (bus.addr & ~0xff) + 0x100;
    bus2.tag = CalTag(bus2.addr, true);
    bus2.size = bus.size - bus1.size;
    assert(bus2.size < bus.size);

    assert((bus1.tag & 0xff) == 0);
    assert((bus2.tag & 0xff) == 0);
    // Organize data
    if (!bus.is_load) {
        uint64_t mask = 0xFFFFFFFFFFFFFFFF;
        bus1.data = bus.data & (mask >> (bus2.size * 8));
        bus2.data = bus.data >> (bus1.size * 8);
    }
}

bool AddrOverlap(uint64_t addr_s, uint32_t size_s, uint64_t addr_l, uint32_t size_l)
{
    uint64_t addr_l_h;
    uint64_t addr_s_h;
    addr_l_h = addr_l + size_l -1;
    addr_s_h = addr_s + size_s -1;
    if ((addr_l_h < addr_s) || (addr_s_h<addr_l))
        return false;
    else
        return true;
}

uint64_t ExtractData(uint2048_t &data, uint64_t addr, uint32_t size)
{
    uint8_t *p = (uint8_t*)data.bits;
    uint32_t off = addr & 0xff;
    assert(off + size <= 0x100);
    uint64_t r = 0;
    for (uint32_t i = 0; i < size; i++) {
        r = r | ((uint64_t)p[off+i] << (8 *  i));
    }
    return r;
}

uint64_t ExtractData(uint512_t &data, uint64_t addr, uint32_t size)
{
    uint8_t *p = (uint8_t*)data.bits;
    uint32_t off = addr & 0x3f;
    ASSERT(off + size <= 0x40);
    ASSERT(size <= 8);
    uint64_t r = 0;
    for (uint32_t i = 0; i < size; i++) {
        r = r | ((uint64_t)p[off+i] << (8 *  i));
    }
    return r;
}


void ClipUnalignMem(uint64_t &addr, uint32_t &size, uint64_t c_addr)
{
    uint64_t addr_o = addr;
    uint32_t size_o = size;

    if (addr_o < c_addr) {
        // xxoo
        // --|-----|
        ASSERT(addr_o + size_o > c_addr);
        addr = c_addr;
        size = size_o - (0x40 - (addr_o & 0x3f));
    } else {
        if ((addr_o + size_o) > (c_addr + 0x40)) {
            //        ooxx
            // --|-----|--
            addr = addr_o;
            size = 0x40 - (addr_o & 0x3f);
        } else {
            //     oooo
            // --|-----|--
            addr = addr_o;
            size = size_o;
        }
    }
}

void MTCClipUnalignMem(uint64_t &addr, uint32_t &size, uint64_t c_addr)
{
    uint64_t addr_o = addr;
    uint32_t size_o = size;

    if (addr_o < c_addr) {
        // xxoo
        // --|-----|
        assert(addr_o + size_o > c_addr);
        addr = c_addr;
        size = size_o - (0x100 - (addr_o & 0xff));
    } else {
        if ((addr_o + size_o) > (c_addr + 0x100)) {
            //        ooxx
            // --|-----|--
            addr = addr_o;
            size = 0x100 - (addr_o & 0xff);
        } else {
            //     oooo
            // --|-----|--
            addr = addr_o;
            size = size_o;
        }
    }
}


void UpdateSTValid(bool *valid, uint64_t st_addr, uint32_t st_size, bool val, uint64_t c_addr)
{
    uint64_t addr = st_addr;
    uint32_t size = st_size;

    ClipUnalignMem(addr, size, c_addr);

    uint32_t dcacheWaySize = 64;
    ASSERT(size <= dcacheWaySize && " size upper dcache_way_size(64)");
    int off = addr & 0x3F;
    for (uint32_t i = 0; i < size; i++) {
        valid[off + i] = val;
    }
}

void MTCUpdateSTValid(bool *valid, uint64_t st_addr, uint32_t st_size, bool val, uint64_t c_addr)
{
    uint64_t addr = st_addr;
    uint32_t size = st_size;

    MTCClipUnalignMem(addr, size, c_addr);

    int off = addr & 0xff;
    for (uint32_t i = 0; i < size; i++) {
        valid[off + i] = val;
    }
}

void UpdateData(uint2048_t &c_data, uint64_t st_addr, uint32_t st_size, uint64_t s_data, uint64_t c_addr)
{
    uint64_t addr = st_addr;
    uint32_t size = st_size;
    uint64_t data = s_data;

    MTCClipUnalignMem(addr, size, c_addr);

    if (st_addr < c_addr) {
        // xxoo
        // --|-----|
        data = data >> (8 * (st_size - size));
    }

    int dwI = (addr >> 3) & 0x7;
    uint64_t *pdw = &c_data.bits[dwI];
    uint8_t *pb = (uint8_t *)pdw;
    int off = addr & 0x7;
    pb += off;
    uint8_t *sb = (uint8_t *)&data;
    for (uint32_t i = 0; i < size; i++) {
        pb[i] = sb[i];
    }
}

void UpdateData(uint512_t &c_data, uint64_t st_addr, uint32_t st_size, uint64_t s_data, uint64_t c_addr)
{
    uint64_t addr = st_addr;
    uint32_t size = st_size;
    uint64_t data = s_data;

    ClipUnalignMem(addr, size, c_addr);

    if (st_addr < c_addr) {
        // xxoo
        // --|-----|
        data = data >> (8 * (st_size - size));
    }

    int dwI = (addr >> 3) & 0x7;
    uint64_t *pdw = &c_data.bits[dwI];
    uint8_t *pb = (uint8_t *)pdw;
    int off = addr & 0x7;
    pb += off;
    uint8_t *sb = (uint8_t *)&data;
    for (uint32_t i = 0; i < size; i++) {
        pb[i] = sb[i];
    }
}

void UpdateData(uint512_t &cache, uint8_t *data, bool *valid)
{
    for (int i = 0; i < 8; i++) {
        uint64_t *bits = &cache.bits[i];
        for (int j = 0; j < 8; j++) {
            uint8_t *dst = (uint8_t*)bits;
            if (valid[i * 8 + j]) {
                dst[j] = data[i * 8 + j];
            }
        }
    }
}

void UpdateData(uint2048_t &cache, uint8_t *data, bool *valid)
{
    for (int i = 0; i < 32; i++) {
        uint64_t *bits = &cache.bits[i];
        for (int j = 0; j < 8; j++) {
            uint8_t *dst = (uint8_t*)bits;
            if (valid[i * 8 + j]) {
                dst[j] = data[i * 8 + j];
            }
        }
    }
}

uint64_t CalTag(uint64_t addr, bool toMtcLsu)
{
    if (!toMtcLsu) {
        return addr & ~0x3f;
    } else {
        return addr & ~0xff;
    }
}

ROBID GetPrevRegSeq(SimSys* sim, ROBID seq, OperandType type, uint32_t peid, uint32_t tid)
{
    uint32_t speCnt = sim->core->configs.stdPeCount;
    uint32_t vpeCnt = sim->core->configs.simtPeCount;
    if (peid < speCnt) {
        if (type == OperandType::OPD_TLINK || type == OperandType::OPD_ULINK) {
            auto &sgprRenameUnit = sim->core->bctrl->scalarPE->d2Stage.sgprRenameUnit;
            return sgprRenameUnit[peid][tid][SGPRType2Idx(type)].GetPrevROBID(seq);
        }
    }
    if (peid >= speCnt && peid < speCnt + vpeCnt) {
        auto vpe = dynamic_pointer_cast<VecPE>(sim->core->peArray[peid]);
        auto &sgpr = vpe->rename.sgprRenameUnit[tid];
        switch (type) {
            case OperandType::OPD_TLINK:
            case OperandType::OPD_ULINK:
                return sgpr[SGPRType2Idx(type)].GetPrevROBID(seq);
            case OperandType::OPD_PREDMASK:
                return vpe->rename.predMaskRenameUnit[tid].GetPrevROBID(seq);
            default:
                break;
        }
        return seq;
    }
    return seq;
}
}