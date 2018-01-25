// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_INTERNAL_UDP_SERVICE_IMPL_HPP
#define VSOMEIP_INTERNAL_UDP_SERVICE_IMPL_HPP

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp_ext.hpp>

#include <vsomeip/defines.hpp>

#include "server_endpoint_impl.hpp"
#include <atomic>

namespace vsomeip {

typedef server_endpoint_impl<
            boost::asio::ip::udp_ext
        > udp_server_endpoint_base_impl;

class udp_server_endpoint_impl: public udp_server_endpoint_base_impl {

public:
    udp_server_endpoint_impl(std::shared_ptr<endpoint_host> _host,
                             endpoint_type _local,
                             boost::asio::io_service &_io,
                             configuration::endpoint_queue_limit_t _queue_limit);
    virtual ~udp_server_endpoint_impl();

    void start();
    void stop();

    void receive();

    bool send_to(const std::shared_ptr<endpoint_definition> _target,
            const byte_t *_data, uint32_t _size, bool _flush);
    void send_queued(const queue_iterator_type _queue_iterator);

    void join(const std::string &_address);
    void leave(const std::string &_address);

    void add_default_target(service_t _service,
            const std::string &_address, uint16_t _port);
    void remove_default_target(service_t _service);
    bool get_default_target(service_t _service, endpoint_type &_target) const;

    std::uint16_t get_local_port() const;
    bool is_local() const;

    client_t get_client(std::shared_ptr<endpoint_definition> _endpoint);

    void receive_cbk(boost::system::error_code const &_error,
                     std::size_t _size,
                     boost::asio::ip::address const &_destination);

    void print_status();

private:
    void set_broadcast();
    bool is_joined(const std::string &_address) const;
    bool is_joined(const std::string &_address, bool* _received) const;
    std::string get_remote_information(
            const queue_iterator_type _queue_iterator) const;

private:
    socket_type socket_;
    endpoint_type remote_;

    mutable std::mutex default_targets_mutex_;
    std::map<service_t, endpoint_type> default_targets_;
    mutable std::mutex joined_mutex_;
    std::map<std::string, bool> joined_;
    std::atomic<bool> joined_group_;

    message_buffer_t recv_buffer_;
    std::mutex socket_mutex_;

    const std::uint16_t local_port_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_UDP_SERVICE_IMPL_HPP
