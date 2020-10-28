#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    n = (n + isn.raw_value()) % (1UL << 32);
    return WrappingInt32{static_cast<uint32_t>(n)};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    int64_t minDis = (static_cast<int64_t>(n - isn) + (1L << 32)) % (1L << 32);
    uint64_t t = (checkpoint & 0xFFFFFFFF00000000) + minDis;
    if (t >= 1UL << 32) {
        t -= 1UL << 32;
    }
    uint64_t ths = checkpoint < (1UL << 31) ? 0
                                            : checkpoint > UINT64_MAX - (1UL << 31) ? UINT64_MAX - (1UL << 32) + 1
                                                                                    : checkpoint - (1UL << 31);
    while (t < ths) {
        t += (1UL << 32);
    }
    return t;
}
