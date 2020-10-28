#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    auto header = seg.header();
    if (header.syn) {
        _syn_flag = true;
        _isn = header.seqno;
    }
    if (!_syn_flag) {
        return;
    }
    uint64_t abs_seqno = unwrap(header.seqno, _isn, _reassembler.stream_out().bytes_written());
    uint64_t stream_index = abs_seqno - 1 + (header.syn ? 1 : 0);
    _reassembler.push_substring(seg.payload().copy(), stream_index, header.fin);
    if (_reassembler.stream_out().input_ended()) {
        _fin_flag = true;
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syn_flag) {
        return std::nullopt;
    }
    uint64_t stream_index = _reassembler.stream_out().bytes_written();
    uint64_t abs_seqno = stream_index + 1 + (_fin_flag ? 1 : 0);
    return wrap(abs_seqno, _isn);
}

size_t TCPReceiver::window_size() const { return stream_out().remaining_capacity(); }
