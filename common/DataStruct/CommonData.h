#pragma once

#include <string>
#include <cstdint>
namespace JCore {

using TimeStamp = uint64_t;
using Pid = uint64_t;
using Tid = uint64_t;

struct PTid {
    Pid pid = -1;
    Tid tid = -1;

    PTid() = default;
    PTid(Pid p, Tid t) : pid(p), tid(t) {}

    bool operator==(const PTid &oth) const
    {
        return pid == oth.pid && tid == oth.tid;
    }
    bool operator!=(const PTid &oth) const
    {
        return !(*this == oth);
    }

    bool operator<(const PTid &oth) const
    {
        return pid != oth.pid ? pid < oth.pid : tid < oth.tid;
    }
};

struct EventId {
    PTid ptid;   // by default { -1, -1 }
    int eid = -1;  // event id

    EventId() = default;
    EventId(PTid p, int e) : ptid(p), eid(e) {}

    bool operator==(const EventId &oth) const
    {
        return ptid == oth.ptid && eid == oth.eid;
    }
    bool operator!=(const EventId &oth) const
    {
        return !(*this == oth);
    }

    bool operator<(const EventId &oth) const
    {
        return ptid != oth.ptid ? ptid < oth.ptid : eid < oth.eid;
    }

    bool Valid() const
    {
        return *this != EventId{};
    }
};

const std::string TASK_LABEL = "B:";

struct SwimLogData {
    std::string name = "";
    Pid pid = 0;
    Tid tid = 0;
    int eventId = -1;
    TimeStamp sTime = 0;
    TimeStamp eTime = 0;
    std::string hint = "";
};

} // namespace JCore