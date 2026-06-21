#pragma once

#include <deque>

#include "bctrl/bfu/bfu_component.h"
#include "bctrl/bfu/common/MachineInst.h"

namespace JCore {


class BFU;

namespace NS_CORE {

class BRQ : public BFUComponent {
    PtrFB redirect_fb = nullptr;
    PtrFB nuke_fb = nullptr;
    std::vector<PtrMachineInst> stashH = {};
    pos_t nuke_pos = 0;

    std::vector<std::deque<PtrFB>> brq;

    // mispredictions
    struct MispInfo {
        addr_t pc;
        uint32_t cnt;
        uint32_t misp;
        std::string t;
    };
    std::map<addr_t, MispInfo> mispreds; // pc -> mispInfo

public:
    BRQ() {};
    ~BRQ() {};

    BFU *top = NULL;
    bool push(PtrFB const& fb);
    bool ResolveHeader(PtrMachineInst const& h);
    void ReportFlush(seq_t fbid, seq_t fbid_local, uint64_t pc, uint32_t stid);
    PtrFB CommitHeader(PtrMachineInst const& h);
    void resolveFetchBundle(PtrFB const& fb);
    bool checkResolve(PtrFB const& fb);
    void UpdateInorder();
    void CommitFetchBundle();
    PtrFB tryRedirect();
    bool HasRedirect();
    PtrFB nukeflush(PtrMachineInst const& h);
    void flush(PtrFB const& fb);
    bool isStall(size_t reserve, uint32_t stid);
    void dumpMispreds();
    void resetMispreds();
    void Build() override;

    void updateResolveInfo(PtrFB const& fb);
    bool HasFBByHdr(PtrMachineInst const& h) const;
    bool IsOlderThanFront(PtrMachineInst const& h) const;
    bool IsOlderThanFront(seq_t fbid, seq_t fbid_local, uint32_t stid) const;
    std::deque<PtrFB>::iterator getFBByHdr(PtrMachineInst const& h);
    std::deque<PtrFB>::iterator getFBByFbid(seq_t fbid, seq_t fbid_local, uint32_t stid);
    std::deque<PtrFB>::iterator getGlobalFBByFbid(seq_t fbid, uint32_t stid);
    std::deque<PtrFB>::iterator getBodyEndFBByHdr(PtrMachineInst const& h);
    std::deque<PtrFB>::iterator getFBStartByHid(seq_t hid, uint32_t stid);
    PtrMachineInst getBStartMhdrByFbid(seq_t hid, uint32_t stid);
};

}

} // namespace JCore
