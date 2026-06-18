#include <cstdio>
#include <cstdlib>

#include "SoftCore.h"
#include "utils/ParseArgs.h"

using namespace std;
using namespace JCore;

static int ParseCommandLine(int argc, char **argv, struct EmulatorCommandLineArgs& args)
{
    JCore::ParseArgs argsPrase = JCore::ParseArgs("gfrun");
    argsPrase.RegisterParam("-f", args.filename, "Executable to load (binary or ELF)");

    // 1: basic info; 3: rob debug info;
    argsPrase.RegisterParam("-t", args.traceMode, "1: Basic log information; 2: Tile Detail Datas");
    argsPrase.RegisterParam("-m", args.blockNum, "Number of blocks executed when simulation terminates");
    argsPrase.RegisterParam("--stop_pc", args.stopPC, "Stop at this Block PC Address.");
    argsPrase.RegisterParam("--stop_cycle", args.stopCycles, "Stop execution after this cycle.");
    argsPrase.RegisterParam("--debug_spc", args.debugLogStartPC, "Start Debug logging from this PC address");
    argsPrase.RegisterParam("--init_sp_with_env", args.initStackWithEnv, "");
    argsPrase.RegisterParam("--log_file", args.logname, "Log file name, set empty to print to standard output");
    argsPrase.RegisterParam("-s", args.cfgs, "Override default configs");
    argsPrase.Parse(argc, argv);

    // Print the execution command to the screen
    for (int i = 0; i < argc; ++i) {
        std::cout << argv[i] << " ";
    }
    std::cout << std::endl;

    return 0;
}

int main(int argc, char *argv[])
{
    struct EmulatorCommandLineArgs args;

    int errCode = ParseCommandLine(argc, argv, args);
    if (errCode != 0) {
        return errCode;
    }

    std::shared_ptr<SoftCore> core = std::make_shared<SoftCore>();
    if (args.traceMode != 0) {
        core->EnableTrace();
        if (args.traceMode == 1) {
            LoggerManager::GetManager().ResetLevel(LoggerLevel::DEBUG);
        } else {
            LoggerManager::GetManager().ResetLevel(LoggerLevel::DETAIL);
        }
    }
    if (!args.logname.empty()) {
        LoggerManager::GetManager().FileLoggerRegister(args.logname, false);
        LoggerManager::GetManager().stdEnabled = false;
    }
    core->inputArgs = args;
    core->Init();

    // Load binary paramters
    int targetArgc = 0;
    auto targetArgv = argv;
    uint64_t* regs = static_cast<uint64_t*>(malloc(sizeof(uint64_t) * static_cast<uint64_t>(GPR::GPR_COUNT)));
    std::map<uint64_t, uint64_t> sysregs;
    std::map<uint64_t, uint64_t> sysregs_null;
    uint64_t spTop = 0;
    uint64_t startAddr = 0;
    core->gfrunning = true;
    JsonElement* jsonElement = new JsonElement;
    jsonElement->regs = regs;
    jsonElement->filename = args.filename.c_str();
    const std::vector<std::string> targetArgs(targetArgv, targetArgv + targetArgc);
    // Merged parse of slice instruction segment and data segment
    core->memory->gfrunning = args.initStackWithEnv;
    struct addr addrRet = core->memory->LoadElf(*jsonElement, sysregs, spTop, targetArgc, targetArgs);
    startAddr = addrRet.start_addr;
    if (addrRet.sp_addr == static_cast<uint64_t>(-1)) {
        core->ckpt_file = true;
    }
    if (args.blockNum != INST_MAX_PC) {
        core->memory->execBlockCnt = std::min(core->memory->execBlockCnt, args.blockNum);
    }
    args.blockNum = core->memory->execBlockCnt;
    for (uint64_t thread = 0; thread < core->config.multiThreadNum; thread++) {
        core->ResetPC(startAddr, thread);
        if (addrRet.sp_addr == static_cast<uint64_t>(-1)) {
            for (int i = 0; i < static_cast<int>(GPR::GPR_COUNT); i++) {
                core->SetGPR(i, regs[i], thread);
            }
            for (auto& it: sysregs) {
                core->SetSystemReg(it.first, it.second, thread);
            }
        } else {
            core->SetGPR(static_cast<int>(GPR::GPR_SP), addrRet.sp_addr, thread);
        }
    }

    LOG_ERROR << "Starting from 0x" << std::hex << startAddr;

    uint64_t currentPc = startAddr;
    while (currentPc != args.stopPC && (!core->coreSimEnd)) {
        // Turn trace on
        if (args.debugLogStartPC == currentPc) {
            core->EnableTrace();
        }
        core->Step();
        currentPc = core->GetPC(0); // 此结束条件在多线程下有问题
        if (core->GetTotalBlockNum() >= args.blockNum) {
            LOG_ERROR << "number of executions reaches the upper bound\nexecution ends";
            break;
        }
    }

    core->ReportStat();
    if ((addrRet.sp_addr == INST_MAX_PC) && (args.blockNum == INST_MAX_PC)) {
        core->CheckCkptRlt(args.filename.c_str());
        LOG_ERROR << "Checkpoint checks suaccelssfully! R2 = " << std::hex
            << core->GetGPR(static_cast<uint64_t>(GPR::GPR_A0), 0);
    } else {
        LOG_ERROR << "Suaccelss to Reach the End of Benchmark! R2 = " << std::hex
            << core->GetGPR(static_cast<uint64_t>(GPR::GPR_A0), 0);
    }

    delete jsonElement;
    free(regs);
}