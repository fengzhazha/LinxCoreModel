#ifndef GFU2_CROSSBAR_H
#define GFU2_CROSSBAR_H

#include <functional>
#include <map>

#include "CacheLog.h"
#include "CacheMsg.h"
#include "core/Bus.h"
#include "CrossbarStats.h"
#include "L2Stats.h"

template<class T>
class Crossbar{
private:
    struct VoqEntry
    {
        int bufIdx;
        uint32_t enqueuedCycle;
        uint32_t latency;
    };

    class OutputSch{
    public:
        OutputSch(int voqNum, Crossbar &parent): voqNum(voqNum),schIdx(0),
                parent(parent) {
            for(int i = 0; i<voqNum; i++){
                voq.push_back(new RingQueue<VoqEntry>());
            }
        }

        ~OutputSch(){
            for(int i = 0; i<voqNum; i++){
                if(voq[i]!= nullptr){
                    delete voq[i];
                }
            }
        }

    private:
        int voqNum;

        int schIdx;

        friend class Crossbar;
        Crossbar &parent;

        std::vector<RingQueue<VoqEntry>*> voq;

        uint64_t getCycles() const
        {
            return parent.stats.cycles;
        }

    public:
        std::function<bool(T)>sendPkt;

        void bindCallback(std::function<bool(T)>_sendPkt){
            sendPkt=_sendPkt;
        }

        bool insert(int i, int idx, uint32_t latency){
            if(idx >= voqNum){
                return false;
            }
            VoqEntry e;
            e.bufIdx = i;
            e.enqueuedCycle = getCycles();
            e.latency = latency;
            voq[idx]->write(e);
            return true;
        }

        bool schPkt(int &bufIdx,int &portId){
            bool ret = false;
            uint32_t currentCycle = getCycles();
            for(int i = 0; i <voqNum; i++){
                if(voq[schIdx]->empty()){
                    schIdx=(schIdx + 1)%voqNum;
                    continue;
                }
                VoqEntry e = voq[schIdx]->Front();
                if (currentCycle - e.enqueuedCycle < e.latency) {
                    schIdx=(schIdx + 1)%voqNum;
                    continue;
                }

                voq[schIdx]->read();
                bufIdx = e.bufIdx;
                portId = schIdx;
                ret = true;
                schIdx = (schIdx + 1) % voqNum;
                break;
            }
            return ret;
        }
    };

private:
    uint8_t crossbarId; // for debugging
    int maxBufSize;
    int cpuSidePortNum;
    int memSidePortNum;
    std::vector<std::vector<T>*> cpuSideBufList;
    std::vector<std::vector<T>*> memSideBufList;
    std::vector<OutputSch*>cpuSidePktOsList;
    std::vector<OutputSch*>memSidePktOsList;

    std::map<uint32_t, size_t> cpuSidePortIdToIndex;
    std::map<uint32_t, size_t> memSidePortIdToIndex;

    std::vector<uint32_t> cpuSidePortIndexToId;
    std::vector<uint32_t> memSidePortIndexToId;

    std::function<uint32_t(T pkt)> _calcCpuSidePortId;
    std::function<uint32_t(T pkt)> _calcMemSidePortId;

    std::function<uint32_t(T pkt)> calcCpuToMemLatency;
    std::function<uint32_t(T pkt)> calcMemToCpuLatency;

    int calcCpuSidePortIndex(T pkt);
    void addCpuSidePortId(uint32_t cpuSidePortId);
    void addMemSidePortId(uint32_t memSidePortId);
    int calcMemSidePortIndex(T pkt);

public:
    Crossbar();
    ~Crossbar();

    TopCacheStat     *topCacheStat;
    CrossbarStats   stats;
    CacheLog        *logger;

    void Build(int maxBufSizePara, int cpuSidePortNumPara, int memSidePortNumPara);

    void setCrossbarId(uint8_t crossbarId);
    uint8_t getCrossbarId() const;

    uint32_t cpuSideSendingBufHasSpace(uint8_t cpuSidePortId);
    bool recvCpuSidePkt(T pkt);
    bool recvMemSidePkt(T pkt);

    void registerCpuSideRecvPktFromMemSide(std::function<bool(T)> callback, uint32_t id);
    void registerMemSideRecvPktFromCpuSide(std::function<bool(T)> callback, uint32_t id);
    void registerCalcMemSidePortId(std::function<uint32_t(T pkt)>callback);
    void registerCalcCpuSidePortId(std::function<uint32_t(T pkt)>callback);
    void registerCalcCpuToMemLatency(std::function<uint32_t(T pkt)> callback);
    void registerCalcMemToCpuLatency(std::function<uint32_t(T pkt)> callback);
    void Work();
private:
    void sendMemSidePkt();
    void sendCpuSidePkt();
};

#include "CacheMsg.h"

namespace JCore {

template class Crossbar<PtrCacheMsg>;

} // namespace JCore

#endif  // GFU2_CROSSBAR_H
