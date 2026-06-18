#ifndef   VECTOR_CT_H
#define   VECTOR_CT_H
#include "vectorcore/VectorCommon.h"

namespace JCore {

class VectorCTInstance : public VCore::VectorCommon {
public:
    SimQueue<VCore::DataLM2CT*>      buffer;
    bool IsIdle() override
    {
        return buffer.Empty();
    }
};

class VectorCT : public VCore::VectorCommon {
public:
    void Work();
    void Xfer();
    void Build();
    void Reset();

    void InitCTArray();

    bool IsIdle() override
    {
        bool res = std::all_of(m_ctArray.begin(), m_ctArray.end(),
            [](VectorCTInstance* item) {return item->IsIdle();});
        return res;
    }

private:
    const uint32_t m_instNumPerCycle = 4;
    std::vector<VectorCTInstance*>  m_ctArray;
};

} // namespace JCore

#endif