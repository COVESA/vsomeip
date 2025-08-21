// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "to_string.hpp"

#include "../../../../implementation/protocol/src/routing_info_command.cpp"
#include "../../../../implementation/protocol/src/config_command.cpp"
#include "../../../../implementation/protocol/src/command.cpp"
#include "../../../../implementation/protocol/src/routing_info_entry.cpp"

#include <ostream>
#include <iomanip>

namespace vsomeip_v3::testing {
char const* to_string(vsomeip_v3::protocol::id_e _id) {
    switch (_id) {
    case protocol::id_e::ASSIGN_CLIENT_ID:
        return "ASSIGN_CLIENT_ID";
    case protocol::id_e::ASSIGN_CLIENT_ACK_ID:
        return "ASSIGN_CLIENT_ACK_ID";
    case protocol::id_e::REGISTER_APPLICATION_ID:
        return "REGISTER_APPLICATION_ID";
    case protocol::id_e::DEREGISTER_APPLICATION_ID:
        return "DEREGISTER_APPLICATION_ID";
    case protocol::id_e::ROUTING_INFO_ID:
        return "ROUTING_INFO_ID";
    case protocol::id_e::REGISTERED_ACK_ID:
        return "REGISTERED_ACK_ID";
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
    case protocol::routing_info_entry_type_e::RIE_ADD_CLIENT:
        return "RIE_ADD_CLIENT";
    case protocol::routing_info_entry_type_e::RIE_DELETE_CLIENT:
        return "RIE_DELETE_CLIENT";
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
std::string to_string(std::pair<std::string, std::string> const& pair) {
    return "{" + pair.first + " : " + pair.second + "}";
}
std::string to_string(vsomeip_v3::protocol::config_command const& c) {
    std::vector<std::pair<std::string, std::string>> v;
    auto const& container = c.configs();
    v.reserve(container.size());
    std::copy(container.begin(), container.end(), std::back_inserter(v));
    return to_string(v);
}
}
