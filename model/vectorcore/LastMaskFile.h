#ifndef BLOCKISA_MODEL_LAST_MASK_FILE_H
#define BLOCKISA_MODEL_LAST_MASK_FILE_H
#include <vector>
#include <map>

#include "vectorcore/VectorCommon.h"

namespace JCore {

class VectorCore;
class MemoryCore;
struct LastMaskFileState {
    bool vld = false;
    ROBID bid;
    uint32_t lastGroupNum = 0;
};

struct MaskReleaseInfo {
    ROBID bid;
    uint32_t stid = -1U;
};

class LastMaskFile {
public:
    LastMaskFile();
    ~LastMaskFile();

    INPUT SimQueue<MaskReleaseInfo>*    m_GROB2MaskFile;

    void                                Work();
    void                                Build(uint32_t depth, uint32_t fetchWidth);

    void                                Alloc();
    void                                Alloc(const ROBID &bid, uint32_t lastGroupNum, uint32_t stid);
    void                                Release();
    void                                Release(MaskReleaseInfo &info);
    bool                                CheckStall(uint32_t reserved, uint32_t stid);
    uint32_t                            GetLastGroupNum(const ROBID &bid, uint32_t stid);

    SimSys*                             GetSim();
    void                                SetSim(SimSys *sim);

    void                                SetFlush(FlushBus &bus);
private:
    SimSys*                             sim;
    VectorCore*                         top;
    MemoryCore*                         mtop;
    std::vector<std::map<ROBID, LastMaskFileState>>  m_entries;
    uint32_t                            depth = 0;
    std::vector<uint32_t>               size;
    uint32_t                            fetchWidth = 0;
};


} // namespace JCore

#endif  // BLOCKISA_MODEL_LAST_MASK_FILE_H
