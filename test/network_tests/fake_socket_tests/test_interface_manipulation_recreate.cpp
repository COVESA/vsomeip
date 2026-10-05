// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sample_configurations.hpp"

#include "helpers/app.hpp"
#include "helpers/attribute_recorder.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/ecu_setup.hpp"
#include "helpers/service_state.hpp"

#include "common/iteration_log_capture.hpp"

#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>

namespace vsomeip_v3::testing {

// The ECUs are built locally inside the test loop instead of as fixture members,
// so the whole scenario can be rebuilt from scratch on every iteration.
struct interface_manipulation_recreate : public base_fake_socket_fixture {
    std::vector<event_spec> const event_specs_both_{{0x8001, {0x1}, vsomeip::reliability_type_e::RT_RELIABLE},
                                                    {0x8002, {0x2}, vsomeip::reliability_type_e::RT_UNRELIABLE}};

    std::vector<event_spec> events = {event_spec{0x8003, {0x3}, vsomeip::reliability_type_e::RT_UNRELIABLE}};

    interface interface_{0x3345, events, event_specs_both_};
};

/*
 * Regression test for the add_routing_info() <-> on_disconnect() race.
 *
 * The race can only happen on the first interface-down after a fresh subscription, while the
 * provider is still in its repetition OFFER phase and an OFFER is being processed at the very
 * instant the link drops (a stale is_established()==true then lets add_routing_info() re-offer
 * the service that on_disconnect() just stopped offering). Flapping a single long-lived link does
 * not help, as after the first recovery the provider settles into its cyclic-offer,
 * so no later drop coincides with an in-flight OFFER.
 *
 * To get more than one shot at the race we must rebuild the whole scenario, fresh routers and
 * fresh offer/subscribe for each iteration, which means a fresh socket_manager so the re-created
 * ECUs do not collide with the previous iteration's retained connection state.
 */
TEST_F(interface_manipulation_recreate, no_spurious_availability_when_recreated_from_scratch) {
    common::iteration_log_capture log_capture;

    for (size_t i = 0; i < 401; ++i) {
        // Discard the previous iteration's captured output, so only this iteration's trace is dumped on failure.
        log_capture.begin_iteration();
        try {
            // fresh per-iteration socket_manager so the re-created ECUs start from a clean slate
            reset_socket_manager();

            ecu_config ecu_one_cfg{boardnet::ecu_one_config};
            ecu_config ecu_two_cfg{boardnet::ecu_two_config};
            ecu_setup ecu_one{"ecu_one", ecu_one_cfg.add_interface({interface_}), *socket_manager_};
            ecu_setup ecu_two{"ecu_two", ecu_two_cfg, *socket_manager_};

            ecu_one.prepare();
            ecu_two.prepare();

            ecu_one.start_router();
            ecu_two.start_router();

            auto* router_one = ecu_one.router_;
            auto* router_two = ecu_two.router_;

            // offer/subscribe and wait for the freshly established subscription to become available
            router_one->offer(interface_);
            router_two->subscribe(interface_);

            ASSERT_TRUE(router_two->subscription_record_.wait_for_any(
                    event_subscription::successfully_subscribed_to({interface_.instance_, interface_.events_[0]})))
                    << "subscription did not succeed on iteration " << i;
            ASSERT_TRUE(router_two->availability_record_.wait_for_last(service_availability::available(interface_.instance_)))
                    << "service did not become available on iteration " << i;

            // drop the freshly established link, while the provider may still be repeating its OFFER
            ASSERT_TRUE(ecu_one.set_routing(fake_netlink_connector::state_e::DOWN));
            ASSERT_TRUE(ecu_two.set_routing(fake_netlink_connector::state_e::DOWN));

            router_two->availability_record_.clear();
            EXPECT_FALSE(router_two->availability_record_.wait_for_any(service_availability::available(interface_.instance_),
                                                                       std::chrono::milliseconds(100)))
                    << "Spurious availability after link drop on iteration " << i;

            // ecu_one/ecu_two are torn down at scope exit (apps stopped + joined) before the next
            // iteration's reset_socket_manager(), so the next iteration starts entirely from scratch
        } catch (...) {
            // throw_on_failure is enabled, so a failed ASSERT/EXPECT lands here: emit just this
            // iteration's captured trace, then let gtest handle the failure.
            log_capture.dump();
            throw;
        }
    }
    log_capture.dump();
}
}
