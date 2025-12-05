// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_LOCAL_ACCEPTOR_TCP_IMPL_HPP_
#define VSOMEIP_V3_LOCAL_ACCEPTOR_TCP_IMPL_HPP_

#include "local_acceptor.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <mutex>

namespace vsomeip_v3 {

class tcp_acceptor;
class tcp_socket;
class configuration;

/**
 * @class local_acceptor_tcp_impl
 * @brief TCP-based acceptor for intra-host vsomeip connections.
 *
 * This class manages a TCP acceptor that:
 * - Searches for available ports in a configured range
 * - Binds to a local TCP endpoint
 * - Accepts incoming TCP connections
 * - Creates local_socket_tcp_impl instances for accepted connections
 *
 * Protocol-specific features:
 * - Port range search for finding available ports
 * - SO_REUSEADDR
 * - IP_FREEBIND for binding to non-local addresses (Linux)
 * - Automatic socket option configuration on accepted sockets
 *
 * Thread-safety: All public methods are thread-safe via internal mutex.
 *
 * @note The init() method can be called multiple times to retry binding
 *       to different ports during port search.
 */
class local_acceptor_tcp_impl : public local_acceptor, public std::enable_shared_from_this<local_acceptor_tcp_impl> {
public:
    /**
     * @brief Constructs a TCP acceptor.
     * @param _io IO context for asynchronous operations.
     * @param _configuration Configuration containing socket options and port ranges.
     */
    local_acceptor_tcp_impl(boost::asio::io_context& _io, std::shared_ptr<configuration> _configuration);
    virtual ~local_acceptor_tcp_impl() override;

    /**
     * @brief Initializes the acceptor by opening, binding, and listening on the endpoint.
     * @param _local_ep Local TCP endpoint (address:port) to bind to.
     * @param _ec Error code set if initialization fails.
     *
     * This method can be called multiple times with different ports during port search.
     * If binding fails (port in use), the socket remains open for the next attempt.
     * If listen fails, the socket is closed and must be re-initialized.
     *
     * Errors:
     * - bind failure: Expected during port search, socket stays open
     * - listen failure: Unexpected, socket is closed
     */
    void init(boost::asio::ip::tcp::endpoint _local_ep, boost::system::error_code& _ec);
    virtual void close(boost::system::error_code& _ec) override;
    virtual void cancel(boost::system::error_code& _ec) override;

    virtual void async_accept(connection_handler _handler) override;

    virtual port_t get_local_port() override;

private:
    void accept_cbk(boost::system::error_code const& _ec, connection_handler _handler, std::shared_ptr<tcp_socket> _socket);

    boost::asio::io_context& io_;
    std::mutex mtx_;
    boost::asio::ip::tcp::endpoint local_ep_;
    std::unique_ptr<tcp_acceptor> const acceptor_;
    std::shared_ptr<configuration> const configuration_;
};

}

#endif
