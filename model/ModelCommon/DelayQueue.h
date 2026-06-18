#pragma once

#ifndef DELAY_QUEUE_BUS_H
#define DELAY_QUEUE_BUS_H

#include <cstdint>
#include <vector>

#include "ModelSpec.h"

namespace JCore {
constexpr uint32_t DEFAULT_RANDOM_LATENCY = 300;
template <class T>
class ThdSimQ {
public:
    ThdSimQ()
    {}

public:
    void Work()
    {
        for (auto &entry : m_entity) {
            entry.Work();
        }
    }
    void Build(uint32_t thds)
    {
        m_entity.resize(thds);
        for (auto &entry : m_entity) {
            entry.Build();
        }
    }
    void Build(uint32_t thds, uint32_t depth)
    {
        m_entity.resize(thds);
        for (auto &entry : m_entity) {
            entry.Build();
            entry.InitMaxSize(depth);
        }
    }
    void Write(T &entry, uint32_t tid)
    {
        m_entity[tid].Write(entry);
    }

    uint32_t Size(uint32_t tid)
    {
        return m_entity[tid].Size();
    }
    uint32_t SizeW(uint32_t tid)
    {
        return m_entity[tid].SizeW();
    }
    void Reset(uint32_t tid)
    {
        m_entity[tid].Reset();
    }
    void Reset()
    {
        for (auto &e : m_entity) {
            e.Reset();
        }
    }
    void ResetAll()
    {
        for (uint32_t i = 0; i < m_entity.size(); ++i) {
            m_entity[i].Reset();
        }
    }
    template <class Function>
    void FlushIf(Function &cond, uint32_t tid)
    {
        m_entity[tid].FlushIf(cond);
    }
    template <class Function>
    void FlushIf(Function &cond)
    {
        for (auto &e : m_entity) {
            e.FlushIf(cond);
        }
    }
    bool GetStall(uint32_t tid)
    {
        return m_entity[tid].getStall();
    }
    void SetStall(uint32_t tid)
    {
        m_entity[tid].setStall();
    }
    void UnsetStall(uint32_t tid)
    {
        m_entity[tid].unsetStall();
    }
    void UnsetStall()
    {
        for (auto &e : m_entity) {
            e.unsetStall();
        }
    }
    bool Empty(uint32_t tid)
    {
        return m_entity[tid].Empty();
    }
    T Front(uint32_t tid)
    {
        return m_entity[tid].Front();
    }
    SimQueue<T> &GetRawEntity(uint32_t tid)
    {
        return m_entity[tid];
    };
    T Read(uint32_t tid)
    {
        return m_entity[tid].Read();
    }
    bool Full(uint32_t tid)
    {
        return m_entity[tid].Full();
    }
    bool Full(uint32_t tid, uint32_t reserve)
    {
        return m_entity[tid].Full(reserve);
    }

private:
    std::vector<SimQueue<T>> m_entity;
};

template <typename T>
class ThdRingQ {
public:
    ThdRingQ()
    {}
    ThdRingQ(uint32_t thds, uint32_t size)
    {
        m_entity.resize(thds);
        for (uint32_t i = 0; i < m_entity.size(); ++i) {
            m_entity[i] = RingQueue<T>(size);
        }
    }
public:
    void Write(T req, uint32_t tid)
    {
        ASSERT(tid < m_entity.size());
        m_entity[tid].Write(req);
    }
    void UpdateFront(T &req, uint32_t tid)
    {
        ASSERT(tid < m_entity.size());
        m_entity[tid].UpdateFront(req);
    }
    T Read(uint32_t tid)
    {
        return m_entity[tid].Read();
    }
    void Reset(uint32_t tid)
    {
        m_entity[tid].Reset();
    }
    void Reset()
    {
        for (uint32_t tid = 0; tid < m_entity.size(); ++tid) {
            m_entity[tid].Reset();
        }
    }
    void ResetAll()
    {
        for (uint32_t tid = 0; tid < m_entity.size(); ++tid) {
            m_entity[tid].Reset();
        }
    }
    void Build(uint32_t thds, uint32_t depth)
    {
        m_entity.resize(thds);
        for (uint32_t tid = 0; tid < m_entity.size(); ++tid) {
            m_entity[tid] = RingQueue<T>(depth);
        }
    }
    template <class Function>
    void FlushIf(Function &cond, uint32_t tid)
    {
        m_entity[tid].FlushIf(cond);
    }
    template <class Function>
    void FlushIf(Function &cond)
    {
        for (uint32_t tid = 0; tid < m_entity.size(); ++tid) {
            m_entity[tid].FlushIf(cond);
        }
    }
    void FlushIf(uint32_t tid)
    {
        m_entity[tid].Reset();
    }
    uint32_t Size(uint32_t tid)
    {
        return m_entity[tid].Size();
    }
    T Front(uint32_t tid)
    {
        return m_entity[tid].Front();
    }
    T &At(uint32_t idx, uint32_t tid)
    {
        return m_entity[tid][idx];
    }
    bool Empty(uint32_t tid)
    {
        return m_entity[tid].Empty();
    }
    bool Full(uint32_t tid)
    {
        return m_entity[tid].Full();
    }
    bool Full()
    {
        for (uint32_t tid = 0; tid < m_entity.size(); ++tid) {
            if (m_entity[tid].Full()) {
                return true;
            }
        }
        return false;
    }
    std::vector<RingQueue<T>> &GetData()
    {
        return m_entity;
    }

private:
    std::vector<RingQueue<T>> m_entity;
};

template <class T>
class Queue : public SimObj {
public:
    SimSys *GetSim() override { ASSERT(0); return nullptr; }
    virtual void write (T const& val) = 0;
    virtual T read() = 0;
    virtual bool empty() = 0;
};

template <class T>
class DelayQueue : public Queue<T> {
public:
    DelayQueue(uint32_t d = 1) : delay(d) {}

    void Build() { Reset(); }
    void Reset() override { data_queue.clear(); time_queue.clear(); }
    void Work() override {}
    void Xfer() override {
        if (data_queue.empty()) {
            tick = 0;
        } else {
            tick++;
        }
    }
    void ReportStat() override {}
    void write(T const& pkt) override {
        data_queue.push_back(pkt);
        time_queue.push_back(tick + delay);
    }
    T read() override {
        ASSERT(!empty());
        T front = data_queue.front();
        data_queue.pop_front();
        time_queue.pop_front();
        return front;
    }

    bool empty() override { return time_queue.empty() || tick < time_queue.front(); }

    template <class Function>
    void FlushIf(Function& match) {
        auto it_d = data_queue.begin();
        auto it_t = time_queue.begin();
        for (; it_d != data_queue.end();) {
            if (match(*it_d)) {
                it_d = data_queue.erase(it_d);
                it_t = time_queue.erase(it_t);
            } else {
                it_d++;
                it_t++;
            }
        }
    }
private:
    uint64_t tick = 0;
    uint32_t delay = 0;
    std::deque<T> data_queue;
    std::deque<uint64_t> time_queue;
};

template <class T>
class DelayRandomQueue : public Queue<T> {
private:
    uint64_t tick = 0;
    std::multimap <uint64_t, T> timeDataMap;

public:
    DelayRandomQueue() : tick(0) {}

    void Build() { Reset(); }
    void Reset() override {timeDataMap.clear();}
    void Work() override {}
    void Xfer() override {
        if (timeDataMap.empty()) {
            tick = 0;
        } else {
            tick++;
        }
    }
    void ReportStat() override {}
    void write(T const& pkt, uint64_t latency) {
        uint64_t time = tick + latency;
        timeDataMap.insert(std::pair<uint64_t, T>(time, pkt));
    }
    void write(T const& pkt) override {
        write(pkt, DEFAULT_RANDOM_LATENCY);
    }

    // just read one data
    T read() override {
        ASSERT(!empty());
        auto it = timeDataMap.begin();
        T data = it->second;
        timeDataMap.erase(it);
        return data;
    }

    bool empty() override {
        if (!timeDataMap.empty()) {
            auto it = timeDataMap.begin();
            if (it->first <= tick) {
                return false;
            }
        }
        return true;
    }

    template <class Function>
    void FlushIf(Function& match) {
        auto it = timeDataMap.begin();
        for (; it != timeDataMap.end();) {
            if (match(it->first)) {
                it = timeDataMap.erase(it);
            } else {
                ++it;
            }
        }
    }
};
}
#endif