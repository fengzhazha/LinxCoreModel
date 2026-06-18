#pragma once

#include <cstdint>
#include <memory>

#include "bctrl/bfu/common/core_type.h"

namespace JCore {


namespace NS_CORE {

class MemReq {    
public:    
    enum Type {
        HEADER, BTLB, READ, WRITE
    };
    enum Source {
        BFU, MMU
    };
private:
    static seq_t sid_global;
    seq_t sid;
    Type type;
    // Source src;
    addr_t pa_cl;
    // uint8_t size; // in byte
public: 
    MemReq() = delete;
    MemReq(Type t, addr_t pa_cl) : sid(sid_global++), type(t), pa_cl(pa_cl) {};

    bool isHeader() { return type == HEADER; }
    bool isBTLB() { return type == BTLB; }
    bool isWrite() { return type == WRITE; }
    addr_t getPA() { return pa_cl; }
    seq_t getSID() { return sid; }
};

typedef std::shared_ptr<MemReq> PtrMemReq;

}

} // namespace JCore
