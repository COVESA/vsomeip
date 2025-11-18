// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if defined(__linux__) || defined(__QNX__)
#ifndef VSOMEIP_V3_LOCAL_ACCEPTOR_UDS_IMPL_HPP_
#define VSOMEIP_V3_LOCAL_ACCEPTOR_UDS_IMPL_HPP_

#include "local_acceptor.hpp"
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/system/error_code.hpp>
#include <mutex>

namespace vsomeip_v3 {

class configuration;

/**
 * @class local_acceptor_uds_impl
 * @brief UDS-based acceptor for intra-host vsomeip connections.
 *
 * This class manages a Unix Domain Socket acceptor that:
 * - Binds to a file system path
 * - Accepts incoming UDS connections
 * - Creates local_socket_uds_impl instances for accepted connections
 *
 * UDS-specific features:
 * - File system cleanup (unlink) before binding
 * - Permissions setting on the socket file (chmod)
 * - Optional native file descriptor assignment (for systemd socket activation)
 * - SO_PEERCRED support for credential passing
 *
 * Thread-safety: All public methods are thread-safe via internal mutex.
 *
 * @note Only available on Linux and QNX platforms.
 */
class local_acceptor_uds_impl : public local_acceptor, public std::enable_shared_from_this<local_acceptor_uds_impl> {
public:
    using endpoint = boost::asio::local::stream_protocol::endpoint;
    using socket = boost::asio::local::stream_protocol::socket;
    using acceptor = boost::asio::local::stream_protocol::acceptor;

    /**
     * @brief Constructs a UDS acceptor.
     * @param _io IO context for asynchronous operations.
     * @param _own_endpoint Local UDS endpoint (socket file path).
     * @param _configuration Configuration containing permissions and other settings.
     */
    local_acceptor_uds_impl(boost::asio::io_context& _io, endpoint _own_endpoint, std::shared_ptr<configuration> _configuration);
    virtual ~local_acceptor_uds_impl() override;

    /**
     * @brief Initializes the acceptor by opening, binding, and listening.
     * @param _ec Error code set if initialization fails.
     * @param _native_fd Optional native file descriptor (for systemd socket activation).
     *
     * If _native_fd is provided, the acceptor is assigned the existing descriptor.
     * Otherwise, a new socket is created, bound, and set to listen mode.
     *
     * The socket file is unlinked before binding (cleanup from previous runs).
     * Permissions are set according to configuration after successful bind.
     */
    void init(boost::system::error_code& _ec, std::optional<int> _native_fd);
    virtual void close(boost::system::error_code& _ec) override;
    virtual void cancel(boost::system::error_code& _ec) override;

    virtual void async_accept(connection_handler _handler) override;

    virtual port_t get_local_port() override;

private:
    void accept_cbk(boost::system::error_code const& _ec, connection_handler _handler);

    boost::asio::io_context& io_;
    std::mutex mtx_;
    std::unique_ptr<socket> socket_;
    std::unique_ptr<acceptor> const acceptor_;
    endpoint const own_endpoint_;
    std::shared_ptr<configuration> const configuration_;
};

}

#endif
#endif
