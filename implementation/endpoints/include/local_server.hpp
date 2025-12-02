// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_LOCAL_SERVER_HPP_
#define VSOMEIP_V3_LOCAL_SERVER_HPP_

#include "local_receive_buffer.hpp"
#include "timer.hpp"
#include <vsomeip/primitive_types.hpp>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>

#include <memory>
#include <map>
#include <optional>
#include <unordered_map>

namespace vsomeip_v3 {

class endpoint_host;
class local_endpoint;
class local_acceptor;
class local_socket;
class configuration;
class routing_host;

/**
 * @class local_server
 * @brief Server for creating and managing receiver endpoints from accepted connections.
 *
 * This class is responsible for:
 * - Accepting incoming connections via a local_acceptor (TCP or UDS)
 * - Performing connection handshake (client ID assignment or confirmation)
 * - Creating local_endpoint instances for accepted clients
 * - Managing lifecycle of all connected receiver endpoints
 * - Notifying routing_host about client connections/disconnections
 *
 * Connection handshake:
 * 1. Accept socket connection
 * 2. Receive first message (ASSIGN_CLIENT or CONFIG command)
 * 3. For routing manager: Assign client ID and send ACK
 * 4. For clients: Confirm environment and hand over to routing
 * 5. Create local_endpoint and start I/O
 *
 * Thread-safety: All public methods are thread-safe. The server uses a lifecycle
 * counter to reject connections from previous incarnations during restart scenarios.
 *
 * Protocol-agnostic: Works with both TCP and UDS acceptors via local_acceptor interface.
 *
 * @note This class does NOT forward application messages - that is handled by local_endpoint.
 */
class local_server : public std::enable_shared_from_this<local_server> {
public:
    /**
     * @brief Constructs a local server.
     * @param _io IO context for asynchronous operations.
     * @param _acceptor Acceptor for incoming connections (TCP or UDS).
     * @param _configuration Configuration for security, limits, and routing info.
     * @param _routing_host Routing manager for client registration.
     * @param _endpoint_host Endpoint manager for lifecycle notifications.
     * @param _is_router True if this is the routing manager's server.
     * @note the acceptor needs to be already in the "listen" state.
     */
    local_server(boost::asio::io_context& _io, std::shared_ptr<local_acceptor> _acceptor, std::shared_ptr<configuration> _configuration,
                 std::weak_ptr<routing_host> _routing_host, std::weak_ptr<endpoint_host> _endpoint_host, bool _is_router);
    ~local_server();

    /**
     * @brief Starts accepting connections.
     * @note Must be called to begin serving clients.
     */
    void start();

    /**
     * @brief Stops the server, closes acceptor, and disconnects all clients.
     * @note Increments lifecycle counter to reject stale connection attempts.
     */
    void stop();

    /**
     * @brief Cancels pending accepts but keeps acceptor open.
     * @note Increments lifecycle counter and disconnects clients, but allows restart.
     */
    void halt();

    /**
     * @brief Disconnects a specific client.
     * @param _client Client ID to disconnect.
     * @param _due_to_error If true, forces immediate close without graceful shutdown.
     */
    void disconnect_from(client_t _client, bool _due_to_error);

    /**
     * @brief Retrieves the local port number the server is bound to.
     * @return Port number for TCP, VSOMEIP_SEC_PORT_UNUSED for UDS.
     */
    port_t get_local_port() const;
    void print_status() const;

private:
    /**
     * @brief Callback invoked when a connection is accepted.
     * @param _ec Error code from accept operation.
     * @param _socket Accepted socket (ownership transferred).
     * @param _lc_count Lifecycle counter from when the callback had been queued.
     *
     * Creates a tmp_connection to perform handshake before creating local_endpoint.
     */
    void accept_cbk(boost::system::error_code const& _ec, std::shared_ptr<local_socket> _socket, uint32_t _lc_count);

    /**
     * @brief Adds a fully-handshaked connection as a managed endpoint.
     * @param _client Client ID of the connected application.
     * @param _socket Socket for communication (ownership transferred).
     * @param _buffer Receive buffer (may contain data after handshake).
     * @param _lc_count Lifecycle counter from when connection was initiated.
     * @param _environment Client environment string (hostname, etc.).
     *
     * Rejects connection if lifecycle counter doesn't match (stale connection).
     * Notifies routing_host and creates local_endpoint for ongoing communication.
     */
    void add_connection(client_t _client, std::shared_ptr<local_socket> _socket, std::shared_ptr<local_receive_buffer> _buffer,
                        uint32_t _lc_count, std::string _environment);

    /**
     * @brief Removes a connection when endpoint reports failure.
     * @param _client Client ID to remove.
     * @param _ep Endpoint reporting the failure.
     *
     * Validates that the endpoint matches the registered one before removing.
     */
    void remove_connection(client_t _client, std::shared_ptr<local_endpoint> _ep);

    /**
     * @brief Stops all connected client endpoints.
     * @note Called during server shutdown.
     */
    void stop_client_connections_unlock();

    void start_unlock(uint32_t _lc_count);

    /**
     * @struct tmp_connection
     * @brief Temporary wrapper for performing connection handshake.
     *
     * This helper class handles the initial protocol exchange to:
     * - Determine the client's vsomeip ID
     * - Exchange routing information (hostname, etc.)
     * - Transfer the socket and buffer to a local_endpoint after handshake
     *
     * Lifetime: Created on accept, destroyed after handshake completes.
     */
    struct tmp_connection : std::enable_shared_from_this<tmp_connection> {
        tmp_connection(std::shared_ptr<local_socket> _socket, bool is_router_, uint32_t _lc_count, std::weak_ptr<local_server> _parent,
                       std::shared_ptr<configuration> _configuration);
        ~tmp_connection();

        void async_receive();
        void receive_cbk(boost::system::error_code const& _ec, size_t _bytes);
        client_t assign_client(size_t _message_size) const;
        void confirm_connection(size_t _message_size);
        void send_client_id(client_t _client);
        void hand_over(client_t _client, std::string _environment);

        bool const is_router_{false};
        uint32_t const lc_count_{0};
        std::shared_ptr<local_socket> socket_; // not const as it will be moved out after the handshake

        std::shared_ptr<local_receive_buffer> const receive_buffer_;

        std::weak_ptr<local_server> const parent_;
        std::shared_ptr<configuration> const configuration_;
    };

    bool const is_router_{false};
    uint32_t lc_count_{0};
    boost::asio::io_context& io_;
    std::shared_ptr<local_acceptor> const acceptor_;
    std::shared_ptr<configuration> const configuration_;
    std::shared_ptr<timer> debounce_;
    std::weak_ptr<routing_host> const routing_host_;
    std::weak_ptr<endpoint_host> const endpoint_host_;

    std::mutex mutable mtx_;
    std::unordered_map<client_t, std::shared_ptr<local_endpoint>> clients_;
};
}

#endif
