#include "iex/iex_bn.h"

#include "core/Core.h"
#include "iex/iex.h"

namespace JCore {


using namespace std;

void BypassNetwork::Build()
{
}

void BypassNetwork::Reset()
{
}

void BypassNetwork::Xfer()
{
    E1Bypass();
    E2Bypass();
    E3Bypass();
    E4Bypass();
    E5Bypass();
    W1Bypass();
    W2Bypass();
}

void BypassNetwork::ProcessSimtBypass(SimInst &wInst, ExecEngineTyp anotherIdMem, BypassType type)
{
    std::shared_ptr<IEX> iex;
    if (top->core->IsVectorIex(anotherIdMem)) {
        iex = top->core->vectorTop->GetIex(static_cast<uint32_t>(anotherIdMem) -
            static_cast<uint32_t>(ExecEngineTyp::IEX_NUM));
    } else {
        iex = top->core->iex[anotherIdMem];
    }
    auto anotherPipeMem = iex->GetBN().pipe_i2;
    for (uint32_t j = 0; j < anotherPipeMem.size(); j++) {
        auto &rInst = *anotherPipeMem[j];
        if (!rInst) {
            continue;
        }
        if (rInst->biqType == BIQType::VEC_IQ && rInst->peID != wInst->peID) {
            continue;
        }
        // glRegBypass(rInst, wInst, type);
    }
}

void BypassNetwork::ProcessBypass(BypassType type, vector<SimInst*>& w_pipe, vector<SimInst*>& r_pipe)
{
    for (uint32_t j = 0; j < w_pipe.size(); j++) {
        auto& wInst = *w_pipe[j];
        if (!wInst) {
            continue;
        }
        for (uint32_t i = 0; i < r_pipe.size(); i++) {
            auto& rInst = *r_pipe[i];
            if (rInst != nullptr) {
                // relative index. And auto global-get instruction must be absolute index.
                RegBypass(rInst, wInst);;
            }
        }
    }
}

void BypassNetwork::E1Bypass()
{
    // Crossbar 1: E1 to I2 bypass
    ProcessBypass(BypassType::E1_I2, pipe_e1, pipe_i2);
}

void BypassNetwork::E2Bypass()
{
    // Crossbar 2: E2 to I2 bypass
    ProcessBypass(BypassType::E2_I2, pipe_e2, pipe_i2);
}

void BypassNetwork::E3Bypass()
{
    // Crossbar 3: E3 to I2 bypass
    ProcessBypass(BypassType::E3_I2, pipe_e3, pipe_i2);
}

void BypassNetwork::E4Bypass()
{
    // Crossbar 4: E4 to I2 bypass
    ProcessBypass(BypassType::E4_I2, pipe_e4, pipe_i2);
}

void BypassNetwork::E5Bypass()
{
    // Crossbar 5: E5 to I2 bypass
    ProcessBypass(BypassType::E5_I2, pipe_e4, pipe_i2);
}

void BypassNetwork::W1Bypass()
{
    // Crossbar 6: W1 to I2 bypass
    ProcessBypass(BypassType::W1_I2, pipe_w1, pipe_i2);
}

void BypassNetwork::W2Bypass()
{
    // Crossbar 7: W2 to I2 bypass
    ProcessBypass(BypassType::W2_I2, pipe_w2, pipe_i2);
}

void SetVecData(POperandPtr src, POperandPtr dst)
{
    if (src->vecDataVld) {
        if (dst->vecDataVld) {
            src->vecData.Set(dst->vecData.data);
        } else {
            src->vecData.Set(dst->data);
        }
    }
}

void BypassNetwork::RegBypass(SimInst rInst, SimInst wInst)
{
    for (auto dst : wInst->pdsts_) {
        for (auto src : rInst->psrcs_) {
            if (!OperandMatch(rInst, src, wInst, dst)) {
                continue;
            }
            src->dataVld = true;
            src->data = dst->data;
            src->dataFromBypass = true;
            SetVecData(src, dst);
            if (src->type == JCore::OperandType::STACK_POINTER && !wInst->isLoadReturn) {
                src->data = wInst->psrcs_[SRC0_IDX]->data;
            }
        }
    }
}

bool BypassNetwork::OperandMatch(SimInst rInst, POperandPtr src, SimInst wInst, POperandPtr dst)
{
    if (wInst->SrcTypeContain(OperandType::STACK_POINTER) && dst->type == OperandType::OPD_GREG) {
        return true;
    }
    if (src->IsLocalReg()) {
        if (wInst->SrcTypeContain(OperandType::STACK_POINTER) || rInst->peID != wInst->peID
            || rInst->GetTid() != wInst->GetTid()) {
            return false;
        }
    }
    if (src->type == OperandType::STACK_POINTER && rInst->GetTid() != wInst->GetTid()) {
        return false;
    }
    return src->type == dst->type && src->ptag == dst->ptag;
}

SimSys *BypassNetwork::GetSim() {
    return top->GetSim();
}

} // namespace JCore
