#ifndef ASSERT_ERROR_H
#define ASSERT_ERROR_H

#pragma once

#include <cstddef>
#include <exception>
#include <sstream>
#include <string>
#include <memory>
#include <cstring>
#include <cassert>

#include "lazy.h"

namespace JCore {
using Backtrace = std::shared_ptr<LazyValue<std::string>>;

Backtrace GetBacktrace(size_t skipFrames, size_t maxFrames);

struct ErrorMessage {
public:
    std::string Message() { return ss.str(); }

    template <typename T>
    ErrorMessage &operator<<(const T &value) {
        ss << value;
        return *this;
    }

    // support std::endl etc.
    ErrorMessage &operator<<(std::ostream &(*manipulator)(std::ostream &)) {
        ss << manipulator;
        return *this;
    }

    std::stringstream ss;
};

class Error : public std::exception {
public:
    Error(const char *func, const char *file, size_t line, const std::string &msg, Backtrace backtrace)
        : func_(func), file_(file), line_(line), msg_(msg), backtrace_(backtrace) {}

    Error(const char *func, const char *file, size_t line, Backtrace backtrace)
        : func_(func), file_(file), line_(line), backtrace_(backtrace) {}

    const char *what() const noexcept override;

    int operator=(ErrorMessage &msg) {
        msg_ = msg.Message();
        /* avoid nested throw */
        if (std::uncaught_exceptions() == 0) {
            throw *this;
        }
        return 0;
    }

private:
    const char *func_;
    const char *file_;
    size_t line_;
    std::string msg_;
    std::string umsg_;
    Backtrace backtrace_;
    mutable LazyShared<std::string> what_;
};

class AssertInfo {
public:
    [[noreturn]] int operator=(ErrorMessage &msg) {
        (void)fprintf(stderr, "%s\n", msg.Message().c_str());
        abort();
    }
};

#ifndef __DEVICE__
#define ASSERT(cond)                                                                                                           \
   (cond) ? 0 : JCore::Error(__func__, __FILE__, __LINE__, JCore::GetBacktrace(0, /* 64 is maxFrames */ 64)) = \
        JCore::ErrorMessage() << "ASSERTION FAILED: " #cond << "\n"

#define TILEFWK_ERROR()                                                                                            \
    JCore::Error(__func__, __FILE__, __LINE__, JCore::GetBacktrace(0, /* 64 is maxFrames */ 64)) = \
        JCore::ErrorMessage()
#else

#define ASSERT(cond)                             \
    (cond) ? 0 : AssertInfo() = JCore::ErrorMessage() \
        << "ASSERTION FAILED: " #cond " file " << __FILE__ << ":" << __LINE__ << "\n"
#endif
} // namespace JCore
#endif