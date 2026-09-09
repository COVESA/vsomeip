// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

/// test `vsomeip_v3::application` APIs that are specific to the routing manager
/// e.g., `register_routing_state_handler`

#include "helpers/ecu_setup.hpp"
#include "helpers/base_fake_socket_fixture.hpp"

#include "sample_configurations.hpp"

#include <gtest/gtest.h>

using namespace vsomeip_v3;
using namespace vsomeip_v3::testing;

struct test_api_rm : public base_fake_socket_fixture {
    void start_router() {
        ecu_.prepare();
        ecu_.start_apps();

        routingmanagerd_ = ecu_.apps_[routingmanager_name_];
    }

    std::string const routingmanager_name_{"router_one"};

    ecu_setup ecu_{"single-ecu", ecu_config{boardnet::ecu_one_config}, *socket_manager_};

    app* routingmanagerd_{};
};

TEST_F(test_api_rm, set_routing_state) {
    /// check that set_routing_state triggers the registered routing_state_handler

    start_router();

    // register a routing state handler on the router application
    attribute_recorder<routing_state_e> routing_state_record;
    routingmanagerd_->get_application()->register_routing_state_handler(
            [&routing_state_record](routing_state_e _state) { routing_state_record.record(_state); });

    // stress test with multiple state transitions
    for (int i = 0; i < 100; ++i) {
        // set routing state to suspended
        routingmanagerd_->set_routing_state(routing_state_e::RS_SUSPENDED);

        // handler must have been called with RS_SUSPENDED
        ASSERT_TRUE(routing_state_record.wait_for_last(routing_state_e::RS_SUSPENDED));

        // set routing state to resumed
        routingmanagerd_->set_routing_state(routing_state_e::RS_RESUMED);

        // handler must have been called with RS_RESUMED
        ASSERT_TRUE(routing_state_record.wait_for_last(routing_state_e::RS_RESUMED));
    }
}

TEST_F(test_api_rm, set_routing_state_changes_before_do_not_count) {
    start_router();

    routingmanagerd_->set_routing_state(routing_state_e::RS_SUSPENDED);
    routingmanagerd_->set_routing_state(routing_state_e::RS_RESUMED);

    attribute_recorder<routing_state_e> routing_state_record;
    routingmanagerd_->get_application()->register_routing_state_handler(
            [&routing_state_record](routing_state_e _state) { routing_state_record.record(_state); });

    routingmanagerd_->set_routing_state(routing_state_e::RS_SUSPENDED);

    ASSERT_TRUE(routing_state_record.wait_for_sequence({routing_state_e::RS_SUSPENDED}));
}

TEST_F(test_api_rm, set_routing_state_triggers_state_once) {
    start_router();

    attribute_recorder<routing_state_e> routing_state_record;
    routingmanagerd_->get_application()->register_routing_state_handler(
            [&routing_state_record](routing_state_e _state) { routing_state_record.record(_state); });

    routingmanagerd_->set_routing_state(routing_state_e::RS_SUSPENDED);

    ASSERT_TRUE(routing_state_record.wait_for_sequence({routing_state_e::RS_SUSPENDED}));

    routingmanagerd_->set_routing_state(routing_state_e::RS_RESUMED);

    ASSERT_TRUE(routing_state_record.wait_for_sequence({routing_state_e::RS_SUSPENDED, routing_state_e::RS_RESUMED}));

    routingmanagerd_->set_routing_state(routing_state_e::RS_SUSPENDED);

    ASSERT_TRUE(routing_state_record.wait_for_sequence(
            {routing_state_e::RS_SUSPENDED, routing_state_e::RS_RESUMED, routing_state_e::RS_SUSPENDED}));
}

TEST_F(test_api_rm, set_routing_state_is_idempotent) {
    /// check that `set_routing_state` is idempotent by abusing the state hook

    start_router();

    attribute_recorder<routing_state_e> routing_state_record;
    routingmanagerd_->get_application()->register_routing_state_handler(
            [&routing_state_record](routing_state_e _state) { routing_state_record.record(_state); });

    routingmanagerd_->set_routing_state(routing_state_e::RS_SUSPENDED);
    routingmanagerd_->set_routing_state(routing_state_e::RS_SUSPENDED);

    ASSERT_TRUE(routing_state_record.wait_for_sequence({routing_state_e::RS_SUSPENDED}));

    routingmanagerd_->set_routing_state(routing_state_e::RS_RESUMED);
    routingmanagerd_->set_routing_state(routing_state_e::RS_RESUMED);

    ASSERT_TRUE(routing_state_record.wait_for_sequence({routing_state_e::RS_SUSPENDED, routing_state_e::RS_RESUMED}));

    routingmanagerd_->set_routing_state(routing_state_e::RS_SUSPENDED);
    routingmanagerd_->set_routing_state(routing_state_e::RS_SUSPENDED);

    ASSERT_TRUE(routing_state_record.wait_for_sequence(
            {routing_state_e::RS_SUSPENDED, routing_state_e::RS_RESUMED, routing_state_e::RS_SUSPENDED}));
}
