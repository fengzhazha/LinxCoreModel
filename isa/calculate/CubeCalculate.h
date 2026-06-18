#ifndef FLOATING_POINT_ARITHMETIC
#define FLOATING_POINT_ARITHMETIC

#include <cstdint>
#include <map>
#include <vector>
#include <iostream>
#include "softfloat.h"
#include "softfloat-types.h"
#include "../../common/Common.h"
#include "ISACommon/DataType.h"
#include "ISACommon/LayOut.h"

namespace JCore {
class CubeCalculate {
public:
    static uint64_t EleAdd(uint64_t left, uint64_t right, DataType dataType, float_status status);
    static uint64_t EleSub(uint64_t left, uint64_t right, DataType dataType, float_status status);
    static uint64_t EleMul(uint64_t left, uint64_t right, DataType dataType, float_status status);
    static uint64_t EleMulScale(uint64_t baseValue, uint64_t scaleValue, DataType dataType, float_status status);
    static uint64_t EleSize(DataType dataType);
    static double EleRealSize(DataType dataType);
    static uint64_t EleZero(DataType dataType);
    static uint64_t EleMax(uint64_t left, uint64_t right, DataType dataType, float_status status);
    static const char* EleType(DataType dataType);
    static uint64_t EleOffset(uint64_t base, uint64_t idx, DataType dataType);
    static uint64_t EleDataExtract(uint64_t originData, uint64_t idx, DataType dataType);
    static uint64_t EleDataMerge(uint64_t originData, uint64_t idx, DataType dataType);

    static bool SwitchRD2NZ(std::vector<uint64_t>& matrix, size_t blockRow,
                            size_t blockCol, size_t totalRow, size_t totalCol);
    static bool SwitchRD2ZN(std::vector<uint64_t>& matrix, size_t blockRow,
                            size_t blockCol, size_t totalRow, size_t totalCol);
    static bool SwitchRD2NN(std::vector<uint64_t>& matrix, size_t blockRow,
                            size_t blockCol, size_t totalRow, size_t totalCol);
    static bool SwitchRD2ZZ(std::vector<uint64_t>& matrix, size_t blockRow,
                            size_t blockCol, size_t totalRow, size_t totalCol);
    static void ResultMatrix(const std::vector<uint64_t> &matrixData,
                                       std::pair<size_t, size_t> matrixShape,
                                       size_t eleSize,
                                       LayOut tileConvertType);
    static bool SwitchCD2NZ(std::vector<uint64_t>& matrix, size_t blockRow, size_t blockCol, size_t totalRow,
                            size_t totalCol);
    static bool SwitchCD2ZN(std::vector<uint64_t>& matrix, size_t blockRow, size_t blockCol, size_t totalRow,
                            size_t totalCol);
    static bool SwitchRD2CD(std::vector<uint64_t>& matrix, size_t totalRow, size_t totalCol);

    // 矩阵打印
    static void PrintData(const std::vector<uint64_t>& src,
                          uint64_t row,
                          uint64_t column,
                          uint64_t maxColsPerBlock = 16);
    // origin: HIF4(aka E1M2)
    // ea: E6M2 shared by 64 elements
    // eb: E1_8 shared by 8 elements
    // ec: E1_16 shared by 4 elements
    static uint64_t GetElementValue(uint64_t origin, uint64_t ea, uint64_t eb, uint64_t ec, float_status* status);
};

} // namespace JCore
#endif