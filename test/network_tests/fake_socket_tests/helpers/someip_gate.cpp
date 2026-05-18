// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "someip_gate.hpp"

#include "test_logging.hpp"

namespace vsomeip_v3::testing {

someip_gate::someip_gate(hidden) { }

std::shared_ptr<someip_gate> someip_gate::create() {
    auto ptr = std::make_shared<someip_gate>(hidden{});
    ptr->pipe_ = std::make_shared<data_pipe>(
            static_cast<data_pipe::external_message_checker_t>([weak_self = std::weak_ptr<someip_gate>(ptr)](auto const& _msg) {
                if (auto self = weak_self.lock(); self) {
                    return (*self)(_msg);
                }
                return data_pipe_state::OPEN;
            }));
    return ptr;
}

std::shared_ptr<data_pipe> someip_gate::get_data_pipe() const {
    return pipe_;
}

data_pipe_state someip_gate::operator()(someip_message const& _msg) {
    std::scoped_lock lock{mtx_};

    if (state_ == gate_state::BLOCKED) {
        return data_pipe_state::CLOSED;
    }
    if (!search_) {
        return data_pipe_state::OPEN;
    }

    bool matched = false;
    if (auto const* t = std::get_if<trigger>(&search_->trigger_)) {
        if (!_msg.msg_)
            return data_pipe_state::OPEN;
        bool const service_match = (_msg.msg_->get_service() == t->service_);
        bool const method_match = (_msg.msg_->get_method() == t->method_);
        bool const type_match = (!t->type_ || _msg.msg_->get_message_type() == *t->type_);
        bool const payload_match = (!t->payload_ || (*t->payload_)(_msg.msg_->get_payload()));
        matched = service_match && method_match && type_match && payload_match;
    } else if (auto const* t = std::get_if<sd_trigger>(&search_->trigger_)) {
        if (!_msg.sd_)
            return data_pipe_state::OPEN;
        for (auto const& entry : _msg.sd_->get_entries()) {
            if (t->id_ == entry->get_type() && t->ttl_ == entry->get_ttl()) {
                matched = true;
                break;
            }
        }
    }

    if (matched && ++search_->count_ >= search_->barrier_) {
        state_ = gate_state::BLOCKED;
        cv_.notify_all();
        return data_pipe_state::CLOSED;
    }

    return data_pipe_state::OPEN;
}

void someip_gate::block_at(trigger _trigger, uint32_t _count) {
    std::shared_ptr<data_pipe> pipe;
    {
        std::scoped_lock lock{mtx_};
        TEST_LOG << "[someip_gate] Setup gate to block when seeing "
                 << "service=0x" << std::hex << _trigger.service_ << " method=0x" << _trigger.method_ << " for the " << std::dec << _count
                 << " time, on gate mem: " << pipe_.get();
        search_ = {std::move(_trigger), _count, 0};
        state_ = gate_state::UNBLOCKED;
        pipe = pipe_;
    }
    // careful not to call this under the lock, as it will re-invoke the call operator
    pipe->set_state(data_pipe_state::OPEN);
}

void someip_gate::block_at(sd_trigger _trigger, uint32_t _count) {
    std::shared_ptr<data_pipe> pipe;
    {
        std::scoped_lock lock{mtx_};
        TEST_LOG << "[someip_gate/sd] Setup gate to block when seeing " << to_string(_trigger.id_) << ":" << _trigger.ttl_ << " for the "
                 << _count << " time, on gate mem: " << pipe_.get();
        search_ = {std::move(_trigger), _count, 0};
        state_ = gate_state::UNBLOCKED;
        pipe = pipe_;
    }
    // careful not to call this under the lock, as it will re-invoke the call operator
    pipe->set_state(data_pipe_state::OPEN);
}

void someip_gate::block(bool _block) {
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

bool someip_gate::wait_for_blocked(std::chrono::milliseconds _timeout) const {
    std::unique_lock lock{mtx_};
    if (state_ == gate_state::BLOCKED) {
        return true;
    }
    return cv_.wait_for(lock, _timeout, [this] { return state_ == gate_state::BLOCKED; });
}

} // namespace vsomeip_v3::testing
