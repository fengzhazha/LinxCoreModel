#include "lsu/lsu_interface.h"

namespace JCore {

void LSUInterface::Work()
{
    for (auto &q : wakeup_l1_lu_q) {
        q.Work();
    }
    for (auto &q : upgrade_l1_lu_q) {
        q.Work();
    }
    atomic_lu_su_q.Work();

    for (auto &q : commit_su_scb_array) {
        q.Work();
    }
    for (auto &q : wakeup_scb_lu_q) {
        q.Work();
    }
    for (auto &q : lsuTMABlockCmdQ) {
        q.Work();
    }
}

} // namespace JCore