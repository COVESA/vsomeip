// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <boost/asio/local/stream_protocol.hpp>
#include <functional>

namespace vsomeip_v3 {

class uds_socket;

class uds_acceptor {
public:
    using connect_handler = std::function<void(boost::system::error_code const&)>;
    using endpoint = boost::asio::local::stream_protocol::endpoint;

    virtual ~uds_acceptor() = default;

    [[nodiscard]] virtual bool is_open() const = 0;
    virtual void assign(endpoint::protocol_type _pt, int _fd, boost::system::error_code& _ec) = 0;
    virtual void open(endpoint::protocol_type _pt, boost::system::error_code& _ec) = 0;

    virtual void bind(endpoint const& _ep, boost::system::error_code& _ec) = 0;
    virtual void listen(int _max_listen_connections, boost::system::error_code& _ec) = 0;
    [[nodiscard]] virtual bool unlink(char const* _path) = 0;

    virtual void close(boost::system::error_code& _ec) = 0;
    virtual void cancel(boost::system::error_code& _ec) = 0;

    virtual void set_reuse_address(boost::system::error_code& _ec) = 0;

    virtual void async_accept(uds_socket& _socket, endpoint& _peer_ep, connect_handler _handler) = 0;
};

}
