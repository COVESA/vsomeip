// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/timer.hpp"
#include "../include/abstract_socket_factory.hpp"
#include "../../utility/include/is_value.hpp"

#include <boost/asio/error.hpp>

#include <boost/system/error_code.hpp>
#include <vsomeip/internal/logger.hpp>

namespace vsomeip_v3 {

timer::timer([[maybe_unused]] hidden _h, std::unique_ptr<abstract_timer> _timer, std::chrono::milliseconds _interval, task_t _task) :
    timer_(std::move(_timer)), interval_(_interval), task_(std::move(_task)) { }

timer::~timer() {
    stop();
}

std::shared_ptr<timer> timer::create(boost::asio::io_context& _io, std::chrono::milliseconds _interval, task_t _task) {
    return std::make_shared<timer>(hidden{}, abstract_socket_factory::get()->create_timer(_io), _interval, std::move(_task));
}

void timer::start() {
    std::scoped_lock lock{mtx_};
    if (is_value(state_).none_of(state_e::STOPPED, state_e::IN_TASK_STOPPED)) {
        // the timer is already running -> restart it
        stop_unlocked();
    }
    if (is_value(state_).any_of(state_e::IN_TASK, state_e::IN_TASK_STARTED, state_e::IN_TASK_STOPPED)) {
        // the timer is currently executing the task
        // -> adjust the state but don't do anything, as it will be restarted after being done with
        // the task
        state_ = state_e::IN_TASK_STARTED;
        return;
    }
    start_unlocked();
}

void timer::stop() {
    std::scoped_lock lock{mtx_};
    if (is_value(state_).any_of(state_e::STOPPED, state_e::IN_TASK_STOPPED)) {
        return;
    }
    stop_unlocked();
}

[[nodiscard]] bool timer::is_running() const {
    std::scoped_lock lock{mtx_};
    return is_value(state_).none_of(state_e::STOPPED, state_e::IN_TASK_STOPPED);
}

[[nodiscard]] bool timer::set_interval(std::chrono::milliseconds _interval) {
    std::scoped_lock lock{mtx_};
    if (state_ != state_e::STOPPED) {
        return false;
    }
    interval_ = _interval;
    return true;
}

[[nodiscard]] bool timer::set_task(task_t _task) {
    std::scoped_lock lock{mtx_};
    if (state_ != state_e::STOPPED) {
        return false;
    }
    task_ = std::move(_task);
    return true;
}

void timer::stop_unlocked() {
    VSOMEIP_DEBUG << "timer::" << __func__ << ": stopped: " << this;
    if (is_value(state_).any_of(state_e::IN_TASK_STARTED, state_e::IN_TASK)) {
        state_ = state_e::IN_TASK_STOPPED;
        return;
    }
    state_ = state_e::STOPPED;
    timer_->cancel();
}

void timer::start_unlocked() {
    VSOMEIP_DEBUG << "timer::" << __func__ << ": started: " << this;
    state_ = state_e::STARTED;
    try {
        timer_->expires_after(interval_);
        timer_->async_wait([weak_self = weak_from_this()](auto const& _ec) {
            if (auto self = weak_self.lock(); self) {
                self->handle(_ec);
            }
        });
    } catch (std::exception const& _e) {
        VSOMEIP_ERROR << "timer::" << __func__ << ": ERROR: can not start timer due to: " << _e.what();
    }
}

void timer::handle(boost::system::error_code const& _ec) {
    if (_ec == boost::asio::error::operation_aborted) {
        return;
    }
    std::unique_lock lock{mtx_};
    if (state_ != state_e::STARTED) {
        VSOMEIP_DEBUG << "timer::" << __func__ << ": not executing the task: " << this;
        return;
    }
    state_ = state_e::IN_TASK;
    lock.unlock();
    VSOMEIP_DEBUG << "timer::" << __func__ << ": start task: " << this;
    bool const restart = task_();
    VSOMEIP_DEBUG << "timer::" << __func__ << ": end task: " << this;
    lock.lock();
    if (state_ == state_e::IN_TASK_STARTED || (restart && state_ == state_e::IN_TASK)) {
        start_unlocked();
        return;
    }
    state_ = state_e::STOPPED;
}
}
