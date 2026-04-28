// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "data_pipe.hpp"

#include "command_message.hpp"
#include "test_logging.hpp"

#include <vsomeip/message.hpp>

namespace vsomeip_v3::testing {

data_pipe::data_pipe(local_message_checker_t _checker) : checker_(std::move(_checker)) { }

data_pipe::data_pipe(external_message_checker_t _external_checker) : external_checker_(std::move(_external_checker)) { }

void data_pipe::init(open_reaction_t _react) {
    std::scoped_lock lock{mtx_};
    open_reaction_ = std::move(_react);
    input_data_.clear();
    data_to_forward_.clear();
}

void data_pipe::exchange_queues(data_pipe& _pipe) {
    {
        std::scoped_lock lock{mtx_, _pipe.mtx_};
        std::swap(input_data_, _pipe.input_data_);
        std::swap(data_to_forward_, _pipe.data_to_forward_);
    }
    std::scoped_lock lck{mtx_};
    push_data(lck);
}

size_t data_pipe::size() const {
    std::scoped_lock lock{mtx_};
    return data_to_forward_.size();
}

void data_pipe::add_data(control_data_t& _data) {
    std::scoped_lock lock{mtx_};
    input_data_.push_back(_data);
    if (state_ == data_pipe_state::CLOSED) {
        return;
    }
    push_data(lock);
}

void data_pipe::add_data(std::vector<unsigned char> _data) {
    std::scoped_lock lock{mtx_};
    control_data_t data{.buffer_ = std::move(_data), .addresses_ = std::nullopt};
    input_data_.push_back(std::move(data));
    if (state_ == data_pipe_state::CLOSED) {
        return;
    }
    push_data(lock);
}

[[nodiscard]] size_t data_pipe::fetch_data(boost::asio::mutable_buffer _out) {
    std::scoped_lock lock{mtx_};
    if (data_to_forward_.size() == 0) {
        return 0;
    }

    size_t len{0};
    auto data_it = data_to_forward_.begin();
    while (data_it != data_to_forward_.end()) {
        // Number of bytes we can write from the current segment without exceeding the output buffer size.
        auto const segment_writable = std::min(_out.size() - len, data_it->buffer_.size());
        std::copy(data_it->buffer_.begin(), data_it->buffer_.begin() + static_cast<std::ptrdiff_t>(segment_writable),
                  static_cast<unsigned char*>(_out.data()) + len);
        len += segment_writable;
        if (segment_writable == data_it->buffer_.size()) {
            // Fully consumed the current segment, move to the next one.
            ++data_it;
        } else {
            // Partially consumed the current segment, erase the consumed part and stop.
            data_it->buffer_.erase(data_it->buffer_.begin(), data_it->buffer_.begin() + static_cast<std::ptrdiff_t>(segment_writable));
            break;
        }
    }
    // Remove all fully consumed segments from the forward queue.
    data_to_forward_.erase(data_to_forward_.begin(), data_it);

    return len;
}

[[nodiscard]] bool data_pipe::fetch_data(control_data_t& _out) {
    std::scoped_lock lock{mtx_};
    if (data_to_forward_.empty()) {
        return false;
    }

    _out = *data_to_forward_.begin();
    data_to_forward_.erase(data_to_forward_.begin());

    return true;
}

void data_pipe::set_state(data_pipe_state _input) {
    open_reaction_t react;
    {
        std::scoped_lock lock{mtx_};
        state_ = _input;
        if (state_ == data_pipe_state::OPEN && open_reaction_) {
            TEST_LOG << "[data_pipe] is now OPEN, mem: " << this;
            push_data(lock);
            react = open_reaction_;
        }
    }
    if (react) {
        react();
    }
}

void data_pipe::push_data(std::scoped_lock<std::mutex> const& _lock) {
    if (checker_ || external_checker_) {
        push_checked_data(_lock);
    } else {
        // No checker, push all segments/datagrams through as they are, without parsing.
        push_through(_lock);
    }
}

void data_pipe::push_checked_data(std::scoped_lock<std::mutex> const&) {
    size_t segment_offset = 0;
    while (segment_offset < input_data_.size()) {
        size_t local_offset = 0;
        bool closed{false};
        while (local_offset < input_data_[segment_offset].buffer_.size()) {
            size_t msg_size{0};
            if (checker_) {
                command_message msg;
                msg_size = parse(input_data_[segment_offset].buffer_.data() + local_offset,
                                 input_data_[segment_offset].buffer_.size() - local_offset, msg);
                if (msg_size == 0) {
                    TEST_LOG << "[data_pipe] ERROR Can not parse vsomeip command, mem: " << this;
                    continue;
                }
                state_ = checker_(msg);
            } else if (external_checker_) {
                someip_message msg;
                msg_size = parse_sequential_someip(input_data_[segment_offset].buffer_.data() + local_offset,
                                                   input_data_[segment_offset].buffer_.size() - local_offset, msg);
                if (msg_size == 0) {
                    TEST_LOG << "[data_pipe] ERROR Can not parse someip message, mem: " << this << " removing remaining data in segment";
                    input_data_[segment_offset].buffer_.erase(input_data_[segment_offset].buffer_.begin()
                                                                      + static_cast<std::ptrdiff_t>(local_offset),
                                                              input_data_[segment_offset].buffer_.end());
                    continue;
                }
                state_ = external_checker_(msg);
            }

            if (state_ == data_pipe_state::CLOSED) {
                TEST_LOG << "[data_pipe] is now CLOSED, mem: " << this;
                closed = true;
                break;
            }
            local_offset += msg_size;
        }

        if (local_offset > 0) {
            // Copy what has been parsed from the current segment to the forward queue and erase it from the input queue.
            control_data_t data{.buffer_ = std::vector<unsigned char>(input_data_[segment_offset].buffer_.begin(),
                                                                      input_data_[segment_offset].buffer_.begin() + local_offset),
                                .addresses_ = input_data_[segment_offset].addresses_};
            data_to_forward_.emplace_back(data);

            input_data_[segment_offset].buffer_.erase(input_data_[segment_offset].buffer_.begin(),
                                                      input_data_[segment_offset].buffer_.begin()
                                                              + static_cast<std::ptrdiff_t>(local_offset));
        }

        if (closed) {
            // If the pipe is closed, stop processing further segments.
            // Current segment keeps the blocked command.
            input_data_.erase(input_data_.begin(), input_data_.begin() + static_cast<std::ptrdiff_t>(segment_offset));
            return;
        }

        ++segment_offset;
    }
    input_data_.erase(input_data_.begin(), input_data_.begin() + static_cast<std::ptrdiff_t>(segment_offset));
}

void data_pipe::push_through(std::scoped_lock<std::mutex> const&) {
    std::copy(input_data_.begin(), input_data_.end(), std::back_inserter(data_to_forward_));
    input_data_.clear();
}
}
