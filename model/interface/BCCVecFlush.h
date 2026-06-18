#ifndef BLOCKISA_MODEL_INTERFACE_BCCVECFLUSH_H
#define BLOCKISA_MODEL_INTERFACE_BCCVECFLUSH_H

namespace JCore {


class BCCVecFlush {
public:
    ROBID GetBid() const { return bid; }
    void SetBid(ROBID value) { bid = value; }
    uint64_t GetTid() const { return tid; }
    void SetTid(uint64_t value) { tid = value; }
    BCCVecFlush(ROBID bid) : bid(bid) {}
private:
    ROBID bid;
    uint64_t tid = 0;
};


} // namespace JCore

#endif  // BLOCKISA_MODEL_INTERFACE_BCCVECFLUSH_H
