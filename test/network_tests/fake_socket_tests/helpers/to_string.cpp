// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "to_string.hpp"
#include "command_message.hpp"

#include <ostream>
#include <iomanip>

namespace vsomeip_v3::testing {
char const* to_string(vsomeip_v3::return_code_e const& e) {
    switch (e) {
    case return_code_e::E_OK:
        return "E_OK";
    case return_code_e::E_NOT_OK:
        return "E_NOT_OK";
    case return_code_e::E_UNKNOWN_SERVICE:
        return "E_UNKNOWN_SERVICE";
    case return_code_e::E_UNKNOWN_METHOD:
        return "E_UNKNOWN_METHOD";
    case return_code_e::E_NOT_READY:
        return "E_NOT_READY";
    case return_code_e::E_NOT_REACHABLE:
        return "E_NOT_REACHABLE";
    case return_code_e::E_TIMEOUT:
        return "E_TIMEOUT";
    case return_code_e::E_WRONG_PROTOCOL_VERSION:
        return "E_WRONG_PROTOCOL_VERSION";
    case return_code_e::E_WRONG_INTERFACE_VERSION:
        return "E_WRONG_INTERFACE_VERSION";
    case return_code_e::E_MALFORMED_MESSAGE:
        return "E_MALFORMED_MESSAGE";
    case return_code_e::E_WRONG_MESSAGE_TYPE:
        return "E_WRONG_MESSAGE_TYPE";
    case return_code_e::E_UNKNOWN:
        return "E_UNKNOWN";
    }
    return "INVALID";
}

char const* to_string(vsomeip_v3::sd::entry_type_e const& e) {
    switch (e) {
    case sd::entry_type_e::FIND_SERVICE:
        return "FIND_SERVICE";
    case sd::entry_type_e::STOP_OFFER_SERVICE:
        return "(STOP_)OFFER_SERVICE"; // OFFER_SERVICE + STOP_OFFER_SERVICE have the same numerical value
    case sd::entry_type_e::REQUEST_SERVICE:
        return "REQUEST_SERVICE";
    case sd::entry_type_e::FIND_EVENT_GROUP:
        return "FIND_EVENT_GROUP";
    case sd::entry_type_e::STOP_PUBLISH_EVENTGROUP:
        return "(STOP_)PUBLISH_EVENTGROUP";
    case sd::entry_type_e::STOP_SUBSCRIBE_EVENTGROUP:
        return "(STOP_)SUBSCRIBE_EVENTGROUP";
    case sd::entry_type_e::STOP_SUBSCRIBE_EVENTGROUP_ACK:
        return "(STOP_)SUBSCRIBE_EVENTGROUP_ACK";
    case sd::entry_type_e::UNKNOWN:
        return "UNKNOWN";
    }
    return "INVALID";
}

char const* to_string(vsomeip_v3::protocol::routing_info_entry_type_e e) {
    switch (e) {
    case protocol::routing_info_entry_type_e::RIE_ADD_SERVICE_INSTANCE:
        return "RIE_ADD_SERVICE_INSTANCE";
    case protocol::routing_info_entry_type_e::RIE_DELETE_SERVICE_INSTANCE:
        return "RIE_DELETE_SERVICE_INSTANCE";
    case protocol::routing_info_entry_type_e::RIE_UNKNOWN:
        return "RIE_UNKNOWN";
    default:
        return "RIE_INVALID";
    }
}

char const* to_string(vsomeip_v3::sd::entry_type_e _id, ttl_t _ttl) {
    switch (_id) {
    case vsomeip_v3::sd::entry_type_e::FIND_SERVICE:
        return "FIND_SERVICE";
    case vsomeip_v3::sd::entry_type_e::OFFER_SERVICE:
        if (_ttl) {
            return "OFFER_SERVICE";
        }
        return "STOP_OFFER_SERVICE";
    case vsomeip_v3::sd::entry_type_e::REQUEST_SERVICE:
        return "REQUEST_SERVICE";
    case vsomeip_v3::sd::entry_type_e::FIND_EVENT_GROUP:
        return "FIND_EVENT_GROUP";
    case vsomeip_v3::sd::entry_type_e::PUBLISH_EVENTGROUP:
        if (_ttl) {
            return "PUBLISH_EVENTGROUP";
        }
        return "STOP_PUBLISH_EVENTGROUP";
    case vsomeip_v3::sd::entry_type_e::SUBSCRIBE_EVENTGROUP:
        if (_ttl) {
            return "SUBSCRIBE_EVENTGROUP";
        }
        return "STOP_SUBSCRIBE_EVENTGROUP";
    case vsomeip_v3::sd::entry_type_e::SUBSCRIBE_EVENTGROUP_ACK:
        if (_ttl) {
            return "SUBSCRIBE_EVENTGROUP_ACK";
        }
        return "STOP_SUBSCRIBE_EVENTGROUP_ACK";
    default:
        return "UNKNOWN_ID";
    }
}
std::string to_string(vsomeip_v3::protocol::service_data const& service) {
    std::stringstream s;
    s << "[" << hex4(service.service_) << "." << hex4(service.instance_) << ":" << static_cast<int>(service.major_version_) << "."
      << service.minor_version_ << "]";
    return s.str();
}
std::string to_string(vsomeip_v3::protocol::routing_info_entry_data const& e) {
    std::stringstream s;
    s << "routing_info_type: " << to_string(e.type_);
    s << ", client_id: " << hex4(e.client_);
    s << ", address: " << e.address_;
    s << ", port: " << e.port_;
    s << ", services: " << to_string(e.services_);
    return s.str();
}
std::string to_string(vsomeip_v3::protocol::routing_info_command_data const& c) {
    std::stringstream s;
    s << "routing_info: " << to_string(c.payload_);
    return s.str();
}
std::string to_string(command_message const& c) {
    std::stringstream s;
    s << c;
    return s.str();
}

std::string to_string(std::shared_ptr<vsomeip_v3::sd::entry_impl> const& e) {
    std::stringstream s;
    s << "{type: " << to_string(e->get_type()) << ", service: " << hex4(e->get_service()) << ", instance: " << hex4(e->get_instance())
      << ", ttl: " << e->get_ttl() << "}";

    return s.str();
}
std::string to_string(std::string const& _str) {
    return "\"" + _str + "\"";
}

std::string hex_bytes_to_string(std::string_view bytes) {
    std::ostringstream os;

    os << std::hex << std::setfill('0');

    for (char ch : bytes) {
        os << std::setw(2) << static_cast<uint16_t>(ch);
    }

    return os.str();
}
std::string to_string(vsomeip_v3::payload const& p) {
    std::vector<unsigned char> input_payload;
    auto payload_it = p.get_data();
    input_payload.reserve(p.get_length());
    std::copy(payload_it, payload_it + p.get_length(), std::back_inserter(input_payload));
    return to_string(input_payload);
}
std::string to_string(vsomeip_v3::protocol::command_header const& c) {
    std::stringstream s;
    s << "{id: " << c.id_ << ", version: " << c.version_ << ", client: " << hex4(c.client_) << ", length: " << c.length_ << "}";
    return s.str();
}
}
