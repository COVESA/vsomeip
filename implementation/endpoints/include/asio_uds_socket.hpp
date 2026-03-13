// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#if defined(__linux__) || defined(__QNX__)
#include "uds_socket.hpp"

#include <sys/socket.h>
#include <sys/un.h>

namespace vsomeip_v3 {

class asio_uds_socket final : public uds_socket {
public:
    explicit asio_uds_socket(boost::asio::io_context& _io) : socket_(_io) { }

private:
    [[nodiscard]] bool is_open() const override { return socket_.is_open(); }
    void open(endpoint::protocol_type _pt, boost::system::error_code& _ec) override { socket_.open(_pt, _ec); }

    void close(boost::system::error_code& _ec) override { socket_.close(_ec); }
    void cancel(boost::system::error_code& _ec) override { socket_.cancel(_ec); }

    [[nodiscard]] bool get_peer_credentials(vsomeip_sec_client_t& _client) override {
        int handle = socket_.native_handle();
        ucred out;
        if (socklen_t len = sizeof(ucred); -1 == ::getsockopt(handle, SOL_SOCKET, SO_PEERCRED, &out, &len)) {
            return false;
        }
        _client.user = out.uid;
        _client.group = out.gid;
        _client.port = VSOMEIP_SEC_PORT_UNUSED;
        return true;
    }
    void set_reuse_address(boost::system::error_code& _ec) { socket_.set_option(boost::asio::socket_base::reuse_address(true), _ec); }
    void async_connect(endpoint const& _ep, connect_handler _handler) override { socket_.async_connect(_ep, std::move(_handler)); }
    void async_receive(boost::asio::mutable_buffer _buffer, rw_handler _handler) override {
        socket_.async_receive(_buffer, std::move(_handler));
    }
    void async_write(boost::asio::const_buffer const& _buffer, rw_handler _handler) {
        boost::asio::async_write(socket_, _buffer, std::move(_handler));
    }
    // needs to access the socket member to create a meaningful new connection
    friend class asio_uds_acceptor;
    boost::asio::local::stream_protocol::socket socket_;
};
}

#endif
