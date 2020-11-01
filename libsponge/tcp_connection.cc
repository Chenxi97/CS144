#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_recv; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    // update receive time
    _time_since_recv = 0;
    if (seg.header().rst) {
        abort_connection();
        return;
    }
    _receiver.segment_received(seg);
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }
    if (seg.length_in_sequence_space() > 0) {
        _sender.fill_window();
        // if no segments produced, we need make a empty segment to ack
        if (_sender.segments_out().empty()) {
            _sender.send_empty_segment();
        }
        send_segments();
    }
    // set _linger_after_streams_finish false If the inbound stream ends before the TCPConnection
    // has reached EOF on its outbound stream
    if (_receiver.stream_out().input_ended() &&
        _sender.next_seqno_absolute() < _sender.stream_in().bytes_written() + 2) {
        _linger_after_streams_finish = false;
    }
}

bool TCPConnection::active() const {
    if (_abort) {
        return false;
    }
    return _linger_after_streams_finish || !sender_in_fin_acked() || !_receiver.stream_out().input_ended();
}

size_t TCPConnection::write(const string &data) {
    return _sender.stream_in().write(data);
    _sender.fill_window();
    send_segments();
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_recv += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    // abort connection if retransmit too many times
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        send_rst_segment();
        abort_connection();
        return;
    }
    // fill window if syn has been acked
    if (_sender.next_seqno_absolute() > _sender.bytes_in_flight()) {
        _sender.fill_window();
    }
    send_segments();
    // end connections cleanly if necessary
    if (sender_in_fin_acked() && _receiver.stream_out().input_ended() && _linger_after_streams_finish &&
        _time_since_recv >= 10 * _cfg.rt_timeout) {
        _linger_after_streams_finish = false;
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_segments();
}

void TCPConnection::connect() {
    _sender.fill_window();
    if (_sender.segments_out().empty()) {
        std::cerr << "Should send syn." << std::endl;
    }
    send_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            send_rst_segment();
            abort_connection();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::abort_connection() {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _abort = true;
}

void TCPConnection::send_rst_segment() {
    if (_sender.segments_out().empty()) {
        _sender.send_empty_segment();
    }
    auto seg = _sender.segments_out().front();
    seg.header().rst = true;
    _segments_out.emplace(seg);
}

void TCPConnection::send_segments() {
    auto maybe_ack = _receiver.ackno();
    if (maybe_ack.has_value()) {
        auto ackno = maybe_ack.value();
        auto window_size = min(_receiver.window_size(), static_cast<size_t>(std::numeric_limits<uint16_t>::max()));
        while (!_sender.segments_out().empty()) {
            auto temp_seg = _sender.segments_out().front();
            _sender.segments_out().pop();
            temp_seg.header().ack = true;
            temp_seg.header().ackno = ackno;
            temp_seg.header().win = static_cast<uint16_t>(window_size);
            _segments_out.emplace(temp_seg);
        }
    } else {
        // only for initial syn segment
        while (!_sender.segments_out().empty()) {
            _segments_out.emplace(_sender.segments_out().front());
            _sender.segments_out().pop();
        }
    }
}

bool TCPConnection::sender_in_fin_acked() const {
    return _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 && _sender.bytes_in_flight() == 0;
}
