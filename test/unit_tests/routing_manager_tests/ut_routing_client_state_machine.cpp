// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../../../implementation/routing/include/routing_client_state_machine.hpp"

using namespace vsomeip_v3;

class routing_client_state_machine_test : public ::testing::Test {
protected:
    std::shared_ptr<routing_client_state_machine> create_state_machine() { return std::make_shared<routing_client_state_machine>(); }

    static constexpr client_t assigned_client_{0x6363};
    static constexpr client_t reassigned_client_{0x6364};
    static constexpr client_t foreign_client_{0xcafe};
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
    EXPECT_TRUE(sm->registered(assigned_client_));
    EXPECT_EQ(routing_client_state_e::ST_REGISTERED, sm->state());
}

// ============================================================================
// Registered client-id (NTWALL-1304)
// ============================================================================

TEST_F(routing_client_state_machine_test, client_is_unset_before_the_first_registration) {
    auto sm = create_state_machine();
    EXPECT_EQ(VSOMEIP_CLIENT_UNSET, sm->client());

    sm->target_running();
    ASSERT_TRUE(sm->start_registration());
    EXPECT_EQ(VSOMEIP_CLIENT_UNSET, sm->client()) << "no id is assigned until the registration completes";
}

TEST_F(routing_client_state_machine_test, client_reports_the_registered_id) {
    auto sm = create_state_machine();
    sm->target_running();
    ASSERT_TRUE(sm->start_registration());

    ASSERT_TRUE(sm->registered(assigned_client_));
    EXPECT_EQ(assigned_client_, sm->client());
}

TEST_F(routing_client_state_machine_test, client_is_only_changed_by_a_completed_registration) {
    auto sm = create_state_machine();
    sm->target_running();
    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered(assigned_client_));

    EXPECT_FALSE(sm->registered(foreign_client_)) << "already registered, transition is refused";
    EXPECT_EQ(assigned_client_, sm->client()) << "a refused transition leaves the registered id alone";

    sm->deregistered();
    EXPECT_EQ(VSOMEIP_CLIENT_UNSET, sm->client());

    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered(reassigned_client_));
    EXPECT_EQ(reassigned_client_, sm->client());
}

TEST_F(routing_client_state_machine_test, deregistered_from_any_state) {
    auto sm = create_state_machine();
    sm->target_running();

    // From ST_DEREGISTERED
    sm->deregistered();
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());

    // From ST_REGISTERING
    ASSERT_TRUE(sm->start_registration());
    sm->deregistered();
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());

    // From ST_REGISTERED
    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered(assigned_client_));
    sm->deregistered();
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());
}

TEST_F(routing_client_state_machine_test, graceful_deregistration_flow) {
    auto sm = create_state_machine();
    sm->target_running();

    // Get to registered state
    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered(assigned_client_));

    // ST_REGISTERED -> ST_DEREGISTERED
    sm->deregistered();
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());
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
    ASSERT_TRUE(sm->registered(assigned_client_));
    EXPECT_FALSE(sm->start_registration());
    EXPECT_EQ(routing_client_state_e::ST_REGISTERED, sm->state());
}

TEST_F(routing_client_state_machine_test, cannot_mark_assigned_from_non_assigning) {
    auto sm = create_state_machine();
    sm->target_running();

    // From ST_DEREGISTERED
    EXPECT_FALSE(sm->registered(assigned_client_));
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());

    // Get to ST_REGISTERED
    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered(assigned_client_));

    // From ST_REGISTERED
    EXPECT_FALSE(sm->registered(assigned_client_));
    EXPECT_EQ(routing_client_state_e::ST_REGISTERED, sm->state());
}

TEST_F(routing_client_state_machine_test, cannot_mark_registered_from_non_registering) {
    auto sm = create_state_machine();
    sm->target_running();

    // From ST_DEREGISTERED
    EXPECT_FALSE(sm->registered(assigned_client_));
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());

    // Get to ST_REGISTERED
    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered(assigned_client_));

    // From ST_REGISTERED
    EXPECT_FALSE(sm->registered(assigned_client_));
    EXPECT_EQ(routing_client_state_e::ST_REGISTERED, sm->state());
}

// ============================================================================
// Re-registration Tests
// ============================================================================

TEST_F(routing_client_state_machine_test, can_reregister_after_deregistration) {
    auto sm = create_state_machine();
    sm->target_running();

    // First registration
    ASSERT_TRUE(sm->start_registration());
    ASSERT_TRUE(sm->registered(assigned_client_));

    // Deregister
    sm->deregistered();
    EXPECT_EQ(routing_client_state_e::ST_DEREGISTERED, sm->state());

    // Second registration
    EXPECT_TRUE(sm->start_registration());
    EXPECT_TRUE(sm->registered(assigned_client_));
    EXPECT_EQ(routing_client_state_e::ST_REGISTERED, sm->state());
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
    ASSERT_TRUE(sm->registered(assigned_client_));
    ASSERT_FALSE(sm->start_registration());

    // Deregister and restart
    sm->deregistered();

    sm->target_running();
    EXPECT_TRUE(sm->start_registration());
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(routing_client_state_machine_test, rapid_state_transitions) {
    auto sm = create_state_machine();
    sm->target_running();

    // Rapid transitions without delays
    EXPECT_TRUE(sm->start_registration());
    EXPECT_TRUE(sm->registered(assigned_client_));
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

    EXPECT_TRUE(sm1->registered(assigned_client_));
    EXPECT_EQ(routing_client_state_e::ST_REGISTERED, sm1->state());
    EXPECT_EQ(routing_client_state_e::ST_REGISTERING, sm2->state());
}
