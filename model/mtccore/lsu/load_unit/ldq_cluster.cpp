#include "mtccore/lsu/load_unit/ldq_cluster.h"

#include <algorithm>

namespace JCore {

/* Entry */
void MTCLUEntryInfo::Reset(void)
{
    ldqHit = false;
    l1Hit = false;
    ldqRnt = false;
    l1Rnt = false;
    storeBypass = false;
    scbRnt = false;
    stqRnt = false;
    mdbHit = false;
    fsm = MTC_LDQ_IDLE;
    waitStore = false;
    mdbStall = false;
    waitStoreTpc = 0;
    crossLine = false;
    hitCnt = 0;
    waitingStoreCycle = 0; // stall_cycle
    delayCnt = 0;
}

void MTCLUEntryInfo::insert(MemReqBus &bus)
{
    memReq = bus;
    memReq.cID = cID;
    memReq.eID = eID;
    fsm = MTC_LDQ_WAIT;
    delayCnt = 1;
    crossLine = bus.isCrossCacheLine;

    memReq.mtc_reqData.Reset();
}

void MTCLUEntryInfo::rewait(void)
{
    memReq.mtc_reqData.Reset();
    ldqRnt = false;
    l1Rnt = false;
    scbRnt = false;
    stqRnt = false;
}

bool MTCLUEntryInfo::IsWorking(void)
{
    return  fsm != MTC_LDQ_RESOLVED && fsm != MTC_LDQ_IDLE;
}

bool MTCLUEntryInfo::IsIdle(void)
{
    return fsm == MTC_LDQ_IDLE;
}

bool MTCLUEntryInfo::dataHit()
{
    return (ldqHit || l1Hit || storeBypass);
}

/* Data field */
void MtcDataField::Reset()
{
    data.Reset();
    tag = 0;
    vld = false;
}

/* Cluster data */
void MtcClusterData::Build(uint32_t bufferSize)
{
    dField.resize(bufferSize);
}

void MtcClusterData::replace(uint64_t t, ReqData256 &d)
{
    if (dField.size() == 0) {
        return;
    }

    if (dField[replaceIdx].vld) {
        top->setLdqHitInfo(dField[replaceIdx].tag, false);
    }

    dField[replaceIdx].vld = true;
    dField[replaceIdx].tag = CalTag(t);
    dField[replaceIdx].data.extractFrom(d.data, d.positionVld);
    replaceIdx = (replaceIdx + 1) % dField.size();

    top->setLdqHitInfo(t, true);
}

void MtcClusterData::merge(uint64_t t, ReqData256 &d)
{
    if (dField.size() == 0) {
        return;
    }

    for (uint32_t i = 0; i < dField.size(); ++i)
        if (dField[i].vld && t == dField[i].tag) {
            dField[i].data.extractFrom(d.data, d.positionVld);
            return;
        }
    replace(t, d);
}

uint2048_t MtcClusterData::getData(uint64_t t)
{
    for (uint32_t i = 0; i < dField.size(); ++i)
        if (dField[i].vld && t == dField[i].tag)
            return dField[i].data;

    ASSERT(0 && "no Data");
    return uint2048_t();
}

bool MtcClusterData::lookup(uint64_t t)
{
    if (dField.size() == 0) {
        return false;
    }

    for (uint32_t i = 0; i < dField.size(); ++i)
        if (dField[i].vld && t == dField[i].tag)
            return true;
    return false;
}

void MtcClusterData::Reset()
{
    for (auto &d : dField) {
        d.data.Reset();
        d.tag = 0;
        d.vld = false;
    }
    replaceIdx = 0;
}

/* Cluster */
void MtcClusterInfo::Reset(void)
{
    for (auto &e : entryArr) {
        e.Reset();
    }
    size = 0;
    cData.Reset();
}

void MtcClusterInfo::Build(uint32_t clusterDpeth, uint32_t cID, uint32_t cDataSize)
{
    entryArr = std::vector<MTCLUEntryInfo>(clusterDpeth);
    for (uint32_t i = 0; i < entryArr.size(); ++i) {
        entryArr[i].eID = i;
        entryArr[i].cID = cID;
    }

    cData.top = this;
    cData.Build(cDataSize);
}

bool MtcClusterInfo::checkHit(uint64_t tag)
{
    return cData.lookup(tag);
}

void MtcClusterInfo::setLdqHitInfo(uint64_t tag, bool hit)
{
    for (auto &e : entryArr) {
        if (e.IsWorking() && e.memReq.tag == tag) {
            e.ldqHit = hit;
            if (!e.ldqHit && !e.l1Hit) {
                e.fsm = MTC_LDQ_L1_DC_MISS;
            }
        }
    }
}

bool MtcClusterInfo::IsIteratorEnding(uint32_t it)
{
    return it >= size;
}

} // namespace JCore