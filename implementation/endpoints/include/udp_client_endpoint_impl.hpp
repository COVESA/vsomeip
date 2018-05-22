// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_INTERNAL_UDP_CLIENT_IMPL_HPP
#define VSOMEIP_INTERNAL_UDP_CLIENT_IMPL_HPP

#include <memory>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>

#include <vsomeip/defines.hpp>

#include "client_endpoint_impl.hpp"

namespace vsomeip {

class endpoint_adapter;

typedef client_endpoint_impl<
            boost::asio::ip::udp
        > udp_client_endpoint_base_impl;

class udp_client_endpoint_impl: virtual public udp_client_endpoint_base_impl {

public:
    udp_client_endpoint_impl(std::shared_ptr<endpoint_host> _host,
                             endpoint_type _local,
                             endpoint_type _remote,
                             boost::asio::io_service &_io,
                             configuration::endpoint_queue_limit_t _queue_limit);
    virtual ~udp_client_endpoint_impl();

    void start();
    void restart(bool _force);

    void receive_cbk(boost::system::error_code const &_error,
                     std::size_t _bytes);

    bool get_remote_address(boost::asio::ip::address &_address) const;
    std::uint16_t get_remote_port() const;
    bool is_local() const;
    void print_status();
private:
    void send_queued();
    void connect();
    void receive();
    void set_local_port();
    const std::string get_address_port_remote() const;
    const std::string get_address_port_local() const;
    std::string get_remote_information() const;

    message_buffer_t recv_buffer_;

    const boost::asio::ip::address remote_address_;
    const std::uint16_t remote_port_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_UDP_CLIENT_IMPL_HPP
