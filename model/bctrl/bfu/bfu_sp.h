#pragma once

#include <cstdint>

#include "bctrl/bfu/bfu_common.h"
#include "bctrl/bfu/bfu_component.h"
#include "ISA.h"
#include "bctrl/bfu/common/MachineInst.h"

namespace JCore {
using namespace std;

namespace NS_CORE {
constexpr uint64_t HEADER16_TYPE_END = 2;
constexpr uint64_t HEADER32_TYPE_END = 1;
constexpr uint64_t HEADER48_TYPE_END = 0;
constexpr uint64_t HEADER64_TYPE_END = 0;

constexpr uint64_t FIELD_16_TYPE_BEGIN = 1;
constexpr uint64_t FIELD_16_TYPE_END = 3;

constexpr uint64_t FIELD16_DIM_BEGIN = 1;
constexpr uint64_t FIELD16_DIM_END = 5;

constexpr uint64_t FIELD_32_TYPE_BEGIN = 1;
constexpr uint64_t FIELD_32_TYPE_END = 3;

constexpr uint64_t FIELD_48_TYPE_BEGIN = 17;
constexpr uint64_t FIELD_48_TYPE_END = 19;

constexpr uint64_t FIELD_64_UPPER_TYPE_BEGIN = 33;
constexpr uint64_t FIELD_64_UPPER_TYPE_END = 35;

constexpr uint64_t PREFIX_TYPE = 7;

constexpr uint64_t KILL_BRTYPE = 0b001;
constexpr uint64_t KILL_OPCODE = 0b001;
constexpr uint64_t KILL_TYPE = 0b001;

constexpr uint64_t FIELD_32_OPCODE_BEGIN = 4;
constexpr uint64_t FIELD_32_OPCODE_END = 6;
constexpr uint64_t FIELD_32_BRTYPE_BEGIN = 12;
constexpr uint64_t FIELD_32_BRTYPE_END = 14;

class StaticPredictor : public BFUComponent {
private:
    std::vector<PreDcodeInfo> globalDec;
    std::vector<PreDcodeInfo> localDec;
public:
    StaticPredictor() {};
    ~StaticPredictor() {};
    using BFUComponent::Build;
    void Build(uint32_t laneN, uint32_t threadCount);
    void Predict(PtrFB const& fb);
    void PredictLB(PtrFB const& fb);
    bool IsConcat(uint64_t bin, uint64_t nByte);
    bool IsMInst(uint64_t bin, uint64_t nByte);
    bool IsTemplate(uint64_t &bin, uint64_t binWidth);
    void Reset(uint32_t stid);
    void ResetLocal(uint32_t stid);
    void Flush(seq_t fbid_g, seq_t fbid_l, bool global_flush, bool local_flush, uint32_t stid);
    void SetHeaderStash(NS_CORE::PtrMachineInst h, SPInfoPtr minst, bool global, uint32_t pipe_id, uint32_t stid);
    void RecoverGlobalDec(PtrFB fb);
    addr_t GetAddPCDst(SPInfoPtr &inst, addr_t pc);
    void HandleAddPC(PtrMachineInst h, SPInfoPtr inst);
    void ResetPipe(bool global, uint32_t pipe_id, uint32_t stid);
    void GlobalToLocal(uint32_t pipe_id, uint32_t stid);
};

}


} // namespace JCore
