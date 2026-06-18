#pragma once
#ifndef RING_Q
#define RING_Q

#include <vector>

#include "../utils/error.h"

namespace JCore {
template <class T>
class RingQ {
    std::vector<T>          ringQ;
    uint32_t                rptr = 0;
    uint32_t                wptr = 0;
    uint32_t                qSize = 0;
    uint32_t                capacity = 0;
    uint32_t                backToFrontThreshold = 0;
    std::string             label = "";

public:
    RingQ() : capacity((std::size_t)1024) {
        Build();
    };

    RingQ(uint32_t size) : capacity(size) {
        Build();
    };

    RingQ(uint32_t size, uint32_t threshold, std::string str) : capacity(size)
    {
        Build();
        backToFrontThreshold = threshold;
        label = str;
    };
    virtual ~RingQ() {};

    void Build() {
        rptr = 0;
        wptr = 0;
        qSize = 0;
        ringQ.resize(capacity);
    };

    void Reset() {
        rptr = 0;
        wptr = 0;
        qSize = 0;
        ringQ.clear();
        ringQ.resize(capacity);
    }

    void Write(T const& pkt) {
        ringQ[wptr] = pkt;
        wptr = (wptr + 1) % capacity;
        qSize++;
        ASSERT(qSize <= capacity);
    };

    void WriteDropOldest(T const& pkt)
    {
        if (Full()) {
            (void)Read();
        }
        Write(pkt);
    }

    void WriteMoveDropOldest(T&& pkt)
    {
        if (Full()) {
            (void)Read();
        }
        ringQ[wptr] = std::move(pkt);
        wptr = (wptr + 1) % capacity;
        qSize++;
        ASSERT(qSize <= capacity);
    }

    T& BackToFrontIndex(uint32_t k)
    {
        if (k >= qSize || k > backToFrontThreshold) {
            ASSERT(false) << (label + "BackToFrontIndex: out of index range");
        }
        // 队尾元素下标 = (wptr + capacity - 1) % capacity
        // 往前k个：再减k
        uint32_t index = (wptr + capacity - 1 - k) % capacity;
        return ringQ[index];
    }

    T Read()
    {
        T front = ringQ[rptr];
        ringQ[rptr] = T();
        rptr = (rptr + 1) % capacity;
        ASSERT(qSize > 0);
        qSize--;
        return front;
    };

    T Front()
    {
        ASSERT(qSize > 0);
        T front = ringQ[rptr];
        return front;
    };

    T Back()
    {
        ASSERT(qSize > 0);
        T back = ringQ[(wptr + capacity - 1) % capacity];
        return back;
    };

    void ChangeBack(T const& pkt)
    {
        ASSERT(qSize > 0);
        uint32_t index = (wptr + capacity - 1) % capacity;
        ringQ[index] = pkt;
    };

    size_t Size()
    {
        return qSize;
    };

    bool Empty()
    {
        return (qSize == 0);
    };

    bool Full()
    {
        return qSize >= capacity;
    };

    T &operator[] (uint64_t idx) {
        uint64_t entryIndex = (idx + rptr) % capacity;
        return ringQ[entryIndex];
    };
};
}
#endif