// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_LOCAL_ENDPOINT_HPP_
#define VSOMEIP_V3_LOCAL_ENDPOINT_HPP_

#include "endpoint.hpp"
#include "local_receive_buffer.hpp"
#include "timer.hpp"

#include <cstdint>
#include <vsomeip/vsomeip_sec.h>

#include <boost/asio.hpp>

#include <memory>

namespace vsomeip_v3 {

class endpoint_host;
class configuration;
class routing_host;
class local_socket;

/**
 * @struct local_endpoint_context
 * @brief Shared context/dependencies for local endpoint operations.
 *
 * Groups the common infrastructure needed by all local endpoints:
 * IO context, configuration, and host references.
 */
struct local_endpoint_context {
    boost::asio::io_context& io_;
    std::shared_ptr<configuration> configuration_;
    std::weak_ptr<routing_host> routing_host_;
    std::weak_ptr<endpoint_host> endpoint_host_;
};

/**
 * @struct local_endpoint_params
 * @brief Parameters specific to a single endpoint instance.
 *
 * Contains the peer-specific information and socket for this endpoint.
 */
struct local_endpoint_params {
    client_t peer_{0};
    std::shared_ptr<local_socket> socket_;
};

/**
 * @class local_endpoint
 * @brief Non-restartable, full-duplex endpoint for intra-host vsomeip communication.
 *
 * This class represents a bi-directional communication channel between two vsomeip
 * applications on the same host. It manages:
 * - Connection lifecycle
 * - Message send/receive queuing and flow control
 * - Security credential validation
 * - Error handling and escalation
 *
 * State machine:
 * @code
 * INIT       -> CONNECTING (connect initiated)
 * CONNECTING -> CONNECTED  (connection established)
 * CONNECTING -> INIT       (retry after timeout/failure)
 * CONNECTING -> FAILED     (max retries exhausted)
 * CONNECTED  -> FAILED     (I/O error)
 * FAILED     -> STOPPED    (cleanup via error handler)
 * *          -> STOPPED    (explicit stop)
 * @endcode
 *
 * Roles:
 * - Sender endpoints: Created via create_client_ep(), initiate connections (INIT state)
 * - Receiver endpoints: Created via create_server_ep() from accepted connections (CONNECTED state)
 *
 * Thread-safety: All public methods are thread-safe. Callbacks to external code
 * (routing_host, endpoint_host, error_handler) are invoked without holding internal locks
 * to prevent deadlocks.
 *
 * Protocol-agnostic: Delegates protocol-specific behavior to local_socket implementations.
 *
 * @note This endpoint is non-restartable - once stopped, it cannot be reused.
 */
class local_endpoint : public endpoint, public std::enable_shared_from_this<local_endpoint> {
    struct hidden { };

    /**
     * @enum state_e
     * @brief Connection states for the endpoint lifecycle.
     */
    enum class state_e {
        INIT, ///< Initial state, ready to connect (sender only)
        CONNECTING, ///< Connection in progress
        CONNECTED, ///< Connected and ready for I/O
        STOPPED, ///< Stopped, resources released
        FAILED ///< Error occurred, awaiting cleanup
    };

    static char const* to_string(state_e _state);
    friend std::ostream& operator<<(std::ostream& _out, state_e _state);

public:
    /**
     * @brief Creates a receiver endpoint from an accepted connection.
     * @param _context Shared infrastructure context.
     * @param _params Endpoint-specific parameters.
     * @param _is_routing_endpoint whether this is the routing server
     * @return Endpoint in CONNECTED state, or nullptr if security check fails.
     */
    static std::shared_ptr<local_endpoint> create_server_ep(local_endpoint_context const& _context, local_endpoint_params _params,
                                                            std::shared_ptr<local_receive_buffer> _receive_buffer,
                                                            bool _is_routing_endpoint);

    /**
     * @brief Creates a sender endpoint for initiating connections.
     * @param _context Shared infrastructure context.
     * @param _params Endpoint-specific parameters.
     * @return Endpoint in INIT state, ready to connect via start().
     */
    static std::shared_ptr<local_endpoint> create_client_ep(local_endpoint_context const& _context, local_endpoint_params _params);

    /**
     * @brief Internal constructor - use factory methods instead.
     */
    local_endpoint(hidden, local_endpoint_context const& _context, local_endpoint_params _params,
                   std::shared_ptr<local_receive_buffer> _receive_buffer, bool _is_routing_endpoint_, state_e _initial_state);

    virtual ~local_endpoint() override;

    /**
     * @brief Starts the endpoint (connect for senders, begin I/O for receivers).
     *
     * For INIT state (senders): Initiates connection to peer.
     * For CONNECTED state (receivers): Starts send/receive operations.
     */
    void start() override;

    /**
     * @brief Stops the endpoint and closes the connection.
     * @param _due_to_error If true, forces immediate close (for TCP, no wait for pending data).
     * @note After stop(), the endpoint cannot be reused.
     */
    void stop(bool _due_to_error) override;

    /**
     * @brief Sends data to the connected peer.
     * @param _data Pointer to data buffer.
     * @param _size Size of data in bytes.
     * @return true if queued successfully, false if queue or message limit exceeded
     *
     * Messages are queued and sent asynchronously.
     * Queue size is determined from configuration.
     */
    bool send(byte_t const* _data, uint32_t _size) override;

    /**
     * @brief Retrieves the client ID of the connected peer.
     * @return vsomeip client ID of the peer application.
     */
    client_t connected_client() const;

    /**
     * @brief Returns a human-readable name for this endpoint.
     * @return String containing role, endpoints, and memory address.
     */
    std::string const& name() const;

public:
    void prepare_stop(const prepare_stop_handler_t& _handler, service_t _service = ANY_SERVICE) override;

    bool is_established() const override;
    bool is_established_or_connected() const override;

    bool send_to(const std::shared_ptr<endpoint_definition> _target, const byte_t* _data, uint32_t _size) override;
    bool send_error(const std::shared_ptr<endpoint_definition> _target, const byte_t* _data, uint32_t _size) override;
    void receive() override;

    void add_default_target(service_t _service, const std::string& _address, uint16_t _port) override;
    void remove_default_target(service_t _service) override;
    void remove_stop_handler(service_t _service) override;

    std::uint16_t get_local_port() const override;
    bool is_reliable() const override;
    bool is_local() const override;

    void register_error_handler(const error_handler_t& _error) override;

    void print_status() override;
    size_t get_queue_size() const override;

    void set_established(bool _established) override;
    void set_connected(bool _connected) override;

private:
    void restart(bool _force = false) override;

    /**
     * @brief Transitions to FAILED state and invokes error handler.
     * @note Called internally when unrecoverable errors occur. Must not lock the mutex when invoking
     */
    void escalate();

    /**
     * @brief Internal part of escalate() with lock held
     * @note `_lock` must be held, will be unlocked to invoke error handler, then locked again
     */
    void escalate_internal(std::unique_lock<std::mutex>& _lock);

    /**
     * @brief Validates peer credentials against security policy.
     * @return true if peer is allowed to connect, false otherwise.
     * @note Called during connection establishment.
     */
    bool is_allowed();

    void connect_cbk(boost::system::error_code const& _ec);
    void send_cbk(boost::system::error_code const& _ec, size_t _bytes, std::vector<uint8_t> _send_buffer);
    void receive_cbk(boost::system::error_code const& _ec, size_t _bytes);
    [[nodiscard]] bool process(size_t _new_bytes);

    void connect_unlock();
    void stop_unlocked(bool _due_to_error);
    void set_state_unlocked(state_e _state);
    void receive_unlock();
    void send_unlock();

    void send_buffer_unlock();

    std::string status() const;
    std::string status_unlock() const;

private:
    bool const is_routing_endpoint_{false};
    bool is_sending_{false};
    state_e state_{state_e::STOPPED};
    client_t const peer_;
    vsomeip_sec_client_t sec_client_{};

    uint32_t const max_connection_attempts_{0};
    uint32_t reconnect_counter_{0};

    size_t const max_message_size_{0};
    size_t const queue_limit_{0};

    std::shared_ptr<local_receive_buffer> const receive_buffer_;
    std::vector<uint8_t> send_queue_;
    error_handler_t error_handler_;

    boost::asio::io_context& io_;
    // shared_ptr because the tcp local_socket needs to be aware of the life time of this socket
    std::shared_ptr<local_socket> const socket_;
    std::weak_ptr<configuration> const configuration_;
    std::weak_ptr<routing_host> const routing_host_;
    std::weak_ptr<endpoint_host> const endpoint_host_;

    std::shared_ptr<timer> connect_debounce_;
    std::shared_ptr<timer> connecting_timebox_;

    mutable std::mutex mutex_;
};

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_ENDPOINT_HPP_
