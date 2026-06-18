#pragma once
#ifndef SIM_QUEUE_H
#define SIM_QUEUE_H

#include <deque>

#include "include/ModelSpec.h"

namespace JCore {
template <class T>
class SimQueue : public SimObj {
    uint32_t maxQSize = 4096;
    std::deque<T> writeQ;
    std::deque<T> readQ;
    std::deque<uint32_t> delayQ;
    uint32_t latency = 1;
    bool stall;

public:
    SimQueue() { stall = false; };
    virtual ~SimQueue() {};

    void Build() const {};

    void Work() override
    {
        if (stall) {
            return;
        }
        for (auto it = delayQ.begin(); it != delayQ.end(); ++it) {
            if (*it != 0) {
                --(*it);
            }
        }
        while (!writeQ.empty() && readQ.size() < maxQSize && (delayQ.empty() || delayQ.front() == 0)) {
            readQ.push_back(std::move(writeQ.front()));
            writeQ.pop_front();
            if (!delayQ.empty()) {
                delayQ.pop_front();
            }
        }
    };

    void Xfer() override
    {
    };

    void Reset() override
    {
        writeQ.clear();
        readQ.clear();
        delayQ.clear();
        stall = false;
    }
    void Setlatency(uint32_t d)
    {
        latency = d;
    }
    void ReportStat() override {}

    bool toBeOverflow (uint32_t reserve) { return writeQ.size() + readQ.size() + reserve >= maxQSize; }
    bool getStall() { return stall; }
    void setStall() { stall = true; }
    void unsetStall() { stall = false; }

    SimSys* GetSim() override
    {
        ASSERT(0);
        return nullptr;
    };

    template <typename... Arg>
    void Write(Arg&&... pkt)
    {
        // Let compiler determine whether to invoke `std::move` or not.
        writeQ.emplace_back(std::forward<Arg>(pkt)...);
        if (latency > 1) {
            delayQ.emplace_back(latency);
        }
        ASSERT(writeQ.size() <= maxQSize);
    };

    T Read()
    {
        ASSERT(readQ.size());
        // Let us `std::move`, which is exactly we want here.
        T result = std::move(readQ.front());
        readQ.pop_front();
        return result;
    };

    T Front()
    {
        // NEVER use `std::move` here.
        // Because `readQ.front()` is supposed to remain in `readQ` currently.
        ASSERT(readQ.size());
        return readQ.front();
    };

    void Pop()
    {
        ASSERT(readQ.size());
        readQ.pop_front();
    }

    bool Empty()
    {
        return readQ.empty();
    };

    bool WRBothEmpty()
    {
        return readQ.empty() && writeQ.empty();
    }

    bool Full()
    {
        return Size() + SizeW() >= maxQSize;
    };
    bool Full(uint32_t reserve)
    {
        return Size() + SizeW() + reserve >= maxQSize;
    };

    uint32_t Size()
    {
        return readQ.size();
    };

    uint32_t SizeW()
    {
        return writeQ.size();
    };

    int MaxSize()
    {
        return maxQSize;
    }

    std::deque<T> &GetRawWriteData()
    {
        return writeQ;
    };
    std::deque<T> &GetRawReadData()
    {
        return readQ;
    };
    std::deque<uint32_t> &GetRawDelayCycle()
    {
        return delayQ;
    };

    template <class Function>
    void FlushIf(Function& match)
    {
        for (auto it = writeQ.begin(); it != writeQ.end();) {
            if (match(*it)) {
                if (!delayQ.empty()) {
                    delayQ.erase(delayQ.begin() + (it - writeQ.begin()));
                }
                it = writeQ.erase(it);
            } else {
                ++it;
            }
        }

        for (auto it = readQ.begin(); it != readQ.end();) {
            if (match(*it)) {
                it = readQ.erase(it);
            } else {
                ++it;
            }
        }
    }
    template <class Function>
    bool MatchIf(Function &match)
    {
        for (auto it = writeQ.cbegin(); it != writeQ.cend(); ++it) {
            if (match(*it)) {
                return true;
            }
        }
        for (auto it = readQ.cbegin(); it != readQ.cend(); ++it) {
            if (match(*it)) {
                return true;
            }
        }
        return false;
    }
    template <class Function>
    uint32_t MatchCount(const Function &match)
    {
        uint32_t cnt = 0;
        for (auto it = writeQ.cbegin(); it != writeQ.cend(); ++it) {
            if (match(*it)) {
                ++cnt;
            }
        }
        for (auto it = readQ.cbegin(); it != readQ.cend(); ++it) {
            if (match(*it)) {
                ++cnt;
            }
        }
        return cnt;
    }

    void InitMaxSize(uint32_t size)
    {
        maxQSize = size;
    }
};
} // namespace JCore
#endif