#include "DebugLog.h"

#include <cstdarg>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

namespace JCore {

uint64_t DebugLog::debugId = UINT64_MAX;

DebugLog::DebugLog()
    : fileOpened_(false) {
}

bool DebugLog::Init(const std::string& filename)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (fileOpened_) {
        Close();
    }
    file_.open(filename, std::ios::out | std::ios::trunc);
    fileOpened_ = file_.is_open();
    if (!fileOpened_) {
        std::cerr << "[ERROR] Failed to open log file: " << filename << std::endl;
        return false;
    }
    constexpr int lengthTypeA = 80;
    constexpr int lengthTypeB = 10;
    constexpr int lengthTypeC = 13;
    file_ << "Simulation Debug Log\n";
    file_ << std::string(lengthTypeA, '=') << "\n";
    file_ << std::left
          << std::setw(lengthTypeB) << "Cycle"
          << std::setw(lengthTypeC) << "[Unit.Stage]"
          << std::setw(lengthTypeB) << "BlockId"
          << std::setw(lengthTypeB) << "TileOpId"
          << "Event\n";
    file_ << std::string(lengthTypeA, '-') << "\n";
    file_.flush();
    debugModeOn = true;
    debugId = 0;
    return true;
}

void DebugLog::Close()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (fileOpened_) {
        constexpr int lengthTypeA = 80;
        file_ << std::string(lengthTypeA, '=') << "\n";
        file_ << "Log closed.\n";
        file_.close();
        fileOpened_ = false;
    }
}

void DebugLog::SetLogLevelThreshold(uint8_t threshold)
{
    if (threshold >= LogLevel::CRITICAL && threshold <= LogLevel::MINIMAL) {
        std::lock_guard<std::mutex> lock(mutex_);
        logLevelThreshold_ = threshold;
    } else {
        std::cerr << "[ERROR] DebugLog: Invalid log level threshold: " << threshold
                  << ". Must be 1-5." << std::endl;
    }
}

uint8_t DebugLog::GetLogLevelThreshold() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return logLevelThreshold_;
}

uint64_t DebugLog::AllocDebugId()
{
    if (!debugModeOn) {
        return UINT64_MAX;
    }

    ASSERT(debugId != UINT64_MAX && "DebugLogger is not initialized");
    if (debugId == UINT64_MAX - 1) {
        debugId = 0;
    }
    return debugId++;
}
} // namespace JCore
