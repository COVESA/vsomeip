// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../../../implementation/routing/include/routing_client_state_machine.hpp"

#include <boost/asio.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <thread>
#include <chrono>
#include <atomic>
#include <condition_variable>

using namespace vsomeip_v3;
using namespace std::chrono_literals;

class routing_client_state_machine_test : public ::testing::Test {
protected:
    void SetUp() override {
        error_count_ = 0;
        // Keep io_context alive with a work guard
        work_guard_ =
                std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(io_context_.get_executor());
        io_thread_ = std::thread([this] { io_context_.run(); });
    }

    void TearDown() override {
        // Release work guard to allow io_context to stop
        work_guard_.reset();
        io_context_.stop();
        if (io_thread_.joinable()) {
            io_thread_.join();
        }
    }

    std::shared_ptr<routing_client_state_machine> create_state_machine(std::chrono::milliseconds assignment_timeout = 500ms,
                                                                       std::chrono::milliseconds registration_timeout = 500ms,
                                                                       std::chrono::milliseconds shutdown_timeout = 1000ms) {

        routing_client_state_machine::configuration config;
        config.assignment_timeout_ = assignment_timeout;
        config.register_timeout_ = registration_timeout;
        config.shutdown_timeout_ = shutdown_timeout;

        return routing_client_state_machine::create(io_context_, config, [this] {
            {
                std::scoped_lock lock(error_mutex_);
                ++error_count_;
            }
            error_cv_.notify_all();
        });
    }

    bool wait_for_error(int initial_count, std::chrono::milliseconds timeout = 2000ms) {
        std::unique_lock<std::mutex> lock(error_mutex_);
        if (error_count_ > initial_count) {
            return true;
        }
        return error_cv_.wait_for(lock, timeout, [this, initial_count] { return error_count_ > initial_count; });
    }

    void reset_error_counter() {
        std::scoped_lock lock(error_mutex_);
        error_count_ = 0;
    }

    int get_error_count() {
        std::scoped_lock lock(error_mutex_);
        return error_count_;
    }

    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_;
    std::thread io_thread_;
    int error_count_{0};
    std::mutex error_mutex_;
    std::condition_variable error_cv_;
};

// ============================================================================
// Basic State Transitions
// ============================================================================

TEST_F(routing_client_state_machine_test, initial_state_is_deregistered) {
    auto sm = create_state_machine();
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());
}

TEST_F(routing_client_state_machine_test, happy_path_full_registration) {
    auto sm = create_state_machine();

    // Start in running mode
    sm->target_running();

    // ST_DEREGISTERED -> ST_CONNECTING
    EXPECT_TRUE(sm->start_connecting());
    EXPECT_EQ(routing_client_state_e::ST_CONNECTING, sm->state());

    // ST_CONNECTING -> ST_ASSIGNING
    EXPECT_TRUE(sm->start_assignment());
    EXPECT_EQ(routing_client_state_e::ST_ASSIGNING, sm->state());

    // ST_ASSIGNING -> ST_ASSIGNED
    EXPECT_TRUE(sm->assigned());
    EXPECT_EQ(routing_client_state_e::ST_ASSIGNED, sm->state());

    // ST_ASSIGNED -> ST_REGISTERING
    EXPECT_TRUE(sm->start_registration());
    EXPECT_EQ(routing_client_state_e::ST_REGISTERING, sm->state());

    // ST_REGISTERING -> ST_REGISTERED
    EXPECT_TRUE(sm->registered());
    EXPECT_EQ(routing_client_state_e::ST_REGISTERED, sm->state());
}

TEST_F(routing_client_state_machine_test, deregistered_from_any_state) {
    auto sm = create_state_machine();
    sm->target_running();

    // From ST_DEREGISTERED
    int initial_count = get_error_count();
    sm->deregistered();
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());
    EXPECT_TRUE(wait_for_error(initial_count, 100ms));

    reset_error_counter();

    // From ST_CONNECTING
    ASSERT_TRUE(sm->start_connecting());
    initial_count = get_error_count();
    sm->deregistered();
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());
    EXPECT_TRUE(wait_for_error(initial_count, 100ms));

    reset_error_counter();

    // From ST_ASSIGNING
    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    initial_count = get_error_count();
    sm->deregistered();
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());
    EXPECT_TRUE(wait_for_error(initial_count, 100ms));

    reset_error_counter();

    // From ST_ASSIGNED
    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    ASSERT_TRUE(sm->assigned());
    initial_count = get_error_count();
    sm->deregistered();
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());
    EXPECT_TRUE(wait_for_error(initial_count, 100ms));

    reset_error_counter();

    // From ST_REGISTERING
    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    ASSERT_TRUE(sm->assigned());
    ASSERT_TRUE(sm->start_registration());
    initial_count = get_error_count();
    sm->deregistered();
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());
    EXPECT_TRUE(wait_for_error(initial_count, 100ms));

    reset_error_counter();

    // From ST_REGISTERED
    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    ASSERT_TRUE(sm->assigned());
    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered());
    initial_count = get_error_count();
    sm->deregistered();
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());
    EXPECT_TRUE(wait_for_error(initial_count, 100ms));
}

TEST_F(routing_client_state_machine_test, graceful_deregistration_flow) {
    auto sm = create_state_machine();
    sm->target_running();
    int initial_count = get_error_count();

    // Get to registered state
    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    ASSERT_TRUE(sm->assigned());
    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered());

    // ST_REGISTERED -> ST_DEREGISTERING
    EXPECT_TRUE(sm->start_deregister());
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERING, sm->state());

    // ST_DEREGISTERING -> ST_DEREGISTERED
    sm->deregistered();
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());
    EXPECT_TRUE(wait_for_error(initial_count, 100ms));
}

// ============================================================================
// Invalid State Transitions
// ============================================================================

TEST_F(routing_client_state_machine_test, cannot_start_connecting_when_shutdown) {
    auto sm = create_state_machine();
    sm->target_shutdown();

    EXPECT_FALSE(sm->start_connecting());
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());
}

TEST_F(routing_client_state_machine_test, cannot_start_connecting_from_non_deregistered) {
    auto sm = create_state_machine();
    sm->target_running();

    // Get to ST_CONNECTING
    ASSERT_TRUE(sm->start_connecting());

    // Try to start connecting again - should fail
    EXPECT_FALSE(sm->start_connecting());
    EXPECT_EQ(routing_client_state_e::ST_CONNECTING, sm->state());

    // Get to ST_ASSIGNING
    ASSERT_TRUE(sm->start_assignment());
    EXPECT_FALSE(sm->start_connecting());
    EXPECT_EQ(routing_client_state_e::ST_ASSIGNING, sm->state());
}

TEST_F(routing_client_state_machine_test, cannot_start_assignment_from_non_connecting) {
    auto sm = create_state_machine();
    sm->target_running();

    // From ST_DEREGISTERED
    EXPECT_FALSE(sm->start_assignment());
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());

    // Get to ST_ASSIGNING
    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());

    // Try to start assignment again - should fail
    EXPECT_FALSE(sm->start_assignment());
    EXPECT_EQ(routing_client_state_e::ST_ASSIGNING, sm->state());

    // From ST_ASSIGNED
    ASSERT_TRUE(sm->assigned());
    EXPECT_FALSE(sm->start_assignment());
    EXPECT_EQ(routing_client_state_e::ST_ASSIGNED, sm->state());
}

TEST_F(routing_client_state_machine_test, cannot_mark_assigned_from_non_assigning) {
    auto sm = create_state_machine();
    sm->target_running();

    // From ST_DEREGISTERED
    EXPECT_FALSE(sm->assigned());
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());

    // From ST_CONNECTING
    ASSERT_TRUE(sm->start_connecting());
    EXPECT_FALSE(sm->assigned());
    EXPECT_EQ(routing_client_state_e::ST_CONNECTING, sm->state());

    // Get to ST_ASSIGNED
    ASSERT_TRUE(sm->start_assignment());
    ASSERT_TRUE(sm->assigned());

    // From ST_ASSIGNED
    EXPECT_FALSE(sm->assigned());
    EXPECT_EQ(routing_client_state_e::ST_ASSIGNED, sm->state());
}

TEST_F(routing_client_state_machine_test, cannot_start_registration_from_non_assigned) {
    auto sm = create_state_machine();
    sm->target_running();

    // From ST_DEREGISTERED
    EXPECT_FALSE(sm->start_registration());
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());

    // From ST_CONNECTING
    ASSERT_TRUE(sm->start_connecting());
    EXPECT_FALSE(sm->start_registration());
    EXPECT_EQ(routing_client_state_e::ST_CONNECTING, sm->state());

    // From ST_ASSIGNING
    ASSERT_TRUE(sm->start_assignment());
    EXPECT_FALSE(sm->start_registration());
    EXPECT_EQ(routing_client_state_e::ST_ASSIGNING, sm->state());

    // Get to ST_REGISTERING
    ASSERT_TRUE(sm->assigned());
    ASSERT_TRUE(sm->start_registration());

    // From ST_REGISTERING
    EXPECT_FALSE(sm->start_registration());
    EXPECT_EQ(routing_client_state_e::ST_REGISTERING, sm->state());
}

TEST_F(routing_client_state_machine_test, cannot_mark_registered_from_non_registering) {
    auto sm = create_state_machine();
    sm->target_running();

    // From ST_DEREGISTERED
    EXPECT_FALSE(sm->registered());
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());

    // From ST_CONNECTING
    ASSERT_TRUE(sm->start_connecting());
    EXPECT_FALSE(sm->registered());
    EXPECT_EQ(routing_client_state_e::ST_CONNECTING, sm->state());

    // From ST_ASSIGNING
    ASSERT_TRUE(sm->start_assignment());
    EXPECT_FALSE(sm->registered());
    EXPECT_EQ(routing_client_state_e::ST_ASSIGNING, sm->state());

    // From ST_ASSIGNED
    ASSERT_TRUE(sm->assigned());
    EXPECT_FALSE(sm->registered());
    EXPECT_EQ(routing_client_state_e::ST_ASSIGNED, sm->state());

    // Get to ST_REGISTERED
    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered());

    // From ST_REGISTERED
    EXPECT_FALSE(sm->registered());
    EXPECT_EQ(routing_client_state_e::ST_REGISTERED, sm->state());
}

TEST_F(routing_client_state_machine_test, cannot_start_deregister_from_non_registered) {
    auto sm = create_state_machine();
    sm->target_running();

    // From ST_DEREGISTERED
    EXPECT_FALSE(sm->start_deregister());
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());

    // From ST_CONNECTING
    ASSERT_TRUE(sm->start_connecting());
    EXPECT_FALSE(sm->start_deregister());
    EXPECT_EQ(routing_client_state_e::ST_CONNECTING, sm->state());

    // From ST_ASSIGNING
    ASSERT_TRUE(sm->start_assignment());
    EXPECT_FALSE(sm->start_deregister());
    EXPECT_EQ(routing_client_state_e::ST_ASSIGNING, sm->state());
}

// ============================================================================
// Timeout Tests
// ============================================================================

TEST_F(routing_client_state_machine_test, assignment_timeout) {
    auto sm = create_state_machine(500ms, 500ms);
    sm->target_running();
    int initial_count = get_error_count();

    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    EXPECT_EQ(routing_client_state_e::ST_ASSIGNING, sm->state());

    // Wait for timeout (500ms + margin)
    EXPECT_TRUE(wait_for_error(initial_count, 700ms));
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());
}

TEST_F(routing_client_state_machine_test, registration_timeout) {
    auto sm = create_state_machine(500ms, 500ms);
    sm->target_running();
    int initial_count = get_error_count();

    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    ASSERT_TRUE(sm->assigned());
    ASSERT_TRUE(sm->start_registration());
    EXPECT_EQ(routing_client_state_e::ST_REGISTERING, sm->state());

    // Wait for timeout (500ms + margin)
    EXPECT_TRUE(wait_for_error(initial_count, 700ms));
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());
}

TEST_F(routing_client_state_machine_test, assignment_timeout_canceled_on_success) {
    auto sm = create_state_machine(500ms, 500ms);
    sm->target_running();
    int initial_count = get_error_count();

    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());

    // Complete assignment before timeout
    std::this_thread::sleep_for(100ms);
    ASSERT_TRUE(sm->assigned());

    // Wait to ensure timeout doesn't fire
    EXPECT_FALSE(wait_for_error(initial_count, 600ms));
    EXPECT_EQ(routing_client_state_e::ST_ASSIGNED, sm->state());
}

TEST_F(routing_client_state_machine_test, registration_timeout_canceled_on_success) {
    auto sm = create_state_machine(500ms, 500ms);
    sm->target_running();
    int initial_count = get_error_count();

    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    ASSERT_TRUE(sm->assigned());
    ASSERT_TRUE(sm->start_registration());

    // Complete registration before timeout
    std::this_thread::sleep_for(100ms);
    ASSERT_TRUE(sm->registered());

    // Wait to ensure timeout doesn't fire
    EXPECT_FALSE(wait_for_error(initial_count, 600ms));
    EXPECT_EQ(routing_client_state_e::ST_REGISTERED, sm->state());
}

TEST_F(routing_client_state_machine_test, timeout_does_not_fire_after_manual_deregister) {
    auto sm = create_state_machine(500ms, 500ms);
    sm->target_running();
    int initial_count = get_error_count();

    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    EXPECT_EQ(routing_client_state_e::ST_ASSIGNING, sm->state());

    // Manually deregister
    sm->deregistered();
    EXPECT_TRUE(wait_for_error(initial_count, 100ms));

    int error_count_after_deregister = get_error_count();

    // Wait to ensure assignment timeout doesn't fire again
    std::this_thread::sleep_for(600ms);
    EXPECT_EQ(error_count_after_deregister, get_error_count());
}

// ============================================================================
// Await Methods Tests
// ============================================================================

TEST_F(routing_client_state_machine_test, await_registered_succeeds_when_already_registered) {
    auto sm = create_state_machine();
    sm->target_running();

    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    ASSERT_TRUE(sm->assigned());
    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered());

    EXPECT_TRUE(sm->await_registered());
}

TEST_F(routing_client_state_machine_test, await_registered_waits_for_registration) {
    auto sm = create_state_machine();
    sm->target_running();

    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    ASSERT_TRUE(sm->assigned());
    ASSERT_TRUE(sm->start_registration());

    // Start waiting in separate thread
    std::atomic<bool> await_result{false};
    std::thread waiter([&sm, &await_result] { await_result = sm->await_registered(); });

    // Complete registration after a delay
    std::this_thread::sleep_for(100ms);
    ASSERT_TRUE(sm->registered());

    waiter.join();
    EXPECT_TRUE(await_result.load());
}

TEST_F(routing_client_state_machine_test, await_registered_fails_from_wrong_state) {
    auto sm = create_state_machine();
    sm->target_running();

    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());

    // Not in ST_REGISTERING
    EXPECT_FALSE(sm->await_registered());
}

TEST_F(routing_client_state_machine_test, await_registered_times_out) {
    auto sm = create_state_machine(500ms, 500ms, 200ms); // Short shutdown timeout
    sm->target_running();

    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    ASSERT_TRUE(sm->assigned());
    ASSERT_TRUE(sm->start_registration());

    // Don't call registered() - should timeout
    EXPECT_FALSE(sm->await_registered());
}

TEST_F(routing_client_state_machine_test, await_deregistered_succeeds_when_already_deregistered) {
    auto sm = create_state_machine();

    EXPECT_TRUE(sm->await_deregistered());
}

TEST_F(routing_client_state_machine_test, await_deregistered_waits_for_deregistration) {
    auto sm = create_state_machine();
    sm->target_running();

    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    ASSERT_TRUE(sm->assigned());
    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered());
    ASSERT_TRUE(sm->start_deregister());

    // Start waiting in separate thread
    std::atomic<bool> await_result{false};
    std::thread waiter([&sm, &await_result] { await_result = sm->await_deregistered(); });

    // Complete deregistration after a delay
    std::this_thread::sleep_for(100ms);
    sm->deregistered();

    waiter.join();
    EXPECT_TRUE(await_result.load());
}

TEST_F(routing_client_state_machine_test, await_deregistered_fails_from_wrong_state) {
    auto sm = create_state_machine();
    sm->target_running();

    ASSERT_TRUE(sm->start_connecting());

    // Not in ST_DEREGISTERING
    EXPECT_FALSE(sm->await_deregistered());
}

// ============================================================================
// Re-registration Tests
// ============================================================================

TEST_F(routing_client_state_machine_test, can_reregister_after_deregistration) {
    auto sm = create_state_machine();
    sm->target_running();

    // First registration
    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    ASSERT_TRUE(sm->assigned());
    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered());

    // Deregister
    int initial_count = get_error_count();
    sm->deregistered();
    EXPECT_TRUE(wait_for_error(initial_count, 100ms));

    reset_error_counter();

    // Second registration
    EXPECT_TRUE(sm->start_connecting());
    EXPECT_TRUE(sm->start_assignment());
    EXPECT_TRUE(sm->assigned());
    EXPECT_TRUE(sm->start_registration());
    EXPECT_TRUE(sm->registered());
    EXPECT_EQ(routing_client_state_e::ST_REGISTERED, sm->state());

    // Should not have received error during re-registration
    EXPECT_EQ(0, get_error_count());
}

TEST_F(routing_client_state_machine_test, can_reregister_after_assignment_timeout) {
    auto sm = create_state_machine(500ms, 500ms);
    sm->target_running();
    int initial_count = get_error_count();

    // First attempt with timeout
    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    EXPECT_TRUE(wait_for_error(initial_count, 700ms));
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());

    reset_error_counter();

    // Second attempt successful
    EXPECT_TRUE(sm->start_connecting());
    EXPECT_TRUE(sm->start_assignment());
    EXPECT_TRUE(sm->assigned());
    EXPECT_TRUE(sm->start_registration());
    EXPECT_TRUE(sm->registered());
    EXPECT_EQ(routing_client_state_e::ST_REGISTERED, sm->state());

    // Should not have received error during re-registration
    EXPECT_EQ(0, get_error_count());
}

TEST_F(routing_client_state_machine_test, can_reregister_after_registration_timeout) {
    auto sm = create_state_machine(500ms, 500ms);
    sm->target_running();
    int initial_count = get_error_count();

    // First attempt with timeout
    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    ASSERT_TRUE(sm->assigned());
    ASSERT_TRUE(sm->start_registration());
    EXPECT_TRUE(wait_for_error(initial_count, 700ms));
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());

    reset_error_counter();

    // Second attempt successful
    EXPECT_TRUE(sm->start_connecting());
    EXPECT_TRUE(sm->start_assignment());
    EXPECT_TRUE(sm->assigned());
    EXPECT_TRUE(sm->start_registration());
    EXPECT_TRUE(sm->registered());
    EXPECT_EQ(routing_client_state_e::ST_REGISTERED, sm->state());

    // Should not have received error during re-registration
    EXPECT_EQ(0, get_error_count());
}

// ============================================================================
// Target Shutdown/Running Tests
// ============================================================================

TEST_F(routing_client_state_machine_test, target_shutdown_prevents_new_connections) {
    auto sm = create_state_machine();
    sm->target_running();

    ASSERT_TRUE(sm->start_connecting());

    // Shutdown
    sm->target_shutdown();

    // Cannot start assignment when shut down
    EXPECT_FALSE(sm->start_assignment());
}

TEST_F(routing_client_state_machine_test, can_restart_after_shutdown) {
    auto sm = create_state_machine();
    sm->target_running();

    ASSERT_TRUE(sm->start_connecting());
    sm->target_shutdown();

    // Cannot proceed while shut down
    EXPECT_FALSE(sm->start_assignment());

    // Deregister and restart
    sm->deregistered();

    sm->target_running();
    EXPECT_TRUE(sm->start_connecting());
    EXPECT_TRUE(sm->start_assignment());
}

// ============================================================================
// Error Handler Tests
// ============================================================================

TEST_F(routing_client_state_machine_test, error_handler_called_on_timeout) {
    auto sm = create_state_machine(500ms, 500ms);
    sm->target_running();
    int initial_count = get_error_count();

    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    EXPECT_TRUE(wait_for_error(initial_count, 700ms));
}

TEST_F(routing_client_state_machine_test, error_handler_called_on_manual_deregister) {
    auto sm = create_state_machine();
    sm->target_running();
    int initial_count = get_error_count();

    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    sm->deregistered();
    EXPECT_TRUE(wait_for_error(initial_count, 200ms));
}

TEST_F(routing_client_state_machine_test, null_error_handler_does_not_crash) {
    routing_client_state_machine::configuration config;
    config.assignment_timeout_ = 500ms;

    auto sm = routing_client_state_machine::create(io_context_, config, nullptr);
    sm->target_running();

    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());

    // Should not crash even though error_handler is null
    std::this_thread::sleep_for(700ms);
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());
}

TEST_F(routing_client_state_machine_test, error_handler_executes_synchronously_on_timeout) {
    std::atomic<std::thread::id> handler_thread_id;

    routing_client_state_machine::configuration config;
    config.assignment_timeout_ = 50ms;

    auto sm = routing_client_state_machine::create(io_context_, config,
                                                   [&handler_thread_id] { handler_thread_id = std::this_thread::get_id(); });

    sm->target_running();
    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());

    std::this_thread::sleep_for(100ms);

    // Handler should execute on io_context thread
    EXPECT_EQ(handler_thread_id.load(), io_thread_.get_id());
}

TEST_F(routing_client_state_machine_test, error_handler_can_reenter_state_machine) {
    std::atomic<routing_client_state_e> observed_state;
    std::atomic<bool> reentry_successful{false};

    routing_client_state_machine::configuration config;
    config.assignment_timeout_ = 50ms;

    std::shared_ptr<routing_client_state_machine> sm;
    sm = routing_client_state_machine::create(io_context_, config, [&sm, &observed_state, &reentry_successful] {
        // This should NOT deadlock since lock is released
        observed_state = sm->state();

        // Try to restart connection (should succeed)
        if (sm->start_connecting()) {
            reentry_successful = true;
        }
    });

    sm->target_running();
    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());

    std::this_thread::sleep_for(100ms);

    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, observed_state.load());
    EXPECT_TRUE(reentry_successful);
    EXPECT_EQ(routing_client_state_e::ST_CONNECTING, sm->state());
}

TEST_F(routing_client_state_machine_test, no_error_handler_on_graceful_shutdown) {
    auto sm = create_state_machine(50ms, 50ms);
    sm->target_running();

    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());

    // Shutdown before timeout
    sm->target_shutdown();

    int error_count_before = get_error_count();

    // Let timeout fire
    std::this_thread::sleep_for(100ms);

    // No error handler should be called (graceful shutdown)
    EXPECT_EQ(error_count_before, get_error_count());
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());
}

TEST_F(routing_client_state_machine_test, no_stale_handlers_after_rapid_restart) {
    auto sm = create_state_machine(50ms, 50ms);
    sm->target_running();

    int initial_count = get_error_count();

    // Start assignment that will timeout
    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());

    // Wait for timeout and handler execution
    EXPECT_TRUE(wait_for_error(initial_count, 100ms));
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());

    int count_after_timeout = get_error_count();

    // Immediately restart and complete registration
    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    ASSERT_TRUE(sm->assigned());
    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered());

    // Wait to ensure no late handlers
    std::this_thread::sleep_for(100ms);

    // Should still have same error count (no stale handlers)
    EXPECT_EQ(count_after_timeout, get_error_count());
    EXPECT_EQ(routing_client_state_e::ST_REGISTERED, sm->state());
}

TEST_F(routing_client_state_machine_test, handler_during_shutdown_race) {
    auto sm = create_state_machine(50ms, 50ms);
    sm->target_running();

    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());

    int initial_count = get_error_count();

    // Race: shutdown very close to timeout
    std::this_thread::sleep_for(45ms);
    sm->target_shutdown();
    std::this_thread::sleep_for(20ms);

    int final_count = get_error_count();

    // Handler called 0 or 1 times depending on race winner
    EXPECT_LE(final_count - initial_count, 1);
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());

    // Restart and verify no accumulated handlers
    sm->target_running();
    ASSERT_TRUE(sm->start_connecting());
    std::this_thread::sleep_for(50ms);

    EXPECT_EQ(final_count, get_error_count());
}

TEST_F(routing_client_state_machine_test, multiple_shutdown_restart_cycles_no_accumulation) {
    auto sm = create_state_machine(50ms, 50ms);

    for (int cycle = 0; cycle < 3; ++cycle) {
        sm->target_running();
        ASSERT_TRUE(sm->start_connecting());
        ASSERT_TRUE(sm->start_assignment());

        // Shutdown before timeout
        std::this_thread::sleep_for(30ms);
        sm->target_shutdown();

        // Let timeout fire (should not call handler)
        std::this_thread::sleep_for(40ms);
        EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());
    }

    int error_count_after_cycles = get_error_count();

    // Restart one more time
    sm->target_running();
    ASSERT_TRUE(sm->start_connecting());
    std::this_thread::sleep_for(100ms);

    // No handlers should have accumulated
    EXPECT_EQ(error_count_after_cycles, get_error_count());
}

TEST_F(routing_client_state_machine_test, handler_invoked_exactly_once_per_timeout) {
    std::atomic<int> local_handler_count{0};

    routing_client_state_machine::configuration config;
    config.assignment_timeout_ = 50ms;
    config.register_timeout_ = 50ms;

    auto sm = routing_client_state_machine::create(io_context_, config, [&local_handler_count] { ++local_handler_count; });

    sm->target_running();

    // Timeout 1: Assignment
    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(1, local_handler_count.load());

    // Timeout 2: Registration
    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    ASSERT_TRUE(sm->assigned());
    ASSERT_TRUE(sm->start_registration());
    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(2, local_handler_count.load());

    // Manual deregister
    ASSERT_TRUE(sm->start_connecting());
    sm->deregistered();
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(3, local_handler_count.load());
}

TEST_F(routing_client_state_machine_test, graceful_deregister_after_registered_calls_handler) {
    auto sm = create_state_machine();
    sm->target_running();

    // Get to registered state
    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    ASSERT_TRUE(sm->assigned());
    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered());

    int initial_count = get_error_count();

    // Start graceful deregister but don't shutdown
    ASSERT_TRUE(sm->start_deregister());
    sm->deregistered();

    // Handler SHOULD be called (shall_run_ still true)
    EXPECT_TRUE(wait_for_error(initial_count, 100ms));
}

TEST_F(routing_client_state_machine_test, graceful_deregister_with_shutdown_no_handler) {
    auto sm = create_state_machine();
    sm->target_running();

    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    ASSERT_TRUE(sm->assigned());
    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered());
    ASSERT_TRUE(sm->start_deregister());

    // Shutdown before calling deregistered()
    sm->target_shutdown();

    int initial_count = get_error_count();
    sm->deregistered();

    // Handler should NOT be called (graceful shutdown)
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(initial_count, get_error_count());
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(routing_client_state_machine_test, rapid_state_transitions) {
    auto sm = create_state_machine();
    sm->target_running();

    // Rapid transitions without delays
    EXPECT_TRUE(sm->start_connecting());
    EXPECT_TRUE(sm->start_assignment());
    EXPECT_TRUE(sm->assigned());
    EXPECT_TRUE(sm->start_registration());
    EXPECT_TRUE(sm->registered());
    EXPECT_EQ(routing_client_state_e::ST_REGISTERED, sm->state());
}

TEST_F(routing_client_state_machine_test, multiple_state_machine_instances) {
    auto sm1 = create_state_machine();
    auto sm2 = create_state_machine();

    sm1->target_running();
    sm2->target_running();

    // Both should work independently
    EXPECT_TRUE(sm1->start_connecting());
    EXPECT_TRUE(sm2->start_connecting());

    EXPECT_EQ(routing_client_state_e::ST_CONNECTING, sm1->state());
    EXPECT_EQ(routing_client_state_e::ST_CONNECTING, sm2->state());

    EXPECT_TRUE(sm1->start_assignment());
    EXPECT_EQ(routing_client_state_e::ST_ASSIGNING, sm1->state());
    EXPECT_EQ(routing_client_state_e::ST_CONNECTING, sm2->state());
}

TEST_F(routing_client_state_machine_test, configuration_timeout_values_respected) {
    auto sm = create_state_machine(100ms, 200ms, 300ms);
    sm->target_running();
    int initial_count = get_error_count();

    // Test assignment timeout (100ms)
    auto start = std::chrono::steady_clock::now();
    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    EXPECT_TRUE(wait_for_error(initial_count, 200ms));
    auto duration = std::chrono::steady_clock::now() - start;

    // Should timeout around 100ms (with some margin)
    EXPECT_GE(duration, 100ms);
    EXPECT_LT(duration, 300ms);

    reset_error_counter();
    initial_count = get_error_count();

    // Test registration timeout (200ms)
    start = std::chrono::steady_clock::now();
    ASSERT_TRUE(sm->start_connecting());
    ASSERT_TRUE(sm->start_assignment());
    ASSERT_TRUE(sm->assigned());
    ASSERT_TRUE(sm->start_registration());
    EXPECT_TRUE(wait_for_error(initial_count, 300ms));
    duration = std::chrono::steady_clock::now() - start;

    // Should timeout around 200ms
    EXPECT_GE(duration, 200ms);
    EXPECT_LT(duration, 400ms);
}
