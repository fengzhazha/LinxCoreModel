#pragma once

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <numeric>

namespace JCore {

namespace NS_PLAT {

constexpr int TOTAL_WIDTH = 60;
constexpr int NAME_WIDTH = 50;
constexpr int VAL_WIDTH = 10;
constexpr int PCT_WIDTH = 7;
constexpr int INDENT_WIDTH = 2;
constexpr int MAX_HISTOGRAM_PARTITIONS = 8;

class Reporter {
    std::ofstream fout;

public:
    static const int floatPrec = 2;
    static const int basePercent = 100;
    Reporter();

    static void PrintName(std::string const &name, uint32_t len);
    static void ReportTitle(const std::string &title);
    static void ReportVal(const std::string &name, uint64_t val);
    static void ReportVal(const std::string &name, float val);
    static void ReportVal(const std::string &name, double val);
    static void ReportAvg(const std::string &name, uint64_t numerator, uint64_t denominator);
    static void ReportAvg(const std::string &name, float numerator, float denominator);
    static void ReportPct(const std::string &name, uint64_t numerator, uint64_t denominator);
    static void ReportPct(const std::string &name, float numerator, float denominator);
    static void ReportPct(const std::string &name, float rate);
    static void ReportValAndPct(const std::string &name, uint64_t numerator, uint64_t denominator);
    static void ReportValAndPctFl(const std::string &name, double numerator, double denominator);
    static void ReportHexCounter(const std::string &name, uint64_t pc, uint64_t counter);
    static void ReportStallLoc(const std::string &name, uint64_t localBpc, uint64_t localTpc, uint64_t peerBpc,
                               uint64_t val);
    static void ReportHistogram(const std::string &name, std::vector<uint64_t> &distribution);
    std::streambuf *ReportSetOutStreamFile(const std::string &fileName);
    std::streambuf *ReportSetOutStreamFile(const std::string &fileName, bool isApp);

    void ReportResetOutStreamCout(std::streambuf *pOld);
};
} // namespace NS_PLAT
} // namespace JCore