// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "service_state.hpp"

#include <boost/asio/ip/address.hpp>
#include <sys/types.h>
#include <cstdint>
#include <string>
#include <variant>
#include <optional>
#include <vector>

namespace vsomeip_v3::testing {
struct application_config {
    std::string name_;
    std::optional<client_t> id_;
    std::optional<size_t> threads_ = std::nullopt;
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
    std::optional<someip_tp> tp_;
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

/// The port ranges vsomeip falls back to when an "acceptances" entry does not list any range.
namespace default_ports {
inline constexpr port_range optional_client{30491, 30499};
inline constexpr port_range optional_client_spare{30898, 30998};
inline constexpr port_range optional_server{30501, 30599};
inline constexpr port_range secure_client{32491, 32499};
inline constexpr port_range secure_client_spare{32898, 32998};
inline constexpr port_range secure_server{32501, 32599};
}

/// Classification of a port range inside an "acceptances" entry. Secure services may only be
/// offered on a `secure` port; `optional` ports are the ones that may carry unprotected traffic.
enum class port_type { optional, secure };

struct acceptance_port_range {
    std::optional<port_range> ports_;
    port_type type_{port_type::optional};
};

/// One entry of the "acceptances" array, i.e. the SD acceptance rules for a single remote device.
struct acceptance_config {
    boost::asio::ip::address address_;
    std::vector<std::string> activation_paths_;
    std::vector<acceptance_port_range> unreliable_;
    std::vector<acceptance_port_range> reliable_;
};

struct service_discovery_config {
    uint32_t initial_delay_min_{10};
    uint32_t initial_delay_max_{100};
};
struct security_credentials {
    uid_t uid_{};
    gid_t gid_{};
};

/// Credential based security. Every entry of allowed_ becomes an allow-policy for that uid/gid
/// pair; with check_credentials_ set, any other local peer is refused instead of only logged
/// (audit mode). Credentials are only passed over UDS, so this has no effect on local TCP routing.
struct security_config {
    bool check_credentials_{true};
    std::vector<security_credentials> allowed_;
};
struct ecu_config {
    /// Construct an ecu_config from the list of interfaces to offer on the boardnet.
    /// offered:      interfaces this ECU offers on the boardnet (ports auto-assigned from base_port)
    /// base_port:    first port to assign; incremented per offered interface
    ecu_config() = default;
    ecu_config(std::vector<interface> offered, vsomeip::port_t base_port = 30501);

    ecu_config& add_interface(std::vector<interface> offered, vsomeip::port_t base_port = 30501);

    ecu_config& with_initial_delay(uint32_t min, uint32_t max);

    /// Adds an "acceptances" entry, i.e. the SD acceptance rules of one remote device.
    ecu_config& add_acceptance(acceptance_config rule);

    /// Marks a service instance as secure: offers of it are only accepted when they arrive from a
    /// secure port of a device that has acceptance rules configured.
    ecu_config& add_secure_service(service_instance si);

    std::vector<application_config> apps_;
    std::vector<service_config> services_;
    std::string network_;
    boost::asio::ip::address unicast_ip_{boost::asio::ip::make_address("127.0.0.1")};
    std::optional<std::variant<local_tcp_config, std::string>> routing_config_ = std::string("routingmanagerd"); // defaults to uds
    std::optional<client_port_config> client_ports_ = client_port_config{.remote_ports_ = {30501, 30599}, .client_ports_ = {30491, 30499}};
    std::vector<acceptance_config> acceptances_;
    std::vector<service_instance> secure_services_;
    bool sd_{true};
    service_discovery_config service_discovery_;
    bool uds_preferred_{false};
    /// No security node at all means the security implementation stays switched off.
    std::optional<security_config> security_;
};

/// Serializes an ecu_config to a vsomeip-compatible JSON configuration string.
std::string to_json_string(const ecu_config& cfg);

/// Returns a copy of an existing ecu_config with additional boardnet interfaces appended.
inline ecu_config with_interfaces(ecu_config base, std::vector<interface> extra, vsomeip::port_t base_port = 30501) {
    base.add_interface(std::move(extra), base_port);
    return base;
}
}
