#pragma once

#include <vector>
#include <random>
#include "core/Bus.h"
#include "GFUSim.h"
#include "ModelSpec.h"

#ifndef RANDOM_MODEL_SOC_H
#define RANDOM_MODEL_SOC_H

namespace JCore {

struct GammaGenerator {
    std::mt19937 rng;
    std::gamma_distribution<double> dist;
    double threshold;
};

class RandomModelSoC : public SimObj {
    SimSys* sim = nullptr;

public:
    RandomModelSoC();
    ~RandomModelSoC() override;
    void    Build(SimSys* simSys);
    std::vector<GFUMemReq>          gfuL2ReqList;
    DelayRandomQueue<GFUMemReq>     randomModelQueue;

    void                            Work() override;
    void                            Xfer() override;
    void                            Reset() override;
    void                            ReportStat() override {};
    static void SimMemOper(SimSys *sim, GFUMemReq &req);
    static void CheckReqSize(const GFUMemReq& gfuMemReq);
private:
    uint32_t GetTnxNum(const uint64_t addr, const uint32_t size) const;
    void SetQueue(const GFUMemReq &gfuMemReq);
    void SetCredit(const GFUMemReq &gfuMemReq);
    void SetDelayQueue(const GFUMemReq &gfuMemReq);
    bool IsCreditEnough(const GFUMemReq &gfuMemReq);
    void UpdateCredit();
    uint32_t GetRspNum() const;

    void SetupGammaGen(uint32_t slotId, uint32_t mean, uint32_t variance, uint32_t seed);
    uint32_t GammaGen(uint32_t slotId);

    uint32_t l2ReadCredit = 0;
    uint32_t l2WriteCredit = 0;
    uint32_t maxReadCredit = 4096;  // Can config from core.toml
    uint32_t maxWriteCredit = 4096; // Can config from  core.toml

    GammaGenerator gamma[2];
};


} // namespace JCore

#endif