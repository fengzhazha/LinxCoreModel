#ifndef   MEMORY_CT_H
#define   MEMORY_CT_H
#include "mtccore/memcore/MemoryCommon.h"

namespace JCore {

class MemoryCTInstance : public MCore::MemoryCommon {
public:
    SimQueue<MCore::DataLM2CT*>      buffer;
    bool IsIdle() override
    {
        return buffer.Empty();
    }
};

class MemoryCT : public MCore::MemoryCommon {
public:
    void Work();
    void Xfer();
    void Build();
    void Reset();

    void InitCTArray();

    bool IsIdle() override
    {
        bool res = std::all_of(m_ctArray.begin(), m_ctArray.end(),
            [](MemoryCTInstance* item) {return item->IsIdle();});
        return res;
    }

private:
    const uint32_t m_instNumPerCycle = 4;
    std::vector<MemoryCTInstance*>  m_ctArray;
};

} // namespace JCore

#endif