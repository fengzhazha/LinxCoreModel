#include "TraceLogger.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <map>

#include "SimSys.h"

namespace JCore {

using namespace std;

namespace TraceLog {
    Json Event::ToJson() const
    {
        Json root;

        root["name"] = name;
        if (!category.empty()) {
            root["cat"] = category;
        }
        root["ph"] = phase;
        if (!bp.empty()) {
            root["bp"] = bp;
        }
        if (id > 0) {
            root["id"] = id;
        }
        root["ts"] = timestamp;
        root["pid"] = pid;
        root["tid"] = tid;

        if (!hint.empty()) {
            Json args;
            args["accelerator-hint"] = hint;

            root["args"] = std::move(args);
        }

        return root;
    }

    Json Event::ToFlowStartJson(int flowId, TimeStamp dstTime) const
    {
        Json root;
        root["cat"] = "machine-view-last-dep";
        root["id"] = flowId;
        root["name"] = "machine-view-last-dep";
        root["ph"] = "s";
        root["pid"] = pid;
        root["tid"] = tid;
        if (timestamp > dstTime) {
            root["ts"] = dstTime - 1;
        } else {
            root["ts"] = timestamp - 1;
        }
        return root;
    }

    Json Event::ToFlowEndJson(int flowId) const
    {
        Json root;
        root["bp"] = "e";
        root["cat"] = "machine-view-last-dep";
        root["id"] = flowId;
        root["name"] = "machine-view-last-dep";
        root["ph"] = "f";
        root["pid"] = pid;
        root["tid"] = tid;
        root["ts"] = timestamp;
        return root;
    }

    int Event::ExtraHintInfo(std::string &key)
    {
        size_t pos = hint.find(key);
        pos += key.length();
        std::string numStr = hint.substr(pos);
        size_t spacePos = numStr.find(' ');
        numStr = numStr.substr(0, spacePos);
        std::stringstream ss(numStr);
        int value;
        ss >> value;
        return value;
    }

    Json CounterEvent::ToJson() const
    {
        Json jSize;
        jSize["size"] = size;

        Json root;
        root["args"] = jSize;
        root["name"] = category;
        root["pid"] = pid;
        root["tid"] = tid;
        root["ph"] = "C";
        root["ts"] = timestamp;

        return root;
    }

    Json Thread::ToJson() const
    {
        Json root;
        Json args;
        args["name"] = name;

        root["args"] = std::move(args);
        root["cat"] = "__metadata";
        root["name"] = "thread_name";
        root["ph"] = "M";
        root["pid"] = pid;
        root["tid"] = tid;

        return root;
    }

    Json Process::ToJson() const
    {
        Json root;

        Json args;
        args["name"] = name;

        root["args"] = std::move(args);
        root["cat"] = "__metadata";
        root["name"] = "process_name";
        root["ph"] = "M";
        root["pid"] = pid;

        return root;
    }

    Json Process::ToSortIndexJson(int sortIndex) const
    {
        Json root;

        Json args;
        args["sort_index"] = sortIndex;

        root["args"] = std::move(args);
        root["cat"] = "__metadata";
        root["name"] = "process_sort_index";
        root["ph"] = "M";
        root["pid"] = pid;

        return root;
    }

    void Duration::OutputContextSwitchTrace(std::ofstream &os, std::map<Pid, Process> &mProcesses,
                                            std::map<PTid, Thread> &mThreads, const uint64_t sysClockTicks)
    {
        string processInfo = to_string(start.tid);
        std::string subgraphName = "SUBGRAPH";
        auto it = mThreads.find(PTid{start.pid, start.tid});
        if (it != mThreads.end()) {
            subgraphName = it->second.name;
        }
        std::ostringstream cpuId;
        int formatOffset = 3;
        cpuId << setfill('0') << setw(formatOffset) << mProcesses[start.tid].coreIdx;
        string cpuIdx = cpuId.str();
        string cpuInfo = "(   0) [" + cpuIdx + "] ....";

        string cpuMgrTile = "cpumgr-idle-" + to_string(start.pid);
        string cpuSwitchTitle = subgraphName + "-" + to_string(start.tid);

        string cpuWakeupInfo =
            ": sched_wakeup: comm=" + subgraphName + " pid=" + processInfo + " prio=20 target_cpu=" + cpuIdx;
        string cpuSwitchInfo1 =
            ": sched_switch: prev_comm=cpumgr-idle-0 prev_pid=0 prev_prio=-2 prev_state=R+ ==> next_comm=" + subgraphName +
            " next_pid=" + processInfo + " next_prio=5";
        string cpuSwitchInfo2 = ": sched_switch: prev_comm=" + subgraphName + " prev_pid=" + processInfo +
                                  " prev_prio=5 prev_state= ==> next_comm=cpumgr-idle-0 next_pid=0 next_prio=-2";
        string cpuIdleInfo = ": cpu_idle: state=0 cpu_id=" + cpuIdx; 

        std::ostringstream cyc1;
        int precision = 6;
        cyc1 << std::fixed << std::setprecision(precision) << (double(start.timestamp) / sysClockTicks);
        string sCycle = cyc1.str();

        std::ostringstream cyc2;
        cyc2 << std::fixed << std::setprecision(precision) << (double(end.timestamp) / sysClockTicks);
        string eCycle = cyc2.str();

        os << setw(width) << left << cpuMgrTile << setw(width) << right << cpuInfo << setw(width2) << right << sCycle;
        os << cpuWakeupInfo << std::endl;
        os << setw(width) << left << cpuSwitchTitle << setw(width) << right << cpuInfo << setw(width2) << right << sCycle;
        os << cpuSwitchInfo1 << std::endl;

        os << setw(width) << left << cpuMgrTile << setw(width) << right << cpuInfo << setw(width2) << right << eCycle;
        os << cpuSwitchInfo2 << std::endl;
        os << setw(width) << left << cpuMgrTile << setw(width) << right << cpuInfo << setw(width2) << right << eCycle;
        os << cpuIdleInfo << std::endl;
    }

    void Duration::OutputBeginEndTrace(std::ofstream &os, std::map<Pid, Process> &mProcesses,
                                       std::map<PTid, Thread> &mThreads, const uint64_t sysClockTicks)
    {
        string processInfo = to_string(start.pid);
        std::ostringstream cpuId;
        int formatOffset = 3;
        cpuId << setfill('0') << setw(formatOffset) << mProcesses[start.pid].coreIdx;
        string cpuIdx = cpuId.str();
        string cpuInfo = "(" + processInfo + ") [" + cpuIdx + "] ....";

        string pipeName = "";
        auto it = mThreads.find(PTid{start.pid, start.tid});
        if (it != mThreads.end()) {
            pipeName = it->second.name;
        }

        string title = pipeName + "-" + to_string(start.tid);
        string beginInfo = ": tracing_mark_write: B|" + processInfo + "|" + start.name + " " + start.hint;
        string endInfo = ": tracing_mark_write: E|" + processInfo + "|";

        std::ostringstream cyc1;
        int precision = 6;
        cyc1 << std::fixed << std::setprecision(precision) << (double(start.timestamp) / sysClockTicks);
        string sCycle = cyc1.str();

        std::ostringstream cyc2;
        cyc2 << std::fixed << std::setprecision(precision) << (double(end.timestamp) / sysClockTicks);
        string eCycle = cyc2.str();

        os << setw(width) << left << title << setw(width) << right << cpuInfo << setw(width2) << right << sCycle;
        os << beginInfo << std::endl;
        os << setw(width) << left << title << setw(width) << right << cpuInfo << setw(width2) << right << eCycle;
        os << endInfo << std::endl;
    }

    Json Duration::ToJson() const
    {
        Json root;

        if (!start.category.empty()) {
            root["cat"] = start.category;
        }
        root["ph"] = "X";
        root["id"] = start.id;
        root["name"] = start.name;
        root["pid"] = start.pid;
        root["tid"] = start.tid;
        root["ts"] = start.timestamp;
        root["dur"] = end.timestamp - start.timestamp;
        if (!start.hint.empty()) {
            Json args;
            args["block-hint"] = start.hint;
            root["args"] = std::move(args);
        }
        return root;
    }

    bool TraceLogger::EnableLogger(uint64_t cycle)
    {
        if (!config.enableLimitOutput) {
            return true;
        }
        if (cycle < config.cycleLowerThreshold) {
            return false;
        }
        if (cycle > config.cycleUpperThreshold) {
            return false;
        }
        return true;
    }

    void TraceLogger::SetProcessName(std::string name, Pid pid, size_t coreIdx)
    {
        if (mProcesses.find(pid) == mProcesses.end()) {
            mProcesses.emplace(pid, Process(name, pid, coreIdx));
        } else {
            mProcesses[pid] = Process(name, pid, coreIdx);
        }
    }

    void TraceLogger::SetThreadName(std::string name, Pid pid, Tid tid)
    {
        mThreads[PTid{pid, tid}] = Thread(name, pid, tid);
    }

    Event TraceLogger::AddEventBegin(std::string name, Pid pid, Tid tid,
                                     TimeStamp timestamp, std::string hint)
    {
        Event beginEvent;
        if (!EnableLogger(timestamp)) {
            return beginEvent;
        }
        mEventIdPtr++;
        beginEvent.name = name;
        beginEvent.id = mEventIdPtr;
        beginEvent.category = "event";
        beginEvent.phase = "B";
        beginEvent.bp = "";
        beginEvent.timestamp = timestamp;
        beginEvent.pid = pid;
        beginEvent.tid = tid;
        beginEvent.hint = hint;
        mEvents.push_back(beginEvent);
        m_eventStacks[PTid{pid, tid}].push(beginEvent);
        return beginEvent;
    }

    Event TraceLogger::AddEventEnd(Pid pid, Tid tid, TimeStamp timestamp)
    {
        Event endEvent;
        if (!EnableLogger(timestamp)) {
            return endEvent;
        }
        if (m_eventStacks.empty()) {
            return endEvent;
        }
        auto beginEvent = m_eventStacks[PTid{pid, tid}].top();
        m_eventStacks[PTid{pid, tid}].pop();
        endEvent.name = beginEvent.name;
        endEvent.id = beginEvent.id;
        endEvent.category = "event";
        endEvent.phase = "E";
        endEvent.bp = "";
        endEvent.timestamp = timestamp;
        endEvent.pid = pid;
        endEvent.tid = tid;
        endEvent.hint = beginEvent.hint;
        mEvents.push_back(endEvent);
        mDurations.emplace(beginEvent.id, Duration{beginEvent, endEvent});
        return endEvent;
    }

    void TraceLogger::AddDuration(const SwimLogData &data)
    {
        if (!EnableLogger(data.eTime)) {
            return;
        }
        Event beginEvent;
        beginEvent.name = data.name;
        beginEvent.id = data.eventId;
        beginEvent.category = "event";
        beginEvent.phase = "B";
        beginEvent.bp = "";
        beginEvent.timestamp = data.sTime;
        beginEvent.pid = data.pid;
        beginEvent.tid = data.tid;
        beginEvent.hint = data.hint;
        Event endEvent;
        endEvent.name = data.name;
        endEvent.id = data.eventId;
        endEvent.category = "event";
        endEvent.phase = "E";
        endEvent.bp = "";
        endEvent.timestamp = data.eTime;
        endEvent.pid = data.pid;
        endEvent.tid = data.tid;
        endEvent.hint = beginEvent.hint;
        mEvents.push_back(beginEvent);
        mEvents.push_back(endEvent);
        mDurations.emplace(beginEvent.id, Duration{beginEvent, endEvent});
    }

    int TraceLogger::GetEventId()
    {
        if (!EnableLogger(sim->getCycles())) {
            return -1;
        }
        return mEventIdPtr++;
    }

    void TraceLogger::AddFlow(uint64_t srcEventId, uint64_t dstEventId)
    {
        if (!EnableLogger(sim->getCycles())) {
            return;
        }
        EventId srcId;
        EventId dstId;
        srcId.eid = srcEventId;
        dstId.eid = dstEventId;
        AddFlow("flow", srcId, dstId);
    }

    void TraceLogger::AddFlow(std::string name, EventId from, EventId to)
    {
        mFlows.push_back(Flow{name, from, to});
    }

    void TraceLogger::AddCounterEvent(Pid pid, Tid tid, CounterType type, int tileSize)
    {
        if (!EnableLogger(sim->getCycles())) {
            return;
        }
        mCounterEventIdPtr++;
        CounterEvent sizeCount;
        sizeCount.id = mCounterEventIdPtr;
        sizeCount.category = "count";
        sizeCount.phase = "C";
        sizeCount.type = type;
        sizeCount.size = tileSize;
        sizeCount.timestamp = TimeStamp(sim->getCycles());
        sizeCount.pid = pid;
        sizeCount.tid = tid;
        mCounters.emplace_back(sizeCount);
        mCounts[PTid{pid, tid}].emplace_back(sizeCount);
    }

    Json TraceLogger::ToJson()
    {
        Json root;
        auto traceEvents = Json::array();

        int processSortIndex = 0;
        for (auto &entry : mProcesses) {
            traceEvents.emplace_back(entry.second.ToJson());
            traceEvents.emplace_back(entry.second.ToSortIndexJson(processSortIndex++));
        }
        for (auto &entry : mThreads) {
            if (entry.first.tid > coreTid && entry.first.tid < reversedTidNum ) {
                continue;
            }
            traceEvents.emplace_back(entry.second.ToJson());
        }

        for (auto &duration : mDurations) {
            traceEvents.emplace_back(duration.second.ToJson());
        }

        int flowIndex = 0;
        for (auto &flow : mFlows) {
            auto& fStart = mDurations[flow.from.eid].end;
            auto& fEnd = mDurations[flow.to.eid].start;
            traceEvents.emplace_back(fStart.ToFlowStartJson(flowIndex, fEnd.timestamp));
            traceEvents.emplace_back(fEnd.ToFlowEndJson(flowIndex));
            flowIndex++;
        }

        GetCounters();
        for (auto &qsize : eachMachineQueueSize) {
            auto qJson = QSizeToJson(qsize.second);
            traceEvents.insert(traceEvents.end() , qJson.begin(), qJson.end());
        }

        root["traceEvents"] = std::move(traceEvents);
        return root;
    }

    void TraceLogger::ToTrace(std::ofstream &os)
    {
        // Context switch
        for (auto &duration : mDurations) {
            if (duration.second.start.pid == topMachineViewPid) {
                duration.second.OutputContextSwitchTrace(os, mProcesses, mThreads, config.sysClockTicks);
            } else {
                duration.second.OutputBeginEndTrace(os, mProcesses, mThreads, config.sysClockTicks);
            }
        }
    }

    void TraceLogger::LogTaskInfo(Event &start, Event &end)
    {
        int coreId = mProcesses[start.pid].coreIdx;

        // Get TaskID
        std::string taskKey = "TaskId:";
        int taskId = start.ExtraHintInfo(taskKey);

        std::string subgraphKey = "pSgId:";
        int pSgId = start.ExtraHintInfo(subgraphKey);

        uint64_t sTime = start.timestamp;
        uint64_t eTime = end.timestamp;

        // 创建任务JSON对象（不再包含coreType）
        Json taskJson;
        taskJson["taskId"] = taskId;
        taskJson["execStart"] = sTime;
        taskJson["execEnd"] = eTime;
        taskJson["subGraphId"] = pSgId;

        mCoreInfoLogs[coreId].taskLogs.push_back(taskJson);
    }

    void TraceLogger::LogPipeInfo(Event &start, Event &end)
    {
        std::string name = "";
        int coreId = mProcesses[start.pid].coreIdx;
        uint64_t sTime = start.timestamp;
        uint64_t eTime = end.timestamp;
        Json pipeJson;
        pipeJson["tileOp"] = start.name;
        pipeJson["execStart"] = sTime;
        pipeJson["execEnd"] = eTime;

        mCoreInfoLogs[coreId].pipeLogs[name].emplace_back(pipeJson);
    }

    void TraceLogger::LogCoreInfo(Duration &duration)
    {
        auto &start = duration.start;
        auto &end = duration.end;
        size_t initPos = start.name.find("Init");
        if (initPos != std::string::npos) {
            return;
        }
        if (start.tid == 1) {
            LogTaskInfo(start, end);
        } else {
            LogPipeInfo(start, end);
        }
    }

    void TraceLogger::ToFilterTrace(std::ofstream &os, std::map<int, std::pair<std::string, std::vector<Json>>> &coreTasks)
    {
        for (auto it : mProcesses) {
            auto machineType = GetMachineType(it.first);
            coreTasks[it.second.coreIdx] = {it.second.name, {}};
            mCoreInfoLogs[it.second.coreIdx] =
                CoreInfoLog(it.second.coreIdx, MachineName(machineType));
        }

        for (auto &duration : mDurations) {
            LogCoreInfo(duration.second);
            auto &start = duration.second.start;
            auto &end = duration.second.end;
            size_t initPos = start.name.find("Init");
            if (initPos != std::string::npos) {
                continue;
            }
            // Get CoreID
            int coreId = mProcesses[start.pid].coreIdx;

            // Get TaskID
            std::string taskKey = "TaskId:";
            int taskId = start.ExtraHintInfo(taskKey);

            std::string subgraphKey = "pSgId:";
            int pSgId = start.ExtraHintInfo(subgraphKey);

            uint64_t sTime = start.timestamp;
            uint64_t eTime = end.timestamp;

            // 创建任务JSON对象（不再包含coreType）
            Json taskJson;
            taskJson["taskId"] = taskId;
            taskJson["execStart"] = sTime;
            taskJson["execEnd"] = eTime;
            taskJson["subGraphId"] = pSgId;

            coreTasks[coreId].second.push_back(taskJson);
        }

        // 输出分组结果
        Json printJson;

        for (auto &it : coreTasks) {
            Json coreJson;
            coreJson["blockIdx"] = it.first;
            coreJson["coreType"] = it.second.first;  // 核心类型提升到分组层级
            coreJson["tasks"] = it.second.second;     // 任务列表
            printJson.push_back(coreJson);
        }

        os << printJson.dump(1) << std::endl;
    }

    void TraceLogger::GetCounters()
    {
        std::map<uint64_t, int> cyclesQSize;
        for (auto &countEvent : mCounts) {
            // Add filter logic and sample logic
            cyclesQSize.clear();
            int qSize = 0;
            for (auto &event : countEvent.second) {
                if (event.type == CounterType::QUEUE_PUSH) {
                    qSize += event.size;
                    cyclesQSize[event.timestamp] = qSize;
                } else {
                    qSize -= event.size;
                    cyclesQSize[event.timestamp] = qSize;
                }
            }
            for (auto& it : cyclesQSize) {
                CounterEvent sizeCount;
                sizeCount.id = mQSizeIdPtr++,
                sizeCount.category = mThreads[countEvent.first].name,
                sizeCount.phase = "C",
                sizeCount.type = CounterType::QUEUE_PUSH,
                sizeCount.size = it.second,
                sizeCount.timestamp = TimeStamp(it.first),
                sizeCount.pid = countEvent.first.pid,
                sizeCount.tid = countEvent.first.tid,
                eachMachineQueueSize[countEvent.first].emplace_back(sizeCount);
            }
        }
    }

    Json TraceLogger::QSizeToJson(std::vector<CounterEvent> &counterQ)
    {
        Json root = Json::array();
        for (auto &count : counterQ) {
            root.emplace_back(count.ToJson());
        }
        return root;
    }
}

} // namespace JCore