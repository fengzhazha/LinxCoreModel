#pragma once

#include <deque>
#include <map>
#include <set>
#include <vector>

#include "../emulator/Memory.h"
#include "core/Bus.h"
#include "core/Packet.h"
#include "ModelSpec.h"
#include "SimSys.h"

namespace JCore {

struct mLSUConfig;

enum MTC_CacheState {
    MTC_CS_INV,     // invalid
    MTC_CS_VALID
};

struct MTC_L1Entry {
    MTC_CacheState state;
    uint64_t addr;
    uint2048_t data;
    MTC_L1Entry() : state(MTC_CS_INV), addr(-1U) {}
    uint32_t pref_type;
    bool prefetch;
    bool demand;
};

class MTC_L1Cache : public SimObj {
public:
    MTC_L1Entry                     **cache;
    uint32_t                    nset;
    uint32_t                    nway;
    uint32_t                    way_size;

    /* find entry in cache */
    MTC_L1Entry*                    find(uint64_t addr);
    /* calculate set index of virtual address */
    virtual uint32_t            calcSetIdx(uint64_t va);
    /* physical address = virtual address */
    bool                        tagMatch(uint64_t a0, uint64_t a1);
    /* translate virtual address to physical address */
    uint64_t                    transToPa(uint64_t va);
    /* update */
    void                        updateLRU(uint32_t set, uint32_t way);
    /* set status invalid */
    MTC_L1Entry                     invd(uint64_t addr);
    /* get share entry */
    MTC_L1Entry                     getS(uint64_t addr);
    /* update entry from share to excl */
    bool                        upgrade(uint64_t addr);

    virtual void                Reset();
    virtual void                Work();
    virtual void                Xfer();
    virtual SimSys              *GetSim();
    virtual void                Build();
protected:
    MTC_L1Cache();
    virtual ~MTC_L1Cache();
};

} // namespace JCore