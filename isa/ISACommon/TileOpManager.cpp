#include "ISACommon/TileOpManager.h"

namespace JCore {

TileOpManager::TileOpManager()
{
    RegisterTileOpcode();
}

void TileOpManager::RegisterTileOpInfo(TileOp opcode, std::string str, MachineType cType)
{
    assert(opcode < TileOp::TILE_OP_NUMBER);
    assert(strToEnum_.count(str) == 0);
    assert(registered.count(opcode) == 0);
    registered.emplace(opcode);
    strToEnum_.emplace(str, opcode);
    opcodeInfos_[static_cast<int>(opcode)] = TileOpcodeInfo(opcode, std::move(str), cType);
}

void TileOpManager::RegisterTileOpcode()
{
    /* VECTOR */
    RegisterTileOpInfo(TileOp::TADD, "TADD", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TSUB, "TSUB", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TMUL, "TMUL", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TDIV, "TDIV", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TREM, "TREM", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TFMOD, "TFMOD", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TAND, "TAND", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TOR, "TOR", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TXOR, "TXOR", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TSHL, "TSHL", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TSHR, "TSHR", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TMAX, "TMAX", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TMIN, "TMIN", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TCMP, "TCMP", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TPRELU, "TPRELU", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TABS, "TABS", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TNOT, "TNOT", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TNEG, "TNEG", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TEXP, "TEXP", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TLOG, "TLOG", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TRECIP, "TRECIP", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TSQRT, "TSQRT", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TRSQRT, "TRSQRT", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TRELU, "TRELU", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TADDC, "TADDC", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TSUBC, "TSUBC", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TSEL, "TSEL", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TCVT, "TCVT", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TADDS, "TADDS", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TSUBS, "TSUBS", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TMULS, "TMULS", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TDIVS, "TDIVS", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TREMS, "TREMS", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TFMODS, "TFMODS", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TANDS, "TANDS", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TORS, "TORS", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TXORS, "TXORS", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TSHLS, "TSHLS", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TSHRS, "TSHRS", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TMAXS, "TMAXS", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TMINS, "TMINS", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TCMPS, "TCMPS", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TLRELU, "TLRELU", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TADDSC, "TADDSC", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TSUBSC, "TSUBSC", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TSELS, "TSELS", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TEXPANDS, "TEXPANDS", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TROWSUM, "TROWSUM", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TROWMAX, "TROWMAX", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TROWMIN, "TROWMIN", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TROWPROD, "TROWPROD", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TROWEXPAND, "TROWEXPAND", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TROWEXPANDADD, "TROWEXPANDADD", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TROWEXPANDSUB, "TROWEXPANDSUB", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TROWEXPANDMUL, "TROWEXPANDMUL", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TROWEXPANDDIV, "TROWEXPANDDIV", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TROWEXPANDMAX, "TROWEXPANDMAX", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TROWEXPANDMIN, "TROWEXPANDMIN", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TROWEXPANDEXPDIF, "TROWEXPANDEXPDIF", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TCOLSUM, "TCOLSUM", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TCOLMAX, "TCOLMAX", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TCOLMIN, "TCOLMIN", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TCOLPROD, "TCOLPROD", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TCOLEXPAND, "TCOLEXPAND", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TCOLEXPANDADD, "TCOLEXPANDADD", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TCOLEXPANDSUB, "TCOLEXPANDSUB", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TCOLEXPANDMUL, "TCOLEXPANDMUL", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TCOLEXPANDDIV, "TCOLEXPANDDIV", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TCOLEXPANDMAX, "TCOLEXPANDMAX", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TCOLEXPANDMIN, "TCOLEXPANDMIN", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::TCOLEXPANDEXPDIF, "TCOLEXPANDEXPDIF", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::ESAVE, "ESAVE", MachineType::VECLITE);
    RegisterTileOpInfo(TileOp::ERCOV, "ERCOV", MachineType::VECLITE);
    /* TMA */
    RegisterTileOpInfo(TileOp::TLOAD, "TLOAD", MachineType::TMA);
    RegisterTileOpInfo(TileOp::TSTORE, "TSTORE", MachineType::TMA);
    RegisterTileOpInfo(TileOp::TMOV, "TMOV", MachineType::TAU);
    RegisterTileOpInfo(TileOp::MGATHER, "MGATHER", MachineType::TAU);
    RegisterTileOpInfo(TileOp::MSCATTER, "MSCATTER", MachineType::TAU);
    RegisterTileOpInfo(TileOp::VGATHER, "VGATHER", MachineType::TAU);
    RegisterTileOpInfo(TileOp::VSCATTER, "VSCATTER", MachineType::TAU);
    /* CUBE */
    RegisterTileOpInfo(TileOp::TMATMUL, "TMATMUL", MachineType::CUBE);
    RegisterTileOpInfo(TileOp::TMATMUL_BIAS, "TMATMUL_BIAS", MachineType::CUBE);
    RegisterTileOpInfo(TileOp::TMATMUL_ACC, "TMATMUL_ACC", MachineType::CUBE);
    RegisterTileOpInfo(TileOp::TMATMULMX, "TMATMULMX", MachineType::CUBE);
    RegisterTileOpInfo(TileOp::TMATMULMX_BIAS, "TMATMULMX_BIAS", MachineType::CUBE);
    RegisterTileOpInfo(TileOp::TMATMULMX_ACC, "TMATMULMX_ACC", MachineType::CUBE);
    RegisterTileOpInfo(TileOp::ACCCVT, "ACCCVT", MachineType::CUBE);
    RegisterTileOpInfo(TileOp::TGEMV, "TGEMV", MachineType::CUBE);
    RegisterTileOpInfo(TileOp::TGEMV_BIAS, "TGEMV_BIAS", MachineType::CUBE);
    RegisterTileOpInfo(TileOp::TGEMV_ACC, "TGEMV_ACC", MachineType::CUBE);
    RegisterTileOpInfo(TileOp::TGEMVMX, "TGEMVMX", MachineType::CUBE);
    RegisterTileOpInfo(TileOp::TGEMVMX_BIAS, "TGEMVMX_BIAS", MachineType::CUBE);
    RegisterTileOpInfo(TileOp::TGEMVMX_ACC, "TGEMVMX_ACC", MachineType::CUBE);
    RegisterTileOpInfo(TileOp::TINVALID, "TINVALID", MachineType::VECTOR);
    RegisterTileOpInfo(TileOp::TSTASH_TA_INPUT, "TSTASH_TA_INPUT", MachineType::STASH);
    RegisterTileOpInfo(TileOp::TSTASH_TO_OUTPUT, "TSTASH_TO_OUTPUT", MachineType::STASH);
    RegisterTileOpInfo(TileOp::OTHER_TSTASH, "OTHER_TSTASH", MachineType::STASH);
    RegisterTileOpInfo(TileOp::TRING, "TRING_ONE", MachineType::TMU);
    RegisterTileOpInfo(TileOp::TRING_TIME_1, "TRING_TWO", MachineType::TMU);
    RegisterTileOpInfo(TileOp::TRING_TIME_2, "TRING_THREE", MachineType::TMU);
    RegisterTileOpInfo(TileOp::TRING_TIME_3, "TRING_FOUR", MachineType::TMU);
    RegisterTileOpInfo(TileOp::TRING_TIME_4, "TRING_FIVE", MachineType::TMU);
}

} // namespace JCore