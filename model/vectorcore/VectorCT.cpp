#include "vectorcore/VectorCT.h"

namespace JCore {

void VectorCT::InitCTArray()
{
    for (uint32_t i = 0; i < m_instNumPerCycle; i++) {
        m_ctArray[i] = new VectorCTInstance();
    }
}

} // namespace JCore