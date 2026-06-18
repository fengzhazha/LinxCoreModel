#pragma once

#include "pe/ifu/common/ifu_common.h"
#include "pe/ifu/common/ifu_utils.h"

namespace JCore {


namespace PE_IFU {

class PEIFU;

class IFUComponent {
public:
    virtual void RegisterComponent(PEIFU* ifu) final;
    virtual void Build() {};
    virtual SimSys* GetSim() { return sim; }
protected:
    uint32_t                            id;
    SimSys*                             sim;
    PEIFU*                              ifu;
    IFUConfig*                          configs;
    IFUStats*                           stats;
    IFUUtils*                           utils;
    std::string                         logLabel = "";
};
}

} // namespace JCore
