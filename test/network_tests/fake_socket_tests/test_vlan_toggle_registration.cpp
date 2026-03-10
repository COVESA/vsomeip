// Copyright (C) 2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/app.hpp"
#include "helpers/command_message.hpp"

#include <boost/asio/error.hpp>
#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <vector>

namespace vsomeip_v3::testing {

static constexpr auto routingmanager_name_ = "routingmanagerd";
static constexpr auto server_name_ = "server";
static constexpr auto client_name_ = "client";
static constexpr auto client_one_name_ = "client-one";
static constexpr auto client_two_name_ = "client-two";

/**
 * @brief Test fixture for VLAN toggle scenarios.
 *
 * This test simulates what happens when a network interface goes down and comes back up
 * (like a VLAN toggle), and tests whether applications can recover their registration state.
 *
 * Unlike the process-based vlan_toggle_tests, this uses the fake socket infrastructure
 * to run all applications in-process with different names.
 */
struct test_vlan_toggle_registration : public base_fake_socket_fixture {
    test_vlan_toggle_registration() {
        use_configuration("multiple_client_one_process.json");
        create_app(routingmanager_name_);
        create_app(server_name_);
        create_app(client_name_);
        create_app(client_one_name_);
        create_app(client_two_name_);
    }

    void start_router() {
        routingmanagerd_ = start_client(routingmanager_name_);
        ASSERT_NE(routingmanagerd_, nullptr);
        ASSERT_TRUE(await_connectable(routingmanager_name_));
    }

    void start_server() {
        server_ = start_client(server_name_);
        ASSERT_NE(server_, nullptr);
        ASSERT_TRUE(server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
        server_->offer(service_instance_);
    }

    void start_all_clients() {
        client_ = start_client(client_name_);
        ASSERT_NE(client_, nullptr);
        ASSERT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

        client_one_ = start_client(client_one_name_);
        ASSERT_NE(client_one_, nullptr);
        ASSERT_TRUE(client_one_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

        client_two_ = start_client(client_two_name_);
        ASSERT_NE(client_two_, nullptr);
        ASSERT_TRUE(client_two_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    }

    void start_all_apps() {
        start_router();
        start_server();
        start_all_clients();
    }

    /**
     * @brief Simulates VLAN going DOWN by disconnecting all connections.
     *
     * This mimics what happens when the network interface is disabled.
     */
    void simulate_vlan_down() {
        // Disconnect all client connections to router and server
        // Ignore connections to simulate network being down
        for (auto const& name : {routingmanager_name_, server_name_, client_name_, client_one_name_, client_two_name_}) {
            set_ignore_connections(name, true);
        }

        // Break existing connections with timeout error (simulating network down)

        // Clients (including server as client) <-> RoutingManager
        for (auto const& client : {client_name_, client_one_name_, client_two_name_, server_name_}) {
            std::ignore = disconnect(client, boost::asio::error::timed_out, routingmanager_name_, std::nullopt);
        }

        // Clients <-> Server
        for (auto const& client : {client_name_, client_one_name_, client_two_name_}) {
            std::ignore = disconnect(client, boost::asio::error::timed_out, server_name_, std::nullopt);
        }
    }

    /**
     * @brief Simulates VLAN going UP by allowing reconnections.
     *
     * This mimics what happens when the network interface is re-enabled.
     */
    void simulate_vlan_up() {
        // Re-allow connections to simulate network coming back up
        for (auto const& name : {routingmanager_name_, server_name_, client_name_, client_one_name_, client_two_name_}) {
            set_ignore_connections(name, false);
        }

        // Trigger connection reset to force reconnection attempts
        // Clients (including server as client) <-> RoutingManager
        for (auto const& client : {client_name_, client_one_name_, client_two_name_, server_name_}) {
            std::ignore = disconnect(client, std::nullopt, routingmanager_name_, boost::asio::error::connection_reset);
        }
    }

    service_instance service_instance_{0x3344, 0x1};

    app* routingmanagerd_{nullptr};
    app* server_{nullptr};
    app* client_{nullptr};
    app* client_one_{nullptr};
    app* client_two_{nullptr};
};

/**
 * @brief Simpler version: Test basic VLAN toggle recovery without blocking any commands.
 *
 * This serves as a baseline test to verify the VLAN toggle simulation works correctly
 * before adding the complexity of blocking commands.
 */
TEST_F(test_vlan_toggle_registration, basic_vlan_toggle_recovery) {
    // Start all applications
    start_all_apps();

    // Request service from clients
    client_->request_service(service_instance_);
    client_one_->request_service(service_instance_);
    client_two_->request_service(service_instance_);

    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    ASSERT_TRUE(client_one_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    ASSERT_TRUE(client_two_->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    TEST_LOG << "[step] Initial setup complete";

    // Simulate VLAN DOWN
    TEST_LOG << "[step] Simulating VLAN DOWN";
    simulate_vlan_down();

    // Wait for ST_DEREGISTERED state
    EXPECT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_DEREGISTERED));
    EXPECT_TRUE(client_one_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_DEREGISTERED));
    EXPECT_TRUE(client_two_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_DEREGISTERED));
    EXPECT_TRUE(server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_DEREGISTERED));

    // Simulate VLAN UP
    TEST_LOG << "[step] Simulating VLAN UP";
    simulate_vlan_up();

    // Wait for re-registration
    EXPECT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    EXPECT_TRUE(client_one_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    EXPECT_TRUE(client_two_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    EXPECT_TRUE(server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    TEST_LOG << "[step] All applications re-registered after VLAN toggle";
}

/**
 * @brief Test that blocking ASSIGN_CLIENT once after VLAN toggle UP still allows recovery.
 *
 * This test:
 * 1. Starts all applications and verifies they register
 * 2. Simulates VLAN DOWN (network interface disabled)
 * 3. Simulates VLAN UP (network interface re-enabled)
 * 4. Blocks ASSIGN_CLIENT (0x00) command ONCE for each client after reconnection
 * 5. Verifies all applications can still recover their registration state
 *
 * The expectation is that vsomeip should retry the registration and eventually succeed
 * even if the first ASSIGN_CLIENT is dropped.
 */
TEST_F(test_vlan_toggle_registration, block_register_application_once_after_vlan_toggle) {
    // Step 1: Start all applications and verify initial registration
    start_all_apps();

    // Request service from clients
    client_->request_service(service_instance_);
    client_one_->request_service(service_instance_);
    client_two_->request_service(service_instance_);

    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    ASSERT_TRUE(client_one_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
    ASSERT_TRUE(client_two_->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    TEST_LOG << "[step] All applications registered and service available";

    // Step 2: Simulate VLAN DOWN
    TEST_LOG << "[step] Simulating VLAN DOWN";
    simulate_vlan_down();

    // Wait for ST_DEREGISTERED state
    EXPECT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_DEREGISTERED));
    EXPECT_TRUE(client_one_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_DEREGISTERED));
    EXPECT_TRUE(client_two_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_DEREGISTERED));
    EXPECT_TRUE(server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_DEREGISTERED));

    // Step 3: Setup to block ASSIGN_CLIENT once for each client after VLAN UP
    TEST_LOG << "[step] Setting up ASSIGN_CLIENT blockers";

    // These promises will be set when we block the first registration attempt
    std::promise<void> client_one_blocked_promise;
    std::promise<void> client_two_blocked_promise;
    auto client_one_blocked_future = client_one_blocked_promise.get_future();
    auto client_two_blocked_future = client_two_blocked_promise.get_future();

    std::atomic<bool> client_one_already_blocked{false};
    std::atomic<bool> client_two_already_blocked{false};

    set_custom_command_handler(client_one_name_, routingmanager_name_,
                               [&client_one_blocked_promise, &client_one_already_blocked](command_message const& _cmd) {
                                   if (_cmd.id_ == protocol::id_e::ASSIGN_CLIENT_ID) {
                                       bool expected = false;
                                       if (client_one_already_blocked.compare_exchange_strong(expected, true)) {
                                           TEST_LOG << "[handler] Blocking first ASSIGN_CLIENT from client-one";
                                           client_one_blocked_promise.set_value();
                                           return true; // Block this message
                                       }
                                   }
                                   return false; // Allow message through
                               });

    set_custom_command_handler(client_two_name_, routingmanager_name_,
                               [&client_two_blocked_promise, &client_two_already_blocked](command_message const& _cmd) {
                                   if (_cmd.id_ == protocol::id_e::ASSIGN_CLIENT_ID) {
                                       bool expected = false;
                                       if (client_two_already_blocked.compare_exchange_strong(expected, true)) {
                                           TEST_LOG << "[handler] Blocking first ASSIGN_CLIENT from client-two";
                                           client_two_blocked_promise.set_value();
                                           return true; // Block this message
                                       }
                                   }
                                   return false; // Allow message through
                               });

    // Step 4: Simulate VLAN UP
    TEST_LOG << "[step] Simulating VLAN UP";
    simulate_vlan_up();

    // Step 5: Verify all applications recover their registration state
    TEST_LOG << "[step] Waiting for all applications to re-register";

    // Wait for connections to be re-established
    ASSERT_TRUE(await_connection(client_name_, routingmanager_name_));
    ASSERT_TRUE(await_connection(client_one_name_, routingmanager_name_));
    ASSERT_TRUE(await_connection(client_two_name_, routingmanager_name_));
    ASSERT_TRUE(await_connection(server_name_, routingmanager_name_));

    // Wait for the blocking to occur
    EXPECT_EQ(client_one_blocked_future.wait_for(std::chrono::seconds(2)), std::future_status::ready)
            << "Expected first ASSIGN_CLIENT from client-one to be blocked within timeout";
    EXPECT_EQ(client_two_blocked_future.wait_for(std::chrono::seconds(2)), std::future_status::ready)
            << "Expected first ASSIGN_CLIENT from client-two to be blocked within timeout";

    // Verify all applications eventually registered
    // Note: Longer timeout needed because blocking ASSIGN_CLIENT triggers a 3s assignment timeout + retry.
    EXPECT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED, std::chrono::seconds(4)))
            << "Client failed to re-register after VLAN toggle with blocked first ASSIGN_CLIENT";

    EXPECT_TRUE(client_one_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED, std::chrono::seconds(4)))
            << "Client-one failed to re-register after VLAN toggle with blocked first ASSIGN_CLIENT";

    EXPECT_TRUE(client_two_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED, std::chrono::seconds(4)))
            << "Client-two failed to re-register after VLAN toggle with blocked first ASSIGN_CLIENT";

    EXPECT_TRUE(server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED, std::chrono::seconds(4)))
            << "Server failed to re-register after VLAN toggle";

    TEST_LOG << "[step] All applications successfully re-registered after VLAN toggle";

    // Clear the custom handlers
    set_custom_command_handler(client_name_, routingmanager_name_, nullptr);
    set_custom_command_handler(client_one_name_, routingmanager_name_, nullptr);
    set_custom_command_handler(client_two_name_, routingmanager_name_, nullptr);

    EXPECT_TRUE(client_->availability_record_.wait_for_last(service_availability::available(service_instance_)))
            << "Client did not receive service availability after VLAN toggle recovery";

    EXPECT_TRUE(client_one_->availability_record_.wait_for_last(service_availability::available(service_instance_)))
            << "Client-one did not receive service availability after VLAN toggle recovery";

    EXPECT_TRUE(client_two_->availability_record_.wait_for_last(service_availability::available(service_instance_)))
            << "Client-two did not receive service availability after VLAN toggle recovery";

    TEST_LOG << "[step] Test completed successfully - all applications recovered after VLAN toggle";
}

/**
 * @brief Test using drop_command_once to explicitly block registration once.
 *
 * This test uses the drop_command_once API to block the ASSIGN_CLIENT command exactly once.
 */
TEST_F(test_vlan_toggle_registration, block_register_using_drop_command_once) {
    // Start router first
    start_router();

    // For client, we want to block the first ASSIGN_CLIENT
    // Use drop_command_once to set up the block
    create_app(client_name_);

    // Prepare to drop the first ASSIGN_CLIENT command
    auto fut = drop_command_once(client_name_, routingmanager_name_, protocol::id_e::ASSIGN_CLIENT_ID);

    // Start client but don't wait for registration yet
    auto* client = start_client(client_name_);
    ASSERT_NE(client, nullptr);

    // Wait for connection to router
    ASSERT_TRUE(await_connection(client_name_, routingmanager_name_));

    // Sanity check that the right message was dropped
    ASSERT_TRUE(fut.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    EXPECT_EQ(fut.get(), protocol::id_e::ASSIGN_CLIENT_ID);

    // The client should eventually register even if first attempt was dropped
    EXPECT_TRUE(client->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED, std::chrono::seconds(4)))
            << "Client should recover and register after first ASSIGN_CLIENT was dropped";
}

}
