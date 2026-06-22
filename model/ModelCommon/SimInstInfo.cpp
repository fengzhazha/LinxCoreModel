#include "SimInstInfo.h"

#include <string>

#include "SimSys.h"
#include "bus/MemReqBus.h"
#include "bctrl/bfu/bfu_common.h"

namespace JCore {
using namespace std;
seq_t BFUMachineInfo::hid_global = 0;

BFUMachineInfo::BFUMachineInfo(uint64_t cycles) :
    hid(BFUMachineInfo::hid_global++), fbid(0), pc(0), spInfo(nullptr), vld(true), create_time(cycles),
        fetch_time(cycles), f1_time(cycles), bhc_fetch_time(cycles), ib_time(cycles) {
};

BFUMachineInfo::BFUMachineInfo(const BFUMachineInfo& minfo) : global(minfo.global),
                                                              hid(minfo.hid),
                                                              fbid(minfo.fbid),
                                                              fbid_local(minfo.fbid_local),
                                                              pc(minfo.pc),
                                                              spInfo(nullptr),
                                                              vld(minfo.vld),
                                                              concat(minfo.concat),
                                                              fetch_end(minfo.fetch_end),
                                                              needBstop(minfo.needBstop),
                                                              nuke_after_redirect(minfo.nuke_after_redirect),
                                                              create_time(minfo.create_time),
                                                              fetch_time(minfo.fetch_time),
                                                              f1_time(minfo.f1_time),
                                                              bhc_fetch_time(minfo.bhc_fetch_time),
                                                              ib_time(minfo.ib_time),
                                                              resolve_time(minfo.resolve_time),
                                                              commit_time(minfo.commit_time),
                                                              after_branch(minfo.after_branch),
                                                              predict_tgt(minfo.predict_tgt),
                                                              predict_taken(minfo.predict_taken),
                                                              first_after_redirect(minfo.first_after_redirect),
                                                              first_after_nuke(minfo.first_after_nuke),
                                                              resolved(minfo.resolved),
                                                              resolve_taken(minfo.resolve_taken),
                                                              resolve_mispredict(minfo.resolve_mispredict),
                                                              resolve_tgt(minfo.resolve_tgt),
                                                              committed(minfo.committed),
                                                              predict_at_once(minfo.predict_at_once),
                                                              predict_forward(minfo.predict_forward)
{
    if (minfo.spInfo) {
        auto &sp = minfo.spInfo;
        spInfo = std::make_shared<SPMInstInfo>();
        spInfo->isInst = sp->isInst;
        spInfo->bsize = sp->bsize;
        spInfo->hsize = sp->hsize;
        spInfo->isInst = sp->isInst;
        spInfo->afterBranch = sp->afterBranch;
        spInfo->cut = sp->cut;
        spInfo->isBstart = sp->isBstart;
        spInfo->hasBend = sp->hasBend;
    }
}

BFUMachineInfo &BFUMachineInfo::operator=(const BFUMachineInfo& minfo)
{
    if (this == &minfo) {
        return *this;
    }
    global = minfo.global;
    hid = minfo.hid;
    fbid = minfo.fbid;
    fbid_local = minfo.fbid_local;
    pc = minfo.pc;
    spInfo = nullptr;
    vld = minfo.vld;
    concat = minfo.concat;
    fetch_end = minfo.fetch_end;
    needBstop = minfo.needBstop;
    nuke_after_redirect = minfo.nuke_after_redirect;
    create_time = minfo.create_time;
    fetch_time = minfo.fetch_time;
    f1_time = minfo.f1_time;
    bhc_fetch_time = minfo.bhc_fetch_time;
    ib_time = minfo.ib_time;
    resolve_time = minfo.resolve_time;
    commit_time = minfo.commit_time;
    after_branch = minfo.after_branch;
    predict_tgt = minfo.predict_tgt;
    predict_taken = minfo.predict_taken;
    first_after_redirect = minfo.first_after_redirect;
    first_after_nuke = minfo.first_after_nuke;
    resolved = minfo.resolved;
    resolve_taken = minfo.resolve_taken;
    resolve_mispredict = minfo.resolve_mispredict;
    resolve_tgt = minfo.resolve_tgt;
    committed = minfo.committed;
    predict_at_once = minfo.predict_at_once;
    predict_forward = minfo.predict_forward;
    if (minfo.spInfo) {
        auto &sp = minfo.spInfo;
        spInfo = std::make_shared<SPMInstInfo>();
        spInfo->isInst = sp->isInst;
        spInfo->bsize = sp->bsize;
        spInfo->hsize = sp->hsize;
        spInfo->isInst = sp->isInst;
        spInfo->afterBranch = sp->afterBranch;
        spInfo->cut = sp->cut;
        spInfo->isBstart = sp->isBstart;
        spInfo->hasBend = sp->hasBend;
    }
    return *this;
}

BFUMachineInfo::BFUMachineInfo(seq_t fbid, addr_t pc, SPInfoPtr sp, time_t create_time, time_t fetch_time,
    time_t f1_time, time_t bhc_fetch_time, time_t ib_time) :
    hid(BFUMachineInfo::hid_global++), fbid(fbid), pc(pc), spInfo(nullptr), vld(true), create_time(create_time),
        fetch_time(fetch_time), f1_time(f1_time), bhc_fetch_time(bhc_fetch_time), ib_time(ib_time) {
    if (sp) {
        // spInfo = std::make_shared<SPMInstInfo>(*sp);
        spInfo = std::make_shared<SPMInstInfo>();
        spInfo->isInst = sp->isInst;
        spInfo->bsize = sp->bsize;
        spInfo->hsize = sp->hsize;
        spInfo->isInst = sp->isInst;
        spInfo->afterBranch = sp->afterBranch;
        spInfo->cut = sp->cut;
        spInfo->isBstart = sp->isBstart;
        spInfo->hasBend = sp->hasBend;
    }
};

PhysicalOperand::PhysicalOperand(Operand &opd) : Operand(opd)
{
    renamed = false;
    ready = false;
    ptag = 0;
    if (!isDst) {
        ready = OperandTypeDefaultReady(type);
    }
}

PhysicalOperand::PhysicalOperand(OperandPtr &ptr) : PhysicalOperand(*ptr)
{
}

PhysicalOperand::PhysicalOperand(OperandType typ, uint64_t val) : Operand(typ, val)
{
    renamed = false;
    ready = false;
    ptag = 0;
    if (!isDst) {
        ready = OperandTypeDefaultReady(type);
    }
}

bool PhysicalOperand::IsSP()
{
    return type == OperandType::OPD_GREG && value == static_cast<uint64_t>(GPR::GPR_SP);
}

std::string PhysicalOperand::Dump(uint64_t idx)
{
    std::stringstream oss;
    oss << (isDst ? "dst" : "src") << std::dec << idx << ":";
    oss << Operand::Assemble(true);
    if (OperandTypeNeedRename(type) || OperandTypeIsTile(type)) {
        if (renamed) {
            oss << "-P(" << std::dec << ptag << ")";
            if (!isDst) {
                oss << (ready ? "-rdy" : "-not_rdy");
            }
        } else {
            oss << "(!renamed)";
        }
    }
    if (dataVld) {
        oss << " data=0x" << std::hex << data;
    }
    if (vecDataVld) {
        oss << ", width=" << dec << vecData.width;
        oss << ", vecData_data_size="  << vecData.data.size() << ", vecData=(";
        for (uint32_t i = 0; i < vecData.data.size(); ++i) {
            oss << left << "i:" << dec << i << ", 0x" << hex << vecData.data[i] << " ";
        }
        oss << ")";
    }
    return oss.str();
}

uint64_t PhysicalOperand::GetAllocRegSize(uint64_t lanes)
{
    switch (type) {
        case OperandType::OPD_VTLINK:
        case OperandType::OPD_VULINK:
        case OperandType::OPD_VMLINK:
        case OperandType::OPD_VNLINK: {
            uint64_t opdSize = vecDataVld ?  std::ceil((vecData.width * lanes * 1.0f) / REG_SIZE_BIT) : 1;
            if (opdSize < 1) {
                opdSize = 1;
            }
            if (opdSize > NUM32 && opdSize < NUM64) {
                opdSize = NUM64;
            }
            return opdSize;
        }
        default:
            return 1;
    }
}

bool PhysicalOperand::CanBypass(uint64_t pickTime) const
{
    if (wakeupTime == -1ULL) {
        return false;
    }
    const uint32_t bypassInterval = 3;
    if (pickTime <= (wakeupTime + bypassInterval)) {
        return true;
    }
    return false;
}

SimInstInfo::SimInstInfo() : MInst()
{
    pipeCycle = std::make_shared<CycleInfo>();
}

SimInstInfo::SimInstInfo(const CycleBus& bundleCycleInfo) : MInst()
{
    pipeCycle = std::make_shared<CycleInfo>(*bundleCycleInfo);
}

void SimInstInfo::Decode(uint64_t tpc, uint64_t bin, uint32_t sizeByte, bool &is64Dst, uint32_t laneNum)
{
    DecodeBin(bin, sizeByte);
    if (codeLen == EncodeLen::ENL_V) {
        for (auto &dst : dsts) {
            if (dst->width == OperandWidth::OPDW_D) {
                is64Dst = true;
                dst->width = OperandWidth::OPDW_W;
            }
        }
    }
    pc = tpc;
    ConvertPOperand(laneNum);
}

void SimInstInfo::ConvertPOperand(uint32_t laneNum)
{
    psrcs_.clear();
    pdsts_.clear();
    for (size_t i = 0; i < srcs.size(); i++) {
        psrcs_.emplace_back(std::make_shared<PhysicalOperand>(*srcs[i]));
        srcs[i] = psrcs_[i];
    }
    for (size_t i = 0; i < dsts.size(); i++) {
        pdsts_.emplace_back(std::make_shared<PhysicalOperand>(*dsts[i]));
        dsts[i] = pdsts_[i];
    }
    if (codeLen == EncodeLen::ENL_V) {
        for (auto &psrc : psrcs_) {
            if (OperandTypeNeedVecData(psrc->type)) {
                psrc->InitVecData(static_cast<uint32_t>(psrc->width), laneNum);
            }
        }
        for (auto &pdst : pdsts_) {
            pdst->InitVecData(static_cast<uint32_t>(pdst->width), laneNum);
        }
        if (accMemInfo) {
            accMemInfo->vecDataVld = true;
            accMemInfo->vecData.Init(static_cast<uint32_t>(OperandWidth::OPDW_D), laneNum);
        }
    }
}

std::vector<POperandPtr>& SimInstInfo::GetrcArray()
{
    return psrcs_;
}

std::vector<POperandPtr>& SimInstInfo::GetPDstArray()
{
    return pdsts_;
}
POperandPtr SimInstInfo::GetPSrcPtr(size_t index) const
{
    if (index >= psrcs_.size()) {
        return nullptr;
    }
    return psrcs_[index];
}

POperandPtr SimInstInfo::GetPDstPtr(size_t index) const
{
    if (index >= pdsts_.size()) {
        return nullptr;
    }
    return pdsts_[index];
}

POperandPtr SimInstInfo::GetPSrcPtrByType(OperandType type)
{
    for (auto psrc : psrcs_) {
        if (psrc->type == type) {
            return psrc;
        }
    }
    return nullptr;
}

POperandPtr SimInstInfo::GetPDstPtrByType(OperandType type)
{
    for (auto pdst : pdsts_) {
        if (pdst->type == type) {
            return pdst;
        }
    }
    return nullptr;
}

size_t SimInstInfo::GetSrcIdxByType(OperandType type)
{
    for (size_t i = 0; i < psrcs_.size(); ++i) {
        if (psrcs_[i]->type == type) {
            return i;
        }
    }
    return -1U;
}

size_t SimInstInfo::GetDstIdxByType(OperandType type)
{
    for (size_t i = 0; i < pdsts_.size(); ++i) {
        if (pdsts_[i]->type == type) {
            return i;
        }
    }
    return -1U;
}

OperandType SimInstInfo::GetDstTileType()
{
    for (auto &dst : pdsts_) {
        return dst->type;
    }
    return OperandType::OPD_INVALID;
}

uint64_t SimInstInfo::GetDstTileSize()
{
    uint64_t size = 0;
    for (auto &dst : pdsts_) {
        size += dst->size;
    }
    return size;
}

POperandPtr SimInstInfo::GetFirstValidTileSrc() const
{
    for (auto &psrc : psrcs_) {
        if (psrc->type != OperandType::OPD_TILE_ACC && OperandTypeIsTile(psrc->type)) {
            return psrc;
        }
    }
    return nullptr;
}

bool SimInstInfo::IsReady()
{
    for (auto &psrc : psrcs_) {
        if ((OperandTypeNeedRename(psrc->type) && !psrc->ready) || OperandTypeIsAlwaysNotReady(psrc->type)) {
            return false;
        }
    }
    for (auto &pdst : pdsts_) {
        if (OperandTypeNeedRename(pdst->type) && !pdst->renamed) {
            return false;
        }
    }
    return true;
}

bool SimInstInfo::CheckCancel(uint32_t mask)
{
    for (auto psrc : psrcs_) {
        if (psrc->lpvInfo != nullptr && psrc->lpvInfo->CheckCancel(mask)) {
            return true;
        }
    }
    return false;
}

bool SimInstInfo::CheckNoCancel()
{
    for (auto src : psrcs_) {
        if (src->type == OperandType::OPD_GREG && src->lpvInfo != nullptr && src->lpvInfo->notEmpty) {
            return false;
        }
    }
    return true;
}

bool SimInstInfo::IsSrcGPRRenamed()
{
    bool renamed = true;
    for (auto &psrc : psrcs_) {
        if (psrc->type == OperandType::OPD_GREG && !psrc->renamed) {
            return false;
        }
    }
    return renamed;
}
bool SimInstInfo::IsDstGPRRenamed()
{
    bool renamed = true;
    for (auto &pdst : pdsts_) {
        if (pdst->type == OperandType::OPD_GREG && !pdst->renamed) {
            return false;
        }
    }
    return renamed;
}

bool SimInstInfo::Execute(uint32_t lane, uint64_t tpc)
{
    PrePareSrc(lane, tpc);
    bool ret = Calculate();
    ProcessDst(lane, tpc);
    if (opcode == Opcode::OP_ACRC) { // 目前ACRC 作为程序结束。
        terminate = true;
    }
    return ret;
}
void SimInstInfo::PrePareSrc(uint32_t lane, uint64_t tpc)
{
    (void)tpc;
    bool multiLane = codeLen == EncodeLen::ENL_V;
    for (auto &psrc : psrcs_) {
        if (multiLane && OperandTypeNeedVecData(psrc->type)) {
            psrc->data = psrc->vecData.Get(lane);
        }
        if (multiLane && (psrc->type == OperandType::OPD_TLINK || psrc->type == OperandType::OPD_ULINK ||
            psrc->type == OperandType::OPD_RI)) {
            // VCALL, src是标量寄存器，src值与lane id无关
            if (lane == 0) {
                GetSrcData(psrc);
            }
            continue;
        }
        GetSrcData(psrc);
    }
}

void SimInstInfo::ProcessDst(uint32_t lane, uint64_t tpc)
{
    (void)tpc;
    bool multiLane = codeLen == EncodeLen::ENL_V;
    for (auto &pdst : pdsts_) {
        if (multiLane && OperandTypeNeedVecData(pdst->type)) {
            pdst->vecData.Set(pdst->data, lane);
        } else {
            pdst->dataVld = true;
        }
    }
    if (accMemInfo != nullptr && multiLane) {
        accMemInfo->vecData.Set(accMemInfo->accMemAddr, lane, static_cast<uint32_t>(OperandWidth::OPDW_D));
    }
}

void SimInstInfo::GetSrcData(POperandPtr &src)
{
    switch (src->type) {
        case OperandType::OPD_ZERO:
            src->data = 0;
            break;
        case OperandType::OPD_SIMM:
            src->data = static_cast<int64_t>(src->value);
            break;
        case OperandType::OPD_UIMM:
            src->data = src->value;
            break;
        default:
            break;
    }
    OperandCvtVal(src);
}

bool SimInstInfo::Calculate()
{
    return JCore::Calculate::MInstCalculator::Inst().CalculateMinst(*this);
}

bool SimInstInfo::SrcTypeContain(OperandType type)
{
    bool res = false;
    for (auto &src : psrcs_) {
        if (src->type == type) {
            res = true;
            break;
        }
    }
    return res;
}

bool SimInstInfo::DstTypeContain(OperandType type)
{
    bool res = false;
    for (auto &dst : pdsts_) {
        if (dst->type == type) {
            res = true;
            break;
        }
    }
    return res;
}

bool SimInstInfo::SrcContainSP()
{
    bool res = false;
    for (auto &src : psrcs_) {
        if (src->type == OperandType::OPD_GREG && src->value == static_cast<uint64_t>(GPR::GPR_SP)) {
            res = true;
            break;
        }
    }
    return res;
}

bool SimInstInfo::DstContainSP()
{
    bool res = false;
    for (auto &dst : pdsts_) {
        if (dst->type == OperandType::OPD_GREG && dst->value == static_cast<uint64_t>(GPR::GPR_SP)) {
            res = true;
            break;
        }
    }
    return res;
}

void SimInstInfo::MoveLpv()
{
    for (auto &psrc : psrcs_) {
        if (psrc->lpvInfo) {
            psrc->lpvInfo->Move();
        }
    }
}

PLpvInfo SimInstInfo::GetLpv()
{
    PLpvInfo info = std::make_shared<LpvInfo>();
    for (uint32_t i = 0; i < LPV_SIZE; ++i) {
        for (auto &psrc : psrcs_) {
            info->lpv[i] |= (psrc->lpvInfo) ? psrc->lpvInfo->lpv[i] : 0;
        }
        info->notEmpty |= (info->lpv[i] != 0);
    }

    if (!info->notEmpty) {
        PLpvInfo plpv;
        return plpv;
    }

    return info;
}

BranchType SimInstInfo::GetBranchType()
{
    return bcmd == nullptr ? BranchType::BLK_BR_FALL : bcmd->branchType;
}

bool SimInstInfo::IsFallthrough()
{
    return GetBranchType() == BranchType::BLK_BR_FALL;
}

bool SimInstInfo::IsCond()
{
    return GetBranchType() == BranchType::BLK_BR_COND;
}

bool SimInstInfo::IsDirect()
{
    return GetBranchType() == BranchType::BLK_BR_DIRECT || GetBranchType() == BranchType::BLK_BR_CALL;
}

bool SimInstInfo::IsIndirect()
{
    return GetBranchType() == BranchType::BLK_BR_IND || GetBranchType() == BranchType::BLK_BR_ICALL;
}

bool SimInstInfo::IsCall()
{
    return GetBranchType() == BranchType::BLK_BR_CALL || GetBranchType() == BranchType::BLK_BR_ICALL;
}

bool SimInstInfo::IsReturn()
{
    return GetBranchType() == BranchType::BLK_BR_RET;
}

bool SimInstInfo::IsTerminateFlag()
{
    constexpr uint64_t ARCR_END_FLAG = 3;
    return opcode == Opcode::OP_ACRC && srcs[SRC0_IDX]->data == ARCR_END_FLAG;
}

bool SimInstInfo::IsSetRet()
{
    return opcode == Opcode::OP_SETRET;
}

uint64_t SimInstInfo::GetSetRetDst()
{
    int64_t imm = 0;
    uint64_t mask = 0xffffffffffffffff;
    if (srcs[SRC0_IDX]->cvt == JCore::OPConvertType::OPCVT_U32
        || srcs[SRC0_IDX]->cvt == JCore::OPConvertType::OPCVT_S32) {
        mask = (1ULL << 32) - 1;
    } else if (srcs[SRC0_IDX]->cvt == JCore::OPConvertType::OPCVT_U16
        || srcs[SRC0_IDX]->cvt == JCore::OPConvertType::OPCVT_S16) {
        mask = (1ULL << 16) - 1;
    } else if (srcs[SRC0_IDX]->cvt == JCore::OPConvertType::OPCVT_U8
        || srcs[SRC0_IDX]->cvt == JCore::OPConvertType::OPCVT_S8) {
        mask = (1ULL << 8) - 1;
    }
    imm = static_cast<int64_t>(GetCvtVal(srcs[SRC0_IDX]->cvt,
          srcs[SRC0_IDX]->shamt, srcs[SRC0_IDX]->value & mask));
    return static_cast<uint64_t>(pc + imm);
}

uint64_t SimInstInfo::GetBundlePosPC()
{
    return (pc + GetEncodeLenVal(codeLen) - JCore::NS_CORE::MIN_BUNDLE_SIZE);
}

RFReqBus SimInstInfo::GenRFReqBus(bool isRead)
{
    RFReqBus req;
    req.peid = peID;
    req.coreId = coreID;
    req.bid = bid;
    req.gid = gid;
    req.rid = rid;
    req.tid = tid;
    req.stid = stid;
    req.vld = true;
    // TODO: replace new srcs/dsts bus in RFReqBus
    // Read
    if (isRead) {
        for (auto src : psrcs_) {
            auto reqSrc = std::make_shared<PhysicalOperand>(*src);
            req.srcs.emplace_back(reqSrc);
        }
        return req;
    }

    // Write
    for (auto dst : pdsts_) {
        req.dsts.emplace_back(std::make_shared<PhysicalOperand>(*dst));
    }
    return req;
}

void SimInstInfo::RFRetSetData(RFReqBus const& rfRet)
{
    if (!rfRet.vld) {
        return;
    }
    ASSERT(rfRet.srcs.size() == psrcs_.size());
    for (size_t i = 0; i < rfRet.srcs.size(); ++i) {
        if (rfRet.srcs[i]->dataVld && !psrcs_[i]->dataVld) {
            ASSERT(psrcs_[i]->type == psrcs_[i]->type);
            psrcs_[i]->dataVld = true;
            psrcs_[i]->data = rfRet.srcs[i]->data;
            if (psrcs_[i]->type == OperandType::OPD_PREDMASK) {
                psrcs_[i]->data &= predMask;
                if (OpcodeIsDirectInnerJump(opcode)) {
                    psrcs_[i]->data = 1;
                }
            }
            if (psrcs_[i]->vecDataVld) {
                ASSERT(psrcs_[i]->vecData.CheckCopyRight(rfRet.srcs[i]->vecData));
                psrcs_[i]->vecData = rfRet.srcs[i]->vecData;
            }
        }
    }
}

RFReqBus SimInstInfo::GenSYSReqBus()
{
    RFReqBus req;
    req.Reset();
    req.gid = gid;
    req.tid = tid;
    req.bid = bid;
    req.rid = rid;
    req.peid = peID;
    req.coreId = coreID;
    req.stid = stid;

    for (auto &psrc : psrcs_) {
        if (psrc->type == OperandType::OPD_SYS) {
            req.vld = true;
            req.srcs.push_back(std::make_shared<PhysicalOperand>(*psrc));
        }
    }

    return req;
}

void SimInstInfo::RecSYSReqRet(RFReqBus const& sysRet) {
    // For simt
    if (!sysRet.vld) {
        return;
    }

    size_t retIdx = 0;
    for (auto src : psrcs_) {
        if (src->type == OperandType::OPD_SYS) {
            src->dataVld = true;
            src->data = sysRet.srcs[retIdx]->data;
        }
    }

    // // only FP insts need to receive this request
    // if (fpmSrc.sys_tag_vld && sysRet.fpmSrc.sys_tag_vld) {
    //     ASSERT(OpcodeIsFp(opcode) && "Non-FP insts cannot get FSSR register!");
    //     fpmSrc.data = sysRet.fpmSrc.data;
    // }
}

RFReqBus SimInstInfo::GenSYSWFBus()
{
    RFReqBus req;
    req.gid = gid;
    req.bid = bid;
    req.tid = tid;
    req.rid = rid;
    req.peid = peID;
    req.coreId = coreID;
    req.stid = stid;
    for (auto &pdst : pdsts_) {
        if (pdst->type == OperandType::OPD_SYS && pdst->dataVld) {
            req.vld = true;
            req.dsts.push_back(std::make_shared<PhysicalOperand>(*pdst));
        }
    }
    return req;
}

MemReqBus SimInstInfo::GenMemReq(uint32_t peCount, uint32_t lane) {
    MemReqBus memReq;
    memReq.peID = peID;
    memReq.coreId = coreID;
    memReq.toMtcLsu = (peID >= peCount);
    memReq.iq_name = this->iq_name;
    memReq.fbid = bfuInfo ? bfuInfo->fbid : 0;
    memReq.stid = this->stid;
    if (this->psrcs_.size() > SRC1_IDX) {
        memReq.src1 = this->psrcs_[SRC1_IDX];
    }
    memReq.fbid_local = bfuInfo ? bfuInfo->fbid_local : 0;
    memReq.iexTyp = iexType;
    // memReq.noSplitBlk = noSplitBlk;
    memReq.tSeq = tSeq;
    memReq.uSeq = uSeq;
    memReq.predSeq = predSeq;
    // TODO: for scalar(BCC), we use stid; but in vector core, we use tid, may needs to find a way to separate these
    memReq.tid = stid;
    memReq.subrid = subrid;

    // Service non-speculative load/store first
    if (accMemInfo) {
        memReq.vld = true;
        memReq.opcode = opcode;
        memReq.is_load = OpcodeIsLoad(memReq.opcode);
        memReq.size = GetLoadStoreBytes(memReq.opcode);
        // memReq.addr = dst0.vecDataVld ? dst0.vec_data.Get(lane, static_cast<uint32_t>(OperandWidth::OPDW_D)) : dst0.dataOut;
        memReq.addr = accMemInfo->accMemAddr;
        if (opcode == Opcode::OP_TLD) {
            memReq.size = 256;
            ASSERT(memReq.subrid.val < 512);
        }
        // 此处为ct 块做的特有memreq，后续和白晓瑞确认，修改ct 方案，或者修改opcode，微架构不支持此操作
        //  else if (GetLoadStoreBytes(opcode) > 0 && OpcodeIsLoad(opcode)) {
        //     ASSERT(opcode == Opcode::OP_LDI_U);
        //     memReq.size = GetLoadStoreBytes(opcode);
        // }
        // FIXME:handle prefetch
        memReq.bid = bid;
        memReq.gid = gid;
        memReq.prefetch = false;
        memReq.rid = rid;
        memReq.instUid = this->uid;
        memReq.tpc = pc;
        memReq.lsID = lsID;
        memReq.is_llsc = false;
        memReq.intercept = intercept;
        memReq.tag = CalTag(memReq.addr, memReq.toMtcLsu);
        memReq.stack_vld = stack_type == StackInstType::STACK_LOAD;

        if (!memReq.is_load) {
            memReq.data_vld = true;
            if (OpcodeInInstGroup(memReq.opcode, InstGroup::CACHE_MAINTAIN)) {
                // 目前应该只有 DC_ZVA 会走这个地方
                ASSERT(this->opcode == Opcode::OP_DC_ZVA);
                memReq.data = 0;  // todo
            } else {
                uint32_t dataSrc = GetStoreDataSrcIndex(opcode);
                ASSERT(psrcs_.size() > dataSrc);
                auto &psrc = psrcs_[dataSrc];
                memReq.data = psrc->vecDataVld ?
                              psrc->vecData.Get(lane, static_cast<uint32_t>(psrc->width)) :
                              psrc->data;
            }
            // memReq.ref_id = ref.lsInfo.id;
            memReq.type = type;
            memReq.sid = sid;
            if (OpcodeInInstGroup(opcode, InstGroup::ATOMIC)) {
                memReq.addr = accMemInfo->accMemAddr;
            }
        } else {
            memReq.data_vld = false;
        }
        if (memReq.prefetch) {
            // TODO: Fix in 0.33. Differentiated by src2
            memReq.type = PFTYPE_SW_L1D;
        }
    }
    memReq.reqData.Reset();
    memReq.mtc_reqData.Reset();
    memReq.isCrossCacheLine = AddrCrossCacheline(memReq.addr, memReq.size, memReq.toMtcLsu);
    if (opcode == Opcode::OP_TLD) {
        ASSERT(!memReq.isCrossCacheLine);
    }
    return memReq;
}

PEResolveBus SimInstInfo::GenRslvBus()
{
    PEResolveBus rslv;
    rslv.peid = peID;
    rslv.coreId = coreID;
    rslv.bid = bid;
    rslv.rid = rid;
    rslv.stid = stid;
    rslv.gid = gid;
    rslv.subrid = subrid;
    rslv.tid = tid;
    rslv.pipe_cycle = pipeCycle;
    rslv.iq_name = iq_name;
    rslv.isComplete = true;
    if (this->accMemInfo) {
        rslv.accMemInfo = accMemInfo;
    }
    if (this->setcInfo) {
        rslv.setcInfo = setcInfo;
    }
    if (this->ssrInfo) {
        rslv.ssrInfo = ssrInfo;
    }
    if (this->atomicInfo) {
        rslv.atomicInfo = atomicInfo;
    }
    if (this->reduceInfo) {
        rslv.reduceInfo = reduceInfo;
    }
    if (this->brInfo) {
        rslv.brInfo = brInfo;
    }

    if (GetPSrcPtrByType(OperandType::OPD_PREDMASK) != nullptr) {
        rslv.srcMVld = true;
        rslv.simtMask = GetPSrcPtrByType(OperandType::OPD_PREDMASK)->data;
    }
    for (auto dst : pdsts_) {
        rslv.dsts.push_back(dst);
    }
    if ((stack_type == StackInstType::STACK_LOAD || stack_type == StackInstType::STACK_GET) && stack_check) {
        rslv.stack = true;
    }
    return rslv;
}

std::string SimInstInfo::Dump()
{
    std::stringstream oss;
    // 指令基础执行信息
    oss << "BPC:0x" << std::hex << bpc;
    oss << " TPC:0x"<< std::hex << pc;
    oss << " P" << dec << peID << "-";
    oss << "L" << dec << laneID << "-";
    oss << "(B" << dec << AsWrap(bid) << ")-";
    oss << "G" << dec << gid <<"-";
    oss << "T" << dec << tid <<"-";
    oss << "I" << dec << rid <<"-";
    oss << "ST" << dec << stid;

    // 用于SGPR(T/U) 的恢复
    oss <<"  TR" << dec << tSeq << "-";
    oss <<"UR" << dec << uSeq;

    // 访存指令相关信息
    oss << " sid:" << std::dec << sid;
    oss << " lsid:" << std::dec << lsID;

    if (assembleStr.empty()) {
        oss << " ";
        uint64_t cnt = 0;
        for (auto &psrc : psrcs_) {
            oss << psrc->Dump(cnt) << ",";
            cnt++;
        }
        if (!pdsts_.empty()) {
            oss << "->";
            cnt = 0;
            for (auto &pdst : pdsts_) {
                oss << pdst->Dump(cnt) << ",";
                cnt++;
            }
        }
    }
    if (accMemInfo && accMemInfo->vecDataVld) {
        oss << " AccMemAddrs (";
        for (uint32_t i = 0; i < accMemInfo->vecData.data.size(); ++i) {
            oss << left << "0x" << hex << accMemInfo->vecData.data[i] << " ";
        }
        oss << ")";
    }

    oss << "|" << MInst::Assemble();
    oss << " last: " << std::boolalpha << isLastInBlock;
    return oss.str();
}

std::string SimInstInfo::DumpPipeViewInfo()
{
    std::stringstream out;
    out << "(TPC:0x" << std::hex << pc << ") ";
    if (autogen) {
        out << OpcodeManager::Inst().GetOpcodeStr(opcode);
    } else {
        out << Assemble();
    }
    out << " (iqId:" << std::dec << iqid << " ep:" << isqPickerId << ")";
    if (codeLen != EncodeLen::ENL_V) {
        out << "[B" << std::dec << bid.val << ",G" << gid.val << ",T" << tid << ",R" << rid.val << "]";
    }
    return out.str();
}

SimInst SimInstInfo::GenNextBSTOP()
{
    SimInst stop = std::make_shared<SimInstInfo>();
    stop->opcode = Opcode::OP_BSTOP;
    stop->pc = pc + GetEncodeLenVal(codeLen);
    stop->autogen = true;
    return stop;
}

uint64_t SimInstInfo::GetNextPC() const
{
    return pc + GetEncodeLenVal(codeLen);
}

bool SimInstInfo::CheckOOOLoad()
{
    for (auto &psrc : psrcs_) {
        if (OperandTypeIsLTAR(psrc->type)) {
            return true;
        }
    }
    return false;
}

bool SimInstInfo::RangedDataReady()
{
    if (!IsReady()) {
        return false;
    }
    for (auto psrc : psrcs_) {
        if (!psrc->DataRanged()) {
            return false;
        }
    }
    return true;
}

void SimInstInfo::ResetLpv()
{
    for (auto psrc : psrcs_) {
        if (psrc->lpvInfo) {
            psrc->lpvInfo->Reset();
        }
    }
}

bool SimInstInfo::IsVgather() const
{
    return isVgather;
}

VectorBridgeReq SimInstInfo::GenVgatherReq(uint64_t addr)
{
    VectorBridgeReq req = VectorBridgeReq();
    req.cmdType = ReqCmdType::VGATHER;
    req.bid = bid;
    req.tid = transactionId;
    req.groupSlotId = groupSlotId;
    req.stid = stid;
    req.predicateLaneMask = predMask;
    POperandPtr psrc = GetPSrcPtrByType(OperandType::OPD_TO_GPR_REG);
    req.dstDataType = psrc->dataType;
    req.srcDataType = psrcs_[SRC0_IDX]->dataType; // 内部直接获取地址DataType
    req.baseAddrTR = addr;
    req.baseAddrDstTR = psrc->data;
    return req;
}

bool SimInstInfo::IsVscatter() const
{
    return isVscatter;
}

// 辅助函数：位宽转换为 DataType
static DataType WidthToDataType(uint32_t width)
{
    switch (width) {
        case 64: return DataType::UINT64;
        case 32: return DataType::UINT32;
        case 16: return DataType::UINT16;
        case 8:  return DataType::UINT8;
        case 4:  return DataType::UINT4;
        default: return DataType::INVALID;
    }
}

VectorBridgeReq SimInstInfo::GenVscatterReq(uint32_t stqIndex)
{
    VectorBridgeReq req = VectorBridgeReq();
    req.cmdType = ReqCmdType::VSCATTER;
    req.bid = bid;
    req.tid = stqIndex;  // 使用 STQ index 作为唯一标识
    req.groupSlotId = groupSlotId;
    req.stid = stid;
    req.predicateLaneMask = predMask;

    // VSCATTER: 从SimInst的操作数width字段直接读取位宽并转换为UINT DataType
    // OperandWidth枚举值 = 位宽数值 (OPDW_W=0x20=32, OPDW_D=0x40=64)
    // VSCATTER: STORE  数据，[GM地址]
    // SRC0 = 数据操作数, SRC1 = 地址基址
    req.dstDataType = WidthToDataType(static_cast<uint32_t>(psrcs_[SRC0_IDX]->width));
    req.srcDataType = WidthToDataType(static_cast<uint32_t>(psrcs_[SRC1_IDX]->width));

    LOG_INFO_M(Unit::VECTOR, Stage::NA) << "GenVscatterReq: stqIndex=" << stqIndex
        << ", groupSlotId=" << groupSlotId << ", tid=" << req.tid
        << ", dataWidth=" << static_cast<uint32_t>(psrcs_[SRC0_IDX]->width) << "bits"
        << ", addrWidth=" << static_cast<uint32_t>(psrcs_[SRC1_IDX]->width) << "bits";
    return req;
}

bool SimInstInfo::IsMcallLoadGlobal()
{
    return OpcodeIsLoad(opcode) && biqType == BIQType::MCALL_IQ && (accMemInfo != nullptr ? !accMemInfo->local : true);
}

std::ostream& operator<<(std::ostream& out, SimInst const& inst)
{
    out << inst->Dump();
    return out;
}
} // namespace JCore
