// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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

    std::shared_ptr<routing_client_state_machine> create_state_machine(std::chrono::milliseconds shutdown_timeout = 1000ms) {

        routing_client_state_machine::configuration config;
        config.shutdown_timeout_ = shutdown_timeout;

        return routing_client_state_machine::create(config, [this] {
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

    // ST_DEREGISTERED -> ST_REGISTERING
    EXPECT_TRUE(sm->start_registration());
    EXPECT_EQ(routing_client_state_e::ST_REGISTERING, sm->state());

    // ST_REGISTERING -> ST_REGISTERED
    EXPECT_TRUE(sm->registered(0x1234));
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

    // From ST_REGISTERING
    ASSERT_TRUE(sm->start_registration());
    initial_count = get_error_count();
    sm->deregistered();
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());
    EXPECT_TRUE(wait_for_error(initial_count, 100ms));

    reset_error_counter();

    // From ST_REGISTERED
    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered(0x1234));
    initial_count = get_error_count();
    sm->deregistered();
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());
    EXPECT_TRUE(wait_for_error(initial_count, 100ms));

    reset_error_counter();
}

TEST_F(routing_client_state_machine_test, graceful_deregistration_flow) {
    auto sm = create_state_machine();
    sm->target_running();
    int initial_count = get_error_count();

    // Get to registered state
    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered(0x1234));

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

    EXPECT_FALSE(sm->start_registration());
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());
}

TEST_F(routing_client_state_machine_test, cannot_start_connecting_from_non_deregistered) {
    auto sm = create_state_machine();
    sm->target_running();

    // Get to ST_REGISTERING
    ASSERT_TRUE(sm->start_registration());

    // Try to start connecting again - should fail
    EXPECT_FALSE(sm->start_registration());
    EXPECT_EQ(routing_client_state_e::ST_REGISTERING, sm->state());

    // Get to ST_REGISTERED
    ASSERT_TRUE(sm->registered(0x1234));
    EXPECT_FALSE(sm->start_registration());
    EXPECT_EQ(routing_client_state_e::ST_REGISTERED, sm->state());
}

TEST_F(routing_client_state_machine_test, cannot_mark_assigned_from_non_assigning) {
    auto sm = create_state_machine();
    sm->target_running();

    // From ST_DEREGISTERED
    EXPECT_FALSE(sm->registered(0x1234));
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());

    // Get to ST_REGISTERED
    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered(0x1234));

    // From ST_REGISTERED
    EXPECT_FALSE(sm->registered(0x1234));
    EXPECT_EQ(routing_client_state_e::ST_REGISTERED, sm->state());
}

TEST_F(routing_client_state_machine_test, cannot_mark_registered_from_non_registering) {
    auto sm = create_state_machine();
    sm->target_running();

    // From ST_DEREGISTERED
    EXPECT_FALSE(sm->registered(0x1234));
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());

    // Get to ST_REGISTERED
    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered(0x1234));

    // From ST_REGISTERED
    EXPECT_FALSE(sm->registered(0x1234));
    EXPECT_EQ(routing_client_state_e::ST_REGISTERED, sm->state());
}

TEST_F(routing_client_state_machine_test, cannot_start_deregister_from_non_registered) {
    auto sm = create_state_machine();
    sm->target_running();

    // From ST_DEREGISTERED
    EXPECT_FALSE(sm->start_deregister());
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());

    // From ST_REGISTERING
    ASSERT_TRUE(sm->start_registration());
    EXPECT_FALSE(sm->start_deregister());
    EXPECT_EQ(routing_client_state_e::ST_REGISTERING, sm->state());
}

// ============================================================================
// Await Methods Tests
// ============================================================================

TEST_F(routing_client_state_machine_test, await_registered_succeeds_when_already_registered) {
    auto sm = create_state_machine();
    sm->target_running();

    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered(0x1234));

    EXPECT_TRUE(sm->await_registered());
}

TEST_F(routing_client_state_machine_test, await_registered_waits_for_registration) {
    auto sm = create_state_machine();
    sm->target_running();

    ASSERT_TRUE(sm->start_registration());

    // Start waiting in separate thread
    std::atomic<bool> await_result{false};
    std::thread waiter([&sm, &await_result] { await_result = sm->await_registered(); });

    // Complete registration after a delay
    std::this_thread::sleep_for(100ms);
    ASSERT_TRUE(sm->registered(0x1234));

    waiter.join();
    EXPECT_TRUE(await_result.load());
}

TEST_F(routing_client_state_machine_test, await_registered_fails_from_wrong_state) {
    auto sm = create_state_machine();
    sm->target_running();

    // Not in ST_REGISTERING
    EXPECT_FALSE(sm->await_registered());
}

TEST_F(routing_client_state_machine_test, await_registered_times_out) {
    auto sm = create_state_machine(200ms); // Short shutdown timeout
    sm->target_running();

    ASSERT_TRUE(sm->start_registration());

    // Don't call registered(0x1234) - should timeout
    EXPECT_FALSE(sm->await_registered());
}

TEST_F(routing_client_state_machine_test, await_deregistered_succeeds_when_already_deregistered) {
    auto sm = create_state_machine();

    EXPECT_TRUE(sm->await_deregistered());
}

TEST_F(routing_client_state_machine_test, await_deregistered_waits_for_deregistration) {
    auto sm = create_state_machine();
    sm->target_running();

    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered(0x1234));
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

    ASSERT_TRUE(sm->start_registration());

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
    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered(0x1234));

    // Deregister
    int initial_count = get_error_count();
    sm->deregistered();
    EXPECT_TRUE(wait_for_error(initial_count, 100ms));

    reset_error_counter();

    // Second registration
    EXPECT_TRUE(sm->start_registration());
    EXPECT_TRUE(sm->registered(0x1234));
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

    ASSERT_TRUE(sm->start_registration());

    // Shutdown
    sm->target_shutdown();

    // Cannot start registration when shut down
    EXPECT_FALSE(sm->start_registration());
}

TEST_F(routing_client_state_machine_test, can_restart_after_shutdown) {
    auto sm = create_state_machine();
    sm->target_running();

    ASSERT_TRUE(sm->start_registration());
    sm->target_shutdown();

    // Cannot proceed while shut down
    ASSERT_TRUE(sm->registered(0x1234));
    ASSERT_FALSE(sm->start_registration());

    // Deregister and restart
    sm->deregistered();

    sm->target_running();
    EXPECT_TRUE(sm->start_registration());
}

// ============================================================================
// Error Handler Tests
// ============================================================================

TEST_F(routing_client_state_machine_test, error_handler_called_on_manual_deregister) {
    auto sm = create_state_machine();
    sm->target_running();
    int initial_count = get_error_count();

    ASSERT_TRUE(sm->start_registration());
    sm->deregistered();
    EXPECT_TRUE(wait_for_error(initial_count, 200ms));
}

TEST_F(routing_client_state_machine_test, graceful_deregister_after_registered_calls_handler) {
    auto sm = create_state_machine();
    sm->target_running();

    // Get to registered state
    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered(0x1234));

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

    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered(0x1234));
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
    EXPECT_TRUE(sm->start_registration());
    EXPECT_TRUE(sm->registered(0x1234));
    EXPECT_EQ(routing_client_state_e::ST_REGISTERED, sm->state());
}

TEST_F(routing_client_state_machine_test, multiple_state_machine_instances) {
    auto sm1 = create_state_machine();
    auto sm2 = create_state_machine();

    sm1->target_running();
    sm2->target_running();

    // Both should work independently
    EXPECT_TRUE(sm1->start_registration());
    EXPECT_TRUE(sm2->start_registration());

    EXPECT_EQ(routing_client_state_e::ST_REGISTERING, sm1->state());
    EXPECT_EQ(routing_client_state_e::ST_REGISTERING, sm2->state());

    EXPECT_TRUE(sm1->registered(0x1234));
    EXPECT_EQ(routing_client_state_e::ST_REGISTERED, sm1->state());
    EXPECT_EQ(routing_client_state_e::ST_REGISTERING, sm2->state());
}
