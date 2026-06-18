#include "iex/iex_rf.h"

#include "../configs/stack_config.h"
#include "core/Core.h"
#include "iex/iex.h"

namespace JCore {


using namespace std;

void RegFileState::Build(CoreConfig &configs, uint64_t preg_count, uint64_t pe_count, ExecEngineTyp type)
{
    uint64_t localRegT = type == SCALAR_IEX ? configs.local_reg_t : configs.simt_local_reg_t;
    uint64_t localRegU = type == SCALAR_IEX ? configs.local_reg_u : configs.simt_local_reg_u;
    uint64_t localRegP = type == SCALAR_IEX ? 0 : configs.simt_local_reg_p;

    if (type == MEM_IEX) {
        localRegT = type == SCALAR_IEX ? configs.local_reg_t : configs.mtc_local_reg_t;
        localRegU = type == SCALAR_IEX ? configs.local_reg_u : configs.mtc_local_reg_u;
        localRegP = type == SCALAR_IEX ? 0 : configs.mtc_local_reg_p;
    }

    // Build physical register, both simt and scalar
    // global GPR
    glRegFile = vector<uint64_t>(preg_count);
    if (type == SIMT_IEX || static_cast<uint32_t>(type) >= static_cast<uint32_t>(ExecEngineTyp::IEX_NUM)) {
        lpRegFile = vector<uint64_t>(configs.vec_lpreg_count);
    } else if (type == MEM_IEX) {
        lpRegFile = vector<uint64_t>(configs.mtc_lpreg_count);
    }
    // local gpr, scalar reg for both simt and scalar
    uint32_t threadCnt = type == SCALAR_IEX ? configs.stdThreadCount : configs.threadCount;
    tRegFile = vector<vector<vector<uint64_t>>>(pe_count, vector<vector<uint64_t>>(threadCnt,
                                                                                   vector<uint64_t>(localRegT)));
    uRegFile = vector<vector<vector<uint64_t>>>(pe_count, vector<vector<uint64_t>>(threadCnt,
                                                                                   vector<uint64_t>(localRegU)));
    // SIMT only
    vrfRegFile = vector<uint64_t>(configs.vrf_preg_count * SINGLE_VRF_SIZE, 0);
    // pRegFile = vector<__uint128_t>(localRegP, -1);
    SIMTMask = vector<vector<vector<uint64_t>>>(pe_count, vector<vector<uint64_t>>(threadCnt,
                                                                            vector<uint64_t>(localRegP, -1UL)));
}

void RegFileState::Reset() {
}

void RegFile::Build(uint64_t preg_count, uint64_t pe_count)
{
    configs.overrideDefaultConfig(GetSim()->getCfgs());
    regFile.Build(top->core->configs, preg_count, pe_count, top->id);
    StackConfig sconfigs;
    sconfigs.overrideDefaultConfig(GetSim()->getCfgs());
    uint64_t sreg_count = sconfigs.sp_preg_count;
    regFile.stackRegFile = vector<uint64_t>(sreg_count);
}

void RegFile::Reset() {
    regFile.Reset();
}

void RegFile::Work() {
    readfile();
    writefile();
    bypass();
}

void RegFile::Xfer() {
    recieveWFReqFromRT();
}

void RegFile::recieveWFReqFromRT() {
    rf_wr_q.Work();
    while (!rf_wr_q.Empty()) {
        auto req = rf_wr_q.Read();
        if (!req.vld) {
            continue;
        }
        for (auto dst : req.dsts) {
            if (dst->type == OperandType::OPD_GREG) {
                setGPR(dst->ptag, dst->data);
            } else if (dst->type == OperandType::OPD_TLINK) {
                regFile.tRegFile[req.peid][req.tid][dst->ptag] = dst->data;
            } else if (dst->type == OperandType::OPD_ULINK) {
                regFile.uRegFile[req.peid][req.tid][dst->ptag] = dst->data;
            } else if (dst->IsVRFReg()) {
                regFile.vrfRegFile[dst->ptag * SINGLE_VRF_SIZE] = dst->data;
            } else if (dst->type == OperandType::OPD_PREDMASK) {
                regFile.SIMTMask[req.peid][req.tid][dst->ptag] = dst->data;
            }
        }
    }
}

void RegFile::flush(FlushBus &flushReq) {
    auto match = [&flushReq] (RFReqBus *port) -> bool {
        if (!port->vld) {
            return true;
        }
        if (flushReq.req.stid != port->stid) {
            return false;
        }
        if (flushReq.baseOnPE && flushReq.req.peID != port->peid) {
            return false;
        }
        if (flushReq.baseOnThread && flushReq.req.tid != port->tid) {
            return false;
        }
        return flushReq.baseOnBid ? LessEqual(flushReq.req.bid, port->bid):
            LessEqual(flushReq.req.bid, flushReq.req.rid, port->bid, port->rid);
    };
    for (auto &port : rf_rd_req) {
        if (match(port)) {
            port->Reset();
        }
    }
    for (auto &port : rf_wr_req) {
        if (match(port)) {
            port->Reset();
        }
    }
    for (auto &port : rf_ld_wr_req) {
        if (match(port)) {
            port->Reset();
        }
    }
    for (auto &port : rf_data_ret) {
        if (match(port)) {
            port->Reset();
        }
    }
    auto match_bus = [&flushReq] (RFReqBus bus) -> bool {
        if (!bus.vld) {
            return true;
        }
        if (flushReq.baseOnPE && flushReq.req.peID != bus.peid) {
            return false;
        }
        if (flushReq.baseOnThread && flushReq.req.tid != bus.tid) {
            return false;
        }
        return flushReq.baseOnBid ? LessEqual(flushReq.req.bid, bus.bid):
            LessEqual(flushReq.req.bid, flushReq.req.rid, bus.bid, bus.rid);
    };
    rf_wr_q.FlushIf(match_bus);
    if (flushReq.req.maskVld) {
        // TODO: Mask flush
        for (size_t i = 0; i < regFile.SIMTMask[flushReq.req.peID][flushReq.req.tid].size(); ++i) {
            regFile.SIMTMask[flushReq.req.peID][flushReq.req.tid][i] = flushReq.req.simtMask;
        }
    }
}

SimSys *RegFile::GetSim() {
    return top->GetSim();
}

void RegFile::receiveReq() {

}

void RegFile::setGPR(uint32_t ptag, uint64_t data) {
    if (top->id == SCALAR_IEX) {
        ASSERT(ptag < regFile.glRegFile.size());
        regFile.glRegFile[ptag] = data;
    } else if (top->core->IsVectorIex(top->machineType) || top->id == MEM_IEX) {
        ASSERT(ptag < regFile.lpRegFile.size());
        regFile.lpRegFile[ptag] = data;
    }
}

uint64_t RegFile::getGPR(uint32_t ptag) {
    if (top->id == SCALAR_IEX) {
        ASSERT(ptag < regFile.glRegFile.size());
        return regFile.glRegFile[ptag];
    }

    ASSERT(top->core->IsVectorIex(top->machineType) || top->id == MEM_IEX);
    ASSERT(ptag < regFile.lpRegFile.size());
    return regFile.lpRegFile[ptag];
}

void RegFile::readfile() {
    // global register
    auto getGLreg = [this](uint32_t ptag) {
        return this->getGPR(ptag);
    };

    auto getLocalScalarVecReg = [](bool vecVld, VecData &vecD, std::vector<uint64_t> &reg, uint32_t tag) {
        if (vecVld) {
            vecD.Set(reg[tag]);
        }
    };

    auto getLocalVecReg = [](bool vec_vld, VecData &vecD, std::vector<uint64_t> &reg, uint32_t tag) {
        if (vec_vld) {
            vecD.Set(reg, tag);
        }
    };
    auto getGprFunc = [](bool vld, VecData& vecData, uint64_t data) {
        if (vld) {
            vecData.Set(data);
        }
    };

    for (uint32_t i = 0; i < rf_rd_req.size(); i++) {
        RFReqBus *req = rf_rd_req[i];
        rf_data_ret[i]->Reset();
        if (!req->vld) {
            continue;
        }

        rf_data_ret[i]->vld = true;
        rf_data_ret[i]->peid = req->peid;
        rf_data_ret[i]->bid = req->bid;
        rf_data_ret[i]->gid = req->gid;
        rf_data_ret[i]->rid = req->rid;
        rf_data_ret[i]->tid = req->tid;
        auto &sreg = regFile.stackRegFile;
        auto &treg = regFile.tRegFile[req->peid][req->tid];
        auto &ureg = regFile.uRegFile[req->peid][req->tid];
        auto &vrfReg = regFile.vrfRegFile;
        auto &predreg = regFile.SIMTMask[req->peid][req->tid];
        for (auto src : req->srcs) {
            if (src->type == OperandType::STACK_POINTER) {
                src->dataVld = true;
                src->data = sreg[src->ptag];
            } else if (src->type == OperandType::OPD_GREG) {
                src->dataVld = true;
                src->data = getGLreg(src->ptag);
                getGprFunc(src->vecDataVld, src->vecData, src->data);
            } else if (src->type == OperandType::OPD_ZERO) {
                src->dataVld = true;
                src->data = 0;
            } else if (src->type == OperandType::OPD_TLINK) {
                ASSERT(src->ptag < treg.size());
                src->dataVld = true;
                src->data = treg[src->ptag];
                getLocalScalarVecReg(src->vecDataVld, src->vecData, treg, src->ptag);
            } else if (src->type == OperandType::OPD_ULINK) {
                src->dataVld = true;
                ASSERT(src->ptag < ureg.size());
                src->data = ureg[src->ptag];
                getLocalScalarVecReg(src->vecDataVld, src->vecData, ureg, src->ptag);
            } else if (src->IsVRFReg()) {
                src->dataVld = true;
                src->data = vrfReg[src->ptag * SINGLE_VRF_SIZE];
                getLocalVecReg(src->vecDataVld, src->vecData, vrfReg, src->ptag * SINGLE_VRF_SIZE);
            } else if (src->type == OperandType::OPD_PREDMASK) {
                src->dataVld = true;
                ASSERT(src->ptag < predreg.size());
                src->data = predreg[src->ptag];
                getLocalScalarVecReg(src->vecDataVld, src->vecData, predreg, src->ptag);
            }
            rf_data_ret[i]->srcs.emplace_back(src);
        }
    }

    if (req_vec_siex_q) {
        while (!req_vec_siex_q->Empty()) {
            LocalFreeInfo info = req_vec_siex_q->Read();
            if (!info.last) {
                info.data = getGPR(info.ptag);
                LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[BCC IEX]: Read reg. bid "
                    << info.bid << " offset " << info.offset << " data 0x" << info.data;
            }
            rsp_siex_vec_q->Write(info);
        }
    }

    if (gsReqVecSiexQ) {
        while (!gsReqVecSiexQ->Empty()) {
            LocalFreeInfo info = gsReqVecSiexQ->Read();
            if (!info.last) {
                info.data = getGPR(info.ptag);
                LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[BCC IEX]: Read reg. bid "
                    << info.bid << " offset " << info.offset << " data 0x" << info.data;
            }
            gsRspSiexVecQ->Write(info);
        }
    }

    // send to mtc core
    if (mtc_req_vec_siex_q) {
        while (!mtc_req_vec_siex_q->Empty()) {
            LocalFreeInfo info = mtc_req_vec_siex_q->Read();
            if (!info.last) {
                info.data = getGPR(info.ptag);
                LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[BCC IEX]: Read reg. bid "
                    << info.bid << " offset " << info.offset << " data 0x" << info.data;
            }
            mtc_rsp_siex_vec_q->Write(info);
        }
    }
}

void RegFile::writefile() {
    auto setGLreg = [this](uint32_t ptag, POperandPtr &dst) {
        ASSERT(ptag <= top->core->configs.ggpr_count && "The value of ptag is greater than the max value!");
        setGPR(ptag, dst->data);
    };

    auto setLocalVecReg = [](bool vec_vld, VecData &vecD, std::vector<uint64_t> &reg, uint32_t tag) {
        if (vec_vld) {
            vecD.write(reg, tag);
        }
    };

    auto writeReg = [this, setGLreg, setLocalVecReg](const std::vector<RFReqBus*>& wr_req) {
        for (uint32_t i = 0; i < wr_req.size(); i++) {
            RFReqBus *req = wr_req[i];
            if (!req->vld) {
                continue;
            }
            auto &treg = regFile.tRegFile[req->peid][req->tid];
            auto &ureg = regFile.uRegFile[req->peid][req->tid];
            auto &vrfReg = regFile.vrfRegFile;
            auto &predreg = regFile.SIMTMask[req->peid][req->tid];
            auto &sreg = regFile.stackRegFile;
            for (auto dst : req->dsts) {
                if (dst->type == OperandType::OPD_GREG) {
                    setGLreg(dst->ptag, dst);
                } else if (dst->type == OperandType::STACK_POINTER) {
                    sreg[dst->ptag] = dst->data;
                } else if (dst->type == OperandType::OPD_TLINK) {
                    treg[dst->ptag] = dst->data;
                    setLocalVecReg(dst->vecDataVld, dst->vecData, treg, dst->ptag);
                } else if (dst->type == OperandType::OPD_ULINK) {
                    ureg[dst->ptag] = dst->data;
                    setLocalVecReg(dst->vecDataVld, dst->vecData, ureg, dst->ptag);
                } else if (dst->IsVRFReg()) {
                    vrfReg[dst->ptag * SINGLE_VRF_SIZE] = dst->data;
                    setLocalVecReg(dst->vecDataVld, dst->vecData, vrfReg, dst->ptag * SINGLE_VRF_SIZE);
                } else if (dst->type == OperandType::OPD_PREDMASK) {
                    predreg[dst->ptag] = dst->data;
                    setLocalVecReg(dst->vecDataVld, dst->vecData, predreg, dst->ptag);
                }
            }
        }
    };

    writeReg(rf_wr_req);
    writeReg(rf_ld_wr_req);
    for (auto &peq : pe_rf_wr_req) {
        while (!peq->Empty()) {
            auto req = peq->Read();
            for (auto dst : req.dsts) {
                if (dst->type == OperandType::OPD_PREDMASK) {
                    regFile.SIMTMask[req.peid][req.tid][dst->ptag] = dst->data;
                }
            }
        }
    }

    if (data_vec_viex_q != nullptr) {
        while (!data_vec_viex_q->Empty()) {
            LocalFreeInfo info = data_vec_viex_q->Read();
            if (!info.last) {
                LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[Vector IEX]: Write reg. bid "
                    << info.bid << " offset " << info.offset << " data 0x" << info.data;
                setGPR(info.ptag, info.data);
            } else {
                data_viex_vec_q->Write(info);
            }
        }
    }

    if (gsDataVecViexQ != nullptr) {
        while (!gsDataVecViexQ->Empty()) {
            LocalFreeInfo info = gsDataVecViexQ->Read();
            if (!info.last) {
                LOG_INFO_M(Unit::VECTOR, Stage::NA) << "[Vector IEX]: Write reg. bid "
                    << info.bid << " offset " << info.offset << " data 0x" << info.data;
                setGPR(info.ptag, info.data);
            } else {
                gsDataViexVecQ->Write(info);
            }
        }
    }
}

void RegFile::BypassReqW2R(RFReqBus* wReq, RFReqBus* rReq, bool loadBypass)
{
    if (wReq->tid != rReq->tid) {
        return;
    }
    auto checkOperandMatch = [&wReq, &rReq](POperandPtr const& dst, POperandPtr const& src)->bool {
        if (src->type != dst->type || src->ptag != dst->ptag) {
            return false;
        }
        if (src->IsLocalReg() && (wReq->bid != rReq->bid || wReq->gid != rReq->gid
            || wReq->peid != rReq->peid || wReq->tid != rReq->tid)) {
            return false;
        }
        return true;
    };
    for (auto dst : wReq->dsts) {
        for (auto src : rReq->srcs) {
            if (!src->vecData.CheckCopyRight(dst->vecData)) {
                continue;
            }
            bool maskBypss = loadBypass ? (rReq->bid == wReq->bid && rReq->gid == wReq->gid && rReq->peid == wReq->peid) : true;
            if (src->type == dst->type && src->type == OperandType::OPD_PREDMASK && dst->dataVld && maskBypss) {
                src->data = dst->data;
            }
            if (!checkOperandMatch(dst, src)) {
                continue;
            }
            src->dataVld = true;
            src->data = dst->data;
            if (src->vecDataVld) {
                if (!dst->vecDataVld) {
                    src->vecData.Set(dst->data);
                } else {
                    ASSERT(src->vecData.CheckCopyRight(dst->vecData));
                    src->vecData = dst->vecData;
                }
            }
        }
    }
}

void RegFile::bypass()
{
    for (auto &w_req : rf_wr_req) {
        if (!w_req->vld) {
            continue;
        }
        for (auto &r_req : rf_data_ret) {
            if (!r_req->vld) {
                continue;
            }
            BypassReqW2R(w_req, r_req);
        }
    }

    for (auto &w_req : rf_ld_wr_req) {
        if (!w_req->vld) {
            continue;
        }
        for (auto &r_req : rf_data_ret) {
            if (!r_req->vld) {
                continue;
            }
            BypassReqW2R(w_req, r_req, true);
        }
    }
}

void RegFile::returnData() {
}

} // namespace JCore
