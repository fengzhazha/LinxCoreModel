#pragma once

#include <stdlib.h>
#include <string>

#include "bctrl/bfu/bfu_common.h"
#include "bctrl/bfu/bfu_component.h"

namespace JCore {
namespace NS_CORE {

class BFULogger : public BFUComponent {
    bool enable {false};
    FILE* logfile;

public:
    BFULogger();
    ~BFULogger();

    void Build();
    void setEnable(); // at warmup finish
    void enableAnyWay();

    void printBaseInfo(char const* module, BFUStage stage);

    template<typename... ARGS>
    void debug(char const* module, BFUStage stage, ARGS... args) {
        if (enable) {
            printBaseInfo(module, stage);
            fprintf(logfile, args...);
            fflush(logfile);
        }
    }


    char const* stage2Str(BFUStage s);

};

}

} // namespace JCore
