// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#if defined(__linux__) || defined(__QNX__)
#include "uds_acceptor.hpp"
#include "asio_uds_socket.hpp"
#include <boost/asio/io_context.hpp>

namespace vsomeip_v3 {

class uds_socket;

class asio_uds_acceptor final : public uds_acceptor {
public:
    explicit asio_uds_acceptor(boost::asio::io_context& _io) : acceptor_(_io) { }

private:
    [[nodiscard]] bool is_open() const override { return acceptor_.is_open(); }
    void assign(endpoint::protocol_type _pt, int _fd, boost::system::error_code& _ec) override { acceptor_.assign(_pt, _fd, _ec); }
    void open(endpoint::protocol_type _pt, boost::system::error_code& _ec) override { acceptor_.open(_pt, _ec); }

    void bind(endpoint const& _ep, boost::system::error_code& _ec) override { acceptor_.bind(_ep, _ec); }
    void listen(int _max_listen_connections, boost::system::error_code& _ec) override { acceptor_.listen(_max_listen_connections, _ec); }
    [[nodiscard]] bool unlink(char const* _path) override {
        if (-1 == ::unlink(_path) && errno != ENOENT) {
            return false;
        }
        return true;
    }

    void close(boost::system::error_code& _ec) override { acceptor_.close(_ec); }
    void cancel(boost::system::error_code& _ec) override { acceptor_.cancel(_ec); }

    void set_reuse_address(boost::system::error_code& _ec) override {
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true), _ec);
    }

    void async_accept(uds_socket& _socket, endpoint& _peer_ep, connect_handler _handler) override {
        auto* socket_impl = dynamic_cast<asio_uds_socket*>(&_socket);
        if (!socket_impl) {
            _handler(boost::asio::error::make_error_code(boost::asio::error::invalid_argument));
            return;
        }
        acceptor_.async_accept(socket_impl->socket_, _peer_ep, std::move(_handler));
    }

    boost::asio::local::stream_protocol::acceptor acceptor_;
};
}

#endif
