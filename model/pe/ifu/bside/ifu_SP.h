#pragma once

#include "core/Bus.h"
#include "pe/ifu/common/ifu_component.h"

namespace JCore {


namespace PE_IFU {

class SP : public IFUComponent {
public:
    explicit SP(PEIFU* p);
    ~SP();
    void                    Build() override;
    void                    Work(FetchReqPtr &fetchReq);
    void                    handleBEnd(FetchReqPtr const& fetchReq, uint32_t idx, bool isBEnd);
    void                    HandleBEnd(FetchReqPtr const& fetchReq, uint32_t idx, bool isBEnd, uint32_t totalWith);
    void                    HandleJump(FetchReqPtr const& fetchReq, uint32_t idx, bool IsDirect, uint32_t totalWith);
    void                    handleBR(FetchReqPtr const& fetchReq, uint32_t idx);

    PEIFU*                  peifu = nullptr;
    bool                    stall = false;
    void                    StallStaticPredict();
    void                    UnStallStaticPredict();
    bool                    NeedStallStaticPredict();
};
}

} // namespace JCore
