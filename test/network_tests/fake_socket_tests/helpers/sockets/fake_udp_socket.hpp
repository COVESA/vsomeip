// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../../implementation/endpoints/include/udp_socket.hpp"

#include "fake_udp_socket_handle.hpp"

namespace vsomeip_v3::testing {

class fake_udp_socket : public udp_socket {
public:
    explicit fake_udp_socket(std::shared_ptr<fake_udp_socket_handle> _state) : state_(std::move(_state)) { }

    [[nodiscard]] bool is_open() const { return state_->is_open(); }

    [[nodiscard]] virtual int native_handle() override {
        // this function should not be called within a test execution. Otherwise a proper
        // indirection is missing. Because this function is only required to be called when using
        // native system calls, something that should be avoided when using fake sockets
        throw std::runtime_error("native_handle is not allowed to be called within a test. A "
                                 "proper abstraction is missing");
        return -1;
    }

    void open(boost::asio::ip::udp::endpoint::protocol_type _type, boost::system::error_code& _ec) override {
        _ec = boost::system::error_code();
        state_->open(_type);
    }

    void bind(boost::asio::ip::udp::endpoint const& _ep, boost::system::error_code& _ec) override {
        if (!state_->bind(_ep)) {
            _ec = boost::asio::error::access_denied;
            return;
        }
        _ec = boost::system::error_code();
    }

    void close(boost::system::error_code& _ec) override {
        _ec = boost::system::error_code();
        state_->close();
    }

    bool native_non_blocking() const override { return false; }
    void native_non_blocking(bool, boost::system::error_code&) override { }

    virtual boost::asio::ip::udp::endpoint local_endpoint(boost::system::error_code& _ec) const override {
        _ec = boost::system::error_code();
        return state_->local_endpoint();
    }

#if defined(__linux__) || defined(__QNX__)
    virtual void set_option([[maybe_unused]] udp_bind_to_device _opt, boost::system::error_code& _ec) override {
        _ec = boost::system::error_code();
    }
    virtual void set_option([[maybe_unused]] udp_packet_info_ip4 _opt, boost::system::error_code& _ec) override {
        _ec = boost::system::error_code();
    }
    virtual void set_option([[maybe_unused]] udp_packet_info_ip6 _opt, boost::system::error_code& _ec) override {
        _ec = boost::system::error_code();
    }
    virtual void set_option([[maybe_unused]] udp_send_timeout _opt, boost::system::error_code& _ec) override {
        _ec = boost::system::error_code();
    }
    virtual void set_option([[maybe_unused]] udp_receive_timeout _opt, boost::system::error_code& _ec) override {
        _ec = boost::system::error_code();
    }

    virtual bool can_read_fd_flags() override { return false; }
#endif
#ifdef __linux__
    virtual void set_option([[maybe_unused]] udp_receive_buffer_force _opt, boost::system::error_code& _ec) override {
        _ec = boost::system::error_code();
    }
#endif

    void set_option(boost::asio::ip::udp::socket::reuse_address, boost::system::error_code&) override { }

    void set_option(boost::asio::ip::multicast::outbound_interface, boost::system::error_code&) override { }

    void set_option(boost::asio::ip::udp::socket::broadcast, boost::system::error_code&) override { }

    void set_option(boost::asio::ip::udp::socket::receive_buffer_size, boost::system::error_code&) override { }

    void set_option(boost::asio::ip::multicast::join_group _join, boost::system::error_code& _ec) override {
        state_->set_option(_join, _ec);
    }

    void set_option(boost::asio::ip::multicast::leave_group _leave, boost::system::error_code& _ec) override {
        state_->set_option(_leave, _ec);
    }

    void get_option(boost::asio::ip::udp::socket::receive_buffer_size&, boost::system::error_code&) override { }

    void async_connect(boost::asio::ip::udp::endpoint const& _ep, connect_handler _handler) override {
        state_->async_connect(_ep, std::move(_handler));
    }

    void async_receive_from(boost::asio::mutable_buffer _buffer, boost::asio::ip::udp::endpoint& _endpoint, rw_handler _handler) override {
        state_->async_receive_from(std::move(_buffer), _endpoint, std::move(_handler));
    }

    void async_send(boost::asio::const_buffer const& _buffer, rw_handler _handler) override {
        state_->async_send(_buffer, std::move(_handler));
    }

    void async_send_to(boost::asio::const_buffer const& _buffer, boost::asio::ip::udp::endpoint _endpoint, rw_handler _handler) override {
        state_->async_send_to(_buffer, _endpoint, std::move(_handler));
    }

    /*
     * Stash an error code to be processed during the receive operation on this socket once.
     */
    void stash_ec(boost::system::error_code _ec) { state_->stash_ec(_ec); }

private:
    std::shared_ptr<fake_udp_socket_handle> state_;
};

}
