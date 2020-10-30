#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _ackno; }

void TCPSender::fill_window() {
    // send syn
    if (_state == Sender_State::CLOSED) {
        send_segment(true, false, std::nullopt);
        _state = Sender_State::SYN_SENT;
        return;
    }
    if (_state != Sender_State::SYN_ACKED) {
        return;
    }
    // send 1 byte to keep alive when _window_size = 0 and no bytes in flight
    if (_window_size == 0 && bytes_in_flight() == 0) {
        if (need_send_fin()) {
            send_fin_segment(std::nullopt);
        } else if (!_stream.buffer_empty()) {
            send_segment(false, false, _stream.read(1));
        }
    }
    // send more bytes when bytes_need_send > 0
    while (bytes_need_send() > 0 && !_stream.buffer_empty()) {
        auto payload_size = min(bytes_need_send(), min(_stream.buffer_size(), TCPConfig::MAX_PAYLOAD_SIZE));
        auto data = _stream.read(payload_size);
        if (need_send_fin() && bytes_need_send() > payload_size) {
            // set fin when bytes_need_send is not full
            send_fin_segment(data);
        } else {
            send_segment(false, false, data);
        }
    }
    // send fin if bytes_need_send > 0
    if (bytes_need_send() > 0 && need_send_fin()) {
        send_fin_segment(std::nullopt);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    auto abs_ackno = unwrap(ackno, _isn, _ackno);
    if (abs_ackno < _ackno || abs_ackno > _next_seqno) {
        // invalid ackno
        return;
    }
    _window_size = window_size;
    if (abs_ackno == _ackno) {
        return;
    }
    // new ack received
    if (_state == Sender_State::SYN_SENT) {
        _state = Sender_State::SYN_ACKED;
    }
    _ackno = abs_ackno;
    for (auto it = _outstanding_seg.begin(); it != _outstanding_seg.end(); it = _outstanding_seg.erase(it)) {
        if (it->first + it->second.length_in_sequence_space() > _ackno) {
            break;
        }
    }
    _timer.set_rto(_initial_retransmission_timeout);
    _consecutive_retransmissions = 0;
    if (_outstanding_seg.empty()) {
        _timer.expire();
    } else {
        _timer.start();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (_timer.is_expired()) {
        // timer is closed
        return;
    }
    _timer.tick(ms_since_last_tick);
    if (!_timer.is_expired()) {
        // do not alarm
        return;
    }
    if (!_outstanding_seg.empty()) {
        auto it = _outstanding_seg.begin();
        _segments_out.emplace(it->second);
        if (_window_size > 0 || it->second.header().syn) {
            _consecutive_retransmissions++;
            _timer.set_rto(_timer.get_rto() * 2);
        }
        _timer.start();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() { send_segment(false, false, std::nullopt); }

void TCPSender::send_fin_segment(std::optional<std::string> data) {
    send_segment(false, true, data);
    _state = Sender_State::FIN_SENT;
}

void TCPSender::send_segment(bool syn, bool fin, std::optional<std::string> data) {
    auto seg = make_segment(syn, fin, data);
    _segments_out.emplace(seg);
    if (seg.length_in_sequence_space() == 0) {
        return;
    }
    _outstanding_seg.insert({_next_seqno, seg});
    _next_seqno += seg.length_in_sequence_space();
    if (_timer.is_expired()) {
        _timer.start();
    }
}

TCPSegment TCPSender::make_segment(bool syn, bool fin, std::optional<std::string> data) {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    seg.header().syn = syn;
    seg.header().fin = fin;
    if (data.has_value()) {
        seg.payload() = Buffer(std::move(data.value()));
    }
    return seg;
}

void Timer::tick(const size_t ms_since_last_tick) {
    _time_elapsed += ms_since_last_tick;
    _expired = _time_elapsed >= _rto;
}

void Timer::start() {
    _time_elapsed = 0;
    _expired = false;
}