#ifndef ASSERT_LAZY_H
#define ASSERT_LAZY_H

#pragma once

#include <atomic>

namespace JCore {

template <class T>
class LazyValue {
public:
    virtual ~LazyValue() = default;
    virtual const T &Get() const = 0;
};

template <typename T>
class LazyShared {
public:
    LazyShared() = default;

    LazyShared(const LazyShared &rhs) {
        if (auto val = rhs.value_.load(std::memory_order_acquire)) {
            value_ = new T(*val);
        }
    }

    LazyShared(LazyShared &&rhs) {
        if (auto val = rhs.value_.exchange(nullptr, std::memory_order_acq_rel)) {
            value_ = new T(*val);
        }
    }

    virtual ~LazyShared() { Reset(); }

    template <typename Factory>
    T &Ensure(const Factory &factory) {
        if (auto val = value_.load(std::memory_order_acquire)) {
            return *val;
        }
        auto val = new T(factory());
        T *old = nullptr;
        if (!value_.compare_exchange_strong(old, val, std::memory_order_release, std::memory_order_acquire)) {
            delete val;
            return *old;
        }
        return *val;
    }

    void Reset() {
        if (auto old = value_.load(std::memory_order_relaxed)) {
            value_.store(nullptr, std::memory_order_relaxed);
            delete old;
        }
    }

    void swap(LazyShared &other) noexcept {
        value_.swap(other.value_);
    }

    LazyShared &operator=(const LazyShared &rhs) {
        LazyShared temp(rhs);  // 拷贝构造
        swap(temp);           // 交换
        return *this;
    }

    LazyShared &operator=(LazyShared &&rhs) noexcept {
        LazyShared temp(std::move(rhs));  // 移动构造
        swap(temp);                      // 交换
        return *this;
    }

private:
    std::atomic<T *> value_{nullptr};
};
} // namespace JCore
#endif