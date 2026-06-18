#pragma once

#include <cassert>
#include <cstdint>
#include <memory>

#include "bctrl/bfu/bfu_common.h"

namespace JCore {


class BFUConfig;

namespace NS_CORE {

class BFU;

// class BCtrlStats;
class BFULogger;
class BFUUtils;

class BFUComponent {
protected:
    BFU* bfu;
    BFUConfig* cfg;
    BCtrlStats* stats;
    BFULogger* logger;
    BFUUtils* utils;
public:
    ~BFUComponent() {};
    virtual void RegisterComponent(BFU* bfu) final; // implemented in BFU.cpp
    virtual void Build() {};
};

}

} // namespace JCore
