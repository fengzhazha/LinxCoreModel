#include "bctrl/bfu/bfu_loop_buffer.h"

#include "bctrl/bfu/bfu.h"
#include "bctrl/bfu/bfu_utils.h"

namespace JCore {


using namespace std;
namespace NS_CORE {
LoopBuffer::LoopBuffer() {}
LoopBuffer::~LoopBuffer() {}

void LoopBuffer::Build() {
    for (uint64_t i = 0; i < cfg->lb_depth; i++) {
        loopBuffer.emplace_back();
    }
    lbTrainer = LBTrainer();
    rBPCList = RecentBPCList(cfg->lb_rlist_depth);
}

void LoopBuffer::train(PtrMachineInst const& h) {
    if (!cfg->lb_enable) {
        return;
    }
    if (cfg->bcache_verbose) {
        cout<<endl<<"Cycle : "<<dec<<bfu->GetSim()->getCycles()<<", correctBCount"<<bfu->GetSim()->correctBCount<<endl;
        cout<<"[BFU]: LB train input pc 0x"<<hex<<h->GetBundlePosPC()<<endl;
    }
    if (!lbTrainer.training) {
        if (matchRBPCList(h->GetBundlePosPC())) {
            // lbTrainer.hdrSeq.push_back(h->minst);
            if (lbTrainer.spin) {
                lbTrainer.needHdr = false;
            }
            if (cfg->bcache_verbose) {
                std::cout << "[BFU]: commit info start train loop buffer entry at: 0x"<< hex << h->GetBundlePosPC();
                std::cout << " hot seq size "<<dec<< lbTrainer.tarBPCList.size() << std::endl;
            }
        }
    } else {
        trainingLB(h);
    }
    rBPCList.insertQ(h->GetBundlePosPC());

    if (cfg->bcache_verbose) {
        cout<<"[BFU]: recent BPC List "<<endl;
        uint32_t cnt = 0;
        for (auto it : rBPCList.BPCQ) {
            cout << " 0x"<<hex<<it.bpc;
            if (it.spin) {
                cout << "[spin*"<<dec<<it.spinCnt<<"]";
            }
            cnt++;
            if (cnt % 8 == 0) {
                cout<<endl;
            }
        }
        cout<<endl;
    }
}

bool LoopBuffer::Lookup(vector<uint64_t> const& indexBPC) {
    LookupLBInfo lookupLBInfo = lookupLB(indexBPC);
    if (lookupLBInfo.valid) {
        stats->lb_hit++;
        LBRPtr = lookupLBInfo.lbIndex;
        loopBuffer[LBRPtr].readPtr = lookupLBInfo.lastEntryIndex;
        // uint64_t lbstartBPC = loopBuffer[LBRPtr].headerList[lookupLBInfo.lastEntryIndex].bpc;
        uint64_t lbstartBPC = 0;
        countSendH = 0;
        lockLBEntry(LBRPtr);

        if (cfg->bcache_verbose) {
            std::cout << "[BFU LB]: Loop buffer hit at entry[ " << dec << lookupLBInfo.lbIndex;
            std::cout << " ], index[ " << dec << lookupLBInfo.lastEntryIndex;
            std::cout << " ] start bpc 0x" << hex << lbstartBPC << std::endl;
        }
        return true;
    }
    return false;
}

bool LoopBuffer::matchRBPCList(uint64_t curBPC) {
    if (rBPCList.BPCQ.size() <=0) {
        return false;
    }
    if (curBPC > rBPCList.BPCQ.back().bpc) {
        return false;
    }
    bool spinBPC = false;
    uint64_t i = 1;
    for (auto rit = rBPCList.BPCQ.rbegin(); rit != rBPCList.BPCQ.rend() && i < 100; ++rit, i++) {
        if (i > cfg->lb_entry_depth) {
            return false;
        }
        if ((*rit).bpc == curBPC) {
            // Check spin BPC
            if (rit == rBPCList.BPCQ.rbegin()) {
                if ((*rit).spin) { 
                    if ((*rit).spinCnt >= 3) {
                        spinBPC = true;
                    } else {
                        return false;
                    }
                } else {
                    return false;
                }
            }
            // Currently, Only Work for spin BPC
            lbTrainer.training = true;
            auto front_rit = rit;
            front_rit++;
            if (spinBPC) {
                lbTrainer.aaccelss = (*rit).spinCnt + 1;
                lbTrainer.traPtr = 0;
                lbTrainer.spin = true;
                lbTrainer.tarBPCList.push_back((*rit));
                lbTrainer.updateBPCListPtr();
                lbTrainer.tarBPCList.back().spinCnt++;
                return true;
            } else {
                for (; rit != rBPCList.BPCQ.rbegin(); --rit) {
                    lbTrainer.tarBPCList.push_back(*rit);
                    while ((*rit).spin && (*rit).spinCnt > 1) {
                        lbTrainer.tarBPCList.push_back(*rit);
                        (*rit).spinCnt--;
                    }
                }
                lbTrainer.tarBPCList.push_back(*rit);
                if (lbTrainer.tarBPCList.size() > cfg->lb_entry_depth) {
                    lbTrainer.Reset();
                    return false;
                }
                lbTrainer.updateBPCListPtr();
                // TODO: fix get target bpclist that length of hot sequence larger than one
                lbTrainer.aaccelss = 1;
                uint64_t sIdx = rBPCList.BPCQ.size() - lbTrainer.tarBPCList.size();
                uint64_t i = sIdx - lbTrainer.tarBPCList.size();
                for (; i >= 0 && i < rBPCList.BPCQ.size();) {
                    if (rBPCList.BPCQ[i].bpc == rBPCList.BPCQ[sIdx].bpc) {
                        i = i - lbTrainer.tarBPCList.size();
                        lbTrainer.aaccelss++;
                    } else {
                        break;
                    }
                }
                lbTrainer.traPtr = 1;
                return true;
            }
        }
    }
    return false;
}

void LoopBuffer::trainingLB(PtrMachineInst const& h) {
    uint64_t idx = 0;
    bool hitFirst = false;
    bool hitSecond = false;
    lbTrainer.matchTraList(h->GetBundlePosPC(), hitFirst, hitSecond);

    if (hitFirst) {
        idx = lbTrainer.traPtr;
        if (cfg->bcache_verbose) {
            if (!lbTrainer.trainingBifur) {
                cout << "[BFU]: training match at: 0x"<< hex << h->GetBundlePosPC() << std::endl;
            } else {
                cout << "[BFU]: training match first half bifurcate branch at: 0x"<< hex << h->GetBundlePosPC() << std::endl;
            }
        }
        if (lbTrainer.addBifur) {
            lbTrainer.addBifur = false;
            lbTrainer.tarBPCList.back().lPtrVld = true;
            lbTrainer.tarBPCList.back().lPtr = 0;
            lbTrainer.tarBPCList.back().rPtrVld = true;
            lbTrainer.tarBPCList.back().rPtr = lbTrainer.bifurIdx;
        }
        // if (lbTrainer.needHdr && !lbTrainer.bifurcate) {
        //     lbTrainer.hdrSeq.push_back(h->minst);
        // }
        if (lbTrainer.spin && idx == 0) {
            lbTrainer.tarBPCList[idx].spinCnt++;
        }
        if (lbTrainer.trainingBifur && lbTrainer.traPtr < lbTrainer.bifurIdx) {
            lbTrainer.trainingBifur = false;
            if (!lbTrainer.bifurInLB && !lbTrainer.needHdr && lbTrainer.secAaccelss >= cfg->lb_bifur_train_delay) {
                // lbTrainer.insertBifur = true;
                insertLB(true);
                if (!lbTrainer.training) {
                    return;
                }
                lbTrainer.checkBifur = false;
                lbTrainer.secAaccelss = 0;
            }
            if (lbTrainer.checkBifur && lbTrainer.bifurInLB) {
                updateLBCnt(true);
                lbTrainer.checkBifur = false;
                lbTrainer.secAaccelss = 0;
            }
            lbTrainer.aaccelss = 0;
        }

        lbTrainer.addTPtr(false);


        uint64_t threshold = lbTrainer.spin ? cfg->lb_spin_train_delay : cfg->lb_train_delay;
        uint64_t preUpdateThre = (100 * threshold <= 800) ? 100 * threshold : 800;
        if (!lbTrainer.existInLB && !lbTrainer.needHdr && lbTrainer.aaccelss >= threshold) {
            insertLB(false);
            if (!lbTrainer.training) {
                return;
            }
        } else if (lbTrainer.existInLB && lbTrainer.spin && (lbTrainer.aaccelss > 0) &&
                    (lbTrainer.aaccelss % (preUpdateThre) == 0)) {
            // To solve the ckpt only executes a specific cyclic sequence
            // lbTrainer.nextBPC = lbTrainer.hdrSeq.front().bpc;
            lbTrainer.keep = true;
            updateLBCnt(false);
        } else if (lbTrainer.existInLB && !lbTrainer.spin &&
                    lbTrainer.traPtr == 0 && (lbTrainer.aaccelss % (10 * threshold) == 0)) {
            // lbTrainer.nextBPC = lbTrainer.hdrSeq.front().bpc;
            lbTrainer.keep = true;
            updateLBCnt(false);
        }
    } else if (hitSecond) {
        idx = lbTrainer.traSecPtr;
        if (lbTrainer.checkBifur) {
            if (lbTrainer.lastIdx < lbTrainer.bifurIdx) {
                updateLBCnt(false);
                lbTrainer.aaccelss = 0;
            }
            lbTrainer.checkBifur = false;
        }
        if (cfg->bcache_verbose) {
            cout << "[BFU]: training match seconde half bifurcate branch at: 0x"<< hex << h->GetBundlePosPC() << std::endl;
        }
        lbTrainer.trainingBifur = true;
        lbTrainer.addTPtr(true);
    } else {
        if (cfg->bcache_verbose) {
            cout << "[BFU]: training not match clear "<<endl;
        }
        if (lbTrainer.existInLB && !lbTrainer.trainingBifur) {
            if (lbTrainer.traPtr == 0) {
                lbTrainer.nextBPC = h->GetBundlePosPC();
            } else {
                // lbTrainer.nextBPC = lbTrainer.hdrSeq.front().bpc;
            }
            lbTrainer.keep = false;
            updateLBCnt(false);
        }
        if (!addBifur(h)) {
            lbTrainer.Reset();
        }
    }
    if (cfg->bcache_verbose) {
        cout<<"aaccelss "<<dec<<lbTrainer.aaccelss<<" secAaccelss "<<dec<<lbTrainer.secAaccelss<<endl;
        if (lbTrainer.existInLB) {
            // cout<<"exist in loopBuffer "<<dec<<lbTrainer.existInLBIdx<<" LBSize "<<dec<<loopBuffer[lbTrainer.existInLBIdx].headerList.size();
            // cout<<" trainer bpclistsize "<<dec<<lbTrainer.tarBPCList.size()<<" hdrSize:"<<lbTrainer.hdrSeq.size()<<endl;
        }
    }
}

bool LoopBuffer::addBifur(PtrMachineInst const& h)
{
    if (!cfg->lb_bifurcate_enable || lbTrainer.missLocVld || !lbTrainer.existInLB || lbTrainer.traPtr != 0) {
        return false;
    }
    if (!lbTrainer.bifurcate) {
        if (lbTrainer.spin) {
            lbTrainer.tarBPCList[0].spinCnt = 0;
        }
        lbTrainer.aaccelss = 0;
        lbTrainer.bifurcate = true;
        lbTrainer.trainingBifur = true;
        // lbTrainer.hitBifur = true;
        lbTrainer.bifurIdx = lbTrainer.tarBPCList.size();
        uint64_t lastPtr = (lbTrainer.traPtr - 1 + lbTrainer.tarBPCList.size()) % lbTrainer.tarBPCList.size();
        lbTrainer.tarBPCList[lastPtr].rPtrVld = true;
        lbTrainer.tarBPCList[lastPtr].rPtr = lbTrainer.tarBPCList.size();
        lbTrainer.tarBPCList.back().rPtrVld = true;
        lbTrainer.tarBPCList.back().rPtr = lbTrainer.tarBPCList.size();
        lbTrainer.traPtr = 0;
    }
    HisBPCEntry hisEn;
    hisEn.bpc = h->GetBundlePosPC();
    lbTrainer.tarBPCList.emplace_back(hisEn);
    // lbTrainer.hdrSeq.emplace_back(h->minst);
    lbTrainer.secAaccelss = 1;
    lbTrainer.addBifur = true;
    if (cfg->bcache_verbose) {
        cout << "[BFU LB]: add bifurcate to train list pc:0x"<<hex<<h->GetBundlePosPC()<<endl;
    }
    if (lbTrainer.tarBPCList.size() - lbTrainer.bifurcate > cfg->lb_entry_depth) {
        if (cfg->bcache_verbose) {
            cout << "[BFU LB]: bifurcate length exceed threshold, clear"<<endl;
        }
        return false;
    }
    return true;
}

uint64_t LoopBuffer::getLBWPtr() {
    stats->lb_insert++;
    if (lbSize < cfg->lb_depth) {
        for (uint64_t i = 0; i < loopBuffer.size(); i++) {
            if (!loopBuffer[i].vld) {
                lbSize++;
                return i;
            }
        }
    }
    uint64_t tempAge = 0;
    uint64_t idx = -1;
    for (uint64_t i = 0; i < cfg->lb_depth; i++) {
        if (loopBuffer[i].locked || !loopBuffer[i].isFirst) {
            continue;
        }
        if (!loopBuffer[i].vld) {
            lbSize++;
            return i;
        } else {
            if (loopBuffer[i].age > tempAge) {
                tempAge = loopBuffer[i].age;
                idx = i;
            } else if (loopBuffer[i].age == tempAge) {
                if (loopBuffer[i].scnt.cnt <= loopBuffer[idx].scnt.cnt) {
                    idx = i;
                } else if (loopBuffer[i].aaccelss < loopBuffer[idx].aaccelss) {
                    idx = i;
                }
            }
        }
    }

    // Free relevent entry.
    releaseLBEntry(idx);
    lbSize++;
    return idx;
}

void LoopBuffer::releaseLBEntry(uint64_t idx) {
    if (loopBuffer[idx].locked || !loopBuffer[idx].vld) {
        return;
    }
    uint64_t curIdx = 0;
    uint64_t nextIdx = 0;
    nextIdx = loopBuffer[idx].nextPtr;
    loopBuffer[idx].Reset();
    if (cfg->bcache_verbose) {
        std::cout << "[BFU LB]: Release loop buffer entry. idx:"<<dec<<idx<<std::endl;
    }
    assert(lbSize > 0);
    lbSize--;
    while (nextIdx < cfg->lb_depth && idx != nextIdx) {
        curIdx = nextIdx;
        nextIdx = loopBuffer[curIdx].nextPtr;
        if (loopBuffer[curIdx].locked) {
            loopBuffer[curIdx].bifurcate = false;
            loopBuffer[curIdx].nextPtr = -1;
            return;
        }
        loopBuffer[curIdx].Reset();
        if (cfg->bcache_verbose) {
            std::cout << "[BFU LB]: Release loop buffer entry. idx:"<<dec<<curIdx<<std::endl;
        }
        assert(lbSize > 0);
        lbSize--;
    }
}

void LoopBuffer::insertLB(bool bifurcate) {
    vector<uint64_t> lookupArray = lbTrainer.getLookupArray(bifurcate);
    LookupLBInfo hitInfo = lookupLB(lookupArray);
    if (hitInfo.valid) {
        if (bifurcate) {
            lbTrainer.bifurInLB = true;
            lbTrainer.bifurInLBIdx = hitInfo.lbIndex;
        } else {
            lbTrainer.existInLB = true;
            lbTrainer.existInLBIdx = hitInfo.lbIndex;
        }
        /* Examples for Reset 
        * LBEntry: A->B->C->D->E
        * lbTrainer: C->D->E->A->B //to fix
        * lbTrainer: A->B  
        * */
        // uint64_t srcListSize = bifurcate ? (lbTrainer.tarBPCList.size() - lbTrainer.bifurIdx) : lbTrainer.tarBPCList.size();
        if (hitInfo.matchLen != lookupArray.size()) {
                //|| loopBuffer[hitInfo.lbIndex].headerList.size() != srcListSize) {
            lbTrainer.Reset();
            loopBuffer[hitInfo.lbIndex].failMatch += 1;
            uint64_t threshold = loopBuffer[hitInfo.lbIndex].spin ? cfg->lb_spin_train_delay : cfg->lb_train_delay;
            if (loopBuffer[hitInfo.lbIndex].failMatch > 2 * threshold && !loopBuffer[hitInfo.lbIndex].locked) {
                releaseLBEntry(hitInfo.lbIndex);
            }
            if (cfg->bcache_verbose) {
                std::cout << "[BFU LB]: lbTrainer bpc list hit loop buffer, but not match fully. clear LBTrainer [idx:";
                std::cout<< dec << hitInfo.lbIndex<< "]";
            }
            return;
        }
        if (cfg->bcache_verbose) {
            std::cout << "[BFU LB]: lbTrainer bpc list hit loop buffer [idx:"<< dec << hitInfo.lbIndex<< "]";
        }
        return;
    }
    if (cfg->printHotPath) {
        printHotPath(lbTrainer.tarBPCList);
    }

    uint64_t idx = getLBWPtr();
    loopBuffer[idx].vld = true;
    uint64_t startLBPtr = idx;
    // uint64_t startQPtr = bifurcate ? lbTrainer.bifurIdx : 0;
    // for (uint64_t i = startQPtr; i < lbTrainer.hdrSeq.size(); i++) {
    //     loopBuffer[idx].headerList.push_back(lbTrainer.hdrSeq[i]);
        
    //     if (((i + 1 - startQPtr) % cfg->lb_entry_depth ) == 0 || (i + 1) == lbTrainer.tarBPCList.size()) {
    //         loopBuffer[idx].startBPC = loopBuffer[idx].headerList.front().bpc;
    //         loopBuffer[idx].size = loopBuffer[idx].headerList.size();
    //         loopBuffer[idx].scnt.Reset(false);
    //         loopBuffer[idx].in_cycle = bfu->GetSim()->getCycles();
    //         if (bifurcate) {
    //             loopBuffer[idx].frontBPC = lbTrainer.tarBPCList[lbTrainer.bifurIdx - 1].bpc;
    //             loopBuffer[idx].aaccelss = lbTrainer.secAaccelss;
    //             loopBuffer[idx].spin = false;
    //             loopBuffer[idx].nextPtr = lbTrainer.existInLBIdx;
    //             if (lbTrainer.spin) {
    //                 loopBuffer[idx].scnt.Reset(true);
    //                 loopBuffer[idx].recordRepCnt(1, loopBuffer[idx].startBPC, true);
    //             }
    //             if (loopBuffer[lbTrainer.existInLBIdx].bifurcate && loopBuffer[lbTrainer.existInLBIdx].nextPtr < cfg->lb_depth) {
    //                 loopBuffer[loopBuffer[lbTrainer.existInLBIdx].nextPtr].bifurcate = false;
    //                 loopBuffer[loopBuffer[lbTrainer.existInLBIdx].nextPtr].nextPtr = -1;
    //                 loopBuffer[lbTrainer.existInLBIdx].multiNext = true;
    //                 loopBuffer[lbTrainer.existInLBIdx].isStable = false;
    //             }
    //             loopBuffer[lbTrainer.existInLBIdx].nextPtr = idx;
    //             loopBuffer[lbTrainer.existInLBIdx].bifurcate = true;
    //             loopBuffer[idx].bifurcate = true;
    //         } else {
    //             loopBuffer[idx].frontBPC = lbTrainer.frontBPC;
    //             loopBuffer[idx].aaccelss = lbTrainer.aaccelss;
    //             loopBuffer[idx].spin = lbTrainer.spin;
    //         }
    //         if (cfg->bcache_verbose) {
    //             std::cout << "[BFU LB]: insert Loop Buffer [idx:" <<dec<< idx <<"] ";
    //             std::cout << "start BPC 0x" << hex << loopBuffer[idx].startBPC;
    //             std::cout << " size:" << dec << loopBuffer[idx].size;
    //             std::cout << " spin:" << boolalpha << loopBuffer[idx].spin <<std::endl;
    //             for (auto entry : loopBuffer[idx].headerList) {
    //                 std::cout << " insert header 0x" << hex << entry.bpc <<std::endl;
    //             }
    //         }

    //         if ((i + 1) != lbTrainer.tarBPCList.size()) {
    //             uint64_t nextIdx = getLBWPtr();
    //             loopBuffer[idx].nextPtr = nextIdx;
    //             loopBuffer[idx].readPtr = 0;
    //             idx = nextIdx;
    //             loopBuffer[idx].vld = true;
    //         }
    //     }
    // }
    if (bifurcate) {
        lbTrainer.bifurInLB = true;
        lbTrainer.bifurInLBIdx = startLBPtr;
    } else {
        lbTrainer.existInLB = true;
        lbTrainer.existInLBIdx = startLBPtr;
    }
    loopBuffer[startLBPtr].isFirst = true;
    if (idx != startLBPtr) {
        loopBuffer[idx].nextPtr = startLBPtr;
    }
    updateLBAge(startLBPtr);
}

void LoopBuffer::updateLBCnt(bool bifurcate)
{
    vector<uint64_t> lookupArray = lbTrainer.getLookupArray(bifurcate);
    LookupLBInfo hitInfo = lookupLB(lookupArray);
    if (hitInfo.valid) {
        if ((bifurcate && lbTrainer.bifurInLB && (lbTrainer.bifurInLBIdx != hitInfo.lbIndex)) ||
            (!bifurcate && lbTrainer.existInLB && (lbTrainer.existInLBIdx != hitInfo.lbIndex))) {
            return;
        }
        uint64_t num = bifurcate ? lbTrainer.secAaccelss : lbTrainer.aaccelss;
        if (num == 0) {
            return;
        }
        uint64_t nBPC = bifurcate ? lbTrainer.tarBPCList[0].bpc : lbTrainer.nextBPC;
        if (cfg->bcache_verbose) {
            cout<<"[BFU LB]: LB Trainer update LBEntry[idx:"<<dec<<hitInfo.lbIndex<<"] repeated count "<<dec<<num<<", next_BPC "<<hex<<nBPC<<endl;
        }
        if (!lbTrainer.keep) {
            loopBuffer[hitInfo.lbIndex].recordRepCnt(num, nBPC, true);
        } else {
            loopBuffer[hitInfo.lbIndex].recordRepCnt(-1, nBPC, false);
        }
        if (!loopBuffer[hitInfo.lbIndex].locked && loopBuffer[hitInfo.lbIndex].repeatCnt > 0) {
            updateLBSCnt2b(hitInfo.lbIndex, true);
            loopBuffer[hitInfo.lbIndex].failMatch = 0;
        }
    }
}

void LoopBuffer::updateLBAge(uint64_t idx) {
    for (uint64_t i = 0; i < cfg->lb_depth; i++) {
        if (loopBuffer[i].vld) {
            loopBuffer[i].age++;
        }
    }
    loopBuffer[idx].age = 0;
    uint64_t nextIdx = loopBuffer[idx].nextPtr;
    while (nextIdx < cfg->lb_depth && nextIdx != idx && !loopBuffer[nextIdx].bifurcate) {
        loopBuffer[nextIdx].age = 0;
        nextIdx = loopBuffer[nextIdx].nextPtr;
    }
}

void LoopBuffer::addEntryRPtr(uint64_t num)
{
    if (loopBuffer[LBRPtr].nextPtr < loopBuffer.size() && !loopBuffer[LBRPtr].bifurcate) {
        uint64_t lbPtr = LBRPtr;
        while (num > 0) {
            loopBuffer[lbPtr].addRPtr();
            if (loopBuffer[lbPtr].readPtr == 0) {
                LBRPtr = loopBuffer[lbPtr].nextPtr;
            }
            num--;
        }
        LBRPtr = lbPtr;
    } else {
        while (num > 0) {
            loopBuffer[LBRPtr].addRPtr();
            num--;
        }
        
    }
}

LookupLBInfo LoopBuffer::lookupLB(vector<uint64_t> const& indexBPC) {
    LookupLBInfo retInfo;
    if (indexBPC.size() <= 1) {
        return retInfo;
    }
    bool match = false;
    for (uint64_t i = 0; i < indexBPC.size() - 1; i++) {
        for (uint64_t j = 0; j < cfg->lb_depth; j++) {
            if (!loopBuffer[j].vld) {
                continue;
            }
            if (indexBPC[i] == loopBuffer[j].startBPC && loopBuffer[j].isFirst) {
                uint64_t lbPtr = j;
                uint64_t bpcPtr = i + 1;
                uint64_t entryPtr = 1;

                while (bpcPtr < indexBPC.size()) {
                    if (entryPtr >= loopBuffer[lbPtr].size) {
                        if (loopBuffer[lbPtr].nextPtr < cfg->lb_depth && !loopBuffer[lbPtr].spin &&
                            !loopBuffer[lbPtr].bifurcate) {
                            lbPtr = loopBuffer[lbPtr].nextPtr;
                            retInfo.cross = true;
                        }
                        entryPtr = 0;
                    }
                    // if (indexBPC[bpcPtr] != loopBuffer[lbPtr].headerList[entryPtr].bpc) {
                    //     match = false;
                    //     break;
                    // } else {
                    //     match = true;
                    // }
                    bpcPtr++;
                    entryPtr++;
                }
                if (match) {
                    retInfo.valid = true;
                    retInfo.trust = isTrust(lbPtr);
                    retInfo.lbIndex = lbPtr;
                    retInfo.lastEntryIndex = entryPtr - 1;
                    retInfo.matchLen = bpcPtr - i;
                    if (cfg->bcache_verbose) {
                        cout << "[BFU LB]: lookupLB info: "<<"hit="<<boolalpha<<match;
                        cout << ", LBIndex: " <<dec<< retInfo.lbIndex << ", startBPC:"<< hex << loopBuffer[j].startBPC;
                        cout << ", match lenth: "<< dec << retInfo.matchLen <<std::endl;
                    }
                    return retInfo;
                }
            }
        }
    }
    if (cfg->bcache_verbose) {
        std::cout << "[BFU LB]: lookupLB info: "<<"hit="<<boolalpha<<match<< std::endl;
    }
    return retInfo;
}

PtrMachineInst LoopBuffer::getHFromLB() {
    // uint64_t entryRPtr = loopBuffer[LBRPtr].readPtr;
    // uint64_t tarLBPtr = LBRPtr;
    // if (entryRPtr < loopBuffer[LBRPtr].size) {
    //     if ((entryRPtr + 1) < loopBuffer[LBRPtr].size) {
    //         loopBuffer[LBRPtr].readPtr++;
    //     } else {
    //         loopBuffer[LBRPtr].readPtr = 0;
    //         if (loopBuffer[LBRPtr].nextPtr < cfg->lb_depth && !loopBuffer[LBRPtr].bifurcate) {
    //             // update next loop buffer reader point
    //             LBRPtr = loopBuffer[LBRPtr].nextPtr;
    //             loopBuffer[LBRPtr].readPtr = 0;
    //         }
    //     }
    //     BlockHeader curH = loopBuffer[tarLBPtr].headerList[entryRPtr];
    //     uint64_t cur_cycle = bfu->GetSim()->getCycles();
    //     PtrMachineInst newH = make_shared<MachineInst>(0, curH.bpc, curH, cur_cycle, cur_cycle, cur_cycle, cur_cycle, cur_cycle);
    //     if (loopBuffer[tarLBPtr].spin) {
    //         newH->predict_taken = true;
    //         newH->predict_tgt = newH->pc;
    //     } else {
    //         BlockHeader nextH = loopBuffer[LBRPtr].headerList[loopBuffer[LBRPtr].readPtr];
    //         if (utils->NextBlockPC(newH) == nextH.bpc) {
    //             newH->predict_taken = false;
    //             newH->predict_tgt = nextH.bpc;
    //         } else {
    //             newH->predict_taken = true;
    //             newH->predict_tgt = nextH.bpc;
    //         }
    //     }
    //     if (cfg->bcache_verbose) {
    //         std::cout << "[BFU LB]: get header from loop buffer ";
    //         std::cout << "pc: 0x" << hex << newH->pc <<" nextpc: 0x" << hex << newH->predict_tgt <<std::endl;
    //     }
    //     countSendH++;
    //     return newH;
    // }
    return nullptr;
}

void LoopBuffer::getNextBPC(uint64_t &nextBPC)
{
    if (loopBuffer[LBRPtr].nextBPC_Vld) {
        nextBPC = loopBuffer[LBRPtr].nextBPC;
    }
}

void LoopBuffer::stopLBFetch() {
    if (!hitLoopBuffer) {
        return;
    }
    hitLoopBuffer = false;
    loopBuffer[LBRPtr].readPtr = 0;
    countSendH = 0;
    unlockLBEntry(LBRPtr);
    loopBuffer[LBRPtr].updateRepCnt();
}

bool LoopBuffer::isSpinBlock()
{
    return loopBuffer[LBRPtr].spin;
}

bool LoopBuffer::isTrust(uint64_t idx)
{
    loopBuffer[idx].scnt.isTaken();
    bool enough = false;
    // if (loopBuffer[idx].repeatCnt == uint64_t(-1)) {
    //     enough = true;
    // } else if (loopBuffer[idx].repeatCnt * loopBuffer[idx].headerList.size() > cfg->lb_entry_depth * 2) {
    //     if ((cfg->two_taken_enable && loopBuffer[idx].headerList.size() < BFU_BANDWIDTH / 2) ||
    //         !cfg->two_taken_enable) {
    //         enough = true;
    //     } else {
    //         enough = false;
    //     }
    // }
    if (!loopBuffer[idx].bifurcate || !loopBuffer[loopBuffer[idx].nextPtr].scnt.isTaken()) {
        return loopBuffer[idx].scnt.isTaken() && !loopBuffer[idx].missLocVld && loopBuffer[idx].isStable && enough;
    } else {
        bool fixedBifur = false;
        uint64_t bifurEnPtr = loopBuffer[idx].nextPtr;
        if (loopBuffer[idx].spin || loopBuffer[bifurEnPtr].spin) {
            fixedBifur = true;
        } else if (loopBuffer[bifurEnPtr].nextPtr != idx) {
            return false;
        } else if (loopBuffer[idx].repeatCnt == 1 || loopBuffer[bifurEnPtr].repeatCnt == 1) {
            fixedBifur = true;
        }
        return (loopBuffer[idx].scnt.isTaken() && !loopBuffer[idx].missLocVld && fixedBifur && loopBuffer[idx].isStable && enough);
    }
}

void LoopBuffer::updateLBSCnt2b(uint64_t idx, bool upgrade) {
    loopBuffer[idx].updateScnt(upgrade);
    if (cfg->bcache_verbose) {
        cout<<"[BFU LB]: update [idx:"<<dec<<idx<<"] Scnt2b to "<<boolalpha<<loopBuffer[idx].scnt.isTaken()<<endl;
    }
    uint64_t nextIdx = loopBuffer[idx].nextPtr;
    if (nextIdx < cfg->lb_depth && !loopBuffer[idx].bifurcate) {
        while (nextIdx != idx && nextIdx < cfg->lb_depth) {
            loopBuffer[nextIdx].updateScnt(upgrade);
            nextIdx = loopBuffer[nextIdx].nextPtr;
        }
    }
}

void LoopBuffer::lockLBEntry(uint64_t idx) {
    loopBuffer[idx].locked = true;
    loopBuffer[idx].aaccelss++;
    updateLBAge(idx);
    uint64_t nextIdx = loopBuffer[idx].nextPtr;
    while (nextIdx < cfg->lb_depth && nextIdx != idx && loopBuffer[nextIdx].scnt.isTaken()) {
        loopBuffer[nextIdx].locked = true;
        loopBuffer[nextIdx].age = 0;
        loopBuffer[nextIdx].aaccelss++;
        nextIdx = loopBuffer[nextIdx].nextPtr;
    }
}

void LoopBuffer::unlockLBEntry(uint64_t idx) {
    loopBuffer[idx].locked = false;
    uint64_t nextIdx = loopBuffer[idx].nextPtr;
    while(nextIdx != idx && nextIdx < cfg->lb_depth) {
        loopBuffer[nextIdx].locked = false;
        nextIdx = loopBuffer[nextIdx].nextPtr;
    }
}

void LoopBuffer::printHotPath(vector<HisBPCEntry> const& loopBList) {
    if (cfg->bcache_verbose) {
        std::cout << "Cycle: " << dec << bfu->GetSim()->getCycles() << std::endl;
        std::cout << "hot loop size: " << dec << loopBList.size() << std::endl;
        uint64_t count = 0;
        for (auto it : loopBList) {
            std::cout << "  0x" << hex << it.bpc;
            if (it.spin) {
                cout << "[spin*"<<dec<<it.spinCnt<<"]";
            }
            if (++count >= 8) {
                std::cout << " entry full." <<std::endl;
                count = 0;
            }
        }
        std::cout << "  end." <<std::endl;
    }
}

void LoopBuffer::printLB()
{
    if (cfg->bcache_verbose) {
        for (uint32_t i = 0; i < loopBuffer.size(); i++) {
            if (!loopBuffer[i].vld) {
                continue;
            }
            cout<<"[idx:]"<<dec<<i<<loopBuffer[i]<<endl;
        }
    }
}

void LoopBuffer::runAtMainPred(PtrFB const& fb) {
    if (!cfg->lb_enable || lbSize <= 0 || !fb->main_info.taken) {
        return;
    }
    vector<uint64_t> lookupArray;
    bool matchBP = false;
    pos_t pos_start = utils->CalcPosInFB(fb->va);
    pos_t pos_taken = fb->main_info.taken? fb->main_info.pos : BFU_BANDWIDTH;
    assert(pos_start <= pos_taken);
    // Lookup LB from FB start position to the taken position
    for (pos_t pos = pos_start; pos < BFU_BANDWIDTH; pos++) {
        if (fb->machineInst[pos] == nullptr) {
            continue;
        }
        if (fb->machineInst[pos]->bfuInfo->vld) {
            lookupArray.push_back(fb->machineInst[pos]->pc);
        }
        if (pos == pos_taken) break;
    }
        matchBP = true;
        lookupArray.push_back(fb->main_info.tgt);
    LookupLBInfo hitInfo = lookupLB(lookupArray);
    if (hitInfo.valid && hitInfo.trust) {
        stats->lb_hit++;
        hitLoopBuffer = true;
        LBRPtr = hitInfo.lbIndex;
        if (matchBP) {
            countSendH = hitInfo.cross ? hitInfo.lastEntryIndex : hitInfo.matchLen - 1;
        } else {
            countSendH = hitInfo.cross ? hitInfo.lastEntryIndex + 1 : hitInfo.matchLen;
        }
        // if (countSendH >= loopBuffer[LBRPtr].repeatCnt * loopBuffer[LBRPtr].headerList.size()) {
        //     stopLBFetch();
        //     if (cfg->bcache_verbose) {
        //         cout << "[BFU LB]: FB in F3 hit loop buffer and trust but reach repeated num [idx:";
        //         cout<< dec << hitInfo.lbIndex<< "]"<<endl;
        //     }
        //     return;
        // }
        loopBuffer[LBRPtr].aaccelss = 0;
        addEntryRPtr(hitInfo.lastEntryIndex);
        uint64_t lbstartBPC = loopBuffer[LBRPtr].startBPC;
        fb->lb_info.front = true;
        fb->lb_info.hit = true;
        fb->lb_info.lbIdx = LBRPtr;
        fb->lb_info.curRepCnt = 0;
        lockLBEntry(LBRPtr);
        if (cfg->bcache_verbose) {
            std::cout << "[BFU LB]: FB in F3 hit loop buffer and trust [idx:"<< dec << hitInfo.lbIndex<< "], sBPC 0x";
            std::cout <<hex<<lbstartBPC<<endl;
        }
    } else if (hitInfo.valid && !hitInfo.trust){
        uint64_t lbstartBPC = loopBuffer[LBRPtr].startBPC;
        if (cfg->bcache_verbose) {
            std::cout<<"[BFU LB]: FB in F3 hit loop buffer but not trust [idx:"<< dec << hitInfo.lbIndex<< "], sBPC 0x";
            std::cout <<hex<<lbstartBPC<<endl;
        }
    } else if (!hitInfo.valid){
        if (cfg->bcache_verbose) {
            std::cout<<"[BFU LB]: FB in F3 not hit loop buffer fbid:"<<dec<<fb->fbid<<endl;
        }
    }
}

void LoopBuffer::lookupAtRedirect(PtrMachineInst const& h, PtrFB const& fb)
{
    if (!cfg->lb_enable || lbSize <= 0 || !fb->main_info.taken) {
        return;
    }
    vector<uint64_t> lookupArray;
    pos_t sPos = utils->CalcPosInFB(fb->va);
    pos_t curPos = utils->CalcPosInFB(h->GetBundlePosPC(), fb->va);
    // cout<<" gap "<<dec<<sPos<<" "<<curPos<<endl;
    assert(sPos <= curPos);
    for (pos_t pos = sPos; pos < BFU_BANDWIDTH; pos++) {
        if (fb->machineInst[pos] == nullptr) {
            continue;
        }
        if (fb->machineInst[pos]->bfuInfo->vld) {
            lookupArray.push_back(fb->machineInst[pos]->pc);
        }
        if (pos == curPos) break;
    }
    lookupArray.push_back(h->bfuInfo->resolve_tgt);
    LookupLBInfo hitInfo = lookupLB(lookupArray);
    if (hitInfo.valid && hitInfo.trust) {
        countSendH = hitInfo.cross ? hitInfo.lastEntryIndex : hitInfo.matchLen - 1;

    }
}

bool LoopBuffer::sendHFromLB(PtrFB const& fb)
{
    // PtrMachineInst h = getHFromLB();
    // bool first = true;
    // pos_t pos = utils->CalcPosInFB(h->GetBundlePosPC());
    // while (first || h) {
    //     stats->lb_sendHdr++;
    //     if (first) {
    //         fb->va = h->GetBundlePosPC();
    //         fb->recent_va = h->GetBundlePosPC();
    //         first = false;
    //         fb->lb_info.hit = true;
    //         fb->lb_info.lbIdx = LBRPtr;
    //         fb->lb_info.curRepCnt = countSendH;
    //     }
    //     fb->machineInst[pos] = h;
    //     fb->machineInst[pos]->vld = true;
    //     fb->machineInst[pos]->fbid = fb->fbid;
    //     fb->lb_info.taken = h->predict_taken;
    //     fb->lb_info.pos = pos;
    //     fb->lb_info.tgt = h->predict_tgt;
    //     if (cfg->bcache_verbose) {
    //         cout<<"[BFU LB]: put header to FetchBundle from loop buffer pc=0x"<<hex<<h->GetBundlePosPC();
    //         cout<<", countSendHdr: "<<dec<<countSendH<<endl;
    //     }
    //     if (loopBuffer[LBRPtr].missCntVld && countSendH == loopBuffer[LBRPtr].lastMissCnt) {
    //         fb->lb_info.checkBP = true;
    //         if (cfg->bcache_verbose) {
    //             cout<<"[BFU LB]: reach miss headers count, to check BP to ensure trace"<<endl;
    //         }
    //     }
    //     if (!h->predict_taken && pos < (BFU_BANDWIDTH - 1) &&
    //         countSendH < loopBuffer[LBRPtr].repeatCnt * loopBuffer[LBRPtr].headerList.size()) {
    //         h = getHFromLB();
    //         pos++;
    //         continue;
    //     }
    //     if (countSendH < loopBuffer[LBRPtr].repeatCnt * loopBuffer[LBRPtr].headerList.size()) {
    //         return true;
    //     } else if (countSendH == loopBuffer[LBRPtr].repeatCnt * loopBuffer[LBRPtr].headerList.size()) {
    //         uint64_t nextBPC = utils->NextBlockPC(h->GetBundlePosPC());
    //         uint64_t fallBPC = utils->NextBlockPC(h->GetBundlePosPC());
    //         fb->lb_info.checkBP = true;
    //         if (loopBuffer[LBRPtr].bifurcate && !loopBuffer[LBRPtr].spin && loopBuffer[loopBuffer[LBRPtr].nextPtr].vld &&
    //             loopBuffer[loopBuffer[LBRPtr].nextPtr].scnt.isTaken()) {
    //             loopBuffer[LBRPtr].readPtr = 0;
    //             unlockLBEntry(LBRPtr);
    //             loopBuffer[LBRPtr].updateRepCnt();
    //             LBRPtr = loopBuffer[LBRPtr].nextPtr;
    //             lockLBEntry(LBRPtr);
    //             loopBuffer[LBRPtr].aaccelss = 0;
    //             nextBPC = loopBuffer[LBRPtr].startBPC;
    //             if (cfg->bcache_verbose) {
    //                 cout<<"[BFU LB]: Reach LBEntry repeated num: "<<dec<<countSendH<<", use bifurcate entry idx:"<<dec<<LBRPtr<<endl;
    //             }
    //             countSendH = 0;
    //             if (nextBPC == fallBPC) {
    //                 fb->machineInst[pos]->predict_taken = false;
    //                 fb->machineInst[pos]->predict_tgt = 0;
    //                 if (pos < (BFU_BANDWIDTH - 1)) {
    //                     h = getHFromLB();
    //                     pos++;
    //                     continue;
    //                 } else {
    //                     fb->lb_info.taken = false;
    //                     fb->lb_info.tgt = 0;
    //                     return true;
    //                 }
    //             } else {
    //                 fb->lb_info.taken = true;
    //                 fb->lb_info.tgt = nextBPC;
    //                 fb->machineInst[pos]->predict_taken = true;
    //                 fb->machineInst[pos]->predict_tgt = nextBPC;
    //                 return true;
    //             }
    //         } else {
    //             getNextBPC(nextBPC);
    //             if (nextBPC == fallBPC) {
    //                 fb->lb_info.taken = false;
    //                 fb->lb_info.tgt = 0;
    //                 fb->machineInst[pos]->predict_taken = false;
    //                 fb->machineInst[pos]->predict_tgt = 0;
    //             } else {
    //                 fb->lb_info.taken = true;
    //                 fb->lb_info.tgt = nextBPC;
    //                 fb->machineInst[pos]->predict_taken = true;
    //                 fb->machineInst[pos]->predict_tgt = nextBPC;
    //             }
    //             bfu->CreateNewInfoToFetchQ(nextBPC ,h->GetBundlePosPC(), false);
    //             if (cfg->bcache_verbose) {
    //                 cout<<"[BFU LB]: Reach LBEntry repeated num: "<<dec<<countSendH<<", create new F0 pc:0x"<<hex<<nextBPC<<endl;
    //             }
    //             stopLBFetch();
    //             return true;
    //         }
    //     } else {
    //         if (cfg->bcache_verbose) {
    //             cout<<"[BFU LB]: Exceed LBEntry repeated num: "<<dec<<countSendH<<", stop use loop buffer"<<endl;
    //         }
    //         stopLBFetch();
    //         return false;
    //     }
    // }
    return false;
}

void LoopBuffer::runAtRedirect(PtrMachineInst const& h, PtrFB const& fb) {
    stopLBFetch();
    if (!fb->lb_info.hit || fb->lb_info.curRepCnt == 0) {
        return;
    }
    if (cfg->bcache_verbose) {
        cout<<"[BFU LB]: redirect header from loopBuffer [idx:"<<dec<<fb->lb_info.lbIdx<<"]"<<", pc=0x"<<hex<<h->GetBundlePosPC();
        cout<<" tgt_pc=0x"<<h->bfuInfo->resolve_tgt<<", cntSendHdr:"<<dec<<fb->lb_info.curRepCnt<<", fb_spos";
        cout<<utils->CalcPosInFB(fb->va)<<" header pos "<<utils->CalcPosInFB(h->GetBundlePosPC(), fb->va)<<endl;
    }
    uint64_t missCount = fb->lb_info.curRepCnt + utils->CalcPosInFB(h->GetBundlePosPC(), fb->va) - utils->CalcPosInFB(fb->va);
    loopBuffer[fb->lb_info.lbIdx].recordMissCnt(missCount);
    if (!fb->lb_info.checkBP && fb->lb_info.curRepCnt < cfg->lb_degrade_length && !loopBuffer[fb->lb_info.lbIdx].spin &&
        !loopBuffer[fb->lb_info.lbIdx].bifurcate) {
        updateLBSCnt2b(fb->lb_info.lbIdx, false);
        if (cfg->bcache_verbose) {
            cout<<"[BFU LB]: Reduce the LB Entry [idx:"<<dec<<fb->lb_info.lbIdx<<"]"<<endl;
        }
    }
}

void LoopBuffer::checkBifurcate()
{
    for (uint32_t i = 0; i < loopBuffer.size(); i++) {
        if (!loopBuffer[i].vld || !loopBuffer[i].bifurcate) {
            continue;
        }
        if (loopBuffer[i].bifurcate) {
            uint32_t count = 0;
            uint32_t nextptr = loopBuffer[i].nextPtr;
            bool    cover = false;
            while (count < cfg->lb_depth && nextptr != i) {
                if (nextptr == i) {
                    cover = true;
                    break;
                }
                nextptr = loopBuffer[nextptr].nextPtr;
                count++;
            }
            if (nextptr == i) {
                cover = true;
            }
            if (!cover) {
                for (uint32_t i = 0; i < loopBuffer.size(); i++) {
                    if (!loopBuffer[i].vld) {
                        continue;
                    }
                    cout<<"[idx:]"<<dec<<i<<loopBuffer[i]<<endl;
                }
                cout<<"Cycle "<<dec<<bfu->GetSim()->getCycles()<<endl;
                assert(0);
            }
        }
    }
}

LoopBufferEntry::LoopBufferEntry()
{
    Reset();
}

void LoopBufferEntry::Reset()
{
    vld = false;
    startBPC = 0;
    frontBPC = 0;
    nextBPC_Vld = false;
    nextBPC = 0;
    nextPtr = 0xffffffffffffffff;
    multiNext = false;
    size = 0;
    age = 0;
    repeatCnt = 0;
    aaccelss = 0;
    readPtr = 0;
    failMatch = 0;
    missCntVld = false;
    lastMissCnt = 0;
    missLocVld = false;
    missLoc = 0;
    isFirst = false;
    isStable = true;
    spin = false;
    bifurcate = false;
    bifurIdx = 0;
    locked = false;
    scnt.Reset(false);
    // headerList.clear();
    lbHisQ.lbRepHis.clear();
    lbHisQ.scnt3.Reset(false);
    in_cycle = 0;
}

std::ostream& operator<<(std::ostream& out, LoopBufferEntry const &entry)
{
    out<<"LBEntry sBPC 0x"<<hex<<entry.startBPC<<", size:"<<dec<<entry.size;
    std::bitset<2> bits(entry.scnt.cnt);
    out<<", trust:"<<bits<<", repeated:"<<dec<<entry.repeatCnt;
    out<<", bifurcate:"<<boolalpha<<entry.bifurcate<<", nBPC 0x"<<hex<<entry.nextBPC;
    if (entry.nextPtr == (uint64_t)-1) {
        out<<", nextEntryPtr:nan";
    } else {
        out<<", nextEntryPtr:"<<dec<<entry.nextPtr;
    }
    out<<", isStable:"<<dec<<entry.isStable;
    out<<", lb";
    for (auto it : entry.lbHisQ.lbRepHis) {
        out<<" "<<dec<<it.repCnt;
    }
    if (entry.missCntVld) {
        out<<", missCnt:"<<dec<<entry.lastMissCnt;
    }
    if (entry.missLocVld) {
        out<<", missLocation:"<<dec<<entry.missLoc;
    }
    out<<", isFirst:"<<boolalpha<<entry.isFirst;
    out<<", insert Cycle:"<<dec<<entry.in_cycle;
    // out<<", readPtr:"<<dec<<entry.readPtr;
    // out<<", locked:"<<dec<<entry.locked;
    return out;
}

void LoopBufferEntry::addRPtr()
{
    // if ((readPtr + 1) == headerList.size()) {
    //     readPtr = 0;
    //     aaccelss++;
    // } else {
    //     readPtr++;
    // }
}

void LBHisCnt::incTPtr() {
    tptr = (tptr + 1) % lbRepHis.size();
}

void LBHisCnt::incRPtr() {
    rptr = (rptr + 1) % lbRepHis.size();
}

void LoopBufferEntry::recordRepCnt(uint64_t count, uint64_t nBPC, bool exact)
{
    LBHisCntEntry entry;
    entry.repCnt = count;
    entry.nextBPC = nBPC;
    if (lbHisQ.lbRepHis.size() >= 4) {
        lbHisQ.lbRepHis.pop_front();
        lbHisQ.lbRepHis.push_back(entry);
        updateRepCnt();
    } else {
        if (repeatCnt == 0) {
            repeatCnt = count;
            nextBPC_Vld = true;
            nextBPC = nBPC;
        }
        lbHisQ.lbRepHis.push_back(entry);
    }
    if (!exact) {
        repeatCnt = locked ? repeatCnt : count;
        return;
    }
}

void LoopBufferEntry::updateRepCnt()
{
    if (lbHisQ.lbRepHis.empty()) {
        return;
    }
    std::set<uint64_t> filter;
    for (auto it : lbHisQ.lbRepHis) {
        filter.insert(it.repCnt);
    }

    if (filter.size() == 1) {
        repeatCnt = locked ? repeatCnt : lbHisQ.lbRepHis.front().repCnt;
    } else {
        if ((filter.size() >= 2 && (*filter.begin()) < 64) || multiNext) {
            isStable = false;
        } else {
            isStable = true;
        }
        // Two Different Selection Examples of Maximum and Minimum
            auto max = std::max_element(lbHisQ.lbRepHis.begin(), lbHisQ.lbRepHis.end(), [](LBHisCntEntry &l, LBHisCntEntry &r) {
                return l.repCnt < r.repCnt;
            });
            if ((*max).repCnt == 1) {
                (*max).repCnt += 1;
            }
            repeatCnt = locked ? repeatCnt : (*max).repCnt;
            nextBPC = (*max).nextBPC;
        // }
    }
}

void LoopBufferEntry::updateScnt(bool upgrade)
{
    if (upgrade) {
        scnt.inc();
    } else {
        scnt.dec();
    }
}

void LoopBufferEntry::recordMissCnt(uint64_t count)
{
    missCntVld = true;
    lastMissCnt = count;
}

void LoopBufferEntry::recordMissLoc(uint64_t location)
{
    missLocVld = true;
    missLoc = location;
}

LBTrainer::LBTrainer()
{
    Reset();
}

void LBTrainer::Reset()
{
    tarBPCList.clear();
    // hdrSeq.clear();
    frontBPC = 0;
    nextBPC = 0;
    traPtr = 0;
    traSecPtr = -1;
    aaccelss = 0;
    firAaccelss = 0;
    secAaccelss = 0;
    training = false;
    trainingBifur = false;
    needHdr = true;
    keep = false;
    insertBifur = false;
    spin = false;
    bifurcate = false;
    addBifur = false;
    checkBifur = false;
    missLocVld = false;
    lastIdx = -1;
    bifurIdx = 0;
    existInLB = false;
    existInLBIdx = -1;
    bifurInLB = false;
    bifurInLBIdx = -1;
}

void LBTrainer::addTPtr(bool hitBifur)
{
    uint64_t index = hitBifur ? traSecPtr : traPtr;
    if (!tarBPCList[index].lPtrVld && !tarBPCList[index].rPtrVld) {
        if ((index + 1) == tarBPCList.size()) {
            traPtr = 0;
            if (index >= bifurIdx) {
                secAaccelss++;
            }
            needHdr = false;
            aaccelss++;
        } else {
            traPtr = index + 1;
        }
        traSecPtr = -1;
        return;
    }
    
    if (tarBPCList[index].lPtrVld) {
        traPtr = tarBPCList[index].lPtr;
        if (traPtr == 0) {
            aaccelss++;
            needHdr = false;
        }
    }
    if (tarBPCList[index].rPtrVld) {
        checkBifur = true;
        lastIdx = index;
        traSecPtr = tarBPCList[index].rPtr;
        if (index == tarBPCList.size() - 1) {
            secAaccelss++;
        }
    }
}

void LBTrainer::updateBPCListPtr()
{
    for (uint32_t i = 0; i < tarBPCList.size(); i++) {
        tarBPCList[i].lPtrVld = true;
        tarBPCList[i].lPtr = (i + 1) % tarBPCList.size();
    }
}

void LBTrainer::matchTraList(uint64_t bpc, bool &hitFir, bool &hitSec)
{
    if (tarBPCList.size() == 0) {
        return;
    }
    if (bpc == tarBPCList[traPtr].bpc) {
        hitFir = true;
        return;
    }
    if (traSecPtr >= (uint64_t)tarBPCList.size()) {
        return;
    }
    if (bpc == tarBPCList[traSecPtr].bpc) {
        hitSec = true;
        return;
    }
}

std::vector<uint64_t> LBTrainer::getLookupArray(bool bifurcate)
{
    vector<uint64_t> lookupArray;
    if (bifurcate) {
        for (uint32_t i = bifurIdx ; i < tarBPCList.size(); i++) {
            if (tarBPCList[i].spin) {
                for (uint32_t j = 0; j < tarBPCList[i].spinCnt; i++) {
                    lookupArray.push_back(tarBPCList[i].bpc);
                }
            } else {
                lookupArray.push_back(tarBPCList[i].bpc);
            }
        }
        if (lookupArray.size() == 1) {
            lookupArray.push_back(lookupArray[0]);
        }
    } else {
        if (spin) {
            lookupArray.push_back(tarBPCList[0].bpc);
            lookupArray.push_back(tarBPCList[0].bpc);
        } else {
            uint32_t threshold = (bifurIdx == 0) ? tarBPCList.size() : bifurIdx;
            for (uint32_t i = 0 ; i < threshold; i++) {
                if (tarBPCList[i].spin) {
                    for (uint32_t j = 0; j < tarBPCList[i].spinCnt; i++) {
                        lookupArray.push_back(tarBPCList[i].bpc);
                    }
                } else {
                    lookupArray.push_back(tarBPCList[i].bpc);
                }
            }
        }
    }
    return lookupArray;
}

RecentBPCList::RecentBPCList()
{
    BPCQ.clear();
    maxSize = 1024;
}

RecentBPCList::RecentBPCList(uint64_t size)
{
    BPCQ.clear();
    maxSize = size;
}

void RecentBPCList::Reset()
{
    BPCQ.clear();
}

void RecentBPCList::insertQ(uint64_t bpc)
{
    HisBPCEntry bpcEntry;
    bpcEntry.bpc = bpc;
    if (!BPCQ.empty() && bpc == BPCQ.back().bpc) {
        BPCQ.back().spin = true;
        BPCQ.back().spinCnt++;
        return;
    }
    if (BPCQ.size() < maxSize) {
        BPCQ.push_back(bpcEntry);
    } else {
        BPCQ.pop_front();
        BPCQ.push_back(bpcEntry);
    }
}

}

} // namespace JCore
