// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <vsomeip/vsomeip_sec.h>

#include <boost/asio/local/stream_protocol.hpp>
#include <functional>

namespace vsomeip_v3 {

class uds_socket {
public:
    using rw_handler = std::function<void(boost::system::error_code const&, size_t)>;
    using connect_handler = std::function<void(boost::system::error_code const&)>;
    using endpoint = boost::asio::local::stream_protocol::endpoint;

    virtual ~uds_socket() = default;

    [[nodiscard]] virtual bool is_open() const = 0;
    virtual void open(endpoint::protocol_type _pt, boost::system::error_code& _ec) = 0;

    virtual void close(boost::system::error_code& _ec) = 0;
    virtual void cancel(boost::system::error_code& _ec) = 0;

    [[nodiscard]] virtual bool get_peer_credentials(vsomeip_sec_client_t& _client) = 0;
    virtual void set_reuse_address(boost::system::error_code& _ec) = 0;

    virtual void async_connect(endpoint const& _ep, connect_handler _handler) = 0;
    virtual void async_receive(boost::asio::mutable_buffer _buffer, rw_handler _handler) = 0;
    virtual void async_write(boost::asio::const_buffer const& _buffer, rw_handler _handler) = 0;
};

}
