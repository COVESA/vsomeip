// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if defined(__linux__) || defined(__QNX__)
#ifndef VSOMEIP_V3_LOCAL_SOCKET_UDS_IMPL_HPP_
#define VSOMEIP_V3_LOCAL_SOCKET_UDS_IMPL_HPP_

#include "local_socket.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#include <mutex>

namespace vsomeip_v3 {

/**
 * @class local_socket_uds_impl
 * @brief UDS-based implementation of local_socket for intra-host vsomeip communication.
 *
 * This class wraps a boost::asio::local::stream_protocol::socket to provide local
 * communication between vsomeip applications on the same host using Unix Domain Sockets.
 *
 * UDS-specific behavior:
 * - Credential passing via SO_PEERCRED (UID, GID extraction)
 * - File system-based endpoints (socket paths)
 * - No TCP-specific options needed (inherently local, no network latency)
 *
 * Thread-safety: All public methods are thread-safe via internal mutex.
 *
 * @note Only available on Linux and QNX platforms.
 */
class local_socket_uds_impl : public local_socket {
public:
    using socket_type = boost::asio::local::stream_protocol::socket;
    using endpoint = boost::asio::local::stream_protocol::endpoint;

    /**
     * @brief Constructs a UDS socket from an existing socket.
     * @param _io IO context for asynchronous operations.
     * @param _socket Existing UDS socket (ownership transferred).
     * @param _own_endpoint This application's UDS endpoint (socket path).
     * @param _peer_endpoint Peer application's UDS endpoint (socket path).
     * @param _role Socket role (SENDER or RECEIVER).
     */
    local_socket_uds_impl(boost::asio::io_context& _io, std::unique_ptr<socket_type> _socket, endpoint _own_endpoint,
                          endpoint _peer_endpoint, socket_role_e _role);

    /**
     * @brief Constructs a new UDS socket.
     * @param _io IO context for asynchronous operations.
     * @param _own_endpoint This application's UDS endpoint (socket path).
     * @param _peer_endpoint Peer application's UDS endpoint to connect to.
     * @param _role Socket role (SENDER or RECEIVER).
     */
    local_socket_uds_impl(boost::asio::io_context& _io, endpoint _own_endpoint, endpoint _peer_endpoint, socket_role_e _role);
    ~local_socket_uds_impl() override = default;

    void stop(bool _force) override;
    void prepare_connect(configuration const& _configuration, boost::system::error_code& _ec) override;

    void async_connect(connect_handler _handler) override;
    void async_receive(boost::asio::mutable_buffer _buffer, read_handler) override;
    void async_send(std::vector<uint8_t> _data, write_handler) override;

    std::string const& to_string() const override;
    bool update(vsomeip_sec_client_t& _client, configuration const& _configuration) override;
    port_t own_port() const override;

    boost::asio::ip::tcp::endpoint peer_endpoint() const override;

private:
    std::unique_ptr<socket_type> const socket_;
    std::mutex socket_mtx_;

    boost::asio::io_context& io_context_;
    endpoint const peer_endpoint_;
    std::string const name_;
};
}

#endif
#endif
