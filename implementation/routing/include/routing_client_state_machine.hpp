// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <vsomeip/primitive_types.hpp>

#include "../../endpoints/include/timer.hpp"

#include "internal.hpp"

#include <memory>
#include <mutex>
#include <condition_variable>
#include <ostream>

namespace vsomeip_v3 {

/**
 * @brief Registration states for a routing manager client.
 *
 * The state machine enforces the following valid state transitions:
 *
 * Normal registration flow:
 *   ST_DEREGISTERED -> ST_REGISTERING -> ST_REGISTERED
 *
 * Error/timeout recovery:
 *   Any state -> ST_DEREGISTERED (via deregistered())
 */
enum class routing_client_state_e : uint8_t {
    ST_REGISTERED = 0x0, ///< Fully registered with routing manager
    ST_DEREGISTERED = 0x1, ///< Not connected or registered
    ST_REGISTERING = 0x2, ///< Waiting for client ID assignment
};

std::ostream& operator<<(std::ostream& out_, routing_client_state_e);

/**
 * @brief State machine for managing routing client registration lifecycle.
 *
 * This class encapsulates the registration state logic for a vsomeip routing
 * client, including connection establishment, client ID assignment, and
 * application registration with the routing manager.
 *
 * **Thread Safety:**
 * All methods are thread-safe and can be called from multiple threads
 * concurrently. Internal state is protected by a mutex.
 *
 * **State Transitions:**
 * The state machine enforces strict state transition rules. Attempts to
 * transition from invalid states will fail and return false.
 *
 * **Timeout Protection:**
 * The registration phase has a configurable timeout. If it doesn't complete
 * within the timeout period, the state machine automatically transitions to
 * ST_DEREGISTERED and invokes the error handler.
 *
 * **Error Handler Invocation:**
 * The error handler is called synchronously (on the io_context thread) when:
 * - An assignment timeout occurs while shall_run_ = true
 * - deregistered() is called manually while shall_run_ = true
 *
 * The error handler is NOT called when:
 * - target_shutdown() was called (graceful shutdown, shall_run_ = false)
 * - The state machine is being destroyed
 *
 * **Important:** The error handler must not block indefinitely, as it runs
 * on the io_context thread that processes timers and other async operations.
 *
 * **Lifecycle Control:**
 * The state machine can be paused (target_shutdown()) and resumed
 * (target_running()). When shut down, no new transitions are allowed and
 * error handlers are suppressed (indicating graceful shutdown).
 *
 * **Usage Example:**
 * @code
 * routing_client_state_machine::configuration config;
 * config.shutdown_timeout_ = std::chrono::seconds(3);
 *
 * auto sm = routing_client_state_machine::create(config,
 *     [this] { on_registration_error(); });
 *
 * sm->target_running();
 *
 * if (sm->start_registration()) {
 *     dispatch_assign_client();
 *     // on ASSIGN_CLIENT_ACK:
 *     sm->registered(assigned_client_id);
 * }
 *
 * // some time later for the shutdown:
 * sm->target_shutdown()
 * sm->deregistered();
 *
 * @endcode
 */
class routing_client_state_machine : public std::enable_shared_from_this<routing_client_state_machine> {
    struct hidden { };

public:
    /**
     * @brief Callback invoked when the state machine autonomously transitions
     *        to ST_DEREGISTERED due to timeout or unexpected error.
     *
     * **Invocation Semantics:**
     * This handler is called synchronously on the io_context thread (the same
     * thread that processes timer callbacks) when an unexpected deregistration
     * occurs (shall_run_ = true).
     *
     * The handler is NOT invoked if:
     * - target_shutdown() was called (graceful shutdown, shall_run_ = false)
     * - The state machine is being destroyed
     *
     * **Thread Safety:**
     * The handler is called WITHOUT the state machine's internal mutex held,
     * so it can safely call state machine methods without deadlock risk.
     */
    using error_handler = std::function<void()>;

    /**
     * @brief Configuration for state machine timeouts.
     */
    struct configuration {
        /// Timeout for await_registered() during the shutdown of the application
        /// this is not the timeout for the registration process during the upstart.
        std::chrono::milliseconds shutdown_timeout_{std::chrono::seconds(3)};
    };

    /**
     * @brief Factory method to create a state machine instance.
     *
     * @param _configuration Timeout configuration
     * @param _handler Callback invoked on autonomous deregistration (may be null)
     * @return Shared pointer to the created state machine
     */
    static std::shared_ptr<routing_client_state_machine> create(configuration const& _configuration, error_handler _handler);

    /**
     * @brief Constructor (use create() factory method instead).
     */
    explicit routing_client_state_machine(hidden, configuration const& _configuration, error_handler _handler);

    /**
     * @brief Get the current state.
     *
     * @return Current registration state
     */
    routing_client_state_e state() const;

    /**
     * @brief Signal that the state machine should stop accepting new transitions.
     *
     * After calling this, start_connecting(), start_assignment(), and
     * start_registration() will fail. Existing timers are NOT cancelled.
     */
    void target_shutdown();

    /**
     * @brief Signal that the state machine should accept new transitions.
     *
     * This is the default state. Call this after target_shutdown() to
     * resume operations.
     */
    void target_running();

    /**
     * @brief Start the client ID assignment phase.
     *
     * Valid transition: ST_DEREGISTERED -> ST_REGISTERING
     *
     * @return true if transition succeeded, false if:
     *         - State machine is shut down
     *         - Current state is not ST_DEREGISTERED
     */
    [[nodiscard]] bool start_registration();

    /**
     * @brief Mark the client ID registration as complete.
     *
     * Valid transition: ST_REGISTERING -> ST_REGISTERED
     *
     * @param client_ assigned client-id, used for logging
     * @return true if transition succeeded, false if:
     *         - Current state is not ST_REGISTERING
     */
    [[nodiscard]] bool registered(client_t _client);

    /**
     * @brief Wait for the state machine to reach ST_REGISTERED.
     *
     * This method blocks until either:
     * - The state becomes ST_REGISTERED, or
     * - The shutdown timeout expires
     *
     * @return true if successfully registered, false if:
     *         - Current state is not ST_REGISTERING (not waiting for registration)
     *         - Timeout occurred before registration completed
     */
    [[nodiscard]] bool await_registered();

    /**
     * @brief Mark deregistration as complete.
     *
     * Valid transition: Any state -> ST_DEREGISTERED
     * This is the only transition allowed from any state. It:
     * - Cancels all active timers
     * - Posts error_handler to io_context (asynchronously)
     *
     * This method is also called internally on timeouts.
     */
    void deregistered();

private:
    /**
     * @brief Internal method to perform deregistration.
     *
     * Must be called with _acquired_lock holding mtx_.
     *
     * This method:
     * 1. Transitions to ST_DEREGISTERED
     * 2. Stops all active timers
     * 3. Checks shall_run_:
     *    - If true: Invokes error_handler synchronously (after unlocking)
     *    - If false: Returns without calling error_handler (graceful shutdown)
     *
     * **Lock Management:**
     * @warning This method takes over the ownership over _acquired_lock
     *          to unlock it before invoking the error
     *          handler.
     *
     * The lock is released to prevent deadlock if the error handler calls
     * back into the state machine.
     *
     * @param _acquired_lock Must be locked on entry.
     */
    void deregister_unlocked(std::unique_lock<std::mutex> _acquired_lock);

    /**
     * @brief Internal method to change state.
     *
     * Must be called with mtx_ held. Logs the state transition and
     * notifies the condition variable when reaching ST_REGISTERED or
     * ST_DEREGISTERED.
     *
     * @param _state The new state to transition to
     */
    void change_state_unlocked(routing_client_state_e _state);

    /// Controls whether new transitions are allowed
    bool shall_run_{true};

    /// Timeout configuration
    configuration const configuration_;

    /// Current registration state
    routing_client_state_e state_{routing_client_state_e::ST_DEREGISTERED};

    /// Callback for autonomous deregistration
    error_handler error_handler_;

    /// Client-id for logging
    client_t client_ = VSOMEIP_CLIENT_UNSET;

    /// Former Client-id for logging
    client_t former_client_ = VSOMEIP_CLIENT_UNSET;

    /// Protects all state and timers
    mutable std::mutex mtx_;

    /// Notified when state reaches ST_REGISTERED or ST_DEREGISTERED
    std::condition_variable cv_;
};

} // namespace vsomeip_v3
