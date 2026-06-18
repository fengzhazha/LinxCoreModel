#ifndef BLOCKISA_MODEL_CREDIT_H
#define BLOCKISA_MODEL_CREDIT_H
/*
 * CREDIT
*/

#include <iostream>
#include "core/Bus.h"

namespace JCore {


class Credit {
public:
    Credit() {}

    bool GetACCFlag() const { return ACC_flag; }
    void SetACCFlag(bool flag) { ACC_flag = flag; }

    BIQType GetBIQType() const { return biqType; }
    void SetBIQType(BIQType type) { biqType = type; }

    bool ACC_flag = false;
    BIQType biqType = BIQType::NONE_IQ;
};


} // namespace JCore

#endif  // BLOCKISA_MODEL_CREDIT_H
