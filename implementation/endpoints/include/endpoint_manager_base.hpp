// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <mutex>
#include <map>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <optional>
#include <condition_variable>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <vsomeip/primitive_types.hpp>
#include <vsomeip/vsomeip_sec.h>
#include <vsomeip/enumeration_types.hpp>
#include "local_endpoint_manager_host.hpp"

#include "../../utility/include/async.hpp"

namespace vsomeip_v3 {

enum class transport_protocol_e : uint8_t { TCP = 0x00, UDS = 0x01 };

class configuration;
class routing_host;
class local_server;
class local_endpoint;
class local_acceptor;

class endpoint_manager_base : public std::enable_shared_from_this<endpoint_manager_base> {
public:
    endpoint_manager_base(local_endpoint_manager_host& _host, boost::asio::io_context& _io,
                          const std::shared_ptr<configuration>& _configuration, std::string _name, std::string _client_host);
    virtual ~endpoint_manager_base() = default;

    void init(std::shared_ptr<routing_host> const& _local_message_handler);

    std::shared_ptr<local_server> create_local_server(transport_protocol_e _transport_protocol);

    std::shared_ptr<local_endpoint> create_routing_client();
    std::shared_ptr<local_endpoint> create_consumer_endpoint(client_t _client, client_t _own_id, boost::asio::ip::address _remote_address,
                                                             port_t _remote_port, std::optional<vsomeip_sec_client_t> _expected_sec_client);

private:
    client_t get_client_id() const;
    std::string get_client_env() const;

    std::shared_ptr<local_endpoint> create_local_client_endpoint(client_t _client, client_t _own_id,
                                                                 boost::asio::ip::address const& _remote_address, port_t _remote_port,
                                                                 bool _is_guest,
                                                                 std::optional<vsomeip_sec_client_t> _expected_sec_client = {});

    bool get_local_server_port(port_t& _port, const std::set<port_t>& _used_ports) const;

    // local server creation helpers
    std::shared_ptr<local_acceptor> create_uds_local_acceptor(const std::string& _path, client_t _client);
    std::shared_ptr<local_acceptor> create_tcp_local_acceptor(client_t _client);

private:
    local_endpoint_manager_host& host_;
    boost::asio::io_context& io_;
    std::shared_ptr<configuration> configuration_;

    bool const is_local_routing_;
    bool const is_uds_preferred_; // UDS is transport type for local routing, but only if preferred in configuration
    port_t local_port_; // local (client) port when connecting to other
                        // vsomeip application via TCP

    std::weak_ptr<routing_host> local_message_handler_;

    const std::string name_;
    const std::string client_host_;
};

} // namespace vsomeip_v3
