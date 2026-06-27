#include "bctrl/bfu/bfu_logger.h"

#include "../configs/bfu_config.h"
#include "bctrl/bfu/bfu.h"

namespace JCore {
namespace NS_CORE {
    BFULogger::BFULogger() {};
    
    BFULogger::~BFULogger() {
        if (enable) fclose(logfile);
    };

    void BFULogger::Build() {
        // enable = cfg->debug_enable;
        // if (enable) logfile = fopen(cfg->debug_log.c_str(), "w");
    }

    void BFULogger::setEnable() {
        // Fixme: refactor warmup related code
        enable = cfg->log_file_enable;
        if (enable) logfile = fopen(cfg->debug_log.c_str(), "w");
    }

    void BFULogger::printBaseInfo(char const* module, BFUStage stage) {
        fprintf(logfile, "c%lu | %s | %s | ", bfu->GetSim()->getCycles(), module, stage2Str(stage));
    }

    void BFULogger::enableAnyWay() {
        enable = true;
        logfile = fopen(cfg->debug_log.c_str(), "w");
    }

    char const* BFULogger::stage2Str(BFUStage s) {
        switch(s) {
            case BFUStage::F0 : return "F0";
            case BFUStage::F1 : return "F1";
            case BFUStage::F2 : return "F2";
            case BFUStage::F3 : return "F3";
            case BFUStage::F4 : return "F4";
            case BFUStage::NIL : return "XX";
            default: assert(0); return "??";
        }
    };
}

} // namespace JCore
