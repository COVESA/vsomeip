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
#include <vsomeip/primitive_types.hpp>
#include <vsomeip/enumeration_types.hpp>
#include "local_endpoint_manager_host.hpp"

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
    /**
     * Re-enables the creation of endpoints as well as accepting provider endpoints
     * (from re-created local_server)
     **/
    void start();

    /**
     * Stops accepting or creating endpoints (except the sender for locking reasons),
     * clears all pending provider endpoints,
     * starts flushing all managed endpoints
     **/
    void stop();

    /**
     * Blocks the current thread for max _timeout or until all managed endpoints have been removed.
     * Note:
     * It is mandatory to have called ::stop() before and do not call ::start() inbetween to avoid
     * expiring the _timeout.
     **/
    [[nodiscard]] bool await_stopped(std::chrono::milliseconds _timeout);

    std::shared_ptr<local_server> create_local_server(transport_protocol_e _transport_protocol);

    std::shared_ptr<local_endpoint> create_routing_client();
    std::shared_ptr<local_endpoint> find_or_create_local_client(client_t _client);
    std::shared_ptr<local_endpoint> find_local_client(client_t _client);

    // server endpoint API (endpoints towards clients of this application)
    std::shared_ptr<local_endpoint> find_local_server_endpoint(client_t _client) const;

    void remove_provider_endpoint(client_t _client, bool _remove_due_to_error);
    void remove_consumer_endpoint(client_t _client, bool _remove_due_to_error);
    void clear_provider_endpoints();
    void clear_consumer_endpoints();
    void stop_all_endpoints();

    // Statistics
    void log_client_states() const;
    void print_status() const;

    uint32_t provider_connection_token(client_t _client) const;

private:
    client_t get_client_id() const;
    std::string get_client_env() const;

    void add_local_server_endpoint(std::shared_ptr<local_endpoint> _connection, uint32_t _token);
    void add_local_server_endpoint_unlocked(client_t _client, const std::shared_ptr<local_endpoint>& _connection);

    std::shared_ptr<local_endpoint> create_local_client_endpoint(client_t _client, client_t _own_id,
                                                                 boost::asio::ip::address const& _remote_address, port_t _remote_port,
                                                                 bool _is_guest);
    std::shared_ptr<local_endpoint> create_local_client_unlocked(client_t _client);
    std::shared_ptr<local_endpoint> find_local_client_unlocked(client_t _client);

    void remove_local_client_endpoint_unlocked(client_t _client, bool _remove_due_to_error);
    void remove_local_server_endpoint_unlocked(client_t _client, bool _remove_due_to_error);

    bool get_local_server_port(port_t& _port, const std::set<port_t>& _used_ports) const;

    // local server creation helpers
    std::shared_ptr<local_acceptor> create_uds_local_acceptor(const std::string& _path, client_t _client);
    std::shared_ptr<local_acceptor> create_tcp_local_acceptor(client_t _client);

    uint32_t bump_provider_token(client_t _client);

private:
    local_endpoint_manager_host& host_;
    boost::asio::io_context& io_;
    std::shared_ptr<configuration> configuration_;

    bool const is_local_routing_;
    bool const is_uds_preferred_; // UDS is transport type for local routing, but only if preferred in configuration
    port_t local_port_; // local (client) port when connecting to other
                        // vsomeip application via TCP

    bool is_started_{false};
    uint32_t lc_token_{0};

    mutable std::mutex mtx_;
    std::condition_variable cv_; // required for stopping
    std::weak_ptr<routing_host> local_message_handler_;
    std::map<client_t, std::shared_ptr<local_endpoint>> local_client_endpoints_;
    std::map<client_t, std::shared_ptr<local_endpoint>> local_server_endpoints_;
    std::map<client_t, uint32_t> provider_tokens_;

    const std::string name_;
    const std::string client_host_;

    /**
     * It can happen that a new connection has to be accepted, when a former
     * endpoint was not cleaned-up. In this situation it is important to first
     * clean-up all client related state, before accepting a new connection.
     * (Because one can neither tell whether it is really the same client, nor,
     * if one would knew, whether all handlers are still installed)
     **/
    std::unordered_map<client_t, std::shared_ptr<local_endpoint>> pending_server_endpoints_;
};

} // namespace vsomeip_v3
