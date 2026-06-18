#pragma once

#include <cstdint>
#include <cstddef>

#include <map>
#include <stack>
#include <tuple>
#include <mutex>
#include <string>
#include <vector>
#include <deque>
#include <iomanip>
#include <unordered_map>

#include "Common.h"
#include "../../configs/trace_config.h"
#include "json.hpp"

namespace JCore {

using Json = nlohmann::json;

class SimSys;
namespace TraceLog {
    struct Event {
        std::string name;
        int id = -1;  // invalid -1
        std::string category;
        std::string phase;
        std::string bp;
        TimeStamp timestamp;
        Pid pid;
        Tid tid;
        std::string hint;

        Json ToJson() const;
        Json ToFlowStartJson(int flowId, TimeStamp dstTime) const;
        Json ToFlowEndJson(int flowId) const;

        EventId GetEventID() const
        {
            return EventId{PTid{pid, tid}, id};
        }

        int ExtraHintInfo(std::string &key);
    };

    enum class CounterType {
        QUEUE_PUSH = 0,
        QUEUE_POP = 1,
        CACHE_HIT = 2,
        CACHE_MISS = 3,
        COUNT_SIZE = 4,
    };

    struct CounterEvent {
        int id = -1;  // invalid -1
        std::string category;
        std::string phase;
        CounterType type;
        int size = 0;
        TimeStamp timestamp;
        Pid pid;
        Tid tid;

        Json ToJson() const;

        EventId GetEventID() const
        {
            return EventId{PTid{pid, tid}, id};
        }
    };

    struct Thread {
        std::string name;
        Pid pid;
        Tid tid;

        Thread() =default;
        Thread(std::string &n, Pid p, Tid t) : name(n), pid(p), tid(t) {}
        Json ToJson() const;
    };

    struct Process {
        std::string name;
        Pid pid;
        size_t coreIdx = 0;
        Process() =default;
        Process(std::string &n, Pid p, size_t core) : name(n), pid(p), coreIdx(core) {}

        Json ToJson() const;
        Json ToSortIndexJson(int sortIndex) const;
    };

    struct Duration {
        Event start;
        Event end;
        int width = 30;
        int width2 = 20;

        Duration() = default;
        Duration(Event s, Event e) : start(s), end(e) {}
        void OutputContextSwitchTrace(std::ofstream &os, std::map<Pid, Process> &mProcesses,
                                      std::map<PTid, Thread> &mThreads, const uint64_t sysClockTicks);
        void OutputBeginEndTrace(std::ofstream &os, std::map<Pid, Process> &mProcesses,
                                 std::map<PTid, Thread> &mThreads, const uint64_t sysClockTicks);
        Json ToJson() const;
    };

    struct Flow {
        std::string name;
        EventId from;
        EventId to;
    };

    struct CoreInfoLog {
        uint64_t idx;
        std::string type;

        std::vector<Json> taskLogs;
        std::map<std::string, std::vector<Json>> pipeLogs;
        explicit CoreInfoLog() {};
        explicit CoreInfoLog(uint64_t id, std::string t) : idx(id), type(t) {
            taskLogs.clear();
            pipeLogs.clear();
        }
    };

    class TraceLogger {
    public:
        TraceConfig config;
        int width = 30;
        int width2 = 20;
        int precision = 6;
        std::map<Pid, Process> mProcesses;
        std::map<PTid, Thread> mThreads;
        std::vector<Event> mEvents;
        std::vector<Flow> mFlows;
        std::vector<CounterEvent> mCounters;
        std::map<PTid, std::vector<CounterEvent>> mCounts;
        std::map<int, Duration> mDurations;
        std::map<int, int> mTaskIDToDurationIndex;
        std::map<PTid, std::stack<Event>> m_eventStacks;

        std::map<PTid, std::vector<CounterEvent>> eachMachineQueueSize;
        std::vector<CounterEvent> totalBufferSize;
        std::map<PTid, std::vector<CounterEvent>> eachBufferWorkload;
        std::vector<CounterEvent> totalDeviceMachineQueueSize;
        std::vector<CounterEvent> totalAicpuMachineQueueSize;
        std::vector<CounterEvent> totalCoreMachineQueueSize;
        std::vector<CounterEvent> totalPipeMachineQueueSize;
        std::vector<CounterEvent> totalDeviceMachinePushpopNum;
        std::vector<CounterEvent> totalAicpuMachinePushpopNum;
        std::vector<CounterEvent> totalCoreMachinePushpopNum;
        std::vector<CounterEvent> totalPipeMachinePushpopNum;
        std::map<PTid, std::vector<CounterEvent>> eachPipeMachineWorkload;
        std::vector<CounterEvent> totalPipeMachineWorkload;
        std::vector<CounterEvent> functionCacheTotal;
        std::vector<CounterEvent> functionCacheHit;
        std::vector<CounterEvent> functionCacheMiss;

        std::map<uint64_t, CoreInfoLog> mCoreInfoLogs;
        bool coreInfoBuild = false;

        size_t topMachineViewPid = 1000;
        size_t reversedTidNum = 100; // For Queue Start tid
        size_t coreTid = 1; // For MachineView tid
        SimSys *sim = nullptr;
        int mEventIdPtr = 0;
        int mCounterEventIdPtr = 0;
        int mQSizeIdPtr = 0;
        bool processDeviceReadyQueue = false;
        explicit TraceLogger()
        {
            mProcesses.clear();
        }
        // Logger Trace
        bool EnableLogger(uint64_t cycle);
        void SetProcessName(std::string name, Pid pid, size_t coreIdx);
        void SetThreadName(std::string name, Pid pid, Tid tid);
        Event AddEventBegin(std::string name, Pid pid, Tid tid, TimeStamp timestamp, std::string hint = "");
        Event AddEventEnd(Pid pid, Tid tid, TimeStamp timestamp);
        void AddDuration(const SwimLogData &data);
        int GetEventId();
        void AddFlow(uint64_t srcEventId, uint64_t dstEventId);
        void AddFlow(std::string name, EventId from, EventId to);
        void AddCounterEvent(Pid pid, Tid tid, CounterType type, int tileSize);
        void LogTaskInfo(Event &start, Event &end);
        void LogPipeInfo(Event &start, Event &end);
        void LogCoreInfo(Duration &duration);

        // Output Trace
        Json ToJson();
        void ToTrace(std::ofstream &os);
        void ToFilterTrace(std::ofstream &os, std::map<int, std::pair<std::string, std::vector<Json>>> &coreTasks);
        void ToPipeTrace(std::ofstream &os);
        void ToCommJson(std::ofstream &os);
        void ToCalendarGlobalJson(std::ofstream &osCalendar,
                                  std::map<int, std::pair<std::string, std::vector<Json>>> coreTasks);

        void GetCounters();
        Json QSizeToJson(std::vector<CounterEvent> &counterQ);
    };
} // namespace TraceLog
} // namespace JCore
