#pragma once

#include <cstdint>
#include <vector>

#include "bctrl/bfu/common/core_type.h"

namespace JCore {


namespace NS_CORE {

class ReplacementPolicy {
protected:
    set_t nset;
    way_t nway;
public:
    ReplacementPolicy(set_t nset, way_t nway) : nset(nset), nway(nway) {};
    virtual ~ReplacementPolicy() {};
    virtual void Reset(set_t set_idx, way_t way_idx) = 0;
    virtual void invalidate(set_t set_idx, way_t way_idx) = 0;
    virtual void touch(set_t set_idx, way_t way_idx) = 0;
    virtual void insert(set_t set_idx, way_t way_idx) = 0;
    virtual way_t getVictim(set_t set_idx) = 0;
    virtual bool isValid(set_t set_idx, way_t way_idx) = 0;
};

class NMRU : public ReplacementPolicy {
    struct RepData {
        bool valid = false;
        bool used = false;
    };
    std::vector<std::vector<RepData>> repdata;
public:
    NMRU(set_t nset, way_t nway);
    void Reset(set_t set_idx, way_t way_idx) override;
    void invalidate(set_t set_idx, way_t way_idx) override;
    void insert(set_t set_idx, way_t way_idx) override;
    void touch(set_t set_idx, way_t way_idx) override;
    way_t getVictim(set_t set_idx) override;
    bool isValid(set_t set_idx, way_t way_idx) override;
};

}

} // namespace JCore
