// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_ENDPOINT_MANAGER_BASE_HPP_
#define VSOMEIP_V3_ENDPOINT_MANAGER_BASE_HPP_

#include <mutex>
#include <map>
#include <set>
#include <unordered_set>
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

    std::shared_ptr<local_endpoint> create_local(client_t _client);
    void remove_local(client_t _client, bool _remove_due_to_error);

    std::shared_ptr<local_endpoint> find_or_create_local(client_t _client);
    std::shared_ptr<local_endpoint> find_local(client_t _client);
    std::shared_ptr<local_endpoint> find_local(service_t _service, instance_t _instance);

    void flush_local_endpoint_queues() const;

    std::unordered_set<client_t> get_connected_clients() const;

    std::shared_ptr<local_server> create_local_server(const std::shared_ptr<routing_host>& _routing_host);

    // Statistics
    void log_client_states() const;
    void print_status() const;

private:
    client_t get_client_id() const;
    std::string get_client_env() const;

    std::shared_ptr<local_endpoint> create_local_unlocked(client_t _client);
    std::shared_ptr<local_endpoint> find_local_unlocked(client_t _client);

    bool get_local_server_port(port_t& _port, const std::set<port_t>& _used_ports) const;

protected:
    routing_manager_base* const rm_;
    boost::asio::io_context& io_;
    std::shared_ptr<configuration> configuration_;

    bool is_local_routing_;
    port_t local_port_; // local (client) port when connecting to other
                        // vsomeip application via TCP

private:
    mutable std::mutex local_endpoint_mutex_;
    std::map<client_t, std::shared_ptr<local_endpoint>> local_endpoints_;
};

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_ENDPOINT_MANAGER_BASE_HPP_
