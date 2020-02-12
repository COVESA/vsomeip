// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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

#include <boost/asio/io_service.hpp>

#include <vsomeip/primitive_types.hpp>

#include "endpoint.hpp"
#include "endpoint_host.hpp"

namespace vsomeip_v3 {

class routing_manager_base;
class configuration;
class local_server_endpoint_impl;
class routing_host;

class endpoint_manager_base
        : public std::enable_shared_from_this<endpoint_manager_base>,
          public endpoint_host {
public:
    endpoint_manager_base(routing_manager_base* const _rm,
            boost::asio::io_service& _io,
            const std::shared_ptr<configuration>& _configuration);
    virtual ~endpoint_manager_base() = default;

    std::shared_ptr<endpoint> create_local(client_t _client);
    void remove_local(client_t _client);

    std::shared_ptr<endpoint> find_or_create_local(client_t _client);
    std::shared_ptr<endpoint> find_local(client_t _client);
    std::shared_ptr<endpoint> find_local(service_t _service, instance_t _instance);

    std::unordered_set<client_t> get_connected_clients() const;

    std::shared_ptr<local_server_endpoint_impl> create_local_server(
            const std::shared_ptr<routing_host> &_routing_host);

    // endpoint_host interface
    virtual void on_connect(std::shared_ptr<endpoint> _endpoint);
    virtual void on_disconnect(std::shared_ptr<endpoint> _endpoint);
    virtual void on_error(const byte_t *_data, length_t _length,
                          endpoint* const _receiver,
                          const boost::asio::ip::address &_remote_address,
                          std::uint16_t _remote_port);
    virtual void release_port(uint16_t _port, bool _reliable);
    client_t get_client() const;

    // Statistics
    void log_client_states() const;

protected:
    std::map<client_t, std::shared_ptr<endpoint>> get_local_endpoints() const;

private:
    std::shared_ptr<endpoint> create_local_unlocked(client_t _client);
    std::shared_ptr<endpoint> find_local_unlocked(client_t _client);

protected:
    routing_manager_base* const rm_;
    boost::asio::io_service& io_;
    std::shared_ptr<configuration> configuration_;

private:
    mutable std::mutex local_endpoint_mutex_;
    std::map<client_t, std::shared_ptr<endpoint> > local_endpoints_;
};

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_ENDPOINT_MANAGER_BASE_HPP_
