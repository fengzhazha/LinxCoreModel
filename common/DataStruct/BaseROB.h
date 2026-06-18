#pragma once
#ifndef BASE_ROB_H
#define BASE_ROB_H

#include <vector>
#include <cstdint>
#include <string>

#include "../utils/error.h"

namespace JCore {
template <typename T>
class BaseROB {
private:
    std::vector<T> ring_;
    uint32_t cap_  = 0;
    uint32_t rptr_ = 0;
    uint32_t wptr_ = 0;
    uint32_t size_ = 0;
    uint32_t backToFrontThreshold = 0;
    std::string label = "";
public:
    BaseROB() = default;
    explicit BaseROB(uint32_t cap, uint32_t threshold, std::string name)
    { 
        Init(cap);
        backToFrontThreshold = threshold;
        label = std::move(name);
    }

    bool Init(uint32_t cap)
    {
        if (cap == 0) {
            return false;
        }
        cap_ = cap;
        ring_.resize(cap_);
        rptr_ = wptr_ = size_ = 0;
        return true;
    }

    template <class InitFn>
    void BuildWithInit(InitFn initFn)
    {
        rptr_ = wptr_ = size_ = 0;
        ring_.resize(cap_);
        for (uint32_t i = 0; i < cap_; ++i) {
            ring_[i] = initFn(i);
        }
    }

    uint32_t Capacity() const
    {
        return cap_;
    }
    uint32_t Size() const
    { 
        return size_;
    }
    bool IsEmpty() const
    {
        return size_ == 0;
    }
    bool IsFull()  const
    { 
        return size_ == cap_;
    }
    uint32_t Rptr() const
    { 
        return rptr_;
    }
    uint32_t Wptr() const
    { 
        return wptr_;
    }

    // 按照索引访问底层数据结构
    T& operator[](uint32_t idx)
    {
        ASSERT(idx < cap_);
        return ring_[idx];
    }
    const T& operator[](uint32_t idx) const
    {
        ASSERT(idx < cap_);
        return ring_[idx];
    }

    T* HeadPtr()
    {
        return IsEmpty() ? nullptr : &ring_[rptr_];
    }
    const T* HeadPtr() const
    { 
        return IsEmpty() ? nullptr : &ring_[rptr_];
    }

    T& BackToFrontIndex(uint32_t k)
    {
        if (k >= size_ || k > backToFrontThreshold) {
            ASSERT(false) << (label + "BackToFrontIndex: out of index range");
        }
        // 队尾元素下标 = (wptr + capacity - 1) % capacity
        // 往前k个：再减k
        uint32_t index = (wptr_ + this->cap_ - 1 - k) % cap_;
        return ring_[index];
    }
    // Reset：清空队列（不改槽位内容）
    void Reset()
    {
        rptr_ = wptr_ = size_ = 0;
    }

    // Reset：清空队列 + 重新初始化所有槽位
    template <class InitFn>
    void ResetWithInit(InitFn initFn)
    {
        rptr_ = wptr_ = size_ = 0;
        for (uint32_t i = 0; i < cap_; ++i) {
            ring_[i] = initFn(i);
        }
    }

    // ----------- Alloc/Dealloc -----------
    T& Alloc()
    {
        ASSERT(!IsFull()) << label + "alloc error";
        uint32_t ptr = wptr_;
        wptr_ = Wrap(wptr_ + 1);
        ++size_;
        return ring_[ptr];
    }

    T& Dealloc()
    {
        ASSERT(!IsEmpty()) << label + "Delloc error";
        uint32_t idx = rptr_;
        rptr_ = Wrap(rptr_ + 1);
        --size_;
        return ring_[idx];
    }

private:
    uint32_t Wrap(uint32_t x) const
    {
        return x % cap_;
    }
};
}
#endif