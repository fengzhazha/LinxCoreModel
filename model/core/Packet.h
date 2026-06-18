#pragma once

#include <bitset>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <memory>
#include <iomanip>
#include <string>
#include <atomic>

namespace JCore {

// TODO (sufang): 1. move to a better place; 2. reduce data copy.
struct uint512_t {
    uint64_t bits[8] {0};

    uint512_t() { Reset(); }

    uint512_t(uint64_t *data) {
        for (int i = 0; i < 8; i++) {
            bits[i] = data[i];
        }
    }

    void copyTo(uint64_t *dst)
    {
        for (int i = 0; i < 8; i++) {
            dst[i] = bits[i];
        }
    }

    void copyFrom(uint64_t *src)
    {
        for (int i = 0; i < 8; i++) {
            bits[i] = src[i];
        }
    }

    void extractFrom(uint512_t &d, bool *positionVld) {
        for (int i = 0; i < 8; i++) {
            uint8_t *dst = (uint8_t*)(&bits[i]);
            uint8_t *src = (uint8_t*)(&d.bits[i]);
            for (int j = 0; j < 8; j++) {
                if (positionVld[i*8+j]) {
                    dst[j] = src[j];
                }
            }
        }
    }

    char* toStr()
    {
        static char str[256];
        std::ostringstream oss;
        for (int i = 7; i > 0; i--) {
            oss << "0x" << std::hex << std::setfill('0') << std::setw(16) << bits[i] << "_";
        }
        oss << "0x" << std::hex << std::setfill('0') << std::setw(16) << bits[0];
        std::string temp = oss.str();
        std::strcpy(str, temp.c_str());
        return str;
    }

    void Reset() {
        for (int i = 0; i < 8; i++) {
            bits[i] = 0;
        }
    }
};

struct uint2048_t {
    uint64_t bits[32] {0};

    uint2048_t() { Reset(); }

    uint2048_t(uint64_t *data)
    {
        for (int i = 0; i < 32; i++) {
            bits[i] = data[i];
        }
    }

    void copyTo(uint64_t *dst)
    {
        for (int i = 0; i < 32; i++) {
            dst[i] = bits[i];
        }
    }

    void copyFrom(uint64_t *src)
    {
        for (int i = 0; i < 32; i++) {
            bits[i] = src[i];
        }
    }

    void extractFrom(uint2048_t &d, bool *positionVld)
    {
        for (int i = 0; i < 32; i++) {
            uint8_t *dst = (uint8_t*)(&bits[i]);
            uint8_t *src = (uint8_t*)(&d.bits[i]);
            for (int j = 0; j < 8; j++) {
                if (positionVld[i*8+j]) {
                    dst[j] = src[j];
                }
            }
        }
    }

    void extractFrom(uint8_t *data, bool *positionVld)
    {
        for (int i = 0; i < 32; i++) {
            uint8_t *dst = (uint8_t*)(&bits[i]);
            uint8_t *src = data;
            for (int j = 0; j < 8; j++) {
                if (positionVld[i*8+j]) {
                    dst[j] = src[j];
                }
            }
        }
    }

    char* toStr()
    {
        static char str[1024];
        std::ostringstream oss;
        for (int i = 31; i > 0; i--) {
            oss << "0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(16) << bits[i] << "_";
        }
        oss << "0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(16) << bits[0];
        std::string temp = oss.str();
        std::strcpy(str, temp.c_str());
        return str;
    }

    void Reset()
    {
        for (int i = 0; i < 32; i++) {
            bits[i] = 0;
        }
    }
};

struct ReqData {
    uint512_t data;
    bool positionVld[64];

    void Reset()
    {
        data.Reset();
        memset(positionVld, false, sizeof(positionVld));
    }

    void zero() {
        data.Reset();
        memset(positionVld, true, sizeof(positionVld));
    }

    void merge(ReqData &d) {
        uint8_t *dst = (uint8_t*)(data.bits);
        uint8_t *src = (uint8_t*)(d.data.bits);
        for (uint32_t i = 0; i < 64; ++i) {
            if (d.positionVld[i]) {
                dst[i] = src[i];
                positionVld[i] = true;
            }
        }
    }

    void mergePosition(ReqData &d) {
        for (uint32_t i = 0; i < 64; ++i) {
            if (d.positionVld[i]) {
                positionVld[i] = true;
            }
        }
    }

    void insertCacheData(uint512_t &d) {
        data = d;
        memset(positionVld, true, sizeof(positionVld));
    }

    void insertValidCacheData(uint512_t &d, bool *p) {
        data = d;
        for (uint32_t i = 0; i < 64; ++i) {
            positionVld[i] = p[i];
        }
    }
};

struct ReqData256 {
    uint2048_t data;
    bool positionVld[256];

    void Reset()
    {
        data.Reset();
        memset(positionVld, false, sizeof(positionVld));
    }

    void zero()
    {
        data.Reset();
        memset(positionVld, true, sizeof(positionVld));
    }

    void merge(ReqData256 &d)
    {
        uint8_t *dst = (uint8_t*)(data.bits);
        uint8_t *src = (uint8_t*)(d.data.bits);
        for (uint32_t i = 0; i < 256; ++i) {
            if (d.positionVld[i]) {
                dst[i] = src[i];
                positionVld[i] = true;
            }
        }
    }

    void mergePosition(ReqData256 &d)
    {
        for (uint32_t i = 0; i < 256; ++i) {
            if (d.positionVld[i]) {
                positionVld[i] = true;
            }
        }
    }

    void insertCacheData(uint2048_t &d)
    {
        data = d;
        memset(positionVld, true, sizeof(positionVld));
    }

    void insertValidCacheData(uint2048_t &d, bool *p)
    {
        data = d;
        for (uint32_t i = 0; i < 256; ++i) {
            positionVld[i] = p[i];
        }
    }
};

/**
 * A generic abstraction for packets among BCTRL, PE array, LSU array and L2 cache.
 * TODO (all): review interface definition adn remove attributes that are specialized!
*/
class Packet {
    using PtrPacket = std::shared_ptr<Packet>;
private:
    enum CmdAttr {
        IsDirty,
        IsExcl,
        IsDup,
        IsInst,
        IsPref,
        NUM_CMD_ATTR
    };
    std::bitset<NUM_CMD_ATTR> attrs;

    static std::atomic<uint32_t> packet_id_counter;
    uint32_t packet_id;
    static PtrPacket createPkt();

public:
    Packet();
    static uint32_t getNextPacketId();
    uint32_t getId() const;
    enum class Requestor {
        BCtrl=0, PE, LSU, L2, NIL
    };

    enum MemCmd {
        ReadReq,
        ReadResp,
        WriteReq,
        WriteResp,
        Writeback,
        InvalidateReq,
        InvalidateResp,
        GetShareReq,
        GetShareResp,
        UpgradeReq,
        UpgradeResp,
        Prefetch,
        NUM_MEM_CMDS
    };

	static const std::string CmdName[]; // RED FLAG: extend in ONE place

    MemCmd cmd;
    uint64_t addr = 0;
    uint32_t tid = 0;
    uint32_t size = 0;
    uint32_t pindex = 0;
    uint512_t data;
    uint2048_t mtc_data;
    bool valid[256];
    uint64_t tpc = 0;   // debug
    bool transferReq = false;
    bool vCoreReq = false;
    uint32_t reqId = 0;
    uint32_t coreId = 0;
    uint32_t peId = 0;
    uint32_t stid = -1U;

    /**
     * src + id is used for distinguishing different requestors (e.g. BCC, PE0, LSU1).
     * user_type can be used by requestors internally.
     * TODO (sufang): src + id is not yet supported by L2, but L2 should not have any knowledge of how many IFUs are there in PE arrays.
     * TODO (sufang): user_type may cause a lot of unscoped enumerations, is this good or not?
     */
    Requestor src = Requestor::NIL;
    uint32_t id = 0;
    uint32_t user_type = 0;

    // Debug
    uint64_t l1_out_cycle = 0;
    uint64_t l2_inputQ_cycle = 0;
    uint64_t l2_recv_cycle = 0;
    uint64_t l2_hit_missfilter_cycle = 0;
    uint64_t l2_missfilter_wakeup_cycle = 0;
    uint64_t l2_pick_cycle = 0;
    uint64_t l2_issue_cycle = 0;
    uint64_t l2_wait_snp = 0;
    uint64_t l2_snp_wakeup = 0;
    uint64_t l2MissCycle = 0;
    uint64_t memRntCycle = 0;
    uint64_t l2RntCycle = 0;
    bool     l2_pref_late = false;
    bool     l2_pref_useful = false;
    void     getPktCycle(PtrPacket pkt);
    void     printPktCycle();

    bool     l2_miss = false;
    bool     l2_miss_due_pref = false;
    bool     demand = false;

    static PtrPacket createPkt(MemCmd cmd, uint32_t tid);
    static PtrPacket CreateRWPkt(bool write, uint32_t tid);
    static PtrPacket createRWResp(bool write, bool excl, uint32_t tid);
    static PtrPacket createInvReq(uint32_t tid)   { return createPkt(InvalidateReq, tid); }
    static PtrPacket createInvResp(uint32_t tid)  { return createPkt(InvalidateResp, tid); }
    static PtrPacket createGetSReq(uint32_t tid)  { return createPkt(GetShareReq, tid); }
    static PtrPacket createGetSResp(uint32_t tid) { return createPkt(GetShareResp, tid); }
    static PtrPacket createWBPkt(bool dirty);
    static PtrPacket createUpgrade(uint32_t tid) { return createPkt(UpgradeReq, tid); }
    static PtrPacket createUpgradeResp(uint32_t tid) { return createPkt(UpgradeResp, tid); }
    static PtrPacket createPrefPkt() { return createPkt(Prefetch, -1); }

    bool isRead(void) const      { return (cmd == ReadReq || cmd == ReadResp); }
    bool isWrite(void) const     { return (cmd == WriteReq || cmd == WriteResp); };
    bool isExcl(void) const      { return attrs.test(IsExcl); };
    bool isInv(void) const       { return (cmd == InvalidateReq || cmd == InvalidateResp); }
    bool isGetS(void) const      { return (cmd == GetShareReq || cmd == GetShareResp); }
    bool isUpgrade(void) const   { return (cmd == UpgradeReq || cmd == UpgradeResp); }
    bool isReq(void) const       { return (cmd == ReadReq || cmd == WriteReq || cmd == InvalidateReq || cmd == GetShareReq || cmd == UpgradeReq); }
    bool isResp(void) const      { return (cmd == ReadResp || cmd == WriteResp || cmd == InvalidateResp || cmd == GetShareResp || cmd == UpgradeResp); }
    bool isWriteBack(void) const { return (cmd == Writeback);  }
    bool isDirty(void) const     { return attrs.test(IsDirty); }
    bool isShare(void) const     { return !attrs.test(IsExcl); }
    bool isDup(void) const       { return attrs.test(IsDup);   }
    bool isInst(void) const      { return attrs.test(IsInst);  }
    bool isPref(void) const      { return cmd == Prefetch; }
    bool hasPref(void) const     { return attrs.test(IsPref);  }

    void setExcl(void)  { attrs.set(IsExcl);  }
    void setDirty(void) { attrs.set(IsDirty); }
    void setDup(void)   { attrs.set(IsDup);   }
    void setInst(void)  { attrs.set(IsInst);  }
    void setPref(void)  { attrs.set(IsPref);  }

	friend std::ostream& operator<<(std::ostream& out, Packet pkt);
};

using PtrPacket = std::shared_ptr<Packet>;

} // namespace JCore
