// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "to_string.hpp"
#include "command_message.hpp"

#include <ostream>
#include <iomanip>

namespace vsomeip_v3::testing {
char const* to_string(vsomeip_v3::protocol::id_e _id) {
    switch (_id) {
    case protocol::id_e::ASSIGN_CLIENT_ID:
        return "ASSIGN_CLIENT_ID";
    case protocol::id_e::ASSIGN_CLIENT_ACK_ID:
        return "ASSIGN_CLIENT_ACK_ID";
    case protocol::id_e::ROUTING_INFO_ID:
        return "ROUTING_INFO_ID";
    case protocol::id_e::PING_ID:
        return "PING_ID";
    case protocol::id_e::PONG_ID:
        return "PONG_ID";
    case protocol::id_e::OFFER_SERVICE_ID:
        return "OFFER_SERVICE_ID";
    case protocol::id_e::STOP_OFFER_SERVICE_ID:
        return "STOP_OFFER_SERVICE_ID";
    case protocol::id_e::SUBSCRIBE_ID:
        return "SUBSCRIBE_ID";
    case protocol::id_e::UNSUBSCRIBE_ID:
        return "UNSUBSCRIBE_ID";
    case protocol::id_e::REQUEST_SERVICE_ID:
        return "REQUEST_SERVICE_ID";
    case protocol::id_e::RELEASE_SERVICE_ID:
        return "RELEASE_SERVICE_ID";
    case protocol::id_e::SUBSCRIBE_NACK_ID:
        return "SUBSCRIBE_NACK_ID";
    case protocol::id_e::SUBSCRIBE_ACK_ID:
        return "SUBSCRIBE_ACK_ID";
    case protocol::id_e::SEND_ID:
        return "SEND_ID";
    case protocol::id_e::NOTIFY_ID:
        return "NOTIFY_ID";
    case protocol::id_e::NOTIFY_ONE_ID:
        return "NOTIFY_ONE_ID";
    case protocol::id_e::REGISTER_EVENT_ID:
        return "REGISTER_EVENT_ID";
    case protocol::id_e::UNREGISTER_EVENT_ID:
        return "UNREGISTER_EVENT_ID";
    case protocol::id_e::ID_RESPONSE_ID:
        return "ID_RESPONSE_ID";
    case protocol::id_e::ID_REQUEST_ID:
        return "ID_REQUEST_ID";
    case protocol::id_e::OFFERED_SERVICES_REQUEST_ID:
        return "OFFERED_SERVICES_REQUEST_ID";
    case protocol::id_e::OFFERED_SERVICES_RESPONSE_ID:
        return "OFFERED_SERVICES_RESPONSE_ID";
    case protocol::id_e::UNSUBSCRIBE_ACK_ID:
        return "UNSUBSCRIBE_ACK_ID";
    case protocol::id_e::RESEND_PROVIDED_EVENTS_ID:
        return "RESEND_PROVIDED_EVENTS_ID";
    case protocol::id_e::UPDATE_SECURITY_POLICY_ID:
        return "UPDATE_SECURITY_POLICY_ID";
    case protocol::id_e::UPDATE_SECURITY_POLICY_RESPONSE_ID:
        return "UPDATE_SECURITY_POLICY_RESPONSE_ID";
    case protocol::id_e::REMOVE_SECURITY_POLICY_ID:
        return "REMOVE_SECURITY_POLICY_ID";
    case protocol::id_e::REMOVE_SECURITY_POLICY_RESPONSE_ID:
        return "REMOVE_SECURITY_POLICY_RESPONSE_ID";
    case protocol::id_e::UPDATE_SECURITY_CREDENTIALS_ID:
        return "UPDATE_SECURITY_CREDENTIALS_ID";
    case protocol::id_e::DISTRIBUTE_SECURITY_POLICIES_ID:
        return "DISTRIBUTE_SECURITY_POLICIES_ID";
    case protocol::id_e::UPDATE_SECURITY_POLICY_INT_ID:
        return "UPDATE_SECURITY_POLICY_INT_ID";
    case protocol::id_e::EXPIRE_ID:
        return "EXPIRE_ID";
    case protocol::id_e::SUSPEND_ID:
        return "SUSPEND_ID";
    case protocol::id_e::CONFIG_ID:
        return "CONFIG_ID";
    case protocol::id_e::UNKNOWN_ID:
        return "UNKNOWN_ID";
    default:
        return "INVALID_ID";
    }
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
std::string to_string(vsomeip_v3::protocol::service const& service) {
    std::stringstream s;
    s << "service: " << std::hex << std::setfill('0') << std::setw(4) << service.service_;
    s << ", instance: " << std::hex << std::setfill('0') << std::setw(4) << service.instance_;
    // omitting the major and minor version as they are of little interest for testing
    return s.str();
}
std::string to_string(vsomeip_v3::protocol::routing_info_entry const& e) {
    std::stringstream s;
    s << "routing_info_type: " << to_string(e.get_type());
    s << ", client_id: " << std::hex << std::setfill('0') << std::setw(4) << e.get_client();
    s << ", address: " << e.get_address();
    s << ", port: " << e.get_port();
    s << ", services: " << to_string(e.get_services());
    return s.str();
}
std::string to_string(vsomeip_v3::protocol::routing_info_command const& c) {
    std::stringstream s;
    s << "routing_info: " << to_string(c.get_entries());
    return s.str();
}
std::string to_string(command_message const& c) {
    std::stringstream s;
    s << c;
    return s.str();
}
std::string to_string(std::string const& _str) {
    return "\"" + _str + "\"";
}

std::string hex_bytes_to_string(std::string_view bytes) {
    std::ostringstream os;

    os << std::hex << std::setfill('0');

    for (unsigned char ch : bytes) {
        os << std::setw(2) << static_cast<uint16_t>(ch);
    }

    return os.str();
}
std::string to_string(vsomeip_v3::protocol::config_command const& c) {
    std::vector<std::pair<std::string, std::string>> v;
    auto const& container = c.configs();
    v.reserve(container.size());

    for (const auto& [key, value] : container) {
        v.emplace_back(key, (key == "expected_id") ? hex_bytes_to_string(value) : value);
    }

    return to_string(v);
}
}
