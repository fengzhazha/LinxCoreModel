#include "utils/reporter.h"

#include <iomanip>
#include <sstream>


namespace JCore {

using namespace std;

namespace NS_PLAT {

Reporter::Reporter() = default;

void Reporter::ReportTitle(string const &title)
{
    int fillWidth = TOTAL_WIDTH - title.length();
    int leftWidth = fillWidth / floatPrec;
    int rightWidth = fillWidth - leftWidth;
    cout << string(leftWidth, '=') << title << string(rightWidth, '=') << endl;
}

void Reporter::ReportVal(string const &name, uint64_t val)
{
    cout << left << setw(NAME_WIDTH - 1) << setfill('.') << name << ':';
    cout << right << setw(VAL_WIDTH) << setfill(' ') << dec << val << endl;
}

void Reporter::ReportVal(string const &name, float val)
{
    cout << left << setw(NAME_WIDTH - 1) << setfill('.') << name << ':';
    cout << right << setw(VAL_WIDTH) << setfill(' ') << setiosflags(ios::fixed) << setprecision(floatPrec) << val;
    cout << endl;
}

void Reporter::ReportVal(string const &name, double val)
{
    cout << left << setw(NAME_WIDTH - 1) << setfill('.') << name << ':';
    cout << right << setw(VAL_WIDTH) << setfill(' ') << setiosflags(ios::fixed) << setprecision(floatPrec) << val;
    cout << endl;
}

void Reporter::ReportAvg(string const &name, uint64_t numerator, uint64_t denominator)
{
    ReportAvg(name, static_cast<float>(numerator), static_cast<float>(denominator));
}

void Reporter::ReportAvg(string const &name, float numerator, float denominator)
{
    cout << left << setw(NAME_WIDTH - 1) << setfill('.') << name << ':';
    cout << right << setw(VAL_WIDTH) << setfill(' ') << setiosflags(ios::fixed) << setprecision(floatPrec);
    if (denominator > 0.0) {
        cout << numerator / denominator << endl;
    } else {
        cout << "nan" << endl;
    }
}

void Reporter::ReportPct(string const &name, uint64_t numerator, uint64_t denominator)
{
    ReportPct(name, static_cast<float>(numerator), static_cast<float>(denominator));
}

void Reporter::ReportPct(string const &name, float numerator, float denominator)
{
    cout << left << setw(NAME_WIDTH - 1) << setfill('.') << name << ':';
    cout << right << setw(VAL_WIDTH) << setfill(' ');

    if (denominator >= 0.0) {
        cout << fixed << setprecision(floatPrec) << numerator / denominator * basePercent << '%' << endl;
    } else {
        cout << "nan%" << endl;
    }
}

void Reporter::ReportPct(string const &name, float rate)
{
    cout << left << setw(NAME_WIDTH-1) << setfill('.') << name << ':';
    cout << right << setw(VAL_WIDTH) << setfill(' ');
    cout << fixed << setprecision(floatPrec) << rate * basePercent << '%' << endl;
}

void Reporter::ReportValAndPct(string const &name, uint64_t numerator, uint64_t denominator)
{
    cout << left << setw(NAME_WIDTH-1) << setfill('.') << name << ':';
    cout << right << setw(VAL_WIDTH) << setfill(' ') << dec << numerator << ' ';
    cout << setw(PCT_WIDTH) << setfill(' ');
    if (denominator == 0) {
        cout << "inf%" << endl;
    } else {
        cout << fixed << setprecision(floatPrec)
            << (static_cast<float>(numerator) / static_cast<float>(denominator)) * basePercent << '%' << endl;
    }
}

void Reporter::ReportValAndPctFl(string const &name, double numerator, double denominator)
{
    cout << left << setw(NAME_WIDTH-1) << setfill('.') << name << ':';
    cout << right << setw(VAL_WIDTH) << setfill(' ') << dec << numerator << ' ';
    cout << setw(PCT_WIDTH) << setfill(' ');
    if (denominator) {
        cout << fixed << setprecision(floatPrec) << (float(numerator) / float(denominator)) * basePercent<< '%' << endl;
    } else {
        cout << "nan%" << endl;
    }
}

void Reporter::ReportHexCounter(const std::string &name, uint64_t pc, uint64_t counter)
{
    cout << left << name << setw((NAME_WIDTH - 1) - name.length()) << setfill('.') << hex << pc << ':';
    cout << right << setw(VAL_WIDTH) << setfill(' ') << dec << counter << endl;
}

void Reporter::ReportStallLoc(string const &name, uint64_t localBpc, uint64_t localTpc, uint64_t peerBpc, uint64_t val)
{
    stringstream ss;
    string newName;
    ss << name << "local_0x" << hex << localBpc << "_0x" << hex << localTpc << "---peer_0x" << hex << peerBpc;
    ss >> newName;
    cout << left << setw(NAME_WIDTH - 1) << setfill('.') << newName << ':';
    cout << right << setw(VAL_WIDTH) << setfill(' ') << setiosflags(ios::fixed) << setprecision(floatPrec) << val
         << endl;
}

void Reporter::ReportHistogram(string const &name, std::vector<uint64_t> &distribution)
{
    cout << left << setw(NAME_WIDTH - 1) << setfill('.') << name << ':';
    uint64_t total_sum = std::accumulate(distribution.begin(), distribution.end(), 0);
    cout << right << setw(VAL_WIDTH) << setfill(' ') << dec << total_sum << endl;
    if (distribution.size() == 0) {
        return;
    }

    // find the maximum non-zero index
    size_t low = 0;
    size_t high = distribution.size() - 1;
    size_t target_index = -1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (distribution[mid] > 0) {
            target_index = mid;
            low = mid + 1; 
        } else { 
            high = mid - 1;
        }
    }

    size_t real_size = target_index + 1;
    size_t num_partitions = real_size < MAX_HISTOGRAM_PARTITIONS ? real_size : MAX_HISTOGRAM_PARTITIONS;
    size_t base_size = real_size / num_partitions;
    size_t remainder = real_size % num_partitions;
    size_t current_start = 0;

    for (size_t i = 0; i < num_partitions; ++i) {
        size_t partition_size = base_size + (i < remainder ? 1 : 0);
        size_t current_end = current_start + partition_size;
        if (current_start >= real_size) {
            break;
        }
        uint64_t current_sum = std::accumulate(distribution.begin() + current_start,
            distribution.begin() + current_end, 0);

        std::stringstream ss;
        ss << "  |--[" << current_start << ", " << (current_end - 1) << "]";
        std::string prefix_str = ss.str();
        size_t prefix_length = prefix_str.length();

        if (prefix_length > NAME_WIDTH) {
            std::cerr << "Warning: Total width " << NAME_WIDTH << " is too small for prefix '" << prefix_str <<
                "'" << std::endl;
            std::cout << prefix_str << std::endl;
            return;
        }

        std::cout << left << std::setfill('.') << std::setw(NAME_WIDTH - 1) << prefix_str << ':';
        std::cout << right << setw(VAL_WIDTH) << setfill(' ') << dec << current_sum << std::endl;
        current_start = current_end;
    }
}

std::streambuf *Reporter::ReportSetOutStreamFile(string const &fileName)
{
    if (fout.is_open()) {
        fout.close();
    }
    fout.open(fileName, ios::out | ios::trunc);
    /* return Store cout stream point */
    return cout.rdbuf(fout.rdbuf());
}

std::streambuf *Reporter::ReportSetOutStreamFile(string const &fileName, bool isApp)
{
    if (fout.is_open()) {
        fout.close();
    }

    if (isApp) {
        fout.open(fileName, ios::out | ios::app);
    } else {
        fout.open(fileName, ios::out | ios::ate);
    }

    /* return Store cout stream point */
    return cout.rdbuf(fout.rdbuf());
}

void Reporter::ReportResetOutStreamCout(std::streambuf *pOld)
{
    if (pOld == nullptr) {
        return;
    }
    /* Reset OutStream To cout */
    cout.rdbuf(pOld);
    if (fout.is_open()) {
        fout.close();
    }
}

}

} // namespace JCore