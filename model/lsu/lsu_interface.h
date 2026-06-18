#pragma once
#include <deque>
#include <vector>

#include "ModelCommon/BlockCommand.h"
#include "core/Bus.h"
#include "core/Packet.h"

namespace JCore {

struct LSUInterface {
    /* \brief From LU to SU */
    std::vector<std::deque<MemReqBus>>                            lookup_lu_su_q;
    std::vector<std::deque<MemReqBus>>                            wait_lu_su_q;
    /* \brief From SU to LU */
    std::vector<std::deque<MemReqBus>>                            lookup_su_lu_q;
    std::vector<std::deque<MemReqBus>>                            wait_su_lu_q;
    std::vector<std::deque<MemReqBus>>                            bypass_su_lu_q;
    std::vector<std::deque<MemReqBus>>                            detect_su_lu_q;
    std::vector<std::deque<MemReqBus>>                            wakeup_su_lu_q;
    /* \brief From LU to L1 */
    std::vector<std::deque<MemReqBus>>               pref_lu_l1_array;
    std::vector<std::deque<MemReqBus>>               tag_lu_l1_array;
    std::vector<std::deque<MemReqBus>>               lookup_lu_l1_array;
    std::vector<std::deque<MemReqBus>>               tag_lu_scb_array;
    std::vector<std::deque<MemReqBus>>               lookup_lu_scb_array;
    /* \brief From L1 to LU */
    std::vector<std::deque<MemReqBus>>                            tag_l1_lu_q;
    std::vector<std::deque<MemReqBus>>                            lookup_l1_lu_q;
    std::vector<std::deque<MemReqBus>>                            tag_scb_lu_q;
    std::vector<std::deque<MemReqBus>>                            lookup_scb_lu_q;
    std::vector<SimQueue<PtrPacket>>                              wakeup_l1_lu_q;
    std::vector<SimQueue<MemReqBus>>                              wakeup_scb_lu_q;
    // std::deque<MemReqBus>                               snoop_l1_lu_q;
    std::vector<std::deque<MemReqBus>>                            pref_l1_lu_q;
    std::vector<SimQueue<MemReqBus>>                              upgrade_l1_lu_q;
    SimQueue<MemReqBus>                              atomic_lu_su_q;
    /* \brief From LU to MDB */
    std::deque<MDBBus>                               lookup_lu_mdb_q;
    std::deque<MDBBus>                               delete_lu_mdb_q;
    std::deque<MDBBus>                               record_lu_mdb_q;
    /* \brief From MDB to LU */
    std::deque<MDBBus>                               lookup_mdb_lu_q;
    /* \brief From MDB to SU */
    std::deque<MDBBus>                               lookup_mdb_su_q;
    /* \brief From Prefetcher to LU */
    SimQueue<MemReqBus>                              *pref_lu_q;
    /* \brief From LU to Prefetcher */
    SimQueue<MemReqBus>                              *lu_pref_q;
    SimQueue<PtrPrefFB>                              *feedback_lu_pref_q;
    /* \brief From Prefetcher to L2 */
    SimQueue<PtrPacket>                              *pref_l2_q;
    /* \brief From L2 to Prefetcher */
    SimQueue<PtrPrefFB>                              *l2_pref_q;
    /* \brief From SU to SCB */
    std::vector<SimQueue<MemReqBus>>                 commit_su_scb_array;
    /* \brief From LU to L2 */
    SimQueue<PtrPacket>                              *snoop_lu_l2_q;
    SimQueue<PtrPacket>                              *lookup_lu_l2_q;
    /* \brief From L1 to L2 */
    /* \brief From L2 to L1 */
    SimQueue<PtrPacket>                              *lookup_l2_l1_q;

    /* BCC->LSU */
    SimQueue<BlockCommandPtr>                       *bccTMABlockCmdQ;
    SimQueue<BlockCommandPtr>                       *tmaBCCWakeupQ;
    SimQueue<IDBus>                                 *bccLsuTloadCommitQ;
    SimQueue<IDBus>                                 *bccLsuTstoreCommitQ;
    // LSU->bridge
    std::vector<SimQueue<MemReqBus>*>               lsuBridgeTloadArray;
    std::vector<SimQueue<MemReqBus>*>               lsuBridgeTstoreArray;

    // LSU internal
    std::vector<SimQueue<BlockCommandPtr>>          lsuTMABlockCmdQ;

    void                                             Work();
};

} // namespace JCore