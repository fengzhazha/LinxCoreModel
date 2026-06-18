#ifndef PIPE_VIEW_OUT_H
#define PIPE_VIEW_OUT_H

#include <fstream>
#include <memory>
#include <string>
#include <iostream>
#include <iomanip>

#include "BlockPipeViewInfo.h"

namespace JCore {
class SimSys;
class PipeViewOut {
public:
    PipeViewOut() = default;
    ~PipeViewOut() = default;

    std::string version = "0004";
    SimSys *sim = nullptr;
    uint64_t id{0};
    uint64_t blockId{~0UL};
    uint64_t retireId{0};
    uint64_t flushId{0};
    uint64_t cycles{~0ULL};
    std::unique_ptr<std::ofstream> oFile = nullptr;
    void Init(std::string fileName);

    uint64_t currentRetCycle = 0;

    uint64_t Start(uint64_t startCycle, uint64_t simId, uint64_t threadId = 0)
    {
        SetCycles(startCycle);
        Print("I", this->id, simId, threadId);
        return this->id++;
    }

    enum class EndType { RETIRED, FLUSHED };
    // End command output for a specific instruction.
    void End(uint64_t startCycle, uint64_t pipeviewId, uint64_t retId, EndType type)
    {
        SetCycles(startCycle);
        Print("R", pipeviewId, retId, static_cast<int>(type));
    }

    void StartStage(const std::string &stageName, uint64_t pipeviewId,  uint64_t startCycle, int laneId = 0)
    {
        if (startCycle == 0) {
            return;
        }
        SetCycles(startCycle);
        Print("S", pipeviewId, laneId, stageName);
    }

    void EndStage(const std::string &stageName, uint64_t pipeviewId, uint64_t startCycle, int laneId = 0)
    {
        SetCycles(startCycle);
        Print("E", pipeviewId, laneId, stageName);
    }

    void Duration(const std::string &stageName, uint64_t pipeviewId, uint64_t startCycle, uint64_t endCycle)
    {
        if (startCycle == 0) {
            return;
        }
        StartStage(stageName, pipeviewId, startCycle);
        EndStage(stageName, pipeviewId, endCycle != 0 ? endCycle : currentRetCycle);
    }

    void SetCurrentRetireCycle(uint64_t cyc)
    {
        currentRetCycle = cyc;
    }

    // text display mode
    enum class CommentType { LEFT_PANE = 0, MOUSE_OVER };
    // Add arbitrary comment text to an instruction
    void AddComment(uint64_t pipeviewId, CommentType type, const std::string &text)
    {
        Print("L", pipeviewId, static_cast<int>(type), text);
    }

    void Step(int64_t cyc)
    {
        Print("C", cyc);
        this->cycles += cyc;
    }

    void SetCycles(uint64_t cyc)
    {
        if (cyc == this->cycles) { return; }
        int64_t delta = this->cycles > cyc
                            ? static_cast<int64_t>(this->cycles - cyc) : -static_cast<int64_t>(cyc - this->cycles);
        Step(-delta);
    }

    template <typename T>
    void Print(const char *cmd, const T &o)
    {
        *this->oFile << cmd << '\t' << o << '\n';
    }

    template <typename T1, typename T2, typename T3>
    void Print(const char *cmd, const T1 &o1, const T2 &o2, const T3 &o3)
    {
        *this->oFile << cmd << '\t' << o1 << '\t' << o2 << '\t' << o3 << '\n';
    }

    void FlushBuffer()
    {
        this->oFile->flush();
    }

    void PrintBlockStatus(BlockPipeViewPtr &blockInfo);
    void PrintGroup(GroupPipeViewPtr &groupInfo);
    void PrintUinst(InstPipeViewPtr &instInfo);
    void PrintVecLiteUinst(InstPipeViewPtr &instInfo);
};
using PipeViewOutPtr = std::shared_ptr<PipeViewOut>;
}
#endif
