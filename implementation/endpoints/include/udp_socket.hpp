// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <boost/asio/ip/udp.hpp>
#include <boost/asio/ip/multicast.hpp>

#include <functional>

namespace vsomeip_v3 {

/**
 * specific abstraction for boost::asio::ip::udp::socket to allow testing without
 * testing boost asio or the OS.
 * For any of the following types or methods the boost::asio documentation should
 * be consulted.
 **/
class udp_socket {
public:
    using rw_handler = std::function<void(boost::system::error_code const&, size_t)>;
    using completion_handler = std::function<void(boost::system::error_code const&)>;

    virtual ~udp_socket() = default;

    [[nodiscard]] virtual bool is_open() const = 0;
    [[nodiscard]] virtual int native_handle() = 0;

    virtual void open(boost::asio::ip::udp::endpoint::protocol_type, boost::system::error_code&) = 0;
    virtual void bind(boost::asio::ip::udp::endpoint const&, boost::system::error_code&) = 0;
    virtual void close(boost::system::error_code&) = 0;
    virtual void shutdown(boost::asio::ip::udp::socket::shutdown_type, boost::system::error_code&) = 0;

    virtual bool native_non_blocking() const = 0;
    virtual void native_non_blocking(bool, boost::system::error_code&) = 0;

    virtual boost::asio::ip::udp::endpoint local_endpoint(boost::system::error_code&) const = 0;

    virtual void set_option(boost::asio::ip::udp::socket::reuse_address, boost::system::error_code&) = 0;
    virtual void set_option(boost::asio::ip::multicast::outbound_interface outbound, boost::system::error_code& ec) = 0;
    virtual void set_option(boost::asio::ip::udp::socket::broadcast broad, boost::system::error_code& ec) = 0;
    virtual void set_option(boost::asio::ip::udp::socket::receive_buffer_size rx_size, boost::system::error_code& ec) = 0;
    virtual void set_option(boost::asio::ip::multicast::join_group join, boost::system::error_code& ec) = 0;
    virtual void set_option(boost::asio::ip::multicast::leave_group leave, boost::system::error_code& ec) = 0;
    virtual void get_option(boost::asio::ip::udp::socket::receive_buffer_size& rx_size, boost::system::error_code& ec) = 0;

    virtual void async_connect(boost::asio::ip::udp::endpoint const&, completion_handler) = 0;
    virtual void async_receive_from(boost::asio::mutable_buffer, boost::asio::ip::udp::endpoint&, rw_handler) = 0;
    virtual void async_send(boost::asio::const_buffer const&, rw_handler) = 0;
    virtual void async_send_to(boost::asio::const_buffer const&, boost::asio::ip::udp::endpoint, rw_handler) = 0;
    virtual void async_wait(boost::asio::ip::udp::socket::wait_type, completion_handler) = 0;
};

}
