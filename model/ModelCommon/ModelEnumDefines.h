#pragma once
#ifndef MODEL_ENUM_DEFINES_H
#define MODEL_ENUM_DEFINES_H

#include <bitset>
#include <array>
#include <string>
namespace JCore {
enum class LastDecodeState {
    LAST_MEMSET,
    LAST_MEMCOPY,
    LAST_DECODER,
    LAST_CODETEMPLATE,
    LAST_OTHER
};

enum class GenMemStage {
    START = 0,
    WAIT_RF,
    GEN_INS,
    COMPLETE
};

enum class GenMemCopyStage {
    GEN_ALIGN = 0,
    GEN_FOLLOW,
    GEN_ENDING
};

enum class HdrSrcType {
    LB_TYPE,
    TILE_TYPE
};

enum class BlockStatus {
    FREE            = 0,
    ALLOCATED       = 1,
    DISPATCHED      = 2,
    RENAMING        = 3,
    RENAMED         = 4,
    ISSUED          = 5,
    PAR_RUNNING     = 6,
    RUNNING         = 7,
    COMPLETED       = 8,
    RETIRED         = 9,
    NEEDFLUSH       = 10,
    NEEDREPLAY      = 11,
    FLUSHED         = 12,
    MISPRED         = 13,
    EXCEPTION       = 14,
    TERMINATE       = 15,
    BROB_STATUS_NUM,
};

inline const std::array<std::string, static_cast<std::size_t>(BlockStatus::BROB_STATUS_NUM)>& BROBStatusNames()
{
    static const std::array<std::string, static_cast<std::size_t>(BlockStatus::BROB_STATUS_NUM)> BROBSTATUS_NAMES = {{
        "free",         // FREE            = 0,
        "allocated",    // ALLOCATED       = 1,
        "dispatched",   // DISPATCHED      = 2,
        "renaming",     // RENAMING        = 3,
        "renamed",      // RENAMED         = 4,
        "issued",       // ISSUED          = 5,
        "par_running",  // PAR_RUNNING     = 6,
        "executing",    // RUNNING         = 7,
        "completed",    // COMPLETED       = 8,
        "retired",      // RETIRED         = 9,
        "need flushing", // NEEDFLUSH       = 10,
        "need replay",  // NEEDREPLAY      = 11,
        "flushed",      // FLUSHED         = 12,
        "mispredicted", // MISPRED         = 13,
        "fault",        // EXCEPTION       = 14,
        "terminate",    // TERMINATE       = 15,
    }};
    return BROBSTATUS_NAMES;
}

inline const std::string& GetBROBStatus(BlockStatus type)
{
    const std::size_t idx = static_cast<std::size_t>(type);
    const auto& names = BROBStatusNames();
    static const std::string INVALID_BROB_STATUS_TYPE = "invalid_brob_status";
    if (idx >= names.size()) {
        return INVALID_BROB_STATUS_TYPE;
    }
    return names[idx];
}

enum class MemCopyStatus {
    FOLLOW_LD = 0,
    FOLLOW_CONCAT,
    FOLLOW_ST,
    FOLLOW_MAX
};

enum class LSUType {
    SCALAR_LSU = 0,
    VECTOR_LSU,
    BRIDGE_TABLE,
    MEMORY_LSU,
    COUNT,
};

/* code templete type */
enum class CodeTemplateID {
    NON_TEMPLATE        = 0,
    SETG_EPILOGUE       = 1,
    FENTRY_TEMPLATE     = 2,
    FEXIT_TEMPLATE      = 3,
    FRET_TEMPLATE       = 4,
    MEM_SET_TEMPLATE    = 5,
    MEM_COPY_TEMPLATE   = 6,
    SMALL_TEMPLATE      = 7,
    ECALL_TEMPLATE      = 8,
    SIMT_TEMPLATE,
    MEM_IN_TEMPLATE,
    MEM_OUT_TEMPLATE,
    CODETEMPLATEIDCNT
};
using TemplateIDSet = std::bitset<static_cast<int>(CodeTemplateID::CODETEMPLATEIDCNT)>;

enum class Location {
    SRC0 = 0,
    SRC1,
    SRC2,
    SRC3,
    SRCM,
    TEMPSRC,
    INREG,
    IMM,
    DST0,
    DST1,
    DST2,
    DSTM,
    OUTREG,
};

enum class SRCFusionForm {
    SRC_0_1,
    SRC_0_2,
    SRC_1_2,
    SRC_0_1_2
};

enum class EXEType {
    ADD,
    SUB,
    LOAD,
};

enum class StackInstType {
    NORMAL,
    STACK_LOAD,
    STACK_GET,
    STACK_SET,
    STACK_SET_WAIT,
    SET_SP,
    LOCAL_SP,
    LAST
};

enum PrefType {
    PFTYPE_STRIDE = 1,
    PFTYPE_STREAM,
    PFTYPE_SW_L1I,
    PFTYPE_SW_L1D,
    PFTYPE_SW_L2
};

enum BlkSrcType {
    NONE_SRC,
    BLK_ISSUEQ,
    FLUSH_BYPASS,
    PARALLEL_FETCH,
    BCTRL_PREF
};

enum class FlushType {
    MISS_PRED_FLUSH, // Inter-block miss Rpedict
    PE_REPLAY,      // Intra-block miss Rpedict
    NUKE_FLUSH,     // Inter-block load/store violation
    INNER_FLUSH,    // Intra-block load/store
    FAST_REPLAY,     // Young blocks fill up LSU
    FAST_FLUSH,      // Flush at once at a point
    SIMT_INNER_FLUSH,
};
enum ExecEngineTyp {
    SCALAR_IEX = 0,
    SIMT_IEX,
    MEM_IEX,
    IEX_NUM,
    SIMT0,
    SIMT1,
    SIMT2,
    SIMT3,
};

enum SpecType {
   SBAR_UNCERTAIN = 0,
   SBAR_CERTAIN = 1,
   SBAR_UNCERTAIN_OOW = 2,
   SBAR_CERTAIN_OOW = 3
};

enum StoreType {
    ST_ALL = 0, // default is 0
    ST_ADDR,
    ST_DATA
};

enum class MemSetRegType {
    MSET_DST_ADDR = 0,
    MSET_DATA,
    MSET_SIZE
};

enum class MemCopyRegType {
    MCOPY_DST_ADDR = 0,
    MCOPY_SRC_ADDR,
    MCOPY_SIZE
};
} // namespace JCore
#endif