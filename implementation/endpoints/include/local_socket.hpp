// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_LOCAL_SOCKET_HPP_
#define VSOMEIP_V3_LOCAL_SOCKET_HPP_

#include <vsomeip/vsomeip_sec.h>
#include <vsomeip/primitive_types.hpp>

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace vsomeip_v3 {
class configuration;

enum class socket_role_e { SENDER, RECEIVER };
char const* to_string(socket_role_e _role);

/**
 * @class local_socket
 * @brief Abstract interface for socket implementations used in local (intra-host) communication.
 *
 * This class provides a protocol-agnostic interface for both TCP and UDS (Unix Domain Sockets)
 * implementations. All implementations must be thread-safe.
 *
 * The interface supports:
 * - Asynchronous connect, send, and receive operations
 * - Socket lifecycle management (preparation, connection, shutdown)
 * - Security credential updates
 * - Endpoint information retrieval
 *
 * Concrete implementations:
 * - local_socket_tcp_impl: TCP-based sockets for local communication
 * - local_socket_uds_impl: Unix Domain Sockets for local communication
 */
class local_socket {
public:
    using read_handler = std::function<void(boost::system::error_code const&, size_t)>;
    using write_handler = std::function<void(boost::system::error_code const&, size_t, std::vector<uint8_t>)>;
    using connect_handler = std::function<void(boost::system::error_code const&)>;

    virtual ~local_socket() = default;

    /**
     * @brief Stops the socket and closes the underlying connection.
     * @param _force If false (TCP only), waits for pending data to be sent before closing.
     *               If true, closes immediately without waiting for pending data.
     */
    virtual void stop(bool _force) = 0;

    /**
     * @brief Prepares the socket for connection by opening and configuring it.
     * @param _configuration Configuration containing socket options.
     * @param _ec Error code set if preparation fails.
     * @note Must be called before async_connect().
     */
    virtual void prepare_connect(configuration const& _configuration, boost::system::error_code& _ec) = 0;

    /**
     * @brief Asynchronously initiates a connection to the peer endpoint.
     * @param _handler Callback invoked when connection completes or fails.
     * @note prepare_connect() must be called before this method.
     */
    virtual void async_connect(connect_handler _handler) = 0;

    /**
     * @brief Asynchronously receives data into the provided buffer.
     * @param _buffer Buffer to receive data into.
     * @param _handler Callback invoked when data is received or an error occurs.
     */
    virtual void async_receive(boost::asio::mutable_buffer _buffer, read_handler _handler) = 0;

    /**
     * @brief Asynchronously sends data to the connected peer.
     * @param _data Data to send (ownership transferred to async operation).
     * @param _handler Callback invoked when send completes, receives the data back.
     */
    virtual void async_send(std::vector<uint8_t> _data, write_handler _handler) = 0;

    /**
     * @brief Updates security client credentials from the connected peer.
     * @param _client Structure to populate with peer credentials (UID, GID, port).
     * @param _configuration Configuration for security settings.
     * @return true if credentials were successfully retrieved, false otherwise.
     */
    virtual bool update(vsomeip_sec_client_t& _client, configuration const& _configuration) = 0;

    virtual port_t own_port() const = 0;
    virtual boost::asio::ip::tcp::endpoint peer_endpoint() const = 0;

    /**
     * @brief Returns a human-readable string representation of this socket.
     * @return String containing role, endpoints, and memory address.
     */
    virtual std::string const& to_string() const = 0;
};
} // namespace vsomeip_v3

#endif
