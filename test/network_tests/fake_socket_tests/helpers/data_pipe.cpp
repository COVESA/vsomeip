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

void data_pipe::add_data(std::vector<unsigned char> _data) {
    std::scoped_lock lock{mtx_};
    input_data_.reserve(input_data_.size() + _data.size());
    std::copy(_data.begin(), _data.end(), std::back_inserter(input_data_));
    if (state_ == data_pipe_state::CLOSED) {
        return;
    }
    push_data(lock);
}

[[nodiscard]] size_t data_pipe::fetch_data(boost::asio::mutable_buffer _out) {
    std::scoped_lock lock{mtx_};
    auto const len = std::min(_out.size(), data_to_forward_.size());
    if (len == 0) {
        return 0;
    }

    char* out = static_cast<char*>(_out.data());
    char* end = out + len;
    for (auto it = data_to_forward_.begin(); out != end; ++out) {
        *out = static_cast<char>(*it);
        ++it;
    }
    data_to_forward_.erase(data_to_forward_.begin(),
                           data_to_forward_.begin() + static_cast<std::vector<unsigned char>::difference_type>(len));
    return len;
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
    if (checker_) {
        push_local_data(_lock);
    } else {
        push_through(_lock);
    }
}

void data_pipe::push_local_data(std::scoped_lock<std::mutex> const&) {
    size_t offset = 0;
    while (offset < input_data_.size()) {
        command_message msg;
        auto msg_size = parse(input_data_.data() + offset, input_data_.size() - offset, msg);
        if (msg_size == 0) {
            TEST_LOG << "[data_pipe] ERROR Can not parse data, mem: " << this;
            break;
        }
        state_ = checker_(msg);
        if (state_ == data_pipe_state::CLOSED) {
            TEST_LOG << "[data_pipe] is now CLOSED, mem: " << this;
            input_data_.erase(input_data_.begin(), input_data_.begin() + static_cast<std::ptrdiff_t>(offset));
            return;
        }
        auto const* begin = input_data_.data() + offset;
        data_to_forward_.insert(data_to_forward_.end(), begin, begin + msg_size);
        offset += msg_size;
    }
    input_data_.erase(input_data_.begin(), input_data_.begin() + static_cast<std::ptrdiff_t>(offset));
}

void data_pipe::push_through(std::scoped_lock<std::mutex> const&) {
    data_to_forward_.reserve(data_to_forward_.size() + input_data_.size());
    std::copy(input_data_.begin(), input_data_.end(), std::back_inserter(data_to_forward_));
    input_data_.clear();
}

}
