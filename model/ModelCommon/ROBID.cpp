#include "ROBID.h"

#include "utils/error.h"

namespace JCore {

void DisableROBID(ROBID &id)
{
    id.valid = false;
}

bool IsDisableROBID(const ROBID &id)
{
    return !id.valid;
}

void IncROBID(ROBID &id, uint32_t ribSize)
{
    // no need to wrap around
    if (id.val + 1 < ribSize) {
        id.val++;
    } else {
        // wrap around
        id.wrap = !id.wrap;
        id.val = 0;
    }
}

void AddROBID(ROBID &id, uint32_t off, uint32_t ribSize)
{
    ASSERT(off < ribSize);
    if (id.val + off < ribSize) {
        id.val = id.val + off;
    } else {
        id.val = (id.val + off) % ribSize;
        id.wrap = !id.wrap;
    }
}

void SubROBID(ROBID &id, uint32_t off, uint32_t ribSize)
{
    ASSERT(off < ribSize);
    if (id.val < off) {
        id.val = ribSize - (off - id.val);
        id.wrap = !id.wrap;
    } else {
        id.val = id.val - off;
    }
}

/* delta = a - b */
int DeltaBID(ROBID a, ROBID b, uint32_t size)
{
    int delta;
    bool neg;
    ROBID big;
    ROBID small;

    if (a < b) {
        big   = b;
        small = a;
        neg   = true;
    } else {
        big   = a;
        small = b;
        neg   = false;
    }

    if (big.wrap != small.wrap) {
        big.val = big.val + size;
    }

    delta = big.val - small.val;
    if (neg) {
        delta = -delta;
    }

    return delta;
}

bool LessEqual(ROBID bid0, ROBID bid1)
{
    if (bid0 < bid1) {
        return true;
    }
    if (bid0 == bid1) {
        return true;
    }
    return false;
}

bool LessEqual(ROBID bid0, ROBID rid0, ROBID bid1, ROBID rid1)
{
    if (bid0 < bid1) {
        return true;
    }
    if (bid0 == bid1) {
        if (rid0 < rid1) {
            return true;
        }
        if (rid0 == rid1) {
            return true;
        }
    }
    return false;
}

bool LessEqual(ROBID bid0, ROBID gid0, ROBID rid0, ROBID bid1, ROBID gid1, ROBID rid1)
{
    if (bid0 < bid1) {
        return true;
    }
    if (bid0 > bid1) {
        return false;
    }

    if (gid0 < gid1) {
        return true;
    }
    if (gid0 > gid1) {
        return false;
    }

    if (rid0 < rid1 || rid0 == rid1) {
        return true;
    }
    return false;
}

bool LessROBID(ROBID bid0, ROBID bid1)
{
    if (bid0 < bid1) {
        return true;
    }
    return false;
}

bool LessROBID(ROBID bid0, ROBID rid0, ROBID bid1, ROBID rid1)
{
    if (bid0 < bid1) {
        return true;
    }
    if (bid0 == bid1 && rid0 < rid1) {
        return true;
    }
    return false;
}

bool LessROBID(ROBID bid0, ROBID gid0, ROBID rid0, ROBID bid1, ROBID gid1, ROBID rid1)
{
    if (bid0 < bid1) {
        return true;
    }

    if (bid0 > bid1) {
        return false;
    }

    if (gid0 < gid1) {
        return true;
    }

    if (gid0 > gid1) {
        return false;
    }

    if (rid0 < rid1) {
        return true;
    }

    return false;
}

bool LessEqualROBID(ROBID bid0, ROBID rid0, ROBID bid1, ROBID rid1)
{
    if (bid0 < bid1) {
        return true;
    }

    if (bid0 > bid1) {
        return false;
    }

    if ((rid0 == rid1) || (rid0 < rid1)) {
        return true;
    }
    return false;
}

uint32_t CalGap(ROBID bid0, ROBID bid1, uint64_t size)
{
    if (bid0.wrap == bid1.wrap) {
        return bid0.val - bid1.val;
    } else {
        return (bid0.val + size) - bid1.val;
    }
}

bool operator<(ROBID lhs, ROBID rhs)
{
    if (lhs.wrap == rhs.wrap) {
        return lhs.val < rhs.val;
    } else {
        return lhs.val > rhs.val;
    }
}

bool operator<=(ROBID lhs, ROBID rhs)
{
    if (lhs.wrap == rhs.wrap) {
        return lhs.val <= rhs.val;
    } else {
        return lhs.val >= rhs.val;
    }
}

bool operator>(ROBID lhs, ROBID rhs)
{
    if (lhs.wrap == rhs.wrap) {
        return lhs.val > rhs.val;
    } else {
        return lhs.val < rhs.val;
    }
}

bool operator>=(ROBID lhs, ROBID rhs)
{
    if (lhs.wrap == rhs.wrap) {
        return lhs.val >= rhs.val;
    } else {
        return lhs.val <= rhs.val;
    }
}

bool operator==(ROBID lhs, ROBID rhs)
{
    if (lhs.wrap == rhs.wrap) {
        return lhs.val == rhs.val;
    } else {
        return false;
    }
}

bool operator!=(ROBID lhs, ROBID rhs)
{
    return !(lhs == rhs);
}
std::ostream& operator<<(std::ostream& out, ROBID const &id)
{
    out << std::dec << id.val;
    return out;
}

std::ostream& operator<<(std::ostream& out, RobIdWrapView v)
{
    out << std::dec << v.id.val;
    if (v.id.wrap) {
        out << "+";
    } else {
        out << "-";
    }
    return out;
}
}