#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof) {
        _eof_flag = true;
        _stream_len = index + data.size();
    }
    // discard if data is out of range [first_unassembled, first_unacceptable).
    size_t start_index = max(index, _output.bytes_written());
    size_t end_index = min(index + data.size(), _output.bytes_read() + _capacity);
    if (start_index >= end_index) {
        check_eof();
        return;
    }
    block_node new_block{start_index, end_index, data.substr(start_index - index, end_index - start_index)};
    auto it = _blocks.lower_bound(new_block);
    // merge left block
    if (it != _blocks.begin()) {
        --it;
        if (merge_block(new_block, *it)) {
            _unassembled_bytes -= it->data.size();
            it = _blocks.erase(it);
        } else {
            ++it;
        }
    }
    // merge right blocks
    while (it != _blocks.end()) {
        if (!merge_block(new_block, *it)) {
            break;
        }
        _unassembled_bytes -= it->data.size();
        it = _blocks.erase(it);
    }
    if (new_block.start == _output.bytes_written()) {
        _output.write(new_block.data);
    } else {
        _blocks.insert(new_block);
        _unassembled_bytes += new_block.data.size();
    }
    check_eof();
}

bool StreamReassembler::merge_block(block_node &dst, const block_node &src) {
    if (src.end < dst.start || src.start > dst.end) {
        return false;
    }
    if (src.start <= dst.start && src.end >= dst.end) {
        dst = src;
        return true;
    }
    if (src.end > dst.end) {
        size_t len = src.end - dst.end;
        dst.data += src.data.substr(dst.end - src.start, len);
        dst.end = src.end;
    }
    if (src.start < dst.start) {
        size_t len = dst.start - src.start;
        dst.data = src.data.substr(0, len) + dst.data;
        dst.start = src.start;
    }
    return true;
}

void StreamReassembler::check_eof() {
    if (_eof_flag && _output.bytes_written() == _stream_len) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _blocks.empty(); }
