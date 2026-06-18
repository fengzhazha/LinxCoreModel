#pragma once
#include <deque>
#include <vector>
#include "../interface/CommitInfo.h"
#include "core/Bus.h"
#include "core/Packet.h"

namespace JCore {

struct MtcLSUInterface {
    /* \brief From LU to SU */
    std::deque<MemReqBus>                            lookup_lu_su_q;
    std::deque<MemReqBus>                            wait_lu_su_q;
    /* \brief From SU to LU */
    std::deque<MemReqBus>                            lookup_su_lu_q;
    std::deque<MemReqBus>                            wait_su_lu_q;
    std::deque<MemReqBus>                            bypass_su_lu_q;
    std::deque<MemReqBus>                            detect_su_lu_q;
    std::deque<MemReqBus>                            wakeup_su_lu_q;
    /* \brief From LU to L1 */
    std::vector<std::deque<MemReqBus>>               pref_lu_l1_array;
    std::vector<std::deque<MemReqBus>>               tag_lu_l1_array;
    std::vector<std::deque<MemReqBus>>               tag_scalper_l1_array;
    std::vector<std::deque<MemReqBus>>               lookup_lu_l1_array;
    std::vector<std::deque<MemReqBus>>               tag_lu_scb_array;
    std::vector<std::deque<MemReqBus>>               lookup_lu_scb_array;
    /* \brief From L1 to LU */
    std::deque<MemReqBus>                            tag_l1_lu_q;
    std::deque<MemReqBus>                            tag_l1_scalper_q;
    std::deque<MemReqBus>                            lookup_l1_lu_q;
    std::deque<MemReqBus>                            tag_scb_lu_q;
    std::deque<MemReqBus>                            lookup_scb_lu_q;
    SimQueue<PtrPacket>                              wakeup_l1_lu_q;
    SimQueue<MemReqBus>                              wakeup_scb_lu_q;
    SimQueue<MemReqBus>                              wakeup_l1_scalper_q;
    // std::deque<MemReqBus>                               snoop_l1_lu_q;
    std::deque<MemReqBus>                            pref_l1_lu_q;
    SimQueue<MemReqBus>                              upgrade_l1_lu_q;
    SimQueue<MemReqBus>                              atomic_lu_su_q;
    /* \brief From LU to MDB */
    std::deque<MDBBus>                               lookup_lu_mdb_q;
    std::deque<MDBBus>                               delete_lu_mdb_q;
    std::deque<MDBBus>                               record_lu_mdb_q;

    std::deque<SimInst>*      iexScalperInstQ;

    std::deque<CommitInfo>                             *pe_scalper_commit_q;

    std::deque<CommitInfo>                            scLdqOrder;

    std::deque<MemReqBus>                              *peScbWrQ;
    /* \brief From MDB to LU */
    std::deque<MDBBus>                               lookup_mdb_lu_q;
    /* \brief From MDB to SU */
    std::deque<MDBBus>                               lookup_mdb_su_q;
    /* \brief From MtcPrefetcher to LU */
    SimQueue<MemReqBus>                              *pref_lu_q;
    /* \brief From LU to MtcPrefetcher */
    SimQueue<MemReqBus>                              *lu_pref_q;
    SimQueue<PtrPrefFB>                              *feedback_lu_pref_q;
    /* \brief From MtcPrefetcher to L2 */
    SimQueue<PtrPacket>                              *pref_l2_q;
    /* \brief From L2 to MtcPrefetcher */
    SimQueue<PtrPrefFB>                              *l2_pref_q;
    /* \brief From SU to SCB */
    std::vector<SimQueue<MemReqBus>>                 commit_su_scb_array;
    /* \brief From LU to L2 */
    SimQueue<PtrPacket>                              *snoop_lu_l2_q;
    SimQueue<PtrPacket>                              *lookup_lu_l2_q;
    /* \brief From L1 to L2 */
    /* \brief From L2 to L1 */
    SimQueue<PtrPacket>                              *lookup_l2_l1_q;

    SimQueue<SimInst>*                                  pe_iex_sc_lda_array;
    SimQueue<SimInst>*                                  pe_iex_lda_array;

    void                                              Work();
};

} // namespace JCore