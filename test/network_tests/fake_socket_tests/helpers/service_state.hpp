// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_SERVICE_STATE_HPP_
#define VSOMEIP_V3_SERVICE_STATE_HPP_

#include <vsomeip/vsomeip.hpp>

#include <ostream>
#include <utility>
#include <iomanip>
#include <sstream>

namespace vsomeip_v3::testing {

struct service_instance {
    vsomeip::service_t service_{};
    vsomeip::instance_t instance_{};

    [[nodiscard]] bool operator==(service_instance const& rhs) const { return service_ == rhs.service_ && instance_ == rhs.instance_; }
    [[nodiscard]] bool operator!=(service_instance const& rhs) const { return !(*this == rhs); }
};

struct service_availability {
    static service_availability available(service_instance _si) { return {_si, vsomeip::availability_state_e::AS_AVAILABLE}; }
    static service_availability unavailable(service_instance _si) { return {_si, vsomeip::availability_state_e::AS_UNAVAILABLE}; }

    service_instance si_{};
    vsomeip::availability_state_e state_{};

    [[nodiscard]] bool operator==(service_availability const& rhs) const { return si_ == rhs.si_ && state_ == rhs.state_; }
    [[nodiscard]] bool operator!=(service_availability const& rhs) const { return !(*this == rhs); }
};

struct event_ids {
    service_instance si_{};
    vsomeip::event_t event_id_{};
    vsomeip::eventgroup_t eventgroup_id_{};
    [[nodiscard]] bool operator==(event_ids const& rhs) const {
        return si_ == rhs.si_ && event_id_ == rhs.event_id_ && eventgroup_id_ == rhs.eventgroup_id_;
    }
    [[nodiscard]] bool operator!=(event_ids const& rhs) const { return !(*this == rhs); }
};

struct event_subscription {
    static event_subscription successfully_subscribed_to(event_ids _ei) { return {_ei, 0}; }
    event_ids ei_{};
    uint16_t error_code_{};
    [[nodiscard]] bool operator==(event_subscription const& rhs) const { return ei_ == rhs.ei_ && error_code_ == rhs.error_code_; }
    [[nodiscard]] bool operator!=(event_subscription const& rhs) const { return !(*this == rhs); }
};

struct client_session {
    vsomeip::client_t client_{};
    vsomeip::session_t session_{};

    [[nodiscard]] bool operator==(client_session const& rhs) const { return client_ == rhs.client_ && session_ == rhs.session_; }
    [[nodiscard]] bool operator!=(client_session const& rhs) const { return !(*this == rhs); }
};

struct request {
    service_instance service_instance_{};
    vsomeip::method_t method_{};
    vsomeip::message_type_e message_type_{};

    std::vector<unsigned char> payload_{};

    [[nodiscard]] bool operator==(request const& rhs) const {
        auto tie = [](auto const& d) { return std::tie(d.service_instance_, d.method_, d.message_type_, d.payload_); };
        return tie(*this) == tie(rhs);
    }
    [[nodiscard]] bool operator!=(request const& rhs) const { return !(*this == rhs); }
};

struct message {
    client_session client_session_{};

    service_instance service_instance_{};
    vsomeip::method_t method_{};
    vsomeip::message_type_e message_type_{};

    std::vector<unsigned char> payload_{};

    [[nodiscard]] bool operator==(message const& rhs) const {
        auto tie = [](auto const& d) { return std::tie(d.client_session_, d.service_instance_, d.method_, d.message_type_, d.payload_); };
        return tie(*this) == tie(rhs);
    }
    [[nodiscard]] bool operator!=(message const& rhs) const { return !(*this == rhs); }
};

struct service_state {
    service_instance service_instance_{};
    bool is_available_{};

    [[nodiscard]] bool operator==(service_state const& rhs) const {
        return service_instance_ == rhs.service_instance_ && is_available_ == rhs.is_available_;
    }
    [[nodiscard]] bool operator!=(service_state const& rhs) const { return !(*this == rhs); }
};

std::ostream& operator<<(std::ostream& o, service_instance const& s);
std::ostream& operator<<(std::ostream& o, service_availability const& s);
std::ostream& operator<<(std::ostream& o, client_session const& c);
std::ostream& operator<<(std::ostream& o, message const& n);
std::ostream& operator<<(std::ostream& o, request const& n);
std::ostream& operator<<(std::ostream& o, service_state const& s);
std::ostream& operator<<(std::ostream& o, event_ids const& s);
std::ostream& operator<<(std::ostream& o, event_subscription const& s);
std::ostream& operator<<(std::ostream& o, std::vector<unsigned char> const& s);
}

#endif
