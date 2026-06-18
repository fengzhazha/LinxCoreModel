#pragma once

#ifndef LSU_UTILS_H
#define LSU_UTILS_H

#include <vector>
#include <cstdint>

#include "ISA.h"
#include "bus/MemReqBus.h"

namespace JCore {
class SimSys;
uint64_t SignExtend(uint64_t data, Opcode opcode);
uint64_t Addr2cID(uint64_t addr, std::vector<uint64_t> dispByBitArr);
bool AddrCrossCacheline(uint64_t addr, uint32_t size, bool toMtcLsu = false);
void GetCrossReq(MemReqBus &bus, MemReqBus &firstBus, MemReqBus &secondBus);
void MTCGetCrossReq(MemReqBus &bus, MemReqBus &bus1, MemReqBus &bus2);
bool AddrOverlap(uint64_t addr_s, uint32_t size_s, uint64_t addr_l, uint32_t size_l);
uint64_t ExtractData(uint512_t &data, uint64_t addr, uint32_t size);
uint64_t ExtractData(uint2048_t &data, uint64_t addr, uint32_t size);
void UpdateData(uint512_t &blk, uint64_t addr, uint32_t size, uint64_t data, uint64_t blk_addr);
void UpdateData(uint512_t &blk, uint8_t *data, bool *valid);
void UpdateData(uint2048_t &blk, uint64_t addr, uint32_t size, uint64_t data, uint64_t blk_addr);
void UpdateData(uint2048_t &blk, uint8_t *data, bool *valid);
void UpdateSTValid(bool *valid, uint64_t addr, uint32_t size, bool val, uint64_t blk_addr);
void MTCUpdateSTValid(bool *valid, uint64_t st_addr, uint32_t st_size, bool val, uint64_t c_addr);
uint64_t CalTag(uint64_t addr, bool toMtcLsu = false);
ROBID GetPrevRegSeq(SimSys* sim, ROBID seq, OperandType type, uint32_t peid, uint32_t tid);
}
#endif