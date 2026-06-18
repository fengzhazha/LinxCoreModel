#pragma once

#ifndef VEC_PE_DECODE_H
#define VEC_PE_DECODE_H

#include <deque>
#include "ModelSpec.h"
#include "pe/PECommon/BlockRunInfo.h"
#include "pe/PECommon/DecodeBundle.h"
#include "ModelCommon/DelayQueue.h"
#include "ModelCommon/bus/RegCopyCtReqBus.h"

namespace JCore {

class VecPE;
class VecPEDecode : public SimObj {
public:
    VecPE* vpeTop;
    /* \brief Workload that the PE will decode. */
    std::vector<RingQueue<BlockRunInfo>*> workThdQ;
    /* \brief Binary of Decode bundle to decode from IFU */
    ThdSimQ<DecodeBundle>*              ifuDecThdQ;
    /* \brief SimInst queue to rename stage from decode. */
    SimQueue<SimInst>*                    dec_ren_q;
    /* \brief Binary of Decode bundle from one thread of ifuDecThdQ */
    INNER std::shared_ptr<SimQueue<DecodeBundle>>   ifu_dec_q;
    std::vector<int> thEntryOcc; // pick thread number
    bool                                stall = false;
    bool                                simtStall = false;
    bool                                contextSwitch = false;
    uint32_t                            binPtr = 0;
    uint32_t                            allocROBCnt = 0;
    // Round Robin pick thread to decode
    uint32_t                            threadPtr = 0;
    uint32_t                            ctThreadPtr = 0;

    std::vector<ROBID>                  vcSid;
    std::vector<bool>                   winding;
    uint64_t                            load_id = 0;

    SimInst                             stallInst = std::shared_ptr<SimInstInfo>(nullptr);
    INNER std::deque<CopyCtReqBus>      ctCopyinReqQueue;
    INNER std::vector<std::shared_ptr<SimQueue<CopyCtReqBus>>> ctReqQueue;
    INNER std::vector<std::shared_ptr<SimQueue<SimInst>>> ctGenInstQueue;

public:
    void Work() override;
    void Xfer() override;
    void Reset() override;
    void ReportStat() override;
    SimSys *GetSim() override;
    void Build();

    /* Main pipelines of vetor-pe. */
    void PickThread();
    void PickIfuThread(); // Pick the bundle of a thread to decode.
    void PickCTThread();
    bool CTPickMatch(uint32_t threadID);
    bool DecodeBinStall(uint32_t threadID, ROBID bid, ROBID gid);
    void DecodeInstBundle();
    void HandleLast(const DecodeBundle &bundle);
    void RecordDecodeStats();
    void UpdateVcSid(uint32_t tid);
    void UpdateInstSid(SimInst &dinst);
    SimInst DecodeInst(const DecodeBundle &bundle, uint32_t binIdx);
    bool CheckFetchBundle(DecodeBundle &bundle);
    void Flush(const FlushBus &flushReq);

    /* Top-down data statistics */
    void ReportTagStall(uint32_t leftSlots);
    void ReportVecRobStall(uint32_t leftSlots);
    void RepLocalRetired(OperandType type, ROBID &seq, bool isDealloc, uint32_t tid);

    // generate simt copyin/copyout template inst
    void  GenVectorMemInst();
    SimInst GenInitSimInst() const;
    void GenStoreInstSrc(SimInst const& inst, const OperandType& type,
        const uint64_t& value, const uint32_t& srcIdx) const;
    void GenLoadInstSrc(SimInst const& inst, const OperandType& type,
        const uint64_t& value, const uint32_t& srcIdx) const;
    void GenLoadInstDst(SimInst const& inst, const OperandType& type, const int64_t& value) const;
    void RecordGenCycle(SimInst const& inst);
    void DecodeCTInst(SimInst const& inst) const;
    void ConvertLoadToAdd(SimInst const& inst) const;
    void GenMcallCopyOutCT(const CopyCtReqBus& req, const OperandType* regTypes);
    void GenMcallCopyInCT(const CopyCtReqBus& req, const OperandType* regTypes);
    void SetScalarLaneMask(SimInst const& inst, OperandType type) const;
    void Dump();
};

}
#endif
