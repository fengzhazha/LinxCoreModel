#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

#include "core/Bus.h"

namespace JCore {

namespace VCore {

enum class State {
    DEFAULT = 0,
    RUNNING,
    IDLE
};

struct DataLM2CT {
    uint64_t pad;
};

class VectorCommon {
public:
    virtual bool IsIdle()
    {
        return m_state == State::IDLE;
    }
    virtual bool IsRunning()
    {
        return m_state == State::RUNNING;
    }
    void SetIdle()
    {
        m_state = State::IDLE;
    }
    void SetRunning()
    {
        m_state = State::RUNNING;
    }

private:
    VCore::State m_state;
};

struct GBufferAllocReq {
    bool                        vld = false;
    BlockCommandPtr             blockCmd = nullptr;
    /* The id of the group within the block, with each block starting from 0 */
    uint32_t                    logicalGID = 0;
    ShapeLoopInfo               shapelpinfo;
    uint32_t                    dispBufferCnt = 0;
};
} // namespace VCore
} // namespace JCore
