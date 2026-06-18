#pragma once

#include "ModelCommon/SimInstInfo.h"

namespace JCore {


class IBPredictor {
public:
    IBPredictor();
    ~IBPredictor();

    uint64_t                    predict();
private:
};

} // namespace JCore
