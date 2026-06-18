#include "mtccore/lsu/lsu_interface.h"

namespace JCore {

void MtcLSUInterface::Work()
{
    wakeup_l1_lu_q.Work();
    upgrade_l1_lu_q.Work();
    atomic_lu_su_q.Work();

    for (auto &q : commit_su_scb_array)
        q.Work();
    wakeup_scb_lu_q.Work();
    wakeup_l1_scalper_q.Work();
}

} // namespace JCore