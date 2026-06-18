#pragma once

#include <deque>
#include <functional>
#include "ModelCommon/bus/RFReqBus.h"
#include "ModelCommon/bus/MemRequest.h"
#include "iex/iex_latency.h"
#include "ModelCommon/ModelCommon.h"
#include "ModelCommon/bus/FlushBus.h"


namespace JCore {

/*
    Input:  IQ seleted, bypass e1/w1/w2 instructions, RF/LSU return

    Output: WB regfile(treg, lgpr), PROB resolve, RF/LSU request

    Stages: RF, EXE, WB

    Ctrl: flush
*/

/*
    Stage P1: interface only (actually this is issue stage)
    Stage I1: generate and send RF read request
    Stage I2: receive RF return and e1/w1/w2 bypass data
    Stage E0: iterate cycles, ALUPipe only. The '64' in latency=64+6 instr, e.g. FDIV, FSQRT
    Stage E1: execute and send ld/st request (sta/bru resolve in this stage)
    Stage E2~E4: execute stages for lda
    Stage W1: receive load data return, send RF write request (could be canceled)
    Stage W2: instruction resolve
*/

class SimSys;
class IEX;

using PipeStallFn_t = std::function<bool(const SimInst&)>;

struct CMDPipe {
    INPUT RFReqBus                  rf_data_ret;
    OUTPUT RFReqBus                 rf_rd_req;
    OUTPUT std::vector<std::vector<SimQueue<PEResolveBus>*>>    rslv_array;
    OUTPUT std::vector<SimQueue<SimInst>*>   rf_ct_q;
    OUTPUT SimQueue<RFReqBus>       *iex_rt_wr_q;

    uint32_t                        pipeid;

    SimInst                           *p1_inst;
    SimInst                           i1_inst;
    SimInst                           i2_inst;
    SimInst                           e1_inst;
    SimInst                           w1_inst;

    CMDPipe() : iex_rt_wr_q(nullptr), pipeid(0), p1_inst(nullptr), sim(nullptr), top(nullptr) {};
    SimSys                          *sim;
    IEX                             *top;

    void                            Build(uint32_t id);
    void                            Reset();
    void                            Work();
    void                            move();
    void                            moveLpv();
    SimSys                          *GetSim() { return sim; }
    void                            flush(FlushBus &flushReq);

    void                            runP1();
    void                            runI1();
    void                            runI2();
    void                            runE1();
    void                            runW1();

    void                            LogPrint();
    bool                            CheckTileRegStall();
};

struct ALUPipe {
    INPUT RFReqBus                  rf_data_ret;
    INPUT RFReqBus                  sys_data_ret;
    OUTPUT RFReqBus                 rf_rd_req;
    OUTPUT std::vector<RFReqBus>    rf_wr_req;
    OUTPUT RFReqBus                 sys_rd_req;
    OUTPUT RFReqBus                 sys_wr_req;
    OUTPUT std::vector<std::vector<SimQueue<PEResolveBus>*>>    rslv_array;
    OUTPUT std::vector<SimQueue<SimInst>*>   rf_ct_q;
    OUTPUT SimQueue<RFReqBus>       *iex_rt_wr_q;

    uint32_t                        pipeid;
    uint32_t                        ex_stage_num;

    SimInst                           *p1_inst;
    SimInst                           i1_inst;
    SimInst                           i2_inst;
    std::deque<std::pair<uint32_t, SimInst>> e0_inst; // <latency, inst> for 64+6 (iter stage)
    std::set<IexExecUnit>           e0_unit;
    std::vector<std::vector<SimInst>> ex_inst; // E1-E5
    std::vector<SimInst>              w1_inst;
    std::set<uint64_t>              w1_flag; // <time> indicates there is inst wr at time
    std::vector<SimInst>              w2_inst;
    uint32_t                        iqDeallocOption = 0; // 0:E1, 1: I2
    std::vector<SimInst>              release_iq_inst;

    ALUPipe() : iex_rt_wr_q(nullptr), pipeid(0), ex_stage_num(0), p1_inst(nullptr), sim(nullptr), top(nullptr) {};
    SimSys                          *sim;
    IEX                             *top;

    void                            Build(uint32_t id);
    void                            Reset();
    void                            Work();
    void                            move();
    void                            moveLpv();
    SimSys                          *GetSim() { return sim; }
    void                            flush(FlushBus &flushReq);
    bool                            isPipeStallByScb(const SimInst &inst, uint32_t& noBusCfltCycles) const;
    bool                            isPipeStallByIterCycles(const SimInst &inst) const;

    void                            runP1();
    void                            runI1();
    void                            runI2();
    void                            runE0();
    void                            runEx(Stage stage);
    void                            executeInstr(const SimInst &inst, Stage stage); // called at last Ex tick
    void                            runW1();
    void                            runW2();

    IexExecUnit                     getExecUnit(const SimInst &inst) const;
    uint32_t                        getExLatency(const SimInst &inst) const;
    uint32_t                        getIterCycles(const SimInst &inst) const;
    void                            LogPrint();
};

struct LDAPipe {
    INPUT RFReqBus                  rf_data_ret;
    INPUT RFReqBus                  sys_data_ret;
    OUTPUT RFReqBus                 rf_rd_req;
    OUTPUT RFReqBus                 rf_wr_req;
    OUTPUT RFReqBus                 rf_ld_wr_req;
    OUTPUT RFReqBus                 sys_rd_req;
    INPUT std::vector<SimQueue<MemReqBus>*>        lda_ret_q;
    OUTPUT std::vector<SimQueue<MemReqBus>*>       lda_req_q;
    OUTPUT std::vector<std::vector<SimQueue<PEResolveBus>*>>    rslv_array;
    OUTPUT SimQueue<RFReqBus>       *iex_rt_wr_q;

    uint32_t                        pipeid;

    SimInst                           *p1_inst;
    SimInst                           i1_inst;
    SimInst                           i2_inst;
    SimInst                           e1_inst;
    SimInst                           e2_inst;
    SimInst                           e3_inst;
    SimInst                           e4_inst;
    SimInst                           w1_inst;
    SimInst                           w2_inst;

    LDAPipe() : iex_rt_wr_q(nullptr), pipeid(0), p1_inst(nullptr), sim(nullptr), top(nullptr), memStall(false) {};
    SimSys                          *sim;
    IEX                             *top;
    bool                            memStall = false;

    void                            Build(uint32_t id);
    void                            Reset();
    void                            Work();
    SimSys                          *GetSim() { return sim; }
    void                            move();
    void                            moveLpv();
    void                            flush(FlushBus &flushReq);

    void                            runP1();
    void                            runI1();
    void                            runI2();
    void                            runE1();
    void                            runE2();
    void                            runE3();
    void                            runE4();
    void                            runW1();
    void                            runW2();
    bool                            checkLSUStall(uint32_t stid);

    void                            LogPrint();
};

struct STAPipe {
    INPUT RFReqBus                  rf_data_ret;
    INPUT RFReqBus                  sys_data_ret;
    OUTPUT RFReqBus                 rf_rd_req;
    OUTPUT RFReqBus                 rf_wr_req;
    OUTPUT std::vector<SimQueue<MemReqBus>*> sta_req_q;
    OUTPUT RFReqBus                 sys_rd_req;
    OUTPUT std::vector<std::vector<SimQueue<PEResolveBus>*>>    rslv_array;
    OUTPUT SimQueue<RFReqBus>       *iex_rt_wr_q;

    uint32_t                        pipeid;

    SimInst                           *p1_inst;
    SimInst                           i1_inst;
    SimInst                           i2_inst;
    SimInst                           e1_inst;
    SimInst                           w1_inst;
    SimInst                           w2_inst;

    STAPipe() : iex_rt_wr_q(nullptr), pipeid(0), p1_inst(nullptr), sim(nullptr), top(nullptr), memStall(false) {};
    SimSys                          *sim;
    IEX                             *top;
    bool                            memStall = false;

    void                            Build(uint32_t id);
    void                            Reset();
    void                            Work();
    SimSys                          *GetSim() { return sim; }
    void                            move();
    void                            moveLpv();
    void                            flush(FlushBus &flushReq);

    void                            runP1();
    void                            runI1();
    void                            runI2();
    void                            runE1();
    void                            runW1();
    void                            runW2();
    bool                            checkLSUStall();

    void                            LogPrint();
    void                            TstoreSendSeqToMscb(SimInst &inst) const;
    void                            TStoreSendNDReqToMscb(SimInst &inst, MemReqBus &scbWrReq) const;
    void                            TStoreSendNormReqToMscb(SimInst &inst, MemReqBus &scbWrReq) const;
    void                            InsertTlsPipeViewToDFX(SimInst &inst);
};

struct STDPipe {
    INPUT RFReqBus                  rf_data_ret;
    INPUT RFReqBus                  sys_data_ret;
    OUTPUT RFReqBus                 rf_rd_req;
    OUTPUT RFReqBus                 rf_wr_req;
    OUTPUT RFReqBus                 sys_rd_req;
    OUTPUT std::vector<SimQueue<MemReqBus>*> std_req_q;
    OUTPUT std::vector<std::vector<SimQueue<PEResolveBus>*>>    rslv_array;

    uint32_t                        pipeid = 0;

    SimInst                           *p1_inst = nullptr;
    SimInst                           i1_inst;
    SimInst                           i2_inst;
    SimInst                           e1_inst;

    STDPipe() : pipeid(0), p1_inst(nullptr), sim(nullptr), top(nullptr) {};
    SimSys                          *sim;
    IEX                             *top;

    void                            Build(uint32_t id);
    void                            Reset();
    void                            Work();
    SimSys                          *GetSim() { return sim; }
    void                            move();
    void                            moveLpv();
    void                            flush(FlushBus &flushReq);

    void                            runP1();
    void                            runI1();
    void                            runI2();
    void                            runE1();
    bool                            checkLSUStall();

    void                            LogPrint();
};

struct BRUPipe {
    INPUT RFReqBus                  rf_data_ret;
    INPUT RFReqBus                  sys_data_ret;
    OUTPUT RFReqBus                 rf_rd_req;
    OUTPUT RFReqBus                 rf_wr_req;
    OUTPUT RFReqBus                 sys_rd_req;
    OUTPUT std::vector<std::vector<SimQueue<PEResolveBus>*>>    rslv_array;
    OUTPUT SimQueue<RFReqBus>       *iex_rt_wr_q;

    uint32_t                        pipeid = 0;

    SimInst                           *p1_inst = nullptr;
    SimInst                           i1_inst;
    SimInst                           i2_inst;
    SimInst                           e1_inst;

    BRUPipe() : iex_rt_wr_q(nullptr), pipeid(0), p1_inst(nullptr), sim(nullptr), top(nullptr) {};
    SimSys                          *sim;
    IEX                             *top;

    void                            Build(uint32_t id);
    void                            Reset();
    void                            Work();
    SimSys                          *GetSim() { return sim; }
    void                            move();
    void                            moveLpv();
    void                            flush(FlushBus &flushReq);

    void                            runP1();
    void                            runI1();
    void                            runI2();
    void                            runE1();

    void                            LogPrint();
};

struct AGUPipe {
    INPUT RFReqBus                  rf_data_ret;
    INPUT RFReqBus                  sys_data_ret;
    OUTPUT RFReqBus                 rf_rd_req;
    OUTPUT RFReqBus                 rf_wr_req;
    OUTPUT RFReqBus                 rf_ld_wr_req;
    OUTPUT RFReqBus                 sys_rd_req;
    INPUT std::vector<SimQueue<MemReqBus>*> lda_ret_q;
    OUTPUT std::vector<SimQueue<MemReqBus>*> lda_req_q;
    OUTPUT std::vector<std::vector<SimQueue<PEResolveBus>*>>    rslv_array;
    OUTPUT SimQueue<RFReqBus>       *iex_rt_wr_q = nullptr;
    OUTPUT std::vector<SimQueue<MemReqBus>*> sta_req_q;

    uint32_t                        pipeid;

    SimInst                           *p1_inst;
    SimInst                           i1_inst;
    SimInst                           i2_inst;
    SimInst                           e1_inst;
    SimInst                           e2_inst;
    SimInst                           e3_inst;
    SimInst                           e4_inst;
    SimInst                           w1_inst;
    SimInst                           w2_inst;

    AGUPipe() : pipeid(0), p1_inst(nullptr), sim(nullptr), top(nullptr), memStall(false) {};
    SimSys                          *sim;
    IEX                             *top;
    bool                            memStall = false;

    void                            Build(uint32_t id);
    void                            Reset();
    void                            Work();
    SimSys                          *GetSim() { return sim; }
    void                            move();
    void                            moveLpv();
    void                            flush(FlushBus &flushReq);

    void                            runP1();
    void                            runI1();
    void                            runI2();
    void                            runE1();
    void                            runE1Load(const SimInst &inst);
    void                            runE1Store(const SimInst &inst);
    void                            LocalStore(const SimInst &inst);
    void                            runE2();
    void                            runE3();
    void                            runE4();
    void                            runW1();
    void                            runW2();
    bool                            checkLSUStall(uint32_t stid);
    MemRequest                      CreateMemReq(const SimInst &inst, bool isLoad, uint64_t mask,
                                                 bool toMem);
    void                            HandleVabLd(const SimInst &inst, bool toMem, const VecData &addrs);
    void                            HandleGatherSplit(const SimInst &inst, const VecData &addrs) const;
    void                            CancelGather(const SimInst &inst) const;

    void                            LogPrint();
};

struct VecScalarPipe {
    INPUT RFReqBus                  rf_data_ret;
    INPUT RFReqBus                  sys_data_ret;
    OUTPUT RFReqBus                 rf_rd_req;
    OUTPUT std::vector<RFReqBus>    rf_wr_req;
    OUTPUT RFReqBus                 sys_rd_req;
    OUTPUT RFReqBus                 sys_wr_req;
    OUTPUT std::vector<std::vector<SimQueue<PEResolveBus>*>>    rslv_array;
    OUTPUT std::vector<SimQueue<SimInst>*>   rf_ct_q;
    OUTPUT SimQueue<RFReqBus>       *iex_rt_wr_q;

    uint32_t                        pipeid;
    uint32_t                        ex_stage_num;

    SimInst                           *p1_inst;
    SimInst                           i1_inst;
    std::vector<std::vector<SimInst>> ex_inst; // E1-E5
    std::vector<SimInst>              w1_inst;
    std::set<uint64_t>              w1_flag; // <time> indicates there is inst wr at time
    std::vector<SimInst>              w2_inst;
    std::vector<unsigned>           branchMaskCount;

    VecScalarPipe() : iex_rt_wr_q(nullptr), pipeid(0), ex_stage_num(0), p1_inst(nullptr), sim(nullptr), top(nullptr) {};
    SimSys                          *sim;
    IEX                             *top;

    void                            Build(uint32_t id);
    void                            Reset();
    void                            Work();
    void                            move();
    void                            moveLpv();
    SimSys                          *GetSim() { return sim; }
    void                            flush(FlushBus &flushReq);
    bool                            isPipeStallByScb(const SimInst &inst, uint32_t& noBusCfltCycles) const;

    void                            runP1();
    void                            runI1();
    void                            runI2();
    void                            runEx(Stage stage);
    void                            executeInstr(const SimInst &inst, Stage stage); // called at last Ex tick
    void                            ExecJump(const SimInst &inst);
    void                            runW1();
    void                            runW2();

    uint32_t                        getExLatency(const SimInst &inst) const;
    void                            LogPrint();
};

} // namespace JCore
