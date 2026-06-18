#include "MInstCalculator.h"

#include "blockArgs/BlockArgs.h"
#include "arithmetic/Arithmetic.h"
#include "arithmetic_fp/ArithmeticFP.h"
#include "cacheMaintain/CacheMaintain.h"
#include "compare/Compare.h"
#include "compare_fp/CompareFP.h"
#include "setc/Setc.h"
#include "pc/PC.h"
#include "immediate/Immediate.h"
#include "branch/Branch.h"
#include "multi_cycle/MultiCycle.h"
#include "bit/Bit.h"
#include "extend/Extend.h"
#include "convert/Convert.h"
#include "load/Load.h"
#include "store/Store.h"
#include "others/Others.h"
#include "atomic/Atomic.h"
#include "ssr/SSR.h"
#include "maxmin/MaxMin.h"
#include "reduce/Reduce.h"
#include "compound/Compound.h"
#include "mov/Mov.h"

namespace JCore {
namespace Calculate {


MInstCalculator MInstCalculator::Inst()
{
    static MInstCalculator instance;
    return instance;
}

MInstCalculator::MInstCalculator()
    : m_handlers({
        { InstGroup::BLOCK_SPLIT, &Calculate::BlockArgs::CalculateBlockArgs }, /* BLOCK_SPLIT */
        { InstGroup::BLOCK_OFFSET, &Calculate::BlockArgs::CalculateBlockArgs }, /* BLOCK_OFFSET */
        { InstGroup::BLOCK_IO, &Calculate::BlockArgs::CalculateBlockArgs }, /* BLOCK_IO */
        { InstGroup::BLOCK_ATTR, &Calculate::BlockArgs::CalculateBlockArgs }, /* BLOCK_ATTR */
        { InstGroup::BLOCK_HINT, &Calculate::BlockArgs::CalculateBlockArgs }, /* BLOCK_HINT */
        { InstGroup::BLOCK_ARGUMENT, &Calculate::BlockArgs::CalculateBlockArgs }, /* BLOCK_ARGUMENT */
        { InstGroup::ARITHMETIC, &Calculate::Arithmetic::CalculateArithmetic }, /* ARITHMETIC */
        { InstGroup::ARITHMETIC_FP, &Calculate::ArithmeticFP::CalculateArithmeticFP }, /* ARITHMETIC_FP */
        { InstGroup::COMPARE, &Calculate::Compare::CalculateCompare }, /* COMPARE */
        { InstGroup::COMPARE_FP, &Calculate::CompareFP::CalculateCompareFP }, /* COMPARE_FP */
        { InstGroup::SETC, &Calculate::Setc::CalculateSetc }, /* SETC */
        { InstGroup::PC, &Calculate::PC::CalculatePC }, /* PC */
        { InstGroup::IMMEDIATE, &Calculate::Immediate::CalculateImmediate }, /* IMMEDIATE */
        { InstGroup::MOVE, &Calculate::Others::CalculateOthers }, /* MOVE */
        { InstGroup::BRANCH, &Calculate::Branch::CalculateBranch }, /* BRANCH */
        { InstGroup::MULTICYCLE, &Calculate::MultiCycle::CalculateMultiCycle }, /* MULTICYCLE */
        { InstGroup::BIT, &Calculate::Bit::CalculateBit }, /* BIT */
        { InstGroup::GQM, 0 }, /* GQM */
        { InstGroup::COMPOUND, &Calculate::Compound::CalculateCompound }, /* COMPOUND */
        { InstGroup::PREFETCH, &Calculate::Others::CalculateOthers }, /* PREFETCH */
        { InstGroup::LOAD, &Calculate::Load::CalculateLoad }, /* LOAD */
        { InstGroup::STORE, &Calculate::Store::CalculateStore }, /* STORE */
        { InstGroup::ATOMIC, &Calculate::Atomic::CalculateAtomic }, /* ATOMIC */
        { InstGroup::EXECUTE_CONTROL, &Calculate::Others::CalculateOthers }, /* EXECUTE_CONTROL */
        { InstGroup::EXTEND, &Calculate::Extend::CalculateExtend }, /* EXTEND */
        { InstGroup::CACHE_MAINTAIN, &Calculate::CacheMaintain::CalculateCacheMaintain }, /* CACHE_MAINTAIN */
        { InstGroup::GMO, 0 }, /* GMO */
        { InstGroup::SSR, &Calculate::SSR::CalculateSSR }, /* SSR */
        { InstGroup::MAX_MIN, &Calculate::MaxMin::CalculateMaxMin }, /* MAX_MIN */
        { InstGroup::CONVERT, &Calculate::Convert::CalculateConvert }, /* CONVERT */
        { InstGroup::REDUCE, &Calculate::Reduce::CalculateReduce }, /* REDUCE */
        { InstGroup::SHUFFLE, 0 }, /* SHUFFLE */
        { InstGroup::SPECIAL, &Calculate::Mov::CalculateMov }, /* MOV */
        { InstGroup::CT_CUSTOM, 0 }, /* CT_CUSTOM */
    })
{}

bool MInstCalculator::CalculateMinst(MInst& inst) const
{
    InstGroup grp = OpcodeManager::Inst().GetOpcodeGroup(inst.opcode);
    Handler handler = GetHandler(grp);
    return handler ? handler(inst) : false;
}

Handler MInstCalculator::GetHandler(InstGroup grp) const
{
    ASSERT(m_handlers.find(grp) != m_handlers.cend());
    return m_handlers.at(grp);
}

} // namespace Calculate
} // namespace JCore
