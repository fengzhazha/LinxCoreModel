#ifndef TILEOP_COMMON_CALCULATE
#define TILEOP_COMMON_CALCULATE

#include "../../softfloat/include/softfloat-types.h"
#include "ISACommon/BlockAttribute.h"

namespace JCore {
class TileOpCommonCalc {
public:
    static uint64_t FillEle(DataType dataType, PadValue paddingValue);
    static uint64_t EleExp(uint64_t ele, DataType dataType, float_status status);
    static uint64_t EleRecip(uint64_t ele, DataType dataType);
};
}

#endif