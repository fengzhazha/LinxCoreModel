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

struct LSUConfig;

enum CacheState {
    CS_INV,     // invalid
    CS_SHARE,   // read only, clean
    CS_EXCL,    // read & write, clean
    CS_MOD      // read & write, dirty
};

struct L1Entry {
    CacheState state;
    uint64_t addr;
    uint512_t data;
    L1Entry() : state(CS_INV), addr(-1U) {}
    uint32_t pref_type;
    bool prefetch;
    bool demand;
};

class L1Cache : public SimObj {
protected:
    L1Cache();
    virtual ~L1Cache();

public:
    L1Entry                     **cache;
    uint32_t                    nset;
    uint32_t                    nway;
    uint32_t                    way_size; 

    /* find entry in cache */
    L1Entry*                    find(uint64_t addr);
    /* calculate set index of virtual address */
    virtual uint32_t            calcSetIdx(uint64_t va);
    /* physical address = virtual address */
    bool                        tagMatch(uint64_t a0, uint64_t a1);
    /* translate virtual address to physical address */
    uint64_t                    transToPa(uint64_t va);
    /* update */
    void                        updateLRU(uint32_t set, uint32_t way);
    /* set status invalid */
    L1Entry                     invd(uint64_t addr);
    /* get share entry */
    L1Entry                     getS(uint64_t addr);
    /* update entry from share to excl */
    bool                        upgrade(uint64_t addr);

    virtual void                Reset();
    virtual void                Work();
    virtual void                Xfer();
    virtual SimSys              *GetSim();
    virtual void                Build();
};

} // namespace JCore