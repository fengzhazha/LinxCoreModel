#include "InstTracer.h"
#include <algorithm>
#include <functional>
#include <string>
#include <vector>
#include "SimSys.h"
#include "core/Core.h"
#include "ModelCommon/SimInstInfo.h"
#include "bctrl/BCtrl.h"
#include "cube/CubeCore.h"

namespace JCore {

using namespace std;

atomic<uint64_t> InstTracer::nextUid { 0 };

static vector<string> GetOpCodeTypeStrings(Opcode opcode)
{
    using FuncTypeT = pair<function<bool(Opcode)>, string>;
    const vector<FuncTypeT> funcTypes = {
        // { OpcodeIsBend, "Bend" },
        // { OpcodeIsInnerJump, "InnerJump" },
        // { OpcodeIsCMD, "CMD" },
        // { OpcodeIsDirectInnerJumpWithoutPcPop, "DirectInnerJumpWithoutPcPop" },
        // { OpcodeIsCondInnerJump, "CondInnerJump" },
        // { OpcodeIsReduce, "Reduce" },
        // { OpcodeIsDirectInnerJump, "DirectInnerJump" },
        // { OpcodeIsSysget, "Sysget" },
        // { OpcodeIsSysset, "Sysset" },
        // { OpcodeIsLoopAux, "LoopAux" },
        // { OpcodeIsBIOT, "BIOT" },
        // { OpcodeIsBIOR, "BIOR" },
        // { OpcodeIsBDIM, "BDIM" },
        // { OpcodeIsBTEXT, "BTEXT" },
        // { OpcodeIsSetc, "Setc" },
        // { OpcodeIsSetcCond, "SetcCond" },
        // { OpcodeIsSetcCondImm, "SetcCondImm" },
        // { OpcodeIsSetcEq, "SetcEq" },
        // { OpcodeIsSetcEqNe, "SetcEqNe" },
        // { OpcodeIsLoadImm, "LoadImm" },
        // { OpcodeIsLoadReg, "LoadReg" },
        // { OpcodeIsLoad, "Load" },
        // { OpcodeIsTLoad, "TLoad" },
        // { OpcodeIsStore, "Store" },
        // { OpcodeIsTStore, "TStore" },
        // { OpcodeIsStoreReg, "StoreReg" },
        // { OpcodeIsCacheManage, "CacheManage" },
        // { OpcodeIsMultiDiv, "MultiDiv" },
        // { OpcodeIsBitOperate, "BitOperate" },
        // { OpcodeIsLock, "Lock" },
        // { OpcodeIsFp, "Fp" },
        // { OpcodeIsFget, "Fget" },
        // { OpcodeIsMoveType, "MoveType" },
        // { OpcodeIsMove, "Move" },
        // { OpcodeIsMovr, "Movr" },
        // { OpcodeIsPrf, "Prf" },
        // { OpcodeIsCmp, "Cmp" },
        // { OpcodeIsFpSimt, "FpSimt" },
        // { OpcodeIsConst, "Const" },
        // { OpcodeIsPcCompute, "PcCompute" },
        // { OpcodeIsBitExten, "BitExten" },
        // { OpcodeIsShift, "Shift" },
        // { OpcodeIsLogic, "Logic" },
        // { OpcodeIsIntCal, "IntCal" },
        // { OpcodeIsCondSelect, "CondSelect" },
        // { OpcodeIsFCVT, "FCVT" },
        // { OpcodeIsDiv, "Div" },
        // { OpcodeIsImac, "Imac" },
        // { OpcodeIsPermute, "Permute" },
        // { OpcodeIsVFp, "VFp" },
        // { OpcodeIsVCvt, "VCvt" },
        // { OpcodeIsShf, "Shf" },
        // { OpcodeIsDivSqrt, "DivSqrt" },
        // { OpcodeIsEnc32TypeA, "Enc32TypeA" },
        // { OpcodeIsEnc32TypeB, "Enc32TypeB" },
        // { OpcodeIsEnc32TypeD, "Enc32TypeD" },
        // { OpcodeIsEnc32TypeE, "Enc32TypeE" },
        // { OpcodeIsEnc32TypeF, "Enc32TypeF" },
        // { OpcodeIsEnc32TypeG, "Enc32TypeG" },
        // { OpcodeIsEnc32TypeI, "Enc32TypeI" },
        // { OpcodeIsEnc32TypeJ, "Enc32TypeJ" },
        // { OpcodeIsSetReturn, "SetReturn" },
        // { OpcodeIsBARG, "BARG" },
        // { OpcodeIsBHINT, "BHINT" },
    };
    vector<string> results;
    results.push_back(GetInstGroupName(OpcodeManager::Inst().GetOpcodeGroup(opcode)));
    for (const auto& rule : funcTypes) {
        const auto& func = rule.first;
        const auto& typeStr = rule.second;
        if (func(opcode)) {
            results.push_back(typeStr);
        }
    }
    return results;
}

// block dump info helper functions

BlockDumpInfo& BlockDumpInfo::operator|=(const BlockDumpInfo& other)
{
    if (other.blockCmd) {
        blockCmd = other.blockCmd;
    }
    if (other.allocCycle > 0) {
        allocCycle = other.allocCycle;
    }
    if (other.commitCycle > 0) {
        commitCycle = other.commitCycle;
    }
    return *this;
}

PBlockDumpInfo& operator|=(PBlockDumpInfo& lhs, const PBlockDumpInfo& rhs)
{
    if (lhs && rhs) {
        *lhs |= *rhs;
    }
    return lhs;
}

// print helper functions

ostream& operator<<(ostream& os, const PBlockDumpInfo& obj)
{
    auto printRegByMask = [&os](uint32_t mask) {
        bool first = true;
        for (uint32_t i = 0; i < static_cast<uint32_t>(GPR::GPR_COUNT); i++) {
            if ((mask & (1U << i)) == 0) {
                continue;
            }
            if (!first) {
                os << ",";
            } else {
                first = false;
            }
            os << "R" << i;
        }
    };
    auto printBlockHeader = [&os, printRegByMask](const BlockCommandPtr& cmd) {
        os << hex;
        os << " BPC:0x" << setw(8) << cmd->bpc;
        os << " Block." << GetBlockTypeName(cmd->blockType);
        os << " Branch." << GetBlockBranchTypeName(cmd->branchType);
        if (!IsBlockTypeParallel(cmd->blockType)) {
            return;
        }
        if (IsBlockTypeNeedVReg(cmd->blockType)) {
            os << " " << GetVREGModeName(cmd->vregMode);
        }
        if (cmd->tileOp != TileOp::TINVALID) {
            os << " TileOp:" << dec << GetTileOpName(cmd->tileOp);
        }
        os << " LB0:" << dec << cmd->lb0;
        os << " LB1:" << dec << cmd->lb1;
        os << " LB2:" << dec << cmd->lb1;
        os << BriefDataType2String(cmd->dataType) << " ";
        if (cmd->blockAttr) {
            os << " " << dec << cmd->blockAttr->Dump();
        }
    };
    if (obj->blockCmd) {
        printBlockHeader(obj->blockCmd);
    }
    os << dec;
    os << " ALLOC:" << setw(8) << obj->allocCycle;
    os << " ISSUE:" << setw(8) << obj->brobIssueCycle;
    if (obj->flushCycle > 0) {
        os << " FLUSH:" << setw(8) << obj->flushCycle;
    } else {
        os << " COMMIT:" << setw(8) << obj->commitCycle;
    }

    os << hex;
    return os;
}

ostream& operator<<(ostream& os, const PInstDumpInfo& obj)
{
    os << dec << left;
    os << " PE:" << setw(6) << obj->peName;
    os << " Gid:" << setw(4) << obj->inst->gid;
    os << " Rid:" << setw(4) << obj->inst->rid;
    os << " T:" << setw(4) << obj->inst->tid;
    os << " F0:" << setw(8) << obj->inst->pipeCycle->f0Cycle;
    os << " F1:" << setw(8) << obj->inst->pipeCycle->f1Cycle;
    os << " F2:" << setw(8) << obj->inst->pipeCycle->f2Cycle;
    os << " F3:" << setw(8) << obj->inst->pipeCycle->f3Cycle;
    os << " D0:" << setw(8) << obj->inst->pipeCycle->d0Cycle;
    os << " D1:" << setw(8) << obj->inst->pipeCycle->decodeCycle;
    os << " D2:" << setw(8) << obj->inst->pipeCycle->renameCycle;
    os << " D3:" << setw(8) << obj->inst->pipeCycle->d3Cycle;
    os << " S1:" << setw(8) << obj->inst->pipeCycle->dispatchCycle;
    os << " P1:" << setw(8) << obj->inst->pipeCycle->p1Cycle;
    os << " I1:" << setw(8) << obj->inst->pipeCycle->i1Cycle;
    os << " I2:" << setw(8) << obj->inst->pipeCycle->i2Cycle;
    os << " E1:" << setw(8) << obj->inst->pipeCycle->e1Cycle;
    os << " W1:" << setw(8) << obj->inst->pipeCycle->w1Cycle;
    os << " R0:" << setw(8) << (obj->commitCycle + 1ULL);
    os << " FLUSH:" << setw(8) << obj->flushCycle;
    auto opcodeTypes = GetOpCodeTypeStrings(obj->inst->opcode);
    if (!opcodeTypes.empty()) {
        for (uint32_t idx = 0; idx < opcodeTypes.size(); idx++) {
            if (idx != 0) {
                os << ",";
            } else {
                os << " OPCODE:";
            }
            os << opcodeTypes[idx];
        }
    }
    if (OpcodeIsLoad(obj->inst->opcode)) {
        os << " GatherLd:" << setw(8) << obj->inst->gatherLd;
    } else if (OpcodeIsStore(obj->inst->opcode)) {
        os << " ScatterSt:" << setw(8) << obj->inst->scatterSt;
    }
    // if (obj->inst->cmd && obj->inst->cmd->blockAttr) {
    //     os << " BATTR:" << obj->inst->cmd->blockAttr->Dump();
    // }
    os << " " << obj->inst;
    return os;
}

ostream& operator<<(ostream& os, const PCubeInstDumpInfo& obj)
{
    os << dec << left;
    os << " PE:" << setw(6) << obj->peName;
    os << " SPLIT:" << setw(8) << obj->splitCycle;
    os << " FIFO:" << setw(8) << obj->fifoCycle;
    os << " REQ:" << setw(8) << obj->reqCycle;
    os << " READY:" << setw(8) << obj->readyCycle;
    os << " DONE:" << setw(8) << obj->doneCycle;
    os << " " << *(obj->inst);
    return os;
}

ostream& operator<<(ostream& os, const PMemDumpInfo& obj)
{
    os << dec << left;
    os << " RECV:" << setw(8) << obj->recvCycle;
    os << " DONE:" << setw(8) << obj->doneCycle;
    os << " " << obj->inst;
    return os;
}

// InstTracer implementation

InstTracer::InstTracer(SimSys* sim, Core* core, ostream* stream)
    : m_sim(sim), m_core(core), m_stream(stream) {}

InstTracer::~InstTracer() {}

void InstTracer::SetFunctionalState(bool enable)
{
    m_enable = enable;
}

bool InstTracer::IsEnabled() const
{
    return m_enable;
}

void InstTracer::Build() const {}

void InstTracer::Xfer()
{
    if (!m_enable) {
        return;
    }

    uint32_t commitInst = 0;
    for (auto& bid : m_completedBlocks) {
        if (m_blockInfo.find(bid) != m_blockInfo.cend()) {
            *m_stream << "[BLOCK] Bid:" << setw(4) << left << dec << bid.val;
            *m_stream << m_blockInfo[bid] << endl;
            (void)m_blockInfo.erase(bid);
        } else {
            *m_stream << "[BLOCK] Bid:" << setw(4) << left << dec << bid.val << " (Info not found)" << endl;
        }
        if (m_instInfo.find(bid) != m_instInfo.cend()) {
            for (auto& info : m_instInfo[bid]) {
                *m_stream << dec << left;
                *m_stream << "[INSTR] INST_UID:" << setw(8) << info->inst->uid;
                *m_stream << " Bid:" << setw(4) << bid.val;
                *m_stream << info << hex << endl;
                commitInst++;
            }
            (void)m_instInfo.erase(bid);
        }
        if (m_cubeInstInfo.find(bid) != m_cubeInstInfo.cend()) {
            for (auto& info : m_cubeInstInfo[bid]) {
                *m_stream << dec << left;
                *m_stream << "[INSTR] INST_UID:" << setw(8) << info->inst->uid;
                *m_stream << " Bid:" << setw(4) << bid.val;
                *m_stream << info << hex << endl;
                commitInst++;
            }
            (void)m_cubeInstInfo.erase(bid);
        }
    }
    m_completedBlocks.clear();

    if (commitInst > 0) {
        m_core->bctrl->blockROB.SetRetiredInstNum(commitInst);
    }
}

void InstTracer::Reset() const {}

void InstTracer::Push(ROBID bid, const PBlockDumpInfo& info)
{
    if (!m_enable) {
        return;
    }
    if (m_blockInfo.find(bid) == m_blockInfo.cend()) {
        m_blockInfo[bid] = make_shared<BlockDumpInfo>();
    }
    m_blockInfo[bid] |= info;
}

void InstTracer::Push(ROBID bid, const PInstDumpInfo& info)
{
    if (!m_enable) {
        return;
    }
    m_instInfo[bid].push_back(info);
}

void InstTracer::Push(ROBID bid, const PCubeInstDumpInfo& info)
{
    if (!m_enable) {
        return;
    }
    m_cubeInstInfo[bid].push_back(info);
}

void InstTracer::Push(uint64_t instUid, const PMemDumpInfo& info)
{
    if (!m_enable) {
        return;
    }
    m_memInfo[instUid].push_back(info);
}

void InstTracer::PushBlockEvent(ROBID bid, BlockEvent event)
{
    if (!m_enable) {
        return;
    }
    if (m_blockInfo.find(bid) == m_blockInfo.cend()) {
        m_blockInfo[bid] = make_shared<BlockDumpInfo>();
    }
    switch (event) {
        case BlockEvent::ALLOC:
            m_blockInfo[bid]->allocCycle = GetSim()->getCycles();
            break;
        case BlockEvent::BROB_ISSUE:
            m_blockInfo[bid]->brobIssueCycle = GetSim()->getCycles();
            break;
        case BlockEvent::MISPRED:
            m_completedBlocks.insert(bid);
            m_blockInfo[bid]->flushCycle = GetSim()->getCycles();
            break;
        case BlockEvent::COMMIT:
            m_completedBlocks.insert(bid);
            m_blockInfo[bid]->commitCycle = GetSim()->getCycles();
            break;
        default:
            break;
    }
}

void InstTracer::PushInstrEvent(ROBID bid, const SimInst& inst, InstrEvent event)
{
    if (!m_enable) {
        return;
    }
    if (m_instInfo.find(bid) == m_instInfo.cend()) {
        cout << __FUNCTION__ << ": BID not found: bid=" << dec << bid.val << hex << endl;
        return;
    }
    auto it = find_if(m_instInfo.at(bid).cbegin(), m_instInfo.at(bid).cend(), [&inst](const PInstDumpInfo& info) {
        return info->inst->uid == inst->uid;
    });
    if (it == m_instInfo.at(bid).cend()) {
        cout << __FUNCTION__ << ": INST not found: inst.uid=" << dec << inst->uid << hex << endl;
        return;
    }
    switch (event) {
        case InstrEvent::MISPRED:
            (*it)->flushCycle = GetSim()->getCycles();
            break;
        case InstrEvent::COMMIT:
            (*it)->commitCycle = GetSim()->getCycles();
            break;
        default:
            break;
    }
}

void InstTracer::PushInstrEvent(ROBID bid, const PCubeUop& inst, InstrEvent event)
{
    if (!m_enable) {
        return;
    }
    if (m_cubeInstInfo.find(bid) == m_cubeInstInfo.cend()) {
        cout << __FUNCTION__ << ": BID not found: bid=" << dec << bid.val << hex << endl;
        return;
    }
    auto it = find_if(m_cubeInstInfo.at(bid).cbegin(), m_cubeInstInfo.at(bid).cend(),
        [&inst](const PCubeInstDumpInfo& info) {
        return info->inst->globleUopIndex == inst->globleUopIndex;
    });
    if (it == m_cubeInstInfo.at(bid).cend()) {
        cout << __FUNCTION__ << ": INST not found: inst.uid=" << dec << inst->globleUopIndex << hex << endl;
        return;
    }
    switch (event) {
        case InstrEvent::SPLIT:
            (*it)->splitCycle = GetSim()->getCycles();
            break;
        case InstrEvent::FIFO:
            (*it)->fifoCycle = GetSim()->getCycles();
            break;
        case InstrEvent::REQ:
            (*it)->reqCycle = GetSim()->getCycles();
            break;
        case InstrEvent::READY:
            (*it)->readyCycle = GetSim()->getCycles();
            break;
        case InstrEvent::DONE:
            (*it)->doneCycle = GetSim()->getCycles();
            break;
        default:
            break;
    }
}

void InstTracer::PushMemEvent(uint64_t instUid, MemEvent event) const
{
    (void)instUid;
    (void)event;
}

SimSys* InstTracer::GetSim()
{
    return m_sim;
}

uint64_t InstTracer::GetNextUid()
{
    return nextUid.fetch_add(1);
}

uint64_t GetInstNextUid()
{
    return InstTracer::GetNextUid();
}

} // namespace JCore
