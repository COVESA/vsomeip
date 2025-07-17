// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "service_state.hpp"

namespace vsomeip_v3::testing {

static char const* to_string(vsomeip_v3::message_type_e m) {
    switch (m) {
    case vsomeip_v3::message_type_e::MT_REQUEST:
        return "MT_REQUEST";
    case vsomeip_v3::message_type_e::MT_REQUEST_NO_RETURN:
        return "MT_REQUEST_NO_RETURN";
    case vsomeip_v3::message_type_e::MT_NOTIFICATION:
        return "MT_NOTIFICATION";
    case vsomeip_v3::message_type_e::MT_REQUEST_ACK:
        return "MT_REQUEST_ACK";
    case vsomeip_v3::message_type_e::MT_REQUEST_NO_RETURN_ACK:
        return "MT_REQUEST_NO_RETURN_ACK";
    case vsomeip_v3::message_type_e::MT_NOTIFICATION_ACK:
        return "MT_NOTIFICATION_ACK";
    case vsomeip_v3::message_type_e::MT_RESPONSE:
        return "MT_RESPONSE";
    case vsomeip_v3::message_type_e::MT_ERROR:
        return "MT_ERROR";
    case vsomeip_v3::message_type_e::MT_RESPONSE_ACK:
        return "MT_RESPONSE_ACK";
    case vsomeip_v3::message_type_e::MT_ERROR_ACK:
        return "MT_ERROR_ACK";
    case vsomeip_v3::message_type_e::MT_UNKNOWN:
        return "MT_UNKNOWN";
    default:
        return "MT_UNKNOWN";
    }
}

std::ostream& operator<<(std::ostream& o, service_instance const& s) {
    return o << "Service: [" << std::hex << std::setfill('0') << std::setw(4) << s.service_ << "."
             << s.instance_ << "]";
}

std::ostream& operator<<(std::ostream& o, client_session const& c) {
    return o << "Client/Session: [" << std::hex << std::setfill('0') << std::setw(4) << c.client_
             << '/' << c.session_ << ']';
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
    return o << "[" << std::hex << std::setfill('0') << std::setw(4) << s.service_instance_.service_
             << "." << std::setw(4) << s.service_instance_.instance_ << '.' << std::setw(4)
             << s.method_ << '.' << to_string(s.message_type_) << "]" << std::dec;
}

std::ostream& operator<<(std::ostream& o, service_state const& s) {
    return o << s.service_instance_ << ", is_available: " << (s.is_available_ ? "true" : "false");
}

std::ostream& operator<<(std::ostream& o, event_ids const& s) {
    return o << "[" << std::hex << std::setfill('0') << std::setw(4) << s.si_.service_ << "."
             << std::setw(4) << s.si_.instance_ << '.' << std::setw(4) << s.eventgroup_id_ << '.'
             << std::setw(4) << s.event_id_ << "]" << std::dec;
}

std::ostream& operator<<(std::ostream& o, event_subscription const& s) {
    return o << "event: " << s.ei_ << ", error_code: " << std::hex << s.error_code_;
}

}
