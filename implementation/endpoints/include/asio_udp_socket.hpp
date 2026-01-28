// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "udp_socket.hpp"

#include <boost/asio/ip/udp.hpp>

namespace vsomeip_v3 {

class asio_udp_socket final : public udp_socket {
public:
    asio_udp_socket(boost::asio::io_context& _io) : socket_(_io) { }

private:
    [[nodiscard]] bool is_open() const override { return socket_.is_open(); }
    [[nodiscard]] int native_handle() override { return socket_.native_handle(); }

    void open(boost::asio::ip::udp::endpoint::protocol_type pt, boost::system::error_code& ec) override { socket_.open(pt, ec); }
    void bind(boost::asio::ip::udp::endpoint const& ep, boost::system::error_code& ec) override { socket_.bind(ep, ec); }

    void close(boost::system::error_code& ec) override { socket_.close(ec); }
    void shutdown(boost::asio::ip::udp::socket::shutdown_type st, boost::system::error_code& ec) override { socket_.shutdown(st, ec); }

    bool native_non_blocking() const override { return socket_.native_non_blocking(); }
    void native_non_blocking(bool mode, boost::system::error_code& ec) override { socket_.native_non_blocking(mode, ec); }

    void set_option(boost::asio::ip::udp::socket::reuse_address ra, boost::system::error_code& ec) override { socket_.set_option(ra, ec); }
    void set_option(boost::asio::ip::multicast::outbound_interface outbound, boost::system::error_code& ec) override {
        socket_.set_option(outbound, ec);
    }
    void set_option(boost::asio::ip::udp::socket::broadcast broad, boost::system::error_code& ec) override {
        socket_.set_option(broad, ec);
    }
    void set_option(boost::asio::ip::udp::socket::receive_buffer_size rx_size, boost::system::error_code& ec) override {
        socket_.set_option(rx_size, ec);
    }
    void set_option(boost::asio::ip::multicast::join_group join, boost::system::error_code& ec) override { socket_.set_option(join, ec); }
    void set_option(boost::asio::ip::multicast::leave_group leave, boost::system::error_code& ec) override {
        socket_.set_option(leave, ec);
    }
    void get_option(boost::asio::ip::udp::socket::receive_buffer_size& rx_size, boost::system::error_code& ec) override {
        socket_.get_option(rx_size, ec);
    }

    boost::asio::ip::udp::endpoint local_endpoint(boost::system::error_code& ec) const override { return socket_.local_endpoint(ec); }

    void async_connect(boost::asio::ip::udp::endpoint const& remote, completion_handler handler) override {
        socket_.async_connect(remote, std::move(handler));
    }
    void async_receive_from(boost::asio::mutable_buffer b, boost::asio::ip::udp::endpoint& remote, rw_handler handler) override {
        socket_.async_receive_from(b, remote, std::move(handler));
    }
    void async_send(boost::asio::const_buffer const& b, rw_handler handler) override { socket_.async_send(b, std::move(handler)); }
    void async_send_to(boost::asio::const_buffer const& b, boost::asio::ip::udp::endpoint destination, rw_handler handler) override {
        socket_.async_send_to(b, destination, std::move(handler));
    }
    void async_wait(boost::asio::ip::udp::socket::wait_type wait, completion_handler handler) override {
        socket_.async_wait(wait, std::move(handler));
    }

    boost::asio::ip::udp::socket socket_;
};

}
