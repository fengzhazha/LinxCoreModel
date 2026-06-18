#include "DecodeUtiles.h"

namespace JCore {
int64_t DecodeUtiles::Extract32(uint32_t bin, int start, int length)
{
    return static_cast<int64_t>(GetBits(bin, start + length - 1, start));
}

int64_t DecodeUtiles::Sextract32(uint32_t bin, int start, int length)
{
    const uint64_t regWidth = 64;
    uint64_t distance = regWidth - length;
    int64_t sextractData = static_cast<int64_t>(GetBits(bin, start + length - 1, start));
    return (sextractData << distance >> distance);
}

int64_t DecodeUtiles::Extract48(uint64_t bin, int start, int length)
{
    return GetBits(bin, start + length - 1, start);
}

int64_t DecodeUtiles::Sextract48(uint64_t bin, int start, int length)
{
    int distance = 64 - length;
    int64_t sextractData = static_cast<int64_t>(GetBits(bin, start + length - 1, start));
    return (sextractData << distance >> distance);
}

int64_t DecodeUtiles::Extract64(uint64_t bin, int start, int length)
{
    return GetBits(bin, start + length - 1, start);
}

int64_t DecodeUtiles::Sextract64(uint64_t bin, int start, int length)
{
    int distance = 64 - length;
    int64_t sextractData = static_cast<int64_t>(GetBits(bin, start + length - 1, start));
    return (sextractData << distance >> distance);
}

uint64_t DecodeUtiles::Deposit(uint64_t second, int shift, uint64_t first)
{
    return (first << shift) | second;
}

uint32_t DecodeUtiles::Deposit32(uint32_t value, int start, int length, uint32_t fieldval)
{
    uint32_t mask32;
    constexpr int zero = 0;
    constexpr int maxLen = 32;
    assert(start >= zero && length > zero && length <= maxLen - start);
    mask32 = (~0U >> (maxLen - length)) << start;
    return (value & ~mask32) | ((fieldval << start) & mask32);
}

uint64_t DecodeUtiles::Deposit64(uint64_t value, int start, int length, uint64_t fieldval)
{
    uint64_t mask64;
    constexpr int zero = 0;
    constexpr int maxLen = 64;
    assert(start >= zero && length > zero && length <= maxLen - start);
    mask64 = (~0ULL >> (maxLen - length)) << start;
    return (value & ~mask64) | ((fieldval << start) & mask64);
}

uint64_t DecodeUtiles::AutoInc1(uint64_t bin)
{
    return bin + 1;
}
uint64_t DecodeUtiles::AutoInc2(uint64_t bin)
{
    return bin + 2;
}
uint64_t DecodeUtiles::AutoInc3(uint64_t bin)
{
    return bin + 3;
}
}