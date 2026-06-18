#pragma once

#ifndef VEC_DATA_H
#define VEC_DATA_H

#include <cstdint>
#include <vector>

namespace JCore {
const int REG_SIZE_BIT = 64;
struct VecData {
    // Unit is bit
    bool vld = false;
    uint64_t width = 0;
    uint64_t m_lane = 0;
    // size of data
    uint64_t size = 0;
    std::vector<uint64_t> data;

    __uint128_t LEFT_MOVE_N_STEP(unsigned __int128 data, int i, int step) const
    {
        return data << (i * step);
    }

    __uint128_t RIGHT_MOVE_N_STEP(unsigned __int128 data, unsigned int i, unsigned int step) const
    {
        return data >> (i * step);
    }

    void Init(uint64_t w, uint64_t lane)
    {
        vld = true;
        width = w;
        ASSERT(width > 0);
        m_lane = lane;
        ASSERT(m_lane > 0);
        size = (w * lane) / REG_SIZE_BIT;
        auto rem = (w * lane) % REG_SIZE_BIT;
        if (rem) {
            size += 1;
        }
        size = size < 1 ? 1 : size;
        ASSERT(size > 0);
        data.resize(size);
    }

    void ReSetSize(uint32_t tmpSize)
    {
        ASSERT(tmpSize > 0);
        data.resize(tmpSize);
    }

    void Set(uint64_t gprVal)
    {
        uint64_t mask = (static_cast<__uint128_t>(1) << width) - 1;
        uint64_t d = gprVal & mask;
        uint64_t dst = 0;
        // Clear
        std::fill(data.begin(), data.end(), 0);

        // Merge to the 64-bits valaue
        ASSERT(width > 0);
        for (uint64_t i = 0; i * width < 64; ++i) {
            dst |= LEFT_MOVE_N_STEP(d, i, width);
        }

        // Set the slot
        for (uint64_t i = 0; i < size; ++i) {
            data[i] = dst;
        }
    }

    void Set(uint64_t value, uint32_t lane, uint32_t wid)
    {
        uint64_t mask = (static_cast<__uint128_t>(1) << wid) - 1;
        uint64_t dst = value & mask;
        // Get index of vector data
        uint64_t idx = (wid * lane) / REG_SIZE_BIT;
        // Get offset in a data
        uint64_t offset = ((wid * lane) % REG_SIZE_BIT) / wid;
        ASSERT(idx < data.size());
        uint64_t &src = data[idx];
        // Clear
        src &=  ~(LEFT_MOVE_N_STEP(mask, offset, wid));
        // Set the value
        src |= LEFT_MOVE_N_STEP(dst, offset, wid);
    }

    void DumpHex() const {
        std::cout << "=== Data Dump (Hex) ===" << std::endl;
        for (size_t i = 0; i < data.size(); ++i) {
            std::cout << "data[" << i << "]: 0x"
                      << std::hex << std::setw(16) << std::setfill('0')
                      << data[i] << std::dec << std::endl;
        }
        std::cout << std::endl;
    }

    void Set(uint64_t value, uint32_t lane)
    {
        uint64_t mask = (static_cast<__uint128_t>(1) << width) - 1;
        uint64_t dst = value & mask;
        // Get index of vector data
        uint64_t idx = (width * lane) / REG_SIZE_BIT;
        // Get offset in a data
        uint64_t offset = ((width * lane) % REG_SIZE_BIT) / width;
        ASSERT(idx < size);
        uint64_t &src = data[idx];
        // Clear
        src &=  ~(LEFT_MOVE_N_STEP(mask, offset, width));
        // Set the value
        src |= LEFT_MOVE_N_STEP(dst, offset, width);
    }

    void Set(const std::vector<uint64_t> &d, uint32_t beginIdx = 0)
    {
        // Clear
        std::fill(data.begin(), data.end(), 0);

        for (uint64_t i = 0; i < size; ++i) {
            uint32_t dIdx = (beginIdx + i) % d.size();
            data[i] = d[dIdx];
        }
    }
    void SetVal(const std::vector<uint8_t> &d, uint32_t lane, uint32_t wid)
    {
        constexpr uint64_t bytes = 8;
        ASSERT(d.size() <= bytes);
        uint64_t retVal = 0;
        for (uint32_t i = 0; i < d.size(); ++i) {
            retVal |= (d[i] << (bytes * i));
        }
        Set(retVal, lane);
    }
    void SetByIter(uint64_t val, uint64_t maxVal)
    {
        const int byte8 = 64;
        uint64_t mask = (static_cast<__uint128_t>(1) << width) - 1;
        uint64_t num = 64 / width;
        // Set the slot
        for (uint64_t i = 0; i < size; ++i) {
            for (uint64_t j = 0; j * width < byte8; ++j) {
                uint64_t offset = i * num + j;
                data[i] |= LEFT_MOVE_N_STEP((((val + offset) % maxVal) & mask), j, width);
            }
        }
    }

    uint64_t Get(uint32_t lane, uint32_t wid) const
    {
        ASSERT(wid != 0);
        uint64_t mask = (static_cast<__uint128_t>(1) << wid) - 1;
        // Get index of vector data
        uint64_t idx = (wid * lane) / REG_SIZE_BIT;
        // Get offset in a data
        uint64_t offset = ((wid * lane) % REG_SIZE_BIT) / wid;

        ASSERT(idx < data.size());
        return (RIGHT_MOVE_N_STEP(data[idx], offset, wid)) & mask;
    }

    uint64_t Get(uint32_t lane) const
    {
        uint64_t mask = (static_cast<__uint128_t>(1) << width) - 1;
        // Get index of vector data
        uint64_t idx = (width * lane) / REG_SIZE_BIT;
        ASSERT(width != 0);
        // Get offset in a data
        uint64_t offset = ((width * lane) % 64) / width;

        ASSERT(idx < size);
        return (RIGHT_MOVE_N_STEP(data[idx], offset, width)) & mask;
    }

    void write(std::vector<uint64_t> &d, uint32_t beginIdx = 0)
    {
        for (uint64_t i = 0; i < size; ++i) {
            uint32_t dIdx = (beginIdx + i) % d.size();
            d.at(dIdx) = data.at(i);
        }
    }

    bool CheckCopyRight(const VecData &d) const
    {
        return (this->width == d.width) && (this->size == d.size);
    }
};
}
#endif
