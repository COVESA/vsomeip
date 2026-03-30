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

#include <boost/asio/io_context.hpp>
#include <vsomeip/primitive_types.hpp>

namespace vsomeip_v3 {

class routing_manager_base;
class configuration;
class routing_host;
class local_server;
class local_endpoint;

class endpoint_manager_base : public std::enable_shared_from_this<endpoint_manager_base> {
public:
    endpoint_manager_base(routing_manager_base* const _rm, boost::asio::io_context& _io,
                          const std::shared_ptr<configuration>& _configuration);
    virtual ~endpoint_manager_base() = default;

    void init(std::shared_ptr<routing_host> const& _local_message_handler);

    std::shared_ptr<local_server> create_local_server();

    std::shared_ptr<local_endpoint> create_local_client(client_t _client);
    std::shared_ptr<local_endpoint> find_or_create_local_client(client_t _client);
    std::shared_ptr<local_endpoint> find_local_client(client_t _client);

    // server endpoint API (endpoints towards clients of this application)
    std::shared_ptr<local_endpoint> find_local_server_endpoint(client_t _client) const;

    void remove_local(client_t _client, bool _remove_due_to_error);
    void clear_local_endpoints(bool _remove_due_to_error = false);
    void stop_all_endpoints();

    void flush_local_endpoint_queues() const;

    // Statistics
    void log_client_states() const;
    void print_status() const;

private:
    client_t get_client_id() const;
    std::string get_client_env() const;

    void add_local_server_endpoint(std::shared_ptr<local_endpoint> _connection);
    void add_local_server_endpoint_unlocked(client_t _client, const std::shared_ptr<local_endpoint>& _connection);

    std::shared_ptr<local_endpoint> create_local_client_unlocked(client_t _client);
    std::shared_ptr<local_endpoint> find_local_client_unlocked(client_t _client);

    void remove_local_client_endpoint_unlocked(client_t _client, bool _remove_due_to_error);
    void remove_local_server_endpoint_unlocked(client_t _client, bool _remove_due_to_error);

    bool get_local_server_port(port_t& _port, const std::set<port_t>& _used_ports) const;

protected:
    routing_manager_base* const rm_;
    boost::asio::io_context& io_;
    std::shared_ptr<configuration> configuration_;

    bool const is_local_routing_;
    port_t local_port_; // local (client) port when connecting to other
                        // vsomeip application via TCP

private:
    mutable std::mutex mtx_;
    std::weak_ptr<routing_host> local_message_handler_;
    std::map<client_t, std::shared_ptr<local_endpoint>> local_client_endpoints_;
    std::map<client_t, std::shared_ptr<local_endpoint>> local_server_endpoints_;

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
