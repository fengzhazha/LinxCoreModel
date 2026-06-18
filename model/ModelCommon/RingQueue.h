#pragma once
#ifndef RING_QUEUE_H
#define RING_QUEUE_H

#include <deque>

#include "include/ModelSpec.h"

namespace JCore {
template <class T>
class RingQueue : public SimObj {
public:
    static const int        MAX_RING_SIZE = 1024;
    std::vector<T>          ringQ;
    uint32_t                rptr;
    uint32_t                wptr;
    uint32_t                qSize;
    uint32_t                capacity;

    RingQueue() : capacity(MAX_RING_SIZE)
    {
        Build();
    };

    RingQueue(uint32_t size) : capacity(size)
    {
        Build();
    };

    virtual ~RingQueue() {};

    void Build()
    {
        rptr = 0;
        wptr = 0;
        qSize = 0;
        ringQ.resize(capacity);
    };

    void Work() override
    {
    };

    void Xfer() override
    {
    };

    void Reset() override
    {
        rptr = 0;
        wptr = 0;
        qSize = 0;
        ringQ.clear();
        ringQ.resize(capacity);
    }

    SimSys* GetSim() override
    {
        ASSERT(0);
        return nullptr;
    };

    void ReportStat() override {}

    void Write(T const& pkt)
    {
        ringQ[wptr] = pkt;
        wptr = (wptr + 1) % capacity;
        qSize++;
        ASSERT(qSize <= capacity);
    };

    T Read()
    {
        T front = ringQ[rptr];
        ringQ[rptr] = T();
        rptr = (rptr + 1) % capacity;
        ASSERT(qSize > 0);
        qSize--;
        return front;
    };

    void UpdateFront(T const& pkt)
    {
        ringQ[rptr] = pkt;
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

    size_t Size() const
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

    T &operator[] (uint64_t idx)
    {
        uint64_t entryIndex = (idx + rptr) % capacity;
        return ringQ[entryIndex];
    };

    template <class Function>
    void FlushIf(Function& match)
    {
        RingQueue tmpQ = RingQueue(capacity);
        for (uint32_t i = 0; i < qSize; i++) {
            uint32_t entryIdx = (rptr + i) % capacity;
            if (!match(ringQ[entryIdx])) {
                tmpQ.Write(ringQ[entryIdx]);
            }
        }

        ringQ = std::move(tmpQ.ringQ);
        rptr = tmpQ.rptr;
        wptr = tmpQ.wptr;
        qSize = tmpQ.qSize;
    }

    template <class Function>
    void Handle(Function& func)
    {
        for (uint32_t i = 0; i < capacity; i++) {
            uint32_t entryIdx = (rptr + i) % capacity;
            func(ringQ[entryIdx]);
        }
    }

    bool Find(T &elememt)
    {
        for (uint32_t i = 0; i < capacity; i++) {
            uint32_t entryIdx = (rptr + i) % capacity;
            if (ringQ[entryIdx] == elememt) {
                return true;
            }
        }

        return false;
    }
};

} // namespace JCore
#endif