#pragma once

#include <deque>
#include <list>
#include <map>
#include <vector>

#include "../configs/bfu_config.h"
#include "bctrl/bfu/bfu_component.h"
#include "bctrl/bfu/bfu_interface.h"
#include "core/Bus.h"

namespace JCore {


struct BFUConfig;

namespace NS_CORE {

class BFU;

struct LBHisCntEntry {
    uint64_t                    repCnt = 0;
    uint64_t                    nextBPC = 0;
};

struct LBHisCnt {
    std::deque<LBHisCntEntry>   lbRepHis;
    uint32_t                    wptr = 0;
    uint32_t                    tptr = 0;
    uint32_t                    rptr = 0;
    SatCnt3b                    scnt3;
    void                        incTPtr();
    void                        incRPtr();
};

struct LoopBufferEntry {
    bool                        vld;
    uint64_t                    startBPC;
    uint64_t                    frontBPC;
    uint64_t                    nextBPC_Vld;
    uint64_t                    nextBPC;
    uint64_t                    nextPtr;
    bool                        multiNext;
    uint64_t                    size;
    uint64_t                    age;
    uint64_t                    repeatCnt;
    uint64_t                    aaccelss;
    uint64_t                    readPtr;
    uint64_t                    failMatch;
    bool                        missCntVld;
    uint64_t                    lastMissCnt;
    bool                        missLocVld;
    uint64_t                    missLoc;
    bool                        isStable;
    bool                        spin;
    bool                        bifurcate;
    uint64_t                    bifurIdx;
    bool                        isFirst;
    bool                        locked;
    SatCnt2b                    scnt;
    // std::vector<BlockHeader>    headerList;
    LBHisCnt                    lbHisQ;
    uint64_t                    in_cycle;
    LoopBufferEntry();
    void Reset();
    void addRPtr();
    void recordRepCnt(uint64_t count, uint64_t nBPC, bool exact);
    void updateRepCnt();
    void updateScnt(bool upgrade);
    void recordMissCnt(uint64_t count);
    void recordMissLoc(uint64_t location);
    friend std::ostream& operator<<(std::ostream& out, LoopBufferEntry const &entry);
};


struct HisBPCEntry {
    uint64_t                    bpc = 0x0;
    bool                        lPtrVld = false;
    uint32_t                    lPtr = -1;
    bool                        rPtrVld = false;
    uint32_t                    rPtr = -1;
    bool                        spin = false;
    uint32_t                    spinCnt = 1;
};

struct LBTrainer {
    /* \brief History BPC list to train */
    std::vector<HisBPCEntry>    tarBPCList;
    /* \brief Block Header list */
    // std::vector<BlockHeader>    hdrSeq;
    /* \brief BPC before loop */
    uint64_t                    frontBPC;
    /* \brief BPC after loop */
    uint64_t                    nextBPC;
    /* \brief Location pointer of trainer list to compare */
    uint64_t                    traPtr;
    /* \brief Location pointer of trainer list to compare */
    uint64_t                    traSecPtr;
    /* \brief Number of times the loop is repeated */
    uint64_t                    aaccelss;
    /* \brief For bifurcate loop, number of times the first loop is repeated */
    uint64_t                    firAaccelss;
    /* \brief For bifurcate loop, number of times the second loop is repeated */
    uint64_t                    secAaccelss;

    /* \brief Whether the trainer is trinaing */
    bool                        training;
    /* \brief Whether the trainer is trinaing bifurcate */
    bool                        trainingBifur;
    /* \brief Whether the trainer need header */
    bool                        needHdr;
    /* \brief Whether the trainer keep to train */
    bool                        keep;
    /* \brief Whether to insert the fork */
    bool                        insertBifur;
    /* \brief Whether the loop is spin */
    bool                        spin;
    /* \brief Whether the loop has bifurcates */
    bool                        bifurcate;
    bool                        addBifur;
    /* \brief Whether to check bifurcates count. */
    bool                        checkBifur;
    /* \brief Whether the trainer has miss location. */
    bool                        missLocVld;
    uint64_t                    lastIdx;
    /* \brief The bifurcate start location */
    uint64_t                    bifurIdx;
    /* \brief Whether this loop is existed in LB */
    bool                        existInLB;
    /* \brief The index of this loop in LB */
    uint64_t                    existInLBIdx;
    /* \brief Whether this bifurcate loop is existed in LB */
    bool                        bifurInLB;
    /* \brief The index of this bifurcate loop in LB */
    uint64_t                    bifurInLBIdx;
    LBTrainer();
    void                        Reset();
    void                        addTPtr(bool hitBifur);
    void                        updateBPCListPtr();
    void                        matchTraList(uint64_t bpc, bool &hitFir, bool &hitSec);
    std::vector<uint64_t>       getLookupArray(bool bifurcate);
};

struct RecentBPCList {
    std::deque<HisBPCEntry>     BPCQ;
    uint32_t                    maxSize;
    RecentBPCList();
    RecentBPCList(uint64_t size);
    void                        Reset();
    void                        insertQ(uint64_t bpc);
};

class LoopBuffer : public BFUComponent {
protected:
    std::vector<LoopBufferEntry>    loopBuffer;
    LBTrainer                       lbTrainer;
    RecentBPCList                   rBPCList;
    uint64_t                        lbSize = 0;

    bool                            hitLoopBuffer = false;
    uint64_t                        LBRPtr = 0;
    uint64_t                        countSendH = 0;

public:
    LoopBuffer();
    ~LoopBuffer();

    void                            Build();
    void                            train(PtrMachineInst const& h);
    bool                            Lookup(std::vector<uint64_t> const& indexBPC);
    bool                            matchRBPCList(uint64_t cur_bpc);
    void                            trainingLB(PtrMachineInst const& h);
    bool                            addBifur(PtrMachineInst const& h);

    uint64_t                        getLBWPtr();
    void                            releaseLBEntry(uint64_t idx);
    void                            insertLB(bool bifurcate);
    void                            updateLBCnt(bool bifurcate);
    void                            addEntryRPtr(uint64_t num);
    

    bool                            useLoopBuffer() { return hitLoopBuffer;}
    void                            stopLBFetch();
    bool                            isSpinBlock();
    bool                            isTrust(uint64_t idx);
    LookupLBInfo                    lookupLB(std::vector<uint64_t> const& indexBPC);
    PtrMachineInst                         getHFromLB();
    void                            getNextBPC(uint64_t &nextBPC);
    void                            updateLBAge(uint64_t idx);
    void                            updateLBSCnt2b(uint64_t idx, bool upgrade);
    void                            lockLBEntry(uint64_t idx);
    void                            unlockLBEntry(uint64_t idx);

    void                            printHotPath(std::vector<HisBPCEntry> const& loopBList);
    void                            printLB();

    void                            runAtMainPred(PtrFB const& fb);
    void                            lookupAtRedirect(PtrMachineInst const& h, PtrFB const& fb);
    bool                            sendHFromLB(PtrFB const& fb);
    void                            runAtRedirect(PtrMachineInst const& h, PtrFB const& fb);
    void                            handleLBRlvH(PtrMachineInst const& header);
    void                            checkBifurcate();

};
}   // namespace NS_CORE

} // namespace JCore
