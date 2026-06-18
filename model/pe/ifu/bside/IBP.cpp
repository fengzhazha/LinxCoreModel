#include "pe/ifu/bside/IBP.h"

#include "bitset"
#include "core/Bus.h"
#include "pe/ifu/iside/pe_ifu.h"

namespace JCore {


using namespace std;

namespace PE_IFU {

IBP::IBP() {}
IBP::~IBP() {}

void IBP::Build()
{
    Reset();
    logLabel = "[IBP" + std::to_string(id);
}

void IBP::Reset()
{
    hyperBQ.clear();
    hyperIQ.clear();
    return;
}

void IBP::Report()
{
    stringstream s = stringstream();
    s << "Inst Predictor " << id;
    stats->report(s.str());
}

bool IBP::predict(uint64_t tpc, uint32_t instSize, uint32_t &mask, uint64_t &dst)
{
    if (configs->inst_bp_mode == static_cast<uint64_t>(BP_Mode::FULL_BP)) {
        return checkBranchInst(tpc, instSize, mask, dst);
    }
    if (configs->inst_bp_mode == static_cast<uint64_t>(BP_Mode::NO_BP)) {
        return noHistBP(tpc, instSize, mask, dst);
    }
    return true;
}

void IBP::update(uint64_t tpc, uint64_t dst, uint32_t instSize)
{
    if (configs->inst_bp_mode == static_cast<uint64_t>(BP_Mode::FULL_BP)) {
        reportBranchInst(tpc, dst, instSize);
    }
    if (configs->inst_bp_mode == static_cast<uint64_t>(BP_Mode::NO_BP)) {
        return noHistUpdate(tpc, dst, instSize);
    }
    return;
}

bool IBP::isHyperBlock(uint64_t bpc)
{
    auto it = find(hyperBQ.begin(), hyperBQ.end(), bpc);
    return (it!=hyperBQ.end());
}

void IBP::reportBlockHyper(uint64_t bpc)
{
    if (!isHyperBlock(bpc)) {
        hyperBQ.push_back(bpc);
    }
}

bool IBP::isBranchInst(uint64_t tpc)
{
    auto it = find_if(hyperIQ.begin(), hyperIQ.end(), [&](BranchEntry& e) {
        return tpc == e.src;
    });
    return (it!=hyperIQ.end());
}

bool IBP::checkBranchInst(uint64_t tpc, uint32_t instSize, uint32_t &mask, uint64_t &dst)
{
    LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel << " Predict] from tpc 0x" << hex << tpc << "mask "
        << dec << mask;
    for (uint32_t i = 0; i < mask; i += instSize) {
        auto it = find_if(hyperIQ.begin(), hyperIQ.end(), [&](BranchEntry& e) {
            return tpc + i == e.src;
        });
        if (it!=hyperIQ.end()) {
            stats->pred_count++;
            mask = i + instSize;
            dst = it->dst;
            LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel << " Predict] tpc 0x" << hex << (tpc + i)
                << "target 0x" << hex << dst;
            return true;
        }
    }
    return false;
}

void IBP::reportBranchInst(uint64_t src, uint64_t dst, uint32_t instSize)
{
    auto it = find_if(hyperIQ.begin(), hyperIQ.end(), [&](BranchEntry& e) {
        return src == e.src;
    });
    if (it!=hyperIQ.end()) {
        if (it->dst != dst) {
            // update branch destination
            stats->mis_count++;
            LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel << " Update] miss. tpc 0x" << hex << src
                << " target 0x" << hex << dst << " predict 0x" <<it->dst;
            it->dst = dst;
        } else {
            LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel << " Update] hit.  tpc 0x" << hex << src
                << " target 0x" << hex << dst;
            stats->corr_count++;
        }
    } else {
        // add new branch inst entry
        stats->nc_count++;
        BranchEntry e;
        e.src = src;
        e.dst = dst;
        hyperIQ.push_back(e);
    }
}

bool IBP::isTaken(PredictorState &state)
{
    if (state == W_T || state == S_T) {
        return true;
    }
    return false;
}

void IBP::updateState(PredictorState &state, bool upgrade)
{
    switch (state) {
        case S_NT:
            state = upgrade ? W_NT : S_NT;
            break;
        case W_NT:
            state = upgrade ? W_T : S_NT;
            break;
        case W_T:
            state = upgrade ? S_T : W_NT;
            break;
        case S_T:
            state = upgrade ? S_T : W_T;
            break;
        default:
            ASSERT(0 && "Error: undefined predictor state. \n");
            break;
    }
}

bool IBP::noHistBP(uint64_t tpc, uint32_t instSize, uint32_t &mask, uint64_t &dst)
{
    for (uint32_t i = 0; i < mask; i += instSize) {
        auto it = instMap.find(tpc + i);
        if (it != instMap.end()) {
            stats->pred_count++;
            const uint64_t num = 2;
            bool taken = isTaken(it->second.state);
            if (taken) {
                LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel << " Taken. tpc 0x" << hex
                    << (tpc + i) << " tgt 0x" << it->second.dst << " 2-bit counter " << bitset<num>(it->second.state);
            } else if (!taken) {
                LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel << " Not Taken. tpc 0x" << hex << (tpc + i)
                    << " tgt 0x" << it->second.dst << " 2-bit counter " << bitset<num>(it->second.state);
            }
            if (taken) {
                mask = i + instSize;
                dst = it->second.dst;
                return true;
            } else {
                break;
            }
        }
    }
    return false;
}

void IBP::noHistUpdate(uint64_t src, uint64_t dst, uint32_t instSize)
{
    auto it = instMap.find(src);
    if (it == instMap.end()) {
        stats->nc_count++;
        LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel << " Insert] bp Not Cover. tpc 0x" << hex << src
            << " target 0x" << hex << dst;
        PredictEntry entry;
        entry.state = (dst == (src + instSize)) ? W_NT : W_T;
        entry.dst = dst;
        instMap.insert(std::pair<uint64_t, PredictEntry>(src, entry));
        return;
    }

    /* State is Taken.
    * 1) dst != pred_dst. result taken. downgrade state. if still taken update destination.
    * 2) dst != pred_dst. result not taken. downgrade state.
    * 3) dst == pred dst. result taken.  upgrade state.
    * State is NOT Taken.
    * 1) dst == tpc + instSize. result not taken. downgrade state.
    * 2) dst != tpc + instSize. result taken. upgrade state. update destination.
    */
    const uint64_t num = 2;
    if (isTaken(it->second.state)) {
        if (dst != it->second.dst) {
            stats->mis_count++;
            LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel << " Update] miss. tpc 0x" << hex << src << " target 0x"
                << hex << dst << " predict 0x" << it->second.dst << " 2bit counter" <<bitset<num>(it->second.state);
            updateState(it->second.state, false);
            if (isTaken(it->second.state)) {
                it->second.dst = (dst == (src + instSize)) ? it->second.dst : dst;
            }
        } else {
            stats->corr_count++;
            updateState(it->second.state, true);
            LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel << " Update] bp Taken correct. tpc 0x" << hex << src
                << " predict 0x"<< hex << it->second.dst <<" 2bit counter" << bitset<num>(it->second.state);
        }
    } else {
        if (dst != (src + instSize)) {
            stats->mis_count++;
            LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel << " Update] miss. tpc 0x" << hex<< src << " target 0x"
                << hex << dst << " predict 0x" << it->second.dst << " 2bit counter" << bitset<num>(it->second.state);
            updateState(it->second.state, true);
            it->second.dst = dst;
        } else {
            stats->corr_count++;
            updateState(it->second.state, false);
            LOG_INFO_M(ifu->machineType, Stage::F3) << logLabel << " Update] bp Not Taken correct. tpc 0x" << hex << src
                << " predict 0x" << hex << it->second.dst << " 2bit counter" << bitset<num>(it->second.state);
        }
    }
}
}

} // namespace JCore
