// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/primitive_types.hpp>
#include <vsomeip/vsomeip.hpp>

#include <ostream>
#include <utility>
#include <vector>
#include <iomanip>
#include <sstream>

namespace vsomeip_v3::testing {

struct service_instance {
    vsomeip::service_t service_{};
    vsomeip::instance_t instance_{};
    major_version_t major_{ANY_MAJOR};
    minor_version_t minor_{ANY_MINOR};

    // Not including major/minor since availability handler only delivers service/instance.
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
    vsomeip::reliability_type_e reliability_{vsomeip::reliability_type_e::RT_RELIABLE};
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
    bool reliability_{true};
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

struct interface {
    struct event_spec {
        vsomeip::event_t event_id_{};
        vsomeip::eventgroup_t eventgroup_id_{};
        vsomeip::reliability_type_e reliability_{vsomeip::reliability_type_e::RT_UNRELIABLE};
    };

    explicit interface(vsomeip::service_t service,
                       std::vector<event_spec> events = {event_spec{0x8001, 0x1, vsomeip::reliability_type_e::RT_UNRELIABLE}},
                       std::vector<event_spec> fields = {event_spec{0x8002, 0x1, vsomeip::reliability_type_e::RT_UNRELIABLE}},
                       vsomeip::instance_t instance = 0x1);

    service_instance instance_;
    std::vector<event_ids> events_;
    std::vector<event_ids> fields_;
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
