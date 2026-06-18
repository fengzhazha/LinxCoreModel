#include "core/Bus.h"

#include "core/Core.h"
#include "ModelCommon/SimInstInfo.h"

namespace JCore {
using namespace std;

std::ostream& operator<<(std::ostream& out, BHeader const &header) {
    out << "B:0x" << std::hex << header->bpc;
    // if (header->isInst) {
    //     out << "BPC 0x" << hex << header->tpc << " ";
    // } else {
    //     out << "BPC 0x" << hex << header->bpc << " ";
    // }
    // out << "Block";

    // out << "." << GetBlockTypeName(header->type);

    // if (header->type == BlockType::CONTROL || header->type == BlockType::TEMPLATE ||
    //     header->type == BlockType::SYSTEM_32) {
    //     out << "." << GetBlockOpcodeName(header->opcode);
    // }

    // out << " " << GetBlockBranchTypeName(header->branchType) << " ";

    // out << "BNEXT 0x" << hex << header->divertBPC << " ";
    // out << "BTEXT 0x" << hex << header->payloadPC << " ";
    // out << "Size " << dec << header->size << " ";
    // out << "B" << dec << header->bSeq << "-";
    // out << "P" << header->peid;
    // if (header->debugId != UINT64_MAX) {
    //     out << "-Did" << dec << header->debugId;
    // }
    // out << " is_inst " << header->isInst << " ";
    // out << "is_last " << header->isLast << " ";

    // // to delete
    // if (header->inputRegMask) {
    //     out << "GET( ";
    //     for (int i = 0; i < GPR_COUNT; i++) {
    //         if ((1 << i) & header->inputRegMask)
    //             out << "R" << dec << i << " ";
    //     }
    //     out << ") ";
    // }
    // if (header->outputRegMask) {
    //     out << "SET( ";
    //     for (int i = 0; i < GPR_COUNT; i++) {
    //         if ((1 << i) & header->outputRegMask)
    //             out << "R" << dec << i << " ";
    //     }
    //     out << ")";
    // }

    return out;
}

std::ostream& operator<<(std::ostream& out, VecData const &vd) {
    out << "[";
    out << "vld: " << vd.vld << hex << " width: 0x" << vd.width << " size: 0x" << vd.size << " (";

    for (auto d : vd.data) {
        out << d << ", ";
    }

    out << ")]";
    return out;
}

std::ostream& operator<<(std::ostream& out, MemRequest const &memRequest) {
    out << "TPC 0x" << hex << memRequest.tpc <<" B" << dec << memRequest.bid
         << "-G" << memRequest.gid << "-T" << memRequest.tid << "-R" << memRequest.rid << " lsID:" << memRequest.lsID
         << " peID:" << memRequest.peID << " " << OpcodeManager::Inst().GetOpcodeStr(memRequest.opcode) << " lane size:"<< memRequest.laneSize;
    uint64_t mask = 0xffffffffffffffff;
    const uint64_t maxLane = 64;
    if (memRequest.realReqCnt < maxLane) {
        mask = ((1ULL << memRequest.realReqCnt) - 1);
    }
    if (mask != 0xffffffffffffffff) {
        out << " mask=0x" << hex << mask;
    }
    if (memRequest.isLoad && memRequest.data.vld) {
        out<<" ADDR 0x"<<hex<<memRequest.addrs<<" ";
        out<<" DATA 0x"<<memRequest.data;
    }
    out << dec;
    return out;
}

std::ostream& operator<<(std::ostream& out, MemReqBus const &memReqBus) {
    out << "TPC 0x" << hex << memReqBus.tpc << " B" << dec << memReqBus.bid << ":G" << memReqBus.gid << ":R"
        << memReqBus.rid << ":STID" << memReqBus.stid << ":SUB" << memReqBus.subrid << " LSID:" << memReqBus.lsID
        << " peID:" << memReqBus.peID << " " << OpcodeManager::Inst().GetOpcodeStr(memReqBus.opcode) << " size:"
        << dec << memReqBus.size << " toMtc:" << memReqBus.toMtcLsu << " fromScalper: " << memReqBus.fromScalper;
    if (memReqBus.tile.vld) {
        out << " TILE " << " dtype:" << BriefDataType2String(memReqBus.tile.dType);
        out << " Layout:";
        if (memReqBus.tile.layout != LayOut::LAYOUT_COUNT) {
            out << GetLayOutName(memReqBus.tile.layout);
        } else {
            out << " unkown";
        }
        out << hex << " D1GM:0x" << memReqBus.tile.d1GM << " D2GM:0x" << memReqBus.tile.d2GM;
        out << dec << " stride: " << memReqBus.tile.strideGM;
        out << hex << " TRbase:0x " << memReqBus.tile.baseAddrTR;
        out << " D1TR:0x" << memReqBus.tile.d1TR << " D2TR:0x" << memReqBus.tile.d2TR;
    }
    if (memReqBus.instUid != -1ULL) {
        out << " INST_UID:" << dec << memReqBus.instUid;
    }

    if(!memReqBus.is_load) {
        out << " SID:" << dec << memReqBus.sid;
    }
    if (memReqBus.iexTyp != SCALAR_IEX) {
        out<<" LANE " << dec << memReqBus.simtLane;
        uint64_t mask = 0xffffffffffffffff;
        const uint64_t maxLane = 64;
        if (memReqBus.realReqCnt < maxLane) {
            mask = ((1ULL << memReqBus.realReqCnt) - 1);
        }
        out << " mask=0x" << hex << mask;
    }
    out<<" ADDR 0x"<<hex<<memReqBus.addr<<" ";
    if (memReqBus.data_vld) {
        out<<"TYPE ";
        switch (memReqBus.type) {
            case ST_ALL : out << "all";    break;
            case ST_ADDR: out << "addr";   break;
            case ST_DATA: out << "data";   break;
            default: out << "xxx";
        }
        out<<" DATA 0x"<<memReqBus.data;
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, IDBus &fullID) {
    out << "BID " << fullID.bid <<" RID " << fullID.rid;
    out << " LID " << fullID.lid << " SID " << fullID.sid;
    out << " LSID " << fullID.lsID;
    return out;
}

static const char* FetchStatusToStr(FetchReqStatus status)
{
    const static map<FetchReqStatus, const char*> mp = {
        {ICACHE_IDEL, "IDLE"},
        {ICACHE_HIT, "ICACHE_HIT"},
        {ICACHE_WAIT, "ICACHE_WAIT"},
        {ICACHE_MISS, "ICACHE_MISS"},
        {ICACHE_GET, "ICACH_GET"},
    };
    auto it = mp.find(status);
    if (it != mp.end()) {
        return it->second;
    }
    return "UNKNOWN";
}

std::ostream& operator<<(std::ostream& out, FetchReqBus &fetchReqBus) {
    out<<"FetchReq BPC 0x"<<hex<<fetchReqBus.bpc<<" TPC 0x"<<hex<<fetchReqBus.tpc<<" mask "<<dec<<fetchReqBus.mask;
    out<< "STID:" << dec << fetchReqBus.stid;
    out<<" First:"<<boolalpha<<fetchReqBus.first<<", Last:"<<boolalpha<<fetchReqBus.last;
    out<<", blkNBr:"<<boolalpha<<fetchReqBus.blockNoBranch<<", BdlNBr:"<<boolalpha<<fetchReqBus.bundleNoBranch;
    out<<" (Block "<<dec<<fetchReqBus.bid.val<<" Group "<< fetchReqBus.gid << " fid " << fetchReqBus.fid <<" )";
    out<<" (Status: "<<FetchStatusToStr(fetchReqBus.status)<<")";
    if (fetchReqBus.merge) {
        out<<"\n FetchReq merge next BID: "<<dec<<fetchReqBus.nextBid.val<<", BPC 0x"<<hex<<fetchReqBus.nextBPC;
        cout<<", mask "<<dec<<(fetchReqBus.mask - fetchReqBus.mask1)<<", last ";
        out<<boolalpha<<fetchReqBus.nextLast;
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, PEControlBus &peControlBus) {
    out<<"peControlBus.bid: "<<peControlBus.bid<<" bpc: "<<hex<<peControlBus.bpc<<" tpc: "<<hex<<peControlBus.tpc;
    out<<" fecthTpc: "<<hex<<peControlBus.fetchTpc;
    return out;
}

std::ostream& operator<<(std::ostream& out, PEResolveBus &rlsvBus) {
    out<<"ROB resolve : B"<<dec<<rlsvBus.bid<<":T"<<rlsvBus.rid<<":PE"<<rlsvBus.peid;
    if (rlsvBus.isIssue) {
        out<<" inst issued";
    }
    if (rlsvBus.isComplete) {
        out<<" inst completed data:";
    }
    for (auto dst : rlsvBus.dsts) {
        if (dst->dataVld) {
            out << " data" << std::hex << dst->data;
        }
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, FlushBus const &flushBus)
{
    out<<"FlushBus TYPE";
    switch (flushBus.req.type) {
        case FlushType::MISS_PRED_FLUSH:   out << " Miss predict";     break;
        case FlushType::NUKE_FLUSH:        out << " Nuke flush";       break;
        case FlushType::INNER_FLUSH:       out << " Inner flush";      break;
        case FlushType::PE_REPLAY:         out << " PE replay";        break;
        case FlushType::FAST_REPLAY:       out << " Fast replay";      break;
        case FlushType::SIMT_INNER_FLUSH:  out << " SIMT inner flush"; break;
        default:                out << " Unrecognized";     break;
    }
    out << " B" << dec << flushBus.req.bid << ":T" << flushBus.req.rid << ":G" << flushBus.req.gid
        << " LSID:" << flushBus.req.lsID;
    out<<" fbid="<<flushBus.req.fbid<<" fbid_local="<<flushBus.req.fbid_local;
    out<<" PE:"<<flushBus.req.peID<<" immFlush:"<<flushBus.req.immediateFlush;
    out << hex << " Req FetchTPC=0x" << flushBus.req.fetchTPC;
    return out;
}

std::ostream& operator<<(std::ostream& out, RFReqBus &rfBus)
{
    if (!rfBus.vld) {
        out<<"bus invalid";
        return out;
    }
    out<<"RF B"<<dec<<rfBus.bid<<":G"<<rfBus.gid<<":I"<<rfBus.rid<<":T"<<rfBus.tid<<":PE"<<rfBus.peid;
    // TODO：
    // if (rfBus.src0.vld) {
    //     out<<" (src0:"<<dec;
    //     srcOut(out, rfBus.src0);
    // }
    // if (rfBus.src1.vld) {
    //     out<<" (src1:"<<dec;
    //     srcOut(out, rfBus.src1);
    // }
    // if (rfBus.src2.vld) {
    //     out<<" (src2:"<<dec;
    //     srcOut(out, rfBus.src2);
    // }
    // if (rfBus.src3.vld) {
    //     out<<" (src3:"<<dec;
    //     srcOut(out, rfBus.src3);
    // }
    // if (rfBus.srcM.vld) {
    //     out << "(srcM:" << dec;
    //     srcOut(out, rfBus.srcM);
    // }
    // if (rfBus.dst0.vld) {
    //     out<<" (dst0:"<<dec;
    //     dstOut(out, rfBus.dst0);
    // }
    // if (rfBus.dst1.vld) {
    //     out<<" (dst1:"<<dec;
    //     dstOut(out, rfBus.dst1);
    // }
    // if (rfBus.dstM.vld) {
    //     if (rfBus.dstM.dataOut_vld) {
    //         cout << " simt mask:0x" << rfBus.dstM.dataOut;
    //     }
    // }

    return out;
}

std::ostream& operator<<(std::ostream& out, PLpvInfo &info) {
    if (!info) {
        return out;
    }

    for (uint32_t i = 0; i < LPV_SIZE; ++i) {
        if (i != 0)
            out << ",";
        cout << "0x" << hex << info->lpv[i];
    }

    return out;
}

std::ostream& operator<<(std::ostream& out, StackRenameBus &bus) {
    if (!bus.vld) {
        out << "invalid";
        return out;
    }
    out << "B" << dec << bus.bid << ":T" << bus.rid;
    out << ":LS" << bus.lsID << ":imm(" << hex << bus.imm<<") ";
    switch (bus.type) {
        case StackInstType::NORMAL:
            out << "normal ";
            break;
        case StackInstType::STACK_LOAD:
            out << "stack load ";
            break;
        case StackInstType::STACK_GET:
            out << "stack get ";
            break;
        case StackInstType::STACK_SET:
            out << "stack set ";
            break;
        case StackInstType::SET_SP:
            out << "set sp ";
            break;
        case StackInstType::LOCAL_SP:
            out << "local set sp ";
            break;
        default:
            break;
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, MDBBus &bus) {
    out << "MDBus ";
    if (!bus.vld) {
        out << "invalid";
        return out;
    }

    out << "ld info B" << dec << bus.ldInfo.bid << ":LS" << bus.ldInfo.lsID << " ";
    out << "st info B" << dec << bus.stInfo.bid << ":LS" << bus.stInfo.lsID << " ";
    out << "hit " << bus.hit;
    return out;
}

void receiveData(uint2048_t &cache, uint2048_t &data, bool *valid)
{
    for (int i = 0; i < 32; i++) {
        uint64_t *dBit = &cache.bits[i];
        uint64_t *sBits = &data.bits[i];
        for (int j = 0; j < 8; j++) {
            uint8_t *dst = (uint8_t*)dBit;
            uint8_t *src = (uint8_t*)sBits;
            if (valid[i * 8 + j]) {
                dst[j] = src[j];
            }
        }
    }
}

void receiveData(uint512_t &cache, uint512_t &data, bool *valid)
{
    for (int i = 0; i < 8; i++) {
        uint64_t *dBit = &cache.bits[i];
        uint64_t *sBits = &data.bits[i];
        for (int j = 0; j < 8; j++) {
            uint8_t *dst = (uint8_t*)dBit;
            uint8_t *src = (uint8_t*)sBits;
            if (valid[i * 8 + j]) {
                dst[j] = src[j];
            }
        }
    }
}

} // namespace JCore
