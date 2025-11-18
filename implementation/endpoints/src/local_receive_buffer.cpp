// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/local_receive_buffer.hpp"

#include "../../protocol/include/protocol.hpp"
#include "../../configuration/include/configuration.hpp"

#include <cstdint>
#include <vsomeip/internal/logger.hpp>

#include <limits>
#include <iostream>

namespace vsomeip_v3 {

void next_message_result::set_message(uint8_t const* _data, uint32_t _size) {
    message_data_ = _data;
    message_size_ = _size;
    error_ = false;
}
void next_message_result::set_error() {
    error_ = true;
    message_data_ = nullptr;
}

void next_message_result::clear() {
    error_ = false;
    message_data_ = nullptr;
}

local_receive_buffer::local_receive_buffer(uint32_t _max_message_length, uint32_t _buffer_shrink_threshold) :
    max_message_length_(_max_message_length), buffer_shrink_threshold_(_buffer_shrink_threshold), mem_(initial_buffer_size_) { }

bool local_receive_buffer::next_message(next_message_result& _result) {
    uint32_t length = 0;
    if (size_t bytes = end_ - start_; bytes >= protocol::COMMAND_POSITION_SIZE + sizeof(length)) {
        memcpy(&length, &mem_[start_] + protocol::COMMAND_POSITION_SIZE, sizeof(length));
        // MESSAGE_SIZE_UNLIMITED  is numerical limit of uin32_t therefore the subsequent check suffices
        if (length > max_message_length_) {
            VSOMEIP_ERROR << __func__ << " message length: " << length << " exceeded allowed max message size";
            _result.set_error();
            return false;
        }
        auto const size = length + protocol::COMMAND_HEADER_SIZE;
        if (size > bytes) {
            if (size > mem_.size()) {
                // capacity is only missing if the shifted buffer is not sufficient
                if (!add_capacity(size - mem_.size())) {
                    _result.set_error();
                    return false;
                }
            }
            _result.clear();
            return false;
        }
        if (size <= std::numeric_limits<uint32_t>::max()) {
            _result.set_message(&mem_[start_], static_cast<uint32_t>(size));
            start_ += size;
            // Shrink heuristic: Only shrink if we're consistently processing small messages.
            // Large messages (> capacity/2) indicate we might need the extra space,
            // so reset the counter. Small messages increment it, and when the threshold
            // is reached, we shrink back to the initial size (happens in the next call
            // when buffer is empty).
            if (size > mem_.capacity() / 2) {
                shrink_ct_ = 0; // Large message - don't shrink yet
            } else {
                shrink_ct_++; // Small message - increment shrink counter
            }
            return true;
        }
        VSOMEIP_ERROR << __func__ << " message size: " << size << " exceeded numerical limits";
        _result.set_error();
        return false;
    }
    if (auto const free_bytes = mem_.size() - (end_ - start_); free_bytes < protocol::COMMAND_POSITION_SIZE + sizeof(length)) {
        // only capacity missing, if the shifted buffer is not sufficient
        if (!add_capacity(protocol::COMMAND_POSITION_SIZE + sizeof(length) - free_bytes)) {
            _result.set_error();
            return false;
        }
    } else if (end_ == start_ && buffer_shrink_threshold_ > 0 && shrink_ct_ >= buffer_shrink_threshold_
               && mem_.size() > initial_buffer_size_) {
        shrink();
    }
    _result.clear();
    return false;
}

void local_receive_buffer::shift_front() {
    if (start_ == 0) {
        return;
    }
    // Do NOT:
    // * Use iterator arithmetics as it relies on ptrdiff_t which is int32_t on a u32 architecture,
    // but the max message size that needs support is of uint32_t::max.
    // * memcpy as the regions might overlapp and this is u.b. then
    std::copy(mem_.data() + start_, mem_.data() + end_, mem_.data());
    end_ -= start_;
    start_ = 0;
}

void local_receive_buffer::shrink() {
    mem_.resize(initial_buffer_size_, 0x0);
    mem_.shrink_to_fit();
    shrink_ct_ = 0;
    start_ = 0;
    end_ = start_;
}

[[nodiscard]] bool local_receive_buffer::add_capacity(size_t _capacity) {
    if (_capacity > std::numeric_limits<size_t>::max() - mem_.size()) {
        VSOMEIP_ERROR << "lrb::" << __func__ << ": Insufficient memory to consume the remaining: " << _capacity << " bytes";
        return false;
    }
    size_t const new_capa = _capacity + mem_.size();
    mem_.resize(new_capa);

    return true;
}

[[nodiscard]] bool local_receive_buffer::bump_end(size_t _new_bytes) {
    if (_new_bytes > mem_.size() - end_) {
        return false;
    }
    end_ += _new_bytes;
    return true;
}

std::ostream& operator<<(std::ostream& _out, local_receive_buffer const& _buffer) {
    return _out << std::dec << "{mem_size: " << _buffer.mem_.size() << ", first_used: " << _buffer.start_ << ", last_used: " << _buffer.end_
                << ", shrink: [" << _buffer.shrink_ct_ << ", " << _buffer.buffer_shrink_threshold_ << "]}";
}
}
