#pragma once

#include "../configs/iex_config.h"
#include "ModelCommon/ModelCommon.h"
#include "ModelCommon/SimInstInfo.h"

namespace JCore {


/*
    Input: vector<SimInst*> pipe_i2(read bypass tag), vector<SimInst*> pipe_e1/e4/w1/w2(bypass source)

    Crossbar: w1-i2, w2-i2, e1-i2

    Output: vector<SimInst*> pipe_i2
*/
enum class BypassType {
    E1_I2 = 0,
    E2_I2,
    E3_I2,
    E4_I2,
    E5_I2,
    W1_I2,
    W2_I2,
    BYPASS_TYPE_NUM
};

enum class BypassPipe {
    CMD_PIPE = 0,
    ALU_PIPE,
    LDA_PIPE,
    STA_PIPE,
    STD_PIPE,
    BRU_PIPE,
    BYPASS_PIPE_NUM
};

class SymSys;
class IEX;

class BypassNetwork {
private:

public:
    BypassNetwork() {};
    ~BypassNetwork() {};

    IEX                                 *top;
    SimSys                              *GetSim();

    INPUT OUTPUT std::vector<SimInst*>    pipe_i2;
    INPUT std::vector<SimInst*>           pipe_e1;
    INPUT std::vector<SimInst*>           pipe_e2;
    INPUT std::vector<SimInst*>           pipe_e3;
    INPUT std::vector<SimInst*>           pipe_e4;
    INPUT std::vector<SimInst*>           pipe_e5;
    INPUT std::vector<SimInst*>           pipe_w1;
    INPUT std::vector<SimInst*>           pipe_w2;

    void                                Build();
    void                                Reset();
    void                                Xfer();

    /* \brief Pipe e1 to i2 bypass */
    void                                E1Bypass();
    /* \brief Pipe e2 to i2 bypass */
    void                                E2Bypass();
    /* \brief Pipe e3 to i2 bypass */
    void                                E3Bypass();
    /* \brief Pipe e4 to i2 bypass */
    void                                E4Bypass();
    /* \brief Pipe e5 to i2 bypass */
    void                                E5Bypass();
    /* \brief Pipe w1 to i2 bypass */
    void                                W1Bypass();
    /* \brief Pipe w2 to i2 bypass */
    void                                W2Bypass();
    /* \brief bypass register */
    void                                RegBypass(SimInst rInst, SimInst wInst);
    bool                                OperandMatch(SimInst rInst, POperandPtr src, SimInst wInst, POperandPtr dst);
    void                                ProcessSimtBypass(SimInst &wInst, ExecEngineTyp anotherIdMem, BypassType type);
    void                                ProcessBypass(BypassType type, std::vector<SimInst*>& w_pipe,
                                                      std::vector<SimInst*>& r_pipe);
};

} // namespace JCore
