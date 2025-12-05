// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_LOCAL_SOCKET_TCP_IMPL_HPP_
#define VSOMEIP_V3_LOCAL_SOCKET_TCP_IMPL_HPP_

#include "local_socket.hpp"
#include "tcp_socket.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <mutex>
namespace vsomeip_v3 {

/**
 * @class local_socket_tcp_impl
 * @brief TCP-based implementation of local_socket for intra-host vsomeip communication.
 *
 * This class wraps a tcp_socket to provide local communication between vsomeip applications
 * on the same host. It implements protocol-specific behavior such as:
 * - TCP socket options (NO_DELAY, LINGER, keep-alive settings)
 * - Quick ACK for local connections (Linux)
 * - Graceful shutdown with optional waiting for pending data
 * - Security credential extraction via TCP endpoints
 *
 * Thread-safety: All public methods are thread-safe via internal mutex.
 *
 * @note For same-host communication, keep-alive is disabled. For cross-container/VM
 *       communication on the same host, keep-alive with aggressive timeouts is enabled.
 */
class local_socket_tcp_impl : public local_socket, public std::enable_shared_from_this<local_socket_tcp_impl> {
public:
    /**
     * @brief Constructs a TCP socket from an existing socket (established connection).
     * @param _io IO context for asynchronous operations.
     * @param _socket Existing TCP socket (ownership transferred).
     * @param _own This application's TCP endpoint (address:port).
     * @param _peer Peer application's TCP endpoint (address:port).
     * @param _role Socket role (SENDER or RECEIVER).
     */
    local_socket_tcp_impl(boost::asio::io_context& _io, std::shared_ptr<tcp_socket> _socket, boost::asio::ip::tcp::endpoint _own,
                          boost::asio::ip::tcp::endpoint _remote, socket_role_e _role);

    /**
     * @brief Constructs a new TCP socket.
     * @param _io IO context for asynchronous operations.
     * @param _own This application's TCP endpoint to bind to.
     * @param _peer Peer application's TCP endpoint to connect to.
     * @param _role Socket role (SENDER or RECEIVER).
     */
    local_socket_tcp_impl(boost::asio::io_context& _io, boost::asio::ip::tcp::endpoint _own, boost::asio::ip::tcp::endpoint _remote,
                          socket_role_e _role);
    ~local_socket_tcp_impl() override;

    void stop(bool _force) override;
    void prepare_connect(configuration const& _configuration, boost::system::error_code& _ec) override;

    void async_connect(connect_handler _handler) override;
    void async_receive(boost::asio::mutable_buffer _buffer, read_handler) override;
    void async_send(std::vector<uint8_t> _data, write_handler) override;

    std::string const& to_string() const override;
    bool update(vsomeip_sec_client_t& _client, configuration const& _configuration) override;
    port_t own_port() const override;

    boost::asio::ip::tcp::endpoint peer_endpoint() const override;

    /**
     * @brief Applies TCP socket options based on configuration and endpoint addresses.
     * @param _configuration Configuration containing timeout and keep-alive settings.
     *
     * Sets:
     * - SO_LINGER(0) for immediate RST on close (avoids TIME_WAIT issues)
     * - SO_REUSEADDR for address reuse
     * - TCP_NODELAY (disable Nagle algorithm)
     * - Keep-alive settings (if peer is on different address)
     * - TCP_USER_TIMEOUT (Linux, if peer is on different address)
     */
    void set_socket_options(configuration const& _configuration);

private:
    // This should have been a unique_ptr, but because of API limitations,
    // it needs to be a shared_ptr, see local_acceptor_tcp_impl::async_accept
    // for a reason.
    std::shared_ptr<tcp_socket> const socket_;
    std::mutex socket_mtx_;

    socket_role_e const role_;
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::endpoint const peer_endpoint_;
    boost::asio::ip::tcp::endpoint const own_endpoint_;
    std::string const name_;
};
} // namespace vsomeip_v3

#endif
