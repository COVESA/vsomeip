// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/logging.hpp"

namespace vsomeip_v3::protocol {
char const* to_string(id_e _id) {
    switch (_id) {
    case protocol::id_e::ASSIGN_CLIENT_ID:
        return "ASSIGN_CLIENT_ID";
    case protocol::id_e::ASSIGN_CLIENT_ACK_ID:
        return "ASSIGN_CLIENT_ACK_ID";
    case protocol::id_e::DEREGISTER_APPLICATION_ID:
        return "DEREGISTER_APPLICATION_ID";
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
std::ostream& operator<<(std::ostream& _out, id_e _id) {
    return _out << to_string(_id);
}

}
