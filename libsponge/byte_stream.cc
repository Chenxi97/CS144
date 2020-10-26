#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _cap(capacity) {}

size_t ByteStream::write(const string &data) {
    size_t len = min(data.size(), _cap - _buf.size());
    for (size_t i = 0; i < len; ++i) {
        _buf.emplace_back(data[i]);
    }
    _write_ct += len;
    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string output{};
    size_t length = min(len, _buf.size());
    return output.assign(_buf.begin(), _buf.begin() + length);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t length = min(len, _buf.size());
    _read_ct += length;
    while (length--) {
        _buf.pop_front();
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string output{};
    size_t length = min(len, _buf.size());
    _read_ct += length;
    while (length--) {
        output.push_back(_buf.front());
        _buf.pop_front();
    }
    return output;
}

void ByteStream::end_input() { _end = true; }

bool ByteStream::input_ended() const { return _end; }

size_t ByteStream::buffer_size() const { return _buf.size(); }

bool ByteStream::buffer_empty() const { return _buf.empty(); }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _write_ct; }

size_t ByteStream::bytes_read() const { return _read_ct; }

size_t ByteStream::remaining_capacity() const { return _cap - _buf.size(); }
