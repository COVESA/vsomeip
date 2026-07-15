// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "local_receive_buffer.hpp"
#include "local_client_data.hpp"
#include "timer.hpp"

#include "internal.hpp"

#include "../../protocol/include/command_types.hpp"
#include "../../protocol/include/serialize.hpp"
#include "../../protocol/include/logging.hpp"

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/constants.hpp>
#include <vsomeip/vsomeip_sec.h>
#include <vsomeip/internal/logger.hpp>

#include <boost/asio.hpp>

#include <limits>
#include <vector>
#include <cstdint>
#include <memory>

namespace vsomeip_v3 {

class configuration;
class routing_host;
class local_socket;

namespace trace {
class connector_impl;
} // namespace trace

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
};

/**
 * @struct local_endpoint_params
 * @brief Parameters specific to a single endpoint instance.
 *
 * Contains the peer-specific information and socket for this endpoint.
 */
struct local_endpoint_params {
    client_t peer_{0};
    client_t own_{VSOMEIP_CLIENT_UNSET};
    std::string env_;
    std::shared_ptr<local_socket> socket_;
    /// TCP endpoint the peer advertised during the assign_client handshake
    boost::asio::ip::address routing_address_{};
    port_t routing_port_{ILLEGAL_PORT};
};

/**
 * @class command_batch
 * @brief Accumulates several serialized protocol commands so that local_endpoint::send() can enqueue
 * them with a single lock acquisition and a single flush (one write burst) instead of one syscall per
 * command.
 */
class command_batch {
public:
    /// Serialize and append one protocol command to the batch.
    template<typename T>
    command_batch& add(T const& _command) {
        static_assert(!std::is_same_v<T, protocol::send_command_data>,
                      "command_batch is for untraced control commands; use local_endpoint::send() for payload messages");
        uint32_t const its_size = protocol::wire_size(_command);
        if (its_size == 0) {
            return *this;
        }
        std::size_t const its_offset = buffer_.size();
        buffer_.resize(its_offset + its_size);
        protocol::serialize(_command, buffer_.data() + its_offset);
        if (its_size > largest_command_) {
            largest_command_ = its_size;
        }
        return *this;
    }

    /// Whether no command has been added yet.
    bool empty() const { return buffer_.empty(); }

private:
    friend class local_endpoint;
    std::vector<uint8_t> buffer_;
    uint32_t largest_command_{0};
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
 * CONNECTED  -> FAILED     (I/O error or cleanup_handler triggered with an error)
 * FAILED     -> STOPPED    (cleanup via cleanup_handler with an error)
 * *          -> STOPPED    (explicit stop, potentially called via the cleanup_handler without an error)
 * @endcode
 *
 * Roles:
 * - Sender endpoints: Created via create_client_ep(), initiate connections (INIT state)
 * - Receiver endpoints: Created via create_server_ep() from accepted connections (CONNECTED state)
 *
 * Thread-safety: All public methods are thread-safe. Callbacks to external code
 * (routing_host, cleanup_handler) are invoked without holding internal locks
 * to prevent deadlocks.
 *
 * Callback Guarantee: All public member functions guarantee that they will not
 * synchronously invoke any callbacks (cleanup_handler, message handlers, etc.) except
 * for destructors of promoted weak_ptr references that expire during the function call,
 * or the injected local_socket.
 * This ensures that:
 * - Public functions can be safely called while holding external locks
 * - No re-entrancy issues arise from calling public methods
 * - Callbacks are only invoked asynchronously via boost::asio::post() on the io_context
 *
 * Protocol-agnostic: Delegates protocol-specific behavior to local_socket implementations.
 *
 * @note This endpoint is non-restartable - once stopped, it cannot be reused.
 */
class local_endpoint : public std::enable_shared_from_this<local_endpoint> {
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
    using cleanup_handler_t = std::function<void(bool)>;
    /**
     * @brief Creates a receiver endpoint from an accepted connection.
     * @param _context Shared infrastructure context.
     * @param _params Endpoint-specific parameters.
     * @return Endpoint in CONNECTED state, or nullptr if security check fails.
     */
    static std::shared_ptr<local_endpoint> create_server_ep(local_endpoint_context const& _context, local_endpoint_params _params,
                                                            std::shared_ptr<local_receive_buffer> _receive_buffer);

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
                   std::shared_ptr<local_receive_buffer> _receive_buffer, state_e _initial_state);

    ~local_endpoint();

    /**
     * @brief Starts the endpoint (connect for senders, begin I/O for receivers).
     *
     * For INIT state (senders): Initiates connection to peer.
     * For CONNECTED state (receivers): Starts send/receive operations.
     */
    void start(uint32_t _lc_token = 0);

    /**
     * @brief Stops the endpoint and closes the connection.
     * @param _due_to_error If true, forces immediate close (for TCP, no wait for pending data).
     * @note After stop(), the endpoint cannot be reused.
     */
    void stop(bool _due_to_error);

    /**
     * @brief Sends data to the connected peer.
     * @param _data Pointer to data buffer.
     * @param _size Size of data in bytes.
     * @return true if queued successfully, false if queue or message limit exceeded
     *
     * Messages are queued and sent asynchronously.
     * Queue size is determined from configuration.
     *
     * Note that the messages are only send out after start() has been called.
     */
    bool send(byte_t const* _data, uint32_t _size);

    /**
     * @brief Sends a typed message to the connected peer.
     * @tparam T Type of the message.
     * @param _in Message to send.
     * @return true if queued successfully, false if queue or message limit exceeded.
     *
     * Messages are queued and sent asynchronously.
     * Queue size is determined from configuration.
     *
     * Note that the messages are only send out after start() has been called.
     */
    template<typename T>
    bool send(T const& _in, std::shared_ptr<trace::connector_impl> const& _tc = nullptr);

    /**
     * @brief Enqueues a batch of already-serialized commands and flushes the send queue once.
     * @param _batch The accumulated commands (@see command_batch).
     * @return true if queued successfully, false if the queue/message limit was exceeded or the
     * endpoint is currently flushing.
     */
    bool send(command_batch const& _batch);

    /**
     * @brief Retrieves the client ID of the connected peer.
     * @return vsomeip client ID of the peer application.
     */
    client_t connected_client() const;

    /**
     * @brief Returns a human-readable name for this endpoint.
     * @return String containing role, endpoints, memory address and file descriptor.
     */
    std::string name() const;

    /**
     * Sets an internal flag to reject any new send and trigger the
     * cleanup_handler_ as soon as all data has been flushed.
     **/
    void start_flushing();

    /**
     * @brief Triggers the cleanup_handler of the endpoint with an error.
     */
    void trigger_error();

public:
    std::uint16_t get_local_port() const;
    boost::asio::ip::tcp::endpoint peer_endpoint() const;

    void register_cleanup_handler(const cleanup_handler_t& _handler);

    void print_status();
    size_t get_queue_size() const;

    std::string get_env() const;
    vsomeip_sec_client_t get_sec_client() const;

private:
    /**
     * @brief Transitions to FAILED state and invokes cleanup_handler_ with an error.
     * @note Called internally when unrecoverable errors occur. Must not lock the mutex when invoking
     */
    void escalate();

    /**
     * @brief Internal part of escalate() with lock held
     * @note `_lock` must be held, will be unlocked to invoke cleanup_handler with an error, then locked again
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
    [[nodiscard]] bool process(size_t _new_bytes, std::unique_lock<std::mutex>& _lock);

    void assignment_timeout();

    void connect_unlock();
    void stop_internal(std::unique_lock<std::mutex>& lock, bool _due_to_error);
    void set_state_unlocked(state_e _state);
    void receive_unlock();
    void send_unlock();

    void send_buffer_unlock();

    std::string status() const;
    std::string status_unlock() const;

    void partial_message_timeout();

private:
    // this flag indicates whether sending is already allowed, after
    // starting already connected. Before le::start() had been called,
    // nothing should have been send.
    bool is_started_{false};
    bool is_sending_{false};
    // this flag is stating whether a graceful shutdown is targeted (note that this flag can be set for an endpoint in any of the states
    // INIT, CONNECTING, CONNECTED and can therefore not be easily represented as a state itself)
    bool is_flushing_{false};
    state_e state_{state_e::STOPPED};
    client_t own_{VSOMEIP_CLIENT_UNSET};
    // holds the peer id, the peer env and the sec_client.
    // Note that only in case of the endpoint being
    // the server does the env contain meaningful data.
    local_client_data peer_data_;

    uint32_t const max_connection_attempts_{0};
    uint32_t reconnect_counter_{0};

    size_t const max_message_size_{0};
    size_t const queue_limit_{0};

    std::shared_ptr<local_receive_buffer> const receive_buffer_;
    std::vector<uint8_t> send_queue_;
    cleanup_handler_t cleanup_handler_;

    boost::asio::io_context& io_;
    // shared_ptr because the tcp local_socket needs to be aware of the life time of this socket
    std::shared_ptr<local_socket> const socket_;
    std::weak_ptr<configuration> const configuration_;
    std::weak_ptr<routing_host> const routing_host_;

    std::shared_ptr<timer> connect_debounce_;
    std::shared_ptr<timer> connecting_timebox_;
    std::shared_ptr<timer> assignment_timebox_;
    std::shared_ptr<timer> partial_message_timebox_;

    mutable std::mutex mutex_;
};

} // namespace vsomeip_v3
