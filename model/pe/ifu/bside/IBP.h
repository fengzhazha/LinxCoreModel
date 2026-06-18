#pragma once

#include <deque>
#include <unordered_map>
#include <vector>

#include "pe/ifu/common/ifu_component.h"

namespace JCore {


namespace PE_IFU {

class IBP : public IFUComponent {
public:
    IBP();
    ~IBP();

    void                                Build();
    void                                Reset();
    void                                Report();
    bool                                predict(uint64_t tpc, uint32_t instSize, uint32_t &mask, uint64_t &dst);
    void                                update(uint64_t tpc, uint64_t dst, uint32_t instSize);

    /* \brief just follow the last. */
    /* \brief hyper block bpc queue */
    std::deque<uint64_t>                hyperBQ;
    /* \brief hyper minst tpc queue (record the latest inner-jump destination) */
    std::deque<BranchEntry>             hyperIQ;

    std::unordered_map<uint64_t, PredictEntry>      instMap;

    /* \brief follow the last destination. */
    /* \brief find BPC in hyper block queue */
    bool                                isHyperBlock(uint64_t bpc);
    /* \brief find TPC in branch instruction queue */
    bool                                isBranchInst(uint64_t tpc);
    /* \brief push block's BPC into hyper block queue */
    void                                reportBlockHyper(uint64_t bpc);
    /* \brief push minst tpc into branch instruction queue */
    void                                reportBranchInst(uint64_t src, uint64_t dst, uint32_t instSize);
    /* \brief check branch instrutions in this fetch bundle */
    bool                                checkBranchInst(uint64_t tpc, uint32_t instSize, uint32_t &mask, uint64_t &dst);
    
    /* \brief two-bit saturation counter branch prediction. */
    bool                                isTaken(PredictorState &state);
    /* \brief update state of predictor. */
    void                                updateState(PredictorState &state, bool upgrade);
    /* \brief check the hyperIBP to predict. */
    bool                                noHistBP(uint64_t tpc, uint32_t instSize, uint32_t &mask, uint64_t &dst);
    /* \brief update the hyperIBP according to the inst result. */
    void                                noHistUpdate(uint64_t src, uint64_t dst, uint32_t instSize);
    /* \brief judge the state is taken. */
protected:
};
}

} // namespace JCore
