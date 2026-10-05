// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "service_state.hpp"

#include "../../../../implementation/utility/include/utility.hpp"

namespace vsomeip_v3::testing {

interface::interface(vsomeip::service_t _service, std::vector<event_spec> _events, std::vector<event_spec> _fields,
                     vsomeip::instance_t _instance) : interface({_service, _instance}, std::move(_events), std::move(_fields)) { }

interface::interface(service_instance _instance, std::vector<event_spec> _events, std::vector<event_spec> _fields,
                     std::optional<someip_tp> tp) : instance_(_instance) {
    for (auto const& e : _events) {
        events_.push_back({e.event_id_, e.eventgroup_id_, e.reliability_});
    }
    for (auto const& f : _fields) {
        fields_.push_back({f.event_id_, f.eventgroup_id_, f.reliability_});
    }
    if (tp) {
        tp_ = *tp;
    }
}

static char const* to_string(vsomeip_v3::availability_state_e s) {
    switch (s) {
    case vsomeip_v3::availability_state_e::AS_UNAVAILABLE:
        return "AS_UNAVAILABLE";
    case vsomeip_v3::availability_state_e::AS_OFFERED:
        return "AS_OFFERED";
    case vsomeip_v3::availability_state_e::AS_AVAILABLE:
        return "AS_AVAILABLE";
    case vsomeip_v3::availability_state_e::AS_UNKNOWN:
    default:
        return "AS_UNKNOWN";
    }
}

std::ostream& operator<<(std::ostream& o, service_instance const& s) {
    return o << "[" << hex4(s.service_) << "." << hex4(s.instance_) << ":" << static_cast<int>(s.major_) << "." << s.minor_ << "]";
}

std::ostream& operator<<(std::ostream& o, service_availability const& s) {
    return o << "Availability: [" << s.si_ << ", state: " << to_string(s.state_) << "]";
}

std::ostream& operator<<(std::ostream& o, client_session const& c) {
    return o << "Client/Session: [" << std::hex << std::setfill('0') << std::setw(4) << c.client_ << '/' << c.session_ << ']';
}

std::ostream& operator<<(std::ostream& o, message const& n) {
    return o << n.service_instance_ << ", " << n.client_session_ << ", Method: " << n.method_
             << ", MessageType: " << to_string(n.message_type_) << ", Payload: " << n.payload_;
}

std::ostream& operator<<(std::ostream& o, std::vector<unsigned char> const& s) {
    bool first = true;
    o << '[';
    o << std::hex;
    for (auto c : s) {
        if (first) {
            first = false;
        } else {
            o << ", ";
        }
        o << static_cast<int>(c);
    }
    return o << ']' << std::dec;
}

std::ostream& operator<<(std::ostream& o, request const& s) {
    return o << "[" << std::hex << std::setfill('0') << std::setw(4) << s.service_instance_.service_ << "." << std::setw(4)
             << s.service_instance_.instance_ << '.' << std::setw(4) << s.method_ << '.' << to_string(s.message_type_) << "]" << std::dec;
}

std::ostream& operator<<(std::ostream& o, service_state const& s) {
    return o << s.service_instance_ << ", is_available: " << (s.is_available_ ? "true" : "false");
}

std::ostream& operator<<(std::ostream& o, event_ids const& s) {
    return o << "[" << std::hex << std::setfill('0') << std::setw(4) << s.si_.service_ << "." << std::setw(4) << s.si_.instance_ << '.'
             << std::setw(4) << s.eventgroup_id_ << '.' << std::setw(4) << s.event_id_ << std::dec
             << ":reliable=" << static_cast<unsigned int>(static_cast<uint8_t>(s.reliability_)) << "]";
}

std::ostream& operator<<(std::ostream& o, event_spec const& s) {
    return o << "[" << std::hex << std::setfill('0') << std::setw(4) << s.eventgroup_id_ << '.' << std::setw(4) << s.event_id_ << std::dec
             << ":reliable=" << static_cast<unsigned int>(static_cast<uint8_t>(s.reliability_)) << "]";
}

std::ostream& operator<<(std::ostream& o, event_subscription const& s) {
    return o << "event: " << s.ei_ << ", error_code: " << std::hex << s.error_code_;
}

}
