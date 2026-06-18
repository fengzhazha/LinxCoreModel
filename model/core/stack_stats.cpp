#include "core/stack_stats.h"

namespace JCore {

void StackStats::report() {
    rpt->ReportTitle("Stack Rename Stats");
    rpt->ReportValAndPct("Rename stall", stall_count, cycles);
    rpt->ReportAvg("Average Occpied Physical Register", (float)occupied_ptag_toal, (float)cycles);
    rpt->ReportValAndPct("Occpied Physical Register <10%", occupied_ptag_10, cycles);
    rpt->ReportValAndPct("Occpied Physical Register <25%", occupied_ptag_25, cycles);
    rpt->ReportValAndPct("Occpied Physical Register <50%", occupied_ptag_50, cycles);
    rpt->ReportValAndPct("Occpied Physical Register <75%", occupied_ptag_75, cycles);
    rpt->ReportValAndPct("Occpied Physical Register <90%", occupied_ptag_90, cycles);
    rpt->ReportValAndPct("Occpied Physical Register <=100%", occupied_ptag_100, cycles);
    rpt->ReportVal("Initial Load For Stack-set Count Total", init_load_set);
    rpt->ReportVal("Stack-set Total", stack_set);
    rpt->ReportVal("Stack-get Total", stack_get);
    rpt->ReportAvg("Average Renamed Block Number Per-cycle", renamed_block, cycles);
    rpt->ReportValAndPct("Renamed Block Number 0", renamed_block_0, cycles);
    rpt->ReportValAndPct("Renamed Block Number 1", renamed_block_1, cycles);
    rpt->ReportValAndPct("Renamed Block Number 2", renamed_block_2, cycles);
    rpt->ReportValAndPct("Renamed Block Number 3", renamed_block_3, cycles);
    rpt->ReportValAndPct("Renamed Block Number 4", renamed_block_4, cycles);
    rpt->ReportValAndPct("Renamed Block Number 5", renamed_block_5, cycles);
    rpt->ReportValAndPct("Renamed Block Number 6", renamed_block_6, cycles);
    rpt->ReportValAndPct("Renamed Block Number 7", renamed_block_7, cycles);
    rpt->ReportValAndPct("Renamed Block Number 8", renamed_block_8, cycles);
    rpt->ReportValAndPct("Renamed Block Number >8", renamed_block_more, cycles);
}

void StackStats::Reset()
{
    preg_count = 0;
    cycles = 0;
    stall_count = 0;
    occupied_ptag_toal = 0;
    occupied_ptag_10 = 0;
    occupied_ptag_25 = 0;
    occupied_ptag_50 = 0;
    occupied_ptag_75 = 0;
    occupied_ptag_90 = 0;
    occupied_ptag_100 = 0;
    init_load_set = 0;
    stack_set = 0;
    stack_get = 0;
    renamed_block = 0;
    renamed_block_0 = 0;
    renamed_block_1 = 0;
    renamed_block_2 = 0;
    renamed_block_3 = 0;
    renamed_block_4 = 0;
    renamed_block_5 = 0;
    renamed_block_6 = 0;
    renamed_block_7 = 0;
    renamed_block_8 = 0;
    renamed_block_more = 0;
}

} // namespace JCore