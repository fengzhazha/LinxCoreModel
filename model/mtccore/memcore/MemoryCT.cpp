#include "mtccore/memcore/MemoryCT.h"

namespace JCore {

void MemoryCT::InitCTArray()
{
    for (uint32_t i = 0; i < m_instNumPerCycle; i++) {
        m_ctArray[i] = new MemoryCTInstance();
    }
}

} // namespace JCore