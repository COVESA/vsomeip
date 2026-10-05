// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <vsomeip/primitive_types.hpp>

#include "internal.hpp"

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
 * Error recovery:
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
 * Not thread safe. Any usage has to be synchronized externally.
 *
 * **State Transitions:**
 * The state machine enforces strict state transition rules. Attempts to
 * transition from invalid states will fail and return false.
 *
 * **Lifecycle Control:**
 * The state machine can be paused (target_shutdown()) and resumed
 * (target_running()). When shut down, no new transitions are allowed.
 *
 * **Usage Example:**
 * @code
 * auto sm = routing_client_state_machine();
 *
 * sm.target_running();
 *
 * if (sm.start_registration()) {
 *     dispatch_assign_client();
 *     // on ASSIGN_CLIENT_ACK:
 *     sm.registered(assigned_client_id);
 * }
 *
 * // some time later for the shutdown:
 * sm.target_shutdown()
 * sm.deregistered();
 *
 * @endcode
 */
class routing_client_state_machine {
public:
    /**
     * @brief Constructor
     */
    explicit routing_client_state_machine() = default;

    /**
     * @brief Get the current state.
     *
     * @return Current registration state
     */
    routing_client_state_e state() const;

    /**
     * @brief Signal that the state machine should stop accepting new transitions.
     *
     * After calling this, start_registration() will fail.
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
     * @brief Mark deregistration as complete.
     *
     * Valid transition: Any state -> ST_DEREGISTERED
     * This is the only transition allowed from any state
     */
    void deregistered();

private:
    /**
     * @brief Internal method to change state.
     *
     * Must be called with rmc mutex_ held. Logs the state transition
     * when reaching ST_REGISTERED or ST_DEREGISTERED.
     *
     * @param _state The new state to transition to
     */
    void change_state(routing_client_state_e _state);

    /// Controls whether new transitions are allowed
    bool shall_run_{true};

    /// Current registration state
    routing_client_state_e state_{routing_client_state_e::ST_DEREGISTERED};

    /// Client-id for logging
    client_t client_ = VSOMEIP_CLIENT_UNSET;

    /// Former Client-id for logging
    client_t former_client_ = VSOMEIP_CLIENT_UNSET;
};

} // namespace vsomeip_v3
