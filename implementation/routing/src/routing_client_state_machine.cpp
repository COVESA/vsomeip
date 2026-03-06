// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>

#include "../include/routing_client_state_machine.hpp"
#include "../../utility/include/is_value.hpp"
#include "../../utility/include/utility.hpp"
#include "../../logger/include/logger_ext.hpp"

namespace vsomeip_v3 {

#define VSOMEIP_LOG_PREFIX "rcsm"

static constexpr char const* to_string(routing_client_state_e _state) {
    switch (_state) {
    case routing_client_state_e::ST_REGISTERED:
        return "ST_REGISTERED";
    case routing_client_state_e::ST_DEREGISTERED:
        return "ST_DEREGISTERED";
    case routing_client_state_e::ST_REGISTERING:
        return "ST_REGISTERING";
    case routing_client_state_e::ST_DEREGISTERING:
        return "ST_DEREGISTERING";
    default:
        return "UNKNOWN";
    }
}

std::ostream& operator<<(std::ostream& _out, routing_client_state_e _state) {
    return _out << to_string(_state);
}

routing_client_state_machine::routing_client_state_machine(hidden, configuration const& _configuration, error_handler _handler) :
    configuration_(_configuration), error_handler_(std::move(_handler)) { }

std::shared_ptr<routing_client_state_machine> routing_client_state_machine::create(configuration const& _configuration,
                                                                                   error_handler _handler) {
    return std::make_shared<routing_client_state_machine>(hidden{}, _configuration, std::move(_handler));
}

routing_client_state_e routing_client_state_machine::state() const {
    std::scoped_lock lock{mtx_};
    return state_;
}

void routing_client_state_machine::target_shutdown() {
    std::scoped_lock lock{mtx_};
    shall_run_ = false;
}

void routing_client_state_machine::target_running() {
    std::scoped_lock lock{mtx_};
    shall_run_ = true;
}

[[nodiscard]] bool routing_client_state_machine::start_registration() {

    std::scoped_lock lock{mtx_};
    if (!shall_run_ || state_ != routing_client_state_e::ST_DEREGISTERED) {
        VSOMEIP_WARNING_P << "Unexpected state: " << state_ << ", target_running: " << std::boolalpha << shall_run_;
        return false;
    }
    change_state_unlocked(routing_client_state_e::ST_REGISTERING);
    return true;
}

[[nodiscard]] bool routing_client_state_machine::registered(client_t _client) {
    std::scoped_lock lock{mtx_};
    if (state_ != routing_client_state_e::ST_REGISTERING) {
        VSOMEIP_WARNING_P << "Unexpected state: " << state_;
        return false;
    }

    VSOMEIP_INFO_P << "Former client id: 0x" << hex4(former_client_) << ", new 0x" << hex4(_client);
    former_client_ = client_;
    client_ = _client;

    change_state_unlocked(routing_client_state_e::ST_REGISTERED);
    return true;
}

[[nodiscard]] bool routing_client_state_machine::await_registered() {
    std::unique_lock lock{mtx_};
    if (state_ == routing_client_state_e::ST_REGISTERED) {
        return true;
    }
    if (state_ != routing_client_state_e::ST_REGISTERING) {
        return false;
    }
    bool const result =
            cv_.wait_for(lock, configuration_.shutdown_timeout_, [this] { return state_ != routing_client_state_e::ST_REGISTERING; });
    return result ? state_ == routing_client_state_e::ST_REGISTERED : false;
}

[[nodiscard]] bool routing_client_state_machine::start_deregister() {
    std::scoped_lock lock{mtx_};
    if (state_ != routing_client_state_e::ST_REGISTERED) {
        VSOMEIP_WARNING_P << "Unexpected state: " << state_;
        return false;
    }
    change_state_unlocked(routing_client_state_e::ST_DEREGISTERING);
    return true;
}

void routing_client_state_machine::deregistered() {
    std::unique_lock lock{mtx_};
    deregister_unlocked(std::move(lock));
}

[[nodiscard]] bool routing_client_state_machine::await_deregistered() {
    std::unique_lock lock{mtx_};
    if (state_ == routing_client_state_e::ST_DEREGISTERED) {
        return true;
    }
    if (state_ != routing_client_state_e::ST_DEREGISTERING) {
        VSOMEIP_WARNING_P << "Unexpected state: " << state_;
        return false;
    }
    bool const result =
            cv_.wait_for(lock, configuration_.shutdown_timeout_, [this] { return state_ != routing_client_state_e::ST_DEREGISTERING; });
    return result ? state_ == routing_client_state_e::ST_DEREGISTERED : false;
}

void routing_client_state_machine::deregister_unlocked(std::unique_lock<std::mutex> _acquired_lock) {
    change_state_unlocked(routing_client_state_e::ST_DEREGISTERED);
    if (client_ != VSOMEIP_CLIENT_UNSET) {
        // stash the last meaningful client id, to be able to tell whether the client id changed across registration states
        former_client_ = client_;
    }
    client_ = VSOMEIP_CLIENT_UNSET;

    if (!shall_run_) {
        return;
    }
    auto copy = error_handler_;
    _acquired_lock.unlock();

    if (copy) {
        copy();
    }
}

void routing_client_state_machine::change_state_unlocked(routing_client_state_e _state) {
    VSOMEIP_INFO_P << "Client 0x" << hex4(client_) << ", state " << state_ << " -> " << _state;
    state_ = _state;
    if (is_value(state_).any_of(routing_client_state_e::ST_REGISTERED, routing_client_state_e::ST_DEREGISTERED)) {
        cv_.notify_one();
    }
}
}
