// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <condition_variable>
#include <queue>
#include <thread>
#include <unordered_map>

#include "../include/boardnet_endpoint_host.hpp"
#include "../include/endpoint_manager_base.hpp"
#include "../include/auxiliary_context.hpp"
#include "../include/endpoint_definition.hpp"

namespace vsomeip_v3 {

class routing_manager_impl;
class local_server;
class local_acceptor;
class boardnet_endpoint;

class endpoint_manager_impl : public boardnet_endpoint_host, public std::enable_shared_from_this<endpoint_manager_impl> {
public:
    endpoint_manager_impl(routing_manager_impl* const _rm, boost::asio::io_context& _io,
                          const std::shared_ptr<configuration>& _configuration);
    ~endpoint_manager_impl();

    std::shared_ptr<boardnet_endpoint> find_or_create_remote_client(service_t _service, instance_t _instance, bool _reliable);

    void find_or_create_remote_client(service_t _service, instance_t _instance);
    void is_remote_service_known(service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor,
                                 const boost::asio::ip::address& _reliable_address, uint16_t _reliable_port, bool* _reliable_known,
                                 const boost::asio::ip::address& _unreliable_address, uint16_t _unreliable_port,
                                 bool* _unreliable_known) const;
    void add_remote_service_info(service_t _service, instance_t _instance, const std::shared_ptr<endpoint_definition>& _ep_definition);
    void add_remote_service_info(service_t _service, instance_t _instance,
                                 const std::shared_ptr<endpoint_definition>& _ep_definition_reliable,
                                 const std::shared_ptr<endpoint_definition>& _ep_definition_unreliable);
    void clear_remote_service_info(service_t _service, instance_t _instance, bool _reliable);

    std::shared_ptr<boardnet_endpoint> create_server_endpoint(uint16_t _port, bool _reliable, bool _start);

    std::shared_ptr<boardnet_endpoint> find_server_endpoint(uint16_t _port, bool _reliable) const;

    std::shared_ptr<boardnet_endpoint> find_or_create_server_endpoint(uint16_t _port, bool _reliable, bool _start, service_t _service,
                                                                      instance_t _instance, bool& _is_found, bool _is_multicast = false);
    bool remove_server_endpoint(uint16_t _port, bool _reliable);

    void clear_client_endpoints(service_t _service, instance_t _instance, bool _reliable);
    void find_or_create_multicast_endpoint(service_t _service, instance_t _instance, const boost::asio::ip::address& _sender,
                                           const boost::asio::ip::address& _address, uint16_t _port);
    void clear_multicast_endpoints(service_t _service, instance_t _instance);

    bool supports_selective(service_t _service, instance_t _instance) const;

    void print_status() const;

    bool create_routing_root(std::shared_ptr<local_server>& _root, const transport_protocol_e& _type, bool& _is_socket_activated,
                             const std::shared_ptr<routing_host>& _host);

    instance_t find_instance(service_t _service, boardnet_endpoint* const _endpoint) const;
    instance_t find_instance_multicast(service_t _service, const boost::asio::ip::address& _sender) const;

    bool remove_instance(service_t _service, boardnet_endpoint* const _endpoint);
    bool remove_instance_multicast(service_t _service, instance_t _instance);

    // boardnet_endpoint_host interface
    void on_connect(std::shared_ptr<boardnet_endpoint> _endpoint);
    void on_disconnect(std::shared_ptr<boardnet_endpoint> _endpoint);
    bool on_bind_error(std::shared_ptr<boardnet_endpoint> _endpoint, const boost::asio::ip::address& _remote_address, uint16_t _remote_port,
                       uint16_t& _local_port);
    void on_error(const byte_t* _data, length_t _length, boardnet_endpoint* const _receiver,
                  const boost::asio::ip::address& _remote_address, std::uint16_t _remote_port);

    void get_used_client_ports(const boost::asio::ip::address& _remote_address, port_t _remote_port,
                               std::map<bool, std::set<port_t>>& _used_ports);
    void request_used_client_port(const boost::asio::ip::address& _remote_address, port_t _remote_port, bool _reliable, port_t _local_port);
    void release_used_client_port(const boost::asio::ip::address& _remote_address, port_t _remote_port, bool _reliable, port_t _local_port);

    // Statistics
    void log_client_states() const;
    void log_server_states() const;

    // add join/leave options
    void add_multicast_option(const multicast_option_t& _option);
    std::unordered_set<client_t> get_connected_clients() const;
    void suspend();
    void resume();
    void start();
    void stop();

    std::shared_ptr<local_endpoint> find_routing_endpoint(client_t _client) const;
    void remove_routing_endpoint(client_t _client);
    void clear_routing_endpoints();
    void flush_routing_endpoint_queues();

    /**
     * drops all routing_endpoints having a peer address equal to @param _address
     **/
    void drop_from(const boost::asio::ip::address& _address);

    bool get_guest(client_t _client, boost::asio::ip::address& _address, port_t& _port) const;

private:
    std::shared_ptr<boardnet_endpoint> find_remote_client(service_t _service, instance_t _instance, bool _reliable);
    std::shared_ptr<boardnet_endpoint> create_remote_client(service_t _service, instance_t _instance, bool _reliable);
    std::shared_ptr<boardnet_endpoint> create_client_endpoint(const boost::asio::ip::address& _address, uint16_t _local_port,
                                                              uint16_t _remote_port, bool _reliable);

    // routing root creation helpers
    bool create_local_uds_acceptor(std::shared_ptr<local_acceptor>& _uds_acceptor, const std::string& _endpoint_path,
                                   bool& _is_socket_activated);
    // TCP substitute for UDS on platforms that lack UDS support (non-Linux/QNX).
    bool create_fallback_tcp_acceptor(std::shared_ptr<local_acceptor>& _tcp_acceptor) const;
    // TCP routing root acceptor bound to the configured host address+port; used by guests.
    bool create_local_tcp_acceptor(std::shared_ptr<local_acceptor>& _tcp_acceptor) const;
    void setup_root_server(std::shared_ptr<local_server>& _root, std::shared_ptr<local_acceptor> _acceptor,
                           const std::shared_ptr<routing_host>& _host);

    // process join/leave options
    void process_multicast_options();

    bool is_used_endpoint(boardnet_endpoint* const _endpoint) const;

    client_t get_client() const;
    std::string get_client_host() const;

    void add_local_routing_endpoint(std::shared_ptr<local_endpoint> _ep);
    void add_local_routing_endpoint_unlocked(client_t _client, const std::shared_ptr<local_endpoint>& _ep);

private:
    boost::asio::io_context& io_;
    std::shared_ptr<configuration> configuration_;
    routing_manager_impl* const router_;

    bool const is_local_routing_;
    bool const is_uds_preferred_;

    mutable std::recursive_mutex endpoint_mutex_;
    // Client endpoints for remote services
    std::map<service_t, std::map<instance_t, std::map<bool, std::shared_ptr<endpoint_definition>>>> remote_service_info_;

    typedef std::map<service_t, std::map<instance_t, std::map<bool, std::shared_ptr<boardnet_endpoint>>>> remote_services_t;
    remote_services_t remote_services_;

    using client_endpoints_t = std::map<boost::asio::ip::address,
                                        std::map<uint16_t, std::map<bool, std::map<partition_id_t, std::shared_ptr<boardnet_endpoint>>>>>;
    client_endpoints_t client_endpoints_;

    std::map<service_t, std::map<boardnet_endpoint*, instance_t>> service_instances_;
    std::map<service_t, std::map<boost::asio::ip::address, instance_t>> service_instances_multicast_;

    std::map<boost::asio::ip::address, std::map<port_t, std::map<bool, std::set<port_t>>>> used_client_ports_;

    // Server endpoints for local services
    using server_endpoints_t = std::map<uint16_t, std::map<bool, std::shared_ptr<boardnet_endpoint>>>;
    server_endpoints_t server_endpoints_;

    // Multicast endpoint info (notifications)
    std::map<service_t, std::map<instance_t, std::shared_ptr<endpoint_definition>>> multicast_info_;
    auxiliary_context auxiliary_context_;

    // Socket option processing (join, leave)
    std::mutex options_mutex_;
    bool is_processing_options_;
    std::condition_variable options_condition_;
    std::queue<multicast_option_t> options_queue_;
    std::thread options_thread_;

    mutable std::mutex routing_endpoint_mtx_;
    std::unordered_map<client_t, std::shared_ptr<local_endpoint>> routing_endpoints_;
    std::unordered_map<client_t, std::shared_ptr<local_endpoint>> pending_routing_endpoints_;
};

} // namespace vsomeip_v3
