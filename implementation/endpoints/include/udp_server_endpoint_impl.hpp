// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_UDP_SERVER_ENDPOINT_IMPL_HPP_
#define VSOMEIP_V3_UDP_SERVER_ENDPOINT_IMPL_HPP_

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp_ext.hpp>

#include <atomic>

#include <vsomeip/defines.hpp>

#include "server_endpoint_impl.hpp"
#include "tp_reassembler.hpp"

namespace vsomeip_v3 {

typedef server_endpoint_impl<
            boost::asio::ip::udp_ext
        > udp_server_endpoint_base_impl;

class udp_server_endpoint_impl: public udp_server_endpoint_base_impl {

public:
    udp_server_endpoint_impl(const std::shared_ptr<endpoint_host>& _endpoint_host,
                             const std::shared_ptr<routing_host>& _routing_host,
                             const endpoint_type& _local,
                             boost::asio::io_service &_io,
                             const std::shared_ptr<configuration>& _configuration);
    virtual ~udp_server_endpoint_impl();

    void start();
    void stop();

    void receive();

    bool send_to(const std::shared_ptr<endpoint_definition> _target,
            const byte_t *_data, uint32_t _size);
    bool send_error(const std::shared_ptr<endpoint_definition> _target,
                const byte_t *_data, uint32_t _size);
    void send_queued(const queue_iterator_type _queue_iterator);
    void get_configured_times_from_endpoint(
            service_t _service, method_t _method,
            std::chrono::nanoseconds *_debouncing,
            std::chrono::nanoseconds *_maximum_retention) const;

    VSOMEIP_EXPORT void join(const std::string &_address);
    VSOMEIP_EXPORT void join_unlocked(const std::string &_address);
    void leave(const std::string &_address);

    void add_default_target(service_t _service,
            const std::string &_address, uint16_t _port);
    void remove_default_target(service_t _service);
    bool get_default_target(service_t _service, endpoint_type &_target) const;

    std::uint16_t get_local_port() const;
    bool is_local() const;

    void print_status();
    bool is_reliable() const;

private:
    void leave_unlocked(const std::string &_address);
    void set_broadcast();
    void receive_unicast();
    void receive_multicast(uint8_t _id);
    bool is_joined(const std::string &_address) const;
    bool is_joined(const std::string &_address, bool* _received) const;
    std::string get_remote_information(
            const queue_iterator_type _queue_iterator) const;
    std::string get_remote_information(const endpoint_type& _remote) const;

    const std::string get_address_port_local() const;
    bool tp_segmentation_enabled(service_t _service, method_t _method) const;

    void on_unicast_received(boost::system::error_code const &_error,
            std::size_t _bytes,
            boost::asio::ip::address const &_destination);

    void on_multicast_received(boost::system::error_code const &_error,
            std::size_t _bytes,
            boost::asio::ip::address const &_destination,
            uint8_t _multicast_id);

    void on_message_received(boost::system::error_code const &_error,
                     std::size_t _bytes,
                     boost::asio::ip::address const &_destination,
                     endpoint_type const &_remote,
                     message_buffer_t const &_buffer);

private:
    socket_type unicast_socket_;
    endpoint_type unicast_remote_;
    message_buffer_t unicast_recv_buffer_;
    mutable std::mutex unicast_mutex_;

    std::unique_ptr<socket_type> multicast_socket_;
    std::unique_ptr<endpoint_type> multicast_local_;
    endpoint_type multicast_remote_;
    message_buffer_t multicast_recv_buffer_;
    mutable std::mutex multicast_mutex_;
    uint8_t multicast_id_;
    std::map<std::string, bool> joined_;
    std::atomic<bool> joined_group_;

    mutable std::mutex default_targets_mutex_;
    std::map<service_t, endpoint_type> default_targets_;

    const std::uint16_t local_port_;

    std::shared_ptr<tp::tp_reassembler> tp_reassembler_;
    boost::asio::steady_timer tp_cleanup_timer_;
};

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_UDP_SERVER_ENDPOINT_IMPL_HPP_
