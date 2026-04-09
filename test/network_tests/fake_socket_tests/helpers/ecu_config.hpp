// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "service_state.hpp"

#include <boost/asio/ip/address.hpp>

#include <string>
#include <variant>
#include <optional>
#include <vector>

namespace vsomeip_v3::testing {
struct application_config {
    std::string name_;
    std::optional<client_t> id_;
};
struct event_config {
    vsomeip::event_t event_id_{};
    bool is_field_{false};
    bool is_reliable_{false};
};
struct eventgroup_config {
    vsomeip::eventgroup_t eventgroup_id_{};
    std::vector<vsomeip::event_t> event_ids_;
};
struct reliable_port_config {
    port_t port_{};
    bool enable_magic_cookies_{false};
};
struct service_config {
    service_instance si_;
    std::optional<port_t> unreliable_port_;
    std::optional<reliable_port_config> reliable_port_;
    std::vector<event_config> events_;
    std::vector<eventgroup_config> event_groups_;
};

struct local_tcp_config {
    std::string router_name_;
    boost::asio::ip::address host_;
    boost::asio::ip::address guest_;
};

struct port_range {
    port_t first_{};
    port_t last_{};
};

struct client_port_config {
    port_range remote_ports_;
    port_range client_ports_;
};

struct ecu_config {
    /// Construct an ecu_config from the list of interfaces to offer on the boardnet.
    /// offered:      interfaces this ECU offers on the boardnet (ports auto-assigned from base_port)
    /// base_port:    first port to assign; incremented per offered interface
    ecu_config() = default;
    ecu_config(std::vector<interface> offered, vsomeip::port_t base_port = 30501);

    ecu_config& add_interface(std::vector<interface> offered, vsomeip::port_t base_port = 30501);

    std::vector<application_config> apps_;
    std::vector<service_config> services_;
    std::string network_;
    boost::asio::ip::address unicast_ip_{boost::asio::ip::make_address("127.0.0.1")};
    std::optional<std::variant<local_tcp_config, std::string>> routing_config_ = std::string("routingmanagerd"); // defaults to uds
    std::optional<client_port_config> client_ports_ = client_port_config{.remote_ports_ = {30501, 30599}, .client_ports_ = {30491, 30499}};
    bool sd_{true};
    bool uds_preferred_{false};
};

/// Serializes an ecu_config to a vsomeip-compatible JSON configuration string.
std::string to_json_string(const ecu_config& cfg);

/// Returns a copy of an existing ecu_config with additional boardnet interfaces appended.
inline ecu_config with_interfaces(ecu_config base, std::vector<interface> extra, vsomeip::port_t base_port = 30501) {
    base.add_interface(std::move(extra), base_port);
    return base;
}
}
