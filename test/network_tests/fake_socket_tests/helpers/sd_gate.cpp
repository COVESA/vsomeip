// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sd_gate.hpp"

#include "test_logging.hpp"

namespace vsomeip_v3::testing {

sd_gate::sd_gate(hidden) { }

std::shared_ptr<sd_gate> sd_gate::create() {
    auto ptr = std::make_shared<sd_gate>(hidden{});
    ptr->pipe_ = std::make_shared<data_pipe>(
            static_cast<data_pipe::external_message_checker_t>([weak_self = std::weak_ptr<sd_gate>(ptr)](auto const& _cmd) {
                if (auto self = weak_self.lock(); self) {
                    return (*self)(_cmd);
                }
                return data_pipe_state::OPEN;
            }));
    return ptr;
}

std::shared_ptr<data_pipe> sd_gate::get_data_pipe() const {
    return pipe_;
}

data_pipe_state sd_gate::operator()(someip_message const& _msg) {
    std::scoped_lock lock{mtx_};

    if (state_ == gate_state::BLOCKED) {
        return data_pipe_state::CLOSED;
    }
    if (!search_) {
        return data_pipe_state::OPEN;
    }

    if (!_msg.sd_) {
        return data_pipe_state::OPEN;
    }

    // Iterate all sd entries
    for (const auto& entry : _msg.sd_->get_entries()) {
        if (search_->sd_.id_ == entry->get_type() && search_->sd_.ttl_ == entry->get_ttl()) {
            if (++search_->count_ >= search_->barrier_) {
                state_ = gate_state::BLOCKED;
                cv_.notify_all();
                return data_pipe_state::CLOSED;
            } else {
                return data_pipe_state::OPEN;
            }
        }
    }

    return data_pipe_state::OPEN;
}

void sd_gate::block_at(someip_sd_record_message _sd_msg, uint32_t _count) {
    std::shared_ptr<data_pipe> pipe;
    {
        std::scoped_lock lock{mtx_};
        TEST_LOG << "[sd_gate] Setup gate to block when seeing " << to_string(_sd_msg.id_) << ":" << _sd_msg.ttl_ << " for the " << _count
                 << " time, on gate mem: " << pipe_.get();
        search_ = {_sd_msg, _count, 0};
        state_ = gate_state::UNBLOCKED;
        pipe = pipe_;
    }
    // careful not to call this function under the lock, as this will re-call
    // the call operator
    pipe->set_state(data_pipe_state::OPEN);
}

void sd_gate::block(bool _block) {
    std::shared_ptr<data_pipe> pipe;
    {
        std::scoped_lock lock{mtx_};
        search_ = std::nullopt;
        if (_block) {
            state_ = gate_state::BLOCKED;
        } else {
            state_ = gate_state::UNBLOCKED;
            pipe = pipe_;
        }
    }
    if (pipe) {
        pipe->set_state(data_pipe_state::OPEN);
    }
}

bool sd_gate::wait_for_blocked(std::chrono::milliseconds _timeout) const {
    std::unique_lock lock{mtx_};
    if (state_ == gate_state::BLOCKED) {
        return true;
    }
    return cv_.wait_for(lock, _timeout, [this] { return state_ == gate_state::BLOCKED; });
}

}
