// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "protocol.hpp"

#include "../../configuration/include/debounce_filter_impl.hpp"

#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/primitive_types.hpp>
#include <vsomeip/message.hpp>
#include <vsomeip/payload.hpp>
#include <vsomeip/defines.hpp>

#include <boost/asio/ip/address_v4.hpp>

#include "protocol.hpp"

#include <array>
#include <compare>
#include <cstring>
#include <initializer_list>
#include <span>
#include <string_view>
#include <compare>
#include <vector>
#include <type_traits>
#include <span>
#include <set>
#include <memory>
#include <numeric>

namespace vsomeip_v3::protocol {

struct command_header {
    auto operator<=>(command_header const&) const = default;

    static command_header create(id_e _id, uint32_t _length, client_t _client) {
        return {.id_ = _id, .version_ = IPC_VERSION, .client_ = _client, .length_ = _length};
    }

    id_e id_;
    version_t version_;
    client_t client_; // Sender's client identifier.
    uint32_t length_; // Payload length (bytes following the header).
};

struct simple_command_data {
    auto operator<=>(simple_command_data const&) const = default;

    static simple_command_data create(id_e _id, client_t _client) { return {.header_ = command_header::create(_id, 0, _client)}; }

    command_header header_;
};

struct service_data {
    auto operator<=>(service_data const&) const = default;

    static constexpr uint32_t wire_size_{sizeof(service_t) + sizeof(instance_t) + sizeof(major_version_t) + sizeof(minor_version_t)};

    service_t service_;
    instance_t instance_;
    major_version_t major_version_;
    minor_version_t minor_version_;
};

struct service_command_data {
    auto operator<=>(service_command_data const&) const = default;

    static service_command_data create(id_e _id, client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                       minor_version_t _minor) {
        return {.header_ = command_header::create(_id, service_data::wire_size_, _client),
                .payload_ = {.service_ = _service, .instance_ = _instance, .major_version_ = _major, .minor_version_ = _minor}};
    }

    command_header header_;
    service_data payload_;
};

struct multiple_service_command_data {
    static multiple_service_command_data create(id_e _id, client_t _client, std::span<service_data const> _in) {
        return {.header_ = command_header::create(_id, static_cast<uint32_t>(_in.size()) * service_data::wire_size_, _client),
                .payload_ = _in};
    }

    command_header header_;
    std::span<service_data const> payload_;
};

struct release_service_data {
    auto operator<=>(release_service_data const&) const = default;

    static constexpr uint32_t wire_size_{sizeof(service_t) + sizeof(instance_t)};

    service_t service_;
    instance_t instance_;
};

struct release_service_command_data {
    auto operator<=>(release_service_command_data const&) const = default;

    static release_service_command_data create(client_t _client, service_t _service, instance_t _instance) {
        return {.header_ = command_header::create(id_e::RELEASE_SERVICE_ID, release_service_data::wire_size_, _client),
                .payload_ = {.service_ = _service, .instance_ = _instance}};
    }

    command_header header_;
    release_service_data payload_;
};

template<typename T>
struct single_field_command_data {
    auto operator<=>(single_field_command_data const&) const = default;

    static single_field_command_data create(id_e _id, client_t _client, T const& _field) {
        return {.header_ = command_header::create(_id, sizeof(T), _client), .payload_ = _field};
    }

    command_header header_;
    T payload_;
};

struct config_entry {
    std::string_view key_{};
    std::string_view value_{};
};

struct config_command_data {
    command_header header_;
    std::span<const config_entry> payload_;
};

struct assign_client_data {
    std::string_view name_;
    std::array<uint8_t, 4> address_bytes_{};
    port_t port_{0};
    bool has_address_{false};
};

struct assign_client_command_data {
    command_header header_;
    assign_client_data payload_;
};
struct register_event_data {
    auto operator<=>(register_event_data const&) const = default;

    static constexpr uint32_t fixed_wire_size_{sizeof(service_t) + sizeof(instance_t) + sizeof(event_t) + sizeof(event_type_e)
                                               + sizeof(bool) + sizeof(reliability_type_e) + sizeof(bool) + sizeof(uint16_t)};

    uint32_t payload_size() const {
        return fixed_wire_size_ + static_cast<uint32_t>(eventgroups_.size()) * static_cast<uint32_t>(sizeof(eventgroup_t));
    }

    service_t service_;
    instance_t instance_;
    event_t event_;
    event_type_e event_type_;
    bool is_provided_;
    reliability_type_e reliability_;
    bool is_cyclic_;
    std::vector<eventgroup_t> eventgroups_;
};

struct ipc_message_header {
    static constexpr uint32_t wire_size_ = sizeof(instance_t) + sizeof(bool) + sizeof(uint8_t) + sizeof(client_t);
    instance_t instance_;
    bool reliable_;
    uint8_t status_;
    client_t target_;
};

struct extended_someip_message {
    ipc_message_header auxiliary_header_;
    std::shared_ptr<message> data_;
};

struct raw_someip_message {
    ipc_message_header auxiliary_header_;
    byte_t const* data_;
    uint32_t size_;
};

struct send_command_data {
    command_header header_;
    extended_someip_message payload_;
};

struct send_command_raw {
    command_header header_;
    raw_someip_message payload_;
};

struct unregister_event_data {
    auto operator<=>(unregister_event_data const&) const = default;

    static constexpr uint32_t wire_size_{sizeof(service_t) + sizeof(instance_t) + sizeof(event_t) + sizeof(uint8_t)};
    service_t service_;
    instance_t instance_;
    event_t event_;
    bool is_provided_;
};

struct register_events_command_data {
    static register_events_command_data create(client_t _client, std::span<register_event_data const> _in) {
        uint32_t length{0};
        for (auto const& reg : _in) {
            length += reg.payload_size();
        }
        return {.header_ = command_header::create(id_e::REGISTER_EVENT_ID, length, _client), .payload_ = _in};
    }

    command_header header_;
    std::span<register_event_data const> payload_;
};

struct unregister_event_command_data {
    auto operator<=>(unregister_event_command_data const&) const = default;

    static unregister_event_command_data create(client_t _client, service_t _service, instance_t _instance, event_t _event,
                                                bool _is_provided) {
        return {.header_ = command_header::create(id_e::UNREGISTER_EVENT_ID, unregister_event_data::wire_size_, _client),
                .payload_ = {.service_ = _service, .instance_ = _instance, .event_ = _event, .is_provided_ = _is_provided}};
    }

    command_header header_;
    unregister_event_data payload_;
};

struct subscribe_data {
    auto operator<=>(subscribe_data const&) const = default;

    static constexpr uint32_t wire_size_{sizeof(service_t) + sizeof(instance_t) + sizeof(eventgroup_t) + sizeof(major_version_t)
                                         + sizeof(event_t) + sizeof(pending_id_t)};

    service_t service_;
    instance_t instance_;
    eventgroup_t eventgroup_;
    major_version_t major_;
    event_t event_;
    pending_id_t pending_id_;
};

struct subscribe_with_filter_data {
    auto operator<=>(subscribe_with_filter_data const&) const = default;

    subscribe_data data_;
    std::shared_ptr<debounce_filter_impl_t> filter_;
};

struct unsubscribe_command_data {
    auto operator<=>(unsubscribe_command_data const&) const = default;

    static unsubscribe_command_data create(id_e _id, client_t _client, subscribe_data _data) {
        return {.header_ = command_header::create(_id, subscribe_data::wire_size_, _client), .payload_ = _data};
    }

    command_header header_;
    subscribe_data payload_;
};

struct subscribe_command_data {
    command_header header_;
    subscribe_with_filter_data payload_;
};

struct subscribe_answer_data {
    auto operator<=>(subscribe_answer_data const&) const = default;

    static constexpr uint32_t wire_size_{sizeof(service_t) + sizeof(instance_t) + sizeof(eventgroup_t) + sizeof(client_t) + sizeof(event_t)
                                         + sizeof(pending_id_t)};

    service_t service_;
    instance_t instance_;
    eventgroup_t eventgroup_;
    client_t subscriber_;
    event_t event_;
    pending_id_t pending_id_;
};

struct subscribe_answer_command_data {
    auto operator<=>(subscribe_answer_command_data const&) const = default;

    command_header header_;
    subscribe_answer_data payload_;
};

struct unsubscribe_ack_data {
    auto operator<=>(unsubscribe_ack_data const&) const = default;

    static constexpr uint32_t wire_size_{sizeof(service_t) + sizeof(instance_t) + sizeof(eventgroup_t) + sizeof(pending_id_t)};

    service_t service_;
    instance_t instance_;
    eventgroup_t eventgroup_;
    pending_id_t pending_id_;
};

struct unsubscribe_ack_command_data {
    auto operator<=>(unsubscribe_ack_command_data const&) const = default;

    static unsubscribe_ack_command_data create(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                               pending_id_t _pending_id) {
        return {.header_ = command_header::create(id_e::UNSUBSCRIBE_ACK_ID, unsubscribe_ack_data::wire_size_, _client),
                .payload_ = {.service_ = _service, .instance_ = _instance, .eventgroup_ = _eventgroup, .pending_id_ = _pending_id}};
    }

    command_header header_;
    unsubscribe_ack_data payload_;
};

struct remove_security_policy_data {
    auto operator<=>(remove_security_policy_data const&) const = default;

    static constexpr uint32_t wire_size_{sizeof(uint32_t) + sizeof(uid_t) + sizeof(gid_t)};

    uint32_t update_id_;
    uid_t uid_;
    gid_t gid_;
};

struct remove_security_policy_command_data {
    auto operator<=>(remove_security_policy_command_data const&) const = default;

    static remove_security_policy_command_data create(client_t _client, uint32_t _update_id, uid_t _uid, gid_t _gid) {
        return {.header_ = command_header::create(id_e::REMOVE_SECURITY_POLICY_ID, remove_security_policy_data::wire_size_, _client),
                .payload_ = {.update_id_ = _update_id, .uid_ = _uid, .gid_ = _gid}};
    }

    command_header header_;
    remove_security_policy_data payload_;
};

struct update_security_credentials_command_data {
    auto operator<=>(update_security_credentials_command_data const&) const = default;

    command_header header_;
    std::vector<std::pair<uid_t, gid_t>> payload_;
};

// used for serialization and the compound command
struct update_security_policy_view {
    uint32_t update_id_;
    std::span<byte_t const> policy_;
};

// used for deserialization
struct update_security_policy_data {
    uint32_t update_id_;
    std::vector<byte_t> policy_;
};

struct update_security_policy_command_data {
    command_header header_;
    update_security_policy_view payload_;
};

struct distribute_security_policies_data {
    command_header header_;
    std::vector<std::span<byte_t const>> payload_;
};
// One entry of a ROUTING_INFO command. Unlike the fixed-size command payloads
// above, an entry is variable-length: the (IPv4) address is optional and the
// number of services varies, so this is an owning struct rather than a trivially
// copyable POD.
struct routing_info_entry_data {
    bool operator==(routing_info_entry_data const&) const = default;

    routing_info_entry_type_e type_{routing_info_entry_type_e::RIE_UNKNOWN};
    client_t client_{};
    boost::asio::ip::address_v4 address_{};
    port_t port_{0};
    std::vector<service_data> services_{};
};

struct routing_info_command_data {
    bool operator==(routing_info_command_data const&) const = default;

    command_header header_;
    std::vector<routing_info_entry_data> payload_;
};

// Wire-size helpers (sum of field sizes, no padding)
constexpr uint32_t wire_size(command_header const&) {
    return sizeof(id_e) + sizeof(version_t) + sizeof(client_t) + sizeof(uint32_t);
}

constexpr uint32_t wire_size(service_data const&) {
    return service_data::wire_size_;
}

inline uint32_t wire_size(std::set<std::pair<uid_t, gid_t>> const& _input) {
    return static_cast<uint32_t>(_input.size()) * (sizeof(uid_t) + sizeof(gid_t));
}

inline uint32_t wire_size(register_event_data const& _in) {
    return _in.payload_size();
}

inline uint32_t wire_size(std::shared_ptr<message> const& _input) {
    if (!_input) {
        return 0;
    }
    auto p = _input->get_payload();
    return VSOMEIP_FULL_HEADER_SIZE + (p ? p->get_length() : 0);
}

constexpr uint32_t wire_size(release_service_data const&) {
    return release_service_data::wire_size_;
}

constexpr uint32_t wire_size(unregister_event_data const&) {
    return unregister_event_data::wire_size_;
}

constexpr uint32_t wire_size(unsubscribe_ack_data const&) {
    return unsubscribe_ack_data::wire_size_;
}

constexpr uint32_t wire_size(remove_security_policy_data const&) {
    return remove_security_policy_data::wire_size_;
}

inline uint32_t wire_size(std::shared_ptr<debounce_filter_impl_t> const& _filter) {
    return _filter ? static_cast<uint32_t>(sizeof(bool) + sizeof(bool) + sizeof(int64_t)
                                           + (_filter->ignore_.size() * (sizeof(size_t) + sizeof(byte_t))) + sizeof(bool))
                   : 0;
}

inline uint32_t wire_size(routing_info_entry_data const& _entry) {
    // type (1) + entry-size field (4) + client (2)
    uint32_t its_size = static_cast<uint32_t>(ROUTING_INFO_ENTRY_HEADER_SIZE);
    its_size += sizeof(uint32_t); // client-info size field
    if (!_entry.address_.is_unspecified()) {
        its_size += static_cast<uint32_t>(sizeof(boost::asio::ip::address_v4::bytes_type) + sizeof(port_t));
    }
    its_size += sizeof(uint32_t); // services-array size field
    its_size += static_cast<uint32_t>(_entry.services_.size()) * service_data::wire_size_;
    return its_size;
}

inline uint32_t wire_size(config_command_data const& _d) {
    size_t size = wire_size(_d.header_);
    for (auto const& e : _d.payload_) {
        size += sizeof(uint32_t) + e.key_.size() + sizeof(uint32_t) + e.value_.size();
    }
    return static_cast<uint32_t>(size);
}

inline uint32_t wire_size(assign_client_data const& _d) {
    size_t size = sizeof(uint32_t) + _d.name_.size() + sizeof(uint8_t);
    if (_d.has_address_) {
        size += _d.address_bytes_.size() + sizeof(port_t);
    }
    return static_cast<uint32_t>(size);
}

template<typename T>
uint32_t wire_size(T const& _in) {
    return wire_size(_in.header_) + _in.header_.length_;
}

// Factory functions
inline simple_command_data create_ping_cmd(client_t _client) {
    return simple_command_data::create(id_e::PING_ID, _client);
}

inline simple_command_data create_pong_cmd(client_t _client) {
    return simple_command_data::create(id_e::PONG_ID, _client);
}

inline simple_command_data create_suspend_cmd(client_t _client) {
    return simple_command_data::create(id_e::SUSPEND_ID, _client);
}

inline service_command_data create_offer_service_cmd(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                                     minor_version_t _minor) {
    return service_command_data::create(id_e::OFFER_SERVICE_ID, _client, _service, _instance, _major, _minor);
}

inline service_command_data create_stop_offer_service_cmd(client_t _client, service_t _service, instance_t _instance,
                                                          major_version_t _major, minor_version_t _minor) {
    return service_command_data::create(id_e::STOP_OFFER_SERVICE_ID, _client, _service, _instance, _major, _minor);
}

inline auto create_release_service_cmd(client_t _client, service_t _service, instance_t _instance) {
    return release_service_command_data::create(_client, _service, _instance);
}

inline auto create_offered_services_request_cmd(client_t _client, offer_type_e _offer_type) {
    return single_field_command_data<offer_type_e>::create(id_e::OFFERED_SERVICES_REQUEST_ID, _client, _offer_type);
}

inline auto create_resend_provided_events_cmd(client_t _client, pending_remote_offer_id_t _id) {
    return single_field_command_data<pending_remote_offer_id_t>::create(id_e::RESEND_PROVIDED_EVENTS_ID, _client, _id);
}

inline auto create_assign_client_ack_cmd(client_t _client, client_t _assigned_id) {
    return single_field_command_data<client_t>::create(id_e::ASSIGN_CLIENT_ACK_ID, _client, _assigned_id);
}

inline auto create_update_security_policy_response_cmd(client_t _client, uint32_t _update_id) {
    return single_field_command_data<uint32_t>::create(id_e::UPDATE_SECURITY_POLICY_RESPONSE_ID, _client, _update_id);
}

inline auto create_remove_security_policy_response_cmd(client_t _client, uint32_t _update_id) {
    return single_field_command_data<uint32_t>::create(id_e::REMOVE_SECURITY_POLICY_RESPONSE_ID, _client, _update_id);
}

inline auto create_request_service_cmd(client_t _client, std::span<service_data const> _data) {
    return multiple_service_command_data::create(id_e::REQUEST_SERVICE_ID, _client, std::move(_data));
}

inline auto create_offered_services_response_cmd(client_t _client, std::span<service_data const> _data) {
    return multiple_service_command_data::create(id_e::OFFERED_SERVICES_RESPONSE_ID, _client, std::move(_data));
}

inline auto create_register_events_cmd(client_t _client, std::span<register_event_data const> _data) {
    return register_events_command_data::create(_client, std::move(_data));
}

// _sender is the client that emits the command (command header client).
// _target is the addressee carried in the IPC header. For requests/responses
// this equals the message's own client, but for NOTIFY_ONE it is the specific
// subscriber, which is NOT encoded in the (shared) notification message — see
// event::notify(), which reuses one message object for every subscriber. The
// legacy send_command carried this in a dedicated target field for the same
// reason. The header sender drives the routing manager's bound-client security
// check, while the IPC target tells the router which client to deliver to.
inline auto create_send_cmd(id_e _id, client_t _sender, std::shared_ptr<message> const& _msg, client_t _target) {
    return send_command_data{.header_ = command_header::create(_id, ipc_message_header::wire_size_ + wire_size(_msg), _sender),
                             .payload_ = extended_someip_message{.auxiliary_header_ = {.instance_ = _msg->get_instance(),
                                                                                       .reliable_ = _msg->is_reliable(),
                                                                                       .status_ = _msg->get_check_result(),
                                                                                       .target_ = _target},
                                                                 .data_ = _msg}};
}

inline auto create_send_cmd_raw(id_e _id, client_t _sender, byte_t const* _data, uint32_t _size, ipc_message_header _message_header) {
    return send_command_raw{.header_ = command_header::create(_id, ipc_message_header::wire_size_ + _size, _sender),
                            .payload_ = raw_someip_message{.auxiliary_header_ = _message_header, .data_ = _data, .size_ = _size}};
}

inline auto create_unregister_event_cmd(client_t _client, service_t _service, instance_t _instance, event_t _event, bool _is_provided) {
    return unregister_event_command_data::create(_client, _service, _instance, _event, _is_provided);
}

inline auto create_unsubscribe_ack_cmd(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                       pending_id_t _pending_id) {
    return unsubscribe_ack_command_data::create(_client, _service, _instance, _eventgroup, _pending_id);
}

inline auto create_remove_security_policy_cmd(client_t _client, uint32_t _update_id, uid_t _uid, gid_t _gid) {
    return remove_security_policy_command_data::create(_client, _update_id, _uid, _gid);
}

inline auto create_expire_cmd(client_t _client, subscribe_data _data) {
    return unsubscribe_command_data::create(id_e::EXPIRE_ID, _client, _data);
}

inline auto create_unsubscribe_cmd(client_t _client, subscribe_data _data) {
    return unsubscribe_command_data::create(id_e::UNSUBSCRIBE_ID, _client, _data);
}

inline auto create_subscribe_cmd(client_t _client, std::shared_ptr<debounce_filter_impl_t> _filter, subscribe_data _data) {
    return subscribe_command_data{
            .header_ = command_header::create(id_e::SUBSCRIBE_ID, subscribe_data::wire_size_ + wire_size(_filter), _client),
            .payload_ = {.data_ = _data, .filter_ = _filter}};
}

inline auto create_update_security_credentials_cmd(client_t _client, std::set<std::pair<uid_t, gid_t>> const& _input) {
    std::vector<std::pair<uid_t, gid_t>> data{_input.begin(), _input.end()};
    return update_security_credentials_command_data{
            .header_ = command_header::create(id_e::UPDATE_SECURITY_CREDENTIALS_ID, wire_size(_input), _client),
            .payload_ = std::move(data)};
}

inline auto create_update_security_policy_cmd(client_t _client, uint32_t _update_id, std::span<byte_t const> _payload) {
    return update_security_policy_command_data{
            .header_ = command_header::create(id_e::UPDATE_SECURITY_POLICY_ID, static_cast<uint32_t>(_payload.size()) + sizeof(uint32_t),
                                              _client),
            .payload_ = update_security_policy_view{.update_id_ = _update_id, .policy_ = std::move(_payload)}};
}
inline auto create_distribute_security_policy_cmd(client_t _client, std::vector<std::span<byte_t const>> _payload) {
    uint32_t length = static_cast<uint32_t>(_payload.size() + 1) * sizeof(uint32_t);
    for (auto const& payload : _payload) {
        length += static_cast<uint32_t>(payload.size());
    }
    return distribute_security_policies_data{.header_ = command_header::create(id_e::DISTRIBUTE_SECURITY_POLICIES_ID, length, _client),
                                             .payload_ = std::move(_payload)};
}

inline routing_info_command_data create_routing_info_cmd(client_t _client, std::vector<routing_info_entry_data> _entries) {
    uint32_t length = 0;
    for (auto const& entry : _entries) {
        length += wire_size(entry);
    }
    return {.header_ = command_header::create(id_e::ROUTING_INFO_ID, length, _client), .payload_ = std::move(_entries)};
}

inline auto create_subscribe_ack_cmd(client_t _client, subscribe_answer_data _data) {
    return subscribe_answer_command_data{
            .header_ = command_header::create(id_e::SUBSCRIBE_ACK_ID, subscribe_answer_data::wire_size_, _client), .payload_ = _data};
}

inline auto create_subscribe_nack_cmd(client_t _client, subscribe_answer_data _data) {
    return subscribe_answer_command_data{
            .header_ = command_header::create(id_e::SUBSCRIBE_NACK_ID, subscribe_answer_data::wire_size_, _client), .payload_ = _data};
}

inline config_command_data create_config_cmd(client_t _client, std::initializer_list<config_entry> _entries) {
    uint32_t payload_size = 0;
    for (auto const& e : _entries) {
        payload_size += static_cast<uint32_t>(sizeof(uint32_t) + e.key_.size() + sizeof(uint32_t) + e.value_.size());
    }
    return {.header_ = command_header::create(id_e::CONFIG_ID, payload_size, _client),
            .payload_ = std::span<const config_entry>(_entries.begin(), _entries.size())};
}

inline assign_client_command_data create_assign_client_cmd(client_t _client, std::string_view _name,
                                                           std::array<uint8_t, 4> _address_bytes = {}, port_t _port = 0,
                                                           bool _has_address = false) {
    assign_client_data data{.name_ = _name, .address_bytes_ = _address_bytes, .port_ = _port, .has_address_ = _has_address};
    return {.header_ = command_header::create(id_e::ASSIGN_CLIENT_ID, wire_size(data), _client), .payload_ = data};
}
} // namespace vsomeip_v3::protocol
