#pragma once
#include "bctrl/bfu/bfu_common.h"

namespace JCore {


namespace NS_CORE {

class BFUUtils {
    static const addr_t fb_align_nbyte = MIN_BUNDLE_SIZE * BFU_ALIGNMENT;
public:
    static inline addr_t NextBlockPC(addr_t a)
    {
        return a + MIN_BUNDLE_SIZE;
    }
    static inline addr_t NextBlockPC(PtrMachineInst const& h)
    {
        return NextBlockPC(h->GetBundlePosPC()) + h->bfuInfo->spInfo->bsize;
    }
    static inline addr_t BsizeFromBS(PtrMachineInst const& h)
    {
        return NextBlockPC(h) - NextBlockPC(h->GetBundlePosPC());
    }
    static inline addr_t PrePC(addr_t a)
    {
        return a - MIN_BUNDLE_SIZE;
    }

    static inline addr_t AlignToFB(addr_t fb_va)
    {
        return fb_va & ~(fb_align_nbyte - 1);
    }
    static inline addr_t AlignToCL(addr_t a)
    {
        return a & ~(BFU_CL_NBYTE - 1);
    }
    static inline addr_t AlignToPG(addr_t a)
    {
        return a & ~(BFU_PG_NBYTE - 1);
    }
    static inline addr_t AlignToNthCL(addr_t a, pos_t n)
    {
        return AlignToCL(a) + n * BFU_CL_NBYTE;
    }
    static inline addr_t AlignToNextFB(addr_t a)
    {
        return AlignToFB(a) + BFU_BANDWIDTH * MIN_BUNDLE_SIZE;
    }

    static inline addr_t CalcPCIdx(addr_t a)
    {
        return a >> HEADER_SIZE_LOG2;
    }
    static inline addr_t CalcPGIdx(addr_t a)
    {
        return a / BFU_PG_NBYTE;
    }
    static inline addr_t CalcCLIdx(addr_t a)
    {
        return a / BFU_CL_NBYTE;
    }
    static inline addr_t CalcFBIdx(addr_t a)
    {
        return a / fb_align_nbyte;
    }
    static inline msize_t CalcCrossCLNum(addr_t a)
    {
        return CalcCLIdx(AlignToFB(a) + (BFU_BANDWIDTH-1)*MIN_BUNDLE_SIZE) - CalcCLIdx(a) + 1;
    }

    // Calc the position of a given address that may not be the fb start address
    static inline pos_t CalcPosInFB(addr_t a, addr_t fb_va)
    {
        return (a-AlignToFB(fb_va)) >> HEADER_SIZE_LOG2;
    }
    // Calc the position of a fb start address
    static inline pos_t CalcPosInFB(addr_t fb_va)
    {
        return CalcPosInFB(fb_va, fb_va);
    }
    // A concated block may cross two MIN_BUNDLE_SIZE slots. The following method returns the pos of the second slot.
    static inline pos_t CalcPosInFB(PtrMachineInst const& h, addr_t fb_va)
    {
        return CalcPosInFB(h->GetBundlePosPC(), fb_va);
    }

    static inline addr_t CalcPC(addr_t fb_va, pos_t pos)
    {
        return AlignToFB(fb_va) + pos * MIN_BUNDLE_SIZE;
    };

    static inline addr_t CalcStaticTarget(PtrMachineInst const& h)
    {
        return h->bcmd == nullptr ? 0 : h->bcmd->barg.target;
    }
    static inline bool IsBendOrBstart(const SPInfoPtr& info)
    {
        return (!info->isInst || info->isLast);
    }
    static inline bool IsStartMHdr(PtrMachineInst const& h, seq_t hid)
    {
        return (h && h->bfuInfo && h->bfuInfo->spInfo && h->bfuInfo->vld
                && !h->bfuInfo->spInfo->isInst && h->bfuInfo->hid == hid);
    }
    static inline bool IsFBRange(addr_t va, const PtrFB& fb)
    {
        return (va >= AlignToFB(fb->va) && va <= AlignToNextFB(fb->va));
    }
    static inline bool IsFBRangeRightBorder(addr_t va, const PtrFB& fb)
    {
        return (va > AlignToFB(fb->va) && va <= AlignToNextFB(fb->va));
    }
    static inline bool IsFBRangeLeftBorder(addr_t va, const PtrFB& fb)
    {
        return (va >= AlignToFB(fb->va) && va < AlignToNextFB(fb->va));
    }
    static inline void SetLast(PtrMachineInst& h)
    {
        ASSERT(h->bfuInfo && h->bfuInfo->spInfo);
        h->bfuInfo->spInfo->isInst = true;
        h->bfuInfo->spInfo->isLast = true;
        h->bfuInfo->spInfo->cut = true;
    }
    static inline void SetBsize(PtrMachineInst& h, addr_t start_pc, addr_t end_pc) {
        ASSERT(h->bfuInfo && h->bfuInfo->spInfo);
        h->bfuInfo->spInfo->bsizeVld = true;
        h->bfuInfo->spInfo->bsize = end_pc > start_pc ? end_pc - start_pc : 0;
    }
    static inline bool MhdrInvalid(PtrMachineInst& h)
    {
        return (h == nullptr || !h->bfuInfo->vld);
    }

    static inline addr_t NextFBPC(addr_t fb_va)
    {
        return AlignToFB(fb_va) + BFU_BANDWIDTH * MIN_BUNDLE_SIZE;
    }
    static inline addr_t VA2PA(addr_t pa_pg, addr_t va)
    {
        return pa_pg | (va & (BFU_PG_NBYTE - 1));
    }

    static inline addr_t LastNBit(addr_t a, msize_t n)
    {
        assert(n < 64);
        return a & ((1ULL<<n) - 1);
    }

    static inline bool IsCall(BrType op)
    {
        return (op == BranchType::BLK_BR_CALL) || (op == BranchType::BLK_BR_ICALL);
    }
    static inline bool IsCallOnly(BrType op)
    {
        return (op == BranchType::BLK_BR_CALL);
    }
    static inline bool IsRet(BrType op)
    {
        return op == BranchType::BLK_BR_RET;
    }
    static inline bool IsCond(BrType op)
    {
        return op == BranchType::BLK_BR_COND;
    }
    static inline bool IsDirect(BrType op)
    {
        return (op == BranchType::BLK_BR_DIRECT) || (op == BranchType::BLK_BR_CALL);
    }
    static inline bool IsDirectOnly(BrType op)
    {
        return (op == BranchType::BLK_BR_DIRECT);
    }
    static inline bool IsIndirect(BrType op)
    {
        return (op == BranchType::BLK_BR_IND) || (op == BranchType::BLK_BR_ICALL);
    }
    static inline bool IsIndirectOnly(BrType op)
    {
        return (op == BranchType::BLK_BR_IND);
    }
    static inline bool IsDirectOrCall(BrType op)
    {
        return (op == BranchType::BLK_BR_DIRECT) || (op == BranchType::BLK_BR_CALL) || (op == BranchType::BLK_BR_ICALL);
    }
    static inline bool IsDirectOrIndirect(BrType op)
    {
        return (op == BranchType::BLK_BR_IND) || (op == BranchType::BLK_BR_DIRECT);
    }
    static inline bool IsBranchWithOffeset(BrType op)
    {
        return (IsDirect(op) || IsCond(op));
    }
    static inline bool IsShortForward(addr_t fb_va, addr_t taken_va, addr_t tgt_va)
    {
        return (tgt_va > taken_va + MIN_BUNDLE_SIZE && NextFBPC(fb_va) > tgt_va);
    }
    static inline bool CheckEqual(const PtrFB& fb1, const PtrFB& fb2)
    {
        return (fb1->fbid == fb2->fbid && fb1->fbid_local == fb2->fbid_local);
    }
    static inline bool CheckEqual(seq_t fbid1, seq_t fbid_local1, seq_t fbid2, seq_t fbid_local2)
    {
        return (fbid1 == fbid2 && fbid_local1 == fbid_local2);
    }
    static inline bool CheckOlder(const PtrFB& fb1, const PtrFB& fb2)
    {
        return (fb1->fbid < fb2->fbid || (fb1->fbid == fb2->fbid && fb1->fbid_local < fb2->fbid_local));
    }
    static inline bool hasReturn(const PtrFB& fb, pos_t pos)
    {
        for (pos_t p = CalcPosInFB(fb->va); p <= pos && p < BFU_BANDWIDTH; p++) {
            if (IsRet(fb->sp_info.attr[p])) {
                return true;
            }
        }
        return false;
    }
    static inline pos_t getHeaderPosByHid(PtrFB &fb, seq_t hid)
    {
        pos_t pos = CalcPosInFB(fb->va);
        for ( ; pos < BFU_BANDWIDTH; pos++) {
            if (fb->machineInst[pos] && fb->machineInst[pos]->bfuInfo->vld && !fb->machineInst[pos]->bfuInfo->spInfo->isInst &&
                fb->machineInst[pos]->bfuInfo->hid == hid) {
                break;
            }
        }
        return pos;
    }

    bool BstopNeed(SimInst const& bstart)
    {
        return !bstart->bfuInfo->fetch_end;
    }

    SimInst GenBstop(SimInst const& bstart)
    {
        SimInst minst = std::make_shared<SimInstInfo>();
        ASSERT(bstart->bfuInfo && bstart->bfuInfo->spInfo);
        if (bstart->bfuInfo) {
            minst->bfuInfo = std::make_shared<BFUMachineInfo>(*bstart->bfuInfo);
        }
        if (bstart->bfuInfo->spInfo) {
            minst->bfuInfo->spInfo = std::make_shared<SPMInstInfo>(*bstart->bfuInfo->spInfo);
        }
        minst->opcode = Opcode::OP_BSTOP;
        minst->pc = bstart->pc;
        minst->bfuInfo->spInfo->isInst = true;
        minst->bfuInfo->spInfo->isLast = true;
        minst->isLastInBlock = true;
        minst->stid = bstart->stid;
        return minst;
    }

    static inline addr_t foldPC(addr_t a, msize_t folded_len)
    {
        // fold a to required length
        addr_t res = 0;
        msize_t ngroup = (64+folded_len-1) / folded_len;
        for (msize_t i = 0; i <ngroup; i++) {
            res ^= (a >> i*folded_len);
        }
        return LastNBit(res, folded_len);
    }

    static inline addr_t foldHist(ghr_t* hist, msize_t hist_len, msize_t folded_len)
    {
        // truncate hist (may have multiple 64b groups), and fold to folded_len
        // TODO: find a better way to fold hist!!!
        assert(folded_len < 64);
        msize_t nbin = (hist_len + folded_len - 1) / folded_len;
        std::vector<ghr_t> bins;
        // create bins
        for (msize_t i = 0; i <nbin; i++) {
            pos_t curr_pos = i * folded_len;
            pos_t group_start = curr_pos / 64;
            pos_t group_end = (curr_pos + folded_len - 1) / 64;
            assert(group_start < BFU_GHR_LEN && group_end <= BFU_GHR_LEN);
            pos_t pos_start = curr_pos % 64;
            ghr_t curr_bin = 0;
            if (group_start == group_end) {
                curr_bin = hist[group_start] >> pos_start;
            } else {
                curr_bin = (hist[group_start] >> pos_start) | (hist[group_end] << (64-pos_start));
            }
            bins.push_back(curr_bin);
        }
        // xor all bins
        ghr_t res = 0;
        for (auto& b : bins) res ^= b;
        return LastNBit(res, folded_len);
    }
};

}

} // namespace JCore
