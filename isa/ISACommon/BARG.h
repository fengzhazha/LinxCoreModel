#pragma once

#ifndef CARG_H
#define CARG_H

#include <cstdint>
#include <vector>

namespace JCore {
class BARG {
public:
    /* BARG.BPC.CUR 64bits 当前块指令的块头地址，即当前块指令的BPC(块指针) */
    uint64_t                bpc = 0;
    /* BARG.BPC.TGT 64bits 跳转目标块的块头地址，指示块指令执行完成后的跳转位置, BPCN */
    uint64_t                target = 0;
    /* BARG.LRA 64bits 本地返回地址，用于分离块中返回块头的地址。LRA */
    uint64_t                localRetAddr = 0;
    /* BARG.PARA  本域段包含以下所有内容 */
    /* BARG.TYPE 2bits 块指令跳转类型，定义了当前块指令的跳转逻辑。*/
    uint64_t                blockTypeEncode = 0;
    uint64_t                branchTypeEncode = 0;
    /* BARG.TAKEN 1bits 跳转条件结果，1表示条件跳转成功，0表示条件不满足。*/
    bool                    taken = false;
    /* 块指令的执行序属性 */
    bool                    aq = false;
    bool                    rl = false;
    /* RegDst0-3 */
    std::vector<uint64_t>   dstATag;

    BARG() = default;
    ~BARG() = default;
    uint64_t GetBPC()
    {
        return bpc;
    }
    uint64_t GetTarget()
    {
        return target;
    }
    uint64_t GetLSROthers()
    {
        // is BARG[191:128], need implement
        return 0;
    }
};
}
#endif