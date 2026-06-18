#pragma once
#include <cassert>
#include <cstdint>

#include "bctrl/bfu/bfu_component.h"

namespace JCore {


namespace NS_CORE {

class BitArray {
    msize_t nsize;
    msize_t ndword;
    uint64_t* buf;
public:
    BitArray(msize_t nsize) : nsize(nsize) {
        ndword = (nsize+63)/64;
        buf = new uint64_t[ndword];
        for(msize_t i = 0; i <ndword; i++) {
            buf[i] = 0;
        }
    }
    ~BitArray() { delete[] buf; }
    
    ghr_t extract(idx_t lsb, idx_t msb) {
        // lsb should be less than the array size
        // len should be no more than 64
        msize_t len = msb - lsb + 1;
        assert(lsb <= msb && lsb < nsize && len <= 64);
       
        ghr_t res;
        int lsb_idx = lsb/64;
        int lsb_ost = lsb%64;
        int msb_idx = (msb/64)%ndword;

        if (lsb_idx == msb_idx) {
            res = buf[lsb_idx]>>lsb_ost;
        } else {
            assert(lsb_idx + 1 == msb_idx || msb_idx == 0);
            res = buf[lsb_idx]>>lsb_ost | buf[msb_idx]<<(64-lsb_ost);
        }
        if (len < 64) { // shifting by the size of integer or a larger size is undefined in C++!
            res &= ((1UL << len) - 1);
        }
        return res;
    }

    void set(idx_t lsb, idx_t msb, uint64_t data) {
        msize_t len = msb - lsb + 1;
        assert(lsb <= msb && lsb < nsize && len <= 64);
        int lsb_idx = lsb/64;
        int lsb_ost = lsb%64;
        int msb_idx = (msb/64)%ndword;
        int msb_ost = msb%64;

        auto make_mask = [](int msb_ost, int lsb_ost) -> uint64_t {
            if (msb_ost > 63) {
                msb_ost = 63;
            }
            if (msb_ost < 0) {
                msb_ost = 0;
            }
            if (lsb_ost > 63) {
                lsb_ost = 63;
            }
            if (lsb_ost < 0) {
                lsb_ost = 0;
            }
            if (msb_ost < lsb_ost) {
                return 0;
            }
            uint64_t high = (msb_ost == 63) ? ~0ULL : ((1ULL << (msb_ost+1)) - 1);
            uint64_t low  = (lsb_ost == 0) ? 0 : ((1ULL << lsb_ost) - 1);
            return high ^ low;
        };
        if (lsb_idx == msb_idx) {
            uint64_t wr_mask = make_mask(msb_ost, lsb_ost);
            buf[lsb_idx] = (buf[lsb_idx] & ~wr_mask) | ((data<<lsb_ost) & wr_mask);
        } else {
            assert(lsb_idx + 1 == msb_idx || msb_idx == 0);
            uint64_t wr_mask_lsb = -1UL << lsb_ost;
            uint64_t wr_mask_msb = (1UL<<(msb_ost+1))-1;
            buf[lsb_idx] = (buf[lsb_idx] & ~wr_mask_lsb) | ((data<<lsb_ost) & wr_mask_lsb);
            buf[msb_idx] = (buf[msb_idx] & ~wr_mask_msb) | ((data>>(64-lsb_ost)) & wr_mask_msb);
        }
    }

};

/* \brief Global History Register, implemented as a circular bit buffer */
class GHRQ : public BFUComponent {
    msize_t ndepth;
    bool ghr_3old = false;
    int bfuGhrNOld = 0;
    std::vector<idx_t> wptr;
    std::shared_ptr<BitArray> q;
    uint32_t id = 0;
    uint32_t ghrHistNbit = 0;
    // TODO: add GHRQ stall mechanism
public:
    GHRQ();
    ~GHRQ();
    
    void Build() override;
    
    void getGHRInfo(PtrFB const& fb, int num_vld_fb);
    void recoverWptr(std::vector<idx_t> wptr);

    void runAt0CyclePred(PtrFB const& fb);
    void runAtMainPredFlush(PtrFB const& fb);
    void runAtCallFlush(PtrFB const& fb);
    void runAtMainPred(PtrFB const& fb);
    void runAtUseLB(PtrFB const& fb);
    void runAtRedirect(PtrFB const& fb);
    void runAtNuke(PtrFB const& fb);
    void SetID(uint32_t i);

private:
    ghr_t reverseBits(ghr_t h, msize_t len);
    ghr_t getGHR(idx_t rptr, msize_t len); // len <= 64
    void appendHist(std::vector<PathHist>& hist);
    void appendHist(uint64_t hist, msize_t len);
    void recentHistPlus1(msize_t len);
    void RptHashStats(addr_t pc, addr_t tgt, ghr_t hist);
};

}

} // namespace JCore
