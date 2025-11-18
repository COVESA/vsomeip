// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_LOCAL_ACCEPTOR_HPP_
#define VSOMEIP_V3_LOCAL_ACCEPTOR_HPP_

#include <vsomeip/primitive_types.hpp>

#include <boost/system/error_code.hpp>

#include <memory>
#include <optional>
#include <functional>

namespace vsomeip_v3 {
class local_socket;

/**
 * @class local_acceptor
 * @brief Abstract interface for accepting incoming connections in local communication.
 *
 * This class provides a protocol-agnostic interface for accepting connections from
 * other vsomeip applications on the same host. Concrete implementations handle
 * protocol-specific setup (TCP vs UDS).
 *
 * Responsibilities:
 * - Opening and binding to a local endpoint
 * - Listening for incoming connections
 * - Accepting connections and creating local_socket instances
 * - Lifecycle management (close, cancel operations)
 *
 * Concrete implementations:
 * - local_acceptor_tcp_impl: Accepts TCP connections
 * - local_acceptor_uds_impl: Accepts Unix Domain Socket connections
 */
class local_acceptor {
public:
    virtual ~local_acceptor() = default;

    /**
     * @brief Closes the acceptor and stops accepting new connections.
     * @param _ec Error code set if close operation fails.
     */
    virtual void close(boost::system::error_code& _ec) = 0;

    /**
     * @brief Cancels all pending accept operations.
     * @param _ec Error code set if cancel operation fails.
     */
    virtual void cancel(boost::system::error_code& _ec) = 0;

    /**
     * @brief Callback type for async_accept operations.
     * @param error_code Error that occurred, or success.
     * @param socket Newly accepted socket (nullptr on error).
     */
    using connection_handler = std::function<void(boost::system::error_code const&, std::shared_ptr<local_socket>)>;

    /**
     * @brief Asynchronously accepts the next incoming connection.
     * @param _handler Callback invoked when a connection is accepted or an error occurs.
     * @note Can be called repeatedly to accept multiple connections.
     */
    virtual void async_accept(connection_handler _handler) = 0;

    /**
     * @brief Retrieves the local port number the acceptor is bound to.
     * @return Port number for TCP acceptors, VSOMEIP_SEC_PORT_UNUSED for UDS.
     */
    virtual port_t get_local_port() = 0;
};

}

#endif
